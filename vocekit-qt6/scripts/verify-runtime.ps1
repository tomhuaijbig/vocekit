param(
    [ValidateSet("debug", "release")]
    [string]$Configuration = "release",
    [string]$RuntimeDir = "",
    [string]$SpeechHelperSelfTestPath = "",
    [int]$SpeechHelperSelfTestTimeoutMs = 15000,
    [string[]]$SpeechHelperSelfTestExtraArguments = @(),
    [ValidateSet("", "timeout")]
    [string]$SpeechHelperSelfTestScenario = "",
    [switch]$SpeechHelperSelfTestOnly
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function Invoke-SpeechHelperSelfTest {
    param(
        [Parameter(Mandatory = $true)][string]$HelperPath,
        [Parameter(Mandatory = $true)][string]$RunId,
        [int]$TimeoutMs = 15000,
        [string[]]$ExtraArguments = @()
    )

    if ($TimeoutMs -lt 1) {
        throw "Windows speech helper self-test timeout must be positive."
    }
    if (-not (Test-Path -LiteralPath $HelperPath -PathType Leaf)) {
        throw "Windows speech helper not found: $HelperPath"
    }

    $temporaryRoot = Join-Path ([IO.Path]::GetTempPath()) `
        ("vocekit-speech-selftest-" + [Guid]::NewGuid().ToString("N"))
    $stdoutPath = Join-Path $temporaryRoot "stdout.txt"
    $stderrPath = Join-Path $temporaryRoot "stderr.txt"
    $process = $null
    $phase = "start"
    try {
        [void][IO.Directory]::CreateDirectory($temporaryRoot)
        $arguments = @($ExtraArguments) + @("--self-test", "--run-id", $RunId)
        $process = Start-Process -FilePath $HelperPath `
            -ArgumentList $arguments `
            -NoNewWindow -PassThru `
            -RedirectStandardOutput $stdoutPath `
            -RedirectStandardError $stderrPath
        # Force the native process handle to be opened so ExitCode remains available
        # after the redirected process exits on Windows PowerShell 5.1.
        [void]$process.Handle

        $phase = "wait"
        if (-not $process.WaitForExit($TimeoutMs)) {
            try { Stop-Process -Id $process.Id -Force -ErrorAction Stop } catch { }
            try { $process.WaitForExit() } catch { }
            throw "Windows speech helper self-test timed out after $TimeoutMs ms."
        }
        # Flush redirected streams before reading their files.
        $process.WaitForExit()
        $process.Refresh()
        $phase = "read"
        $exitCode = $process.ExitCode
        [string[]]$stdoutLines = @(
            Get-Content -LiteralPath $stdoutPath -Encoding UTF8 |
                Where-Object { -not [string]::IsNullOrWhiteSpace($_) }
        )
        $stderrText = if (Test-Path -LiteralPath $stderrPath) {
            $rawStderr = [string](Get-Content -LiteralPath $stderrPath -Raw -Encoding UTF8)
            if ($null -eq $rawStderr) { "" } else { $rawStderr.Trim() }
        } else { "" }

        if ($exitCode -ne 0) {
            throw "Windows speech helper self-test exited with code $exitCode. stderr: $stderrText"
        }
        if ($stdoutLines.Count -ne 1) {
            throw "Windows speech helper self-test must emit exactly one non-empty stdout JSON line; received $($stdoutLines.Count)."
        }

        $phase = "json"
        try { $event = $stdoutLines[0] | ConvertFrom-Json -ErrorAction Stop } catch {
            throw "Windows speech helper self-test emitted invalid stdout JSON: $($_.Exception.Message)"
        }
        foreach ($propertyName in @("protocolVersion", "runId", "type", "ok")) {
            if ($null -eq $event.PSObject.Properties[$propertyName]) {
                throw "Windows speech helper self-test JSON is missing '$propertyName'."
            }
        }
        if ([int]$event.protocolVersion -ne 1 -or
            [string]$event.runId -cne $RunId -or
            [string]$event.type -cne "self-test" -or
            -not ($event.ok -is [bool]) -or
            $event.ok -ne $true) {
            throw "Windows speech helper self-test returned an invalid protocol event."
        }
        return $event
    }
    catch {
        throw "Windows speech helper self-test failed during $phase`: $($_.Exception.Message)"
    }
    finally {
        $cleanupError = $null
        if ($null -ne $process) {
            try {
                if (-not $process.HasExited) {
                    Stop-Process -Id $process.Id -Force -ErrorAction SilentlyContinue
                    $process.WaitForExit()
                }
            } catch { }
            try { $process.Dispose() } catch { }
        }
        if ([IO.Directory]::Exists($temporaryRoot)) {
            try { [IO.Directory]::Delete($temporaryRoot, $true) } catch { }
        }
    }
}

if ($SpeechHelperSelfTestOnly) {
    if ([string]::IsNullOrWhiteSpace($SpeechHelperSelfTestPath)) {
        throw "SpeechHelperSelfTestPath is required with SpeechHelperSelfTestOnly."
    }
    $effectiveExtraArguments = @($SpeechHelperSelfTestExtraArguments)
    if (-not [string]::IsNullOrWhiteSpace($SpeechHelperSelfTestScenario)) {
        $effectiveExtraArguments += @("--scenario", $SpeechHelperSelfTestScenario)
    }
    [void](Invoke-SpeechHelperSelfTest `
        -HelperPath ([IO.Path]::GetFullPath($SpeechHelperSelfTestPath)) `
        -RunId "runtime-verify" `
        -TimeoutMs $SpeechHelperSelfTestTimeoutMs `
        -ExtraArguments $effectiveExtraArguments)
    Write-Host "Windows speech helper self-test passed."
    exit 0
}

$projectRoot = [IO.Path]::GetFullPath((Split-Path -Parent $PSScriptRoot))
if ([string]::IsNullOrWhiteSpace($RuntimeDir)) {
    $RuntimeDir = Join-Path $projectRoot ".qt6-deploy"
}
$RuntimeDir = [IO.Path]::GetFullPath($RuntimeDir)
if (-not (Test-Path -LiteralPath $RuntimeDir -PathType Container)) {
    throw "Runtime directory not found: $RuntimeDir"
}

function Get-PeMachine {
    param([string]$Path)
    $stream = [IO.File]::Open($Path, [IO.FileMode]::Open, [IO.FileAccess]::Read, [IO.FileShare]::ReadWrite)
    try {
        $reader = New-Object IO.BinaryReader($stream)
        if ($reader.ReadUInt16() -ne 0x5A4D) { throw "Not a valid PE file: $Path" }
        $stream.Position = 0x3C
        $peOffset = $reader.ReadInt32()
        $stream.Position = $peOffset
        if ($reader.ReadUInt32() -ne 0x00004550) { throw "Invalid PE header: $Path" }
        return $reader.ReadUInt16()
    } finally { $stream.Dispose() }
}

$requiredFiles = @(
    "vocekit.exe", "Qt6Core.dll", "Qt6Gui.dll", "Qt6Widgets.dll",
    "Qt6Network.dll", "Qt6Multimedia.dll", "Qt6WebSockets.dll",
    "libgcc_s_seh-1.dll", "libstdc++-6.dll", "libwinpthread-1.dll",
    "platforms\qwindows.dll", "multimedia\windowsmediaplugin.dll",
    "tls\qschannelbackend.dll", "translations\qt_zh_CN.qm",
    "speech\windows\vocekit-windows-speech.exe",
    "ocr\windows\vocekit-windows-ocr.exe",
    "ocr\rapidocr\vocekit-rapidocr.exe",
    "ocr\rapidocr\LICENSE-RapidOcrOnnx.txt",
    "ocr\rapidocr\models\ch_PP-OCRv3_det_infer.onnx",
    "ocr\rapidocr\models\ch_PP-OCRv3_rec_infer.onnx",
    "ocr\rapidocr\models\ch_ppocr_mobile_v2.0_cls_infer.onnx",
    "ocr\rapidocr\models\ppocr_keys_v1.txt",
    ".vocekit-portable",
    "updater\vocekit-update.ps1",
    "updater\update-policy.json"
)
$missingFiles = New-Object Collections.Generic.List[string]
foreach ($relativePath in $requiredFiles) {
    $path = Join-Path $RuntimeDir $relativePath
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
        $missingFiles.Add($relativePath)
    } elseif ((Get-Item -LiteralPath $path).Length -le 0) {
        $missingFiles.Add("$relativePath (empty file)")
    }
}
if ($missingFiles.Count -gt 0) {
    $details = ($missingFiles | ForEach-Object { "  - $_" }) -join [Environment]::NewLine
    throw "Runtime verification failed. Missing files:$([Environment]::NewLine)$details"
}

# Only DLLs loaded directly by the Qt application must match its machine type.
# OCR and speech helpers are child processes and do not share the Qt application ABI.
$mainExecutable = Join-Path $RuntimeDir "vocekit.exe"
$expectedMachine = Get-PeMachine -Path $mainExecutable
$loadedDlls = @(
    "Qt6Core.dll", "Qt6Gui.dll", "Qt6Widgets.dll", "Qt6Network.dll",
    "Qt6Multimedia.dll", "Qt6WebSockets.dll", "libgcc_s_seh-1.dll",
    "libstdc++-6.dll", "libwinpthread-1.dll",
    "platforms\qwindows.dll", "multimedia\windowsmediaplugin.dll",
    "tls\qschannelbackend.dll"
)
$wrongArchitecture = New-Object Collections.Generic.List[string]
foreach ($relativePath in $loadedDlls) {
    if ((Get-PeMachine -Path (Join-Path $RuntimeDir $relativePath)) -ne $expectedMachine) {
        $wrongArchitecture.Add($relativePath)
    }
}
if ($wrongArchitecture.Count -gt 0) {
    $details = ($wrongArchitecture | ForEach-Object { "  - $_" }) -join [Environment]::NewLine
    throw "Runtime verification failed. Architecture mismatch:$([Environment]::NewLine)$details"
}

$speechHelper = Join-Path $RuntimeDir "speech\windows\vocekit-windows-speech.exe"
[void](Invoke-SpeechHelperSelfTest -HelperPath $speechHelper -RunId "runtime-verify")
$architecture = switch ($expectedMachine) {
    0x014C { "x86" }
    0x8664 { "x64" }
    default { "0x{0:X4}" -f $expectedMachine }
}
Write-Host "Runtime verification passed: $RuntimeDir"
Write-Host "Application architecture: $architecture"
Write-Host "Checked files: $($requiredFiles.Count)"
