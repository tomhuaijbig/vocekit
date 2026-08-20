[CmdletBinding(DefaultParameterSetName = "Create")]
param(
    [Parameter(Mandatory = $true, ParameterSetName = "Create")]
    [string]$UpdateFeedUrl,
    [Parameter(Mandatory = $true, ParameterSetName = "Create")]
    [string]$ReleaseBaseUrl,
    [Parameter(Mandatory = $true, ParameterSetName = "Create")]
    [string]$ReleasePageBaseUrl,
    [Parameter(Mandatory = $true, ParameterSetName = "Create")]
    [string]$ExpectedSignerSubject,
    [Parameter(Mandatory = $true, ParameterSetName = "Create")]
    [string]$ExpectedSignerThumbprint,
    [Parameter(Mandatory = $true, ParameterSetName = "Create")]
    [string]$ExpectedTag,
    [string]$PackageName = "vocekit-qt6-portable",
    [Parameter(Mandatory = $true, ParameterSetName = "DecisionTest")]
    [switch]$DecisionTestMode
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"
. (Join-Path $PSScriptRoot "release-path-safety.ps1")
. (Join-Path $PSScriptRoot "build-provenance.ps1")
. (Join-Path $PSScriptRoot "runtime-helper-provenance.ps1")

function Assert-ReleaseTagMatchesVersion {
    param(
        [Parameter(Mandatory = $true)][string]$Version,
        [Parameter(Mandatory = $true)][string]$ExpectedTag
    )

    # The current manifest protocol always emits channel=stable and
    # prerelease=false. Until channel is an explicit end-to-end input, a
    # prerelease APP_VERSION must fail closed instead of being mislabeled.
    if ($Version -notmatch '^(0|[1-9]\d*)\.(0|[1-9]\d*)\.(0|[1-9]\d*)$') {
        throw "Formal APP_VERSION must be a stable x.y.z version: $Version"
    }
    if ($ExpectedTag -cne "v$Version") {
        throw "Release tag '$ExpectedTag' does not match APP_VERSION '$Version'."
    }
}

function Get-ReleaseCandidatePaths {
    param(
        [Parameter(Mandatory = $true)][string]$DistributionRoot,
        [Parameter(Mandatory = $true)][string]$Version,
        [Parameter(Mandatory = $true)][string]$PackageName
    )

    if ($Version -notmatch '^(0|[1-9]\d*)\.(0|[1-9]\d*)\.(0|[1-9]\d*)$') {
        throw "APP_VERSION is not a supported semantic version: $Version"
    }
    if ([string]::IsNullOrWhiteSpace($PackageName) -or
        $PackageName -ne [IO.Path]::GetFileName($PackageName) -or
        $PackageName.IndexOfAny([IO.Path]::GetInvalidFileNameChars()) -ge 0) {
        throw "PackageName must be a single safe file name."
    }

    $distributionFull = [IO.Path]::GetFullPath($DistributionRoot)
    $releaseDirectory = [IO.Path]::GetFullPath((Join-Path $distributionFull "releases\v$Version"))
    return [PSCustomObject]@{
        ReleaseDirectory = $releaseDirectory
        PackageDirectory = Join-Path $releaseDirectory $PackageName
        ArchivePath = Join-Path $releaseDirectory "$PackageName.zip"
        SidecarPath = Join-Path $releaseDirectory "$PackageName.zip.sha256"
        ManifestPath = Join-Path $releaseDirectory "update-manifest.json"
        CandidatePath = Join-Path $releaseDirectory "release-candidate.json"
    }
}

function Assert-FormalReleaseTargetUnused {
    param([Parameter(Mandatory = $true)][string]$ReleaseDirectory)

    $releaseFull = [IO.Path]::GetFullPath($ReleaseDirectory)
    if (Test-Path -LiteralPath $releaseFull) {
        throw "Formal release target already exists; refusing to overwrite it: $releaseFull"
    }
}

function Assert-FormalReleaseTagUnused {
    param(
        [Parameter(Mandatory = $true)][string]$RepositoryRoot,
        [Parameter(Mandatory = $true)][string]$ExpectedTag
    )

    if ($ExpectedTag -notmatch '^v(0|[1-9]\d*)\.(0|[1-9]\d*)\.(0|[1-9]\d*)$') {
        throw "Formal release tag must use vX.Y.Z."
    }
    $tagRef = "refs/tags/$ExpectedTag"
    $local = Invoke-TrustedGit `
        -RepositoryRoot $RepositoryRoot `
        -Arguments @("show-ref", "--verify", "--quiet", $tagRef) `
        -AllowedExitCodes @(0, 1)
    if ([int]$local.exit_code -eq 0) {
        throw "Formal release tag already exists locally; one version cannot produce a second candidate: $ExpectedTag"
    }
    $remote = Invoke-TrustedGit `
        -RepositoryRoot $RepositoryRoot `
        -Arguments @("ls-remote", "--exit-code", "--refs", "origin", $tagRef) `
        -AllowedExitCodes @(0, 2)
    if ([int]$remote.exit_code -eq 0 -or @($remote.output).Count -ne 0) {
        throw "Formal release tag already exists on origin; one version cannot produce a second candidate: $ExpectedTag"
    }
}

function Assert-FormalSourceStateUnchanged {
    param(
        [Parameter(Mandatory = $true)][string]$RepositoryRoot,
        [Parameter(Mandatory = $true)][string]$ExpectedSourceCommit
    )

    $headResult = Invoke-TrustedGit `
        -RepositoryRoot $RepositoryRoot `
        -Arguments @("rev-parse", "HEAD")
    $head = (@($headResult.output) -join "").Trim()
    if ($head -cne $ExpectedSourceCommit -or
        -not (Get-GitSourceTreeClean -RepositoryRoot $RepositoryRoot)) {
        throw "Git source state changed while the formal candidate was being packaged; generated candidate output is invalid."
    }
}

function Remove-FailedFormalCandidateDirectory {
    param(
        [Parameter(Mandatory = $true)][string]$ReleaseDirectory,
        [Parameter(Mandatory = $true)][string]$DistributionRoot
    )

    $releaseFull = [IO.Path]::GetFullPath($ReleaseDirectory)
    $distributionFull = [IO.Path]::GetFullPath($DistributionRoot).TrimEnd([char[]]@('\', '/'))
    $expectedPrefix = $distributionFull + "\releases\"
    if (-not $releaseFull.StartsWith($expectedPrefix, [StringComparison]::OrdinalIgnoreCase) -or
        [IO.Path]::GetFileName($releaseFull) -notmatch '^v\d+\.\d+\.\d+$') {
        throw "Refusing to invalidate an unsafe formal candidate directory: $releaseFull"
    }
    if ([IO.Directory]::Exists($releaseFull)) {
        [void](Assert-NoReparsePointsInExistingPathChain `
            -Path $releaseFull `
            -Label "Failed formal candidate directory")
        [IO.Directory]::Delete($releaseFull, $true)
    }
}

function Get-ReleaseArtifactBinding {
    param([Parameter(Mandatory = $true)][string]$Path)

    $fullPath = [IO.Path]::GetFullPath($Path)
    if (-not (Test-Path -LiteralPath $fullPath -PathType Leaf)) {
        throw "Release artifact is missing: $fullPath"
    }
    $item = Get-Item -LiteralPath $fullPath
    return [ordered]@{
        name = $item.Name
        bytes = [long]$item.Length
        sha256 = (Get-FileHash -LiteralPath $fullPath -Algorithm SHA256).Hash.ToLowerInvariant()
    }
}

function Get-NormalizedReleaseHttpsUrl {
    param(
        [Parameter(Mandatory = $true)][string]$Value,
        [Parameter(Mandatory = $true)][string]$Label,
        [switch]$BaseUrl
    )

    try { $uri = [Uri]$Value } catch { throw "$Label is not a valid absolute URL." }
    if (-not $uri.IsAbsoluteUri -or $uri.Scheme -ne "https" -or
        [string]::IsNullOrWhiteSpace($uri.Host) -or
        -not [string]::IsNullOrWhiteSpace($uri.UserInfo) -or
        ($BaseUrl -and (-not [string]::IsNullOrWhiteSpace($uri.Query) -or
            -not [string]::IsNullOrWhiteSpace($uri.Fragment)))) {
        throw "$Label must be an absolute HTTPS URL without credentials, query, or fragment."
    }
    if ($BaseUrl) {
        return $Value.TrimEnd("/")
    }
    return $Value
}

function New-ReleaseCandidateRecord {
    param(
        [Parameter(Mandatory = $true)][string]$SourceCommit,
        [Parameter(Mandatory = $true)][string]$Version,
        [Parameter(Mandatory = $true)][string]$ExpectedTag,
        [Parameter(Mandatory = $true)][string]$PackageName,
        [Parameter(Mandatory = $true)][string]$UpdateFeedUrl,
        [Parameter(Mandatory = $true)][string]$ReleaseBaseUrl,
        [Parameter(Mandatory = $true)][string]$ReleasePageBaseUrl,
        [Parameter(Mandatory = $true)][string]$ArchivePath,
        [Parameter(Mandatory = $true)][string]$SidecarPath,
        [Parameter(Mandatory = $true)][string]$ManifestPath,
        [Parameter(Mandatory = $true)]$Signer,
        [Parameter(Mandatory = $true)]$BinaryProvenance,
        [Parameter(Mandatory = $true)][object[]]$RuntimeHelpers
    )

    Assert-ReleaseTagMatchesVersion -Version $Version -ExpectedTag $ExpectedTag
    if ($SourceCommit -notmatch '^[0-9a-fA-F]{40}([0-9a-fA-F]{24})?$') {
        throw "SourceCommit must be a complete Git object ID."
    }
    $sourceTreeCleanProperty = $BinaryProvenance.PSObject.Properties["source_tree_clean"]
    $configurationProperty = $BinaryProvenance.PSObject.Properties["configuration"]
    if ($null -eq $sourceTreeCleanProperty -or
        $sourceTreeCleanProperty.Value -isnot [bool] -or
        -not [bool]$sourceTreeCleanProperty.Value) {
        throw "Signed executable build provenance must confirm a clean Git source tree."
    }
    if ($null -eq $configurationProperty -or
        [string]$configurationProperty.Value -cne "release" -or
        [int]$BinaryProvenance.schema_version -ne 3 -or
        [string]$BinaryProvenance.source_commit -cne $SourceCommit -or
        [string]$BinaryProvenance.version -cne $Version -or
        [string]$BinaryProvenance.update_feed_url -cne $UpdateFeedUrl) {
        throw "Signed executable build provenance does not match source commit, release configuration, and APP_VERSION."
    }
    Assert-ReleaseRuntimeHelperBindings `
        -Bindings $RuntimeHelpers `
        -ExpectedSourceCommit $SourceCommit
    $normalizedUpdateFeedUrl = Get-NormalizedReleaseHttpsUrl `
        -Value $UpdateFeedUrl `
        -Label "UpdateFeedUrl"
    $normalizedReleaseBaseUrl = Get-NormalizedReleaseHttpsUrl `
        -Value $ReleaseBaseUrl `
        -Label "ReleaseBaseUrl" `
        -BaseUrl
    $normalizedReleasePageBaseUrl = Get-NormalizedReleaseHttpsUrl `
        -Value $ReleasePageBaseUrl `
        -Label "ReleasePageBaseUrl" `
        -BaseUrl
    $signerThumbprint = ([string]$Signer.Thumbprint -replace '\s', '').ToUpperInvariant()
    $timestampThumbprint = ([string]$Signer.TimestampThumbprint -replace '\s', '').ToUpperInvariant()
    if ([string]::IsNullOrWhiteSpace([string]$Signer.Subject) -or
        [string]::IsNullOrWhiteSpace([string]$Signer.TimestampSubject) -or
        $signerThumbprint -notmatch '^[0-9A-F]{40,64}$' -or
        $timestampThumbprint -notmatch '^[0-9A-F]{40,64}$') {
        throw "Candidate signer and RFC 3161 timestamp identity are required."
    }

    return [ordered]@{
        schema_version = 1
        kind = "vocekit-signed-release-candidate"
        created_at = [DateTimeOffset]::UtcNow.ToString("o")
        source_commit = $SourceCommit.ToLowerInvariant()
        version = $Version
        tag = $ExpectedTag
        package_name = $PackageName
        binary_provenance = [ordered]@{
            schema_version = 3
            source_commit = [string]$BinaryProvenance.source_commit
            source_tree_clean = [bool]$sourceTreeCleanProperty.Value
            configuration = [string]$configurationProperty.Value
            version = [string]$BinaryProvenance.version
            update_feed_url = [string]$BinaryProvenance.update_feed_url
        }
        runtime_helpers = @($RuntimeHelpers | ForEach-Object { $_ })
        urls = [ordered]@{
            update_feed_url = $normalizedUpdateFeedUrl
            release_base_url = $normalizedReleaseBaseUrl
            release_page_base_url = $normalizedReleasePageBaseUrl
        }
        archive = Get-ReleaseArtifactBinding -Path $ArchivePath
        sidecar = Get-ReleaseArtifactBinding -Path $SidecarPath
        manifest = Get-ReleaseArtifactBinding -Path $ManifestPath
        signer = [ordered]@{
            subject = [string]$Signer.Subject
            thumbprint = $signerThumbprint
            timestamp_subject = [string]$Signer.TimestampSubject
            timestamp_thumbprint = $timestampThumbprint
        }
    }
}

function Get-ReleaseSignerIdentity {
    param(
        [Parameter(Mandatory = $true)][string]$ExecutablePath,
        [Parameter(Mandatory = $true)][string]$ExpectedSubject,
        [string]$ExpectedThumbprint = ""
    )

    $executableFull = [IO.Path]::GetFullPath($ExecutablePath)
    if (-not (Test-Path -LiteralPath $executableFull -PathType Leaf)) {
        throw "Signed release executable is missing: $executableFull"
    }
    $signature = Get-AuthenticodeSignature -LiteralPath $executableFull
    if ($signature.Status -ne [System.Management.Automation.SignatureStatus]::Valid -or
        -not $signature.SignerCertificate) {
        throw "Release executable does not have a valid Authenticode publisher signature: $executableFull"
    }
    if (-not $signature.TimeStamperCertificate) {
        throw "Release executable does not have an RFC 3161 timestamp: $executableFull"
    }

    $actualSubject = $signature.SignerCertificate.Subject
    $actualThumbprint = ($signature.SignerCertificate.Thumbprint -replace '\s', '').ToUpperInvariant()
    $normalizedExpectedThumbprint = ($ExpectedThumbprint -replace '\s', '').ToUpperInvariant()
    if (-not [string]::Equals($actualSubject, $ExpectedSubject, [StringComparison]::OrdinalIgnoreCase)) {
        throw "Release executable publisher subject does not match the expected signer."
    }
    if (-not [string]::IsNullOrWhiteSpace($normalizedExpectedThumbprint) -and
        $actualThumbprint -cne $normalizedExpectedThumbprint) {
        throw "Release executable publisher thumbprint does not match the expected signer."
    }

    return [PSCustomObject]@{
        Subject = $actualSubject
        Thumbprint = $actualThumbprint
        TimestampSubject = $signature.TimeStamperCertificate.Subject
        TimestampThumbprint = ($signature.TimeStamperCertificate.Thumbprint -replace '\s', '').ToUpperInvariant()
    }
}

function Get-FormalRuntimeHelperBindings {
    param(
        [Parameter(Mandatory = $true)][string]$RuntimeDirectory,
        [Parameter(Mandatory = $true)][string]$ExpectedSourceCommit,
        [Parameter(Mandatory = $true)][string]$VerifierPath
    )

    $runtimeFull = [IO.Path]::GetFullPath($RuntimeDirectory)
    $definitions = @(
        @{ helper_name = "vocekit-windows-ocr"; relative_path = "ocr/windows/vocekit-windows-ocr.exe" },
        @{ helper_name = "vocekit-rapidocr"; relative_path = "ocr/rapidocr/vocekit-rapidocr.exe" },
        @{ helper_name = "vocekit-windows-speech"; relative_path = "speech/windows/vocekit-windows-speech.exe" }
    )
    $bindings = New-Object Collections.Generic.List[object]
    foreach ($definition in $definitions) {
        $path = [IO.Path]::GetFullPath((Join-Path $runtimeFull (
            [string]$definition.relative_path
        )))
        $provenance = & $VerifierPath `
            -ExecutablePath $path `
            -ExpectedHelperName ([string]$definition.helper_name) `
            -ExpectedSourceCommit $ExpectedSourceCommit
        $bindings.Add([ordered]@{
            helper_name = [string]$definition.helper_name
            relative_path = [string]$definition.relative_path
            sha256 = (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant()
            provenance = [ordered]@{
                schema_version = [int]$provenance.schema_version
                kind = [string]$provenance.kind
                helper_name = [string]$provenance.helper_name
                source_commit = [string]$provenance.source_commit
                source_tree_clean = [bool]$provenance.source_tree_clean
                configuration = [string]$provenance.configuration
            }
        })
    }
    return @($bindings | ForEach-Object { $_ })
}

function Assert-ReleaseRuntimeHelperBindings {
    param(
        [Parameter(Mandatory = $true)][object[]]$Bindings,
        [Parameter(Mandatory = $true)][string]$ExpectedSourceCommit
    )

    $expected = @{
        "vocekit-windows-ocr" = "ocr/windows/vocekit-windows-ocr.exe"
        "vocekit-rapidocr" = "ocr/rapidocr/vocekit-rapidocr.exe"
        "vocekit-windows-speech" = "speech/windows/vocekit-windows-speech.exe"
    }
    if ($Bindings.Count -ne $expected.Count) {
        throw "Formal candidate must bind exactly three publisher-owned runtime helpers."
    }
    $seen = [Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
    foreach ($binding in $Bindings) {
        $helperName = [string]$binding.helper_name
        if (-not $expected.ContainsKey($helperName) -or -not $seen.Add($helperName) -or
            [string]$binding.relative_path -cne [string]$expected[$helperName] -or
            [string]$binding.sha256 -notmatch '^[0-9a-f]{64}$') {
            throw "Formal candidate runtime helper binding is missing, duplicated, or malformed."
        }
        Assert-RuntimeHelperBuildProvenanceObject `
            -Provenance $binding.provenance `
            -ExpectedHelperName $helperName `
            -ExpectedSourceCommit $ExpectedSourceCommit `
            -ExpectedSourceTreeClean $true `
            -ExpectedConfiguration "Release"
    }
}

if ($DecisionTestMode) {
    return
}

$releaseGate = Join-Path $PSScriptRoot "verify-release-readiness.ps1"
$packageScript = Join-Path $PSScriptRoot "package-test.ps1"
$provenanceVerifier = Join-Path $PSScriptRoot "verify-embedded-build-provenance.ps1"
$helperProvenanceVerifier = Join-Path $PSScriptRoot "verify-runtime-helper-build-provenance.ps1"
$projectRoot = [IO.Path]::GetFullPath((Split-Path -Parent $PSScriptRoot))
$repositoryRoot = [IO.Path]::GetFullPath((Join-Path $projectRoot ".."))
$versionPath = Join-Path $projectRoot "APP_VERSION"
if (-not (Test-Path -LiteralPath $versionPath -PathType Leaf)) {
    throw "APP_VERSION is missing: $versionPath"
}
$version = (Get-Content -LiteralPath $versionPath -Raw -Encoding UTF8).Trim()
Assert-ReleaseTagMatchesVersion -Version $version -ExpectedTag $ExpectedTag
$normalizedExpectedSignerThumbprint = ($ExpectedSignerThumbprint -replace '\s', '').ToUpperInvariant()
if ($normalizedExpectedSignerThumbprint -notmatch '^[0-9A-F]{40,64}$') {
    throw "ExpectedSignerThumbprint must be a complete certificate thumbprint."
}
$paths = Get-ReleaseCandidatePaths `
    -DistributionRoot (Join-Path $projectRoot "dist") `
    -Version $version `
    -PackageName $PackageName
Assert-FormalReleaseTagUnused `
    -RepositoryRoot $repositoryRoot `
    -ExpectedTag $ExpectedTag
[void](Assert-NoReparsePointsInExistingPathChain `
    -Path $paths.ReleaseDirectory `
    -Label "Formal release directory")
Assert-FormalReleaseTargetUnused -ReleaseDirectory $paths.ReleaseDirectory

& $releaseGate `
    -UpdateFeedUrl $UpdateFeedUrl `
    -ExpectedTag $ExpectedTag `
    -ExpectedSignerSubject $ExpectedSignerSubject `
    -ExpectedSignerThumbprint $normalizedExpectedSignerThumbprint `
    -SkipAcceptance `
    -AllowExpectedTagNotCreated

$sourceCommitResult = Invoke-TrustedGit `
    -RepositoryRoot $repositoryRoot `
    -Arguments @("rev-parse", "HEAD")
$sourceCommit = (@($sourceCommitResult.output) -join "").Trim()
if ($sourceCommit -notmatch '^[0-9a-fA-F]{40}([0-9a-fA-F]{24})?$') {
    throw "Unable to resolve the complete source commit for the signed release candidate."
}

$signer = Get-ReleaseSignerIdentity `
    -ExecutablePath (Join-Path $projectRoot ".qt6-deploy\vocekit.exe") `
    -ExpectedSubject $ExpectedSignerSubject `
    -ExpectedThumbprint $normalizedExpectedSignerThumbprint
$binaryProvenance = & $provenanceVerifier `
    -ExecutablePath (Join-Path $projectRoot ".qt6-deploy\vocekit.exe") `
    -ExpectedSourceCommit $sourceCommit `
    -ExpectedVersion $version `
    -ExpectedUpdateFeedUrl $UpdateFeedUrl `
    -ExpectedConfiguration release
[void](Get-FormalRuntimeHelperBindings `
    -RuntimeDirectory (Join-Path $projectRoot ".qt6-deploy") `
    -ExpectedSourceCommit $sourceCommit `
    -VerifierPath $helperProvenanceVerifier)

# Reserve the version target only after all read-only readiness checks pass.
# New-Item fails if another process created it after the earlier fail-closed check.
[void](New-Item -ItemType Directory -Path $paths.ReleaseDirectory -ErrorAction Stop)
[void](Assert-NoReparsePointsInExistingPathChain `
    -Path $paths.ReleaseDirectory `
    -Label "Formal release directory")

$candidateTemporary = $null
try {
    & $packageScript `
        -PackageName $PackageName `
        -RequireSignedBinaries `
        -FailIfOutputExists `
        -ReleaseBaseUrl $ReleaseBaseUrl `
        -ReleasePageBaseUrl $ReleasePageBaseUrl `
        -OutputDirectory $paths.ReleaseDirectory

    # Files copied into the ZIP include tracked prompts/configuration. Re-check
    # both HEAD and the complete clean state after packaging and immediately
    # around candidate publication so a detected mid-build change cannot leave
    # a reusable trusted record.
    Assert-FormalSourceStateUnchanged `
        -RepositoryRoot $repositoryRoot `
        -ExpectedSourceCommit $sourceCommit
    $runtimeHelperBindings = @(Get-FormalRuntimeHelperBindings `
        -RuntimeDirectory $paths.PackageDirectory `
        -ExpectedSourceCommit $sourceCommit `
        -VerifierPath $helperProvenanceVerifier)
    $record = New-ReleaseCandidateRecord `
        -SourceCommit $sourceCommit `
        -Version $version `
        -ExpectedTag $ExpectedTag `
        -PackageName $PackageName `
        -UpdateFeedUrl $UpdateFeedUrl `
        -ReleaseBaseUrl $ReleaseBaseUrl `
        -ReleasePageBaseUrl $ReleasePageBaseUrl `
        -ArchivePath $paths.ArchivePath `
        -SidecarPath $paths.SidecarPath `
        -ManifestPath $paths.ManifestPath `
        -Signer $signer `
        -BinaryProvenance $binaryProvenance `
        -RuntimeHelpers $runtimeHelperBindings
    $candidateTemporary = "$($paths.CandidatePath).tmp-$([Guid]::NewGuid().ToString('N'))"
    $record | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $candidateTemporary -Encoding UTF8
    Assert-FormalSourceStateUnchanged `
        -RepositoryRoot $repositoryRoot `
        -ExpectedSourceCommit $sourceCommit
    Assert-FormalReleaseTagUnused `
        -RepositoryRoot $repositoryRoot `
        -ExpectedTag $ExpectedTag
    [IO.File]::Move($candidateTemporary, $paths.CandidatePath)
    [void](Assert-NoReparsePointsInExistingPathChain `
        -Path $paths.CandidatePath `
        -Label "Release candidate record")
    Assert-FormalSourceStateUnchanged `
        -RepositoryRoot $repositoryRoot `
        -ExpectedSourceCommit $sourceCommit
} catch {
    $failure = $_
    if ($null -ne $candidateTemporary -and [IO.File]::Exists($candidateTemporary)) {
        [IO.File]::Delete($candidateTemporary)
    }
    Remove-FailedFormalCandidateDirectory `
        -ReleaseDirectory $paths.ReleaseDirectory `
        -DistributionRoot (Join-Path $projectRoot "dist")
    throw $failure
}

Write-Host "Immutable signed release candidate created:"
Write-Host "  Directory: $($paths.ReleaseDirectory)"
Write-Host "  Archive: $($paths.ArchivePath)"
Write-Host "  Candidate record: $($paths.CandidatePath)"
