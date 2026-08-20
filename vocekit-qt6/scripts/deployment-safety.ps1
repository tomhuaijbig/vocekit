Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"
. (Join-Path $PSScriptRoot "release-path-safety.ps1")

function Publish-ColdDefaultDeployment {
    param(
        [Parameter(Mandatory = $true)][string]$ProjectRoot,
        [Parameter(Mandatory = $true)][string]$StagingDirectory,
        [Parameter(Mandatory = $true)][string]$Destination,
        [scriptblock]$MoveDirectoryAction = {
            param([string]$Source, [string]$Target)
            [IO.Directory]::Move($Source, $Target)
        },
        [scriptblock]$DeleteDirectoryAction = {
            param([string]$Path)
            [IO.Directory]::Delete($Path, $true)
        }
    )

    $projectFull = [IO.Path]::GetFullPath($ProjectRoot)
    $destinationFull = [IO.Path]::GetFullPath($Destination)
    $expectedDestination = [IO.Path]::GetFullPath((Join-Path $projectFull ".qt6-deploy"))
    $stagingFull = [IO.Path]::GetFullPath($StagingDirectory)
    $stagingPrefix = [IO.Path]::GetFullPath((Join-Path $projectFull ".qt6-deploy.staging-"))
    if ($destinationFull -cne $expectedDestination -or
        -not $stagingFull.StartsWith($stagingPrefix, [StringComparison]::Ordinal) -or
        [IO.Path]::GetFullPath((Split-Path -Parent $stagingFull)) -cne $projectFull) {
        throw "Refusing to publish an unsafe default deployment path."
    }
    [void](Assert-NoReparsePointsInExistingPathChain -Path $destinationFull -Label "Default deployment")
    [void](Assert-NoReparsePointsInExistingPathChain -Path $stagingFull -Label "Deployment staging")
    if (-not [IO.Directory]::Exists($stagingFull)) {
        throw "Verified deployment staging directory is missing: $stagingFull"
    }

    $backup = [IO.Path]::GetFullPath((Join-Path $projectFull (
        ".qt6-deploy.previous-" + [Guid]::NewGuid().ToString("N")
    )))
    $movedOld = $false
    $publishedNew = $false
    try {
        if ([IO.Directory]::Exists($destinationFull)) {
            [void](& $MoveDirectoryAction $destinationFull $backup)
            $movedOld = $true
        }
        [void](& $MoveDirectoryAction $stagingFull $destinationFull)
        $publishedNew = $true
    } catch {
        $publishError = $_.Exception
        if (-not [IO.Directory]::Exists($destinationFull) -and
            $movedOld -and [IO.Directory]::Exists($backup)) {
            try {
                [void](& $MoveDirectoryAction $backup $destinationFull)
                $movedOld = $false
            } catch {
                $restoreError = $_.Exception
                throw (
                    "Deployment publish failed and restoration also failed. " +
                    "The previous deployment backup was preserved at '$backup'. " +
                    "Publish error: $($publishError.Message) Restore error: $($restoreError.Message)"
                )
            }
        }
        throw
    } finally {
        # The previous runtime is the only recoverable copy until the new
        # staging tree reaches the canonical destination. Never remove it on a
        # failed publish or failed restore.
        if ($publishedNew -and $movedOld -and [IO.Directory]::Exists($backup)) {
            [void](& $DeleteDirectoryAction $backup)
        }
    }
}
