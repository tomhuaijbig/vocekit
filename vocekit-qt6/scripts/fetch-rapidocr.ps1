param(
    [string]$ArchivePath = ""
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$projectRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$lockPath = Join-Path $projectRoot `
    "third_party\rapidocr\rapidocr-lock.json"
if (-not (Test-Path -LiteralPath $lockPath -PathType Leaf)) {
    throw "RapidOcrOnnx dependency lock was not found: $lockPath"
}
try {
    $dependencyLock = Get-Content -LiteralPath $lockPath -Raw -Encoding UTF8 |
        ConvertFrom-Json -ErrorAction Stop
} catch {
    throw "RapidOcrOnnx dependency lock is invalid JSON: $($_.Exception.Message)"
}

$sdkName = "Project_RapidOcrOnnx-$($dependencyLock.version)"
$sdkDirectory = Join-Path $projectRoot $sdkName
$archiveName = [string]$dependencyLock.archive_name
$archiveUri = [string]$dependencyLock.archive_url
$expectedArchiveBytes = [long]$dependencyLock.archive_bytes
$expectedArchiveSha256 = ([string]$dependencyLock.archive_sha256).ToUpperInvariant()
$expectedSdkFileCount = [int]$dependencyLock.sdk_file_count
$expectedSdkFingerprint = (
    [string]$dependencyLock.sdk_fingerprint_sha256
).ToUpperInvariant()
if ($dependencyLock.version -ne "1.2.2" -or
    $archiveName -ne "$sdkName.7z" -or
    $archiveUri -ne "https://github.com/RapidAI/RapidOcrOnnx/releases/download/1.2.2/$archiveName" -or
    $expectedArchiveBytes -le 0 -or
    $expectedArchiveSha256 -notmatch '^[0-9A-F]{64}$' -or
    $expectedSdkFileCount -le 0 -or
    $expectedSdkFingerprint -notmatch '^[0-9A-F]{64}$') {
    throw "RapidOcrOnnx dependency lock contains unsupported or invalid values."
}
$expectedModelHashes = @{
    "ch_PP-OCRv3_det_infer.onnx" = "3439588C030FAEA393A54515F51E983D8E155B19A2E8ABA7891934C1CF0DE526"
    "ch_PP-OCRv3_rec_infer.onnx" = "897A3EDEDB38FEE0DAE2C1CCEE38241F37DF202C9509E3ABCA02E9217C5EE615"
    "ch_ppocr_mobile_v2.0_cls_infer.onnx" = "E47ACEDF663230F8863FF1AB0E64DD2D82B838FCEB5957146DAB185A89D6215C"
    "ppocr_keys_v1.txt" = "28B2362AD4AB2DC38769AA72FEB535E3A9DDB3FD2A7585A05920E6393B1DC7F7" # gitleaks:allow -- public file checksum
}

function Get-RapidOcrSdkFingerprint {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Root
    )

    $records = [Collections.Generic.List[string]]::new()
    foreach ($file in Get-ChildItem -LiteralPath $Root -Recurse -File) {
        $relativePath = [IO.Path]::GetRelativePath(
            $Root,
            $file.FullName
        ).Replace("\", "/")
        $fileHash = (
            Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256
        ).Hash
        $records.Add(
            "$relativePath`t$($file.Length)`t$fileHash`n"
        )
    }
    $records.Sort([StringComparer]::Ordinal)
    $manifest = [Text.StringBuilder]::new()
    foreach ($record in $records) {
        [void]$manifest.Append($record)
    }
    $manifestBytes = [Text.UTF8Encoding]::new($false).GetBytes(
        $manifest.ToString()
    )
    $sha256 = [Security.Cryptography.SHA256]::Create()
    try {
        $fingerprint = (
            $sha256.ComputeHash($manifestBytes) |
                ForEach-Object { $_.ToString("x2") }
        ) -join ""
    } finally {
        $sha256.Dispose()
    }
    return [PSCustomObject]@{
        FileCount = $records.Count
        Sha256 = $fingerprint.ToUpperInvariant()
    }
}

function Assert-RapidOcrSdk {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Root
    )

    $requiredFiles = @(
        "LICENSE",
        "include\OcrLite.h",
        "src\OcrLite.cpp",
        "onnxruntime-static\windows-x64\lib\onnxruntime_session.lib",
        "opencv-static\windows-x64\x64\vc16\staticlib\opencv_core460.lib"
    )
    foreach ($relativePath in $requiredFiles) {
        $requiredFile = Join-Path $Root $relativePath
        if (-not (Test-Path -LiteralPath $requiredFile -PathType Leaf)) {
            throw "RapidOcrOnnx SDK file was not found: $requiredFile"
        }
    }

    $modelsDirectory = Join-Path $Root "models"
    foreach ($modelName in $expectedModelHashes.Keys) {
        $modelPath = Join-Path $modelsDirectory $modelName
        if (-not (Test-Path -LiteralPath $modelPath -PathType Leaf)) {
            throw "RapidOCR model was not found: $modelPath"
        }
        $actualHash = (Get-FileHash -LiteralPath $modelPath -Algorithm SHA256).Hash
        if ($actualHash -ne $expectedModelHashes[$modelName]) {
            throw "RapidOCR model checksum mismatch: $modelName"
        }
    }

    $sdkFingerprint = Get-RapidOcrSdkFingerprint -Root $Root
    if ($sdkFingerprint.FileCount -ne $expectedSdkFileCount -or
        $sdkFingerprint.Sha256 -ne $expectedSdkFingerprint) {
        throw (
            "RapidOcrOnnx SDK fingerprint mismatch: expected " +
            "$expectedSdkFileCount files / $expectedSdkFingerprint, got " +
            "$($sdkFingerprint.FileCount) files / $($sdkFingerprint.Sha256)."
        )
    }
}

if (Test-Path -LiteralPath $sdkDirectory -PathType Container) {
    Assert-RapidOcrSdk -Root $sdkDirectory
    Write-Host "Pinned RapidOcrOnnx SDK is already prepared: $sdkDirectory"
    return
}

$downloadRoot = if (-not [string]::IsNullOrWhiteSpace($env:RUNNER_TEMP)) {
    [IO.Path]::GetFullPath($env:RUNNER_TEMP)
} else {
    [IO.Path]::GetFullPath([IO.Path]::GetTempPath())
}
if ([string]::IsNullOrWhiteSpace($ArchivePath)) {
    $ArchivePath = Join-Path $downloadRoot $archiveName
    $downloaded = $false
    for ($attempt = 1; $attempt -le 3; ++$attempt) {
        try {
            Write-Host "Downloading pinned RapidOcrOnnx 1.2.2 archive (attempt $attempt/3)."
            Invoke-WebRequest `
                -Uri $archiveUri `
                -OutFile $ArchivePath `
                -TimeoutSec 600 `
                -UseBasicParsing
            $downloaded = $true
            break
        } catch {
            if ($attempt -eq 3) {
                throw
            }
            Write-Warning "RapidOcrOnnx download attempt $attempt failed: $($_.Exception.Message)"
            Start-Sleep -Seconds (2 * $attempt)
        }
    }
    if (-not $downloaded) {
        throw "RapidOcrOnnx archive download did not complete."
    }
} else {
    $ArchivePath = [IO.Path]::GetFullPath($ArchivePath)
}

if (-not (Test-Path -LiteralPath $ArchivePath -PathType Leaf)) {
    throw "RapidOcrOnnx archive was not found: $ArchivePath"
}
$archive = Get-Item -LiteralPath $ArchivePath
if ($archive.Length -ne $expectedArchiveBytes) {
    throw "RapidOcrOnnx archive size mismatch: expected $expectedArchiveBytes, got $($archive.Length)."
}
$actualArchiveSha256 = (
    Get-FileHash -LiteralPath $ArchivePath -Algorithm SHA256
).Hash
if ($actualArchiveSha256 -ne $expectedArchiveSha256) {
    throw "RapidOcrOnnx archive checksum mismatch."
}
Write-Host "RapidOcrOnnx archive verified: SHA-256 $actualArchiveSha256"

$sevenZip = (Get-Command 7z.exe -ErrorAction Stop).Source
$extractRoot = Join-Path $downloadRoot (
    "vocekit-rapidocr-extract-" + [Guid]::NewGuid().ToString("N")
)
$fullExtractRoot = [IO.Path]::GetFullPath($extractRoot)
$downloadPrefix = $downloadRoot.TrimEnd(
    [IO.Path]::DirectorySeparatorChar,
    [IO.Path]::AltDirectorySeparatorChar
) + [IO.Path]::DirectorySeparatorChar
if (-not $fullExtractRoot.StartsWith(
    $downloadPrefix,
    [StringComparison]::OrdinalIgnoreCase
)) {
    throw "RapidOcrOnnx extraction path escaped the temporary directory."
}

New-Item -ItemType Directory -Path $fullExtractRoot | Out-Null
try {
    & $sevenZip x $ArchivePath "-o$fullExtractRoot" -y
    if ($LASTEXITCODE -ne 0) {
        throw "RapidOcrOnnx extraction failed with exit code $LASTEXITCODE."
    }
    $extractedSdk = Join-Path $fullExtractRoot $sdkName
    Assert-RapidOcrSdk -Root $extractedSdk
    if (Test-Path -LiteralPath $sdkDirectory) {
        throw "RapidOcrOnnx SDK destination appeared during extraction: $sdkDirectory"
    }
    Move-Item -LiteralPath $extractedSdk -Destination $sdkDirectory
} finally {
    if (Test-Path -LiteralPath $fullExtractRoot -PathType Container) {
        Remove-Item -LiteralPath $fullExtractRoot -Recurse -Force
    }
}

Assert-RapidOcrSdk -Root $sdkDirectory
Write-Host "Pinned RapidOcrOnnx SDK prepared: $sdkDirectory"
