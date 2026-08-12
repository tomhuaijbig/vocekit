param(
    [string]$HelperPath
)

$ErrorActionPreference = "Stop"
$projectRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
if ([string]::IsNullOrWhiteSpace($HelperPath)) {
    $HelperPath = Join-Path $projectRoot "helpers\bin\vocekit-windows-speech.exe"
}
$HelperPath = [IO.Path]::GetFullPath($HelperPath)
if (-not (Test-Path -LiteralPath $HelperPath -PathType Leaf)) {
    throw "Built Windows speech helper is required: $HelperPath"
}

Add-Type -AssemblyName System.Speech
$temporaryRoot = Join-Path ([IO.Path]::GetTempPath()) ("vocekit-windows-speech-system-smoke-" + [Guid]::NewGuid().ToString("N"))
[IO.Directory]::CreateDirectory($temporaryRoot) | Out-Null
$startedProcessIds = New-Object System.Collections.Generic.List[int]

function New-RawPcmSample {
    param(
        [string]$VoiceName,
        [string]$Text,
        [string]$OutputPath
    )

    $synthesizer = New-Object System.Speech.Synthesis.SpeechSynthesizer
    $memory = New-Object IO.MemoryStream
    try {
        $voice = @($synthesizer.GetInstalledVoices() | Where-Object {
            $_.Enabled -and $_.VoiceInfo.Name -eq $VoiceName
        }) | Select-Object -First 1
        if ($null -eq $voice) {
            throw "Required System.Speech synthesis voice is not installed: $VoiceName"
        }
        $format = New-Object System.Speech.AudioFormat.SpeechAudioFormatInfo(
            16000,
            [System.Speech.AudioFormat.AudioBitsPerSample]::Sixteen,
            [System.Speech.AudioFormat.AudioChannel]::Mono)
        $synthesizer.SelectVoice($VoiceName)
        $synthesizer.SetOutputToAudioStream($memory, $format)
        $synthesizer.Speak($Text)
        $bytes = $memory.ToArray()
        if ($bytes.Length -eq 0) {
            throw "System.Speech synthesized no PCM bytes for voice: $VoiceName"
        }
        [IO.File]::WriteAllBytes($OutputPath, $bytes)
        return $bytes
    } finally {
        $synthesizer.Dispose()
        $memory.Dispose()
    }
}

function Invoke-HelperWithChunkedPcm {
    param(
        [byte[]]$Pcm,
        [string]$Language,
        [string]$RunId
    )

    $startInfo = New-Object Diagnostics.ProcessStartInfo
    $startInfo.FileName = $HelperPath
    $startInfo.Arguments = "--mode stream --run-id $RunId --language $Language"
    $startInfo.UseShellExecute = $false
    $startInfo.RedirectStandardInput = $true
    $startInfo.RedirectStandardOutput = $true
    $startInfo.RedirectStandardError = $true
    $startInfo.CreateNoWindow = $true
    $process = New-Object Diagnostics.Process
    $process.StartInfo = $startInfo
    try {
        [void]$process.Start()
        $startedProcessIds.Add($process.Id)
        $stdoutTask = $process.StandardOutput.ReadToEndAsync()
        $stderrTask = $process.StandardError.ReadToEndAsync()
        $deadline = [Diagnostics.Stopwatch]::StartNew()
        for ($offset = 0; $offset -lt $Pcm.Length; $offset += 4096) {
            $count = [Math]::Min(4096, $Pcm.Length - $offset)
            $remaining = 30000 - [int]$deadline.ElapsedMilliseconds
            if ($remaining -le 0) {
                throw "Windows speech helper PCM write timed out for $Language."
            }
            $writeTask = $process.StandardInput.BaseStream.WriteAsync($Pcm, $offset, $count)
            if (-not $writeTask.Wait($remaining)) {
                throw "Windows speech helper PCM write timed out for $Language."
            }
        }
        $process.StandardInput.Close()
        $remaining = 30000 - [int]$deadline.ElapsedMilliseconds
        if ($remaining -le 0 -or -not $process.WaitForExit($remaining)) {
            throw "Windows speech helper timed out for $Language."
        }

        $stdout = $stdoutTask.Result.Trim()
        $stderr = $stderrTask.Result.Trim()
        $lines = @($stdout -split "`r?`n" | Where-Object { -not [string]::IsNullOrWhiteSpace($_) })
        if ($lines.Count -lt 2) {
            throw "Windows speech helper emitted too few protocol events for $Language."
        }
        $events = @($lines | ForEach-Object { $_ | ConvertFrom-Json -ErrorAction Stop })
        foreach ($event in $events) {
            if ($event.protocolVersion -ne 1 -or $event.runId -ne $RunId) {
                throw "Windows speech helper emitted a mismatched protocol envelope for $Language."
            }
        }
        if (@($events | Where-Object { $_.type -eq "hypothesis" -or $_.type -eq "recognized" }).Count -eq 0) {
            throw "Windows speech helper emitted no hypothesis or recognized event for $Language."
        }
        $terminal = $events[-1]
        if ($terminal.type -ne "final" -and -not ($terminal.type -eq "error" -and $terminal.errorCode -eq "NO_SPEECH")) {
            throw "Windows speech helper emitted an invalid terminal event for $Language."
        }
        if ($terminal.inputStreamEnded -ne $true) {
            throw "Windows speech helper did not report inputStreamEnded for $Language."
        }
        if ($terminal.type -eq "final" -and $process.ExitCode -ne 0) {
            throw "Windows speech helper final event had exit code $($process.ExitCode) for $Language."
        }
        if ($terminal.type -eq "error" -and $process.ExitCode -ne 9) {
            throw "Windows speech helper NO_SPEECH event had exit code $($process.ExitCode) for $Language."
        }
        if (-not [string]::IsNullOrWhiteSpace($stderr)) {
            throw "Windows speech helper wrote unexpected diagnostics for successful PCM smoke: $Language"
        }

        Write-Host ("System smoke {0}: bytes={1} events={2} terminal={3}" -f $Language, $Pcm.Length, $events.Count, $terminal.type)
    } finally {
        if (-not $process.HasExited) {
            $process.Kill()
            [void]$process.WaitForExit(5000)
        }
        $process.Dispose()
    }
}

try {
    $synthesizer = New-Object System.Speech.Synthesis.SpeechSynthesizer
    try {
        $voices = @($synthesizer.GetInstalledVoices() | Where-Object { $_.Enabled } | ForEach-Object { $_.VoiceInfo })
    } finally {
        $synthesizer.Dispose()
    }
    $zhVoice = @($voices | Where-Object { $_.Name -eq "Microsoft Huihui Desktop" -and $_.Culture.Name -eq "zh-CN" }) | Select-Object -First 1
    if ($null -eq $zhVoice) {
        throw "Required zh-CN voice is not installed: Microsoft Huihui Desktop"
    }
    $enVoice = @($voices | Where-Object {
        $_.Culture.Name -eq "en-US" -and ($_.Name -eq "Microsoft Zira Desktop" -or $_.Name -eq "Microsoft David Desktop")
    } | Sort-Object @{ Expression = { if ($_.Name -eq "Microsoft Zira Desktop") { 0 } else { 1 } } }) | Select-Object -First 1
    if ($null -eq $enVoice) {
        throw "Required en-US voice is not installed: Microsoft Zira Desktop or Microsoft David Desktop"
    }

    $zhText = -join ([char[]]@(0x4F60, 0x597D, 0xFF0C, 0x8FD9, 0x662F, 0x4E00, 0x4E2A, 0x672C, 0x5730, 0x8BED, 0x97F3, 0x8BC6, 0x522B, 0x6D4B, 0x8BD5, 0x3002, 0x4ECA, 0x5929, 0x5929, 0x6C14, 0x5F88, 0x597D, 0x3002))
    $zhPcm = New-RawPcmSample -VoiceName $zhVoice.Name -Text $zhText -OutputPath (Join-Path $temporaryRoot "zh-CN.raw")
    $enPcm = New-RawPcmSample -VoiceName $enVoice.Name -Text "Hello, this is a local speech recognition test. The weather is good today." -OutputPath (Join-Path $temporaryRoot "en-US.raw")
    Invoke-HelperWithChunkedPcm -Pcm $zhPcm -Language "zh-CN" -RunId "system-smoke-zh"
    Invoke-HelperWithChunkedPcm -Pcm $enPcm -Language "en-US" -RunId "system-smoke-en"
} finally {
    if ([IO.Directory]::Exists($temporaryRoot)) {
        [IO.Directory]::Delete($temporaryRoot, $true)
    }
    foreach ($processId in $startedProcessIds) {
        if ($null -ne (Get-Process -Id $processId -ErrorAction SilentlyContinue)) {
            throw "Windows speech helper process remained after System.Speech smoke: $processId"
        }
    }
}

Write-Host "Windows speech helper System.Speech PCM smoke: PASS"
