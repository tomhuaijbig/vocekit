param(
    [string]$PackageName = "vocekit-test"
)

$ErrorActionPreference = "Stop"

$projectRoot = [System.IO.Path]::GetFullPath((Split-Path -Parent $PSScriptRoot))
$releaseDir = Join-Path $projectRoot "release"
$distDir = Join-Path $projectRoot "dist"
$packageDir = Join-Path $distDir $PackageName
$zipPath = Join-Path $distDir "$PackageName.zip"
$runtimeVerifier = Join-Path $PSScriptRoot "verify-runtime.ps1"

function Assert-ChildPath {
    param(
        [string]$BasePath,
        [string]$TargetPath
    )

    $baseFull = [System.IO.Path]::GetFullPath($BasePath).TrimEnd("\") + "\"
    $targetFull = [System.IO.Path]::GetFullPath($TargetPath)
    if (-not $targetFull.StartsWith($baseFull, [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "Refusing to modify a path outside the distribution directory: $targetFull"
    }
}

$releaseExe = Join-Path $releaseDir "vocekit.exe"
if (-not (Test-Path -LiteralPath $releaseExe)) {
    throw "Release executable not found. Build and deploy the release version first: $releaseExe"
}
if (-not (Test-Path -LiteralPath $runtimeVerifier)) {
    throw "Runtime verifier not found: $runtimeVerifier"
}
& $runtimeVerifier -Configuration release -RuntimeDir $releaseDir

$windowsOcrHelper = Join-Path $releaseDir "ocr\windows\vocekit-windows-ocr.exe"
if (-not (Test-Path -LiteralPath $windowsOcrHelper)) {
    throw "Windows OCR helper is missing from release. Run scripts\deploy.ps1 after building OCR helpers."
}
$rapidOcrHelper = Join-Path $releaseDir "ocr\rapidocr\vocekit-rapidocr.exe"
$rapidOcrModels = Join-Path $releaseDir "ocr\rapidocr\models"
if (-not (Test-Path -LiteralPath $rapidOcrHelper)) {
    throw "RapidOCR helper is missing from release. Run scripts\deploy.ps1 after building OCR helpers."
}
if (-not (Test-Path -LiteralPath $rapidOcrModels)) {
    throw "RapidOCR models are missing from release. Run scripts\deploy.ps1 after building OCR helpers."
}
$windowsSpeechHelper = Join-Path $releaseDir "speech\windows\vocekit-windows-speech.exe"
if (-not (Test-Path -LiteralPath $windowsSpeechHelper -PathType Leaf)) {
    throw "Windows speech helper is missing from release. Run scripts\build-runtime-helpers.ps1, then scripts\deploy.ps1."
}

New-Item -ItemType Directory -Path $distDir -Force | Out-Null
Assert-ChildPath -BasePath $distDir -TargetPath $packageDir
Assert-ChildPath -BasePath $distDir -TargetPath $zipPath

if (Test-Path -LiteralPath $packageDir) {
    Remove-Item -LiteralPath $packageDir -Recurse -Force
}
if (Test-Path -LiteralPath $zipPath) {
    Remove-Item -LiteralPath $zipPath -Force
}

New-Item -ItemType Directory -Path $packageDir -Force | Out-Null

$releaseFull = [System.IO.Path]::GetFullPath($releaseDir).TrimEnd("\")
$excludedExtensions = @(".o", ".obj", ".cpp", ".h", ".cs", ".csproj", ".pro", ".log", ".tmp", ".pdb", ".ilk", ".pcm", ".wav")
$runtimeFiles = Get-ChildItem -LiteralPath $releaseDir -Recurse -File | Where-Object {
    $excludedExtensions -notcontains $_.Extension.ToLowerInvariant()
}

foreach ($file in $runtimeFiles) {
    $relativePath = $file.FullName.Substring($releaseFull.Length).TrimStart("\")
    $destination = Join-Path $packageDir $relativePath
    New-Item -ItemType Directory -Path (Split-Path -Parent $destination) -Force | Out-Null
    Copy-Item -LiteralPath $file.FullName -Destination $destination -Force
}

$configDir = Join-Path $packageDir "config"
$promptsDir = Join-Path $packageDir "prompts"
$recordsDir = Join-Path $packageDir "records"
New-Item -ItemType Directory -Path $configDir, $promptsDir, $recordsDir -Force | Out-Null
$historyBackupName = -join ([char]0x5907, [char]0x4efd, [char]0x6587, [char]0x4ef6)
$historyAllAudioName = -join ([char]0x603b, [char]0x5f55, [char]0x97f3, [char]0x6587, [char]0x4ef6)
$historyAllTextName = -join ([char]0x603b, [char]0x6587, [char]0x672c, [char]0x6587, [char]0x4ef6)
$historyAllDetailName = -join ([char]0x603b, [char]0x8be6, [char]0x7ec6, [char]0x8bb0, [char]0x5f55, [char]0x6587, [char]0x4ef6)
New-Item -ItemType Directory -Path `
    (Join-Path $recordsDir $historyBackupName), `
    (Join-Path $recordsDir $historyAllAudioName), `
    (Join-Path $recordsDir $historyAllTextName), `
    (Join-Path $recordsDir $historyAllDetailName) `
    -Force | Out-Null

Copy-Item -LiteralPath (Join-Path $projectRoot "config\secrets.example.json") `
    -Destination (Join-Path $configDir "secrets.json") -Force
Copy-Item -LiteralPath (Join-Path $projectRoot "config\settings.example.json") `
    -Destination (Join-Path $configDir "settings.json") -Force
Copy-Item -Path (Join-Path $projectRoot "prompts\*") -Destination $promptsDir -Force
Copy-Item -LiteralPath (Join-Path $projectRoot "docs\TESTING.md") `
    -Destination (Join-Path $packageDir "TESTING.md") -Force

if (-not (Test-Path -LiteralPath (Join-Path $packageDir "ocr\windows\vocekit-windows-ocr.exe"))) {
    throw "The generated package is missing the Windows OCR helper."
}
$packagedRapidHelper = Join-Path $packageDir "ocr\rapidocr\vocekit-rapidocr.exe"
if (-not (Test-Path -LiteralPath $packagedRapidHelper)) {
    throw "The generated package is missing the RapidOCR helper."
}
$requiredRapidModels = @(
    "ch_PP-OCRv3_det_infer.onnx",
    "ch_PP-OCRv3_rec_infer.onnx",
    "ch_ppocr_mobile_v2.0_cls_infer.onnx",
    "ppocr_keys_v1.txt"
)
foreach ($modelName in $requiredRapidModels) {
    $modelPath = Join-Path $packageDir "ocr\rapidocr\models\$modelName"
    if (-not (Test-Path -LiteralPath $modelPath)) {
        throw "The generated package is missing a RapidOCR model: $modelName"
    }
}

$packagedSpeechHelper = Join-Path $packageDir "speech\windows\vocekit-windows-speech.exe"
if (-not (Test-Path -LiteralPath $packagedSpeechHelper -PathType Leaf)) {
    throw "The generated package is missing the Windows speech helper."
}
$speechSelfTestOutput = & $packagedSpeechHelper --self-test --run-id package-verify 2>&1
if ($LASTEXITCODE -ne 0 -or ($speechSelfTestOutput -join "`n") -notmatch '"type":"self-test"') {
    throw "Packaged Windows speech helper self-test failed.`n$($speechSelfTestOutput -join "`n")"
}

$secretConfig = Get-Content -LiteralPath (Join-Path $configDir "secrets.json") -Raw -Encoding UTF8 | ConvertFrom-Json
$nonBlankSecrets = @($secretConfig.PSObject.Properties | Where-Object {
    -not [string]::IsNullOrWhiteSpace([string]$_.Value)
})
if ($nonBlankSecrets.Count -ne 0) {
    throw "The generated package contains non-empty API credentials."
}

$forbiddenFiles = @(Get-ChildItem -LiteralPath $packageDir -Recurse -File | Where-Object {
    $_.Extension.ToLowerInvariant() -in @(".o", ".obj", ".cpp", ".h", ".cs", ".csproj", ".pro", ".log", ".tmp", ".pdb", ".ilk", ".pcm", ".wav") -or
    $_.Name -eq "1.txt" -or
    $_.Name -eq "secrets.example.json" -or
    $_.Name -eq "settings.example.json"
})
if ($forbiddenFiles.Count -ne 0) {
    throw "Forbidden development or personal files were found in the generated package."
}

$keyLikeText = @(Get-ChildItem -LiteralPath $packageDir -Recurse -File | Where-Object {
    $_.Extension.ToLowerInvariant() -in @(".json", ".md", ".txt")
} | Select-String -Pattern "sk-[A-Za-z0-9_-]{20,}" -ErrorAction Stop)
if ($keyLikeText.Count -ne 0) {
    throw "A possible API key was found in the generated package."
}

& $runtimeVerifier -Configuration release -RuntimeDir $packageDir

Compress-Archive -Path (Join-Path $packageDir "*") -DestinationPath $zipPath -CompressionLevel Optimal -Force

$packageSize = (Get-ChildItem -LiteralPath $packageDir -Recurse -File | Measure-Object -Property Length -Sum).Sum
$zipSize = (Get-Item -LiteralPath $zipPath).Length
Write-Host "Test package created:"
Write-Host "  Folder: $packageDir"
Write-Host "  Archive: $zipPath"
Write-Host "  Package bytes: $packageSize"
Write-Host "  Archive bytes: $zipSize"
