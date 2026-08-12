param(
    [ValidateSet("debug", "release")]
    [string]$Configuration = "debug",
    [string]$QtBin = $env:QT_BIN,
    [string]$MingwBin = $env:MINGW_BIN,
    [string]$OpenSslBin = $env:OPENSSL_BIN
)

$ErrorActionPreference = "Stop"

if ([string]::IsNullOrWhiteSpace($QtBin)) {
    throw "QtBin is required. Pass -QtBin or set the QT_BIN environment variable."
}
if ([string]::IsNullOrWhiteSpace($MingwBin)) {
    throw "MingwBin is required. Pass -MingwBin or set the MINGW_BIN environment variable."
}
if ([string]::IsNullOrWhiteSpace($OpenSslBin)) {
    throw "OpenSslBin is required. Pass -OpenSslBin or set the OPENSSL_BIN environment variable."
}

$projectRoot = Split-Path -Parent $PSScriptRoot
$executable = Join-Path $projectRoot "$Configuration\vocekit.exe"
$executableDir = Split-Path -Parent $executable
$deployTool = Join-Path $QtBin "windeployqt.exe"
$windowsOcrHelperSourcePath = [System.IO.Path]::Combine($projectRoot, "helpers", "bin", "vocekit-windows-ocr.exe")
$rapidOcrHelperSourcePath = [System.IO.Path]::Combine($projectRoot, "helpers", "bin", "vocekit-rapidocr.exe")
$rapidOcrModelsSourcePath = [System.IO.Path]::Combine($projectRoot, "helpers", "bin", "models")
$rapidOcrLicenseSourcePath = [System.IO.Path]::Combine($projectRoot, "helpers", "bin", "LICENSE-RapidOcrOnnx.txt")
$windowsSpeechHelperSourcePath = [System.IO.Path]::Combine($projectRoot, "helpers", "bin", "vocekit-windows-speech.exe")

if (-not (Test-Path -LiteralPath $executable)) {
    throw "Executable not found: $executable. Build the $Configuration version first."
}
if (-not (Test-Path -LiteralPath $deployTool)) {
    throw "Qt deployment tool not found: $deployTool"
}
if (-not (Test-Path -LiteralPath $MingwBin)) {
    throw "MinGW runtime directory not found: $MingwBin"
}
if (-not (Test-Path -LiteralPath $OpenSslBin)) {
    throw "OpenSSL runtime directory not found: $OpenSslBin"
}

# Add the compiler, Qt, and OpenSSL directories for deployment dependency discovery.
$env:Path = "$MingwBin;$QtBin;$OpenSslBin;$env:Path"
$mode = if ($Configuration -eq "debug") { "--debug" } else { "--release" }

& $deployTool $mode --compiler-runtime --no-translations $executable
if ($LASTEXITCODE -ne 0) {
    throw "Runtime deployment failed with exit code: $LASTEXITCODE"
}

# Qt 5.9 HTTPS requires the OpenSSL 1.0.x runtime.
# windeployqt does not reliably copy these two DLLs, so deploy them explicitly.
$openSslDlls = @("libeay32.dll", "ssleay32.dll")
foreach ($dllName in $openSslDlls) {
    $sourceDll = Join-Path $OpenSslBin $dllName
    if (-not (Test-Path -LiteralPath $sourceDll)) {
        throw "OpenSSL runtime DLL not found: $sourceDll"
    }

    Copy-Item -LiteralPath $sourceDll -Destination (Join-Path $executableDir $dllName) -Force
}

$qtRoot = Split-Path -Parent $QtBin
$sourceTranslation = Join-Path $qtRoot "translations\qt_zh_CN.qm"
$targetTranslations = Join-Path $executableDir "translations"
if (-not (Test-Path -LiteralPath $sourceTranslation)) {
    throw "Qt Chinese translation not found: $sourceTranslation"
}
New-Item -ItemType Directory -Path $targetTranslations -Force | Out-Null
Copy-Item -LiteralPath $sourceTranslation -Destination (Join-Path $targetTranslations "qt_zh_CN.qm") -Force

# The Windows OCR helper is required in every deployed build.
$windowsOcrTargetDir = Join-Path $executableDir "ocr\windows"
if (-not (Test-Path -LiteralPath $windowsOcrHelperSourcePath)) {
    throw "Windows OCR helper not found: $windowsOcrHelperSourcePath. Run scripts\build-ocr-helpers.ps1 first."
}
New-Item -ItemType Directory -Path $windowsOcrTargetDir -Force | Out-Null
Copy-Item -LiteralPath $windowsOcrHelperSourcePath -Destination (Join-Path $windowsOcrTargetDir "vocekit-windows-ocr.exe") -Force

if (-not (Test-Path -LiteralPath $rapidOcrHelperSourcePath)) {
    throw "RapidOCR helper not found: $rapidOcrHelperSourcePath. Run scripts\build-ocr-helpers.ps1 first."
}
if (-not (Test-Path -LiteralPath $rapidOcrModelsSourcePath)) {
    throw "RapidOCR models not found: $rapidOcrModelsSourcePath. Run scripts\build-ocr-helpers.ps1 first."
}
if (-not (Test-Path -LiteralPath $rapidOcrLicenseSourcePath)) {
    throw "RapidOCR license not found: $rapidOcrLicenseSourcePath. Run scripts\build-ocr-helpers.ps1 first."
}
$rapidOcrTargetDir = Join-Path $executableDir "ocr\rapidocr"
$rapidOcrModelsTargetDir = Join-Path $rapidOcrTargetDir "models"
New-Item -ItemType Directory -Path $rapidOcrModelsTargetDir -Force | Out-Null
Copy-Item -LiteralPath $rapidOcrHelperSourcePath -Destination (Join-Path $rapidOcrTargetDir "vocekit-rapidocr.exe") -Force
Copy-Item -Path (Join-Path $rapidOcrModelsSourcePath "*") -Destination $rapidOcrModelsTargetDir -Force
Copy-Item -LiteralPath $rapidOcrLicenseSourcePath -Destination (Join-Path $rapidOcrTargetDir "LICENSE-RapidOcrOnnx.txt") -Force

$windowsSpeechTargetDir = Join-Path $executableDir "speech\windows"
if (-not (Test-Path -LiteralPath $windowsSpeechHelperSourcePath -PathType Leaf)) {
    throw "Windows speech helper not found: $windowsSpeechHelperSourcePath. Run scripts\build-runtime-helpers.ps1 first."
}
New-Item -ItemType Directory -Path $windowsSpeechTargetDir -Force | Out-Null
Copy-Item -LiteralPath $windowsSpeechHelperSourcePath `
    -Destination (Join-Path $windowsSpeechTargetDir "vocekit-windows-speech.exe") -Force

$runtimeVerifier = Join-Path $PSScriptRoot "verify-runtime.ps1"
if (-not (Test-Path -LiteralPath $runtimeVerifier)) {
    throw "Runtime verifier not found: $runtimeVerifier"
}
& $runtimeVerifier -Configuration $Configuration -RuntimeDir $executableDir

Write-Host "Runtime dependencies deployed to: $executableDir"

# Maintenance note: keep this list synchronized with Qt modules and runtime helpers.
