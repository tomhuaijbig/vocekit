param(
    [Parameter(Mandatory = $true)]
    [string]$ArchivePath,
    [string]$VersionFile = "",
    [string]$OutputPath = "",
    [string]$ReleaseBaseUrl = "https://github.com/tomhuaijbig/vocekit/releases/download",
    [string]$ReleasePageBaseUrl = "https://github.com/tomhuaijbig/vocekit/releases/tag",
    [ValidateSet("stable", "preview")]
    [string]$Channel = "stable",
    [string]$ReleaseNotes = ""
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$projectRoot = [IO.Path]::GetFullPath((Split-Path -Parent $PSScriptRoot))
$archiveFull = [IO.Path]::GetFullPath($ArchivePath)
if (-not (Test-Path -LiteralPath $archiveFull -PathType Leaf)) {
    throw "Update archive not found: $archiveFull"
}
if ([string]::IsNullOrWhiteSpace($VersionFile)) {
    $VersionFile = Join-Path $projectRoot "APP_VERSION"
}
$versionFull = [IO.Path]::GetFullPath($VersionFile)
if (-not (Test-Path -LiteralPath $versionFull -PathType Leaf)) {
    throw "APP_VERSION file not found: $versionFull"
}
$version = (Get-Content -LiteralPath $versionFull -Raw -Encoding UTF8).Trim()
if ($version -notmatch '^(0|[1-9]\d*)\.(0|[1-9]\d*)\.(0|[1-9]\d*)(-[0-9A-Za-z.-]+)?$') {
    throw "APP_VERSION is not a supported semantic version: $version"
}
$baseUri = [Uri]$ReleaseBaseUrl
$pageBaseUri = [Uri]$ReleasePageBaseUrl
foreach ($entry in @(
    @{ Name = "ReleaseBaseUrl"; Uri = $baseUri },
    @{ Name = "ReleasePageBaseUrl"; Uri = $pageBaseUri }
)) {
    if (-not $entry.Uri.IsAbsoluteUri -or
        $entry.Uri.Scheme -ne "https" -or
        [string]::IsNullOrWhiteSpace($entry.Uri.Host) -or
        -not [string]::IsNullOrWhiteSpace($entry.Uri.UserInfo)) {
        throw "$($entry.Name) must be an absolute HTTPS URL without embedded credentials."
    }
}
if ([string]::IsNullOrWhiteSpace($OutputPath)) {
    $OutputPath = Join-Path (Split-Path -Parent $archiveFull) "update-manifest.json"
}
$outputFull = [IO.Path]::GetFullPath($OutputPath)
[void][IO.Directory]::CreateDirectory((Split-Path -Parent $outputFull))

$archiveName = [IO.Path]::GetFileName($archiveFull)
$tag = "v$version"
$downloadUrl = $ReleaseBaseUrl.TrimEnd("/") + "/$tag/$archiveName"
$checksumUrl = "$downloadUrl.sha256"
$releasePageUrl = $ReleasePageBaseUrl.TrimEnd("/") + "/$tag"
$sha256 = (Get-FileHash -LiteralPath $archiveFull -Algorithm SHA256).Hash.ToLowerInvariant()
$checksumPath = "$archiveFull.sha256"
Set-Content -LiteralPath $checksumPath -Value "$sha256  $archiveName" -Encoding Ascii

$manifest = [ordered]@{
    schema_version = 1
    version = $version
    channel = $Channel
    release_name = "VoceKit $version"
    release_notes = $ReleaseNotes
    published_at = [DateTimeOffset]::UtcNow.ToString("o")
    release_page_url = $releasePageUrl
    asset_name = $archiveName
    download_url = $downloadUrl
    checksum_url = $checksumUrl
    sha256 = $sha256
    prerelease = ($Channel -eq "preview")
}
$temporary = "$outputFull.tmp"
$manifest | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath $temporary -Encoding UTF8
[IO.File]::Move($temporary, $outputFull, $true)

Write-Host "Update manifest created: $outputFull"
Write-Host "SHA-256 sidecar created: $checksumPath"
