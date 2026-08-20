Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"
Add-Type -AssemblyName System.IO.Compression
Add-Type -AssemblyName System.IO.Compression.FileSystem
Add-Type -AssemblyName System.Drawing

$scriptsRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$finalizeScript = Join-Path $scriptsRoot "scripts\finalize-existing-release-candidate.ps1"

$finalizeTokens = $null
$finalizeParseErrors = $null
$finalizeAst = [Management.Automation.Language.Parser]::ParseFile(
    $finalizeScript,
    [ref]$finalizeTokens,
    [ref]$finalizeParseErrors
)
$finalizeParameterNames = @($finalizeAst.ParamBlock.Parameters | ForEach-Object {
    $_.Name.VariablePath.UserPath
})
if ($finalizeParseErrors.Count -ne 0 -or $finalizeParameterNames -contains "RepositoryRoot") {
    throw "Finalizer production parameters expose a replaceable repository trust root or do not parse."
}

$finalizeSource = [IO.File]::ReadAllText($finalizeScript)
$runtimeGateStart = $finalizeSource.IndexOf(
    "function Assert-ExtractedReleaseRuntime {",
    [StringComparison]::Ordinal
)
$runtimeGateEnd = $finalizeSource.IndexOf(
    "function Get-ImmutableReleaseFileSnapshots",
    $runtimeGateStart,
    [StringComparison]::Ordinal
)
if ($runtimeGateStart -lt 0 -or $runtimeGateEnd -le $runtimeGateStart) {
    throw "Finalizer extracted-runtime trust gate could not be located."
}
$runtimeGateBody = $finalizeSource.Substring($runtimeGateStart, $runtimeGateEnd - $runtimeGateStart)
$authenticodeGateIndex = $runtimeGateBody.IndexOf(
    "Assert-ExtractedReleaseAuthenticode",
    [StringComparison]::Ordinal
)
$postSignatureOperations = @(
    "Assert-NoUnsignedTestMarker",
    '& $PackageScript',
    "Assert-ExtractedRuntimeHelperBindings",
    '& $ProvenanceVerifier',
    '& $RuntimeVerifier'
)
if ($authenticodeGateIndex -lt 0) {
    throw "Finalizer extracted-runtime trust gate has no Authenticode check."
}
foreach ($operation in $postSignatureOperations) {
    $operationIndex = $runtimeGateBody.IndexOf($operation, [StringComparison]::Ordinal)
    if ($operationIndex -le $authenticodeGateIndex) {
        throw "Finalizer may inspect or launch extracted runtime content before Authenticode: $operation"
    }
}

. $finalizeScript -DecisionTestMode

function Copy-TestJsonObject {
    param(
        [Parameter(Mandatory = $true)]$Value,
        [Parameter(Mandatory = $true)][int]$Depth
    )

    $jsonText = $Value | ConvertTo-Json -Depth $Depth
    $convertFromJson = Get-Command ConvertFrom-Json -ErrorAction Stop
    if ($convertFromJson.Parameters.ContainsKey("DateKind")) {
        return $jsonText | ConvertFrom-Json -DateKind String
    }
    return $jsonText | ConvertFrom-Json
}

$reparseTestRoot = Join-Path ([IO.Path]::GetTempPath()) ("vocekit-release-reparse-test-" + [Guid]::NewGuid().ToString("N"))
try {
    $reparseTarget = Join-Path $reparseTestRoot "target"
    $reparseJunction = Join-Path $reparseTestRoot "junction"
    [void][IO.Directory]::CreateDirectory($reparseTarget)
    [IO.File]::WriteAllText((Join-Path $reparseTarget "evidence.png"), "test")
    [void](New-Item -ItemType Junction -Path $reparseJunction -Target $reparseTarget -ErrorAction Stop)
    $reparseWasRejected = $false
    try {
        [void](Assert-NoReparsePointsInExistingPathChain `
            -Path (Join-Path $reparseJunction "evidence.png") `
            -Label "Test evidence")
    } catch {
        $reparseWasRejected = $_.Exception.Message -match "reparse point"
    }
    if (-not $reparseWasRejected) {
        throw "Release path safety accepted an input through a directory junction."
    }
} finally {
    if ([IO.Directory]::Exists($reparseJunction)) {
        [IO.Directory]::Delete($reparseJunction, $false)
    }
    if ([IO.Directory]::Exists($reparseTestRoot)) {
        [IO.Directory]::Delete($reparseTestRoot, $true)
    }
}

$fakeImagePath = Join-Path ([IO.Path]::GetTempPath()) ("vocekit-fake-screenshot-" + [Guid]::NewGuid().ToString("N") + ".png")
try {
    [IO.File]::WriteAllText($fakeImagePath, "not a png")
    $fakeImageWasRejected = $false
    try {
        Assert-SupportedEvidenceImageFile -Path $fakeImagePath
    } catch {
        $fakeImageWasRejected = $_.Exception.Message -match "not a supported image"
    }
    if (-not $fakeImageWasRejected) {
        throw "Acceptance evidence allowed a text file renamed to .png."
    }
} finally {
    if ([IO.File]::Exists($fakeImagePath)) {
        [IO.File]::Delete($fakeImagePath)
    }
}

$truncatedImageFixtures = @(
    [PSCustomObject]@{
        Name = "jpeg"
        Extension = ".jpg"
        Bytes = [byte[]]@(0xFF, 0xD8, 0xFF, 0x00, 0xFF, 0xD9)
    },
    [PSCustomObject]@{
        Name = "png"
        Extension = ".png"
        Bytes = [byte[]]@(
            0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A,
            0x00, 0x00, 0x00, 0x0D, 0x49, 0x48, 0x44, 0x52,
            0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01
        )
    },
    [PSCustomObject]@{
        Name = "webp"
        Extension = ".webp"
        Bytes = [byte[]]@(
            0x52, 0x49, 0x46, 0x46,
            0x08, 0x00, 0x00, 0x00,
            0x57, 0x45, 0x42, 0x50,
            0x56, 0x50, 0x38, 0x20
        )
    }
)
foreach ($fixture in $truncatedImageFixtures) {
    $truncatedImagePath = Join-Path ([IO.Path]::GetTempPath()) (
        "vocekit-truncated-$($fixture.Name)-" + [Guid]::NewGuid().ToString("N") + $fixture.Extension
    )
    try {
        [IO.File]::WriteAllBytes($truncatedImagePath, $fixture.Bytes)
        $truncatedImageWasRejected = $false
        try {
            Assert-SupportedEvidenceImageFile -Path $truncatedImagePath
        } catch {
            $truncatedImageWasRejected = $true
        }
        if (-not $truncatedImageWasRejected) {
            throw "Acceptance evidence allowed a truncated $($fixture.Name) header as a real screenshot."
        }
    } finally {
        if ([IO.File]::Exists($truncatedImagePath)) {
            [IO.File]::Delete($truncatedImagePath)
        }
    }
}

$tinyImagePath = Join-Path ([IO.Path]::GetTempPath()) (
    "vocekit-tiny-screenshot-" + [Guid]::NewGuid().ToString("N") + ".png"
)
$tinyBitmap = [Drawing.Bitmap]::new(1, 1)
try {
    $tinyBitmap.Save($tinyImagePath, [Drawing.Imaging.ImageFormat]::Png)
    $tinyImageWasRejected = $false
    try {
        Assert-SupportedEvidenceImageFile -Path $tinyImagePath
    } catch {
        $tinyImageWasRejected = $_.Exception.Message -match "decoded dimensions"
    }
    if (-not $tinyImageWasRejected) {
        throw "Acceptance evidence allowed a 1x1 placeholder as a desktop screenshot."
    }
} finally {
    $tinyBitmap.Dispose()
    if ([IO.File]::Exists($tinyImagePath)) {
        [IO.File]::Delete($tinyImagePath)
    }
}

function New-TestCandidate {
    return [PSCustomObject]@{
        schema_version = 1
        kind = "vocekit-signed-release-candidate"
        created_at = "2026-08-20T10:00:00Z"
        source_commit = "a" * 40
        version = "0.2.0"
        tag = "v0.2.0"
        package_name = "vocekit-qt6-portable"
        binary_provenance = [PSCustomObject]@{
            schema_version = 3
            source_commit = "a" * 40
            source_tree_clean = $true
            configuration = "release"
            version = "0.2.0"
            update_feed_url = "https://api.github.com/repos/example/vocekit/releases/latest"
        }
        runtime_helpers = @(
            @{ helper_name = "vocekit-windows-ocr"; relative_path = "ocr/windows/vocekit-windows-ocr.exe" },
            @{ helper_name = "vocekit-rapidocr"; relative_path = "ocr/rapidocr/vocekit-rapidocr.exe" },
            @{ helper_name = "vocekit-windows-speech"; relative_path = "speech/windows/vocekit-windows-speech.exe" }
        ) | ForEach-Object {
            [PSCustomObject]@{
                helper_name = $_.helper_name
                relative_path = $_.relative_path
                sha256 = "d" * 64
                provenance = [PSCustomObject]@{
                    schema_version = 1
                    kind = "vocekit-runtime-helper-build-provenance"
                    helper_name = $_.helper_name
                    source_commit = "a" * 40
                    source_tree_clean = $true
                    configuration = "Release"
                }
            }
        }
        urls = [PSCustomObject]@{
            update_feed_url = "https://api.github.com/repos/example/vocekit/releases/latest"
            release_base_url = "https://github.com/example/vocekit/releases/download"
            release_page_base_url = "https://github.com/example/vocekit/releases/tag"
        }
        archive = [PSCustomObject]@{
            name = "vocekit-qt6-portable.zip"
            bytes = 123L
            sha256 = "b" * 64
        }
        sidecar = [PSCustomObject]@{
            name = "vocekit-qt6-portable.zip.sha256"
            bytes = 100L
            sha256 = "c" * 64
        }
        manifest = [PSCustomObject]@{
            name = "update-manifest.json"
            bytes = 200L
            sha256 = "d" * 64
        }
        signer = [PSCustomObject]@{
            subject = "CN=VoceKit Release Test"
            thumbprint = "0123456789ABCDEF0123456789ABCDEF01234567"
            timestamp_subject = "CN=Timestamp Test"
            timestamp_thumbprint = "89ABCDEF0123456789ABCDEF0123456789ABCDEF"
        }
    }
}

function Get-TestStringSha256 {
    param([Parameter(Mandatory = $true)][string]$Value)

    $sha = [Security.Cryptography.SHA256]::Create()
    try {
        return ([BitConverter]::ToString(
            $sha.ComputeHash([Text.Encoding]::UTF8.GetBytes($Value))
        )).Replace("-", "").ToLowerInvariant()
    } finally {
        $sha.Dispose()
    }
}

function Write-TestPng {
    param(
        [Parameter(Mandatory = $true)][string]$Path,
        [Parameter(Mandatory = $true)][int]$Index
    )

    $bitmap = [Drawing.Bitmap]::new(640, 360)
    $graphics = [Drawing.Graphics]::FromImage($bitmap)
    try {
        $graphics.Clear([Drawing.Color]::FromArgb(
            255,
            ($Index * 37) % 256,
            ($Index * 73) % 256,
            ($Index * 109) % 256
        ))
        $bitmap.Save($Path, [Drawing.Imaging.ImageFormat]::Png)
    } finally {
        $graphics.Dispose()
        $bitmap.Dispose()
    }
}

function New-TestEvidence {
    param([Parameter(Mandatory = $true)]$Candidate)

    $cells = New-Object Collections.Generic.List[object]
    foreach ($categoryId in Get-RequiredAcceptanceCategoryIds) {
        foreach ($scale in @(100, 125, 150, 200)) {
            $applications = switch ($categoryId) {
                "system_text_editor" { @([PSCustomObject]@{ id = "windows_notepad"; name = "Windows Notepad"; version = "11.0" }) }
                "office_word_processor" { @([PSCustomObject]@{ id = "microsoft_word"; name = "Microsoft Word"; version = "2024" }) }
                "office_spreadsheet_presentation" { @([PSCustomObject]@{ id = "microsoft_excel"; name = "Microsoft Excel"; version = "2024" }) }
                "browser" { @(
                    [PSCustomObject]@{ id = "microsoft_edge"; name = "Microsoft Edge"; version = "test" },
                    [PSCustomObject]@{ id = "google_chrome"; name = "Google Chrome"; version = "test" }
                ) }
                "instant_messaging" { @([PSCustomObject]@{ id = "wechat"; name = "WeChat"; version = "test" }) }
                "collaboration_office" { @([PSCustomObject]@{ id = "feishu"; name = "Feishu"; version = "test" }) }
                "code_text_editor" { @([PSCustomObject]@{ id = "visual_studio_code"; name = "Visual Studio Code"; version = "test" }) }
                "pdf_readonly" { @([PSCustomObject]@{ id = "edge_pdf"; name = "Microsoft Edge PDF"; version = "test" }) }
            }
            $screenshots = @($applications | ForEach-Object {
                $screenshotReference = "$categoryId-$scale-$($_.id).png"
                [PSCustomObject]@{
                    application_id = $_.id
                    reference = $screenshotReference
                    sha256 = Get-TestStringSha256 -Value $screenshotReference
                }
            })
            $checks = @(
                Get-RequiredAcceptanceCheckIds | ForEach-Object {
                    [PSCustomObject]@{ id = $_; status = "passed" }
                }
            )
            $cells.Add([PSCustomObject]@{
                category_id = $categoryId
                scale_percent = $scale
                status = "passed"
                application_name = ($applications.name -join " + ")
                application_version = ($applications.version -join " + ")
                applications = $applications
                windows_version = "Windows 11 test"
                display_resolution = "1920x1080"
                monitor_coordinates = "0,0,1920,1080"
                tested_at = "2026-08-20T12:00:00Z"
                screenshots = $screenshots
                issues = @()
                checks = $checks
            })
        }
    }
    return [PSCustomObject]@{
        schema_version = 1
        kind = "vocekit-release-acceptance-evidence"
        source_commit = $Candidate.source_commit
        version = $Candidate.version
        tag = $Candidate.tag
        archive = [PSCustomObject]@{
            name = $Candidate.archive.name
            bytes = $Candidate.archive.bytes
            sha256 = $Candidate.archive.sha256
        }
        candidate_record = [PSCustomObject]@{
            name = "release-candidate.json"
            bytes = 321L
            sha256 = "f" * 64
        }
        sidecar = [PSCustomObject]@{
            name = $Candidate.sidecar.name
            bytes = $Candidate.sidecar.bytes
            sha256 = $Candidate.sidecar.sha256
        }
        manifest = [PSCustomObject]@{
            name = $Candidate.manifest.name
            bytes = $Candidate.manifest.bytes
            sha256 = $Candidate.manifest.sha256
        }
        overall_status = "passed"
        cells = @($cells | ForEach-Object { $_ })
    }
}

$candidate = New-TestCandidate
$evidence = New-TestEvidence -Candidate $candidate
Assert-CandidateRecordSchema `
    -Candidate $candidate `
    -ExpectedSignerSubject $candidate.signer.subject `
    -ExpectedSignerThumbprint $candidate.signer.thumbprint `
    -ExpectedUpdateFeedUrl $candidate.urls.update_feed_url `
    -ExpectedReleaseBaseUrl $candidate.urls.release_base_url `
    -ExpectedReleasePageBaseUrl $candidate.urls.release_page_base_url
$dirtyCandidate = Copy-TestJsonObject -Value $candidate -Depth 8
$dirtyCandidate.binary_provenance.source_tree_clean = $false
$dirtyCandidateWasRejected = $false
try {
    Assert-CandidateRecordSchema `
        -Candidate $dirtyCandidate `
        -ExpectedSignerSubject $dirtyCandidate.signer.subject `
        -ExpectedSignerThumbprint $dirtyCandidate.signer.thumbprint `
        -ExpectedUpdateFeedUrl $dirtyCandidate.urls.update_feed_url `
        -ExpectedReleaseBaseUrl $dirtyCandidate.urls.release_base_url `
        -ExpectedReleasePageBaseUrl $dirtyCandidate.urls.release_page_base_url
} catch {
    $dirtyCandidateWasRejected = $_.Exception.Message -match "clean Git source tree"
}
if (-not $dirtyCandidateWasRejected) {
    throw "The release finalizer accepted candidate metadata from a dirty source tree."
}
$debugCandidate = Copy-TestJsonObject -Value $candidate -Depth 8
$debugCandidate.binary_provenance.configuration = "debug"
$debugCandidateWasRejected = $false
try {
    Assert-CandidateRecordSchema `
        -Candidate $debugCandidate `
        -ExpectedSignerSubject $debugCandidate.signer.subject `
        -ExpectedSignerThumbprint $debugCandidate.signer.thumbprint `
        -ExpectedUpdateFeedUrl $debugCandidate.urls.update_feed_url `
        -ExpectedReleaseBaseUrl $debugCandidate.urls.release_base_url `
        -ExpectedReleasePageBaseUrl $debugCandidate.urls.release_page_base_url
} catch {
    $debugCandidateWasRejected = $_.Exception.Message -match "configuration"
}
if (-not $debugCandidateWasRejected) {
    throw "The release finalizer accepted candidate metadata from a clean Debug executable."
}
$staleHelperCandidate = Copy-TestJsonObject -Value $candidate -Depth 8
$staleHelperCandidate.runtime_helpers[0].provenance.source_commit = "f" * 40
$staleHelperWasRejected = $false
try {
    Assert-CandidateRecordSchema `
        -Candidate $staleHelperCandidate `
        -ExpectedSignerSubject $staleHelperCandidate.signer.subject `
        -ExpectedSignerThumbprint $staleHelperCandidate.signer.thumbprint `
        -ExpectedUpdateFeedUrl $staleHelperCandidate.urls.update_feed_url `
        -ExpectedReleaseBaseUrl $staleHelperCandidate.urls.release_base_url `
        -ExpectedReleasePageBaseUrl $staleHelperCandidate.urls.release_page_base_url
} catch {
    $staleHelperWasRejected = $_.Exception.Message -match "source_commit"
}
if (-not $staleHelperWasRejected) {
    throw "The release finalizer accepted a runtime helper built from another source commit."
}
$prereleaseCandidate = Copy-TestJsonObject -Value $candidate -Depth 8
$prereleaseCandidate.version = "0.2.0-beta.1"
$prereleaseCandidate.tag = "v0.2.0-beta.1"
$prereleaseCandidate.binary_provenance.version = $prereleaseCandidate.version
$prereleaseWasRejected = $false
try {
    Assert-CandidateRecordSchema `
        -Candidate $prereleaseCandidate `
        -ExpectedSignerSubject $prereleaseCandidate.signer.subject `
        -ExpectedSignerThumbprint $prereleaseCandidate.signer.thumbprint `
        -ExpectedUpdateFeedUrl $prereleaseCandidate.urls.update_feed_url `
        -ExpectedReleaseBaseUrl $prereleaseCandidate.urls.release_base_url `
        -ExpectedReleasePageBaseUrl $prereleaseCandidate.urls.release_page_base_url
} catch {
    $prereleaseWasRejected = $_.Exception.Message -match "stable x.y.z"
}
if (-not $prereleaseWasRejected) {
    throw "The release finalizer accepted a prerelease candidate under the stable-only protocol."
}
Assert-ReleaseAcceptanceEvidence -Evidence $evidence -Candidate $candidate

$offsetTimestampEvidence = Copy-TestJsonObject -Value $evidence -Depth 10
$offsetTimestampEvidence.cells[0].tested_at = "2026-08-20T20:00:00+08:00"
Assert-ReleaseAcceptanceEvidence -Evidence $offsetTimestampEvidence -Candidate $candidate

$missingTimezoneEvidence = Copy-TestJsonObject -Value $evidence -Depth 10
$missingTimezoneEvidence.cells[0].tested_at = "2026-08-20T12:00:00"
$missingTimezoneWasRejected = $false
try {
    Assert-ReleaseAcceptanceEvidence -Evidence $missingTimezoneEvidence -Candidate $candidate
} catch {
    $missingTimezoneWasRejected = $_.Exception.Message -match "RFC 3339.*explicit timezone"
}
if (-not $missingTimezoneWasRejected) {
    throw "Acceptance evidence allowed tested_at without an explicit RFC 3339 timezone."
}

$singleBrowserEvidence = Copy-TestJsonObject -Value $evidence -Depth 10
$browserCell = @($singleBrowserEvidence.cells | Where-Object { $_.category_id -eq "browser" })[0]
$browserCell.applications = @($browserCell.applications | Where-Object { $_.id -ne "google_chrome" })
$singleBrowserWasRejected = $false
try {
    Assert-ReleaseAcceptanceEvidence -Evidence $singleBrowserEvidence -Candidate $candidate
} catch {
    $singleBrowserWasRejected = $_.Exception.Message -match "google_chrome"
}
if (-not $singleBrowserWasRejected) {
    throw "Browser acceptance evidence did not require both Edge and Chrome."
}

$missingBrowserScreenshotEvidence = Copy-TestJsonObject -Value $evidence -Depth 10
$missingBrowserScreenshotCell = @($missingBrowserScreenshotEvidence.cells | Where-Object { $_.category_id -eq "browser" })[0]
$missingBrowserScreenshotCell.screenshots = @(
    $missingBrowserScreenshotCell.screenshots |
        Where-Object { $_.application_id -ne "google_chrome" }
)
$missingBrowserScreenshotWasRejected = $false
try {
    Assert-ReleaseAcceptanceEvidence -Evidence $missingBrowserScreenshotEvidence -Candidate $candidate
} catch {
    $missingBrowserScreenshotWasRejected = $_.Exception.Message -match "screenshot.*google_chrome"
}
if (-not $missingBrowserScreenshotWasRejected) {
    throw "Browser acceptance evidence did not require screenshots for both Edge and Chrome."
}

$tagMessage = @"
VoceKit release v0.2.0

source-commit: $($candidate.source_commit)
archive-sha256: $($candidate.archive.sha256)
evidence-sha256: $("9" * 64)
"@
Assert-ReleaseTagMessageBindings `
    -Message $tagMessage `
    -Tag $candidate.tag `
    -SourceCommit $candidate.source_commit `
    -ArchiveSha256 $candidate.archive.sha256 `
    -EvidenceSha256 ("9" * 64)
$wrongTagBindingWasRejected = $false
try {
    Assert-ReleaseTagMessageBindings `
        -Message $tagMessage `
        -Tag $candidate.tag `
        -SourceCommit $candidate.source_commit `
        -ArchiveSha256 $candidate.archive.sha256 `
        -EvidenceSha256 ("8" * 64)
} catch {
    $wrongTagBindingWasRejected = $_.Exception.Message -match "evidence-sha256"
}
if (-not $wrongTagBindingWasRejected) {
    throw "The finalizer accepted an annotated tag that did not bind the evidence bytes."
}

$gitGateRoot = Join-Path ([IO.Path]::GetTempPath()) ("vocekit-finalize-git-test-" + [Guid]::NewGuid().ToString("N"))
try {
    $gitRepository = Join-Path $gitGateRoot "repository"
    $gitProject = Join-Path $gitRepository "vocekit-qt6"
    $bareOrigin = Join-Path $gitGateRoot "origin.git"
    $gitEvidencePath = Join-Path $gitGateRoot "acceptance.json"
    [void][IO.Directory]::CreateDirectory($gitProject)
    [IO.File]::WriteAllText((Join-Path $gitProject "APP_VERSION"), "0.2.0`n")
    [IO.File]::WriteAllText((Join-Path $gitRepository "tracked.txt"), "tracked")
    Push-Location $gitRepository
    try {
        & git init --quiet
        & git checkout -q -b main
        & git config user.name "VoceKit Release Test"
        & git config user.email "release-test@example.invalid"
        & git add --all
        & git commit -q -m "test release source"
        $gitSourceCommit = (& git rev-parse HEAD).Trim()
    } finally {
        Pop-Location
    }
    & git init --quiet --bare $bareOrigin
    Push-Location $gitRepository
    try {
        & git remote add origin $bareOrigin
        & git push -q -u origin main
    } finally {
        Pop-Location
    }
    [IO.File]::WriteAllText($gitEvidencePath, "external evidence")
    $gitEvidenceHash = (Get-FileHash -LiteralPath $gitEvidencePath -Algorithm SHA256).Hash.ToLowerInvariant()
    $gitCandidate = New-TestCandidate
    $gitCandidate.source_commit = $gitSourceCommit
    $gitTagMessage = @"
VoceKit release v0.2.0

source-commit: $gitSourceCommit
archive-sha256: $($gitCandidate.archive.sha256)
evidence-sha256: $gitEvidenceHash
"@
    Push-Location $gitRepository
    try {
        & git tag -a v0.2.0 -m $gitTagMessage
    } finally {
        Pop-Location
    }
    Assert-ReleaseFinalizationGitState `
        -RepositoryRoot $gitRepository `
        -ProjectRoot $gitProject `
        -Candidate $gitCandidate `
        -EvidencePath $gitEvidencePath
} finally {
    if ([IO.Directory]::Exists($gitGateRoot)) {
        Get-ChildItem -LiteralPath $gitGateRoot -Force -Recurse | ForEach-Object {
            $_.Attributes = [IO.FileAttributes]::Normal
        }
        [IO.Directory]::Delete($gitGateRoot, $true)
    }
}

$duplicateEvidence = Copy-TestJsonObject -Value $evidence -Depth 8
$duplicateEvidence.cells[31] = $duplicateEvidence.cells[0]
$duplicateWasRejected = $false
try {
    Assert-ReleaseAcceptanceEvidence -Evidence $duplicateEvidence -Candidate $candidate
} catch {
    $duplicateWasRejected = $_.Exception.Message -match "duplicate matrix cell"
}
if (-not $duplicateWasRejected) {
    throw "Acceptance evidence allowed a duplicate matrix cell and a missing required cell."
}

$missingEvidence = Copy-TestJsonObject -Value $evidence -Depth 8
$missingEvidence.cells = @($missingEvidence.cells | Select-Object -First 31)
$missingWasRejected = $false
try {
    Assert-ReleaseAcceptanceEvidence -Evidence $missingEvidence -Candidate $candidate
} catch {
    $missingWasRejected = $_.Exception.Message -match "exactly 32 matrix cells"
}
if (-not $missingWasRejected) {
    throw "Acceptance evidence allowed a missing matrix cell."
}

$duplicateCheckEvidence = Copy-TestJsonObject -Value $evidence -Depth 8
$duplicateCheckEvidence.cells[0].checks[6] = $duplicateCheckEvidence.cells[0].checks[0]
$duplicateCheckWasRejected = $false
try {
    Assert-ReleaseAcceptanceEvidence -Evidence $duplicateCheckEvidence -Candidate $candidate
} catch {
    $duplicateCheckWasRejected = $_.Exception.Message -match "duplicate check"
}
if (-not $duplicateCheckWasRejected) {
    throw "Acceptance evidence allowed a duplicate check and a missing required check."
}

$archiveSafetyRoot = Join-Path ([IO.Path]::GetTempPath()) ("vocekit-archive-safety-test-" + [Guid]::NewGuid().ToString("N"))
try {
    [void][IO.Directory]::CreateDirectory($archiveSafetyRoot)
    $safeArchive = Join-Path $archiveSafetyRoot "safe.zip"
    $safeStaging = Join-Path $archiveSafetyRoot "safe-staging"
    $safeExtraction = Join-Path $archiveSafetyRoot "safe-extraction"
    [void][IO.Directory]::CreateDirectory((Join-Path $safeStaging "folder"))
    [IO.File]::WriteAllText((Join-Path $safeStaging "folder\file.txt"), "safe")
    [IO.File]::WriteAllText((Join-Path $safeStaging "second.txt"), "safe")
    [IO.Compression.ZipFile]::CreateFromDirectory($safeStaging, $safeArchive)
    Expand-VerifiedReleaseArchive -ArchivePath $safeArchive -DestinationPath $safeExtraction
    if (-not (Test-Path -LiteralPath (Join-Path $safeExtraction "folder\file.txt") -PathType Leaf)) {
        throw "Safe release archive was not extracted."
    }

    $unsafeArchive = Join-Path $archiveSafetyRoot "unsafe.zip"
    $unsafeStream = [IO.File]::Open($unsafeArchive, [IO.FileMode]::CreateNew)
    try {
        $unsafeZip = [IO.Compression.ZipArchive]::new($unsafeStream, [IO.Compression.ZipArchiveMode]::Create, $false)
        try {
            $entry = $unsafeZip.CreateEntry("../escaped.txt")
            $writer = [IO.StreamWriter]::new($entry.Open())
            try { $writer.Write("escape") } finally { $writer.Dispose() }
        } finally {
            $unsafeZip.Dispose()
        }
    } finally {
        $unsafeStream.Dispose()
    }
    $unsafeWasRejected = $false
    try {
        Expand-VerifiedReleaseArchive `
            -ArchivePath $unsafeArchive `
            -DestinationPath (Join-Path $archiveSafetyRoot "unsafe-extraction")
    } catch {
        $unsafeWasRejected = $_.Exception.Message -match "unsafe entry path"
    }
    if (-not $unsafeWasRejected) {
        throw "The finalizer extracted an archive path outside its temporary directory."
    }

    $entryLimitWasEnforced = $false
    try {
        Expand-VerifiedReleaseArchive `
            -ArchivePath $safeArchive `
            -DestinationPath (Join-Path $archiveSafetyRoot "limited-extraction") `
            -MaxEntryCount 1
    } catch {
        $entryLimitWasEnforced = $_.Exception.Message -match "maximum entry count"
    }
    if (-not $entryLimitWasEnforced) {
        throw "The finalizer did not enforce the ZIP entry-count limit."
    }

    $reservedArchive = Join-Path $archiveSafetyRoot "reserved.zip"
    $reservedStream = [IO.File]::Open($reservedArchive, [IO.FileMode]::CreateNew)
    try {
        $reservedZip = [IO.Compression.ZipArchive]::new($reservedStream, [IO.Compression.ZipArchiveMode]::Create, $false)
        try {
            $entry = $reservedZip.CreateEntry("CON.txt")
            $writer = [IO.StreamWriter]::new($entry.Open())
            try { $writer.Write("reserved") } finally { $writer.Dispose() }
        } finally {
            $reservedZip.Dispose()
        }
    } finally {
        $reservedStream.Dispose()
    }
    $reservedNameWasRejected = $false
    try {
        Expand-VerifiedReleaseArchive `
            -ArchivePath $reservedArchive `
            -DestinationPath (Join-Path $archiveSafetyRoot "reserved-extraction")
    } catch {
        $reservedNameWasRejected = $_.Exception.Message -match "unsafe entry path"
    }
    if (-not $reservedNameWasRejected) {
        throw "The finalizer accepted a Windows reserved device name in the ZIP."
    }
} finally {
    if ([IO.Directory]::Exists($archiveSafetyRoot)) {
        [IO.Directory]::Delete($archiveSafetyRoot, $true)
    }
}

$markerRoot = Join-Path ([IO.Path]::GetTempPath()) ("vocekit-unsigned-marker-test-" + [Guid]::NewGuid().ToString("N"))
try {
    [void][IO.Directory]::CreateDirectory($markerRoot)
    Assert-NoUnsignedTestMarker -RuntimeDirectory $markerRoot
    [IO.File]::WriteAllText((Join-Path $markerRoot "UNSIGNED_TEST_BUILD"), "test only")
    $unsignedMarkerWasRejected = $false
    try {
        Assert-NoUnsignedTestMarker -RuntimeDirectory $markerRoot
    } catch {
        $unsignedMarkerWasRejected = $_.Exception.Message -match "UNSIGNED_TEST_BUILD"
    }
    if (-not $unsignedMarkerWasRejected) {
        throw "The finalizer accepted an archive carrying the unsigned-test marker."
    }
} finally {
    if ([IO.Directory]::Exists($markerRoot)) {
        [IO.Directory]::Delete($markerRoot, $true)
    }
}

$unsignedBinaryRoot = Join-Path ([IO.Path]::GetTempPath()) ("vocekit-unsigned-binary-test-" + [Guid]::NewGuid().ToString("N"))
try {
    [void][IO.Directory]::CreateDirectory($unsignedBinaryRoot)
    [IO.File]::WriteAllText((Join-Path $unsignedBinaryRoot "vocekit.exe"), "not a signed PE")
    $candidateSigner = (New-TestCandidate).signer
    $unsignedBinaryWasRejected = $false
    try {
        Assert-ExtractedReleaseAuthenticode `
            -RuntimeDirectory $unsignedBinaryRoot `
            -CandidateSigner $candidateSigner `
            -ExpectedSignerSubject $candidateSigner.subject `
            -ExpectedSignerThumbprint $candidateSigner.thumbprint
    } catch {
        $unsignedBinaryWasRejected = $_.Exception.Message -match "Authenticode verification failed"
    }
    if (-not $unsignedBinaryWasRejected) {
        throw "The finalizer accepted an unsigned executable in the extracted release."
    }

    $probeMarker = Join-Path $unsignedBinaryRoot "untrusted-probe-executed.txt"
    $probeScript = Join-Path $unsignedBinaryRoot "probe-marker.ps1"
    $escapedProbeMarker = $probeMarker.Replace("'", "''")
    $probeScriptSource = @"
param(
    `$ValidationOnly,
    `$ValidationPath,
    `$ExecutablePath,
    `$ExpectedHelperName,
    `$ExpectedSourceCommit,
    `$ExpectedVersion,
    `$ExpectedUpdateFeedUrl,
    `$ExpectedConfiguration,
    `$Configuration,
    `$RuntimeDir
)
[IO.File]::WriteAllText('$escapedProbeMarker', 'executed')
"@
    [IO.File]::WriteAllText($probeScript, $probeScriptSource, [Text.UTF8Encoding]::new($false))
    $untrustedCandidate = New-TestCandidate
    foreach ($binding in @($untrustedCandidate.runtime_helpers)) {
        $helperPath = Join-Path $unsignedBinaryRoot ([string]$binding.relative_path).Replace("/", "\")
        [void][IO.Directory]::CreateDirectory((Split-Path -Parent $helperPath))
        [IO.File]::WriteAllText($helperPath, "unsigned helper: $($binding.helper_name)")
        $binding.sha256 = (Get-FileHash -LiteralPath $helperPath -Algorithm SHA256).Hash.ToLowerInvariant()
    }
    $unsignedRuntimeGateRejected = $false
    try {
        Assert-ExtractedReleaseRuntime `
            -RuntimeDirectory $unsignedBinaryRoot `
            -Candidate $untrustedCandidate `
            -ExpectedSignerSubject $untrustedCandidate.signer.subject `
            -ExpectedSignerThumbprint $untrustedCandidate.signer.thumbprint `
            -ExpectedUpdateFeedUrl $untrustedCandidate.urls.update_feed_url `
            -PackageScript $probeScript `
            -HelperProvenanceVerifier $probeScript `
            -ProvenanceVerifier $probeScript `
            -RuntimeVerifier $probeScript
    } catch {
        $unsignedRuntimeGateRejected = $_.Exception.Message -match "Authenticode verification failed"
    }
    if (-not $unsignedRuntimeGateRejected) {
        throw "The extracted-runtime gate did not reject unsigned content at Authenticode."
    }
    if ([IO.File]::Exists($probeMarker)) {
        throw "The finalizer launched an extracted-runtime probe before rejecting unsigned content."
    }
} finally {
    if ([IO.Directory]::Exists($unsignedBinaryRoot)) {
        [IO.Directory]::Delete($unsignedBinaryRoot, $true)
    }
}

function Get-TestFileBinding {
    param([Parameter(Mandatory = $true)][string]$Path)

    $item = Get-Item -LiteralPath $Path
    return [PSCustomObject]@{
        name = $item.Name
        bytes = [long]$item.Length
        sha256 = (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLowerInvariant()
    }
}

$fixtureRoot = Join-Path ([IO.Path]::GetTempPath()) ("vocekit-finalize-metadata-test-" + [Guid]::NewGuid().ToString("N"))
try {
    $fixtureProjectRoot = Join-Path $fixtureRoot "project"
    $releaseDirectory = Join-Path $fixtureProjectRoot "dist\releases\v0.2.0"
    $evidenceDirectory = Join-Path $fixtureRoot "evidence"
    [void][IO.Directory]::CreateDirectory($releaseDirectory)
    [void][IO.Directory]::CreateDirectory($evidenceDirectory)

    $archivePath = Join-Path $releaseDirectory "vocekit-qt6-portable.zip"
    $sidecarPath = "$archivePath.sha256"
    $manifestPath = Join-Path $releaseDirectory "update-manifest.json"
    $candidatePath = Join-Path $releaseDirectory "release-candidate.json"
    $evidencePath = Join-Path $evidenceDirectory "v0.2.0-acceptance.json"
    [IO.File]::WriteAllText($archivePath, "immutable archive bytes")
    $archiveHash = (Get-FileHash -LiteralPath $archivePath -Algorithm SHA256).Hash.ToLowerInvariant()
    [IO.File]::WriteAllText($sidecarPath, "$archiveHash  vocekit-qt6-portable.zip`r`n", [Text.Encoding]::ASCII)
    $manifest = [ordered]@{
        schema_version = 1
        version = "0.2.0"
        channel = "stable"
        release_name = "VoceKit 0.2.0"
        release_notes = "test"
        published_at = "2026-08-20T10:00:00Z"
        release_page_url = "https://github.com/example/vocekit/releases/tag/v0.2.0"
        asset_name = "vocekit-qt6-portable.zip"
        download_url = "https://github.com/example/vocekit/releases/download/v0.2.0/vocekit-qt6-portable.zip"
        checksum_url = "https://github.com/example/vocekit/releases/download/v0.2.0/vocekit-qt6-portable.zip.sha256"
        sha256 = $archiveHash
        prerelease = $false
    }
    $manifestJson = $manifest | ConvertTo-Json -Depth 4
    [IO.File]::WriteAllText($manifestPath, $manifestJson, [Text.UTF8Encoding]::new($false))

    $fixtureCandidate = New-TestCandidate
    $fixtureCandidate.archive = Get-TestFileBinding -Path $archivePath
    $fixtureCandidate.sidecar = Get-TestFileBinding -Path $sidecarPath
    $fixtureCandidate.manifest = Get-TestFileBinding -Path $manifestPath
    $candidateJson = $fixtureCandidate | ConvertTo-Json -Depth 6
    [IO.File]::WriteAllText($candidatePath, $candidateJson, [Text.UTF8Encoding]::new($false))
    $fixtureEvidence = New-TestEvidence -Candidate $fixtureCandidate
    $fixtureEvidence.candidate_record = Get-TestFileBinding -Path $candidatePath
    $screenshotIndex = 0
    foreach ($cell in $fixtureEvidence.cells) {
        foreach ($screenshot in $cell.screenshots) {
            $screenshotPath = Join-Path $evidenceDirectory $screenshot.reference
            Write-TestPng -Path $screenshotPath -Index $screenshotIndex
            $screenshot.sha256 = (Get-FileHash -LiteralPath $screenshotPath -Algorithm SHA256).Hash.ToLowerInvariant()
            $screenshotIndex++
        }
    }
    [IO.File]::WriteAllText($evidencePath, ($fixtureEvidence | ConvertTo-Json -Depth 8), [Text.UTF8Encoding]::new($false))

    $verified = Confirm-ReleaseCandidateMetadata `
        -CandidatePath $candidatePath `
        -EvidencePath $evidencePath `
        -ExpectedSignerSubject $fixtureCandidate.signer.subject `
        -ExpectedSignerThumbprint $fixtureCandidate.signer.thumbprint `
        -ExpectedUpdateFeedUrl $fixtureCandidate.urls.update_feed_url `
        -ExpectedReleaseBaseUrl $fixtureCandidate.urls.release_base_url `
        -ExpectedReleasePageBaseUrl $fixtureCandidate.urls.release_page_base_url `
        -ProjectRoot $fixtureProjectRoot
    if ($verified.ArchivePath -cne [IO.Path]::GetFullPath($archivePath) -or
        $verified.SidecarPath -cne [IO.Path]::GetFullPath($sidecarPath) -or
        $verified.ManifestPath -cne [IO.Path]::GetFullPath($manifestPath) -or
        @($verified.EvidenceAttachmentPaths).Count -ne 36) {
        throw "The metadata finalizer did not resolve the candidate's immutable artifacts."
    }

    $externalCandidateDirectory = Join-Path $fixtureRoot "external\v0.2.0"
    [void][IO.Directory]::CreateDirectory($externalCandidateDirectory)
    foreach ($path in @($candidatePath, $archivePath, $sidecarPath, $manifestPath)) {
        [IO.File]::Copy($path, (Join-Path $externalCandidateDirectory ([IO.Path]::GetFileName($path))))
    }
    $externalCandidateWasRejected = $false
    try {
        Confirm-ReleaseCandidateMetadata `
            -CandidatePath (Join-Path $externalCandidateDirectory "release-candidate.json") `
            -EvidencePath $evidencePath `
            -ExpectedSignerSubject $fixtureCandidate.signer.subject `
            -ExpectedSignerThumbprint $fixtureCandidate.signer.thumbprint `
            -ExpectedUpdateFeedUrl $fixtureCandidate.urls.update_feed_url `
            -ExpectedReleaseBaseUrl $fixtureCandidate.urls.release_base_url `
            -ExpectedReleasePageBaseUrl $fixtureCandidate.urls.release_page_base_url `
            -ProjectRoot $fixtureProjectRoot | Out-Null
    } catch {
        $externalCandidateWasRejected = $_.Exception.Message -match "canonical project dist/releases"
    }
    if (-not $externalCandidateWasRejected) {
        throw "The finalizer accepted a hand-built candidate outside canonical project dist/releases."
    }

    [IO.File]::AppendAllText($candidatePath, " ")
    $tamperedCandidateRecordWasRejected = $false
    try {
        Confirm-ReleaseCandidateMetadata `
            -CandidatePath $candidatePath `
            -EvidencePath $evidencePath `
            -ExpectedSignerSubject $fixtureCandidate.signer.subject `
            -ExpectedSignerThumbprint $fixtureCandidate.signer.thumbprint `
            -ExpectedUpdateFeedUrl $fixtureCandidate.urls.update_feed_url `
            -ExpectedReleaseBaseUrl $fixtureCandidate.urls.release_base_url `
            -ExpectedReleasePageBaseUrl $fixtureCandidate.urls.release_page_base_url `
            -ProjectRoot $fixtureProjectRoot | Out-Null
    } catch {
        $tamperedCandidateRecordWasRejected = $_.Exception.Message -match "immutable candidate byte binding"
    }
    if (-not $tamperedCandidateRecordWasRejected) {
        throw "External acceptance evidence did not anchor the candidate JSON bytes."
    }
    [IO.File]::WriteAllText($candidatePath, $candidateJson, [Text.UTF8Encoding]::new($false))

    $tamperedScreenshotPath = $verified.EvidenceAttachmentPaths[0]
    $originalScreenshotBytes = [IO.File]::ReadAllBytes($tamperedScreenshotPath)
    [IO.File]::AppendAllText($tamperedScreenshotPath, "tampered")
    $tamperedScreenshotWasRejected = $false
    try {
        Confirm-ReleaseCandidateMetadata `
            -CandidatePath $candidatePath `
            -EvidencePath $evidencePath `
            -ExpectedSignerSubject $fixtureCandidate.signer.subject `
            -ExpectedSignerThumbprint $fixtureCandidate.signer.thumbprint `
            -ExpectedUpdateFeedUrl $fixtureCandidate.urls.update_feed_url `
            -ExpectedReleaseBaseUrl $fixtureCandidate.urls.release_base_url `
            -ExpectedReleasePageBaseUrl $fixtureCandidate.urls.release_page_base_url `
            -ProjectRoot $fixtureProjectRoot | Out-Null
    } catch {
        $tamperedScreenshotWasRejected = $_.Exception.Message -match "screenshot bytes"
    }
    if (-not $tamperedScreenshotWasRejected) {
        throw "The finalizer accepted a screenshot whose bytes did not match the evidence hash."
    }
    [IO.File]::WriteAllBytes($tamperedScreenshotPath, $originalScreenshotBytes)

    [IO.File]::AppendAllText($archivePath, "tampered")
    $tamperedArchiveWasRejected = $false
    try {
        Confirm-ReleaseCandidateMetadata `
            -CandidatePath $candidatePath `
            -EvidencePath $evidencePath `
            -ExpectedSignerSubject $fixtureCandidate.signer.subject `
            -ExpectedSignerThumbprint $fixtureCandidate.signer.thumbprint `
            -ExpectedUpdateFeedUrl $fixtureCandidate.urls.update_feed_url `
            -ExpectedReleaseBaseUrl $fixtureCandidate.urls.release_base_url `
            -ExpectedReleasePageBaseUrl $fixtureCandidate.urls.release_page_base_url `
            -ProjectRoot $fixtureProjectRoot | Out-Null
    } catch {
        $tamperedArchiveWasRejected = $_.Exception.Message -match "immutable candidate byte binding"
    }
    if (-not $tamperedArchiveWasRejected) {
        throw "The finalizer accepted a candidate archive whose bytes changed after recording."
    }
    [IO.File]::WriteAllText($archivePath, "immutable archive bytes")

    [IO.File]::Delete($manifestPath)
    $missingArtifactWasRejected = $false
    try {
        Confirm-ReleaseCandidateMetadata `
            -CandidatePath $candidatePath `
            -EvidencePath $evidencePath `
            -ExpectedSignerSubject $fixtureCandidate.signer.subject `
            -ExpectedSignerThumbprint $fixtureCandidate.signer.thumbprint `
            -ExpectedUpdateFeedUrl $fixtureCandidate.urls.update_feed_url `
            -ExpectedReleaseBaseUrl $fixtureCandidate.urls.release_base_url `
            -ExpectedReleasePageBaseUrl $fixtureCandidate.urls.release_page_base_url `
            -ProjectRoot $fixtureProjectRoot | Out-Null
    } catch {
        $missingArtifactWasRejected = $_.Exception.Message -match "Candidate manifest is missing"
    }
    if (-not $missingArtifactWasRejected) {
        throw "The finalizer accepted a candidate with a missing immutable artifact."
    }
    [IO.File]::WriteAllText($manifestPath, $manifestJson, [Text.UTF8Encoding]::new($false))

    $immutableInputs = @(
        $candidatePath,
        $evidencePath,
        $archivePath,
        $sidecarPath,
        $manifestPath
    ) + @($verified.EvidenceAttachmentPaths)
    $testLocks = @(Open-ImmutableReleaseFileLocks -Paths $immutableInputs)
    try {
        Assert-ImmutableReleasePathSetsEqual `
            -Expected $immutableInputs `
            -Actual @($immutableInputs)
        $changedLockedPathSetWasRejected = $false
        try {
            Assert-ImmutableReleasePathSetsEqual `
                -Expected $immutableInputs `
                -Actual @($immutableInputs[0..($immutableInputs.Count - 2)] + (Join-Path $fixtureRoot "unlocked-replacement.png"))
        } catch {
            $changedLockedPathSetWasRejected = $_.Exception.Message -match "path set changed"
        }
        if (-not $changedLockedPathSetWasRejected) {
            throw "A second metadata confirmation was allowed to switch to an unlocked release input."
        }
        if (@(Get-ImmutableReleaseFileSnapshots -Paths $immutableInputs).Count -ne $immutableInputs.Count) {
            throw "Immutable input hashes could not be read while write/delete locks were held."
        }
        $lockedWriteWasRejected = $false
        try {
            [IO.File]::AppendAllText($archivePath, "blocked")
        } catch {
            $lockedWriteWasRejected = $true
        }
        if (-not $lockedWriteWasRejected) {
            throw "Immutable input locks allowed the candidate archive to be replaced or modified."
        }
    } finally {
        Close-ImmutableReleaseFileLocks -Locks $testLocks
    }
    foreach ($path in $immutableInputs) {
        $attributes = [IO.File]::GetAttributes($path)
        [IO.File]::SetAttributes($path, $attributes -bor [IO.FileAttributes]::ReadOnly)
    }
    $snapshotBefore = @(Get-ImmutableReleaseFileSnapshots -Paths $immutableInputs)
    Confirm-ReleaseCandidateMetadata `
        -CandidatePath $candidatePath `
        -EvidencePath $evidencePath `
        -ExpectedSignerSubject $fixtureCandidate.signer.subject `
        -ExpectedSignerThumbprint $fixtureCandidate.signer.thumbprint `
        -ExpectedUpdateFeedUrl $fixtureCandidate.urls.update_feed_url `
        -ExpectedReleaseBaseUrl $fixtureCandidate.urls.release_base_url `
        -ExpectedReleasePageBaseUrl $fixtureCandidate.urls.release_page_base_url `
        -ProjectRoot $fixtureProjectRoot | Out-Null
    $snapshotAfter = @(Get-ImmutableReleaseFileSnapshots -Paths $immutableInputs)
    Assert-ImmutableReleaseFileSnapshotsEqual -Before $snapshotBefore -After $snapshotAfter

    $changedSnapshot = @($snapshotAfter | ForEach-Object {
        [PSCustomObject]@{
            path = $_.path
            bytes = $_.bytes
            sha256 = $_.sha256
            last_write_utc_ticks = $_.last_write_utc_ticks
        }
    })
    $changedSnapshot[0].last_write_utc_ticks++
    $mtimeChangeWasRejected = $false
    try {
        Assert-ImmutableReleaseFileSnapshotsEqual -Before $snapshotBefore -After $changedSnapshot
    } catch {
        $mtimeChangeWasRejected = $_.Exception.Message -match "changed during finalization"
    }
    if (-not $mtimeChangeWasRejected) {
        throw "The finalizer did not detect an immutable input mtime change."
    }
    foreach ($path in $immutableInputs) {
        $attributes = [IO.File]::GetAttributes($path)
        [IO.File]::SetAttributes($path, $attributes -band (-bnot [IO.FileAttributes]::ReadOnly))
    }
} finally {
    if ([IO.Directory]::Exists($fixtureRoot)) {
        foreach ($file in Get-ChildItem -LiteralPath $fixtureRoot -Recurse -File -ErrorAction SilentlyContinue) {
            $attributes = [IO.File]::GetAttributes($file.FullName)
            [IO.File]::SetAttributes($file.FullName, $attributes -band (-bnot [IO.FileAttributes]::ReadOnly))
        }
        [IO.Directory]::Delete($fixtureRoot, $true)
    }
}

Write-Host "Finalize release candidate tests: PASS"
