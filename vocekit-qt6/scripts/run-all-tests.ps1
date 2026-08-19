param(
    [string]$QtBin = "D:\QT66666\6.11.1\mingw_64\bin",
    [string]$MingwBin = "D:\QT66666\Tools\mingw1310_64\bin",
    [string]$OpenSslBin = "",
    [ValidateSet("debug", "release")]
    [string]$Configuration = "debug",
    [string]$ProjectName = ""
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
    $buildPath = @($QtBin, $MingwBin)
    if (-not [string]::IsNullOrWhiteSpace($OpenSslBin)) {
        $buildPath += $OpenSslBin
    }
    $env:PATH = (($buildPath -join ";") + ";" + $env:PATH)
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
    $discoveredProjects = @(
        Get-ChildItem -LiteralPath $testsRoot -Recurse -File |
            Where-Object { $_.Extension -eq ".pro" } |
            Sort-Object FullName
    )
    $projects = $discoveredProjects
    if ($PSBoundParameters.ContainsKey("ProjectName")) {
        $requestedProject = $ProjectName.Trim()
        if ([string]::IsNullOrWhiteSpace($requestedProject)) {
            throw "ProjectName must be a non-empty .pro basename."
        }
        $projects = @(
            $discoveredProjects |
                Where-Object { $_.BaseName -eq $requestedProject }
        )
        if ($projects.Count -eq 0) {
            throw "Test project was not found: $requestedProject"
        }
        if ($projects.Count -gt 1) {
            throw "Test project name is ambiguous: $requestedProject"
        }
    }

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
        $targetWrapperPath = Join-Path (
            $project.DirectoryName
        ) "target_wrapper.sh"
        $targetWrapperExisted = Test-Path -LiteralPath $targetWrapperPath

        Push-Location $project.DirectoryName
        try {
            $qmakeResult = Invoke-CapturedCommand $qmake @(
                "-o",
                $makefileName,
                $project.Name,
                "-spec",
                "win32-g++",
                "CONFIG+=$Configuration",
                "OBJECTS_DIR=$Configuration/.codex/$target/obj",
                "MOC_DIR=$Configuration/.codex/$target/moc",
                "RCC_DIR=$Configuration/.codex/$target/rcc",
                "UI_DIR=$Configuration/.codex/$target/ui"
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
            if (!$targetWrapperExisted -and
                (Test-Path -LiteralPath $targetWrapperPath -PathType Leaf)) {
                $generatedFiles.Add($targetWrapperPath)
            }

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

            $projectQpaPlatform = $env:QT_QPA_PLATFORM
            $projectHadQpaFontDir = Test-Path Env:QT_QPA_FONTDIR
            $projectQpaFontDir = if ($projectHadQpaFontDir) {
                $env:QT_QPA_FONTDIR
            } else {
                $null
            }
            $testOutputPath = Join-Path (
                [IO.Path]::GetTempPath()
            ) ("vocekit-qt6-test-" + [Guid]::NewGuid().ToString("N") + ".txt")
            try {
                # Native HWND and Windows visual-metric tests cannot use the
                # synthetic handles or font metrics of the offscreen plugin.
                if ($target -in @(
                    "selection_observer_tests",
                    "selection_context_action_editor_tests",
                    "selection_context_settings_card_tests",
                    "selection_result_card_tests",
                    "windows_speech_settings_card_tests"
                )) {
                    $env:QT_QPA_PLATFORM = "windows"
                    # Qt 6's Windows plugin discovers installed fonts itself;
                    # the Qt 5-era QPA_FONTDIR override produces empty font
                    # families and invalid visual-metric results here.
                    Remove-Item Env:QT_QPA_FONTDIR -ErrorAction SilentlyContinue
                }
                $runResult = Invoke-CapturedCommand $executable @(
                    "-maxwarnings",
                    "0",
                    "-o",
                    "$testOutputPath,txt"
                )
            } finally {
                $env:QT_QPA_PLATFORM = $projectQpaPlatform
                if ($projectHadQpaFontDir) {
                    $env:QT_QPA_FONTDIR = $projectQpaFontDir
                } else {
                    Remove-Item Env:QT_QPA_FONTDIR -ErrorAction SilentlyContinue
                }
            }
            $runOutput = @($runResult.Output)
            if (Test-Path -LiteralPath $testOutputPath -PathType Leaf) {
                $runOutput += @(Get-Content -LiteralPath $testOutputPath)
                Remove-Item -LiteralPath $testOutputPath -Force
            }
            $runText = $runOutput -join "`n"
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
    DiscoveredProjects = $discoveredProjects.Count
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
        Write-Host $_ -ForegroundColor Red
    }
    exit 1
}
