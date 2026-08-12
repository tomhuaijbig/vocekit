param(
    [string]$QtBin = "D:\QQQQQT0001\5.9\mingw53_32\bin",
    [string]$MingwBin = "D:\QQQQQT0001\Tools\mingw530_32\bin",
    [string]$OpenSslBin = "D:\QQQQQT0001\Tools\mingw530_32\opt\bin",
    [ValidateSet("debug", "release")]
    [string]$Configuration = "debug"
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$projectRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$testsRoot = Join-Path $projectRoot "tests"
$qmake = Join-Path $QtBin "qmake.exe"
$make = Join-Path $MingwBin "mingw32-make.exe"

foreach ($tool in @($qmake, $make)) {
    if (-not (Test-Path -LiteralPath $tool)) {
        throw "Missing build tool: $tool"
    }
}

$hadPath = Test-Path Env:PATH
$hadQpaPlatform = Test-Path Env:QT_QPA_PLATFORM
$hadQpaFontDir = Test-Path Env:QT_QPA_FONTDIR
$originalPath = $env:PATH
$originalQpaPlatform = $env:QT_QPA_PLATFORM
$originalQpaFontDir = $env:QT_QPA_FONTDIR
$temporaryFontDir = $null
$generatedFiles = New-Object System.Collections.Generic.List[string]

try {
    $env:PATH = "$QtBin;$MingwBin;$OpenSslBin;$env:PATH"
    $env:QT_QPA_PLATFORM = "offscreen"
    if ([string]::IsNullOrWhiteSpace($env:QT_QPA_FONTDIR)) {
        $windowsFonts = Join-Path $env:WINDIR "Fonts"
        $fontNames = @(
            "msyh.ttc",
            "msyhbd.ttc",
            "simhei.ttf",
            "simsun.ttc",
            "Deng.ttf",
            "arial.ttf"
        )
        $testFont = $fontNames |
            ForEach-Object { Join-Path $windowsFonts $_ } |
            Where-Object { Test-Path -LiteralPath $_ -PathType Leaf } |
            Select-Object -First 1
        if (-not $testFont) {
            throw "No stable Windows test font was found in $windowsFonts"
        }
        $temporaryFontDir = Join-Path (
            [IO.Path]::GetTempPath()
        ) ("vocekit-qt-fonts-" + [Guid]::NewGuid().ToString("N"))
        New-Item -ItemType Directory -Path $temporaryFontDir | Out-Null
        Copy-Item -LiteralPath $testFont -Destination $temporaryFontDir
        $env:QT_QPA_FONTDIR = $temporaryFontDir
    }

    # Windows -Filter *.pro can also match generated Makefiles.
    $projects = Get-ChildItem -LiteralPath $testsRoot -Recurse -File |
        Where-Object { $_.Extension -eq ".pro" } |
        Sort-Object FullName

    $qtPrograms = 0
    $standalonePrograms = 0
    $passed = 0
    $failed = 0
    $skipped = 0
    $failures = New-Object System.Collections.Generic.List[string]

function Invoke-CapturedCommand {
    param(
        [string]$Program,
        [string[]]$Arguments
    )

    $output = & $Program @Arguments 2>&1
    [PSCustomObject]@{
        ExitCode = $LASTEXITCODE
        Output = @($output | ForEach-Object { $_.ToString() })
    }
}

    for ($index = 0; $index -lt $projects.Count; ++$index) {
        $project = $projects[$index]
        $targetMatch = Select-String `
            -LiteralPath $project.FullName `
            -Pattern "^\s*TARGET\s*=\s*(.+?)\s*$" |
            Select-Object -First 1
        if (-not $targetMatch) {
            $failures.Add("$($project.FullName): missing TARGET")
            continue
        }

        $target = $targetMatch.Matches[0].Groups[1].Value.Trim()
        $makefileName = "Makefile.codex.$target"
        $makefilePath = Join-Path $project.DirectoryName $makefileName

        Push-Location $project.DirectoryName
        try {
            $qmakeResult = Invoke-CapturedCommand $qmake @(
                "-o",
                $makefileName,
                $project.Name,
                "-spec",
                "win32-g++",
                "CONFIG+=$Configuration"
            )
            if ($qmakeResult.ExitCode -ne 0) {
                $failures.Add(
                    "$target`: qmake failed`n$($qmakeResult.Output -join "`n")"
                )
                continue
            }

            Get-ChildItem -LiteralPath $project.DirectoryName -File |
                Where-Object {
                    $_.Name -like "$makefileName*" -or
                    $_.Name -like "object_script.$target.*"
                } |
                ForEach-Object { $generatedFiles.Add($_.FullName) }

            $makeResult = Invoke-CapturedCommand $make @(
                "-f",
                $makefileName,
                "-j2"
            )
            if ($makeResult.ExitCode -ne 0) {
                $failures.Add(
                    "$target`: build failed`n$($makeResult.Output -join "`n")"
                )
                continue
            }

            $executable = Join-Path (
                Join-Path $project.DirectoryName $Configuration
            ) "$target.exe"
            if (-not (Test-Path -LiteralPath $executable)) {
                $executable = Join-Path $project.DirectoryName "$target.exe"
            }
            if (-not (Test-Path -LiteralPath $executable)) {
                $failures.Add("$target`: executable not found after build")
                continue
            }

            if ($target -in @("fake_ocr_helper", "fake_windows_speech_helper")) {
                ++$standalonePrograms
                continue
            }

            if ($target -eq "ssl_runtime_smoke") {
                $runResult = Invoke-CapturedCommand $executable @()
                if ($runResult.ExitCode -ne 0) {
                    $failures.Add(
                        "$target`: execution failed`n$($runResult.Output -join "`n")"
                    )
                } else {
                    ++$standalonePrograms
                }
                continue
            }

            $runResult = Invoke-CapturedCommand $executable @("-maxwarnings", "0")
            $runText = $runResult.Output -join "`n"
            $summary = [regex]::Match(
                $runText,
                "Totals:\s+(\d+)\s+passed,\s+(\d+)\s+failed,\s+(\d+)\s+skipped"
            )
            if ($runResult.ExitCode -ne 0 -or -not $summary.Success) {
                $failures.Add("$target`: test failed`n$runText")
                continue
            }

            ++$qtPrograms
            $passed += [int]$summary.Groups[1].Value
            $failed += [int]$summary.Groups[2].Value
            $skipped += [int]$summary.Groups[3].Value
        } finally {
            Pop-Location
        }

        if ((($index + 1) % 20) -eq 0) {
            Write-Host "Verified $($index + 1)/$($projects.Count) test projects"
        }
    }
} finally {
    try {
        try {
            $generatedFiles |
                Sort-Object -Unique |
                Where-Object {
                    $_.StartsWith(
                        $testsRoot + [IO.Path]::DirectorySeparatorChar,
                        [StringComparison]::OrdinalIgnoreCase
                    )
                } |
                ForEach-Object {
                    if (Test-Path -LiteralPath $_) {
                        Remove-Item -LiteralPath $_ -Force
                    }
                }
        } finally {
            if ($temporaryFontDir -and
                (Test-Path -LiteralPath $temporaryFontDir -PathType Container)) {
                Get-ChildItem -LiteralPath $temporaryFontDir -File |
                    Remove-Item -Force
                Remove-Item -LiteralPath $temporaryFontDir -Force
            }
        }
    } finally {
        if ($hadPath) {
            $env:PATH = $originalPath
        } else {
            Remove-Item Env:PATH -ErrorAction SilentlyContinue
        }
        if ($hadQpaPlatform) {
            $env:QT_QPA_PLATFORM = $originalQpaPlatform
        } else {
            Remove-Item Env:QT_QPA_PLATFORM -ErrorAction SilentlyContinue
        }
        if ($hadQpaFontDir) {
            $env:QT_QPA_FONTDIR = $originalQpaFontDir
        } else {
            Remove-Item Env:QT_QPA_FONTDIR -ErrorAction SilentlyContinue
        }
    }
}

[PSCustomObject]@{
    Projects = $projects.Count
    QtPrograms = $qtPrograms
    StandalonePrograms = $standalonePrograms
    Passed = $passed
    Failed = $failed
    Skipped = $skipped
    InfrastructureFailures = $failures.Count
} | Format-List

if ($failures.Count -gt 0) {
    $failures | ForEach-Object {
        Write-Error $_
    }
    exit 1
}
