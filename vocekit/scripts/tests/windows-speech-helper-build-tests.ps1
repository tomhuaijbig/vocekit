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

Write-Host "Windows speech helper build decision tests: PASS"
