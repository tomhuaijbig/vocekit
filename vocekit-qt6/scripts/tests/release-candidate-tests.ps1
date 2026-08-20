Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$scriptsRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$createScript = Join-Path $scriptsRoot "scripts\create-release-package.ps1"
$packageScript = Join-Path $scriptsRoot "scripts\package-test.ps1"
$manifestScript = Join-Path $scriptsRoot "scripts\create-update-manifest.ps1"
$provenanceScript = Join-Path $scriptsRoot "scripts\verify-embedded-build-provenance.ps1"
$buildProvenanceScript = Join-Path $scriptsRoot "scripts\build-provenance.ps1"
$deploymentSafetyScript = Join-Path $scriptsRoot "scripts\deployment-safety.ps1"

. $createScript -DecisionTestMode
. $provenanceScript -DecisionTestMode
. $packageScript -DecisionTestMode
. $buildProvenanceScript
. $deploymentSafetyScript

function Remove-TestGitDirectory {
    param([Parameter(Mandatory = $true)][string]$Path)

    $fullPath = [IO.Path]::GetFullPath($Path)
    $tempPrefix = [IO.Path]::GetFullPath([IO.Path]::GetTempPath()).TrimEnd("\", "/") + "\"
    if (-not $fullPath.StartsWith($tempPrefix, [StringComparison]::OrdinalIgnoreCase) -or
        [IO.Path]::GetFileName($fullPath) -notlike "vocekit-git-clean-state-test-*") {
        throw "Refusing to remove an unsafe temporary Git fixture path: $fullPath"
    }
    if ([IO.Directory]::Exists($fullPath)) {
        foreach ($file in Get-ChildItem -LiteralPath $fullPath -Recurse -Force -File) {
            [IO.File]::SetAttributes($file.FullName, [IO.FileAttributes]::Normal)
        }
        [IO.Directory]::Delete($fullPath, $true)
    }
}

$decisionModeBypassWasRejected = $false
try {
    & $packageScript -DecisionTestMode -RequireSignedBinaries -FailIfOutputExists
} catch {
    $decisionModeBypassWasRejected = $true
}
if (-not $decisionModeBypassWasRejected) {
    throw "package-test DecisionTestMode could be combined with formal production gates."
}

$buildScriptText = Get-Content -LiteralPath (Join-Path $scriptsRoot "scripts\build.ps1") -Raw -Encoding UTF8
if (-not $buildScriptText.Contains('.build-fingerprint.json') -or
    -not $buildScriptText.Contains('[IO.Directory]::Delete($fullBuildRoot, $true)') -or
    -not $buildScriptText.Contains('[IO.Directory]::CreateDirectory($buildRoot)') -or
    -not $buildScriptText.Contains('source_tree_clean = [bool]$sourceTreeClean') -or
    -not $buildScriptText.Contains('VOCEKIT_SOURCE_TREE_CLEAN=$sourceTreeCleanDefine') -or
    -not $buildScriptText.Contains('Test-ShouldResetQtBuildTree') -or
    -not $buildScriptText.Contains('every clean Release build is') -or
    -not $buildScriptText.Contains('Invalidate-CleanProvenanceBuildOutput') -or
    $buildScriptText.IndexOf('Invalidate-CleanProvenanceBuildOutput') -gt
        $buildScriptText.IndexOf('the newly linked executable and fingerprint were deleted') -or
    -not $buildScriptText.Contains('[IO.FileAttributes]::ReparsePoint') -or
    $buildScriptText.IndexOf('[IO.FileAttributes]::ReparsePoint') -gt
        $buildScriptText.IndexOf('$previousFingerprint =') -or
    $buildScriptText.IndexOf('[IO.Directory]::Delete($fullBuildRoot, $true)') -gt
        $buildScriptText.IndexOf('& $qmake @qmakeArguments') -or
    $buildScriptText.LastIndexOf('if ($resetBuildTree)') -gt
        $buildScriptText.IndexOf('[IO.File]::Move($temporaryFingerprintPath, $fingerprintPath)')) {
    throw "Qt build script does not fully reset the fingerprint-stale shadow build tree before qmake."
}
if (-not (Test-ShouldResetQtBuildTree -Configuration release -SourceTreeClean $true -FingerprintChanged $false) -or
    -not (Test-ShouldResetQtBuildTree -Configuration release -SourceTreeClean $false -FingerprintChanged $true) -or
    (Test-ShouldResetQtBuildTree -Configuration release -SourceTreeClean $false -FingerprintChanged $false) -or
    (Test-ShouldResetQtBuildTree -Configuration debug -SourceTreeClean $true -FingerprintChanged $false) -or
    -not (Test-ShouldResetQtBuildTree -Configuration debug -SourceTreeClean $false -FingerprintChanged $true)) {
    throw "Qt build reset policy does not force a cold clean Release while preserving safe incremental development builds."
}
$mainSourceText = Get-Content -LiteralPath (Join-Path $scriptsRoot "src\main.cpp") -Raw -Encoding UTF8
if (-not $mainSourceText.Contains("UpdateService::defaultFeedUrl()") -or
    -not $mainSourceText.Contains('QStringLiteral("source_tree_clean")') -or
    -not $mainSourceText.Contains('QStringLiteral("configuration")') -or
    -not $mainSourceText.Contains('QStringLiteral("schema_version"), 3')) {
    throw "Executable provenance does not bind the actual update feed, configuration, and clean-source state under schema 3."
}

$gitStateRoot = Join-Path ([IO.Path]::GetTempPath()) ("vocekit-git-clean-state-test-" + [Guid]::NewGuid().ToString("N"))
try {
    [void][IO.Directory]::CreateDirectory($gitStateRoot)
    & git -C $gitStateRoot init --quiet
    if ($LASTEXITCODE -ne 0) { throw "Could not initialize clean-source Git fixture." }
    [IO.File]::WriteAllText((Join-Path $gitStateRoot "tracked.txt"), "tracked", [Text.UTF8Encoding]::new($false))
    [IO.File]::WriteAllText((Join-Path $gitStateRoot ".gitignore"), ".fixture-output/`n", [Text.UTF8Encoding]::new($false))
    & git -C $gitStateRoot add -- tracked.txt .gitignore
    if ($LASTEXITCODE -ne 0) { throw "Could not stage clean-source Git fixture." }
    & git -C $gitStateRoot -c user.name=VoceKit-Test -c user.email=vocekit-test@example.invalid commit --quiet -m "fixture"
    if ($LASTEXITCODE -ne 0) { throw "Could not commit clean-source Git fixture." }
    if (-not (Get-GitSourceTreeClean -RepositoryRoot $gitStateRoot)) {
        throw "A clean temporary Git source tree was reported dirty."
    }

    $ignoredOutput = Join-Path $gitStateRoot ".fixture-output\ignored.bin"
    [void][IO.Directory]::CreateDirectory((Split-Path -Parent $ignoredOutput))
    [IO.File]::WriteAllText($ignoredOutput, "ignored")
    if (-not (Get-GitSourceTreeClean -RepositoryRoot $gitStateRoot)) {
        throw "A Git-ignored build output incorrectly marked the source tree dirty."
    }

    $untrackedSource = Join-Path $gitStateRoot "untracked-source.cpp"
    [IO.File]::WriteAllText($untrackedSource, "untracked")
    if (Get-GitSourceTreeClean -RepositoryRoot $gitStateRoot) {
        throw "An untracked source file was omitted from clean-source provenance."
    }
    [IO.File]::Delete($untrackedSource)
    [IO.File]::AppendAllText((Join-Path $gitStateRoot "tracked.txt"), " modified")
    if (Get-GitSourceTreeClean -RepositoryRoot $gitStateRoot) {
        throw "A modified tracked source file was omitted from clean-source provenance."
    }
    & git -C $gitStateRoot checkout --quiet -- tracked.txt

    & git -C $gitStateRoot update-index --assume-unchanged tracked.txt
    $assumeUnchangedWasRejected = $false
    try { [void](Get-GitSourceTreeClean -RepositoryRoot $gitStateRoot) } catch {
        $assumeUnchangedWasRejected = $_.Exception.Message -match "hidden entries"
    }
    & git -C $gitStateRoot update-index --no-assume-unchanged tracked.txt
    if (-not $assumeUnchangedWasRejected) {
        throw "Git assume-unchanged hid a tracked file from the formal clean-state gate."
    }

    & git -C $gitStateRoot update-index --skip-worktree tracked.txt
    $skipWorktreeWasRejected = $false
    try { [void](Get-GitSourceTreeClean -RepositoryRoot $gitStateRoot) } catch {
        $skipWorktreeWasRejected = $_.Exception.Message -match "hidden entries"
    }
    & git -C $gitStateRoot update-index --no-skip-worktree tracked.txt
    if (-not $skipWorktreeWasRejected) {
        throw "Git skip-worktree hid a tracked file from the formal clean-state gate."
    }

    $oldGitDirectory = [Environment]::GetEnvironmentVariable("GIT_DIR", "Process")
    $oldGitWorkTree = [Environment]::GetEnvironmentVariable("GIT_WORK_TREE", "Process")
    try {
        [Environment]::SetEnvironmentVariable("GIT_DIR", (Join-Path $gitStateRoot ".git"), "Process")
        [Environment]::SetEnvironmentVariable("GIT_WORK_TREE", $gitStateRoot, "Process")
        $gitEnvironmentOverrideWasRejected = $false
        try { [void](Get-GitSourceTreeClean -RepositoryRoot $gitStateRoot) } catch {
            $gitEnvironmentOverrideWasRejected = $_.Exception.Message -match "environment override GIT_DIR"
        }
        if (-not $gitEnvironmentOverrideWasRejected) {
            throw "Process GIT_DIR/GIT_WORK_TREE redirected the formal Git trust root."
        }
    } finally {
        if ([string]::IsNullOrEmpty($oldGitDirectory)) {
            Remove-Item Env:GIT_DIR -ErrorAction SilentlyContinue
        } else {
            $env:GIT_DIR = $oldGitDirectory
        }
        if ([string]::IsNullOrEmpty($oldGitWorkTree)) {
            Remove-Item Env:GIT_WORK_TREE -ErrorAction SilentlyContinue
        } else {
            $env:GIT_WORK_TREE = $oldGitWorkTree
        }
    }

    $oldGitConfigCount = [Environment]::GetEnvironmentVariable("GIT_CONFIG_COUNT", "Process")
    try {
        [Environment]::SetEnvironmentVariable("GIT_CONFIG_COUNT", "1", "Process")
        [Environment]::SetEnvironmentVariable("GIT_CONFIG_KEY_0", "core.worktree", "Process")
        [Environment]::SetEnvironmentVariable("GIT_CONFIG_VALUE_0", $gitStateRoot, "Process")
        $gitConfigInjectionWasRejected = $false
        try { [void](Get-GitSourceTreeClean -RepositoryRoot $gitStateRoot) } catch {
            $gitConfigInjectionWasRejected = $_.Exception.Message -match "GIT_CONFIG_COUNT"
        }
        if (-not $gitConfigInjectionWasRejected) {
            throw "GIT_CONFIG_COUNT/KEY/VALUE injected configuration into the formal Git trust gate."
        }
    } finally {
        if ([string]::IsNullOrEmpty($oldGitConfigCount)) {
            Remove-Item Env:GIT_CONFIG_COUNT -ErrorAction SilentlyContinue
        } else {
            $env:GIT_CONFIG_COUNT = $oldGitConfigCount
        }
        Remove-Item Env:GIT_CONFIG_KEY_0 -ErrorAction SilentlyContinue
        Remove-Item Env:GIT_CONFIG_VALUE_0 -ErrorAction SilentlyContinue
    }

    $fixtureHead = (& git -C $gitStateRoot rev-parse HEAD).Trim()
    & git -C $gitStateRoot update-ref "refs/replace/$fixtureHead" $fixtureHead
    $replaceRefWasRejected = $false
    try { [void](Get-GitSourceTreeClean -RepositoryRoot $gitStateRoot) } catch {
        $replaceRefWasRejected = $_.Exception.Message -match "replace refs"
    }
    & git -C $gitStateRoot update-ref -d "refs/replace/$fixtureHead"
    if (-not $replaceRefWasRejected) {
        throw "A Git replace ref was accepted by the formal source provenance gate."
    }

    Assert-FormalSourceStateUnchanged `
        -RepositoryRoot $gitStateRoot `
        -ExpectedSourceCommit ((& git -C $gitStateRoot rev-parse HEAD).Trim())
    [IO.File]::AppendAllText((Join-Path $gitStateRoot "tracked.txt"), " changed during package")
    $midPackageChangeWasRejected = $false
    try {
        Assert-FormalSourceStateUnchanged `
            -RepositoryRoot $gitStateRoot `
            -ExpectedSourceCommit ((& git -C $gitStateRoot rev-parse HEAD).Trim())
    } catch {
        $midPackageChangeWasRejected = $_.Exception.Message -match "changed while"
    }
    & git -C $gitStateRoot checkout --quiet -- tracked.txt
    if (-not $midPackageChangeWasRejected) {
        throw "Formal candidate packaging accepted a source tree changed after readiness."
    }

    $bareOrigin = Join-Path $gitStateRoot "origin.git"
    & git init --quiet --bare $bareOrigin
    & git -C $gitStateRoot remote add origin $bareOrigin
    Assert-FormalReleaseTagUnused -RepositoryRoot $gitStateRoot -ExpectedTag "v0.2.0"
    & git -C $gitStateRoot -c user.name=VoceKit-Test -c user.email=vocekit-test@example.invalid tag -a v0.2.0 -m "fixture tag"
    $localExistingTagWasRejected = $false
    try { Assert-FormalReleaseTagUnused -RepositoryRoot $gitStateRoot -ExpectedTag "v0.2.0" } catch {
        $localExistingTagWasRejected = $_.Exception.Message -match "already exists locally"
    }
    if (-not $localExistingTagWasRejected) {
        throw "Formal candidate creation accepted an existing local release tag."
    }
    & git -C $gitStateRoot push --quiet origin refs/tags/v0.2.0:refs/tags/v0.2.0
    & git -C $gitStateRoot tag -d v0.2.0 | Out-Null
    $remoteExistingTagWasRejected = $false
    try { Assert-FormalReleaseTagUnused -RepositoryRoot $gitStateRoot -ExpectedTag "v0.2.0" } catch {
        $remoteExistingTagWasRejected = $_.Exception.Message -match "already exists on origin"
    }
    if (-not $remoteExistingTagWasRejected) {
        throw "Formal candidate creation accepted an existing origin release tag."
    }
} finally {
    Remove-TestGitDirectory -Path $gitStateRoot
}

$invalidationTestRoot = Join-Path ([IO.Path]::GetTempPath()) (
    "vocekit-clean-build-invalidation-test-" + [Guid]::NewGuid().ToString("N")
)
try {
    $invalidationBuildRoot = Join-Path $invalidationTestRoot ".qt6-build"
    $invalidationOutputRoot = Join-Path $invalidationBuildRoot "debug"
    [void][IO.Directory]::CreateDirectory($invalidationOutputRoot)
    $invalidationExecutable = Join-Path $invalidationOutputRoot "vocekit.exe"
    $invalidationFingerprint = Join-Path $invalidationBuildRoot ".build-fingerprint.json"
    [IO.File]::WriteAllText($invalidationExecutable, "untrusted executable")
    [IO.File]::WriteAllText($invalidationFingerprint, "stale trusted fingerprint")
    Invalidate-CleanProvenanceBuildOutput `
        -BuildRoot $invalidationBuildRoot `
        -Configuration debug
    if ([IO.File]::Exists($invalidationExecutable) -or
        [IO.File]::Exists($invalidationFingerprint)) {
        throw "A clean-to-dirty build transition left a trusted executable or fingerprint reusable."
    }
} finally {
    if ([IO.Directory]::Exists($invalidationTestRoot)) {
        [IO.Directory]::Delete($invalidationTestRoot, $true)
    }
}

$coldDeployRoot = Join-Path ([IO.Path]::GetTempPath()) (
    "vocekit-cold-deploy-test-" + [Guid]::NewGuid().ToString("N")
)
try {
    $oldDeploy = Join-Path $coldDeployRoot ".qt6-deploy"
    $newDeploy = Join-Path $coldDeployRoot (
        ".qt6-deploy.staging-" + [Guid]::NewGuid().ToString("N")
    )
    [void][IO.Directory]::CreateDirectory($oldDeploy)
    [void][IO.Directory]::CreateDirectory($newDeploy)
    [IO.File]::WriteAllText((Join-Path $oldDeploy "stale.dll"), "stale")
    [IO.File]::WriteAllText((Join-Path $newDeploy "vocekit.exe"), "fresh")
    Publish-ColdDefaultDeployment `
        -ProjectRoot $coldDeployRoot `
        -StagingDirectory $newDeploy `
        -Destination $oldDeploy
    if ([IO.File]::Exists((Join-Path $oldDeploy "stale.dll")) -or
        -not [IO.File]::Exists((Join-Path $oldDeploy "vocekit.exe")) -or
        [IO.Directory]::Exists($newDeploy) -or
        @(Get-ChildItem -LiteralPath $coldDeployRoot -Directory -Filter ".qt6-deploy.previous-*").Count -ne 0) {
        throw "Cold default deployment retained a stale file from the previous runtime."
    }
} finally {
    if ([IO.Directory]::Exists($coldDeployRoot)) {
        [IO.Directory]::Delete($coldDeployRoot, $true)
    }
}

$failedColdDeployRoot = Join-Path ([IO.Path]::GetTempPath()) (
    "vocekit-cold-deploy-restore-failure-test-" + [Guid]::NewGuid().ToString("N")
)
try {
    $failedDestination = Join-Path $failedColdDeployRoot ".qt6-deploy"
    $failedStaging = Join-Path $failedColdDeployRoot (
        ".qt6-deploy.staging-" + [Guid]::NewGuid().ToString("N")
    )
    [void][IO.Directory]::CreateDirectory($failedDestination)
    [void][IO.Directory]::CreateDirectory($failedStaging)
    [IO.File]::WriteAllText((Join-Path $failedDestination "recoverable.dll"), "previous runtime")
    [IO.File]::WriteAllText((Join-Path $failedStaging "vocekit.exe"), "new runtime")
    $injectedMoveFailure = {
        param([string]$Source, [string]$Target)
        if ($Source -like "*.qt6-deploy.staging-*" -or
            $Source -like "*.qt6-deploy.previous-*") {
            throw "Injected directory move failure for recovery test."
        }
        [IO.Directory]::Move($Source, $Target)
    }
    $failedRestoreWasReported = $false
    try {
        Publish-ColdDefaultDeployment `
            -ProjectRoot $failedColdDeployRoot `
            -StagingDirectory $failedStaging `
            -Destination $failedDestination `
            -MoveDirectoryAction $injectedMoveFailure
    } catch {
        $failedRestoreWasReported = $_.Exception.Message -match "backup was preserved at"
    }
    $preservedBackups = @(
        Get-ChildItem -LiteralPath $failedColdDeployRoot -Directory -Filter ".qt6-deploy.previous-*"
    )
    if (-not $failedRestoreWasReported -or
        $preservedBackups.Count -ne 1 -or
        -not [IO.File]::Exists((Join-Path $preservedBackups[0].FullName "recoverable.dll")) -or
        [IO.Directory]::Exists($failedDestination) -or
        -not [IO.Directory]::Exists($failedStaging)) {
        throw "A failed deployment restore did not preserve and report the only recoverable backup."
    }
} finally {
    if ([IO.Directory]::Exists($failedColdDeployRoot)) {
        [IO.Directory]::Delete($failedColdDeployRoot, $true)
    }
}

$testProvenance = [PSCustomObject]@{
    schema_version = 3
    source_commit = "a" * 40
    source_tree_clean = $true
    configuration = "release"
    version = "0.2.0"
    update_feed_url = "https://api.github.com/repos/example/vocekit/releases/latest"
}
Assert-EmbeddedBuildProvenance `
    -Provenance $testProvenance `
    -ExpectedSourceCommit ("a" * 40) `
    -ExpectedVersion "0.2.0" `
    -ExpectedUpdateFeedUrl $testProvenance.update_feed_url `
    -ExpectedConfiguration release
$staleProvenanceWasRejected = $false
try {
    Assert-EmbeddedBuildProvenance `
        -Provenance $testProvenance `
        -ExpectedSourceCommit ("b" * 40) `
        -ExpectedVersion "0.2.0" `
        -ExpectedUpdateFeedUrl $testProvenance.update_feed_url `
        -ExpectedConfiguration release
} catch {
    $staleProvenanceWasRejected = $_.Exception.Message -match "source_commit"
}
if (-not $staleProvenanceWasRejected) {
    throw "A stale binary build provenance commit was accepted for the current source."
}

$dirtyProvenance = $testProvenance | ConvertTo-Json -Depth 4 | ConvertFrom-Json
$dirtyProvenance.source_tree_clean = $false
$dirtyProvenanceWasRejected = $false
try {
    Assert-EmbeddedBuildProvenance `
        -Provenance $dirtyProvenance `
        -ExpectedSourceCommit ("a" * 40) `
        -ExpectedVersion "0.2.0" `
        -ExpectedUpdateFeedUrl $dirtyProvenance.update_feed_url `
        -ExpectedConfiguration release
} catch {
    $dirtyProvenanceWasRejected = $_.Exception.Message -match "source_tree_clean"
}
if (-not $dirtyProvenanceWasRejected) {
    throw "A dirty executable build provenance was accepted for a formal release."
}

$debugProvenance = $testProvenance | ConvertTo-Json -Depth 4 | ConvertFrom-Json
$debugProvenance.configuration = "debug"
$debugProvenanceWasRejected = $false
try {
    Assert-EmbeddedBuildProvenance `
        -Provenance $debugProvenance `
        -ExpectedSourceCommit ("a" * 40) `
        -ExpectedVersion "0.2.0" `
        -ExpectedUpdateFeedUrl $debugProvenance.update_feed_url `
        -ExpectedConfiguration release
} catch {
    $debugProvenanceWasRejected = $_.Exception.Message -match "configuration"
}
if (-not $debugProvenanceWasRejected) {
    throw "A clean Debug executable was accepted as a formal Release candidate."
}

$publicationRoot = Join-Path ([IO.Path]::GetTempPath()) ("vocekit-formal-publication-test-" + [Guid]::NewGuid().ToString("N"))
try {
    $stagingDirectory = Join-Path $publicationRoot "staging"
    $temporaryArchive = Join-Path $publicationRoot "temporary.zip"
    $packageDirectory = Join-Path $publicationRoot "formal-package"
    $archivePath = Join-Path $publicationRoot "formal-package.zip"
    [void][IO.Directory]::CreateDirectory($stagingDirectory)
    [IO.File]::WriteAllText((Join-Path $stagingDirectory "payload.txt"), "payload")
    [IO.File]::WriteAllText($temporaryArchive, "new archive")
    $raceWasRejected = $false
    try {
        Publish-FormalPackagePair `
            -StagingDirectory $stagingDirectory `
            -TemporaryArchive $temporaryArchive `
            -PackageDirectory $packageDirectory `
            -ArchivePath $archivePath `
            -FailureInjection "formal-zip-race-after-directory"
    } catch {
        $raceWasRejected = $true
    }
    if (-not $raceWasRejected -or
        -not (Test-Path -LiteralPath $archivePath -PathType Leaf) -or
        (Get-Content -LiteralPath $archivePath -Raw) -cne "race-sentinel" -or
        (Test-Path -LiteralPath $packageDirectory)) {
        throw "Formal package publication overwrote a ZIP that appeared after preflight."
    }
} finally {
    if ([IO.Directory]::Exists($publicationRoot)) {
        [IO.Directory]::Delete($publicationRoot, $true)
    }
}

Assert-ReleaseTagMatchesVersion -Version "0.2.0" -ExpectedTag "v0.2.0"

$prereleaseVersionWasRejected = $false
try {
    Assert-ReleaseTagMatchesVersion -Version "0.2.0-beta.1" -ExpectedTag "v0.2.0-beta.1"
} catch {
    $prereleaseVersionWasRejected = $_.Exception.Message -match "stable x.y.z"
}
if (-not $prereleaseVersionWasRejected) {
    throw "A prerelease APP_VERSION was mislabeled as a stable formal release."
}

$wrongTagWasRejected = $false
try {
    Assert-ReleaseTagMatchesVersion -Version "0.2.0" -ExpectedTag "v0.2.1"
} catch {
    $wrongTagWasRejected = $_.Exception.Message -match "does not match APP_VERSION"
}
if (-not $wrongTagWasRejected) {
    throw "A signed release candidate accepted a tag that does not match APP_VERSION."
}

$testRoot = Join-Path ([IO.Path]::GetTempPath()) ("vocekit-release-candidate-test-" + [Guid]::NewGuid().ToString("N"))
try {
    $paths = Get-ReleaseCandidatePaths `
        -DistributionRoot $testRoot `
        -Version "0.2.0" `
        -PackageName "vocekit-qt6-portable"
    $expectedReleaseDirectory = Join-Path $testRoot "releases\v0.2.0"
    if ($paths.ReleaseDirectory -cne [IO.Path]::GetFullPath($expectedReleaseDirectory)) {
        throw "The signed candidate is not isolated beneath a version-specific directory."
    }
    Assert-FormalReleaseTargetUnused -ReleaseDirectory $paths.ReleaseDirectory

    [void][IO.Directory]::CreateDirectory($paths.ReleaseDirectory)
    $existingTargetWasRejected = $false
    try {
        Assert-FormalReleaseTargetUnused -ReleaseDirectory $paths.ReleaseDirectory
    } catch {
        $existingTargetWasRejected = $_.Exception.Message -match "already exists"
    }
    if (-not $existingTargetWasRejected) {
        throw "The signed candidate creator accepted an existing formal release target."
    }
} finally {
    if ([IO.Directory]::Exists($testRoot)) {
        [IO.Directory]::Delete($testRoot, $true)
    }
}

$recordRoot = Join-Path ([IO.Path]::GetTempPath()) ("vocekit-release-record-test-" + [Guid]::NewGuid().ToString("N"))
try {
    [void][IO.Directory]::CreateDirectory($recordRoot)
    $archivePath = Join-Path $recordRoot "vocekit-qt6-portable.zip"
    $sidecarPath = "$archivePath.sha256"
    $manifestPath = Join-Path $recordRoot "update-manifest.json"
    [IO.File]::WriteAllText($archivePath, "archive bytes")
    [IO.File]::WriteAllText($sidecarPath, "sidecar bytes")
    [IO.File]::WriteAllText($manifestPath, "manifest bytes")
    $signer = [PSCustomObject]@{
        Subject = "CN=VoceKit Release Test"
        Thumbprint = "0123456789ABCDEF0123456789ABCDEF01234567"
        TimestampSubject = "CN=Timestamp Test"
        TimestampThumbprint = "89ABCDEF0123456789ABCDEF0123456789ABCDEF"
    }
    $runtimeHelpers = @(
        @{ helper_name = "vocekit-windows-ocr"; relative_path = "ocr/windows/vocekit-windows-ocr.exe" },
        @{ helper_name = "vocekit-rapidocr"; relative_path = "ocr/rapidocr/vocekit-rapidocr.exe" },
        @{ helper_name = "vocekit-windows-speech"; relative_path = "speech/windows/vocekit-windows-speech.exe" }
    ) | ForEach-Object {
        [PSCustomObject]@{
            helper_name = $_.helper_name
            relative_path = $_.relative_path
            sha256 = "d" * 64
            provenance = [PSCustomObject]@{
                schema_version = 1
                kind = "vocekit-runtime-helper-build-provenance"
                helper_name = $_.helper_name
                source_commit = "a" * 40
                source_tree_clean = $true
                configuration = "Release"
            }
        }
    }

    $dirtyRecordWasRejected = $false
    try {
        [void](New-ReleaseCandidateRecord `
            -SourceCommit ("a" * 40) `
            -Version "0.2.0" `
            -ExpectedTag "v0.2.0" `
            -PackageName "vocekit-qt6-portable" `
            -UpdateFeedUrl "https://api.github.com/repos/example/vocekit/releases/latest" `
            -ReleaseBaseUrl "https://github.com/example/vocekit/releases/download" `
            -ReleasePageBaseUrl "https://github.com/example/vocekit/releases/tag" `
            -ArchivePath $archivePath `
            -SidecarPath $sidecarPath `
            -ManifestPath $manifestPath `
            -Signer $signer `
            -BinaryProvenance $dirtyProvenance `
            -RuntimeHelpers $runtimeHelpers)
    } catch {
        $dirtyRecordWasRejected = $_.Exception.Message -match "clean Git source tree"
    }
    if (-not $dirtyRecordWasRejected) {
        throw "The signed candidate creator accepted executable provenance from a dirty source tree."
    }

    $record = New-ReleaseCandidateRecord `
        -SourceCommit ("a" * 40) `
        -Version "0.2.0" `
        -ExpectedTag "v0.2.0" `
        -PackageName "vocekit-qt6-portable" `
        -UpdateFeedUrl "https://api.github.com/repos/example/vocekit/releases/latest" `
        -ReleaseBaseUrl "https://github.com/example/vocekit/releases/download" `
        -ReleasePageBaseUrl "https://github.com/example/vocekit/releases/tag" `
        -ArchivePath $archivePath `
        -SidecarPath $sidecarPath `
        -ManifestPath $manifestPath `
        -Signer $signer `
        -BinaryProvenance $testProvenance `
        -RuntimeHelpers $runtimeHelpers

    if ($record.source_commit -cne ("a" * 40) -or
        $record.version -cne "0.2.0" -or
        $record.tag -cne "v0.2.0" -or
        [int]$record.binary_provenance.schema_version -ne 3 -or
        $record.binary_provenance.source_commit -cne ("a" * 40) -or
        $record.binary_provenance.source_tree_clean -isnot [bool] -or
        -not [bool]$record.binary_provenance.source_tree_clean -or
        $record.binary_provenance.configuration -cne "release" -or
        @($record.runtime_helpers).Count -ne 3 -or
        $record.binary_provenance.update_feed_url -cne $testProvenance.update_feed_url -or
        $record.urls.update_feed_url -cne "https://api.github.com/repos/example/vocekit/releases/latest" -or
        $record.urls.release_base_url -cne "https://github.com/example/vocekit/releases/download" -or
        $record.urls.release_page_base_url -cne "https://github.com/example/vocekit/releases/tag" -or
        $record.archive.bytes -ne (Get-Item -LiteralPath $archivePath).Length -or
        $record.archive.sha256 -cne (Get-FileHash -LiteralPath $archivePath -Algorithm SHA256).Hash.ToLowerInvariant() -or
        $record.sidecar.sha256 -cne (Get-FileHash -LiteralPath $sidecarPath -Algorithm SHA256).Hash.ToLowerInvariant() -or
        $record.manifest.sha256 -cne (Get-FileHash -LiteralPath $manifestPath -Algorithm SHA256).Hash.ToLowerInvariant() -or
        $record.signer.subject -cne $signer.Subject -or
        $record.signer.thumbprint -cne $signer.Thumbprint) {
        throw "The release candidate record did not bind the source, release, artifacts, and signer."
    }
} finally {
    if ([IO.Directory]::Exists($recordRoot)) {
        [IO.Directory]::Delete($recordRoot, $true)
    }
}

$privacyRoot = Join-Path ([IO.Path]::GetTempPath()) ("vocekit-package-compatibility-test-" + [Guid]::NewGuid().ToString("N"))
try {
    [void][IO.Directory]::CreateDirectory((Join-Path $privacyRoot "config"))
    [IO.File]::WriteAllText(
        (Join-Path $privacyRoot "config\secrets.json"),
        '{"api_key":"","custom_models":[]}',
        [Text.UTF8Encoding]::new($false)
    )
    & $packageScript -ValidationOnly privacy -ValidationPath $privacyRoot

    $outsideOutputWasRejected = $false
    try {
        & $packageScript -OutputDirectory $privacyRoot
    } catch {
        $outsideOutputWasRejected = $_.Exception.Message -match "project dist directory"
    }
    if (-not $outsideOutputWasRejected) {
        throw "package-test accepted an output directory outside the project dist tree."
    }
} finally {
    if ([IO.Directory]::Exists($privacyRoot)) {
        [IO.Directory]::Delete($privacyRoot, $true)
    }
}

$manifestRoot = Join-Path ([IO.Path]::GetTempPath()) ("vocekit-manifest-compatibility-test-" + [Guid]::NewGuid().ToString("N"))
try {
    [void][IO.Directory]::CreateDirectory($manifestRoot)
    $archivePath = Join-Path $manifestRoot "vocekit-qt6-portable.zip"
    $manifestPath = Join-Path $manifestRoot "update-manifest.json"
    [IO.File]::WriteAllText($archivePath, "archive")
    foreach ($iteration in 1..2) {
        & $manifestScript `
            -ArchivePath $archivePath `
            -OutputPath $manifestPath `
            -ReleaseBaseUrl "https://github.com/example/vocekit/releases/download" `
            -ReleasePageBaseUrl "https://github.com/example/vocekit/releases/tag"
    }
    if (-not (Test-Path -LiteralPath $manifestPath -PathType Leaf) -or
        -not (Test-Path -LiteralPath "$archivePath.sha256" -PathType Leaf)) {
        throw "The update manifest script did not preserve its default overwrite-compatible outputs."
    }
    $manifestHashBefore = (Get-FileHash -LiteralPath $manifestPath -Algorithm SHA256).Hash
    $sidecarHashBefore = (Get-FileHash -LiteralPath "$archivePath.sha256" -Algorithm SHA256).Hash
    $formalOverwriteWasRejected = $false
    try {
        & $manifestScript `
            -ArchivePath $archivePath `
            -OutputPath $manifestPath `
            -ReleaseBaseUrl "https://github.com/example/vocekit/releases/download" `
            -ReleasePageBaseUrl "https://github.com/example/vocekit/releases/tag" `
            -FailIfOutputExists
    } catch {
        $formalOverwriteWasRejected = $_.Exception.Message -match "refusing to overwrite"
    }
    if (-not $formalOverwriteWasRejected -or
        (Get-FileHash -LiteralPath $manifestPath -Algorithm SHA256).Hash -cne $manifestHashBefore -or
        (Get-FileHash -LiteralPath "$archivePath.sha256" -Algorithm SHA256).Hash -cne $sidecarHashBefore) {
        throw "Formal manifest publication overwrote an existing immutable output."
    }
} finally {
    if ([IO.Directory]::Exists($manifestRoot)) {
        [IO.Directory]::Delete($manifestRoot, $true)
    }
}

Write-Host "Release candidate script tests: PASS"
