[CmdletBinding(DefaultParameterSetName = "Package")]
param(
    [Parameter(ParameterSetName = "Package")]
    [Parameter(ParameterSetName = "Validation")]
    [string]$PackageName = "vocekit-test",
    [Parameter(Mandatory = $true, ParameterSetName = "Validation")]
    [ValidateSet("", "privacy", "archive")]
    [string]$ValidationOnly = "",
    [Parameter(ParameterSetName = "Validation")]
    [string]$ValidationPath = "",
    [Parameter(ParameterSetName = "Validation")]
    [string]$ValidationHelperPath = "",
    [Parameter(ParameterSetName = "Package")]
    [ValidateSet("", "after-directory-move", "after-first-backup-cleanup", "formal-zip-race-after-directory")]
    [string]$PublishFailureInjection = "",
    [Parameter(ParameterSetName = "Package")]
    [switch]$RequireSignedBinaries,
    [Parameter(ParameterSetName = "Package")]
    [string]$ReleaseBaseUrl = "",
    [Parameter(ParameterSetName = "Package")]
    [string]$ReleasePageBaseUrl = "",
    [Parameter(ParameterSetName = "Package")]
    [string]$OutputDirectory = "",
    [Parameter(ParameterSetName = "Package")]
    [switch]$FailIfOutputExists,
    [Parameter(Mandatory = $true, ParameterSetName = "DecisionTest")]
    [switch]$DecisionTestMode
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"
Add-Type -AssemblyName System.IO.Compression
Add-Type -AssemblyName System.IO.Compression.FileSystem
. (Join-Path $PSScriptRoot "release-path-safety.ps1")

$projectRoot = [IO.Path]::GetFullPath((Split-Path -Parent $PSScriptRoot))
$releaseDir = Join-Path $projectRoot ".qt6-deploy"
$defaultDistDir = [IO.Path]::GetFullPath((Join-Path $projectRoot "dist"))
$distDir = if ([string]::IsNullOrWhiteSpace($OutputDirectory)) {
    $defaultDistDir
} else {
    [IO.Path]::GetFullPath($OutputDirectory)
}
$defaultDistPrefix = $defaultDistDir.TrimEnd("\") + "\"
if (-not [string]::Equals($distDir, $defaultDistDir, [StringComparison]::OrdinalIgnoreCase) -and
    -not $distDir.StartsWith($defaultDistPrefix, [StringComparison]::OrdinalIgnoreCase)) {
    throw "OutputDirectory must be the project dist directory or one of its descendants: $distDir"
}
$packageDir = Join-Path $distDir $PackageName
$zipPath = Join-Path $distDir "$PackageName.zip"
$runtimeVerifier = Join-Path $PSScriptRoot "verify-runtime.ps1"
$stagingDir = Join-Path $distDir (".staging-" + [Guid]::NewGuid().ToString("N"))
$temporaryZip = Join-Path $distDir (".archive-" + [Guid]::NewGuid().ToString("N") + ".zip")
$publishId = [Guid]::NewGuid().ToString("N")
$backupPackageDir = Join-Path $distDir (".backup-$publishId-package")
$backupZipPath = Join-Path $distDir (".backup-$publishId-package.zip")

function Assert-ChildPath {
    param([string]$BasePath, [string]$TargetPath)
    $baseFull = [IO.Path]::GetFullPath($BasePath).TrimEnd("\") + "\"
    $targetFull = [IO.Path]::GetFullPath($TargetPath)
    if (-not $targetFull.StartsWith($baseFull, [StringComparison]::OrdinalIgnoreCase)) {
        throw "Refusing to modify a path outside the distribution directory: $targetFull"
    }
}

function Get-NormalizedRelativePath {
    param([string]$BasePath, [string]$FullName)
    $baseFull = [IO.Path]::GetFullPath($BasePath).TrimEnd("\")
    return $FullName.Substring($baseFull.Length).TrimStart("\").Replace("\", "/")
}

function Assert-PackagePrivacy {
    param([Parameter(Mandatory = $true)][string]$RootPath)

    $allowedJson = @(
        "config/secrets.json",
        "config/settings.json",
        "updater/update-policy.json"
    )
    $allowedText = @(
        "prompts/asr.txt", "prompts/lexicon.txt", "prompts/qa.txt",
        "prompts/translate.txt", "ocr/rapidocr/models/ppocr_keys_v1.txt"
        , "ocr/rapidocr/license-rapidocronnx.txt"
    )
    $forbiddenExtensions = @(
        ".o", ".obj", ".cpp", ".h", ".cs", ".csproj", ".pro", ".log",
        ".tmp", ".pdb", ".ilk", ".pcm", ".wav", ".mp3", ".m4a", ".aac",
        ".flac", ".ogg", ".png", ".jpg", ".jpeg", ".bmp", ".webp", ".gif",
        ".jsonl", ".bak", ".old"
    )
    $forbiddenSegments = @("records", "history", "logs", "screenshots", "captures", "temp", "tmp", "userdata", "user-data")
    $violations = New-Object Collections.Generic.List[string]
    foreach ($file in Get-ChildItem -LiteralPath $RootPath -Recurse -File) {
        $relative = Get-NormalizedRelativePath -BasePath $RootPath -FullName $file.FullName
        $lower = $relative.ToLowerInvariant()
        $segments = $lower.Split('/')
        if (@($segments | Where-Object { $forbiddenSegments -contains $_ }).Count -gt 0) {
            $violations.Add("forbidden user-data path: $relative")
        }
        $extension = $file.Extension.ToLowerInvariant()
        if ($forbiddenExtensions -contains $extension) {
            $violations.Add("forbidden extension: $relative")
        }
        if ($extension -eq ".json" -and $allowedJson -notcontains $lower) {
            $violations.Add("non-whitelisted JSON: $relative")
        }
        if ($extension -eq ".txt" -and $allowedText -notcontains $lower) {
            $violations.Add("non-whitelisted text: $relative")
        }
        if ($file.Name -in @("secrets.example.json", "settings.example.json", "1.txt")) {
            $violations.Add("development file: $relative")
        }
    }
    if ($violations.Count -gt 0) {
        throw "Package privacy validation failed:`n$($violations -join "`n")"
    }

    $secrets = Get-Content -LiteralPath (Join-Path $RootPath "config\secrets.json") -Raw -Encoding UTF8 | ConvertFrom-Json
    $nonBlankSecrets = @($secrets.PSObject.Properties | Where-Object {
        -not [string]::IsNullOrWhiteSpace([string]$_.Value) -and $_.Name -ne "custom_models"
    })
    if ($nonBlankSecrets.Count -ne 0 -or @($secrets.custom_models).Count -ne 0) {
        throw "The generated package contains non-empty API credentials or custom endpoints."
    }

    $textFiles = @(Get-ChildItem -LiteralPath $RootPath -Recurse -File | Where-Object {
        $_.Extension.ToLowerInvariant() -in @(".json", ".md", ".txt", ".qm")
    })
    $secretSignature = '(?i)(api[_-]?key|api[_-]?secret|access[_-]?token|client[_-]?secret|authorization|bearer)\s*["''=: ]+\s*[A-Za-z0-9_\-]{16,}|sk-[A-Za-z0-9_-]{16,}|AKIA[0-9A-Z]{16}'
    $privateUrlSignature = '(?i)(url|endpoint)\s*["''=: ]+\s*https?://[^\s"'']+'
    $absolutePathSignature = '(?i)(?<![A-Za-z0-9])([A-Z]:\\|\\\\[^\s\\]+\\[^\s\\]+)'
    foreach ($textFile in $textFiles) {
        $content = Get-Content -LiteralPath $textFile.FullName -Raw -Encoding UTF8
        $relativeTextPath = Get-NormalizedRelativePath -BasePath $RootPath -FullName $textFile.FullName
        if ($relativeTextPath -notlike "ocr/rapidocr/LICENSE-*" -and $content -match $secretSignature) {
            throw "Possible credential, token, or private URL found in package text: $($textFile.FullName)"
        }
        if ($relativeTextPath -notlike "ocr/rapidocr/LICENSE-*" -and $content -match $privateUrlSignature) {
            throw "Private endpoint URL found in package text: $($textFile.FullName)"
        }
        if ($content -match $absolutePathSignature) {
            throw "Developer absolute path found in package text: $($textFile.FullName)"
        }
    }
}

function Get-StreamSha256 {
    param([Parameter(Mandatory = $true)][IO.Stream]$Stream)
    $sha = [Security.Cryptography.SHA256]::Create()
    try { return ([BitConverter]::ToString($sha.ComputeHash($Stream))).Replace("-", "") }
    finally { $sha.Dispose() }
}

function Assert-PackageArchive {
    param(
        [Parameter(Mandatory = $true)][string]$ArchivePath,
        [Parameter(Mandatory = $true)][string]$ExpectedHelperPath
    )
    $archive = [IO.Compression.ZipFile]::OpenRead($ArchivePath)
    $extractionRoot = Join-Path ([IO.Path]::GetTempPath()) `
        ("vocekit-archive-validation-" + [Guid]::NewGuid().ToString("N"))
    try {
        $files = @($archive.Entries | Where-Object { -not [string]::IsNullOrEmpty($_.Name) })
        $normalized = @($files | ForEach-Object { $_.FullName.Replace("\", "/").TrimStart("/") })
        foreach ($required in @(
            "vocekit.exe",
            "speech/windows/vocekit-windows-speech.exe",
            ".vocekit-portable",
            "updater/vocekit-update.ps1",
            "updater/update-policy.json"
        )) {
            if (@($normalized | Where-Object { $_ -ieq $required }).Count -ne 1 -or
                @($normalized | Where-Object { $_ -ceq $required }).Count -ne 1) {
                throw "Archive must contain exactly one '$required'."
            }
        }
        foreach ($entryPath in $normalized) {
            if ($entryPath.Contains("../") -or [IO.Path]::IsPathRooted($entryPath)) {
                throw "Archive contains an unsafe entry path: $entryPath"
            }
        }
        $helperIndex = [Array]::IndexOf($normalized, "speech/windows/vocekit-windows-speech.exe")
        $helperEntry = $files[$helperIndex]
        $entryStream = $helperEntry.Open()
        try { $archiveHash = Get-StreamSha256 -Stream $entryStream } finally { $entryStream.Dispose() }
        $packageHash = (Get-FileHash -LiteralPath $ExpectedHelperPath -Algorithm SHA256).Hash
        if ($archiveHash -cne $packageHash) {
            throw "Archive Windows speech helper hash does not match the verified package helper."
        }
        [void][IO.Directory]::CreateDirectory($extractionRoot)
        [IO.Compression.ZipFile]::ExtractToDirectory($ArchivePath, $extractionRoot)
        Assert-PackagePrivacy -RootPath $extractionRoot
    } finally {
        $archive.Dispose()
        if ([IO.Directory]::Exists($extractionRoot)) {
            [IO.Directory]::Delete($extractionRoot, $true)
        }
    }
}

function Publish-FormalPackagePair {
    param(
        [Parameter(Mandatory = $true)][string]$StagingDirectory,
        [Parameter(Mandatory = $true)][string]$TemporaryArchive,
        [Parameter(Mandatory = $true)][string]$PackageDirectory,
        [Parameter(Mandatory = $true)][string]$ArchivePath,
        [ValidateSet("", "formal-zip-race-after-directory")]
        [string]$FailureInjection = ""
    )

    foreach ($formalOutput in @($PackageDirectory, $ArchivePath)) {
        [void](Assert-NoReparsePointsInExistingPathChain `
            -Path $formalOutput `
            -Label "Formal package output")
        if (Test-Path -LiteralPath $formalOutput) {
            throw "Formal package output appeared during publication; refusing to overwrite it: $formalOutput"
        }
    }
    $publishedDirectory = $false
    try {
        [IO.Directory]::Move($StagingDirectory, $PackageDirectory)
        $publishedDirectory = $true
        [void](Assert-NoReparsePointsInExistingPathChain `
            -Path $PackageDirectory `
            -Label "Published package directory")
        if ($FailureInjection -eq "formal-zip-race-after-directory") {
            [IO.File]::WriteAllText($ArchivePath, "race-sentinel")
        }
        # The two-argument overload never overwrites an archive that appears
        # after the initial check.
        [IO.File]::Move($TemporaryArchive, $ArchivePath)
        [void](Assert-NoReparsePointsInExistingPathChain `
            -Path $ArchivePath `
            -Label "Published package archive")
    } catch {
        if ($publishedDirectory -and [IO.Directory]::Exists($PackageDirectory)) {
            [IO.Directory]::Delete($PackageDirectory, $true)
        }
        throw
    }
}

if ($DecisionTestMode) {
    return
}

if (-not [string]::IsNullOrWhiteSpace($ValidationOnly)) {
    if ([string]::IsNullOrWhiteSpace($ValidationPath)) {
        throw "ValidationPath is required with ValidationOnly."
    }
    if ($ValidationOnly -eq "privacy") {
        Assert-PackagePrivacy -RootPath ([IO.Path]::GetFullPath($ValidationPath))
    } else {
        if ([string]::IsNullOrWhiteSpace($ValidationHelperPath)) {
            throw "ValidationHelperPath is required for archive validation."
        }
        Assert-PackageArchive `
            -ArchivePath ([IO.Path]::GetFullPath($ValidationPath)) `
            -ExpectedHelperPath ([IO.Path]::GetFullPath($ValidationHelperPath))
    }
    Write-Host "$ValidationOnly validation passed."
    return
}

if ($FailIfOutputExists) {
    [void](Assert-NoReparsePointsInExistingPathChain `
        -Path $distDir `
        -Label "Formal distribution directory")
    foreach ($formalOutput in @(
        $packageDir,
        $zipPath,
        "$zipPath.sha256",
        (Join-Path $distDir "update-manifest.json")
    )) {
        [void](Assert-NoReparsePointsInExistingPathChain `
            -Path $formalOutput `
            -Label "Formal package output")
        if (Test-Path -LiteralPath $formalOutput) {
            throw "Formal package output already exists; refusing to overwrite it: $formalOutput"
        }
    }
}

Assert-ChildPath -BasePath $distDir -TargetPath $packageDir
Assert-ChildPath -BasePath $distDir -TargetPath $zipPath
Assert-ChildPath -BasePath $distDir -TargetPath $stagingDir
Assert-ChildPath -BasePath $distDir -TargetPath $temporaryZip
Assert-ChildPath -BasePath $distDir -TargetPath $backupPackageDir
Assert-ChildPath -BasePath $distDir -TargetPath $backupZipPath

$releaseExe = Join-Path $releaseDir "vocekit.exe"
if (-not (Test-Path -LiteralPath $releaseExe -PathType Leaf)) {
    throw "Release executable not found. Build and deploy the release version first: $releaseExe"
}
if (-not (Test-Path -LiteralPath $runtimeVerifier -PathType Leaf)) {
    throw "Runtime verifier not found: $runtimeVerifier"
}
& $runtimeVerifier -Configuration release -RuntimeDir $releaseDir
if ($RequireSignedBinaries) {
    $unsigned = New-Object Collections.Generic.List[string]
    foreach ($binary in Get-ChildItem -LiteralPath $releaseDir -File -Recurse | Where-Object {
        $_.Extension.ToLowerInvariant() -in @(".exe", ".dll")
    }) {
        $signature = Get-AuthenticodeSignature -LiteralPath $binary.FullName
        if ($signature.Status -ne [System.Management.Automation.SignatureStatus]::Valid) {
            $unsigned.Add("$($binary.FullName): $($signature.Status)")
        }
    }
    if ($unsigned.Count -gt 0) {
        throw "Release contains unsigned or invalid binaries:`n$($unsigned -join "`n")"
    }
}

# Whitelist only runtime files. Build output and user-created data never become input.
$runtimeRootFiles = @(
    "vocekit.exe", "Qt6Core.dll", "Qt6Gui.dll", "Qt6Widgets.dll", "Qt6Network.dll",
    "Qt6Multimedia.dll", "Qt6Svg.dll", "Qt6WebSockets.dll", "D3Dcompiler_47.dll",
    "opengl32sw.dll", "libgcc_s_seh-1.dll", "libstdc++-6.dll", "libwinpthread-1.dll",
    "avcodec-61.dll", "avformat-61.dll", "avutil-59.dll", "swresample-5.dll", "swscale-8.dll",
    ".vocekit-portable"
)
$runtimeDirectories = @(
    "generic", "iconengines", "imageformats", "multimedia", "networkinformation",
    "platforms", "styles", "tls", "translations", "ocr", "speech", "updater"
)
$createdStage = $false
$backedUpPackage = $false
$backedUpZip = $false
$publishedPackage = $false
$publishedZip = $false
$publishCommitted = $false
try {
    [void][IO.Directory]::CreateDirectory($stagingDir)
    $createdStage = $true
    foreach ($name in $runtimeRootFiles) {
        $source = Join-Path $releaseDir $name
        if (Test-Path -LiteralPath $source -PathType Leaf) {
            Copy-Item -LiteralPath $source -Destination (Join-Path $stagingDir $name) -Force
        }
    }
    foreach ($name in $runtimeDirectories) {
        $source = Join-Path $releaseDir $name
        if (Test-Path -LiteralPath $source -PathType Container) {
            Copy-Item -LiteralPath $source -Destination $stagingDir -Recurse -Force
        }
    }

    foreach ($directory in @("config", "prompts")) {
        [void][IO.Directory]::CreateDirectory((Join-Path $stagingDir $directory))
    }
    Copy-Item -LiteralPath (Join-Path $projectRoot "config\secrets.example.json") -Destination (Join-Path $stagingDir "config\secrets.json") -Force
    Copy-Item -LiteralPath (Join-Path $projectRoot "config\settings.example.json") -Destination (Join-Path $stagingDir "config\settings.json") -Force
    foreach ($name in @("asr.txt", "lexicon.txt", "qa.txt", "translate.txt")) {
        Copy-Item -LiteralPath (Join-Path $projectRoot "prompts\$name") -Destination (Join-Path $stagingDir "prompts\$name") -Force
    }
    Copy-Item -LiteralPath (Join-Path $projectRoot "docs\TESTING.md") -Destination (Join-Path $stagingDir "TESTING.md") -Force
    if (-not $RequireSignedBinaries) {
        Set-Content `
            -LiteralPath (Join-Path $stagingDir "UNSIGNED_TEST_BUILD") `
            -Value "This package is unsigned and is only for controlled testing." `
            -Encoding Ascii
    }

    Assert-PackagePrivacy -RootPath $stagingDir
    & $runtimeVerifier -Configuration release -RuntimeDir $stagingDir
    [IO.Compression.ZipFile]::CreateFromDirectory($stagingDir, $temporaryZip, [IO.Compression.CompressionLevel]::Optimal, $false)
    Assert-PackageArchive -ArchivePath $temporaryZip -ExpectedHelperPath (Join-Path $stagingDir "speech\windows\vocekit-windows-speech.exe")

    try {
        if ($FailIfOutputExists) {
            Publish-FormalPackagePair `
                -StagingDirectory $stagingDir `
                -TemporaryArchive $temporaryZip `
                -PackageDirectory $packageDir `
                -ArchivePath $zipPath `
                -FailureInjection $PublishFailureInjection
            $createdStage = $false
            $publishedPackage = $true
            $publishedZip = $true
            $publishCommitted = $true
        } else {
            if ([IO.Directory]::Exists($packageDir)) {
                [IO.Directory]::Move($packageDir, $backupPackageDir)
                $backedUpPackage = $true
            }
            if ([IO.File]::Exists($zipPath)) {
                [IO.File]::Move($zipPath, $backupZipPath)
                $backedUpZip = $true
            }
            [IO.Directory]::Move($stagingDir, $packageDir)
            $createdStage = $false
            $publishedPackage = $true
            if ($PublishFailureInjection -eq "after-directory-move") {
                throw "Injected package archive publication failure."
            }
            [IO.File]::Move($temporaryZip, $zipPath)
            $publishedZip = $true
            $publishCommitted = $true
        }
    }
    catch {
        if ($publishedZip -and [IO.File]::Exists($zipPath)) {
            [IO.File]::Delete($zipPath)
            $publishedZip = $false
        }
        if ($publishedPackage -and [IO.Directory]::Exists($packageDir)) {
            [IO.Directory]::Delete($packageDir, $true)
            $publishedPackage = $false
        }
        if ($backedUpPackage -and [IO.Directory]::Exists($backupPackageDir)) {
            [IO.Directory]::Move($backupPackageDir, $packageDir)
            $backedUpPackage = $false
        }
        if ($backedUpZip -and [IO.File]::Exists($backupZipPath)) {
            [IO.File]::Move($backupZipPath, $zipPath)
            $backedUpZip = $false
        }
        throw
    }

    # Both new artifacts are committed. Backup cleanup is not part of the
    # transaction: a cleanup failure must never remove or roll back the new pair.
    if ($publishCommitted) {
        try {
            if ($backedUpPackage) {
                [IO.Directory]::Delete($backupPackageDir, $true)
                $backedUpPackage = $false
            }
            if ($PublishFailureInjection -eq "after-first-backup-cleanup") {
                throw "Injected package backup cleanup failure."
            }
            if ($backedUpZip) {
                [IO.File]::Delete($backupZipPath)
                $backedUpZip = $false
            }
        }
        catch {
            Write-Warning "New package pair is committed, but an old package backup could not be removed: $($_.Exception.Message)"
        }
    }

    $packageSize = (Get-ChildItem -LiteralPath $packageDir -Recurse -File | Measure-Object -Property Length -Sum).Sum
    Write-Host "Test package created:"
    Write-Host "  Folder: $packageDir"
    Write-Host "  Archive: $zipPath"
    Write-Host "  Package bytes: $packageSize"
    Write-Host "  Archive bytes: $((Get-Item -LiteralPath $zipPath).Length)"
    if ([string]::IsNullOrWhiteSpace($ReleaseBaseUrl) -xor
        [string]::IsNullOrWhiteSpace($ReleasePageBaseUrl)) {
        throw "ReleaseBaseUrl and ReleasePageBaseUrl must be supplied together."
    }
    if (-not [string]::IsNullOrWhiteSpace($ReleaseBaseUrl)) {
        & (Join-Path $PSScriptRoot "create-update-manifest.ps1") `
            -ArchivePath $zipPath `
            -OutputPath (Join-Path $distDir "update-manifest.json") `
            -ReleaseBaseUrl $ReleaseBaseUrl `
            -ReleasePageBaseUrl $ReleasePageBaseUrl `
            -FailIfOutputExists:$FailIfOutputExists
    } else {
        $staleManifestPath = Join-Path $distDir "update-manifest.json"
        if (Test-Path -LiteralPath $staleManifestPath -PathType Leaf) {
            Remove-Item -LiteralPath $staleManifestPath -Force
        }
        Write-Warning "Internal test package created without an update manifest."
    }
}
finally {
    if ($createdStage -and [IO.Directory]::Exists($stagingDir)) { [IO.Directory]::Delete($stagingDir, $true) }
    if ([IO.File]::Exists($temporaryZip)) { [IO.File]::Delete($temporaryZip) }
    # Backup deletion is intentionally not done here. A backup is deleted only
    # after both new artifacts publish, or moved back by the catch rollback.
}
