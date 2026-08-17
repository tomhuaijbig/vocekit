param(
    [ValidateSet("Release")]
    [string]$Configuration = "Release"
)

$ErrorActionPreference = "Stop"

& (Join-Path $PSScriptRoot "build-ocr-helpers.ps1") -Configuration $Configuration
& (Join-Path $PSScriptRoot "build-windows-speech-helper.ps1") -Configuration $Configuration

Write-Host "Runtime helper builds completed."
