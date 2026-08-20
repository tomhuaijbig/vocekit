[CmdletBinding(DefaultParameterSetName = "Verify")]
param(
    [Parameter(Mandatory = $true, ParameterSetName = "Verify")]
    [string]$ExecutablePath,
    [Parameter(Mandatory = $true, ParameterSetName = "Verify")]
    [ValidateSet(
        "vocekit-windows-ocr",
        "vocekit-rapidocr",
        "vocekit-windows-speech"
    )]
    [string]$ExpectedHelperName,
    [Parameter(Mandatory = $true, ParameterSetName = "Verify")]
    [string]$ExpectedSourceCommit,
    [Parameter(Mandatory = $true, ParameterSetName = "Decision")]
    [switch]$DecisionTestMode,
    [ValidateRange(1000, 60000)]
    [int]$TimeoutMs = 10000
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"
. (Join-Path $PSScriptRoot "runtime-helper-provenance.ps1")

function Assert-FormalRuntimeHelperBuildProvenanceObject {
    param(
        [Parameter(Mandatory = $true)]$Provenance,
        [Parameter(Mandatory = $true)]
        [ValidateSet(
            "vocekit-windows-ocr",
            "vocekit-rapidocr",
            "vocekit-windows-speech"
        )]
        [string]$ExpectedHelperName,
        [Parameter(Mandatory = $true)][string]$ExpectedSourceCommit
    )

    # Formal candidates and deployed packages must never accept a helper that
    # was compiled from a dirty tree. This is deliberately not configurable.
    Assert-RuntimeHelperBuildProvenanceObject `
        -Provenance $Provenance `
        -ExpectedHelperName $ExpectedHelperName `
        -ExpectedSourceCommit $ExpectedSourceCommit `
        -ExpectedSourceTreeClean $true `
        -ExpectedConfiguration "Release"
}

if ($DecisionTestMode) {
    return
}

$provenance = Get-RuntimeHelperExecutableProvenance `
    -ExecutablePath $ExecutablePath `
    -ExpectedHelperName $ExpectedHelperName `
    -ExpectedSourceCommit $ExpectedSourceCommit `
    -ExpectedSourceTreeClean $true `
    -ExpectedConfiguration "Release" `
    -TimeoutMs $TimeoutMs
Write-Output $provenance
