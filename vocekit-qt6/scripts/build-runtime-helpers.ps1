param(
    [ValidateSet("Release")]
    [string]$Configuration = "Release"
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"
$projectRoot = Split-Path -Parent $PSScriptRoot
$repositoryRoot = [IO.Path]::GetFullPath((Join-Path $projectRoot ".."))
. (Join-Path $PSScriptRoot "runtime-helper-provenance.ps1")

$buildState = Get-RuntimeHelperRepositoryState `
    -RepositoryRoot $repositoryRoot `
    -ProjectRoot $projectRoot
$helperNames = @(
    "vocekit-windows-ocr",
    "vocekit-rapidocr",
    "vocekit-windows-speech"
)
Remove-RuntimeHelperBuildOutputs `
    -ProjectRoot $projectRoot `
    -HelperNames $helperNames

$buildCompleted = $false
try {
    & (Join-Path $PSScriptRoot "build-ocr-helpers.ps1") `
        -Configuration $Configuration `
        -ExpectedSourceCommit ([string]$buildState.source_commit) `
        -ExpectedSourceTreeClean ([bool]$buildState.source_tree_clean)
    [void](Assert-RuntimeHelperRepositoryStateUnchanged `
        -Before $buildState `
        -RepositoryRoot $repositoryRoot `
        -ProjectRoot $projectRoot)

    & (Join-Path $PSScriptRoot "build-windows-speech-helper.ps1") `
        -Configuration $Configuration `
        -ExpectedSourceCommit ([string]$buildState.source_commit) `
        -ExpectedSourceTreeClean ([bool]$buildState.source_tree_clean)
    [void](Assert-RuntimeHelperRepositoryStateUnchanged `
        -Before $buildState `
        -RepositoryRoot $repositoryRoot `
        -ProjectRoot $projectRoot)

    foreach ($helperName in $helperNames) {
        $helperPath = Join-Path $projectRoot ("helpers\bin\" + $helperName + ".exe")
        [void](Get-RuntimeHelperExecutableProvenance `
            -ExecutablePath $helperPath `
            -ExpectedHelperName $helperName `
            -ExpectedSourceCommit ([string]$buildState.source_commit) `
            -ExpectedSourceTreeClean ([bool]$buildState.source_tree_clean))
    }
    [void](Assert-RuntimeHelperRepositoryStateUnchanged `
        -Before $buildState `
        -RepositoryRoot $repositoryRoot `
        -ProjectRoot $projectRoot)

    $buildCompleted = $true
    Write-Host "Runtime helper builds completed."
    Write-Host "Runtime helper source commit: $($buildState.source_commit)"
    Write-Host "Runtime helper source tree clean: $($buildState.source_tree_clean)"
    Write-Host "Runtime helper configuration: $Configuration"
} finally {
    if (-not $buildCompleted) {
        Remove-RuntimeHelperBuildOutputs `
            -ProjectRoot $projectRoot `
            -HelperNames $helperNames
    }
}
