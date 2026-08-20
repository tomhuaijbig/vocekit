[CmdletBinding(DefaultParameterSetName = "Verify")]
param(
    [Parameter(Mandatory = $true, ParameterSetName = "Verify")]
    [string]$ExecutablePath,
    [Parameter(Mandatory = $true, ParameterSetName = "Verify")]
    [string]$ExpectedSourceCommit,
    [Parameter(Mandatory = $true, ParameterSetName = "Verify")]
    [string]$ExpectedVersion,
    [Parameter(Mandatory = $true, ParameterSetName = "Verify")]
    [string]$ExpectedUpdateFeedUrl,
    [Parameter(Mandatory = $true, ParameterSetName = "Verify")]
    [ValidateSet("debug", "release")]
    [string]$ExpectedConfiguration,
    [ValidateRange(1000, 60000)]
    [int]$TimeoutMs = 10000,
    [Parameter(Mandatory = $true, ParameterSetName = "DecisionTest")]
    [switch]$DecisionTestMode
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function Get-EmbeddedProvenanceProperty {
    param(
        [Parameter(Mandatory = $true)]$Provenance,
        [Parameter(Mandatory = $true)][string]$Name
    )

    $property = $Provenance.PSObject.Properties[$Name]
    if ($null -eq $property) {
        throw "Embedded build provenance is missing '$Name'."
    }
    return $property.Value
}

function Assert-EmbeddedBuildProvenance {
    param(
        [Parameter(Mandatory = $true)]$Provenance,
        [Parameter(Mandatory = $true)][string]$ExpectedSourceCommit,
        [Parameter(Mandatory = $true)][string]$ExpectedVersion,
        [Parameter(Mandatory = $true)][string]$ExpectedUpdateFeedUrl,
        [Parameter(Mandatory = $true)]
        [ValidateSet("debug", "release")]
        [string]$ExpectedConfiguration
    )

    $schemaVersion = [int](Get-EmbeddedProvenanceProperty -Provenance $Provenance -Name "schema_version")
    $sourceCommit = [string](Get-EmbeddedProvenanceProperty -Provenance $Provenance -Name "source_commit")
    $sourceTreeClean = Get-EmbeddedProvenanceProperty -Provenance $Provenance -Name "source_tree_clean"
    $configuration = [string](Get-EmbeddedProvenanceProperty -Provenance $Provenance -Name "configuration")
    $version = [string](Get-EmbeddedProvenanceProperty -Provenance $Provenance -Name "version")
    $updateFeedUrl = [string](Get-EmbeddedProvenanceProperty -Provenance $Provenance -Name "update_feed_url")
    if ($schemaVersion -ne 3) {
        throw "Embedded build provenance schema_version is unsupported."
    }
    if ($sourceCommit -notmatch '^[0-9a-fA-F]{40}([0-9a-fA-F]{24})?$' -or
        $sourceCommit -cne $ExpectedSourceCommit) {
        throw "Embedded build provenance source_commit does not match the approved source commit."
    }
    if ($sourceTreeClean -isnot [bool] -or -not [bool]$sourceTreeClean) {
        throw "Embedded build provenance source_tree_clean must be the JSON boolean true for a formal release."
    }
    if ($configuration -cne $ExpectedConfiguration) {
        throw "Embedded build provenance configuration does not match the approved build configuration."
    }
    if ($version -cne $ExpectedVersion) {
        throw "Embedded build provenance version does not match APP_VERSION."
    }
    if ($updateFeedUrl -cne $ExpectedUpdateFeedUrl) {
        throw "Embedded build provenance update_feed_url does not match the approved public feed."
    }
}

function Get-EmbeddedBuildProvenance {
    param(
        [Parameter(Mandatory = $true)][string]$ExecutablePath,
        [Parameter(Mandatory = $true)][string]$ExpectedSourceCommit,
        [Parameter(Mandatory = $true)][string]$ExpectedVersion,
        [Parameter(Mandatory = $true)][string]$ExpectedUpdateFeedUrl,
        [Parameter(Mandatory = $true)]
        [ValidateSet("debug", "release")]
        [string]$ExpectedConfiguration,
        [ValidateRange(1000, 60000)][int]$TimeoutMs = 10000
    )

    $executableFull = [IO.Path]::GetFullPath($ExecutablePath)
    if (-not (Test-Path -LiteralPath $executableFull -PathType Leaf)) {
        throw "Executable for embedded build provenance is missing: $executableFull"
    }
    $startInfo = [Diagnostics.ProcessStartInfo]::new()
    $startInfo.FileName = $executableFull
    $startInfo.Arguments = "--build-provenance-json"
    $startInfo.WorkingDirectory = Split-Path -Parent $executableFull
    $startInfo.UseShellExecute = $false
    $startInfo.CreateNoWindow = $true
    $startInfo.RedirectStandardOutput = $true
    $startInfo.RedirectStandardError = $true
    $process = [Diagnostics.Process]::new()
    $process.StartInfo = $startInfo
    try {
        if (-not $process.Start()) {
            throw "Failed to start the executable provenance probe."
        }
        $stdoutTask = $process.StandardOutput.ReadToEndAsync()
        $stderrTask = $process.StandardError.ReadToEndAsync()
        if (-not $process.WaitForExit($TimeoutMs)) {
            try { $process.Kill() } catch { }
            try { $process.WaitForExit() } catch { }
            throw "Executable provenance probe timed out after $TimeoutMs ms."
        }
        $process.WaitForExit()
        $stdout = $stdoutTask.Result
        $stderr = $stderrTask.Result
        if ($process.ExitCode -ne 0) {
            throw "Executable provenance probe exited with code $($process.ExitCode): $stderr"
        }
        $lines = @($stdout -split '\r?\n' | Where-Object { -not [string]::IsNullOrWhiteSpace($_) })
        if ($lines.Count -ne 1) {
            throw "Executable provenance probe must emit exactly one non-empty JSON line."
        }
        try {
            $provenance = $lines[0] | ConvertFrom-Json -ErrorAction Stop
        } catch {
            throw "Executable provenance probe emitted invalid JSON: $($_.Exception.Message)"
        }
        Assert-EmbeddedBuildProvenance `
            -Provenance $provenance `
            -ExpectedSourceCommit $ExpectedSourceCommit `
            -ExpectedVersion $ExpectedVersion `
            -ExpectedUpdateFeedUrl $ExpectedUpdateFeedUrl `
            -ExpectedConfiguration $ExpectedConfiguration
        return $provenance
    } finally {
        $process.Dispose()
    }
}

if ($DecisionTestMode) {
    return
}

Get-EmbeddedBuildProvenance `
    -ExecutablePath $ExecutablePath `
    -ExpectedSourceCommit $ExpectedSourceCommit `
    -ExpectedVersion $ExpectedVersion `
    -ExpectedUpdateFeedUrl $ExpectedUpdateFeedUrl `
    -ExpectedConfiguration $ExpectedConfiguration `
    -TimeoutMs $TimeoutMs
