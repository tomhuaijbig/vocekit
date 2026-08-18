param(
    [string]$QtBin = "D:\QT66666\6.11.1\mingw_64\bin",
    [string]$MingwBin = "D:\QT66666\Tools\mingw1310_64\bin",
    [ValidateSet("debug", "release")]
    [string]$Configuration = "debug",
    [ValidateRange(1, 64)]
    [int]$Jobs = 2,
    [switch]$Run
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$projectRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$projectFile = Join-Path $projectRoot "vocekit.pro"
$buildRoot = Join-Path $projectRoot ".qt6-build"
$qmake = Join-Path $QtBin "qmake.exe"
$make = Join-Path $MingwBin "mingw32-make.exe"

foreach ($tool in @($qmake, $make)) {
    if (-not (Test-Path -LiteralPath $tool -PathType Leaf)) {
        throw "Missing Qt 6 build tool: $tool"
    }
}

New-Item -ItemType Directory -Path $buildRoot -Force | Out-Null

$originalPath = $env:PATH
try {
    $env:PATH = "$QtBin;$MingwBin;$env:PATH"
    $qtVersion = (& $qmake -query QT_VERSION).Trim()
    if (-not $qtVersion.StartsWith("6.")) {
        throw "VoceKit requires Qt 6; qmake reports Qt $qtVersion"
    }

    Push-Location $buildRoot
    try {
        & $qmake $projectFile -spec win32-g++ "CONFIG+=$Configuration"
        if ($LASTEXITCODE -ne 0) {
            throw "Qt 6 qmake failed with exit code $LASTEXITCODE"
        }

        & $make "-j$Jobs"
        if ($LASTEXITCODE -ne 0) {
            throw "Qt 6 build failed with exit code $LASTEXITCODE"
        }
    } finally {
        Pop-Location
    }

    $executable = Join-Path (Join-Path $buildRoot $Configuration) "vocekit.exe"
    if (-not (Test-Path -LiteralPath $executable -PathType Leaf)) {
        throw "Build completed but executable was not found: $executable"
    }

    Write-Host "VoceKit Qt $qtVersion $Configuration build succeeded: $executable"
    if ($Configuration -eq "release") {
        Write-Host "This is a non-portable build output. Run scripts\deploy.ps1 before launching it outside the Qt development environment."
    }
    if ($Run) {
        & $executable
    }
} finally {
    $env:PATH = $originalPath
}
