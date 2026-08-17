param(
    [string]$QtBin = "D:\QT66666\6.11.1\mingw_64\bin",
    [string]$MingwBin = "D:\QT66666\Tools\mingw1310_64\bin",
    [string]$Destination
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$projectRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
if ([string]::IsNullOrWhiteSpace($Destination)) {
    $Destination = Join-Path $projectRoot ".qt6-deploy"
}
$Destination = [IO.Path]::GetFullPath($Destination)

$sourceExecutable = Join-Path $projectRoot ".qt6-build\release\vocekit.exe"
$deployTool = Join-Path $QtBin "windeployqt.exe"
$sourceHelperRoot = Join-Path $projectRoot "helpers\bin"
$translationSource = Join-Path (Split-Path -Parent $QtBin) "translations\qt_zh_CN.qm"
$runtimeVerifier = Join-Path $PSScriptRoot "verify-runtime.ps1"

$requiredSources = @(
    $sourceExecutable,
    $deployTool,
    (Join-Path $sourceHelperRoot "vocekit-windows-ocr.exe"),
    (Join-Path $sourceHelperRoot "vocekit-rapidocr.exe"),
    (Join-Path $sourceHelperRoot "vocekit-windows-speech.exe"),
    (Join-Path $sourceHelperRoot "LICENSE-RapidOcrOnnx.txt"),
    (Join-Path $sourceHelperRoot "models"),
    $translationSource,
    $runtimeVerifier
)
foreach ($source in $requiredSources) {
    if (-not (Test-Path -LiteralPath $source)) {
        throw "Required Qt 6 deployment source not found: $source"
    }
}

New-Item -ItemType Directory -Path $Destination -Force | Out-Null
$targetExecutable = Join-Path $Destination "vocekit.exe"
Copy-Item -LiteralPath $sourceExecutable -Destination $targetExecutable -Force

$originalPath = $env:PATH
try {
    $env:PATH = "$QtBin;$MingwBin;$env:PATH"
    & $deployTool --release --compiler-runtime --no-translations $targetExecutable
    if ($LASTEXITCODE -ne 0) {
        throw "Qt 6 deployment failed with exit code $LASTEXITCODE"
    }
} finally {
    $env:PATH = $originalPath
}

$translationsDir = Join-Path $Destination "translations"
New-Item -ItemType Directory -Path $translationsDir -Force | Out-Null
Copy-Item -LiteralPath $translationSource -Destination (Join-Path $translationsDir "qt_zh_CN.qm") -Force

$windowsOcrDir = Join-Path $Destination "ocr\windows"
$rapidOcrDir = Join-Path $Destination "ocr\rapidocr"
$rapidOcrModelsDir = Join-Path $rapidOcrDir "models"
$windowsSpeechDir = Join-Path $Destination "speech\windows"
foreach ($directory in @($windowsOcrDir, $rapidOcrDir, $rapidOcrModelsDir, $windowsSpeechDir)) {
    New-Item -ItemType Directory -Path $directory -Force | Out-Null
}

Copy-Item -LiteralPath (Join-Path $sourceHelperRoot "vocekit-windows-ocr.exe") -Destination $windowsOcrDir -Force
Copy-Item -LiteralPath (Join-Path $sourceHelperRoot "vocekit-rapidocr.exe") -Destination $rapidOcrDir -Force
Copy-Item -LiteralPath (Join-Path $sourceHelperRoot "LICENSE-RapidOcrOnnx.txt") -Destination $rapidOcrDir -Force
Copy-Item -Path (Join-Path $sourceHelperRoot "models\*") -Destination $rapidOcrModelsDir -Force
Copy-Item -LiteralPath (Join-Path $sourceHelperRoot "vocekit-windows-speech.exe") -Destination $windowsSpeechDir -Force

& $runtimeVerifier -Configuration release -RuntimeDir $Destination

$files = Get-ChildItem -LiteralPath $Destination -File -Recurse
$sizeBytes = ($files | Measure-Object -Property Length -Sum).Sum
Write-Host "Qt 6 deployment succeeded: $Destination"
Write-Host "Files: $($files.Count); Bytes: $sizeBytes"
