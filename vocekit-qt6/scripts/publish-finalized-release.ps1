[CmdletBinding(DefaultParameterSetName = "Draft")]
param(
    [Parameter(Mandatory = $true, ParameterSetName = "Draft")]
    [string]$CandidatePath,
    [Parameter(Mandatory = $true, ParameterSetName = "Draft")]
    [string]$EvidencePath,
    [Parameter(Mandatory = $true, ParameterSetName = "Draft")]
    [string]$ExpectedSignerSubject,
    [Parameter(Mandatory = $true, ParameterSetName = "Draft")]
    [string]$ExpectedSignerThumbprint,
    [Parameter(Mandatory = $true, ParameterSetName = "Draft")]
    [string]$ExpectedUpdateFeedUrl,
    [Parameter(Mandatory = $true, ParameterSetName = "Draft")]
    [string]$ExpectedReleaseBaseUrl,
    [Parameter(Mandatory = $true, ParameterSetName = "Draft")]
    [string]$ExpectedReleasePageBaseUrl,
    [Parameter(Mandatory = $true, ParameterSetName = "Draft")]
    [string]$Repository,
    [Parameter(Mandatory = $true, ParameterSetName = "Draft")]
    [string]$ReleaseNotesPath,
    [Parameter(ParameterSetName = "Draft")]
    [switch]$CreateVerifiedDraft,
    [Parameter(ParameterSetName = "Draft")]
    [switch]$Publish,
    [Parameter(Mandatory = $true, ParameterSetName = "DecisionTest")]
    [switch]$DecisionTestMode
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"
. (Join-Path $PSScriptRoot "release-path-safety.ps1")
. (Join-Path $PSScriptRoot "git-trust-safety.ps1")

function Get-PublishRequiredProperty {
    param(
        [Parameter(Mandatory = $true)]$Object,
        [Parameter(Mandatory = $true)][string]$Name,
        [Parameter(Mandatory = $true)][string]$Context
    )

    $property = $Object.PSObject.Properties[$Name]
    if ($null -eq $property) {
        throw "$Context is missing '$Name'."
    }
    return $property.Value
}

function Read-PublishJsonFile {
    param(
        [Parameter(Mandatory = $true)][string]$Path,
        [Parameter(Mandatory = $true)][string]$Label
    )

    try {
        return Get-Content -LiteralPath $Path -Raw -Encoding UTF8 |
            ConvertFrom-Json -ErrorAction Stop
    } catch {
        throw "$Label is not valid JSON: $($_.Exception.Message)"
    }
}

function Assert-PublishSafeFileName {
    param(
        [Parameter(Mandatory = $true)][string]$Name,
        [Parameter(Mandatory = $true)][string]$Label
    )

    if ([string]::IsNullOrWhiteSpace($Name) -or
        $Name -cne [IO.Path]::GetFileName($Name) -or
        $Name.IndexOfAny([IO.Path]::GetInvalidFileNameChars()) -ge 0 -or
        $Name.EndsWith(" ", [StringComparison]::Ordinal) -or
        $Name.EndsWith(".", [StringComparison]::Ordinal)) {
        throw "$Label must be a single safe file name."
    }
}

function Test-PublishWindowsReservedSegment {
    param([Parameter(Mandatory = $true)][string]$Segment)

    $base = ([IO.Path]::GetFileNameWithoutExtension($Segment)).TrimEnd([char[]]@(' ', '.'))
    return $base -match '^(?i:CON|PRN|AUX|NUL|COM[1-9]|LPT[1-9])$'
}

function Resolve-PublishEvidenceAttachmentPath {
    param(
        [Parameter(Mandatory = $true)][string]$EvidenceDirectory,
        [Parameter(Mandatory = $true)][string]$Reference
    )

    $normalized = $Reference.Replace("\", "/")
    $segments = @($normalized.Split('/') | Where-Object { $_.Length -gt 0 })
    if ([string]::IsNullOrWhiteSpace($normalized) -or
        [IO.Path]::IsPathRooted($Reference) -or
        $normalized.StartsWith("/", [StringComparison]::Ordinal) -or
        $segments.Count -eq 0) {
        throw "Evidence screenshot reference is not a safe relative path: $Reference"
    }
    foreach ($segment in $segments) {
        if ($segment -in @(".", "..") -or
            $segment -match '[<>:"|?*\x00-\x1F]' -or
            $segment.EndsWith(" ", [StringComparison]::Ordinal) -or
            $segment.EndsWith(".", [StringComparison]::Ordinal) -or
            (Test-PublishWindowsReservedSegment -Segment $segment)) {
            throw "Evidence screenshot reference is not a safe relative path: $Reference"
        }
    }
    if ([IO.Path]::GetExtension($segments[-1]).ToLowerInvariant() -notin @(".png", ".jpg", ".jpeg", ".webp")) {
        throw "Evidence screenshot must use a supported image extension: $Reference"
    }

    $evidenceFull = [IO.Path]::GetFullPath($EvidenceDirectory).TrimEnd("\", "/")
    $targetFull = [IO.Path]::GetFullPath((Join-Path $evidenceFull ($segments -join "\")))
    if (-not $targetFull.StartsWith($evidenceFull + "\", [StringComparison]::OrdinalIgnoreCase)) {
        throw "Evidence screenshot escaped its evidence directory: $Reference"
    }
    return $targetFull
}

function Get-PublishFileSnapshot {
    param(
        [Parameter(Mandatory = $true)][string]$Path,
        [Parameter(Mandatory = $true)][string]$Label
    )

    $fullPath = [IO.Path]::GetFullPath($Path)
    [void](Assert-NoReparsePointsInExistingPathChain -Path $fullPath -Label $Label)
    if (-not (Test-Path -LiteralPath $fullPath -PathType Leaf)) {
        throw "$Label is missing: $fullPath"
    }
    $item = Get-Item -LiteralPath $fullPath -Force
    return [PSCustomObject]@{
        path = $fullPath
        name = [IO.Path]::GetFileName($fullPath)
        bytes = [long]$item.Length
        sha256 = (Get-FileHash -LiteralPath $fullPath -Algorithm SHA256).Hash.ToUpperInvariant()
        last_write_utc_ticks = [long]$item.LastWriteTimeUtc.Ticks
    }
}

function Assert-PublishFileBinding {
    param(
        [Parameter(Mandatory = $true)]$Snapshot,
        [Parameter(Mandatory = $true)]$Binding,
        [Parameter(Mandatory = $true)][string]$Label
    )

    $name = [string](Get-PublishRequiredProperty -Object $Binding -Name "name" -Context $Label)
    $bytes = [long](Get-PublishRequiredProperty -Object $Binding -Name "bytes" -Context $Label)
    $sha256 = [string](Get-PublishRequiredProperty -Object $Binding -Name "sha256" -Context $Label)
    if ($name -cne [string]$Snapshot.name -or
        $bytes -ne [long]$Snapshot.bytes -or
        $sha256 -notmatch '^[0-9a-fA-F]{64}$' -or
        $sha256.ToUpperInvariant() -cne [string]$Snapshot.sha256) {
        throw "$Label does not match the locked local file bytes."
    }
}

function Open-PublishReadLocks {
    param([Parameter(Mandatory = $true)][string[]]$Paths)

    $locks = New-Object Collections.Generic.List[IO.FileStream]
    try {
        foreach ($path in $Paths) {
            $fullPath = [IO.Path]::GetFullPath($path)
            [void](Assert-NoReparsePointsInExistingPathChain -Path $fullPath -Label "Frozen release input")
            if (-not (Test-Path -LiteralPath $fullPath -PathType Leaf)) {
                throw "Frozen release input is missing: $fullPath"
            }
            $locks.Add([IO.File]::Open(
                $fullPath,
                [IO.FileMode]::Open,
                [IO.FileAccess]::Read,
                [IO.FileShare]::Read
            ))
        }
        return @($locks | ForEach-Object { $_ })
    } catch {
        foreach ($lock in $locks) { $lock.Dispose() }
        throw
    }
}

function Close-PublishReadLocks {
    param([object[]]$Locks)

    foreach ($lock in @($Locks)) {
        if ($null -ne $lock) { $lock.Dispose() }
    }
}

function Assert-PublishSnapshotsUnchanged {
    param(
        [Parameter(Mandatory = $true)][object[]]$Before,
        [Parameter(Mandatory = $true)][object[]]$After
    )

    if ($Before.Count -ne $After.Count) {
        throw "Frozen release input set changed during draft publication."
    }
    $afterByPath = [Collections.Generic.Dictionary[string, object]]::new([StringComparer]::OrdinalIgnoreCase)
    foreach ($snapshot in $After) {
        if ($afterByPath.ContainsKey([string]$snapshot.path)) {
            throw "Frozen release snapshot contains a duplicate path: $($snapshot.path)"
        }
        $afterByPath.Add([string]$snapshot.path, $snapshot)
    }
    foreach ($beforeSnapshot in $Before) {
        $path = [string]$beforeSnapshot.path
        if (-not $afterByPath.ContainsKey($path)) {
            throw "Frozen release input disappeared during draft publication: $path"
        }
        $afterSnapshot = $afterByPath[$path]
        if ([long]$beforeSnapshot.bytes -ne [long]$afterSnapshot.bytes -or
            [string]$beforeSnapshot.sha256 -cne [string]$afterSnapshot.sha256 -or
            [long]$beforeSnapshot.last_write_utc_ticks -ne [long]$afterSnapshot.last_write_utc_ticks) {
            throw "Frozen release input changed during draft publication: $path"
        }
    }
}

function Open-FinalizedReleaseInputSet {
    param(
        [Parameter(Mandatory = $true)][string]$CandidatePath,
        [Parameter(Mandatory = $true)][string]$EvidencePath,
        [Parameter(Mandatory = $true)][string]$ReleaseNotesPath
    )

    $candidateFull = [IO.Path]::GetFullPath($CandidatePath)
    $evidenceFull = [IO.Path]::GetFullPath($EvidencePath)
    $notesFull = [IO.Path]::GetFullPath($ReleaseNotesPath)
    if ([IO.Path]::GetFileName($candidateFull) -cne "release-candidate.json") {
        throw "CandidatePath must point to release-candidate.json."
    }
    $candidateDirectory = [IO.Path]::GetFullPath((Split-Path -Parent $candidateFull))
    $candidatePrefix = $candidateDirectory.TrimEnd([char[]]@('\', '/')) + "\"
    if ($evidenceFull.StartsWith($candidatePrefix, [StringComparison]::OrdinalIgnoreCase)) {
        throw "EvidencePath must remain external to the immutable candidate directory."
    }
    if ($notesFull.StartsWith($candidatePrefix, [StringComparison]::OrdinalIgnoreCase)) {
        throw "ReleaseNotesPath must remain external to the immutable candidate directory."
    }

    $allLocks = New-Object Collections.Generic.List[object]
    try {
        $primaryPaths = @($candidateFull, $evidenceFull, $notesFull)
        $primarySet = [Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
        foreach ($path in $primaryPaths) {
            if (-not $primarySet.Add($path)) {
                throw "Primary frozen release input path was supplied more than once: $path"
            }
        }
        foreach ($lock in @(Open-PublishReadLocks -Paths $primaryPaths)) { $allLocks.Add($lock) }

        $candidate = Read-PublishJsonFile -Path $candidateFull -Label "Release candidate"
        $evidence = Read-PublishJsonFile -Path $evidenceFull -Label "Acceptance evidence"
        if ([int](Get-PublishRequiredProperty -Object $candidate -Name "schema_version" -Context "Release candidate") -ne 1 -or
            [string](Get-PublishRequiredProperty -Object $candidate -Name "kind" -Context "Release candidate") -cne "vocekit-signed-release-candidate") {
            throw "Release candidate schema is not supported."
        }
        if ([int](Get-PublishRequiredProperty -Object $evidence -Name "schema_version" -Context "Acceptance evidence") -ne 1 -or
            [string](Get-PublishRequiredProperty -Object $evidence -Name "kind" -Context "Acceptance evidence") -cne "vocekit-release-acceptance-evidence") {
            throw "Acceptance evidence schema is not supported."
        }
        $tag = [string](Get-PublishRequiredProperty -Object $candidate -Name "tag" -Context "Release candidate")
        if ($tag -notmatch '^v[0-9A-Za-z][0-9A-Za-z._-]*$' -or
            [IO.Path]::GetFileName($candidateDirectory) -cne $tag) {
            throw "Candidate tag or version-isolated directory is invalid."
        }

        $candidateSnapshot = Get-PublishFileSnapshot -Path $candidateFull -Label "Release candidate"
        Assert-PublishFileBinding `
            -Snapshot $candidateSnapshot `
            -Binding (Get-PublishRequiredProperty -Object $evidence -Name "candidate_record" -Context "Acceptance evidence") `
            -Label "Acceptance evidence candidate_record"

        $artifactPaths = New-Object Collections.Generic.List[string]
        foreach ($field in @("archive", "sidecar", "manifest")) {
            $binding = Get-PublishRequiredProperty -Object $candidate -Name $field -Context "Release candidate"
            $name = [string](Get-PublishRequiredProperty -Object $binding -Name "name" -Context "Release candidate $field")
            Assert-PublishSafeFileName -Name $name -Label "Release candidate $field name"
            $artifactPaths.Add([IO.Path]::GetFullPath((Join-Path $candidateDirectory $name)))
        }

        $screenshotBindings = New-Object Collections.Generic.List[object]
        $screenshotPaths = [Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
        $evidenceDirectory = [IO.Path]::GetFullPath((Split-Path -Parent $evidenceFull))
        foreach ($cell in @(Get-PublishRequiredProperty -Object $evidence -Name "cells" -Context "Acceptance evidence")) {
            foreach ($screenshot in @(Get-PublishRequiredProperty -Object $cell -Name "screenshots" -Context "Acceptance evidence cell")) {
                $reference = [string](Get-PublishRequiredProperty -Object $screenshot -Name "reference" -Context "Evidence screenshot")
                $attachmentPath = Resolve-PublishEvidenceAttachmentPath `
                    -EvidenceDirectory $evidenceDirectory `
                    -Reference $reference
                if (-not $screenshotPaths.Add($attachmentPath)) {
                    throw "Evidence screenshot path is referenced more than once: $reference"
                }
                $screenshotBindings.Add([PSCustomObject]@{
                    path = $attachmentPath
                    sha256 = [string](Get-PublishRequiredProperty -Object $screenshot -Name "sha256" -Context "Evidence screenshot")
                })
            }
        }
        if ($screenshotBindings.Count -eq 0) {
            throw "Acceptance evidence does not reference any screenshots."
        }

        $allPathSet = [Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
        foreach ($path in $primaryPaths) { [void]$allPathSet.Add($path) }
        $derivedPaths = @($artifactPaths | ForEach-Object { $_ }) + @($screenshotPaths | ForEach-Object { $_ })
        foreach ($path in $derivedPaths) {
            if (-not $allPathSet.Add([IO.Path]::GetFullPath($path))) {
                throw "Frozen release input path is duplicated: $path"
            }
        }
        foreach ($lock in @(Open-PublishReadLocks -Paths $derivedPaths)) { $allLocks.Add($lock) }

        $archiveSnapshot = Get-PublishFileSnapshot -Path $artifactPaths[0] -Label "Candidate archive"
        $sidecarSnapshot = Get-PublishFileSnapshot -Path $artifactPaths[1] -Label "Candidate sidecar"
        $manifestSnapshot = Get-PublishFileSnapshot -Path $artifactPaths[2] -Label "Candidate manifest"
        Assert-PublishFileBinding -Snapshot $archiveSnapshot -Binding $candidate.archive -Label "Candidate archive"
        Assert-PublishFileBinding -Snapshot $sidecarSnapshot -Binding $candidate.sidecar -Label "Candidate sidecar"
        Assert-PublishFileBinding -Snapshot $manifestSnapshot -Binding $candidate.manifest -Label "Candidate manifest"
        foreach ($screenshotBinding in $screenshotBindings) {
            $snapshot = Get-PublishFileSnapshot -Path $screenshotBinding.path -Label "Evidence screenshot"
            if ([string]$screenshotBinding.sha256 -notmatch '^[0-9a-fA-F]{64}$' -or
                ([string]$screenshotBinding.sha256).ToUpperInvariant() -cne [string]$snapshot.sha256) {
                throw "Evidence screenshot does not match its locked SHA-256: $($screenshotBinding.path)"
            }
        }

        $allPaths = @($allPathSet | ForEach-Object { $_ })
        $snapshots = @($allPaths | ForEach-Object {
            Get-PublishFileSnapshot -Path $_ -Label "Frozen release input"
        })
        $corePaths = @(
            $archiveSnapshot.path,
            $sidecarSnapshot.path,
            $manifestSnapshot.path,
            $candidateSnapshot.path,
            $evidenceFull
        )
        $coreNames = [Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
        foreach ($path in $corePaths) {
            $coreName = [IO.Path]::GetFileName($path)
            if ($coreName -notmatch '^[A-Za-z0-9._-]+$') {
                throw "Core Release asset names may only contain ASCII letters, digits, dot, underscore, and hyphen: $coreName"
            }
            if (-not $coreNames.Add($coreName)) {
                throw "Core Release asset names must be unique: $coreName"
            }
        }

        return [PSCustomObject]@{
            Candidate = $candidate
            Evidence = $evidence
            CandidatePath = $candidateFull
            EvidencePath = $evidenceFull
            ReleaseNotesPath = $notesFull
            ArchivePath = [string]$archiveSnapshot.path
            SidecarPath = [string]$sidecarSnapshot.path
            ManifestPath = [string]$manifestSnapshot.path
            ScreenshotPaths = @($screenshotPaths | ForEach-Object { $_ })
            Paths = $allPaths
            CoreAssetPaths = $corePaths
            Snapshots = $snapshots
            Locks = @($allLocks | ForEach-Object { $_ })
        }
    } catch {
        Close-PublishReadLocks -Locks @($allLocks | ForEach-Object { $_ })
        throw
    }
}

function Invoke-PublishGitResult {
    param(
        [Parameter(Mandatory = $true)][string]$RepositoryRoot,
        [Parameter(Mandatory = $true)][string[]]$Arguments
    )

    Assert-TrustedGitRepository -RepositoryRoot $RepositoryRoot
    return Invoke-TrustedGitRaw -RepositoryRoot $RepositoryRoot -Arguments $Arguments
}

function Invoke-PublishGit {
    param(
        [Parameter(Mandatory = $true)][string]$RepositoryRoot,
        [Parameter(Mandatory = $true)][string[]]$Arguments
    )

    $result = Invoke-PublishGitResult -RepositoryRoot $RepositoryRoot -Arguments $Arguments
    if ([int]$result.exit_code -ne 0) {
        throw "Git command failed in draft publication gate: git $($Arguments -join ' ')`n$(@($result.output) -join "`n")"
    }
    return @($result.output)
}

function Invoke-PublishGh {
    param(
        [Parameter(Mandatory = $true)][string[]]$Arguments,
        [scriptblock]$TestInvoker = $null
    )

    if ($null -ne $TestInvoker) {
        $result = & $TestInvoker -CommandArguments $Arguments
        if ($null -eq $result -or
            $null -eq $result.PSObject.Properties["exit_code"] -or
            $null -eq $result.PSObject.Properties["output"]) {
            throw "Fake gh invoker returned an invalid result."
        }
        return $result
    }

    try {
        $output = @(& gh @Arguments 2>&1)
        $exitCode = $LASTEXITCODE
    } catch {
        return [PSCustomObject]@{ exit_code = -1; output = $_.Exception.Message }
    }
    return [PSCustomObject]@{
        exit_code = [int]$exitCode
        output = ($output -join "`n")
    }
}

function Invoke-PublishGhRequired {
    param(
        [Parameter(Mandatory = $true)][string[]]$Arguments,
        [scriptblock]$TestInvoker = $null
    )

    $result = Invoke-PublishGh -Arguments $Arguments -TestInvoker $TestInvoker
    if ([int]$result.exit_code -ne 0) {
        throw "GitHub CLI command failed in draft publication gate: gh $($Arguments -join ' ')`n$($result.output)"
    }
    return [string]$result.output
}

function Assert-PublishTagMessageBindings {
    param(
        [Parameter(Mandatory = $true)][string]$Message,
        [Parameter(Mandatory = $true)][string]$Tag,
        [Parameter(Mandatory = $true)][string]$SourceCommit,
        [Parameter(Mandatory = $true)][string]$ArchiveSha256,
        [Parameter(Mandatory = $true)][string]$EvidenceSha256
    )

    if ($Message -notmatch "(?m)^VoceKit release $([regex]::Escape($Tag))\s*$") {
        throw "Annotated tag message is missing the exact release heading."
    }
    foreach ($binding in @(
        @{ Name = "source-commit"; Value = $SourceCommit; Pattern = '[0-9a-fA-F]{40}(?:[0-9a-fA-F]{24})?' },
        @{ Name = "archive-sha256"; Value = $ArchiveSha256; Pattern = '[0-9a-fA-F]{64}' },
        @{ Name = "evidence-sha256"; Value = $EvidenceSha256; Pattern = '[0-9a-fA-F]{64}' }
    )) {
        $matches = [regex]::Matches($Message, "(?m)^$([regex]::Escape($binding.Name)):\s*($($binding.Pattern))\s*$")
        if ($matches.Count -ne 1 -or
            $matches[0].Groups[1].Value.ToUpperInvariant() -cne ([string]$binding.Value).ToUpperInvariant()) {
            throw "Annotated tag message does not exactly bind $($binding.Name)."
        }
    }
}

function Get-PublishLocalTagState {
    param(
        [Parameter(Mandatory = $true)][string]$RepositoryRoot,
        [Parameter(Mandatory = $true)]$Candidate,
        [Parameter(Mandatory = $true)][string]$EvidenceSha256
    )

    $tag = [string]$Candidate.tag
    $tagRef = "refs/tags/$tag"
    $tagOid = (@(Invoke-PublishGit -RepositoryRoot $RepositoryRoot -Arguments @(
        "rev-parse", "--verify", "${tagRef}^{tag}"
    )) -join "").Trim()
    $type = (@(Invoke-PublishGit -RepositoryRoot $RepositoryRoot -Arguments @("cat-file", "-t", $tagOid)) -join "").Trim()
    if ($type -cne "tag") { throw "Local release tag is not an annotated tag: $tag" }
    $tagCommit = (@(Invoke-PublishGit -RepositoryRoot $RepositoryRoot -Arguments @(
        "rev-parse", "--verify", "${tagRef}^{commit}"
    )) -join "").Trim()
    if ($tagOid -notmatch '^[0-9a-fA-F]{40}([0-9a-fA-F]{24})?$' -or
        $tagCommit -cne [string]$Candidate.source_commit) {
        throw "Local annotated tag object or target commit does not match the candidate."
    }
    $message = @(Invoke-PublishGit -RepositoryRoot $RepositoryRoot -Arguments @(
        "for-each-ref", "--format=%(contents)", $tagRef
    )) -join "`n"
    Assert-PublishTagMessageBindings `
        -Message $message `
        -Tag $tag `
        -SourceCommit ([string]$Candidate.source_commit) `
        -ArchiveSha256 ([string]$Candidate.archive.sha256) `
        -EvidenceSha256 $EvidenceSha256
    return [PSCustomObject]@{
        tag = $tag
        ref = $tagRef
        object_oid = $tagOid.ToLowerInvariant()
        commit = $tagCommit.ToLowerInvariant()
        message = $message
    }
}

function Assert-PublishRepositoryAndUrls {
    param(
        [Parameter(Mandatory = $true)][string]$Repository,
        [Parameter(Mandatory = $true)][string]$ExpectedUpdateFeedUrl,
        [Parameter(Mandatory = $true)][string]$ExpectedReleaseBaseUrl,
        [Parameter(Mandatory = $true)][string]$ExpectedReleasePageBaseUrl
    )

    if ($Repository -notmatch '^[A-Za-z0-9_.-]+/[A-Za-z0-9_.-]+$') {
        throw "Repository must use the exact owner/name form."
    }
    $expected = @(
        "https://api.github.com/repos/$Repository/releases/latest",
        "https://github.com/$Repository/releases/download",
        "https://github.com/$Repository/releases/tag"
    )
    $actual = @($ExpectedUpdateFeedUrl, $ExpectedReleaseBaseUrl, $ExpectedReleasePageBaseUrl)
    for ($index = 0; $index -lt $expected.Count; $index++) {
        if ($actual[$index] -cne $expected[$index]) {
            throw "Release URL inputs do not exactly bind the requested GitHub repository."
        }
    }
}

function Assert-PublishOriginMatchesRepository {
    param(
        [Parameter(Mandatory = $true)][string]$RepositoryRoot,
        [Parameter(Mandatory = $true)][string]$Repository
    )

    $origin = (@(Invoke-PublishGit -RepositoryRoot $RepositoryRoot -Arguments @(
        "remote", "get-url", "--push", "origin"
    )) -join "").Trim()
    $escapedRepository = [regex]::Escape($Repository)
    if ($origin -notmatch "^(?i:https://github\.com/$escapedRepository(?:\.git)?/?)$" -and
        $origin -notmatch "^(?i:git@github\.com:$escapedRepository(?:\.git)?)$" -and
        $origin -notmatch "^(?i:ssh://git@github\.com/$escapedRepository(?:\.git)?/?)$") {
        throw "Git origin does not match the requested GitHub repository."
    }
}

function Assert-PublishRemoteTagAbsent {
    param(
        [Parameter(Mandatory = $true)][string]$RepositoryRoot,
        [Parameter(Mandatory = $true)]$TagState
    )

    $result = Invoke-PublishGitResult -RepositoryRoot $RepositoryRoot -Arguments @(
        "ls-remote", "--exit-code", "--refs", "origin", [string]$TagState.ref
    )
    if ([int]$result.exit_code -eq 0) {
        throw "Remote Git tag already exists; refusing to overwrite or resume it: $($TagState.ref)"
    }
    if ([int]$result.exit_code -ne 2) {
        throw "Could not prove that the remote Git tag is absent.`n$(@($result.output) -join "`n")"
    }
}

function Assert-PublishRemoteTagExact {
    param(
        [Parameter(Mandatory = $true)][string]$RepositoryRoot,
        [Parameter(Mandatory = $true)][string]$Repository,
        [Parameter(Mandatory = $true)]$TagState,
        [scriptblock]$GhTestInvoker = $null
    )

    $tagResult = Invoke-PublishGitResult -RepositoryRoot $RepositoryRoot -Arguments @(
        "ls-remote", "--exit-code", "--refs", "origin", [string]$TagState.ref
    )
    if ([int]$tagResult.exit_code -ne 0) {
        throw "Remote Git tag could not be read after the frozen push.`n$(@($tagResult.output) -join "`n")"
    }
    $remoteLines = @($tagResult.output)
    if ($remoteLines.Count -ne 1) {
        throw "Remote Git tag lookup did not return exactly one ref."
    }
    $tagMatch = [regex]::Match(
        [string]$remoteLines[0],
        '^([0-9a-fA-F]{40}(?:[0-9a-fA-F]{24})?)\s+(.+)$'
    )
    if (-not $tagMatch.Success -or
        $tagMatch.Groups[1].Value.ToLowerInvariant() -cne [string]$TagState.object_oid -or
        $tagMatch.Groups[2].Value -cne [string]$TagState.ref) {
        throw "Remote Git tag ref does not resolve to the frozen annotated tag object."
    }

    $peeledRef = "$($TagState.ref)^{}"
    $commitResult = Invoke-PublishGitResult -RepositoryRoot $RepositoryRoot -Arguments @(
        "ls-remote", "--exit-code", "origin", $peeledRef
    )
    if ([int]$commitResult.exit_code -ne 0 -or @($commitResult.output).Count -ne 1) {
        throw "Remote annotated tag target commit could not be read."
    }
    $commitMatch = [regex]::Match(
        [string]@($commitResult.output)[0],
        '^([0-9a-fA-F]{40}(?:[0-9a-fA-F]{24})?)\s+(.+)$'
    )
    if (-not $commitMatch.Success -or
        $commitMatch.Groups[1].Value.ToLowerInvariant() -cne [string]$TagState.commit -or
        $commitMatch.Groups[2].Value -cne $peeledRef) {
        throw "Remote annotated tag does not peel to the candidate source commit."
    }

    $tagName = [Uri]::EscapeDataString([string]$TagState.tag)
    $json = Invoke-PublishGhRequired -Arguments @(
        "api", "-H", "Accept: application/vnd.github+json",
        "-H", "X-GitHub-Api-Version: 2022-11-28",
        "repos/$Repository/git/ref/tags/$tagName"
    ) -TestInvoker $GhTestInvoker
    try { $remoteRef = $json | ConvertFrom-Json -ErrorAction Stop } catch {
        throw "GitHub tag-ref API returned invalid JSON: $($_.Exception.Message)"
    }
    $object = Get-PublishRequiredProperty -Object $remoteRef -Name "object" -Context "GitHub tag ref"
    if ([string](Get-PublishRequiredProperty -Object $object -Name "type" -Context "GitHub tag ref object") -cne "tag" -or
        ([string](Get-PublishRequiredProperty -Object $object -Name "sha" -Context "GitHub tag ref object")).ToLowerInvariant() -cne
            [string]$TagState.object_oid) {
        throw "GitHub tag-ref API does not resolve to the locked annotated tag object."
    }
}

function Push-NewPublishTag {
    param(
        [Parameter(Mandatory = $true)][string]$RepositoryRoot,
        [Parameter(Mandatory = $true)][string]$Repository,
        [Parameter(Mandatory = $true)]$TagState,
        [scriptblock]$GhTestInvoker = $null
    )

    # Freeze the raw annotated-tag object ID. Never push the mutable local ref:
    # another local process can move refs/tags/<tag> after finalization.
    $refspec = "$($TagState.object_oid):$($TagState.ref)"
    $result = Invoke-PublishGitResult -RepositoryRoot $RepositoryRoot -Arguments @(
        "push", "--porcelain", "origin", $refspec
    )
    if ([int]$result.exit_code -ne 0) {
        throw "Exact annotated tag-object push failed; no force or overwrite is allowed.`n$(@($result.output) -join "`n")"
    }
    $newTagPattern = '(?m)^\*\t' + [regex]::Escape($refspec) + '\t\[new tag\]\s*$'
    if ((@($result.output) -join "`n") -notmatch $newTagPattern) {
        throw "Git did not report creation of one new frozen tag; up-to-date or resumed tags are rejected."
    }
    Assert-PublishRemoteTagExact `
        -RepositoryRoot $RepositoryRoot `
        -Repository $Repository `
        -TagState $TagState `
        -GhTestInvoker $GhTestInvoker
}

function Assert-PublishReleaseDoesNotExist {
    param(
        [Parameter(Mandatory = $true)][string]$Repository,
        [Parameter(Mandatory = $true)][string]$Tag,
        [scriptblock]$GhTestInvoker = $null
    )

    $tagName = [Uri]::EscapeDataString($Tag)
    $result = Invoke-PublishGh -Arguments @(
        "api", "--include", "-H", "Accept: application/vnd.github+json",
        "-H", "X-GitHub-Api-Version: 2022-11-28",
        "repos/$Repository/releases/tags/$tagName"
    ) -TestInvoker $GhTestInvoker
    if ([int]$result.exit_code -eq 0) {
        throw "A GitHub Release already exists for tag $Tag; refusing to overwrite or resume it."
    }
    if ([string]$result.output -notmatch '(?m)^HTTP/\S+\s+404(?:\s|$)') {
        throw "Could not prove that the GitHub Release tag is absent; expected an authenticated API 404.`n$($result.output)"
    }
}

function Assert-PublishRemotePreflight {
    param(
        [Parameter(Mandatory = $true)][string]$Repository,
        [Parameter(Mandatory = $true)][string]$Tag,
        [scriptblock]$GhTestInvoker = $null
    )

    [void](Invoke-PublishGhRequired -Arguments @("auth", "status") -TestInvoker $GhTestInvoker)
    $immutableJson = Invoke-PublishGhRequired -Arguments @(
        "api", "-H", "Accept: application/vnd.github+json",
        "-H", "X-GitHub-Api-Version: 2026-03-10",
        "repos/$Repository/immutable-releases"
    ) -TestInvoker $GhTestInvoker
    try { $immutable = $immutableJson | ConvertFrom-Json -ErrorAction Stop } catch {
        throw "GitHub immutable-release preflight returned invalid JSON: $($_.Exception.Message)"
    }
    $enabled = Get-PublishRequiredProperty `
        -Object $immutable `
        -Name "enabled" `
        -Context "GitHub immutable-release setting"
    if ($enabled -isnot [bool] -or -not [bool]$enabled) {
        throw "GitHub immutable Releases must be enabled before a verified draft can be created."
    }
    Assert-PublishReleaseDoesNotExist `
        -Repository $Repository `
        -Tag $Tag `
        -GhTestInvoker $GhTestInvoker
}

function Get-PublishCoreAssetSnapshots {
    param([Parameter(Mandatory = $true)]$InputSet)

    return @($InputSet.CoreAssetPaths | ForEach-Object {
        Get-PublishFileSnapshot -Path $_ -Label "Frozen core Release asset"
    })
}

function Assert-VerifiedDraftRelease {
    param(
        [Parameter(Mandatory = $true)]$Release,
        [Parameter(Mandatory = $true)][string]$Tag,
        [Parameter(Mandatory = $true)][object[]]$CoreAssets,
        [long]$ExpectedReleaseId = 0,
        [switch]$ExpectEmptyAssets
    )

    $releaseId = [long](Get-PublishRequiredProperty -Object $Release -Name "id" -Context "GitHub Release")
    $isDraft = Get-PublishRequiredProperty -Object $Release -Name "draft" -Context "GitHub Release"
    $isImmutable = Get-PublishRequiredProperty -Object $Release -Name "immutable" -Context "GitHub Release"
    $isPrerelease = Get-PublishRequiredProperty -Object $Release -Name "prerelease" -Context "GitHub Release"
    if ($releaseId -le 0 -or
        ($ExpectedReleaseId -gt 0 -and $releaseId -ne $ExpectedReleaseId) -or
        [string](Get-PublishRequiredProperty -Object $Release -Name "tag_name" -Context "GitHub Release") -cne $Tag -or
        $isDraft -isnot [bool] -or -not [bool]$isDraft -or
        $isPrerelease -isnot [bool] -or [bool]$isPrerelease -or
        $isImmutable -isnot [bool] -or [bool]$isImmutable) {
        throw "GitHub Release is not the expected mutable draft for the locked tag."
    }
    $remoteAssets = @(Get-PublishRequiredProperty -Object $Release -Name "assets" -Context "GitHub Release")
    if ($ExpectEmptyAssets) {
        if ($remoteAssets.Count -ne 0) {
            throw "New GitHub draft Release was not empty before the frozen upload."
        }
        return $releaseId
    }
    if ($remoteAssets.Count -ne $CoreAssets.Count) {
        throw "Draft Release does not contain exactly the frozen core asset set."
    }
    $remoteNames = [Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
    foreach ($remoteAsset in $remoteAssets) {
        $remoteName = [string](Get-PublishRequiredProperty -Object $remoteAsset -Name "name" -Context "Draft Release asset")
        if (-not $remoteNames.Add($remoteName)) {
            throw "Draft Release contains a duplicate asset name: $remoteName"
        }
    }
    foreach ($local in $CoreAssets) {
        $matches = @($remoteAssets | Where-Object { [string]$_.name -ceq [string]$local.name })
        if ($matches.Count -ne 1) {
            throw "Draft Release asset is missing or duplicated: $($local.name)"
        }
        $remote = $matches[0]
        $assetId = [long](Get-PublishRequiredProperty -Object $remote -Name "id" -Context "Draft Release asset")
        $state = [string](Get-PublishRequiredProperty -Object $remote -Name "state" -Context "Draft Release asset")
        $digest = [string](Get-PublishRequiredProperty -Object $remote -Name "digest" -Context "Draft Release asset")
        $size = [long](Get-PublishRequiredProperty -Object $remote -Name "size" -Context "Draft Release asset")
        if ($assetId -le 0 -or $state -cne "uploaded" -or
            $digest -notmatch '^sha256:[0-9a-fA-F]{64}$' -or
            $digest.Substring(7).ToUpperInvariant() -cne [string]$local.sha256 -or
            $size -ne [long]$local.bytes) {
            throw "Draft Release asset state, digest, or size does not match locked local bytes: $($local.name)"
        }
    }
    return $releaseId
}

function Get-PublishDraftRelease {
    param(
        [Parameter(Mandatory = $true)][string]$Repository,
        [Parameter(Mandatory = $true)][string]$Tag,
        [scriptblock]$GhTestInvoker = $null
    )

    $tagName = [Uri]::EscapeDataString($Tag)
    $json = Invoke-PublishGhRequired -Arguments @(
        "api", "-H", "Accept: application/vnd.github+json",
        "-H", "X-GitHub-Api-Version: 2026-03-10",
        "repos/$Repository/releases/tags/$tagName"
    ) -TestInvoker $GhTestInvoker
    try { return $json | ConvertFrom-Json -ErrorAction Stop } catch {
        throw "GitHub draft Release query returned invalid JSON: $($_.Exception.Message)"
    }
}

function Invoke-CreateAndVerifyPublishDraft {
    param(
        [Parameter(Mandatory = $true)][string]$Repository,
        [Parameter(Mandatory = $true)]$InputSet,
        [Parameter(Mandatory = $true)][object[]]$CoreAssets,
        [Parameter(Mandatory = $true)][string]$DownloadDirectory,
        [scriptblock]$GhTestInvoker = $null
    )

    $tag = [string]$InputSet.Candidate.tag
    [void](Invoke-PublishGhRequired -Arguments @(
        "release", "create", $tag, "--repo", $Repository,
        "--draft", "--verify-tag", "--title", "VoceKit $($InputSet.Candidate.version)",
        "--notes-file", [string]$InputSet.ReleaseNotesPath
    ) -TestInvoker $GhTestInvoker)

    $emptyDraft = Get-PublishDraftRelease -Repository $Repository -Tag $tag -GhTestInvoker $GhTestInvoker
    $releaseId = Assert-VerifiedDraftRelease `
        -Release $emptyDraft `
        -Tag $tag `
        -CoreAssets $CoreAssets `
        -ExpectEmptyAssets

    $uploadArguments = @("release", "upload", $tag, "--repo", $Repository) + @($InputSet.CoreAssetPaths)
    if ($uploadArguments -contains "--clobber") {
        throw "Internal error: draft upload command must never use --clobber."
    }
    [void](Invoke-PublishGhRequired -Arguments $uploadArguments -TestInvoker $GhTestInvoker)

    $draft = Get-PublishDraftRelease -Repository $Repository -Tag $tag -GhTestInvoker $GhTestInvoker
    [void](Assert-VerifiedDraftRelease `
        -Release $draft `
        -Tag $tag `
        -CoreAssets $CoreAssets `
        -ExpectedReleaseId $releaseId)

    if ([IO.Directory]::Exists($DownloadDirectory) -and
        @(Get-ChildItem -LiteralPath $DownloadDirectory -Force).Count -ne 0) {
        throw "Draft download verification directory must start empty."
    }
    [void][IO.Directory]::CreateDirectory($DownloadDirectory)
    [void](Invoke-PublishGhRequired -Arguments @(
        "release", "download", $tag, "--repo", $Repository,
        "--dir", $DownloadDirectory
    ) -TestInvoker $GhTestInvoker)
    $downloadedFiles = @(Get-ChildItem -LiteralPath $DownloadDirectory -Force -File)
    if ($downloadedFiles.Count -ne $CoreAssets.Count) {
        throw "Downloaded draft does not contain exactly the frozen core asset set."
    }
    foreach ($local in $CoreAssets) {
        $downloadMatches = @($downloadedFiles | Where-Object { $_.Name -ceq [string]$local.name })
        if ($downloadMatches.Count -ne 1) {
            throw "Downloaded draft asset is missing or duplicated: $($local.name)"
        }
        $downloaded = Get-PublishFileSnapshot -Path $downloadMatches[0].FullName -Label "Downloaded draft asset"
        if ([long]$downloaded.bytes -ne [long]$local.bytes -or
            [string]$downloaded.sha256 -cne [string]$local.sha256) {
            throw "Downloaded draft asset does not match the locked local bytes: $($local.name)"
        }
    }
    $finalDraft = Get-PublishDraftRelease -Repository $Repository -Tag $tag -GhTestInvoker $GhTestInvoker
    [void](Assert-VerifiedDraftRelease `
        -Release $finalDraft `
        -Tag $tag `
        -CoreAssets $CoreAssets `
        -ExpectedReleaseId $releaseId)
    return $finalDraft
}

if ($DecisionTestMode) {
    return
}

# Public promotion is deliberately unavailable: the current updater protocol
# has no detached publisher signature or certificate rotation policy. Keep the
# parameter reserved so automation cannot silently mistake this draft gate for
# a public publisher when that protocol is implemented later.
if ($Publish) {
    throw "Public promotion is disabled: the detached publisher-signature protocol, certificate rotation policy, and machine-verifiable PublicationApproval/N-1 evidence are not implemented. No tag or Release was modified."
}

Assert-PublishRepositoryAndUrls `
    -Repository $Repository `
    -ExpectedUpdateFeedUrl $ExpectedUpdateFeedUrl `
    -ExpectedReleaseBaseUrl $ExpectedReleaseBaseUrl `
    -ExpectedReleasePageBaseUrl $ExpectedReleasePageBaseUrl
if ([string]::IsNullOrWhiteSpace($ExpectedSignerSubject)) {
    throw "ExpectedSignerSubject must not be blank."
}
$normalizedThumbprint = ($ExpectedSignerThumbprint -replace '\s', '').ToUpperInvariant()
if ($normalizedThumbprint -notmatch '^[0-9A-F]{40,64}$') {
    throw "ExpectedSignerThumbprint must be a complete hexadecimal certificate thumbprint."
}

$projectRoot = [IO.Path]::GetFullPath((Split-Path -Parent $PSScriptRoot))
$RepositoryRoot = [IO.Path]::GetFullPath((Join-Path $projectRoot ".."))
$actualRepositoryRoot = (@(Invoke-PublishGit `
    -RepositoryRoot $RepositoryRoot `
    -Arguments @("rev-parse", "--show-toplevel")) -join "").Trim()
if ([IO.Path]::GetFullPath($actualRepositoryRoot) -cne $RepositoryRoot) {
    throw "Publisher script directory is not inside the canonical Git repository root."
}
Assert-PublishOriginMatchesRepository -RepositoryRoot $RepositoryRoot -Repository $Repository

$finalizer = Join-Path $PSScriptRoot "finalize-existing-release-candidate.ps1"
$inputSet = $null
$downloadDirectory = Join-Path ([IO.Path]::GetTempPath()) ("vocekit-draft-download-" + [Guid]::NewGuid().ToString("N"))
try {
    # Candidate/evidence are locked before their contents are trusted; derived
    # artifact and screenshot locks are then added without releasing them. All
    # handles stay open through finalization, tag verification, upload, remote
    # digest checks, and the downloaded-ZIP hash check.
    $inputSet = Open-FinalizedReleaseInputSet `
        -CandidatePath $CandidatePath `
        -EvidencePath $EvidencePath `
        -ReleaseNotesPath $ReleaseNotesPath
    $snapshotBefore = @($inputSet.Snapshots)

    & $finalizer `
        -CandidatePath $inputSet.CandidatePath `
        -EvidencePath $inputSet.EvidencePath `
        -ExpectedSignerSubject $ExpectedSignerSubject `
        -ExpectedSignerThumbprint $normalizedThumbprint `
        -ExpectedUpdateFeedUrl $ExpectedUpdateFeedUrl `
        -ExpectedReleaseBaseUrl $ExpectedReleaseBaseUrl `
        -ExpectedReleasePageBaseUrl $ExpectedReleasePageBaseUrl

    $evidenceSnapshot = Get-PublishFileSnapshot -Path $inputSet.EvidencePath -Label "Locked acceptance evidence"
    $tagState = Get-PublishLocalTagState `
        -RepositoryRoot $RepositoryRoot `
        -Candidate $inputSet.Candidate `
        -EvidenceSha256 ([string]$evidenceSnapshot.sha256)

    $coreAssets = @(Get-PublishCoreAssetSnapshots -InputSet $inputSet)
    # Complete every available read-only local and remote check before the first
    # irreversible tag push. Existing tags or Releases are never resumed,
    # deleted, forced, or overwritten by this gate.
    Assert-PublishRemoteTagAbsent -RepositoryRoot $RepositoryRoot -TagState $tagState
    Assert-PublishRemotePreflight `
        -Repository $Repository `
        -Tag ([string]$tagState.tag)

    if (-not $CreateVerifiedDraft) {
        Write-Host "Verified draft publication plan (read-only; no remote writes performed):"
        Write-Host "  Repository: $Repository"
        Write-Host "  Frozen tag object: $($tagState.object_oid)"
        Write-Host "  Frozen tag target: $($tagState.commit)"
        foreach ($asset in $coreAssets) {
            Write-Host "  Asset: $($asset.name) bytes=$($asset.bytes) sha256=$($asset.sha256)"
        }
        Write-Host "Re-run with -CreateVerifiedDraft to push the exact tag object and create a new verified draft."
        Write-Host "Public promotion remains blocked; -Publish always fails closed."
    } else {
        Push-NewPublishTag `
            -RepositoryRoot $RepositoryRoot `
            -Repository $Repository `
            -TagState $tagState
        [void](Invoke-CreateAndVerifyPublishDraft `
            -Repository $Repository `
            -InputSet $inputSet `
            -CoreAssets $coreAssets `
            -DownloadDirectory $downloadDirectory)
        # Re-read both Git transports after all uploads. This script leaves a
        # mutable draft; a future public-promotion protocol must repeat these
        # checks immediately before its own signed action.
        Assert-PublishRemoteTagExact `
            -RepositoryRoot $RepositoryRoot `
            -Repository $Repository `
            -TagState $tagState

        Write-Host "Frozen GitHub draft Release created and verified without changing local candidate bytes."
        Write-Host "  Tag object OID: $($tagState.object_oid)"
        Write-Host "  Tag: $($tagState.tag)"
        Write-Host "  Draft remains unpublished. Public promotion is blocked until detached publisher signatures and publication approval evidence are implemented."
    }

    $snapshotAfter = @($inputSet.Paths | ForEach-Object {
        Get-PublishFileSnapshot -Path $_ -Label "Frozen release input"
    })
    Assert-PublishSnapshotsUnchanged -Before $snapshotBefore -After $snapshotAfter

} finally {
    try {
        if ($null -ne $inputSet) {
            $snapshotFinal = @($inputSet.Paths | ForEach-Object {
                Get-PublishFileSnapshot -Path $_ -Label "Frozen release input"
            })
            Assert-PublishSnapshotsUnchanged -Before @($inputSet.Snapshots) -After $snapshotFinal
        }
    } finally {
        if ($null -ne $inputSet) {
            Close-PublishReadLocks -Locks @($inputSet.Locks)
        }
        if ([IO.Directory]::Exists($downloadDirectory)) {
            [IO.Directory]::Delete($downloadDirectory, $true)
        }
    }
}
