$ErrorActionPreference = "Stop"
$buildScript = Join-Path (Split-Path -Parent $PSScriptRoot) "build-windows-speech-helper.ps1"

. $buildScript -DecisionTestMode

if ((Get-WindowsSpeechBuildDecision -ExitCode 0 -OutputText "") -ne "MSBuild") {
    throw "A successful MSBuild must use the MSBuild output."
}
if ((Get-WindowsSpeechBuildDecision -ExitCode 1 -OutputText "error MSB3644: reference assemblies missing") -ne "RoslynFallback") {
    throw "MSB3644 must select the Roslyn fallback."
}
if ((Get-WindowsSpeechBuildDecision -ExitCode 1 -OutputText "error CS1002: ; expected") -ne "Fail") {
    throw "A normal compiler failure must not select the Roslyn fallback."
}

$testRoot = Join-Path ([IO.Path]::GetTempPath()) ("vocekit-vs-locator-test " + [Guid]::NewGuid().ToString("N"))
try {
    foreach ($edition in @("Community", "Professional", "Enterprise", "BuildTools")) {
        $installation = Join-Path $testRoot ("Microsoft Visual Studio\2022\" + $edition)
        $msbuild = Join-Path $installation "MSBuild\Current\Bin\MSBuild.exe"
        $csc = Join-Path $installation "MSBuild\Current\Bin\Roslyn\csc.exe"
        New-Item -ItemType Directory -Path (Split-Path -Parent $csc) -Force | Out-Null
        New-Item -ItemType File -Path $msbuild -Force | Out-Null
        New-Item -ItemType File -Path $csc -Force | Out-Null
        $toolchain = Resolve-VisualStudioToolchain -InstallationPaths @($installation)
        if ($toolchain.MSBuild -ne $msbuild -or $toolchain.Csc -ne $csc) {
            throw "The Visual Studio locator failed for $edition."
        }
    }

    $customInstallation = Join-Path $testRoot "Dev Tools\Custom VS"
    $customMsbuild = Join-Path $customInstallation "MSBuild\Current\Bin\MSBuild.exe"
    $customCsc = Join-Path $customInstallation "MSBuild\Current\Bin\Roslyn\csc.exe"
    New-Item -ItemType Directory -Path (Split-Path -Parent $customCsc) -Force | Out-Null
    New-Item -ItemType File -Path $customMsbuild -Force | Out-Null
    New-Item -ItemType File -Path $customCsc -Force | Out-Null
    $customFailure = $null
    try {
        $customToolchain = Resolve-VisualStudioToolchain -InstallationPaths @($customInstallation)
        if ($customToolchain.MSBuild -ne $customMsbuild -or $customToolchain.Csc -ne $customCsc) {
            $customFailure = "The Visual Studio locator returned the wrong custom vswhere toolchain."
        }
    } catch {
        $customFailure = "The Visual Studio locator rejected a valid custom vswhere installation path: $($_.Exception.Message)"
    }

    $vswhereFailure = $null
    try {
        $vswhereArguments = @(Get-VisualStudioVsWhereArguments)
        $versionIndex = [Array]::IndexOf($vswhereArguments, "-version")
        if ($versionIndex -lt 0 -or $versionIndex + 1 -ge $vswhereArguments.Count -or
            $vswhereArguments[$versionIndex + 1] -ne "[17.0,18.0)") {
            $vswhereFailure = "The vswhere query must restrict results to Visual Studio 2022."
        }
        foreach ($requiredArgument in @("-latest", "-products", "*", "-requires", "Microsoft.Component.MSBuild", "-property", "installationPath")) {
            if ($vswhereArguments -notcontains $requiredArgument) {
                $vswhereFailure = "The vswhere query is missing required argument: $requiredArgument"
            }
        }
    } catch {
        $vswhereFailure = "The vswhere argument API is missing: $($_.Exception.Message)"
    }

    $notFound = $false
    try {
        Resolve-VisualStudioToolchain -InstallationPaths @((Join-Path $testRoot "missing")) | Out-Null
    } catch {
        $notFound = $_.Exception.Message -match "Visual Studio 2022 MSBuild"
    }
    if (-not $notFound) {
        throw "The Visual Studio locator did not fail clearly when no toolchain existed."
    }

    $fakeProjectRoot = Join-Path $testRoot "stale-output-project"
    $bin = Join-Path $fakeProjectRoot "helpers\bin"
    New-Item -ItemType Directory -Path $bin -Force | Out-Null
    $oldHelper = Join-Path $bin "vocekit-windows-speech.exe"
    $neighbor = Join-Path $bin "keep-me.txt"
    [IO.File]::WriteAllText($oldHelper, "stale sentinel")
    [IO.File]::WriteAllText($neighbor, "neighbor")
    Remove-WindowsSpeechHelperOutput -ProjectRoot $fakeProjectRoot
    if ((Test-Path -LiteralPath $oldHelper) -or -not (Test-Path -LiteralPath $neighbor)) {
        throw "Stale helper cleanup did not remove exactly the target executable."
    }
} finally {
    if ([IO.Directory]::Exists($testRoot)) {
        [IO.Directory]::Delete($testRoot, $true)
    }
}

$smokeScript = Join-Path $PSScriptRoot "windows-speech-helper-system-smoke.ps1"
$smokeText = Get-Content -LiteralPath $smokeScript -Raw
$smokeFailure = $null
if ($smokeText -notmatch '(?m)^\s*\$started\s*=\s*\$false\s*$' -or
    $smokeText -notmatch '(?m)^\s*\$started\s*=\s*\$true\s*$' -or
    $smokeText -notmatch '(?s)finally\s*\{\s*if\s*\(\$started\)\s*\{.*?\$process\.HasExited') {
    $smokeFailure = "The System.Speech smoke cleanup can mask Process.Start failures."
}
$failures = @($customFailure, $vswhereFailure, $smokeFailure) | Where-Object { -not [string]::IsNullOrWhiteSpace($_) }
if ($failures.Count -ne 0) {
    throw ($failures -join " | ")
}

Write-Host "Windows speech helper build decision tests: PASS"
