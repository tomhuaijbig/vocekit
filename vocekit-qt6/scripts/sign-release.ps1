param(
    [string]$RuntimeDir = "",
    [Parameter(Mandatory = $true)]
    [string]$CertificateThumbprint,
    [string]$TimestampUrl = "http://timestamp.digicert.com"
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$projectRoot = [IO.Path]::GetFullPath((Split-Path -Parent $PSScriptRoot))
if ([string]::IsNullOrWhiteSpace($RuntimeDir)) {
    $RuntimeDir = Join-Path $projectRoot ".qt6-deploy"
}
$runtimeFull = [IO.Path]::GetFullPath($RuntimeDir)
if (-not (Test-Path -LiteralPath $runtimeFull -PathType Container)) {
    throw "Runtime directory not found: $runtimeFull"
}
$thumbprint = ($CertificateThumbprint -replace '\s', '').ToUpperInvariant()
if ($thumbprint -notmatch '^[0-9A-F]{40,64}$') {
    throw "Certificate thumbprint is invalid."
}
$signtool = Get-ChildItem -Path "${env:ProgramFiles(x86)}\Windows Kits\10\bin" -Filter signtool.exe -File -Recurse -ErrorAction SilentlyContinue |
    Where-Object { $_.FullName -match '\\x64\\signtool\.exe$' } |
    Sort-Object FullName -Descending |
    Select-Object -First 1
if (-not $signtool) {
    throw "signtool.exe was not found. Install the Windows SDK signing tools."
}

$targets = @(Get-ChildItem -LiteralPath $runtimeFull -File -Recurse | Where-Object {
    $_.Extension.ToLowerInvariant() -in @(".exe", ".dll")
})
if ($targets.Count -eq 0) {
    throw "No executable files found to sign."
}
foreach ($target in $targets) {
    & $signtool.FullName sign /sha1 $thumbprint /fd SHA256 /tr $TimestampUrl /td SHA256 $target.FullName
    if ($LASTEXITCODE -ne 0) {
        throw "Signing failed: $($target.FullName)"
    }
    & $signtool.FullName verify /pa /all $target.FullName
    if ($LASTEXITCODE -ne 0) {
        throw "Signature verification failed: $($target.FullName)"
    }
}
Write-Host "Signed and verified $($targets.Count) runtime binaries."
