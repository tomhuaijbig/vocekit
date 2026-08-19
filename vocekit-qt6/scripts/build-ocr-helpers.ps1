param(
    [ValidateSet("Release")]
    [string]$Configuration = "Release",
    [string]$MSBuildPath = ""
)

$ErrorActionPreference = "Stop"
$projectRoot = Split-Path -Parent $PSScriptRoot

function Resolve-VisualStudioMsBuild {
    param(
        [string]$ExplicitPath
    )

    if (-not [string]::IsNullOrWhiteSpace($ExplicitPath)) {
        $resolved = [IO.Path]::GetFullPath($ExplicitPath)
        if (-not (Test-Path -LiteralPath $resolved -PathType Leaf)) {
            throw "The requested Visual Studio MSBuild executable was not found: $resolved"
        }
        return $resolved
    }

    $candidates = @()
    $programFilesX86 = [Environment]::GetFolderPath(
        [Environment+SpecialFolder]::ProgramFilesX86
    )
    $vswhere = Join-Path $programFilesX86 `
        "Microsoft Visual Studio\Installer\vswhere.exe"
    if (Test-Path -LiteralPath $vswhere -PathType Leaf) {
        $installationPaths = @(& $vswhere `
            -version "[17.0,18.0)" `
            -latest `
            -products "*" `
            -requires Microsoft.Component.MSBuild `
                Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
            -property installationPath)
        if ($LASTEXITCODE -ne 0) {
            throw "Visual Studio Installer vswhere failed with exit code $LASTEXITCODE."
        }
        $candidates += @(
            $installationPaths |
                Where-Object { -not [string]::IsNullOrWhiteSpace($_) }
        )
    }

    $programFilesRoots = @(
        [Environment]::GetFolderPath([Environment+SpecialFolder]::ProgramFiles),
        $programFilesX86
    ) | Where-Object {
        -not [string]::IsNullOrWhiteSpace($_)
    } | Select-Object -Unique
    foreach ($root in $programFilesRoots) {
        foreach ($edition in @(
            "Enterprise", "Professional", "Community", "BuildTools"
        )) {
            $candidates += Join-Path $root `
                ("Microsoft Visual Studio\2022\" + $edition)
        }
    }

    foreach ($installationPath in @($candidates | Select-Object -Unique)) {
        if ([string]::IsNullOrWhiteSpace($installationPath)) {
            continue
        }
        $candidate = Join-Path $installationPath `
            "MSBuild\Current\Bin\MSBuild.exe"
        if (Test-Path -LiteralPath $candidate -PathType Leaf) {
            return [IO.Path]::GetFullPath($candidate)
        }
    }

    throw "Visual Studio 2022 MSBuild with the x64 C++ toolchain was not found."
}

function Resolve-VisualStudioDumpBin {
    param(
        [Parameter(Mandatory = $true)]
        [string]$ResolvedMsBuildPath
    )

    $installationPath = $ResolvedMsBuildPath
    for ($index = 0; $index -lt 4; ++$index) {
        $installationPath = Split-Path -Parent $installationPath
    }
    $msvcToolsRoot = Join-Path $installationPath "VC\Tools\MSVC"
    if (Test-Path -LiteralPath $msvcToolsRoot -PathType Container) {
        foreach ($toolset in Get-ChildItem -LiteralPath $msvcToolsRoot -Directory |
            Sort-Object -Property Name -Descending) {
            $candidate = Join-Path $toolset.FullName `
                "bin\Hostx64\x64\dumpbin.exe"
            if (Test-Path -LiteralPath $candidate -PathType Leaf) {
                return $candidate
            }
        }
    }
    throw "Visual Studio x64 dumpbin.exe was not found under: $msvcToolsRoot"
}

function Assert-NoDynamicVisualCppRuntime {
    param(
        [Parameter(Mandatory = $true)]
        [string]$HelperPath,
        [Parameter(Mandatory = $true)]
        [string]$DumpBinPath
    )

    $output = @(& $DumpBinPath /dependents $HelperPath 2>&1 | ForEach-Object {
        $_.ToString()
    })
    if ($LASTEXITCODE -ne 0) {
        throw "dumpbin dependency inspection failed for $HelperPath with exit code $LASTEXITCODE."
    }
    $dynamicRuntime = @(
        [regex]::Matches(
            ($output -join [Environment]::NewLine),
            '(?im)^\s*((?:MSVCP\d+|VCRUNTIME\d*(?:_\d+)?)\.dll)\s*$'
        ) | ForEach-Object { $_.Groups[1].Value.ToUpperInvariant() } |
            Select-Object -Unique
    )
    if ($dynamicRuntime.Count -gt 0) {
        throw (
            "Portable OCR helper depends on the dynamic Visual C++ runtime: " +
            "$HelperPath -> $($dynamicRuntime -join ', ')"
        )
    }
    Write-Host "Portable OCR helper runtime check passed: $HelperPath"
}

$msbuild = Resolve-VisualStudioMsBuild -ExplicitPath $MSBuildPath
$dumpbin = Resolve-VisualStudioDumpBin -ResolvedMsBuildPath $msbuild
Write-Host "Visual Studio MSBuild: $msbuild"
Write-Host "Visual Studio dumpbin: $dumpbin"

$windowsProject = Join-Path $projectRoot "helpers\windows_ocr\windows_ocr.vcxproj"
& $msbuild $windowsProject /m /p:Configuration=$Configuration /p:Platform=x64
if ($LASTEXITCODE -ne 0) {
    throw "Windows OCR helper build failed with exit code $LASTEXITCODE."
}

$windowsHelper = Join-Path $projectRoot "helpers\bin\vocekit-windows-ocr.exe"
if (-not (Test-Path -LiteralPath $windowsHelper)) {
    throw "Windows OCR helper was not generated: $windowsHelper"
}

$rapidProjectRoot = Join-Path $projectRoot "Project_RapidOcrOnnx-1.2.2"
$rapidProject = Join-Path $projectRoot "helpers\rapidocr\rapidocr_helper.vcxproj"
$rapidModelsSource = Join-Path $rapidProjectRoot "models"
$rapidLicenseSource = Join-Path $rapidProjectRoot "LICENSE"
$rapidHelper = Join-Path $projectRoot "helpers\bin\vocekit-rapidocr.exe"
$rapidModelsTarget = Join-Path $projectRoot "helpers\bin\models"
$rapidLicenseTarget = Join-Path $projectRoot "helpers\bin\LICENSE-RapidOcrOnnx.txt"

if (-not (Test-Path -LiteralPath $rapidProjectRoot)) {
    throw "RapidOcrOnnx project was not found: $rapidProjectRoot"
}

& $msbuild $rapidProject /m /p:Configuration=$Configuration /p:Platform=x64
if ($LASTEXITCODE -ne 0) {
    throw "RapidOCR helper build failed with exit code $LASTEXITCODE."
}
if (-not (Test-Path -LiteralPath $rapidHelper)) {
    throw "RapidOCR helper was not generated: $rapidHelper"
}

Assert-NoDynamicVisualCppRuntime `
    -HelperPath $windowsHelper `
    -DumpBinPath $dumpbin
Assert-NoDynamicVisualCppRuntime `
    -HelperPath $rapidHelper `
    -DumpBinPath $dumpbin

$requiredModels = @{
    "ch_PP-OCRv3_det_infer.onnx" = "3439588C030FAEA393A54515F51E983D8E155B19A2E8ABA7891934C1CF0DE526"
    "ch_PP-OCRv3_rec_infer.onnx" = "897A3EDEDB38FEE0DAE2C1CCEE38241F37DF202C9509E3ABCA02E9217C5EE615"
    "ch_ppocr_mobile_v2.0_cls_infer.onnx" = "E47ACEDF663230F8863FF1AB0E64DD2D82B838FCEB5957146DAB185A89D6215C"
    "ppocr_keys_v1.txt" = "28B2362AD4AB2DC38769AA72FEB535E3A9DDB3FD2A7585A05920E6393B1DC7F7"
}
New-Item -ItemType Directory -Path $rapidModelsTarget -Force | Out-Null
foreach ($modelName in $requiredModels.Keys) {
    $source = Join-Path $rapidModelsSource $modelName
    if (-not (Test-Path -LiteralPath $source)) {
        throw "RapidOCR model was not found: $source"
    }
    $actualHash = (Get-FileHash -LiteralPath $source -Algorithm SHA256).Hash
    if ($actualHash -ne $requiredModels[$modelName]) {
        throw "RapidOCR model checksum mismatch: $modelName"
    }
    Copy-Item -LiteralPath $source -Destination (Join-Path $rapidModelsTarget $modelName) -Force
}

if (-not (Test-Path -LiteralPath $rapidLicenseSource)) {
    throw "RapidOcrOnnx license was not found: $rapidLicenseSource"
}
Copy-Item -LiteralPath $rapidLicenseSource -Destination $rapidLicenseTarget -Force

Write-Host "Windows OCR helper: $windowsHelper"
Write-Host "RapidOCR helper: $rapidHelper"
Write-Host "RapidOCR models: $rapidModelsTarget"
