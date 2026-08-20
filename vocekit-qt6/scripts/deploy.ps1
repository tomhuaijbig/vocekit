param(
    [string]$QtBin = "D:\QT66666\6.11.1\mingw_64\bin",
    [string]$MingwBin = "D:\QT66666\Tools\mingw1310_64\bin",
    [string]$Destination
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"
. (Join-Path $PSScriptRoot "deployment-safety.ps1")
. (Join-Path $PSScriptRoot "runtime-helper-provenance.ps1")

function Assert-DeploymentRuntimeHelperProvenance {
    param(
        [Parameter(Mandatory = $true)][string]$RuntimeDirectory,
        [Parameter(Mandatory = $true)]$RepositoryState
    )

    $runtimeFull = [IO.Path]::GetFullPath($RuntimeDirectory)
    foreach ($definition in @(
        @{ Name = "vocekit-windows-ocr"; Path = "ocr\windows\vocekit-windows-ocr.exe" },
        @{ Name = "vocekit-rapidocr"; Path = "ocr\rapidocr\vocekit-rapidocr.exe" },
        @{ Name = "vocekit-windows-speech"; Path = "speech\windows\vocekit-windows-speech.exe" }
    )) {
        [void](Get-RuntimeHelperExecutableProvenance `
            -ExecutablePath (Join-Path $runtimeFull ([string]$definition.Path)) `
            -ExpectedHelperName ([string]$definition.Name) `
            -ExpectedSourceCommit ([string]$RepositoryState.source_commit) `
            -ExpectedSourceTreeClean ([bool]$RepositoryState.source_tree_clean) `
            -ExpectedConfiguration "Release")
    }
}

$projectRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$repositoryRoot = [IO.Path]::GetFullPath((Join-Path $projectRoot ".."))
$runtimeHelperRepositoryState = Get-RuntimeHelperRepositoryState `
    -RepositoryRoot $repositoryRoot `
    -ProjectRoot $projectRoot
$usesDefaultDestination = [string]::IsNullOrWhiteSpace($Destination)
if ($usesDefaultDestination) {
    $Destination = Join-Path $projectRoot ".qt6-deploy"
}
$Destination = [IO.Path]::GetFullPath($Destination)
$deploymentRoot = $Destination
if ($usesDefaultDestination) {
    $expectedDestination = [IO.Path]::GetFullPath((Join-Path $projectRoot ".qt6-deploy"))
    if ($Destination -cne $expectedDestination) {
        throw "Default deployment destination is not the canonical .qt6-deploy directory."
    }
    [void](Assert-NoReparsePointsInExistingPathChain -Path $Destination -Label "Default deployment")
    $deploymentRoot = [IO.Path]::GetFullPath((Join-Path $projectRoot (
        ".qt6-deploy.staging-" + [Guid]::NewGuid().ToString("N")
    )))
}

$sourceExecutable = Join-Path $projectRoot ".qt6-build\release\vocekit.exe"
$deployTool = Join-Path $QtBin "windeployqt.exe"
$sourceHelperRoot = Join-Path $projectRoot "helpers\bin"
$translationSource = Join-Path (Split-Path -Parent $QtBin) "translations\qt_zh_CN.qm"
$runtimeVerifier = Join-Path $PSScriptRoot "verify-runtime.ps1"
$updaterSource = Join-Path $projectRoot "updater"

$requiredSources = @(
    $sourceExecutable,
    $deployTool,
    (Join-Path $sourceHelperRoot "vocekit-windows-ocr.exe"),
    (Join-Path $sourceHelperRoot "vocekit-rapidocr.exe"),
    (Join-Path $sourceHelperRoot "vocekit-windows-speech.exe"),
    (Join-Path $sourceHelperRoot "LICENSE-RapidOcrOnnx.txt"),
    (Join-Path $sourceHelperRoot "models"),
    $translationSource,
    $runtimeVerifier
    (Join-Path $updaterSource "vocekit-update.ps1")
    (Join-Path $updaterSource "portable.marker")
    (Join-Path $updaterSource "update-policy.json")
)
foreach ($source in $requiredSources) {
    if (-not (Test-Path -LiteralPath $source)) {
        throw "Required Qt 6 deployment source not found: $source"
    }
}

# Helpers are native publisher-owned code, so deployment must bind them to the
# same current repository commit/state instead of accepting an old ignored EXE.
foreach ($sourceHelperDefinition in @(
    @{ Name = "vocekit-windows-ocr"; Path = "vocekit-windows-ocr.exe" },
    @{ Name = "vocekit-rapidocr"; Path = "vocekit-rapidocr.exe" },
    @{ Name = "vocekit-windows-speech"; Path = "vocekit-windows-speech.exe" }
)) {
    [void](Get-RuntimeHelperExecutableProvenance `
        -ExecutablePath (Join-Path $sourceHelperRoot ([string]$sourceHelperDefinition.Path)) `
        -ExpectedHelperName ([string]$sourceHelperDefinition.Name) `
        -ExpectedSourceCommit ([string]$runtimeHelperRepositoryState.source_commit) `
        -ExpectedSourceTreeClean ([bool]$runtimeHelperRepositoryState.source_tree_clean) `
        -ExpectedConfiguration "Release")
}

New-Item -ItemType Directory -Path $deploymentRoot -Force | Out-Null
$targetExecutable = Join-Path $deploymentRoot "vocekit.exe"
Copy-Item -LiteralPath $sourceExecutable -Destination $targetExecutable -Force

$originalPath = $env:PATH
try {
    $env:PATH = "$QtBin;$MingwBin;$env:PATH"
    & $deployTool --release --compiler-runtime --no-translations $targetExecutable
    if ($LASTEXITCODE -ne 0) {
        throw "Qt 6 deployment failed with exit code $LASTEXITCODE"
    }
} finally {
    $env:PATH = $originalPath
}

$translationsDir = Join-Path $deploymentRoot "translations"
New-Item -ItemType Directory -Path $translationsDir -Force | Out-Null
Copy-Item -LiteralPath $translationSource -Destination (Join-Path $translationsDir "qt_zh_CN.qm") -Force

$windowsOcrDir = Join-Path $deploymentRoot "ocr\windows"
$rapidOcrDir = Join-Path $deploymentRoot "ocr\rapidocr"
$rapidOcrModelsDir = Join-Path $rapidOcrDir "models"
$windowsSpeechDir = Join-Path $deploymentRoot "speech\windows"
foreach ($directory in @($windowsOcrDir, $rapidOcrDir, $rapidOcrModelsDir, $windowsSpeechDir)) {
    New-Item -ItemType Directory -Path $directory -Force | Out-Null
}

Copy-Item -LiteralPath (Join-Path $sourceHelperRoot "vocekit-windows-ocr.exe") -Destination $windowsOcrDir -Force
Copy-Item -LiteralPath (Join-Path $sourceHelperRoot "vocekit-rapidocr.exe") -Destination $rapidOcrDir -Force
Copy-Item -LiteralPath (Join-Path $sourceHelperRoot "LICENSE-RapidOcrOnnx.txt") -Destination $rapidOcrDir -Force
Copy-Item -Path (Join-Path $sourceHelperRoot "models\*") -Destination $rapidOcrModelsDir -Force
Copy-Item -LiteralPath (Join-Path $sourceHelperRoot "vocekit-windows-speech.exe") -Destination $windowsSpeechDir -Force

Assert-DeploymentRuntimeHelperProvenance `
    -RuntimeDirectory $deploymentRoot `
    -RepositoryState $runtimeHelperRepositoryState

$updaterDestination = Join-Path $deploymentRoot "updater"
New-Item -ItemType Directory -Path $updaterDestination -Force | Out-Null
Copy-Item -LiteralPath (Join-Path $updaterSource "vocekit-update.ps1") -Destination $updaterDestination -Force
Copy-Item -LiteralPath (Join-Path $updaterSource "update-policy.json") -Destination $updaterDestination -Force
Copy-Item -LiteralPath (Join-Path $updaterSource "portable.marker") -Destination (Join-Path $deploymentRoot ".vocekit-portable") -Force

& $runtimeVerifier -Configuration release -RuntimeDir $deploymentRoot

if ($usesDefaultDestination) {
    Publish-ColdDefaultDeployment `
        -ProjectRoot $projectRoot `
        -StagingDirectory $deploymentRoot `
        -Destination $Destination
    $deploymentRoot = $Destination
    Assert-DeploymentRuntimeHelperProvenance `
        -RuntimeDirectory $deploymentRoot `
        -RepositoryState $runtimeHelperRepositoryState
}

$files = Get-ChildItem -LiteralPath $deploymentRoot -File -Recurse
$sizeBytes = ($files | Measure-Object -Property Length -Sum).Sum
Write-Host "Qt 6 deployment succeeded: $deploymentRoot"
Write-Host "Files: $($files.Count); Bytes: $sizeBytes"
