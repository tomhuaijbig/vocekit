param(
    [string]$RuntimeDir = "",
    [Parameter(Mandatory = $true)]
    [string]$CertificateThumbprint,
    [string]$TimestampUrl = "http://timestamp.digicert.com"
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"
. (Join-Path $PSScriptRoot "release-path-safety.ps1")
. (Join-Path $PSScriptRoot "runtime-helper-provenance.ps1")

function Get-SigningRelativePath {
    param(
        [Parameter(Mandatory = $true)][string]$BasePath,
        [Parameter(Mandatory = $true)][string]$Path
    )

    $baseFull = [IO.Path]::GetFullPath($BasePath).TrimEnd("\", "/")
    $pathFull = [IO.Path]::GetFullPath($Path)
    $prefix = $baseFull + [IO.Path]::DirectorySeparatorChar
    if (-not $pathFull.StartsWith($prefix, [StringComparison]::OrdinalIgnoreCase)) {
        throw "Signing target escaped the runtime directory: $pathFull"
    }
    return $pathFull.Substring($prefix.Length).Replace("\", "/")
}

$projectRoot = [IO.Path]::GetFullPath((Split-Path -Parent $PSScriptRoot))
if ([string]::IsNullOrWhiteSpace($RuntimeDir)) {
    $RuntimeDir = Join-Path $projectRoot ".qt6-deploy"
}
$runtimeFull = [IO.Path]::GetFullPath($RuntimeDir)
if (-not (Test-Path -LiteralPath $runtimeFull -PathType Container)) {
    throw "Runtime directory not found: $runtimeFull"
}
[void](Assert-NoReparsePointsInExistingPathChain -Path $runtimeFull -Label "Release signing runtime")
$repositoryRoot = [IO.Path]::GetFullPath((Join-Path $projectRoot ".."))
$runtimeHelperRepositoryState = Get-RuntimeHelperRepositoryState `
    -RepositoryRoot $repositoryRoot `
    -ProjectRoot $projectRoot
if (-not [bool]$runtimeHelperRepositoryState.source_tree_clean) {
    throw "Formal release signing requires a clean, trusted Git source tree."
}
$helperProvenanceVerifier = Join-Path $PSScriptRoot "verify-runtime-helper-build-provenance.ps1"
$requiredHelperProvenance = @(
    @{ Name = "vocekit-windows-ocr"; Path = "ocr\windows\vocekit-windows-ocr.exe" },
    @{ Name = "vocekit-rapidocr"; Path = "ocr\rapidocr\vocekit-rapidocr.exe" },
    @{ Name = "vocekit-windows-speech"; Path = "speech\windows\vocekit-windows-speech.exe" }
)
foreach ($helper in $requiredHelperProvenance) {
    [void](& $helperProvenanceVerifier `
        -ExecutablePath (Join-Path $runtimeFull ([string]$helper.Path)) `
        -ExpectedHelperName ([string]$helper.Name) `
        -ExpectedSourceCommit ([string]$runtimeHelperRepositoryState.source_commit))
}
$thumbprint = ($CertificateThumbprint -replace '\s', '').ToUpperInvariant()
if ($thumbprint -notmatch '^[0-9A-F]{40,64}$') {
    throw "Certificate thumbprint is invalid."
}
$signingCertificate = Get-ChildItem Cert:\CurrentUser\My |
    Where-Object { $_.Thumbprint -eq $thumbprint } |
    Select-Object -First 1
if (-not $signingCertificate -or -not $signingCertificate.HasPrivateKey) {
    throw "The selected code-signing certificate or its hardware-backed private key is unavailable."
}
$codeSigningOid = "1.3.6.1.5.5.7.3.3"
if ($signingCertificate.NotAfter -le (Get-Date) -or
    $signingCertificate.EnhancedKeyUsageList.ObjectId.Value -notcontains $codeSigningOid) {
    throw "The selected certificate is expired or is not valid for code signing."
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
} | Sort-Object FullName)
if ($targets.Count -eq 0) {
    throw "No executable files found to sign."
}

$allowedPublisherPaths = [Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
@(
    "vocekit.exe",
    "ocr/rapidocr/vocekit-rapidocr.exe",
    "ocr/windows/vocekit-windows-ocr.exe",
    "speech/windows/vocekit-windows-speech.exe",
    "libgcc_s_seh-1.dll",
    "libstdc++-6.dll",
    "libwinpthread-1.dll"
) | ForEach-Object { [void]$allowedPublisherPaths.Add($_) }

$targetRelativePaths = @{}
$unsignedTargets = @()
$alreadyValidCount = 0
foreach ($target in $targets) {
    $relativePath = Get-SigningRelativePath -BasePath $runtimeFull -Path $target.FullName
    $targetRelativePaths[$target.FullName] = $relativePath
    $existingSignature = Get-AuthenticodeSignature -LiteralPath $target.FullName
    if ($existingSignature.Status -eq [System.Management.Automation.SignatureStatus]::Valid) {
        $alreadyValidCount++
        continue
    }
    if ($existingSignature.Status -ne [System.Management.Automation.SignatureStatus]::NotSigned) {
        throw "Refusing to overwrite an invalid existing signature ($($existingSignature.Status)): $($target.FullName)"
    }
    if (-not $allowedPublisherPaths.Contains($relativePath)) {
        throw "Unexpected unsigned binary is not in the publisher allowlist: $relativePath"
    }
    $unsignedTargets += $target
}

foreach ($requiredPath in $allowedPublisherPaths) {
    if ($requiredPath -notin $targetRelativePaths.Values) {
        throw "Required publisher-owned binary is missing from the runtime: $requiredPath"
    }
}

foreach ($target in $unsignedTargets) {
    & $signtool.FullName sign /sha1 $thumbprint /fd SHA256 /tr $TimestampUrl /td SHA256 $target.FullName
    if ($LASTEXITCODE -ne 0) {
        throw "Signing failed: $($target.FullName)"
    }
}

foreach ($target in $targets) {
    $signature = Get-AuthenticodeSignature -LiteralPath $target.FullName
    if ($signature.Status -ne [System.Management.Automation.SignatureStatus]::Valid) {
        throw "Authenticode verification failed ($($signature.Status)): $($target.FullName)"
    }
    $relativePath = $targetRelativePaths[$target.FullName]
    if ($allowedPublisherPaths.Contains($relativePath)) {
        if (-not $signature.SignerCertificate -or
            $signature.SignerCertificate.Thumbprint -ne $thumbprint) {
            throw "Publisher certificate mismatch: $relativePath"
        }
        if (-not $signature.TimeStamperCertificate) {
            throw "RFC 3161 timestamp is missing: $relativePath"
        }
    }
    & $signtool.FullName verify /pa /all /tw $target.FullName
    if ($LASTEXITCODE -ne 0) {
        throw "Signature verification failed: $($target.FullName)"
    }
}
foreach ($helper in $requiredHelperProvenance) {
    [void](& $helperProvenanceVerifier `
        -ExecutablePath (Join-Path $runtimeFull ([string]$helper.Path)) `
        -ExpectedHelperName ([string]$helper.Name) `
        -ExpectedSourceCommit ([string]$runtimeHelperRepositoryState.source_commit))
}
[void](Assert-RuntimeHelperRepositoryStateUnchanged `
    -Before $runtimeHelperRepositoryState `
    -RepositoryRoot $repositoryRoot `
    -ProjectRoot $projectRoot)
Write-Host "Signed $($unsignedTargets.Count) previously unsigned binaries; preserved $alreadyValidCount valid existing signatures; verified $($targets.Count) binaries."
