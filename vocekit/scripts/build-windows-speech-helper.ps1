param(
    [ValidateSet("Release")]
    [string]$Configuration = "Release",
    [switch]$DecisionTestMode
)

$ErrorActionPreference = "Stop"

function Get-WindowsSpeechBuildDecision {
    param(
        [int]$ExitCode,
        [AllowEmptyString()]
        [string]$OutputText
    )

    if ($ExitCode -eq 0) {
        return "MSBuild"
    }
    if ($OutputText -match '(?i)MSB3644' -or
        $OutputText -match '(?i)reference assemblies for .NETFramework,Version=v4\.8 were not found') {
        return "RoslynFallback"
    }
    return "Fail"
}

function Resolve-VisualStudioToolchain {
    param(
        [string[]]$InstallationPaths
    )

    $candidates = @()
    if ($null -ne $InstallationPaths) {
        $candidates = @($InstallationPaths)
    } else {
        $programFilesX86 = [Environment]::GetFolderPath([Environment+SpecialFolder]::ProgramFilesX86)
        $vswhere = Join-Path $programFilesX86 "Microsoft Visual Studio\Installer\vswhere.exe"
        if (Test-Path -LiteralPath $vswhere -PathType Leaf) {
            $located = @(& $vswhere -latest -products * -requires Microsoft.Component.MSBuild -property installationPath)
            if ($LASTEXITCODE -ne 0) {
                throw "Visual Studio Installer vswhere failed with exit code $LASTEXITCODE."
            }
            $candidates += @($located | Where-Object { -not [string]::IsNullOrWhiteSpace($_) })
        }

        $programFilesRoots = @(
            [Environment]::GetFolderPath([Environment+SpecialFolder]::ProgramFiles),
            $programFilesX86
        ) | Where-Object { -not [string]::IsNullOrWhiteSpace($_) } | Select-Object -Unique
        foreach ($root in $programFilesRoots) {
            foreach ($edition in @("Community", "Professional", "Enterprise", "BuildTools")) {
                $candidates += Join-Path $root ("Microsoft Visual Studio\2022\" + $edition)
            }
        }
    }

    foreach ($installationPath in @($candidates | Select-Object -Unique)) {
        if ([string]::IsNullOrWhiteSpace($installationPath)) {
            continue
        }
        $fullInstallationPath = [IO.Path]::GetFullPath($installationPath.Trim())
        if ($fullInstallationPath -notmatch '[\\/]2022[\\/]') {
            continue
        }
        $msbuild = Join-Path $fullInstallationPath "MSBuild\Current\Bin\MSBuild.exe"
        $csc = Join-Path $fullInstallationPath "MSBuild\Current\Bin\Roslyn\csc.exe"
        if ((Test-Path -LiteralPath $msbuild -PathType Leaf) -and
            (Test-Path -LiteralPath $csc -PathType Leaf)) {
            return [PSCustomObject]@{
                InstallationPath = $fullInstallationPath
                MSBuild = $msbuild
                Csc = $csc
            }
        }
    }

    throw "Visual Studio 2022 MSBuild and Roslyn toolchain were not found."
}

function Remove-WindowsSpeechHelperOutput {
    param(
        [Parameter(Mandatory = $true)]
        [string]$ProjectRoot
    )

    $fullProjectRoot = [IO.Path]::GetFullPath($ProjectRoot).TrimEnd([IO.Path]::DirectorySeparatorChar, [IO.Path]::AltDirectorySeparatorChar)
    $helper = [IO.Path]::GetFullPath((Join-Path $fullProjectRoot "helpers\bin\vocekit-windows-speech.exe"))
    if (-not $helper.StartsWith($fullProjectRoot + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) {
        throw "Windows speech helper output escaped the project root: $helper"
    }
    if (Test-Path -LiteralPath $helper -PathType Leaf) {
        Remove-Item -LiteralPath $helper -Force
    }
}

if ($DecisionTestMode) {
    return
}

$projectRoot = Split-Path -Parent $PSScriptRoot
$speechProject = Join-Path $projectRoot "helpers\windows_speech\windows_speech.csproj"

if (-not (Test-Path -LiteralPath $speechProject -PathType Leaf)) {
    throw "Windows speech helper project was not found: $speechProject"
}

$sourceFiles = @(
    (Join-Path $projectRoot "helpers\windows_speech\ProducerConsumerAudioStream.cs"),
    (Join-Path $projectRoot "helpers\windows_speech\Program.cs")
)
foreach ($sourceFile in $sourceFiles) {
    if (-not (Test-Path -LiteralPath $sourceFile -PathType Leaf)) {
        throw "Windows speech helper source was not found: $sourceFile"
    }
}

$toolchain = Resolve-VisualStudioToolchain
$msbuild = $toolchain.MSBuild
Remove-WindowsSpeechHelperOutput -ProjectRoot $projectRoot

$msbuildLines = @(& $msbuild $speechProject /m /nologo /v:minimal /p:Configuration=$Configuration /p:Platform=x64 2>&1 | ForEach-Object {
    $line = $_.ToString()
    Write-Host $line
    $line
})
$msbuildExitCode = $LASTEXITCODE

$usedFallback = $false
$buildDecision = Get-WindowsSpeechBuildDecision -ExitCode $msbuildExitCode -OutputText ($msbuildLines -join [Environment]::NewLine)
if ($buildDecision -eq "MSBuild") {
    Write-Host "Windows speech helper build path: MSBuild (.NET Framework 4.8, x64, $Configuration)"
} else {
    if ($buildDecision -ne "RoslynFallback") {
        throw "Windows speech helper MSBuild failed with exit code $msbuildExitCode; Roslyn fallback is forbidden because this was not MSB3644/reference-assemblies-missing."
    }

    $usedFallback = $true
    $csc = $toolchain.Csc
    $framework = Join-Path $env:WINDIR "Microsoft.NET\Framework64\v4.0.30319"
    $referencePaths = @(
        (Join-Path $framework "mscorlib.dll"),
        (Join-Path $framework "System.dll"),
        (Join-Path $framework "System.Core.dll"),
        (Join-Path $framework "System.Web.Extensions.dll")
    )
    $speechGac = Join-Path $env:WINDIR "Microsoft.NET\assembly\GAC_MSIL\System.Speech"
    if (-not (Test-Path -LiteralPath $speechGac -PathType Container)) {
        throw "Roslyn fallback System.Speech GAC directory was not found: $speechGac"
    }
    $speechReference = Get-ChildItem -LiteralPath $speechGac -Recurse -Filter "System.Speech.dll" -File -ErrorAction SilentlyContinue |
        Sort-Object -Property FullName |
        Select-Object -First 1 -ExpandProperty FullName
    if ([string]::IsNullOrWhiteSpace($speechReference) -or -not (Test-Path -LiteralPath $speechReference -PathType Leaf)) {
        throw "Roslyn fallback System.Speech reference was not found under: $speechGac"
    }
    $referencePaths += $speechReference

    $requiredPaths = @($csc) + $referencePaths
    foreach ($requiredPath in $requiredPaths) {
        if ([string]::IsNullOrWhiteSpace($requiredPath) -or -not (Test-Path -LiteralPath $requiredPath -PathType Leaf)) {
            throw "Roslyn fallback dependency was not found: $requiredPath"
        }
    }

    $outputDirectory = Join-Path $projectRoot "helpers\bin"
    New-Item -ItemType Directory -Path $outputDirectory -Force | Out-Null
    $helper = Join-Path $outputDirectory "vocekit-windows-speech.exe"
    $cscArguments = @(
        "/nologo",
        "/noconfig",
        "/nostdlib+",
        "/target:exe",
        "/platform:x64",
        "/langversion:7.3",
        "/optimize+",
        "/deterministic+",
        "/out:$helper"
    )
    foreach ($referencePath in $referencePaths) {
        $cscArguments += "/reference:$referencePath"
    }
    $cscArguments += $sourceFiles

    Write-Host "Windows speech helper build path: VS2022 Roslyn fallback (MSB3644/reference assemblies missing)"
    & $csc @cscArguments
    if ($LASTEXITCODE -ne 0) {
        throw "Windows speech helper Roslyn fallback failed with exit code $LASTEXITCODE."
    }
}

$helper = Join-Path $projectRoot "helpers\bin\vocekit-windows-speech.exe"
if (-not (Test-Path -LiteralPath $helper -PathType Leaf)) {
    throw "Windows speech helper was not generated: $helper"
}

$selfTestLines = @(& $helper --self-test --run-id build-check)
$selfTestExitCode = $LASTEXITCODE
if ($selfTestExitCode -ne 0) {
    throw "Windows speech helper self-test failed with exit code $selfTestExitCode."
}
if ($selfTestLines.Count -ne 1) {
    throw "Windows speech helper self-test produced $($selfTestLines.Count) stdout lines; exactly one protocol event was expected."
}
try {
    $selfTest = $selfTestLines[0] | ConvertFrom-Json -ErrorAction Stop
} catch {
    throw "Windows speech helper self-test did not produce valid JSON: $($selfTestLines[0])"
}
if ($selfTest.protocolVersion -ne 1 -or $selfTest.runId -ne "build-check" -or $selfTest.type -ne "self-test" -or $selfTest.ok -ne $true) {
    throw "Windows speech helper self-test returned an invalid or failed protocol event: $($selfTestLines[0])"
}

$pathLabel = if ($usedFallback) { "Roslyn fallback" } else { "MSBuild" }
Write-Host "Windows speech helper: $helper"
Write-Host "Windows speech helper verified: --self-test ($pathLabel)"
