param(
    [string]$RuntimeDir = "",
    [string]$UpdateFeedUrl = "",
    [string]$ExpectedTag = "",
    [string]$ExpectedSignerSubject = "",
    [string]$ExpectedSignerThumbprint = "",
    [switch]$SkipGitState,
    [switch]$AllowExpectedTagNotCreated,
    [switch]$SkipAuthenticode,
    [switch]$SkipAcceptance,
    [switch]$SkipPublicFeed
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$projectRoot = [IO.Path]::GetFullPath((Split-Path -Parent $PSScriptRoot))
$repositoryRoot = [IO.Path]::GetFullPath((Join-Path $projectRoot ".."))
$failures = New-Object Collections.Generic.List[string]
. (Join-Path $PSScriptRoot "git-trust-safety.ps1")

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

function Get-ReleaseRelativePath {
    param(
        [Parameter(Mandatory = $true)][string]$BasePath,
        [Parameter(Mandatory = $true)][string]$Path
    )

    $baseFull = [IO.Path]::GetFullPath($BasePath).TrimEnd("\", "/") + "\"
    $pathFull = [IO.Path]::GetFullPath($Path)
    if (-not $pathFull.StartsWith($baseFull, [StringComparison]::OrdinalIgnoreCase)) {
        throw "Path is outside the release runtime: $pathFull"
    }
    return $pathFull.Substring($baseFull.Length).Replace("\", "/")
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
    (Join-Path $projectRoot "scripts\build-provenance.ps1"),
    (Join-Path $projectRoot "scripts\git-trust-safety.ps1"),
    (Join-Path $projectRoot "scripts\deployment-safety.ps1"),
    (Join-Path $projectRoot "scripts\release-path-safety.ps1"),
    (Join-Path $projectRoot "scripts\runtime-helper-provenance.ps1"),
    (Join-Path $projectRoot "scripts\verify-embedded-build-provenance.ps1"),
    (Join-Path $projectRoot "scripts\verify-runtime-helper-build-provenance.ps1"),
    (Join-Path $projectRoot "scripts\publish-finalized-release.ps1"),
    (Join-Path $projectRoot "scripts\tests\runtime-helper-provenance-tests.ps1"),
    (Join-Path $projectRoot "scripts\tests\publish-finalized-release-tests.ps1"),
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
    $crashStepIndex = $releaseWorkflow.IndexOf(
        "- name: Preflight Release crash-handler regression",
        [StringComparison]::Ordinal
    )
    $runtimeHelpersStepIndex = $releaseWorkflow.IndexOf(
        "- name: Prepare pinned RapidOCR SDK and runtime helpers",
        [StringComparison]::Ordinal
    )
    $fullReleaseStepIndex = $releaseWorkflow.IndexOf(
        "- name: Run all Release tests",
        [StringComparison]::Ordinal
    )
    $buildCandidateStepIndex = $releaseWorkflow.IndexOf(
        "- name: Build and deploy unsigned candidate",
        [StringComparison]::Ordinal
    )
    $releaseStepOrderIsValid =
        $crashStepIndex -ge 0 -and
        $runtimeHelpersStepIndex -gt $crashStepIndex -and
        $fullReleaseStepIndex -gt $runtimeHelpersStepIndex -and
        $buildCandidateStepIndex -gt $fullReleaseStepIndex
    $crashStepBlock = if ($releaseStepOrderIsValid) {
        $releaseWorkflow.Substring(
            $crashStepIndex,
            $runtimeHelpersStepIndex - $crashStepIndex
        )
    } else {
        ""
    }
    $fullReleaseStepBlock = if ($releaseStepOrderIsValid) {
        $releaseWorkflow.Substring(
            $fullReleaseStepIndex,
            $buildCandidateStepIndex - $fullReleaseStepIndex
        )
    } else {
        ""
    }
    $crashRunCommand = [regex]::Match(
        $crashStepBlock,
        '(?m)^\s*&\s+\.\\vocekit-qt6\\scripts\\run-all-tests\.ps1[^\r\n]*-Configuration\s+release[^\r\n]*-ProjectName\s+runtime_crash_handler_tests(?:\s|$)'
    )
    $fullReleaseRunCommand = [regex]::Match(
        $fullReleaseStepBlock,
        '(?m)^\s*&\s+\.\\vocekit-qt6\\scripts\\run-all-tests\.ps1[^\r\n]*-Configuration\s+release[^\r\n]*\r?$'
    )
    if (-not $releaseStepOrderIsValid -or
        -not $crashRunCommand.Success -or
        -not $fullReleaseRunCommand.Success -or
        $fullReleaseRunCommand.Value -match '-ProjectName(?:\s|$)') {
        Add-Failure "Release workflow must run the focused crash-handler preflight before runtime helpers and the full Release suite."
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
    foreach ($requiredScript in @(
        'tests\\scripts\\update-helper-tests\.ps1',
        'scripts\\tests\\windows-speech-helper-build-tests\.ps1',
        'scripts\\tests\\runtime-helper-provenance-tests\.ps1',
        'scripts\\tests\\release-candidate-tests\.ps1',
        'scripts\\tests\\finalize-release-candidate-tests\.ps1',
        'scripts\\tests\\publish-finalized-release-tests\.ps1'
    )) {
        if ($workflowText -notmatch "(?m)^\s*&\s+\.\\vocekit-qt6\\$requiredScript\s*\r?$") {
            Add-Failure "GitHub workflow must run the release infrastructure regression script: $requiredScript"
        }
        $legacyCommandPattern = '(?m)^\s*&\s+powershell\.exe\s+-NoLogo\s+-NoProfile\s+-NonInteractive\s+' +
            '-ExecutionPolicy\s+Bypass\s+-File\s+\.\\vocekit-qt6\\' + $requiredScript +
            '\s*\r?\n\s*if\s*\(\$LASTEXITCODE\s+-ne\s+0\)\s*\{\s*throw\b'
        if ($workflowText -notmatch $legacyCommandPattern) {
            Add-Failure "GitHub workflow must fail closed while running the release regression under Windows PowerShell 5.1: $requiredScript"
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
if ($AllowExpectedTagNotCreated -and -not $SkipAcceptance) {
    Add-Failure "AllowExpectedTagNotCreated is only valid for the pre-acceptance signed-candidate phase."
}

if (-not $SkipGitState) {
    try {
        Assert-NoHiddenGitIndexEntries -RepositoryRoot $repositoryRoot
        $status = Invoke-TrustedGit `
            -RepositoryRoot $repositoryRoot `
            -Arguments @("status", "--porcelain=v1", "--untracked-files=all")
        if (@($status.output).Count -gt 0) {
            Add-Failure "Git worktree is not clean; public release artifacts must come from a committed tree."
        }

        [void](Invoke-TrustedGit `
            -RepositoryRoot $repositoryRoot `
            -Arguments @("fetch", "--quiet", "origin", "main"))
        $countResult = Invoke-TrustedGit `
            -RepositoryRoot $repositoryRoot `
            -Arguments @("rev-list", "--left-right", "--count", "origin/main...HEAD")
        $counts = ((@($countResult.output) -join "").Trim()) -split '\s+'
        if ($counts.Count -ne 2 -or $counts[0] -ne "0" -or $counts[1] -ne "0") {
            Add-Failure "Release commit must exactly match origin/main (behind=$($counts[0]), ahead=$($counts[1]))."
        }
        if (-not [string]::IsNullOrWhiteSpace($ExpectedTag) -and
            -not $AllowExpectedTagNotCreated) {
            $tagResult = Invoke-TrustedGit `
                -RepositoryRoot $repositoryRoot `
                -Arguments @("describe", "--tags", "--exact-match", "HEAD") `
                -AllowedExitCodes @(0, 128)
            $actualTag = (@($tagResult.output) -join "").Trim()
            if ([int]$tagResult.exit_code -ne 0 -or $actualTag -cne $ExpectedTag) {
                Add-Failure "HEAD is not tagged exactly as '$ExpectedTag'."
            }
        }
    } catch {
        Add-Failure "Unable to verify trusted Git state: $($_.Exception.Message)"
    }
}

if (-not $SkipAcceptance) {
    Add-Failure "Release-specific acceptance cannot be approved from the static Markdown specification. Pass -SkipAcceptance for source/candidate preflight, then run finalize-existing-release-candidate.ps1 with the frozen candidate and external evidence."
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
            $relativePath = Get-ReleaseRelativePath -BasePath $runtimeFull -Path $binary.FullName
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
    throw "Release source/runtime preflight failed:`n- $($failures -join "`n- ")"
}

Write-Host "Release source/runtime preflight passed for VoceKit $version."
