param(
    [Parameter(Mandatory = $true)]
    [string]$UpdateFeedUrl,
    [Parameter(Mandatory = $true)]
    [string]$ReleaseBaseUrl,
    [Parameter(Mandatory = $true)]
    [string]$ReleasePageBaseUrl,
    [Parameter(Mandatory = $true)]
    [string]$ExpectedSignerSubject,
    [string]$ExpectedSignerThumbprint = "",
    [string]$ExpectedTag = "",
    [string]$PackageName = "vocekit-qt6-portable"
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$releaseGate = Join-Path $PSScriptRoot "verify-release-readiness.ps1"
$packageScript = Join-Path $PSScriptRoot "package-test.ps1"

& $releaseGate `
    -UpdateFeedUrl $UpdateFeedUrl `
    -ExpectedTag $ExpectedTag `
    -ExpectedSignerSubject $ExpectedSignerSubject `
    -ExpectedSignerThumbprint $ExpectedSignerThumbprint

& $packageScript `
    -PackageName $PackageName `
    -RequireSignedBinaries `
    -ReleaseBaseUrl $ReleaseBaseUrl `
    -ReleasePageBaseUrl $ReleasePageBaseUrl

Write-Host "Signed public release package created: $PackageName"
