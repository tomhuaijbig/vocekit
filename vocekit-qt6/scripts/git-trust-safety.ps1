Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function Assert-NoGitTrustEnvironmentOverrides {
    $dangerousNames = @(
        "GIT_DIR",
        "GIT_WORK_TREE",
        "GIT_INDEX_FILE",
        "GIT_OBJECT_DIRECTORY",
        "GIT_ALTERNATE_OBJECT_DIRECTORIES",
        "GIT_COMMON_DIR",
        "GIT_NAMESPACE",
        "GIT_CONFIG",
        "GIT_CONFIG_PARAMETERS",
        "GIT_CONFIG_COUNT",
        "GIT_CONFIG_SYSTEM",
        "GIT_CONFIG_GLOBAL"
    )
    $processEnvironment = @(Get-ChildItem Env:)
    $definedNames = [Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
    foreach ($entry in $processEnvironment) { [void]$definedNames.Add([string]$entry.Name) }
    foreach ($name in $dangerousNames) {
        if ($definedNames.Contains($name)) {
            throw "Git trust gate refuses process environment override $name."
        }
    }
    foreach ($entry in $processEnvironment) {
        if (($entry.Name -like "GIT_CONFIG_KEY_*" -or
            $entry.Name -like "GIT_CONFIG_VALUE_*")) {
            throw "Git trust gate refuses process environment override $($entry.Name)."
        }
    }
}

function Invoke-TrustedGitRaw {
    param(
        [Parameter(Mandatory = $true)][string]$RepositoryRoot,
        [Parameter(Mandatory = $true)][string[]]$Arguments
    )

    $root = [IO.Path]::GetFullPath($RepositoryRoot)
    $output = @()
    $exitCode = -1
    try {
        $output = @(& git --no-replace-objects `
            -c core.fsmonitor=false `
            -c core.untrackedCache=false `
            -C $root @Arguments 2>&1)
        $exitCode = [int]$LASTEXITCODE
    } catch {
        $output = @($_.Exception.Message)
        $exitCode = -1
    }
    return [PSCustomObject]@{ exit_code = $exitCode; output = $output }
}

function Assert-TrustedGitRepository {
    param([Parameter(Mandatory = $true)][string]$RepositoryRoot)

    Assert-NoGitTrustEnvironmentOverrides
    $root = [IO.Path]::GetFullPath($RepositoryRoot)
    if (-not [IO.Directory]::Exists($root)) {
        throw "Canonical Git repository root is missing: $root"
    }
    $rootItem = Get-Item -LiteralPath $root -Force
    if (($rootItem.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) {
        throw "Canonical Git repository root must not be a reparse point: $root"
    }

    # A junction at .git can redirect every otherwise canonical Git query to
    # attacker-controlled metadata while preserving the expected path text.
    $expectedGitDirectory = [IO.Path]::GetFullPath((Join-Path $root ".git"))
    if (-not [IO.Directory]::Exists($expectedGitDirectory)) {
        throw "Canonical Git metadata must be a real .git directory: $expectedGitDirectory"
    }
    $expectedGitDirectoryItem = Get-Item -LiteralPath $expectedGitDirectory -Force
    if (($expectedGitDirectoryItem.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) {
        throw "Canonical Git metadata directory must not be a reparse point: $expectedGitDirectory"
    }

    $topResult = Invoke-TrustedGitRaw -RepositoryRoot $root -Arguments @(
        "rev-parse", "--show-toplevel"
    )
    if ([int]$topResult.exit_code -ne 0 -or @($topResult.output).Count -ne 1 -or
        [IO.Path]::GetFullPath([string]@($topResult.output)[0]) -cne $root) {
        throw "Git --show-toplevel does not match the canonical repository root."
    }

    $gitDirectoryResult = Invoke-TrustedGitRaw -RepositoryRoot $root -Arguments @(
        "rev-parse", "--absolute-git-dir"
    )
    $commonDirectoryResult = Invoke-TrustedGitRaw -RepositoryRoot $root -Arguments @(
        "rev-parse", "--git-common-dir"
    )
    if ([int]$gitDirectoryResult.exit_code -ne 0 -or
        [int]$commonDirectoryResult.exit_code -ne 0 -or
        @($gitDirectoryResult.output).Count -ne 1 -or
        @($commonDirectoryResult.output).Count -ne 1) {
        throw "Unable to resolve the canonical Git metadata directories."
    }
    $actualGitDirectory = [IO.Path]::GetFullPath([string]@($gitDirectoryResult.output)[0])
    $commonText = [string]@($commonDirectoryResult.output)[0]
    $actualCommonDirectory = if ([IO.Path]::IsPathRooted($commonText)) {
        [IO.Path]::GetFullPath($commonText)
    } else {
        [IO.Path]::GetFullPath((Join-Path $root $commonText))
    }
    if ($actualGitDirectory -cne $expectedGitDirectory -or
        $actualCommonDirectory -cne $expectedGitDirectory) {
        throw "Git metadata is not rooted at the canonical repository .git directory."
    }
    $replaceResult = Invoke-TrustedGitRaw `
        -RepositoryRoot $root `
        -Arguments @("for-each-ref", "--format=%(refname)", "refs/replace")
    if ([int]$replaceResult.exit_code -ne 0 -or @($replaceResult.output).Count -ne 0) {
        throw "Git replace refs are not permitted by the formal release trust gate."
    }
}

function Invoke-TrustedGit {
    param(
        [Parameter(Mandatory = $true)][string]$RepositoryRoot,
        [Parameter(Mandatory = $true)][string[]]$Arguments,
        [int[]]$AllowedExitCodes = @(0)
    )

    Assert-TrustedGitRepository -RepositoryRoot $RepositoryRoot
    $result = Invoke-TrustedGitRaw -RepositoryRoot $RepositoryRoot -Arguments $Arguments
    if ([int]$result.exit_code -notin $AllowedExitCodes) {
        throw "Trusted Git command failed (exit $($result.exit_code)): git -C $RepositoryRoot $($Arguments -join ' ')`n$(@($result.output) -join "`n")"
    }
    return $result
}

function Assert-NoHiddenGitIndexEntries {
    param([Parameter(Mandatory = $true)][string]$RepositoryRoot)

    $result = Invoke-TrustedGit `
        -RepositoryRoot $RepositoryRoot `
        -Arguments @("ls-files", "-v")
    $hidden = @($result.output | Where-Object {
        -not [string]::IsNullOrEmpty([string]$_) -and [string]$_[0] -cne "H"
    })
    if ($hidden.Count -gt 0) {
        throw "Git index contains assume-unchanged, skip-worktree, sparse, or other hidden entries: $($hidden -join '; ')"
    }
}
