param(
    [string]$RuntimeDir = "",
    [string]$UpdateFeedUrl = "",
    [string]$ExpectedTag = "",
    [string]$ExpectedSignerSubject = "",
    [string]$ExpectedSignerThumbprint = "",
    [switch]$SkipGitState,
    [switch]$SkipAuthenticode,
    [switch]$SkipAcceptance,
    [switch]$SkipPublicFeed
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$projectRoot = [IO.Path]::GetFullPath((Split-Path -Parent $PSScriptRoot))
$repositoryRoot = [IO.Path]::GetFullPath((Join-Path $projectRoot ".."))
$failures = New-Object Collections.Generic.List[string]

function Add-Failure {
    param([string]$Message)
    $failures.Add($Message)
}

function Assert-HttpsUrl {
    param([string]$Name, [string]$Value)
    if ([string]::IsNullOrWhiteSpace($Value)) {
        Add-Failure "$Name is required for a public release."
        return $null
    }
    try { $uri = [Uri]$Value } catch {
        Add-Failure "$Name is not a valid absolute URL: $Value"
        return $null
    }
    if (-not $uri.IsAbsoluteUri -or $uri.Scheme -ne "https" -or
        [string]::IsNullOrWhiteSpace($uri.Host) -or
        -not [string]::IsNullOrWhiteSpace($uri.UserInfo)) {
        Add-Failure "$Name must be an absolute HTTPS URL without embedded credentials."
        return $null
    }
    return $uri
}

$versionPath = Join-Path $projectRoot "APP_VERSION"
if (-not (Test-Path -LiteralPath $versionPath -PathType Leaf)) {
    Add-Failure "APP_VERSION is missing."
    $version = ""
} else {
    $version = (Get-Content -LiteralPath $versionPath -Raw -Encoding UTF8).Trim()
    if ($version -notmatch '^(0|[1-9]\d*)\.(0|[1-9]\d*)\.(0|[1-9]\d*)(-[0-9A-Za-z.-]+)?$') {
        Add-Failure "APP_VERSION is not a supported semantic version: $version"
    }
}

foreach ($required in @(
    (Join-Path $repositoryRoot ".github\workflows\qt6-ci.yml"),
    (Join-Path $repositoryRoot ".github\workflows\release.yml"),
    (Join-Path $projectRoot "scripts\fetch-rapidocr.ps1"),
    (Join-Path $projectRoot "third_party\rapidocr\rapidocr-lock.json"),
    (Join-Path $projectRoot "docs\UPDATES.md"),
    (Join-Path $projectRoot "docs\ACCEPTANCE_MATRIX.md")
)) {
    if (-not (Test-Path -LiteralPath $required -PathType Leaf)) {
        Add-Failure "Required release-control file is missing: $required"
    }
}

$qtCiWorkflowPath = Join-Path $repositoryRoot ".github\workflows\qt6-ci.yml"
if (Test-Path -LiteralPath $qtCiWorkflowPath -PathType Leaf) {
    $qtCiWorkflow = Get-Content `
        -LiteralPath $qtCiWorkflowPath `
        -Raw `
        -Encoding UTF8
    $fetchRapidOcrCommand = [regex]::Match(
        $qtCiWorkflow,
        '(?m)^\s*&\s+\.\\vocekit-qt6\\scripts\\fetch-rapidocr\.ps1(?:\s|$)'
    )
    $buildOcrHelpersCommand = [regex]::Match(
        $qtCiWorkflow,
        '(?m)^\s*&\s+\.\\vocekit-qt6\\scripts\\build-ocr-helpers\.ps1(?:\s|$)'
    )
    $runTestsCommand = [regex]::Match(
        $qtCiWorkflow,
        '(?m)^\s*&\s+\.\\vocekit-qt6\\scripts\\run-all-tests\.ps1(?:\s|$)'
    )
    if (-not $fetchRapidOcrCommand.Success -or
        -not $buildOcrHelpersCommand.Success -or
        -not $runTestsCommand.Success -or
        $fetchRapidOcrCommand.Index -ge $buildOcrHelpersCommand.Index -or
        $buildOcrHelpersCommand.Index -ge $runTestsCommand.Index) {
        Add-Failure "Qt 6 CI must verify the pinned RapidOCR SDK and build OCR helpers before running integration tests."
    }
}

$releaseWorkflowPath = Join-Path $repositoryRoot ".github\workflows\release.yml"
if (Test-Path -LiteralPath $releaseWorkflowPath -PathType Leaf) {
    $releaseWorkflow = Get-Content -LiteralPath $releaseWorkflowPath -Raw -Encoding UTF8
    foreach ($forbiddenPattern in @(
        'VOCEKIT_SIGNING_CERTIFICATE_BASE64',
        'VOCEKIT_SIGNING_CERTIFICATE_PASSWORD',
        'Import-PfxCertificate',
        'FromBase64String\([^)]*CERTIFICATE'
    )) {
        if ($releaseWorkflow -match $forbiddenPattern) {
            Add-Failure "Release workflow must not export or import a code-signing private key ($forbiddenPattern)."
        }
    }
    if ($releaseWorkflow -match 'contents:\s*write' -or
        $releaseWorkflow -match 'gh\s+release\s+create') {
        Add-Failure "Unsigned GitHub release-candidate workflow must not have permission or commands to publish a Release."
    }
    if ($releaseWorkflow -notmatch 'package-test\.ps1' -or
        $releaseWorkflow -notmatch 'unsigned-candidate') {
        Add-Failure "GitHub release-candidate workflow must create an explicitly unsigned internal-test package."
    }
    if ($releaseWorkflow -notmatch 'fetch-rapidocr\.ps1' -or
        $releaseWorkflow -notmatch 'build-runtime-helpers\.ps1') {
        Add-Failure "GitHub release-candidate workflow must prepare the pinned RapidOCR SDK and build runtime helpers."
    }
    if ($releaseWorkflow -match '\$\{\{\s*github\.ref_name\s*\}\}' -or
        $releaseWorkflow -notmatch '\$env:GITHUB_REF_NAME' -or
        $releaseWorkflow -notmatch 'CANDIDATE_NAME') {
        Add-Failure "Release workflow must treat Git ref names as environment data and sanitize candidate package names."
    }
}

$rapidOcrLockPath = Join-Path $projectRoot `
    "third_party\rapidocr\rapidocr-lock.json"
if (Test-Path -LiteralPath $rapidOcrLockPath -PathType Leaf) {
    try {
        $rapidOcrLock = Get-Content `
            -LiteralPath $rapidOcrLockPath `
            -Raw `
            -Encoding UTF8 | ConvertFrom-Json -ErrorAction Stop
        if ($rapidOcrLock.version -ne "1.2.2" -or
            $rapidOcrLock.archive_name -ne "Project_RapidOcrOnnx-1.2.2.7z" -or
            $rapidOcrLock.archive_url -ne "https://github.com/RapidAI/RapidOcrOnnx/releases/download/1.2.2/Project_RapidOcrOnnx-1.2.2.7z" -or
            [long]$rapidOcrLock.archive_bytes -ne 83183975L -or
            $rapidOcrLock.archive_sha256 -ne "5049D4C9CAF0143A9F35E618F80F7E5946B8DF1E57A90B3CA8E9CC105FC8A6AE" -or
            [int]$rapidOcrLock.sdk_file_count -ne 1445 -or
            $rapidOcrLock.sdk_fingerprint_sha256 -ne "DDDD009B109B84AB693ED0D4592394E84241D7518FAB62FB04FE6250DCCCA1F1") {
            Add-Failure "RapidOCR dependency lock does not match the approved 1.2.2 build inputs."
        }
    } catch {
        Add-Failure "RapidOCR dependency lock is invalid: $($_.Exception.Message)"
    }
}

$windowsOcrProjectPath = Join-Path $projectRoot `
    "helpers\windows_ocr\windows_ocr.vcxproj"
$ocrBuildScriptPath = Join-Path $projectRoot `
    "scripts\build-ocr-helpers.ps1"
if ((Test-Path -LiteralPath $windowsOcrProjectPath -PathType Leaf) -and
    (Get-Content -LiteralPath $windowsOcrProjectPath -Raw -Encoding UTF8) -notmatch
        '<RuntimeLibrary>MultiThreaded</RuntimeLibrary>') {
    Add-Failure "Windows OCR helper must statically link the Visual C++ runtime for portable releases."
}
if ((Test-Path -LiteralPath $ocrBuildScriptPath -PathType Leaf) -and
    (Get-Content -LiteralPath $ocrBuildScriptPath -Raw -Encoding UTF8) -notmatch
        'Assert-NoDynamicVisualCppRuntime') {
    Add-Failure "OCR helper build must reject dynamic Visual C++ runtime dependencies."
}

foreach ($workflowPath in @(
    (Join-Path $repositoryRoot ".github\workflows\qt6-ci.yml"),
    $releaseWorkflowPath
)) {
    if (-not (Test-Path -LiteralPath $workflowPath -PathType Leaf)) {
        continue
    }
    $workflowText = Get-Content -LiteralPath $workflowPath -Raw -Encoding UTF8
    foreach ($match in [regex]::Matches($workflowText, '(?m)^\s*uses:\s*([^\s#]+)')) {
        $actionReference = $match.Groups[1].Value
        if ($actionReference -notmatch '@[0-9a-fA-F]{40}$') {
            Add-Failure "GitHub Action must be pinned to a full commit SHA: $actionReference"
        }
    }
}

$signScriptPath = Join-Path $projectRoot "scripts\sign-release.ps1"
if (-not (Test-Path -LiteralPath $signScriptPath -PathType Leaf)) {
    Add-Failure "Signing script is missing: $signScriptPath"
} else {
    $signScript = Get-Content -LiteralPath $signScriptPath -Raw -Encoding UTF8
    if ($signScript -notmatch 'SignatureStatus\]::Valid' -or
        $signScript -notmatch 'SignatureStatus\]::NotSigned' -or
        $signScript -notmatch 'Refusing to overwrite an invalid existing signature') {
        Add-Failure "Signing script must preserve valid vendor signatures and reject invalid existing signatures."
    }
}

if (-not [string]::IsNullOrWhiteSpace($ExpectedTag) -and
    $ExpectedTag -cne "v$version") {
    Add-Failure "Release tag '$ExpectedTag' does not match APP_VERSION '$version'."
}

if (-not $SkipGitState) {
    Push-Location $repositoryRoot
    try {
        $status = @(& git status --porcelain --untracked-files=all 2>&1)
        if ($LASTEXITCODE -ne 0) {
            Add-Failure "Unable to inspect Git status."
        } elseif ($status.Count -gt 0) {
            Add-Failure "Git worktree is not clean; public release artifacts must come from a committed tree."
        }
        & git fetch --quiet origin main 2>$null
        if ($LASTEXITCODE -ne 0) {
            Add-Failure "Unable to fetch origin/main."
        } else {
            $counts = (& git rev-list --left-right --count origin/main...HEAD).Trim() -split '\s+'
            if ($counts.Count -ne 2 -or $counts[0] -ne "0" -or $counts[1] -ne "0") {
                Add-Failure "Release commit must exactly match origin/main (behind=$($counts[0]), ahead=$($counts[1]))."
            }
        }
        if (-not [string]::IsNullOrWhiteSpace($ExpectedTag)) {
            $actualTag = (& git describe --tags --exact-match HEAD 2>$null).Trim()
            if ($LASTEXITCODE -ne 0 -or $actualTag -cne $ExpectedTag) {
                Add-Failure "HEAD is not tagged exactly as '$ExpectedTag'."
            }
        }
    } finally {
        Pop-Location
    }
}

if (-not $SkipAcceptance) {
    $matrixPath = Join-Path $projectRoot "docs\ACCEPTANCE_MATRIX.md"
    if (Test-Path -LiteralPath $matrixPath -PathType Leaf) {
        $matrix = Get-Content -LiteralPath $matrixPath -Raw -Encoding UTF8
        $table = ($matrix -split '## 每个单元格的必测动作')[0]
        if ($table -match '\|\s*(未执行|失败)\s*\|') {
            Add-Failure "The real-application acceptance matrix still contains 未执行 or 失败 cells."
        }
    }
}

$feedUri = Assert-HttpsUrl -Name "UpdateFeedUrl" -Value $UpdateFeedUrl
if ($feedUri -and -not $SkipPublicFeed) {
    $headers = @{ "User-Agent" = "VoceKit-Release-Gate/$version"; "Accept" = "application/json" }
    try {
        Invoke-WebRequest -Uri $feedUri -Headers $headers -Method Get -TimeoutSec 20 -UseBasicParsing | Out-Null
    } catch {
        $githubMatch = [regex]::Match(
            $feedUri.AbsoluteUri,
            '^https://api\.github\.com/repos/([^/]+/[^/]+)/releases/latest/?$'
        )
        if ($githubMatch.Success) {
            try {
                $repositoryApi = "https://api.github.com/repos/$($githubMatch.Groups[1].Value)"
                Invoke-WebRequest -Uri $repositoryApi -Headers $headers -Method Get -TimeoutSec 20 -UseBasicParsing | Out-Null
                Write-Warning "The public GitHub repository is reachable but has no latest release yet."
            } catch {
                Add-Failure "The configured GitHub update repository is not publicly reachable."
            }
        } else {
            Add-Failure "The configured update feed is not publicly reachable: $($_.Exception.Message)"
        }
    }
}

if (-not $SkipAuthenticode) {
    $normalizedExpectedThumbprint = ($ExpectedSignerThumbprint -replace '\s', '').ToUpperInvariant()
    if ([string]::IsNullOrWhiteSpace($ExpectedSignerSubject) -and
        [string]::IsNullOrWhiteSpace($normalizedExpectedThumbprint)) {
        Add-Failure "ExpectedSignerSubject or ExpectedSignerThumbprint is required for publisher verification."
    }
    if (-not [string]::IsNullOrWhiteSpace($normalizedExpectedThumbprint) -and
        $normalizedExpectedThumbprint -notmatch '^[0-9A-F]{40,64}$') {
        Add-Failure "ExpectedSignerThumbprint is invalid."
    }
    if ([string]::IsNullOrWhiteSpace($RuntimeDir)) {
        $RuntimeDir = Join-Path $projectRoot ".qt6-deploy"
    }
    $runtimeFull = [IO.Path]::GetFullPath($RuntimeDir)
    if (-not (Test-Path -LiteralPath $runtimeFull -PathType Container)) {
        Add-Failure "Runtime directory is missing: $runtimeFull"
    } else {
        $publisherPaths = [Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
        @(
            "vocekit.exe",
            "ocr/rapidocr/vocekit-rapidocr.exe",
            "ocr/windows/vocekit-windows-ocr.exe",
            "speech/windows/vocekit-windows-speech.exe",
            "libgcc_s_seh-1.dll",
            "libstdc++-6.dll",
            "libwinpthread-1.dll"
        ) | ForEach-Object { [void]$publisherPaths.Add($_) }
        $foundPublisherPaths = [Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
        $unsigned = New-Object Collections.Generic.List[string]
        foreach ($binary in Get-ChildItem -LiteralPath $runtimeFull -File -Recurse | Where-Object {
            $_.Extension.ToLowerInvariant() -in @(".exe", ".dll")
        }) {
            $relativePath = [IO.Path]::GetRelativePath($runtimeFull, $binary.FullName).Replace("\", "/")
            $signature = Get-AuthenticodeSignature -LiteralPath $binary.FullName
            if ($signature.Status -ne [System.Management.Automation.SignatureStatus]::Valid) {
                $unsigned.Add("$relativePath`: $($signature.Status)")
                continue
            }
            if ($publisherPaths.Contains($relativePath)) {
                [void]$foundPublisherPaths.Add($relativePath)
                if (-not $signature.SignerCertificate) {
                    Add-Failure "Publisher signer certificate is missing: $relativePath"
                    continue
                }
                if (-not [string]::IsNullOrWhiteSpace($ExpectedSignerSubject) -and
                    -not [string]::Equals(
                        $signature.SignerCertificate.Subject,
                        $ExpectedSignerSubject,
                        [StringComparison]::OrdinalIgnoreCase
                    )) {
                    Add-Failure "Publisher subject mismatch: $relativePath"
                }
                if (-not [string]::IsNullOrWhiteSpace($normalizedExpectedThumbprint) -and
                    $signature.SignerCertificate.Thumbprint -ne $normalizedExpectedThumbprint) {
                    Add-Failure "Publisher certificate thumbprint mismatch: $relativePath"
                }
                if (-not $signature.TimeStamperCertificate) {
                    Add-Failure "RFC 3161 timestamp is missing: $relativePath"
                }
            }
        }
        foreach ($requiredPath in $publisherPaths) {
            if (-not $foundPublisherPaths.Contains($requiredPath)) {
                Add-Failure "Required publisher-owned binary was not verified: $requiredPath"
            }
        }
        if ($unsigned.Count -gt 0) {
            Add-Failure "Runtime contains unsigned or invalid binaries: $($unsigned -join ', ')"
        }
    }
}

if ($failures.Count -gt 0) {
    throw "Public release readiness failed:`n- $($failures -join "`n- ")"
}

Write-Host "Public release readiness passed for VoceKit $version."
