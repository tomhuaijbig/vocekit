Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$scriptsRoot = Split-Path -Parent $PSScriptRoot
$projectRoot = Split-Path -Parent $scriptsRoot
$provenanceScript = Join-Path $scriptsRoot "runtime-helper-provenance.ps1"
$formalVerifier = Join-Path $scriptsRoot "verify-runtime-helper-build-provenance.ps1"
. $provenanceScript
. $formalVerifier -DecisionTestMode

function Assert-Throws {
    param(
        [Parameter(Mandatory = $true)][scriptblock]$Action,
        [Parameter(Mandatory = $true)][string]$MessagePattern
    )

    $caught = $null
    try {
        & $Action
    } catch {
        $caught = $_
    }
    if ($null -eq $caught -or
        [string]$caught.Exception.Message -notmatch $MessagePattern) {
        $actual = if ($null -eq $caught) { "no exception" } else { $caught.Exception.Message }
        throw "Expected failure matching '$MessagePattern', received: $actual"
    }
}

$commit = "a" * 40
$valid = @"
{"schema_version":1,"kind":"vocekit-runtime-helper-build-provenance","helper_name":"vocekit-windows-ocr","source_commit":"$commit","source_tree_clean":true,"configuration":"Release"}
"@ | ConvertFrom-Json
Assert-RuntimeHelperBuildProvenanceObject `
    -Provenance $valid `
    -ExpectedHelperName "vocekit-windows-ocr" `
    -ExpectedSourceCommit $commit `
    -ExpectedSourceTreeClean $true
Assert-FormalRuntimeHelperBuildProvenanceObject `
    -Provenance $valid `
    -ExpectedHelperName "vocekit-windows-ocr" `
    -ExpectedSourceCommit $commit

$dirty = @"
{"schema_version":1,"kind":"vocekit-runtime-helper-build-provenance","helper_name":"vocekit-windows-ocr","source_commit":"$commit","source_tree_clean":false,"configuration":"Release"}
"@ | ConvertFrom-Json
Assert-RuntimeHelperBuildProvenanceObject `
    -Provenance $dirty `
    -ExpectedHelperName "vocekit-windows-ocr" `
    -ExpectedSourceCommit $commit `
    -ExpectedSourceTreeClean $false
Assert-Throws -MessagePattern "source_tree_clean" -Action {
    Assert-FormalRuntimeHelperBuildProvenanceObject `
        -Provenance $dirty `
        -ExpectedHelperName "vocekit-windows-ocr" `
        -ExpectedSourceCommit $commit
}

$badCases = @(
    @{
        value = [PSCustomObject]@{
            schema_version = "1"
            kind = "vocekit-runtime-helper-build-provenance"
            helper_name = "vocekit-windows-ocr"
            source_commit = $commit
            source_tree_clean = $true
            configuration = "Release"
        }
        pattern = "schema_version"
    },
    @{
        value = [PSCustomObject]@{
            schema_version = 1
            kind = "vocekit-runtime-helper-build-provenance"
            helper_name = "vocekit-rapidocr"
            source_commit = $commit
            source_tree_clean = $true
            configuration = "Release"
        }
        pattern = "helper_name"
    },
    @{
        value = [PSCustomObject]@{
            schema_version = 1
            kind = "vocekit-runtime-helper-build-provenance"
            helper_name = "vocekit-windows-ocr"
            source_commit = ("b" * 40)
            source_tree_clean = $true
            configuration = "Release"
        }
        pattern = "source_commit"
    },
    @{
        value = [PSCustomObject]@{
            schema_version = 1
            kind = "vocekit-runtime-helper-build-provenance"
            helper_name = "vocekit-windows-ocr"
            source_commit = $commit
            source_tree_clean = "true"
            configuration = "Release"
        }
        pattern = "source_tree_clean"
    },
    @{
        value = [PSCustomObject]@{
            schema_version = 1
            kind = "vocekit-runtime-helper-build-provenance"
            helper_name = "vocekit-windows-ocr"
            source_commit = $commit
            source_tree_clean = $true
            configuration = "Debug"
        }
        pattern = "configuration"
    }
)
foreach ($case in $badCases) {
    Assert-Throws -MessagePattern $case.pattern -Action {
        Assert-RuntimeHelperBuildProvenanceObject `
            -Provenance $case.value `
            -ExpectedHelperName "vocekit-windows-ocr" `
            -ExpectedSourceCommit $commit `
            -ExpectedSourceTreeClean $true
    }
}

$testRoot = Join-Path `
    ([IO.Path]::GetTempPath()) `
    ("vocekit-runtime-helper-provenance-tests-" + [Guid]::NewGuid().ToString("N"))
$temporaryProvenanceDirectory = Join-Path `
    ([IO.Path]::GetTempPath()) `
    ("vocekit-windows-speech-provenance-" + [Guid]::NewGuid().ToString("N"))
$unsafeTemporaryDirectory = Join-Path `
    ([IO.Path]::GetTempPath()) `
    ("vocekit-unsafe-runtime-helper-" + [Guid]::NewGuid().ToString("N"))
try {
    $fakeProjectRoot = Join-Path $testRoot "vocekit-qt6"
    $fakeBin = Join-Path $fakeProjectRoot "helpers\bin"
    New-Item -ItemType Directory -Path $fakeBin -Force | Out-Null
    foreach ($fileName in @(
        "vocekit-windows-ocr.exe",
        "vocekit-windows-ocr.pdb",
        "vocekit-rapidocr.exe",
        "keep-me.txt"
    )) {
        [IO.File]::WriteAllText((Join-Path $fakeBin $fileName), $fileName)
    }
    Remove-RuntimeHelperBuildOutputs `
        -ProjectRoot $fakeProjectRoot `
        -HelperNames @("vocekit-windows-ocr")
    if ((Test-Path -LiteralPath (Join-Path $fakeBin "vocekit-windows-ocr.exe")) -or
        (Test-Path -LiteralPath (Join-Path $fakeBin "vocekit-windows-ocr.pdb")) -or
        -not (Test-Path -LiteralPath (Join-Path $fakeBin "vocekit-rapidocr.exe")) -or
        -not (Test-Path -LiteralPath (Join-Path $fakeBin "keep-me.txt"))) {
        throw "Runtime helper invalidation did not remove exactly the selected helper outputs."
    }

    $releaseDirectories = @(
        (Join-Path $fakeProjectRoot "helpers\windows_ocr\obj\Release"),
        (Join-Path $fakeProjectRoot "helpers\rapidocr\obj\Release"),
        (Join-Path $fakeProjectRoot "helpers\windows_speech\obj\Release"),
        (Join-Path $fakeProjectRoot "helpers\windows_speech\obj\x64\Release")
    )
    foreach ($directory in $releaseDirectories) {
        New-Item -ItemType Directory -Path $directory -Force | Out-Null
        [IO.File]::WriteAllText((Join-Path $directory "stale-object.sentinel"), "stale")
    }
    $debugNeighbor = Join-Path $fakeProjectRoot "helpers\windows_ocr\obj\Debug\keep.obj"
    New-Item -ItemType Directory -Path (Split-Path -Parent $debugNeighbor) -Force | Out-Null
    [IO.File]::WriteAllText($debugNeighbor, "keep")
    Reset-RuntimeHelperIntermediateOutputs `
        -ProjectRoot $fakeProjectRoot `
        -HelperNames @(
            "vocekit-windows-ocr",
            "vocekit-rapidocr",
            "vocekit-windows-speech"
        )
    foreach ($directory in $releaseDirectories) {
        if (Test-Path -LiteralPath $directory) {
            throw "Cold runtime helper reset retained stale Release intermediates: $directory"
        }
    }
    if (-not (Test-Path -LiteralPath $debugNeighbor -PathType Leaf)) {
        throw "Cold runtime helper reset deleted a non-Release neighbor."
    }

    $oldModels = Join-Path $fakeProjectRoot "helpers\bin\models"
    New-Item -ItemType Directory -Path $oldModels -Force | Out-Null
    [IO.File]::WriteAllText((Join-Path $oldModels "obsolete-model.bin"), "obsolete")
    $modelsPath = Reset-RapidOcrModelOutputDirectory -ProjectRoot $fakeProjectRoot
    if (Test-Path -LiteralPath (Join-Path $modelsPath "obsolete-model.bin")) {
        throw "RapidOCR model reset retained an obsolete model."
    }
    $modelFiles = @{
        "model-a.onnx" = "model-a"
        "keys.txt" = "keys"
    }
    $expectedModelHashes = @{}
    foreach ($modelName in $modelFiles.Keys) {
        $modelPath = Join-Path $modelsPath $modelName
        [IO.File]::WriteAllText($modelPath, $modelFiles[$modelName])
        $expectedModelHashes[$modelName] = (Get-FileHash -LiteralPath $modelPath -Algorithm SHA256).Hash
    }
    Assert-RapidOcrModelOutputDirectory `
        -ModelsPath $modelsPath `
        -ExpectedSha256 $expectedModelHashes
    [IO.File]::WriteAllText((Join-Path $modelsPath "extra.bin"), "extra")
    Assert-Throws -MessagePattern "no extras" -Action {
        Assert-RapidOcrModelOutputDirectory `
            -ModelsPath $modelsPath `
            -ExpectedSha256 $expectedModelHashes
    }

    New-Item -ItemType Directory -Path $temporaryProvenanceDirectory | Out-Null
    $generatedSource = Join-Path $temporaryProvenanceDirectory "BuildProvenance.g.cs"
    $generatedState = [PSCustomObject]@{
        source_commit = $commit
        source_tree_clean = $false
        configuration = "Release"
    }
    [void](New-WindowsSpeechBuildProvenanceSource `
        -Path $generatedSource `
        -BuildState $generatedState)
    $generatedText = Get-Content -LiteralPath $generatedSource -Raw
    foreach ($requiredText in @(
        "SourceCommit = `"$commit`"",
        "SourceTreeClean = false",
        "Configuration = `"Release`""
    )) {
        if ($generatedText -cnotmatch [regex]::Escape($requiredText)) {
            throw "Generated Windows speech provenance source is missing: $requiredText"
        }
    }
    Remove-RuntimeHelperTemporaryDirectory -Path $temporaryProvenanceDirectory
    if (Test-Path -LiteralPath $temporaryProvenanceDirectory) {
        throw "Windows speech provenance temporary directory was not deleted."
    }

    New-Item -ItemType Directory -Path $unsafeTemporaryDirectory | Out-Null
    Assert-Throws -MessagePattern "unsafe Windows speech provenance directory" -Action {
        Remove-RuntimeHelperTemporaryDirectory -Path $unsafeTemporaryDirectory
    }

    [IO.Directory]::Delete($fakeProjectRoot, $true)
    New-Item -ItemType Directory -Path $fakeProjectRoot -Force | Out-Null
    $trackedFile = Join-Path $fakeProjectRoot "tracked.txt"
    [IO.File]::WriteAllText($trackedFile, "initial")
    & git -C $testRoot init --quiet
    if ($LASTEXITCODE -ne 0) { throw "Unable to initialize the provenance Git fixture." }
    & git -C $testRoot config user.name "VoceKit Test"
    & git -C $testRoot config user.email "vocekit-test@example.invalid"
    & git -C $testRoot add -- "vocekit-qt6/tracked.txt"
    & git -C $testRoot commit --quiet -m "fixture"
    if ($LASTEXITCODE -ne 0) { throw "Unable to commit the provenance Git fixture." }

    $cleanState = Get-RuntimeHelperRepositoryState `
        -RepositoryRoot $testRoot `
        -ProjectRoot $fakeProjectRoot
    if (-not [bool]$cleanState.source_tree_clean -or
        [string]$cleanState.configuration -cne "Release" -or
        [string]$cleanState.source_commit -notmatch '^[0-9a-f]{40}([0-9a-f]{24})?$') {
        throw "The trusted Git fixture did not produce a clean Release source state."
    }

    [IO.File]::WriteAllText($trackedFile, "changed")
    $dirtyState = Get-RuntimeHelperRepositoryState `
        -RepositoryRoot $testRoot `
        -ProjectRoot $fakeProjectRoot
    if ([bool]$dirtyState.source_tree_clean) {
        throw "A tracked source change was not detected by the trusted Git state."
    }
    Assert-Throws -MessagePattern "repository changed" -Action {
        [void](Assert-RuntimeHelperRepositoryStateUnchanged `
            -Before $cleanState `
            -RepositoryRoot $testRoot `
            -ProjectRoot $fakeProjectRoot)
    }

    & git -C $testRoot checkout -- "vocekit-qt6/tracked.txt"
    & git -C $testRoot update-index --assume-unchanged "vocekit-qt6/tracked.txt"
    Assert-Throws -MessagePattern "hidden entries" -Action {
        [void](Get-RuntimeHelperRepositoryState `
            -RepositoryRoot $testRoot `
            -ProjectRoot $fakeProjectRoot)
    }
    & git -C $testRoot update-index --no-assume-unchanged "vocekit-qt6/tracked.txt"

    $gitIndexFileWasDefined = Test-Path -LiteralPath Env:GIT_INDEX_FILE
    $savedGitIndexFile = [Environment]::GetEnvironmentVariable(
        "GIT_INDEX_FILE",
        [EnvironmentVariableTarget]::Process
    )
    try {
        [Environment]::SetEnvironmentVariable(
            "GIT_INDEX_FILE",
            (Join-Path $testRoot "fake-index"),
            [EnvironmentVariableTarget]::Process
        )
        Assert-Throws -MessagePattern "GIT_INDEX_FILE" -Action {
            [void](Get-RuntimeHelperRepositoryState `
                -RepositoryRoot $testRoot `
                -ProjectRoot $fakeProjectRoot)
        }
    } finally {
        if ($gitIndexFileWasDefined) {
            $env:GIT_INDEX_FILE = $savedGitIndexFile
        } else {
            Remove-Item -LiteralPath Env:GIT_INDEX_FILE -ErrorAction SilentlyContinue
        }
    }
    if ((Test-Path -LiteralPath Env:GIT_INDEX_FILE) -ne $gitIndexFileWasDefined -or
        ($gitIndexFileWasDefined -and $env:GIT_INDEX_FILE -cne $savedGitIndexFile)) {
        throw "Runtime helper provenance tests did not restore GIT_INDEX_FILE exactly."
    }
} finally {
    foreach ($directory in @(
        $temporaryProvenanceDirectory,
        $unsafeTemporaryDirectory,
        $testRoot
    )) {
        if ([IO.Directory]::Exists($directory)) {
            Get-ChildItem -LiteralPath $directory -Recurse -Force -ErrorAction SilentlyContinue |
                ForEach-Object {
                    try { $_.Attributes = [IO.FileAttributes]::Normal } catch { }
                }
            [IO.Directory]::Delete($directory, $true)
        }
    }
}

$staticChecks = @{
    (Join-Path $projectRoot "helpers\runtime_helper_build_provenance.h") = @(
        "VOCEKIT_HELPER_SOURCE_COMMIT",
        "source_tree_clean",
        "configuration"
    )
    (Join-Path $projectRoot "helpers\windows_speech\Program.cs") = @(
        "--build-provenance-json",
        "BuildProvenance.SourceCommit"
    )
    (Join-Path $projectRoot "helpers\windows_speech\windows_speech.csproj") = @(
        "VoceKitHelperBuildProvenanceSource"
    )
    (Join-Path $scriptsRoot "build-runtime-helpers.ps1") = @(
        "ExpectedSourceCommit",
        "ExpectedSourceTreeClean",
        "Get-RuntimeHelperExecutableProvenance"
    )
    (Join-Path $scriptsRoot "build-ocr-helpers.ps1") = @(
        "Reset-RuntimeHelperIntermediateOutputs",
        "Reset-RapidOcrModelOutputDirectory",
        "Assert-RapidOcrModelOutputDirectory",
        "ch_PP-OCRv3_det_infer.onnx",
        "ch_PP-OCRv3_rec_infer.onnx",
        "ch_ppocr_mobile_v2.0_cls_infer.onnx",
        "ppocr_keys_v1.txt"
    )
}
foreach ($path in $staticChecks.Keys) {
    $text = Get-Content -LiteralPath $path -Raw
    foreach ($pattern in $staticChecks[$path]) {
        if ($text -notmatch [regex]::Escape($pattern)) {
            throw "Runtime helper provenance wiring is missing '$pattern' from $path."
        }
    }
}

Write-Host "Runtime helper provenance tests: PASS"
