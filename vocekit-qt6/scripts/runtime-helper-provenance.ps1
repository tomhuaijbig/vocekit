Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"
. (Join-Path $PSScriptRoot "build-provenance.ps1")

function Get-RuntimeHelperRepositoryState {
    param(
        [Parameter(Mandatory = $true)][string]$RepositoryRoot,
        [Parameter(Mandatory = $true)][string]$ProjectRoot
    )

    $repositoryFull = [IO.Path]::GetFullPath($RepositoryRoot).TrimEnd(
        [IO.Path]::DirectorySeparatorChar,
        [IO.Path]::AltDirectorySeparatorChar
    )
    $projectFull = [IO.Path]::GetFullPath($ProjectRoot).TrimEnd(
        [IO.Path]::DirectorySeparatorChar,
        [IO.Path]::AltDirectorySeparatorChar
    )
    $expectedProjectFull = [IO.Path]::GetFullPath((Join-Path $repositoryFull "vocekit-qt6")).TrimEnd(
        [IO.Path]::DirectorySeparatorChar,
        [IO.Path]::AltDirectorySeparatorChar
    )
    if (-not (Test-Path -LiteralPath $repositoryFull -PathType Container) -or
        -not (Test-Path -LiteralPath $projectFull -PathType Container) -or
        -not [string]::Equals($projectFull, $expectedProjectFull, [StringComparison]::OrdinalIgnoreCase)) {
        throw "Runtime helpers must be built from the fixed vocekit-qt6 project beneath the requested repository root."
    }
    foreach ($path in @($repositoryFull, $projectFull)) {
        $item = Get-Item -LiteralPath $path -Force
        if (($item.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) {
            throw "Runtime helper provenance root must not be a reparse point: $path"
        }
    }

    Assert-TrustedGitRepository -RepositoryRoot $repositoryFull
    $sourceCommitResult = Invoke-TrustedGit `
        -RepositoryRoot $repositoryFull `
        -Arguments @("rev-parse", "HEAD")
    $sourceCommit = (@($sourceCommitResult.output) -join "").Trim().ToLowerInvariant()
    if ($sourceCommit -notmatch '^[0-9a-f]{40}([0-9a-f]{24})?$') {
        throw "Unable to resolve a complete Git commit for runtime helper provenance."
    }
    $sourceTreeClean = Get-GitSourceTreeClean -RepositoryRoot $repositoryFull

    return [PSCustomObject]@{
        schema_version = 1
        source_commit = $sourceCommit
        source_tree_clean = [bool]$sourceTreeClean
        configuration = "Release"
    }
}

function Assert-RuntimeHelperRepositoryStateUnchanged {
    param(
        [Parameter(Mandatory = $true)]$Before,
        [Parameter(Mandatory = $true)][string]$RepositoryRoot,
        [Parameter(Mandatory = $true)][string]$ProjectRoot
    )

    $after = Get-RuntimeHelperRepositoryState `
        -RepositoryRoot $RepositoryRoot `
        -ProjectRoot $ProjectRoot
    if ([string]$after.source_commit -cne [string]$Before.source_commit -or
        [bool]$after.source_tree_clean -ne [bool]$Before.source_tree_clean -or
        [string]$after.configuration -cne [string]$Before.configuration) {
        throw "The Git repository changed while runtime helpers were being built."
    }
    return $after
}

function Assert-RuntimeHelperExpectedState {
    param(
        [Parameter(Mandatory = $true)]$Actual,
        [AllowEmptyString()][string]$ExpectedSourceCommit = "",
        [Nullable[bool]]$ExpectedSourceTreeClean = $null
    )

    $hasCommit = -not [string]::IsNullOrWhiteSpace($ExpectedSourceCommit)
    $hasClean = $null -ne $ExpectedSourceTreeClean
    if ($hasCommit -ne $hasClean) {
        throw "ExpectedSourceCommit and ExpectedSourceTreeClean must be supplied together."
    }
    if ($hasCommit) {
        $normalizedCommit = $ExpectedSourceCommit.Trim().ToLowerInvariant()
        if ($normalizedCommit -notmatch '^[0-9a-f]{40}([0-9a-f]{24})?$' -or
            [string]$Actual.source_commit -cne $normalizedCommit -or
            [bool]$Actual.source_tree_clean -ne [bool]$ExpectedSourceTreeClean) {
            throw "Runtime helper build state no longer matches the orchestrator-approved Git state."
        }
    }
}

function Remove-RuntimeHelperBuildOutputs {
    param(
        [Parameter(Mandatory = $true)][string]$ProjectRoot,
        [string[]]$HelperNames = @(
            "vocekit-windows-ocr",
            "vocekit-rapidocr",
            "vocekit-windows-speech"
        )
    )

    $projectFull = [IO.Path]::GetFullPath($ProjectRoot).TrimEnd(
        [IO.Path]::DirectorySeparatorChar,
        [IO.Path]::AltDirectorySeparatorChar
    )
    if ([IO.Path]::GetFileName($projectFull) -cne "vocekit-qt6") {
        throw "Refusing to invalidate runtime helper outputs outside the vocekit-qt6 project."
    }
    $binFull = [IO.Path]::GetFullPath((Join-Path $projectFull "helpers\bin"))
    $expectedBinFull = [IO.Path]::GetFullPath((Join-Path $projectFull "helpers\bin"))
    if ($binFull -cne $expectedBinFull) {
        throw "Refusing to use an unsafe runtime helper output directory."
    }
    if (Test-Path -LiteralPath $binFull -PathType Container) {
        $binItem = Get-Item -LiteralPath $binFull -Force
        if (($binItem.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) {
            throw "Refusing to invalidate runtime helper outputs through a reparse point: $binFull"
        }
    }

    $allowed = @(
        "vocekit-windows-ocr",
        "vocekit-rapidocr",
        "vocekit-windows-speech"
    )
    $failures = New-Object Collections.Generic.List[string]
    foreach ($helperName in $HelperNames) {
        if ($allowed -cnotcontains $helperName) {
            throw "Unknown runtime helper output name: $helperName"
        }
        foreach ($extension in @(".exe", ".pdb")) {
            $path = Join-Path $binFull ($helperName + $extension)
            try {
                if ([IO.File]::Exists($path)) {
                    [IO.File]::SetAttributes($path, [IO.FileAttributes]::Normal)
                    [IO.File]::Delete($path)
                }
            } catch {
                $failures.Add("$path ($($_.Exception.Message))")
            }
        }
    }
    if ($failures.Count -gt 0) {
        throw "Runtime helper outputs could not be invalidated: $($failures -join '; ')"
    }
}

function Assert-RuntimeHelperDirectoryTreeSafe {
    param([Parameter(Mandatory = $true)][string]$Path)

    if (-not (Test-Path -LiteralPath $Path)) {
        return
    }
    $item = Get-Item -LiteralPath $Path -Force
    if (-not $item.PSIsContainer) {
        throw "Runtime helper build directory is not a directory: $Path"
    }
    if (($item.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) {
        throw "Runtime helper build directory must not be a reparse point: $Path"
    }
    foreach ($child in Get-ChildItem -LiteralPath $Path -Force) {
        if (($child.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) {
            throw "Runtime helper build directory contains a reparse point: $($child.FullName)"
        }
        if ($child.PSIsContainer) {
            Assert-RuntimeHelperDirectoryTreeSafe -Path $child.FullName
        }
    }
}

function Remove-RuntimeHelperAllowedDirectory {
    param(
        [Parameter(Mandatory = $true)][string]$ProjectRoot,
        [Parameter(Mandatory = $true)][string]$RelativePath,
        [Parameter(Mandatory = $true)][string[]]$AllowedRelativePaths
    )

    $projectFull = [IO.Path]::GetFullPath($ProjectRoot).TrimEnd(
        [IO.Path]::DirectorySeparatorChar,
        [IO.Path]::AltDirectorySeparatorChar
    )
    if ([IO.Path]::GetFileName($projectFull) -cne "vocekit-qt6") {
        throw "Refusing to reset runtime helper directories outside the vocekit-qt6 project."
    }
    if (-not (Test-Path -LiteralPath $projectFull -PathType Container)) {
        throw "Runtime helper project root is missing: $projectFull"
    }
    $projectItem = Get-Item -LiteralPath $projectFull -Force
    if (($projectItem.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) {
        throw "Runtime helper project root must not be a reparse point: $projectFull"
    }
    $normalizedRelative = $RelativePath.Replace("/", "\").Trim("\")
    if ($AllowedRelativePaths -cnotcontains $normalizedRelative) {
        throw "Runtime helper build directory is not on the reset allowlist: $RelativePath"
    }
    $targetFull = [IO.Path]::GetFullPath((Join-Path $projectFull $normalizedRelative))
    $projectPrefix = $projectFull + [IO.Path]::DirectorySeparatorChar
    if (-not $targetFull.StartsWith($projectPrefix, [StringComparison]::OrdinalIgnoreCase)) {
        throw "Runtime helper build directory escaped the fixed project: $targetFull"
    }

    $cursor = $projectFull
    foreach ($segment in $normalizedRelative.Split("\")) {
        $cursor = Join-Path $cursor $segment
        if (Test-Path -LiteralPath $cursor) {
            $cursorItem = Get-Item -LiteralPath $cursor -Force
            if (-not $cursorItem.PSIsContainer) {
                throw "Runtime helper build directory path contains a file: $cursor"
            }
            if (($cursorItem.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) {
                throw "Runtime helper build directory path contains a reparse point: $cursor"
            }
        }
    }
    if (Test-Path -LiteralPath $targetFull) {
        Assert-RuntimeHelperDirectoryTreeSafe -Path $targetFull
        Remove-Item -LiteralPath $targetFull -Recurse -Force
    }
}

function Reset-RuntimeHelperIntermediateOutputs {
    param(
        [Parameter(Mandatory = $true)][string]$ProjectRoot,
        [Parameter(Mandatory = $true)]
        [ValidateSet(
            "vocekit-windows-ocr",
            "vocekit-rapidocr",
            "vocekit-windows-speech"
        )]
        [string[]]$HelperNames
    )

    $allowlist = @(
        "helpers\windows_ocr\obj\Release",
        "helpers\rapidocr\obj\Release",
        "helpers\windows_speech\obj\Release",
        "helpers\windows_speech\obj\x64\Release"
    )
    $pathsByHelper = @{
        "vocekit-windows-ocr" = @("helpers\windows_ocr\obj\Release")
        "vocekit-rapidocr" = @("helpers\rapidocr\obj\Release")
        "vocekit-windows-speech" = @(
            "helpers\windows_speech\obj\Release",
            "helpers\windows_speech\obj\x64\Release"
        )
    }
    foreach ($helperName in $HelperNames) {
        foreach ($relativePath in $pathsByHelper[$helperName]) {
            Remove-RuntimeHelperAllowedDirectory `
                -ProjectRoot $ProjectRoot `
                -RelativePath $relativePath `
                -AllowedRelativePaths $allowlist
        }
    }
}

function Reset-RapidOcrModelOutputDirectory {
    param([Parameter(Mandatory = $true)][string]$ProjectRoot)

    $relativePath = "helpers\bin\models"
    Remove-RuntimeHelperAllowedDirectory `
        -ProjectRoot $ProjectRoot `
        -RelativePath $relativePath `
        -AllowedRelativePaths @($relativePath)
    $modelsPath = [IO.Path]::GetFullPath((Join-Path $ProjectRoot $relativePath))
    New-Item -ItemType Directory -Path $modelsPath | Out-Null
    return $modelsPath
}

function Assert-RapidOcrModelOutputDirectory {
    param(
        [Parameter(Mandatory = $true)][string]$ModelsPath,
        [Parameter(Mandatory = $true)][Collections.IDictionary]$ExpectedSha256
    )

    $modelsFull = [IO.Path]::GetFullPath($ModelsPath)
    Assert-RuntimeHelperDirectoryTreeSafe -Path $modelsFull
    $entries = @(Get-ChildItem -LiteralPath $modelsFull -Force)
    $expectedNames = @($ExpectedSha256.Keys | Sort-Object)
    $actualNames = @($entries | ForEach-Object { $_.Name } | Sort-Object)
    if ($entries.Count -ne $expectedNames.Count -or
        ($actualNames -join "`n") -cne ($expectedNames -join "`n")) {
        throw "RapidOCR model output must contain exactly the pinned model files and no extras."
    }
    foreach ($entry in $entries) {
        if ($entry.PSIsContainer -or
            ($entry.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) {
            throw "RapidOCR model output contains a non-regular file: $($entry.FullName)"
        }
        $actualHash = (Get-FileHash -LiteralPath $entry.FullName -Algorithm SHA256).Hash
        if ($actualHash -cne [string]$ExpectedSha256[$entry.Name]) {
            throw "RapidOCR model output checksum mismatch: $($entry.Name)"
        }
    }
}

function Get-RuntimeHelperProvenanceProperty {
    param(
        [Parameter(Mandatory = $true)]$Provenance,
        [Parameter(Mandatory = $true)][string]$Name
    )

    $property = $Provenance.PSObject.Properties[$Name]
    if ($null -eq $property) {
        throw "Runtime helper build provenance is missing '$Name'."
    }
    return $property.Value
}

function Assert-RuntimeHelperBuildProvenanceObject {
    param(
        [Parameter(Mandatory = $true)]$Provenance,
        [Parameter(Mandatory = $true)]
        [ValidateSet(
            "vocekit-windows-ocr",
            "vocekit-rapidocr",
            "vocekit-windows-speech"
        )]
        [string]$ExpectedHelperName,
        [Parameter(Mandatory = $true)][string]$ExpectedSourceCommit,
        [Parameter(Mandatory = $true)][bool]$ExpectedSourceTreeClean,
        [ValidateSet("Release")][string]$ExpectedConfiguration = "Release"
    )

    $schemaVersion = Get-RuntimeHelperProvenanceProperty -Provenance $Provenance -Name "schema_version"
    $kind = [string](Get-RuntimeHelperProvenanceProperty -Provenance $Provenance -Name "kind")
    $helperName = [string](Get-RuntimeHelperProvenanceProperty -Provenance $Provenance -Name "helper_name")
    $sourceCommit = [string](Get-RuntimeHelperProvenanceProperty -Provenance $Provenance -Name "source_commit")
    $sourceTreeClean = Get-RuntimeHelperProvenanceProperty -Provenance $Provenance -Name "source_tree_clean"
    $configuration = [string](Get-RuntimeHelperProvenanceProperty -Provenance $Provenance -Name "configuration")
    $normalizedExpectedCommit = $ExpectedSourceCommit.Trim().ToLowerInvariant()

    if ($schemaVersion -isnot [int] -and $schemaVersion -isnot [long]) {
        throw "Runtime helper build provenance schema_version must be an integer."
    }
    if ([int64]$schemaVersion -ne 1 -or
        $kind -cne "vocekit-runtime-helper-build-provenance") {
        throw "Runtime helper build provenance schema is unsupported."
    }
    if ($helperName -cne $ExpectedHelperName) {
        throw "Runtime helper build provenance helper_name does not match the expected executable."
    }
    if ($normalizedExpectedCommit -notmatch '^[0-9a-f]{40}([0-9a-f]{24})?$' -or
        $sourceCommit -cne $normalizedExpectedCommit) {
        throw "Runtime helper build provenance source_commit does not match the approved source commit."
    }
    if ($sourceTreeClean -isnot [bool]) {
        throw "Runtime helper build provenance source_tree_clean must be a JSON boolean."
    }
    if ([bool]$sourceTreeClean -ne $ExpectedSourceTreeClean) {
        throw "Runtime helper build provenance source_tree_clean does not match the required value."
    }
    if ($configuration -cne $ExpectedConfiguration) {
        throw "Runtime helper build provenance configuration must be Release."
    }
}

function Get-RuntimeHelperExecutableProvenance {
    param(
        [Parameter(Mandatory = $true)][string]$ExecutablePath,
        [Parameter(Mandatory = $true)]
        [ValidateSet(
            "vocekit-windows-ocr",
            "vocekit-rapidocr",
            "vocekit-windows-speech"
        )]
        [string]$ExpectedHelperName,
        [Parameter(Mandatory = $true)][string]$ExpectedSourceCommit,
        [Parameter(Mandatory = $true)][bool]$ExpectedSourceTreeClean,
        [ValidateSet("Release")][string]$ExpectedConfiguration = "Release",
        [ValidateRange(1000, 60000)][int]$TimeoutMs = 10000
    )

    $executableFull = [IO.Path]::GetFullPath($ExecutablePath)
    if (-not (Test-Path -LiteralPath $executableFull -PathType Leaf)) {
        throw "Runtime helper executable is missing: $executableFull"
    }
    $executableItem = Get-Item -LiteralPath $executableFull -Force
    if (($executableItem.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) {
        throw "Runtime helper executable must not be a reparse point: $executableFull"
    }
    $startInfo = New-Object Diagnostics.ProcessStartInfo
    $startInfo.FileName = $executableFull
    $startInfo.Arguments = "--build-provenance-json"
    $startInfo.WorkingDirectory = Split-Path -Parent $executableFull
    $startInfo.UseShellExecute = $false
    $startInfo.CreateNoWindow = $true
    $startInfo.RedirectStandardOutput = $true
    $startInfo.RedirectStandardError = $true
    $process = New-Object Diagnostics.Process
    $process.StartInfo = $startInfo
    try {
        if (-not $process.Start()) {
            throw "Failed to start the runtime helper provenance probe."
        }
        $stdoutTask = $process.StandardOutput.ReadToEndAsync()
        $stderrTask = $process.StandardError.ReadToEndAsync()
        if (-not $process.WaitForExit($TimeoutMs)) {
            try { $process.Kill() } catch { }
            try { $process.WaitForExit() } catch { }
            throw "Runtime helper provenance probe timed out after $TimeoutMs ms."
        }
        $process.WaitForExit()
        $stdout = $stdoutTask.Result
        $stderr = $stderrTask.Result
        if ($process.ExitCode -ne 0) {
            throw "Runtime helper provenance probe exited with code $($process.ExitCode): $stderr"
        }
        $lines = @($stdout -split '\r?\n' | Where-Object { -not [string]::IsNullOrWhiteSpace($_) })
        if ($lines.Count -ne 1) {
            throw "Runtime helper provenance probe must emit exactly one non-empty JSON line."
        }
        try {
            $provenance = $lines[0] | ConvertFrom-Json -ErrorAction Stop
        } catch {
            throw "Runtime helper provenance probe emitted invalid JSON: $($_.Exception.Message)"
        }
        Assert-RuntimeHelperBuildProvenanceObject `
            -Provenance $provenance `
            -ExpectedHelperName $ExpectedHelperName `
            -ExpectedSourceCommit $ExpectedSourceCommit `
            -ExpectedSourceTreeClean $ExpectedSourceTreeClean `
            -ExpectedConfiguration $ExpectedConfiguration
        return $provenance
    } finally {
        $process.Dispose()
    }
}

function New-WindowsSpeechBuildProvenanceSource {
    param(
        [Parameter(Mandatory = $true)][string]$Path,
        [Parameter(Mandatory = $true)]$BuildState
    )

    $sourceCommit = [string]$BuildState.source_commit
    if ($sourceCommit -notmatch '^[0-9a-f]{40}([0-9a-f]{24})?$' -or
        [string]$BuildState.configuration -cne "Release") {
        throw "Cannot generate Windows speech provenance from an invalid build state."
    }
    $cleanLiteral = if ([bool]$BuildState.source_tree_clean) { "true" } else { "false" }
    $source = @"
namespace VoceKit.WindowsSpeech
{
    internal static class BuildProvenance
    {
        internal const int SchemaVersion = 1;
        internal const string Kind = "vocekit-runtime-helper-build-provenance";
        internal const string HelperName = "vocekit-windows-speech";
        internal const string SourceCommit = "$sourceCommit";
        internal const bool SourceTreeClean = $cleanLiteral;
        internal const string Configuration = "Release";
    }
}
"@
    $fullPath = [IO.Path]::GetFullPath($Path)
    $parent = Split-Path -Parent $fullPath
    if (-not (Test-Path -LiteralPath $parent -PathType Container)) {
        throw "Windows speech provenance source parent is missing: $parent"
    }
    $utf8WithoutBom = New-Object Text.UTF8Encoding($false)
    [IO.File]::WriteAllText($fullPath, $source, $utf8WithoutBom)
    return $fullPath
}

function Remove-RuntimeHelperTemporaryDirectory {
    param([Parameter(Mandatory = $true)][string]$Path)

    $temporaryRoot = [IO.Path]::GetFullPath([IO.Path]::GetTempPath()).TrimEnd(
        [IO.Path]::DirectorySeparatorChar,
        [IO.Path]::AltDirectorySeparatorChar
    )
    $fullPath = [IO.Path]::GetFullPath($Path).TrimEnd(
        [IO.Path]::DirectorySeparatorChar,
        [IO.Path]::AltDirectorySeparatorChar
    )
    $parent = Split-Path -Parent $fullPath
    $leaf = Split-Path -Leaf $fullPath
    if ($parent -cne $temporaryRoot -or
        $leaf -notmatch '^vocekit-windows-speech-provenance-[0-9a-f]{32}$') {
        throw "Refusing to remove an unsafe Windows speech provenance directory: $fullPath"
    }
    if (-not (Test-Path -LiteralPath $fullPath)) {
        return
    }
    $item = Get-Item -LiteralPath $fullPath -Force
    if (($item.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) {
        throw "Refusing to remove a Windows speech provenance directory through a reparse point: $fullPath"
    }
    Remove-Item -LiteralPath $fullPath -Recurse -Force
}
