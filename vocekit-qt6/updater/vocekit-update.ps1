param(
    [Parameter(Mandatory = $true)]
    [string]$PackagePath,
    [Parameter(Mandatory = $true)]
    [string]$InstallDir,
    [Parameter(Mandatory = $true)]
    [string]$ExpectedSha256,
    [Parameter(Mandatory = $true)]
    [string]$TargetVersion,
    [int]$WaitForProcessId = 0,
    [string]$RestartExecutable = "",
    [string]$StateRoot = "",
    [ValidateSet("", "after-first-copy")]
    [string]$FailureInjection = "",
    [switch]$NoRestart
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"
Add-Type -AssemblyName System.IO.Compression
Add-Type -AssemblyName System.IO.Compression.FileSystem

function Assert-NotBroadPath {
    param([Parameter(Mandatory = $true)][string]$Path)
    $full = [IO.Path]::GetFullPath($Path).TrimEnd([IO.Path]::DirectorySeparatorChar)
    $root = [IO.Path]::GetPathRoot($full).TrimEnd([IO.Path]::DirectorySeparatorChar)
    if ([string]::IsNullOrWhiteSpace($full) -or $full -eq $root) {
        throw "Refusing to update a broad filesystem path: $full"
    }
    return $full
}

function Write-UpdateResult {
    param(
        [Parameter(Mandatory = $true)][string]$Status,
        [Parameter(Mandatory = $true)][string]$Message,
        [string]$Detail = ""
    )
    [void][IO.Directory]::CreateDirectory($script:stateRootFull)
    $result = [ordered]@{
        status = $Status
        version = $TargetVersion
        time = [DateTimeOffset]::Now.ToString("o")
        message = $Message
        detail = $Detail
    } | ConvertTo-Json -Depth 3
    Set-Content -LiteralPath (Join-Path $script:stateRootFull "last-result.json") -Value $result -Encoding UTF8
}

function Get-NormalizedRelativePath {
    param(
        [Parameter(Mandatory = $true)][string]$BasePath,
        [Parameter(Mandatory = $true)][string]$FullName
    )
    $baseFull = [IO.Path]::GetFullPath($BasePath).TrimEnd([IO.Path]::DirectorySeparatorChar)
    return $FullName.Substring($baseFull.Length).TrimStart("\", "/").Replace("\", "/")
}

function Test-ProtectedRelativePath {
    param([Parameter(Mandatory = $true)][string]$RelativePath)
    $normalized = $RelativePath.Replace("\", "/").TrimStart("/")
    $first = $normalized.Split("/")[0].ToLowerInvariant()
    return $first -in @(
        "config", "prompts", "records", "logs", "userdata", "user-data",
        ".update-backups"
    )
}

$packageFull = [IO.Path]::GetFullPath($PackagePath)
$installFull = Assert-NotBroadPath -Path $InstallDir
if ([string]::IsNullOrWhiteSpace($StateRoot)) {
    $localData = [Environment]::GetFolderPath([Environment+SpecialFolder]::LocalApplicationData)
    $StateRoot = Join-Path $localData "VoceKit\updates"
}
$script:stateRootFull = Assert-NotBroadPath -Path $StateRoot
$stagingRoot = Join-Path $script:stateRootFull ("staging-" + [Guid]::NewGuid().ToString("N"))
$backupRoot = Join-Path $script:stateRootFull (
    "backups\" + [DateTimeOffset]::Now.ToString("yyyyMMdd-HHmmss") + "-" +
    ($TargetVersion -replace '[^0-9A-Za-z.-]', '_')
)
$updatedTargets = New-Object Collections.Generic.List[string]

try {
    if (-not (Test-Path -LiteralPath $packageFull -PathType Leaf)) {
        throw "Update package not found: $packageFull"
    }
    if (-not (Test-Path -LiteralPath $installFull -PathType Container)) {
        throw "Install directory not found: $installFull"
    }
    if (-not (Test-Path -LiteralPath (Join-Path $installFull ".vocekit-portable") -PathType Leaf)) {
        throw "Portable-install marker is missing; refusing to replace files."
    }
    if (-not (Test-Path -LiteralPath (Join-Path $installFull "vocekit.exe") -PathType Leaf)) {
        throw "Installed vocekit.exe was not found."
    }
    $expected = $ExpectedSha256.Trim().ToLowerInvariant()
    if ($expected -notmatch '^[0-9a-f]{64}$') {
        throw "Expected SHA-256 is invalid."
    }
    $actual = (Get-FileHash -LiteralPath $packageFull -Algorithm SHA256).Hash.ToLowerInvariant()
    if ($actual -cne $expected) {
        throw "Update package SHA-256 mismatch. Expected $expected, got $actual."
    }

    if ($WaitForProcessId -gt 0) {
        $process = Get-Process -Id $WaitForProcessId -ErrorAction SilentlyContinue
        if ($process) {
            if (-not $process.WaitForExit(120000)) {
                throw "VoceKit did not exit within 120 seconds."
            }
        }
    }

    [void][IO.Directory]::CreateDirectory($stagingRoot)
    $archive = [IO.Compression.ZipFile]::OpenRead($packageFull)
    try {
        $stagingPrefix = [IO.Path]::GetFullPath($stagingRoot).TrimEnd("\") + "\"
        foreach ($entry in $archive.Entries) {
            $name = $entry.FullName.Replace("/", "\")
            if ([string]::IsNullOrWhiteSpace($name)) {
                continue
            }
            if ([IO.Path]::IsPathRooted($name) -or $name.Contains(":")) {
                throw "Unsafe absolute archive entry: $name"
            }
            $destination = [IO.Path]::GetFullPath((Join-Path $stagingRoot $name))
            if (-not $destination.StartsWith($stagingPrefix, [StringComparison]::OrdinalIgnoreCase)) {
                throw "Archive entry escapes the staging directory: $name"
            }
        }
    }
    finally {
        $archive.Dispose()
    }
    [IO.Compression.ZipFile]::ExtractToDirectory($packageFull, $stagingRoot)

    $stagedExecutable = Join-Path $stagingRoot "vocekit.exe"
    if (-not (Test-Path -LiteralPath $stagedExecutable -PathType Leaf)) {
        throw "Update package does not contain vocekit.exe at its root."
    }

    $runtimeFiles = @(Get-ChildItem -LiteralPath $stagingRoot -File -Recurse | Where-Object {
        $relative = Get-NormalizedRelativePath -BasePath $stagingRoot -FullName $_.FullName
        -not (Test-ProtectedRelativePath -RelativePath $relative)
    })
    if ($runtimeFiles.Count -eq 0) {
        throw "Update package contains no replaceable runtime files."
    }

    [void][IO.Directory]::CreateDirectory($backupRoot)
    $copiedCount = 0
    foreach ($source in $runtimeFiles) {
        $relative = Get-NormalizedRelativePath -BasePath $stagingRoot -FullName $source.FullName
        $target = Join-Path $installFull $relative.Replace("/", "\")
        $targetDirectory = Split-Path -Parent $target
        [void][IO.Directory]::CreateDirectory($targetDirectory)

        if (Test-Path -LiteralPath $target -PathType Leaf) {
            $backup = Join-Path $backupRoot $relative.Replace("/", "\")
            [void][IO.Directory]::CreateDirectory((Split-Path -Parent $backup))
            [IO.File]::Copy($target, $backup, $true)
        }
        $updatedTargets.Add($target)
        [IO.File]::Copy($source.FullName, $target, $true)
        ++$copiedCount
        if ($FailureInjection -eq "after-first-copy" -and $copiedCount -eq 1) {
            throw "Injected update failure after the first copied file."
        }
    }

    Write-UpdateResult -Status "success" -Message "VoceKit $TargetVersion installed successfully."

    if (-not $NoRestart) {
        if ([string]::IsNullOrWhiteSpace($RestartExecutable)) {
            $RestartExecutable = Join-Path $installFull "vocekit.exe"
        }
        if (-not (Test-Path -LiteralPath $RestartExecutable -PathType Leaf)) {
            throw "Restart executable not found: $RestartExecutable"
        }
        Start-Process -FilePath $RestartExecutable -WorkingDirectory $installFull -WindowStyle Hidden
    }
}
catch {
    $failure = $_.Exception.Message
    try {
        foreach ($target in $updatedTargets) {
            if (Test-Path -LiteralPath $target -PathType Leaf) {
                [IO.File]::Delete($target)
            }
        }
        if (Test-Path -LiteralPath $backupRoot -PathType Container) {
            foreach ($backup in Get-ChildItem -LiteralPath $backupRoot -File -Recurse) {
                $relative = Get-NormalizedRelativePath -BasePath $backupRoot -FullName $backup.FullName
                $target = Join-Path $installFull $relative.Replace("/", "\")
                [void][IO.Directory]::CreateDirectory((Split-Path -Parent $target))
                [IO.File]::Copy($backup.FullName, $target, $true)
            }
        }
    }
    catch {
        $failure += " Rollback error: $($_.Exception.Message)"
    }
    Write-UpdateResult -Status "failed" -Message "VoceKit update failed and rollback was attempted." -Detail $failure
    throw $failure
}
finally {
    if (Test-Path -LiteralPath $stagingRoot -PathType Container) {
        [IO.Directory]::Delete($stagingRoot, $true)
    }
}
