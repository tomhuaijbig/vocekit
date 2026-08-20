Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"
. (Join-Path $PSScriptRoot "git-trust-safety.ps1")

function Get-GitSourceTreeClean {
    param([Parameter(Mandatory = $true)][string]$RepositoryRoot)

    $repositoryFull = [IO.Path]::GetFullPath($RepositoryRoot)
    if (-not (Test-Path -LiteralPath $repositoryFull -PathType Container)) {
        throw "Git source tree root is missing: $repositoryFull"
    }

    # Porcelain v1 is stable for scripts. --untracked-files=all includes every
    # untracked source while normal ignores omit controlled build/deploy output.
    # Hidden index flags are rejected first: status alone can be made to omit a
    # modified tracked file via assume-unchanged or skip-worktree.
    Assert-NoHiddenGitIndexEntries -RepositoryRoot $repositoryFull
    $statusResult = Invoke-TrustedGit `
        -RepositoryRoot $repositoryFull `
        -Arguments @("status", "--porcelain=v1", "--untracked-files=all")
    return [bool](@($statusResult.output).Count -eq 0)
}

function Test-ShouldResetQtBuildTree {
    param(
        [Parameter(Mandatory = $true)]
        [ValidateSet("debug", "release")]
        [string]$Configuration,
        [Parameter(Mandatory = $true)][bool]$SourceTreeClean,
        [Parameter(Mandatory = $true)][bool]$FingerprintChanged
    )

    # A clean Release build is a trust boundary: ignored object files are not
    # represented by Git status or the fingerprint and therefore must never be
    # reused for a candidate that can claim source_tree_clean=true. Dirty local
    # development builds retain incremental behavior for iteration speed.
    return [bool](
        $FingerprintChanged -or
        ($Configuration -ceq "release" -and $SourceTreeClean)
    )
}

function Invalidate-CleanProvenanceBuildOutput {
    param(
        [Parameter(Mandatory = $true)][string]$BuildRoot,
        [Parameter(Mandatory = $true)]
        [ValidateSet("debug", "release")]
        [string]$Configuration
    )

    $buildRootFull = [IO.Path]::GetFullPath($BuildRoot)
    $executablePath = [IO.Path]::GetFullPath((Join-Path (Join-Path $buildRootFull $Configuration) "vocekit.exe"))
    $fingerprintPath = [IO.Path]::GetFullPath((Join-Path $buildRootFull ".build-fingerprint.json"))
    $failures = New-Object Collections.Generic.List[string]
    foreach ($path in @($executablePath, $fingerprintPath)) {
        try {
            if ([IO.File]::Exists($path)) {
                [IO.File]::SetAttributes($path, [IO.FileAttributes]::Normal)
                [IO.File]::Delete($path)
            }
        } catch {
            $failures.Add("$path ($($_.Exception.Message))")
        }
    }
    if ($failures.Count -gt 0) {
        throw "Git source tree became dirty and the trusted build output could not be fully invalidated: $($failures -join '; ')"
    }
}
