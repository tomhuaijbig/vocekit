Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$scriptsRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$publishScript = Join-Path $scriptsRoot "scripts\publish-finalized-release.ps1"
. $publishScript -DecisionTestMode

function Assert-TestThrows {
    param(
        [Parameter(Mandatory = $true)][scriptblock]$Action,
        [Parameter(Mandatory = $true)][string]$MessagePattern,
        [Parameter(Mandatory = $true)][string]$FailureMessage
    )

    $rejected = $false
    try { & $Action } catch {
        $rejected = $_.Exception.Message -match $MessagePattern
    }
    if (-not $rejected) { throw $FailureMessage }
}

function Get-TestFileBinding {
    param([Parameter(Mandatory = $true)][string]$Path)

    $item = Get-Item -LiteralPath $Path
    return [ordered]@{
        name = $item.Name
        bytes = [long]$item.Length
        sha256 = (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToUpperInvariant()
    }
}

function Remove-PublishTestDirectory {
    param([Parameter(Mandatory = $true)][string]$Path)

    $full = [IO.Path]::GetFullPath($Path)
    $tempPrefix = [IO.Path]::GetFullPath([IO.Path]::GetTempPath()).TrimEnd([char[]]@('\', '/')) + "\"
    if (-not $full.StartsWith($tempPrefix, [StringComparison]::OrdinalIgnoreCase) -or
        [IO.Path]::GetFileName($full) -notlike "vocekit-publish-gate-test-*") {
        throw "Refusing to remove an unsafe publish-gate fixture: $full"
    }
    if ([IO.Directory]::Exists($full)) {
        foreach ($file in Get-ChildItem -LiteralPath $full -Recurse -Force -File) {
            [IO.File]::SetAttributes($file.FullName, [IO.FileAttributes]::Normal)
        }
        [IO.Directory]::Delete($full, $true)
    }
}

function New-PublishInputFixture {
    param(
        [Parameter(Mandatory = $true)][string]$Root,
        [string]$EvidenceName = "acceptance-evidence.json"
    )

    $candidateDirectory = Join-Path $Root "dist\releases\v1.2.3"
    $evidenceDirectory = Join-Path $Root "evidence"
    $imageDirectory = Join-Path $evidenceDirectory "images"
    [void][IO.Directory]::CreateDirectory($candidateDirectory)
    [void][IO.Directory]::CreateDirectory($imageDirectory)

    $archivePath = Join-Path $candidateDirectory "vocekit-qt6-portable.zip"
    $sidecarPath = "$archivePath.sha256"
    $manifestPath = Join-Path $candidateDirectory "update-manifest.json"
    $candidatePath = Join-Path $candidateDirectory "release-candidate.json"
    $evidencePath = Join-Path $evidenceDirectory $EvidenceName
    $screenshotPath = Join-Path $imageDirectory "chat-100.png"
    $notesPath = Join-Path $Root "release-notes.md"

    [IO.File]::WriteAllBytes($archivePath, [byte[]](1, 3, 3, 7, 9, 11))
    [IO.File]::WriteAllText($sidecarPath, "fixture sidecar", [Text.UTF8Encoding]::new($false))
    [IO.File]::WriteAllText($manifestPath, '{"fixture":true}', [Text.UTF8Encoding]::new($false))
    [IO.File]::WriteAllBytes($screenshotPath, [byte[]](137, 80, 78, 71, 1, 2, 3, 4))
    [IO.File]::WriteAllText($notesPath, "fixture release notes", [Text.UTF8Encoding]::new($false))

    $candidate = [ordered]@{
        schema_version = 1
        kind = "vocekit-signed-release-candidate"
        source_commit = "a" * 40
        version = "1.2.3"
        tag = "v1.2.3"
        archive = Get-TestFileBinding -Path $archivePath
        sidecar = Get-TestFileBinding -Path $sidecarPath
        manifest = Get-TestFileBinding -Path $manifestPath
    }
    [IO.File]::WriteAllText(
        $candidatePath,
        ($candidate | ConvertTo-Json -Depth 8),
        [Text.UTF8Encoding]::new($false)
    )
    $evidence = [ordered]@{
        schema_version = 1
        kind = "vocekit-release-acceptance-evidence"
        candidate_record = Get-TestFileBinding -Path $candidatePath
        cells = @([ordered]@{
            screenshots = @([ordered]@{
                reference = "images/chat-100.png"
                sha256 = (Get-FileHash -LiteralPath $screenshotPath -Algorithm SHA256).Hash
            })
        })
    }
    [IO.File]::WriteAllText(
        $evidencePath,
        ($evidence | ConvertTo-Json -Depth 8),
        [Text.UTF8Encoding]::new($false)
    )
    return [PSCustomObject]@{
        CandidatePath = $candidatePath
        EvidencePath = $evidencePath
        ReleaseNotesPath = $notesPath
        ArchivePath = $archivePath
        SidecarPath = $sidecarPath
        ManifestPath = $manifestPath
        ScreenshotPath = $screenshotPath
    }
}

$scriptText = Get-Content -LiteralPath $publishScript -Raw -Encoding UTF8
$publishTokens = $null
$publishParseErrors = $null
$publishAst = [Management.Automation.Language.Parser]::ParseFile(
    $publishScript,
    [ref]$publishTokens,
    [ref]$publishParseErrors
)
if ($publishParseErrors.Count -ne 0) {
    throw "Publisher script has PowerShell parse errors."
}
$productionParameterNames = @($publishAst.ParamBlock.Parameters | ForEach-Object {
    $_.Name.VariablePath.UserPath
})
if (-not $scriptText.Contains('[switch]$CreateVerifiedDraft') -or
    -not $scriptText.Contains('if (-not $CreateVerifiedDraft)') -or
    $scriptText.Contains('"--draft=false"') -or
    $scriptText.Contains("'--draft=false'") -or
    $scriptText.IndexOf('if ($Publish)') -gt $scriptText.LastIndexOf('Assert-PublishRepositoryAndUrls `') -or
    $productionParameterNames -contains "RepositoryRoot" -or
    $productionParameterNames -contains "GhTestInvoker") {
    throw "Publisher mode boundaries do not make the default path read-only and public promotion fail first."
}

Assert-TestThrows `
    -Action { & $publishScript -DecisionTestMode -CreateVerifiedDraft } `
    -MessagePattern "parameter set|ParameterSet" `
    -FailureMessage "DecisionTestMode could be combined with the remote-write switch."

$dummyGateParameters = @{
    CandidatePath = "missing-candidate.json"
    EvidencePath = "missing-evidence.json"
    ExpectedSignerSubject = "CN=Fixture"
    ExpectedSignerThumbprint = "A" * 40
    ExpectedUpdateFeedUrl = "https://api.github.com/repos/example/vocekit/releases/latest"
    ExpectedReleaseBaseUrl = "https://github.com/example/vocekit/releases/download"
    ExpectedReleasePageBaseUrl = "https://github.com/example/vocekit/releases/tag"
    Repository = "example/vocekit"
    ReleaseNotesPath = "missing-notes.md"
}
Assert-TestThrows `
    -Action { & $publishScript @dummyGateParameters -Publish -CreateVerifiedDraft } `
    -MessagePattern "Public promotion is disabled.*No tag or Release was modified" `
    -FailureMessage "Public promotion did not fail before reading paths or writing remote state."

$preflightCommands = New-Object Collections.Generic.List[string]
$preflightGh = {
    param([string[]]$CommandArguments)
    $preflightCommands.Add(($CommandArguments -join " "))
    if ($CommandArguments[0] -ceq "auth") {
        return [PSCustomObject]@{ exit_code = 0; output = "authenticated" }
    }
    if ($CommandArguments[-1] -match '/immutable-releases$') {
        return [PSCustomObject]@{ exit_code = 0; output = '{"enabled":true}' }
    }
    if ($CommandArguments[-1] -match '/releases/tags/') {
        return [PSCustomObject]@{ exit_code = 1; output = "HTTP/2.0 404 Not Found`n{}" }
    }
    throw "Unexpected read-only preflight command: $($CommandArguments -join ' ')"
}
Assert-PublishRemotePreflight `
    -Repository "example/vocekit" `
    -Tag "v1.2.3" `
    -GhTestInvoker $preflightGh
$preflightText = @($preflightCommands) -join "`n"
if ($preflightText -match '(?i)\brelease\s+(create|upload|edit|delete)\b|\b(POST|PATCH|PUT|DELETE)\b') {
    throw "Default remote preflight constructed a mutating GitHub command."
}

$testRoot = Join-Path ([IO.Path]::GetTempPath()) (
    "vocekit-publish-gate-test-" + [Guid]::NewGuid().ToString("N")
)
try {
    $fixture = New-PublishInputFixture -Root $testRoot
    $inputSet = Open-FinalizedReleaseInputSet `
        -CandidatePath $fixture.CandidatePath `
        -EvidencePath $fixture.EvidencePath `
        -ReleaseNotesPath $fixture.ReleaseNotesPath
    try {
        if (@($inputSet.Locks).Count -ne @($inputSet.Paths).Count -or
            @($inputSet.ScreenshotPaths).Count -ne 1 -or
            @($inputSet.CoreAssetPaths).Count -ne 5) {
            throw "Publisher did not freeze the complete candidate/evidence/notes/screenshot input set."
        }
        $writeWasDenied = $false
        try {
            $writer = [IO.File]::Open(
                $fixture.ArchivePath,
                [IO.FileMode]::Open,
                [IO.FileAccess]::Write,
                [IO.FileShare]::ReadWrite
            )
            $writer.Dispose()
        } catch {
            $writeWasDenied = $true
        }
        if (-not $writeWasDenied) {
            throw "A frozen candidate archive remained writable while publication locks were open."
        }

        $coreAssets = @(Get-PublishCoreAssetSnapshots -InputSet $inputSet)
        $remoteAssets = New-Object Collections.Generic.List[object]
        $assetId = 100
        foreach ($asset in $coreAssets) {
            $remoteAssets.Add([PSCustomObject]@{
                id = $assetId++
                name = $asset.name
                state = "uploaded"
                size = $asset.bytes
                digest = "sha256:$(([string]$asset.sha256).ToLowerInvariant())"
            })
        }
        $release = [PSCustomObject]@{
            id = 77
            tag_name = "v1.2.3"
            draft = $true
            prerelease = $false
            immutable = $false
            assets = @($remoteAssets | ForEach-Object { $_ })
        }
        if ((Assert-VerifiedDraftRelease -Release $release -Tag "v1.2.3" -CoreAssets $coreAssets) -ne 77) {
            throw "A valid frozen draft did not return its stable Release ID."
        }
        $release.assets[0].state = "new"
        Assert-TestThrows `
            -Action { Assert-VerifiedDraftRelease -Release $release -Tag "v1.2.3" -CoreAssets $coreAssets } `
            -MessagePattern "state, digest, or size" `
            -FailureMessage "A non-uploaded GitHub asset passed draft verification."
        $release.assets[0].state = "uploaded"

        $fakeState = [ordered]@{
            Commands = New-Object Collections.Generic.List[object]
            Created = $false
            Uploaded = $false
            ReleaseId = 91
        }
        $fakeGh = {
            param([string[]]$CommandArguments)
            $fakeState.Commands.Add(@($CommandArguments))
            $output = ""
            if ($CommandArguments[0] -ceq "release" -and $CommandArguments[1] -ceq "create") {
                $fakeState.Created = $true
                $output = "https://github.com/example/vocekit/releases/tag/v1.2.3"
            } elseif ($CommandArguments[0] -ceq "release" -and $CommandArguments[1] -ceq "upload") {
                if (-not $fakeState.Created) { throw "fake upload happened before create" }
                $fakeState.Uploaded = $true
            } elseif ($CommandArguments[0] -ceq "release" -and $CommandArguments[1] -ceq "download") {
                $directoryIndex = [Array]::IndexOf($CommandArguments, "--dir")
                if ($directoryIndex -lt 0) { throw "fake download omitted --dir" }
                $downloadRoot = $CommandArguments[$directoryIndex + 1]
                [void][IO.Directory]::CreateDirectory($downloadRoot)
                foreach ($path in $inputSet.CoreAssetPaths) {
                    [IO.File]::Copy($path, (Join-Path $downloadRoot ([IO.Path]::GetFileName($path))))
                }
            } elseif ($CommandArguments[0] -ceq "api" -and
                $CommandArguments[-1] -match '/releases/tags/') {
                [object[]]$assets = @()
                if ($fakeState.Uploaded) {
                    $assets = @($remoteAssets | ForEach-Object { $_ })
                }
                $output = ([ordered]@{
                    id = $fakeState.ReleaseId
                    tag_name = "v1.2.3"
                    draft = $true
                    prerelease = $false
                    immutable = $false
                    assets = $assets
                } | ConvertTo-Json -Depth 8 -Compress)
            } else {
                throw "Unexpected fake gh command: $($CommandArguments -join ' ')"
            }
            return [PSCustomObject]@{ exit_code = 0; output = $output }
        }

        $downloadDirectory = Join-Path $testRoot "download"
        [void](Invoke-CreateAndVerifyPublishDraft `
            -Repository "example/vocekit" `
            -InputSet $inputSet `
            -CoreAssets $coreAssets `
            -DownloadDirectory $downloadDirectory `
            -GhTestInvoker $fakeGh)
        $commandText = @($fakeState.Commands | ForEach-Object { @($_) -join " " }) -join "`n"
        if ($commandText -match '(?m)(?:^|\s)--clobber(?:\s|$)' -or
            $commandText -notmatch 'release create v1\.2\.3 .*--draft .*--verify-tag' -or
            $commandText -notmatch 'release upload v1\.2\.3' -or
            $commandText -notmatch 'release download v1\.2\.3') {
            throw "Verified-draft gh command construction is incomplete or permits clobbering."
        }
    } finally {
        Close-PublishReadLocks -Locks @($inputSet.Locks)
    }

    [IO.File]::AppendAllText($fixture.ArchivePath, "released after lock")

    $unsafeRoot = Join-Path $testRoot "unsafe-name"
    $unsafeFixture = New-PublishInputFixture -Root $unsafeRoot -EvidenceName "bad#evidence.json"
    Assert-TestThrows `
        -Action {
            $unsafeSet = Open-FinalizedReleaseInputSet `
                -CandidatePath $unsafeFixture.CandidatePath `
                -EvidencePath $unsafeFixture.EvidencePath `
                -ReleaseNotesPath $unsafeFixture.ReleaseNotesPath
            Close-PublishReadLocks -Locks @($unsafeSet.Locks)
        } `
        -MessagePattern "Core Release asset names may only contain" `
        -FailureMessage "An asset name with gh label syntax was accepted."
} finally {
    Remove-PublishTestDirectory -Path $testRoot
}

$gitRoot = Join-Path ([IO.Path]::GetTempPath()) (
    "vocekit-publish-gate-test-" + [Guid]::NewGuid().ToString("N")
)
try {
    $workRoot = Join-Path $gitRoot "work"
    $bareRoot = Join-Path $gitRoot "origin.git"
    [void][IO.Directory]::CreateDirectory($workRoot)
    & git init --quiet --bare $bareRoot
    if ($LASTEXITCODE -ne 0) { throw "Could not initialize bare Git fixture." }
    & git -C $workRoot init --quiet
    [IO.File]::WriteAllText((Join-Path $workRoot "tracked.txt"), "one")
    & git -C $workRoot add -- tracked.txt
    & git -C $workRoot -c user.name=VoceKit-Test -c user.email=test@example.invalid commit --quiet -m first
    if ($LASTEXITCODE -ne 0) { throw "Could not commit Git fixture." }
    $commit = (& git -C $workRoot rev-parse HEAD).Trim()
    $archiveSha = "b" * 64
    $evidenceSha = "c" * 64
    $tagMessage = @"
VoceKit release v1.2.3
source-commit: $commit
archive-sha256: $archiveSha
evidence-sha256: $evidenceSha
"@
    & git -C $workRoot -c user.name=VoceKit-Test -c user.email=test@example.invalid tag -a v1.2.3 -m $tagMessage
    & git -C $workRoot remote add origin $bareRoot
    $tagCandidate = [PSCustomObject]@{
        tag = "v1.2.3"
        source_commit = $commit
        archive = [PSCustomObject]@{ sha256 = $archiveSha }
    }
    $tagState = Get-PublishLocalTagState `
        -RepositoryRoot $workRoot `
        -Candidate $tagCandidate `
        -EvidenceSha256 $evidenceSha
    Assert-PublishRemoteTagAbsent -RepositoryRoot $workRoot -TagState $tagState

    [IO.File]::AppendAllText((Join-Path $workRoot "tracked.txt"), "two")
    & git -C $workRoot add -- tracked.txt
    & git -C $workRoot -c user.name=VoceKit-Test -c user.email=test@example.invalid commit --quiet -m second
    & git -C $workRoot -c user.name=VoceKit-Test -c user.email=test@example.invalid tag -f -a v1.2.3 -m "moved local tag"
    $movedOid = (& git -C $workRoot rev-parse "refs/tags/v1.2.3^{tag}").Trim()
    if ($movedOid -ceq $tagState.object_oid) { throw "Git fixture did not move the local tag." }

    $tagGh = {
        param([string[]]$CommandArguments)
        return [PSCustomObject]@{
            exit_code = 0
            output = ([ordered]@{
                ref = "refs/tags/v1.2.3"
                object = [ordered]@{ type = "tag"; sha = $tagState.object_oid }
            } | ConvertTo-Json -Depth 5 -Compress)
        }
    }
    Push-NewPublishTag `
        -RepositoryRoot $workRoot `
        -Repository "example/vocekit" `
        -TagState $tagState `
        -GhTestInvoker $tagGh
    $remoteOid = ((& git ls-remote --refs $bareRoot refs/tags/v1.2.3) -split '\s+')[0]
    if ($remoteOid.ToLowerInvariant() -cne $tagState.object_oid) {
        throw "Raw tag-object push followed the moved local tag ref."
    }
    Assert-TestThrows `
        -Action { Assert-PublishRemoteTagAbsent -RepositoryRoot $workRoot -TagState $tagState } `
        -MessagePattern "already exists" `
        -FailureMessage "An existing remote tag could be resumed or overwritten."
} finally {
    Remove-PublishTestDirectory -Path $gitRoot
}

Write-Host "Publish finalized release gate tests: PASS"
