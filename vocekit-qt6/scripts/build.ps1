param(
    [string]$QtBin = "D:\QT66666\6.11.1\mingw_64\bin",
    [string]$MingwBin = "D:\QT66666\Tools\mingw1310_64\bin",
    [ValidateSet("debug", "release")]
    [string]$Configuration = "debug",
    [ValidateRange(1, 64)]
    [int]$Jobs = 2,
    [string]$UpdateFeedUrl = "",
    [switch]$Run
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$projectRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$repositoryRoot = [IO.Path]::GetFullPath((Join-Path $projectRoot ".."))
$projectFile = Join-Path $projectRoot "vocekit.pro"
$buildRoot = Join-Path $projectRoot ".qt6-build"
$qmake = Join-Path $QtBin "qmake.exe"
$make = Join-Path $MingwBin "mingw32-make.exe"
$compiler = Join-Path $MingwBin "g++.exe"
$versionPath = Join-Path $projectRoot "APP_VERSION"
. (Join-Path $PSScriptRoot "build-provenance.ps1")

foreach ($tool in @($qmake, $make, $compiler, $versionPath)) {
    if (-not (Test-Path -LiteralPath $tool -PathType Leaf)) {
        throw "Missing Qt 6 build tool: $tool"
    }
}

$sourceCommitResult = Invoke-TrustedGit `
    -RepositoryRoot $repositoryRoot `
    -Arguments @("rev-parse", "HEAD")
$sourceCommit = (@($sourceCommitResult.output) -join "").Trim()
if ($sourceCommit -notmatch '^[0-9a-fA-F]{40}([0-9a-fA-F]{24})?$') {
    throw "Unable to resolve the complete Git HEAD for embedded build provenance."
}
$sourceTreeClean = Get-GitSourceTreeClean -RepositoryRoot $repositoryRoot
$sourceTreeCleanDefine = if ($sourceTreeClean) { "1" } else { "0" }

$originalPath = $env:PATH
try {
    $env:PATH = "$QtBin;$MingwBin;$env:PATH"
    $qtVersion = (& $qmake -query QT_VERSION).Trim()
    if (-not $qtVersion.StartsWith("6.")) {
        throw "VoceKit requires Qt 6; qmake reports Qt $qtVersion"
    }

    $qmakeArguments = @(
        $projectFile,
        "-spec",
        "win32-g++",
        "CONFIG+=$Configuration",
        "VOCEKIT_SOURCE_COMMIT=$sourceCommit",
        "VOCEKIT_SOURCE_TREE_CLEAN=$sourceTreeCleanDefine",
        "VOCEKIT_BUILD_CONFIGURATION=$Configuration"
    )
    if (-not [string]::IsNullOrWhiteSpace($UpdateFeedUrl)) {
        $feedUri = [Uri]$UpdateFeedUrl
        if (-not $feedUri.IsAbsoluteUri -or
            $feedUri.Scheme -ne "https" -or
            [string]::IsNullOrWhiteSpace($feedUri.Host) -or
            -not [string]::IsNullOrWhiteSpace($feedUri.UserInfo)) {
            throw "UpdateFeedUrl must be an absolute HTTPS URL without embedded credentials."
        }
        $qmakeArguments += "VOCEKIT_UPDATE_FEED_URL=$UpdateFeedUrl"
    }

    # qmake updates compiler definitions in the generated Makefiles, but make
    # does not treat a changed command line as an object dependency. The top-
    # level qmake Makefiles and their debug/release directories are also one
    # state unit. A fingerprint change therefore resets the complete shadow
    # build tree before qmake runs. Additionally, every clean Release build is
    # a formal trust boundary: Git ignores .o/exe files, so neither a clean
    # status nor an unchanged fingerprint proves those bytes came from HEAD.
    # Such a build must start from an absent shadow tree. Dirty developer builds
    # may remain incremental. Do not rely on qmake's clean target: some Windows
    # kits generate ignored `rm` commands without shipping rm.exe.
    $fullBuildRoot = [IO.Path]::GetFullPath($buildRoot)
    $expectedBuildRoot = [IO.Path]::GetFullPath((Join-Path $projectRoot ".qt6-build"))
    if ($fullBuildRoot -cne $expectedBuildRoot -or
        [IO.Path]::GetFileName($fullBuildRoot) -cne ".qt6-build" -or
        [IO.Path]::GetFullPath((Split-Path -Parent $fullBuildRoot)) -cne
            [IO.Path]::GetFullPath($projectRoot)) {
        throw "Refusing to use an unsafe Qt shadow build path: $fullBuildRoot"
    }
    if ([IO.Directory]::Exists($fullBuildRoot)) {
        $buildRootItem = Get-Item -LiteralPath $fullBuildRoot -Force
        if (($buildRootItem.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) {
            throw "Refusing to use a reparse-point Qt shadow build path: $fullBuildRoot"
        }
    }

    $fingerprintPath = Join-Path $fullBuildRoot ".build-fingerprint.json"
    $fingerprint = [ordered]@{
        schema_version = 2
        configuration = $Configuration
        source_commit = $sourceCommit.ToLowerInvariant()
        source_tree_clean = [bool]$sourceTreeClean
        app_version_sha256 = (Get-FileHash -LiteralPath $versionPath -Algorithm SHA256).Hash.ToLowerInvariant()
        project_file_sha256 = (Get-FileHash -LiteralPath $projectFile -Algorithm SHA256).Hash.ToLowerInvariant()
        update_feed_url = $UpdateFeedUrl
        qt_version = $qtVersion
        qmake_sha256 = (Get-FileHash -LiteralPath $qmake -Algorithm SHA256).Hash.ToLowerInvariant()
        compiler_sha256 = (Get-FileHash -LiteralPath $compiler -Algorithm SHA256).Hash.ToLowerInvariant()
        make_sha256 = (Get-FileHash -LiteralPath $make -Algorithm SHA256).Hash.ToLowerInvariant()
    } | ConvertTo-Json -Compress
    $previousFingerprint = if (Test-Path -LiteralPath $fingerprintPath -PathType Leaf) {
        Get-Content -LiteralPath $fingerprintPath -Raw -Encoding UTF8
    } else {
        ""
    }
    $fingerprintChanged = $previousFingerprint -cne $fingerprint
    $resetBuildTree = Test-ShouldResetQtBuildTree `
        -Configuration $Configuration `
        -SourceTreeClean $sourceTreeClean `
        -FingerprintChanged $fingerprintChanged
    if ($resetBuildTree) {
        if ([IO.Directory]::Exists($fullBuildRoot)) {
            [IO.Directory]::Delete($fullBuildRoot, $true)
        }
    }
    [void][IO.Directory]::CreateDirectory($buildRoot)

    Push-Location $buildRoot
    try {
        & $qmake @qmakeArguments
        if ($LASTEXITCODE -ne 0) {
            throw "Qt 6 qmake failed with exit code $LASTEXITCODE"
        }

        & $make "-j$Jobs"
        if ($LASTEXITCODE -ne 0) {
            throw "Qt 6 build failed with exit code $LASTEXITCODE"
        }
        if ($sourceTreeClean -and
            -not (Get-GitSourceTreeClean -RepositoryRoot $repositoryRoot)) {
            Invalidate-CleanProvenanceBuildOutput `
                -BuildRoot $buildRoot `
                -Configuration $Configuration
            throw "Git source tree became dirty during a clean provenance build; the newly linked executable and fingerprint were deleted, and the next build must reset the shadow tree."
        }
        if ($resetBuildTree) {
            $temporaryFingerprintPath = "$fingerprintPath.$([Guid]::NewGuid().ToString('N')).tmp"
            try {
                Set-Content -LiteralPath $temporaryFingerprintPath -Value $fingerprint -Encoding UTF8 -NoNewline
                [IO.File]::Move($temporaryFingerprintPath, $fingerprintPath)
            } finally {
                Remove-Item -LiteralPath $temporaryFingerprintPath -Force -ErrorAction SilentlyContinue
            }
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
