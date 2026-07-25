param(
    [ValidateSet("debug", "release")]
    [string]$Configuration = "release",
    [string]$RuntimeDir = ""
)

$ErrorActionPreference = "Stop"

$projectRoot = [System.IO.Path]::GetFullPath((Split-Path -Parent $PSScriptRoot))
if ([string]::IsNullOrWhiteSpace($RuntimeDir)) {
    $RuntimeDir = Join-Path $projectRoot $Configuration
}
$RuntimeDir = [System.IO.Path]::GetFullPath($RuntimeDir)

if (-not (Test-Path -LiteralPath $RuntimeDir -PathType Container)) {
    throw "Runtime directory not found: $RuntimeDir"
}

function Get-PeMachine {
    param([string]$Path)

    $stream = [System.IO.File]::Open(
        $Path,
        [System.IO.FileMode]::Open,
        [System.IO.FileAccess]::Read,
        [System.IO.FileShare]::ReadWrite
    )
    try {
        $reader = New-Object System.IO.BinaryReader($stream)
        if ($reader.ReadUInt16() -ne 0x5A4D) {
            throw "Not a valid PE file: $Path"
        }

        $stream.Position = 0x3C
        $peOffset = $reader.ReadInt32()
        $stream.Position = $peOffset
        if ($reader.ReadUInt32() -ne 0x00004550) {
            throw "Invalid PE header: $Path"
        }

        return $reader.ReadUInt16()
    }
    finally {
        $stream.Dispose()
    }
}

$requiredFiles = @(
    "vocekit.exe",
    "Qt5Core.dll",
    "Qt5Gui.dll",
    "Qt5Widgets.dll",
    "Qt5Network.dll",
    "Qt5Multimedia.dll",
    "Qt5WebSockets.dll",
    "libgcc_s_dw2-1.dll",
    "libstdc++-6.dll",
    "libwinpthread-1.dll",
    "libeay32.dll",
    "ssleay32.dll",
    "platforms\qwindows.dll",
    "audio\qtaudio_windows.dll",
    "bearer\qgenericbearer.dll",
    "bearer\qnativewifibearer.dll",
    "mediaservice\dsengine.dll",
    "mediaservice\qtmedia_audioengine.dll",
    "translations\qt_zh_CN.qm",
    "ocr\windows\vocekit-windows-ocr.exe",
    "ocr\rapidocr\vocekit-rapidocr.exe",
    "ocr\rapidocr\LICENSE-RapidOcrOnnx.txt",
    "ocr\rapidocr\models\ch_PP-OCRv3_det_infer.onnx",
    "ocr\rapidocr\models\ch_PP-OCRv3_rec_infer.onnx",
    "ocr\rapidocr\models\ch_ppocr_mobile_v2.0_cls_infer.onnx",
    "ocr\rapidocr\models\ppocr_keys_v1.txt"
)

$missingFiles = New-Object System.Collections.Generic.List[string]
foreach ($relativePath in $requiredFiles) {
    $path = Join-Path $RuntimeDir $relativePath
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
        $missingFiles.Add($relativePath)
        continue
    }

    if ((Get-Item -LiteralPath $path).Length -le 0) {
        $missingFiles.Add("$relativePath (empty file)")
    }
}

if ($missingFiles.Count -gt 0) {
    $details = ($missingFiles | ForEach-Object { "  - $_" }) -join [Environment]::NewLine
    throw "Runtime verification failed. Missing files:$([Environment]::NewLine)$details"
}

# Check the architecture of DLLs loaded directly by the Qt application.
# This catches accidental deployment of a 64-bit OpenSSL runtime beside an x86 build.
$mainExecutable = Join-Path $RuntimeDir "vocekit.exe"
$expectedMachine = Get-PeMachine -Path $mainExecutable
$loadedDlls = @(
    "Qt5Core.dll",
    "Qt5Gui.dll",
    "Qt5Widgets.dll",
    "Qt5Network.dll",
    "Qt5Multimedia.dll",
    "Qt5WebSockets.dll",
    "libgcc_s_dw2-1.dll",
    "libstdc++-6.dll",
    "libwinpthread-1.dll",
    "libeay32.dll",
    "ssleay32.dll",
    "platforms\qwindows.dll",
    "audio\qtaudio_windows.dll"
)

$wrongArchitecture = New-Object System.Collections.Generic.List[string]
foreach ($relativePath in $loadedDlls) {
    $path = Join-Path $RuntimeDir $relativePath
    $machine = Get-PeMachine -Path $path
    if ($machine -ne $expectedMachine) {
        $wrongArchitecture.Add($relativePath)
    }
}

if ($wrongArchitecture.Count -gt 0) {
    $details = ($wrongArchitecture | ForEach-Object { "  - $_" }) -join [Environment]::NewLine
    throw "Runtime verification failed. Architecture mismatch:$([Environment]::NewLine)$details"
}

$architecture = switch ($expectedMachine) {
    0x014C { "x86" }
    0x8664 { "x64" }
    default { "0x{0:X4}" -f $expectedMachine }
}

Write-Host "Runtime verification passed: $RuntimeDir"
Write-Host "Application architecture: $architecture"
Write-Host "Checked files: $($requiredFiles.Count)"
