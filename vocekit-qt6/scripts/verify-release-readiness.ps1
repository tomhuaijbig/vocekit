param(
    [string]$RuntimeDir = "",
    [string]$UpdateFeedUrl = "",
    [string]$ExpectedTag = "",
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
    (Join-Path $projectRoot "docs\UPDATES.md"),
    (Join-Path $projectRoot "docs\ACCEPTANCE_MATRIX.md")
)) {
    if (-not (Test-Path -LiteralPath $required -PathType Leaf)) {
        Add-Failure "Required release-control file is missing: $required"
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
    if ([string]::IsNullOrWhiteSpace($RuntimeDir)) {
        $RuntimeDir = Join-Path $projectRoot ".qt6-deploy"
    }
    $runtimeFull = [IO.Path]::GetFullPath($RuntimeDir)
    if (-not (Test-Path -LiteralPath $runtimeFull -PathType Container)) {
        Add-Failure "Runtime directory is missing: $runtimeFull"
    } else {
        $unsigned = New-Object Collections.Generic.List[string]
        foreach ($binary in Get-ChildItem -LiteralPath $runtimeFull -File -Recurse | Where-Object {
            $_.Extension.ToLowerInvariant() -in @(".exe", ".dll")
        }) {
            $signature = Get-AuthenticodeSignature -LiteralPath $binary.FullName
            if ($signature.Status -ne [System.Management.Automation.SignatureStatus]::Valid) {
                $unsigned.Add("$($binary.Name): $($signature.Status)")
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
