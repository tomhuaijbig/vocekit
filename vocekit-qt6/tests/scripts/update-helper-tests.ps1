Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"
Add-Type -AssemblyName System.IO.Compression
Add-Type -AssemblyName System.IO.Compression.FileSystem

$projectRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot "..\.."))
$updater = Join-Path $projectRoot "updater\vocekit-update.ps1"
if (-not (Test-Path -LiteralPath $updater -PathType Leaf)) {
    throw "Updater script not found: $updater"
}

$testRoot = Join-Path ([IO.Path]::GetTempPath()) ("vocekit-update-test-" + [Guid]::NewGuid().ToString("N"))
$installDir = Join-Path $testRoot "install"
$packageRoot = Join-Path $testRoot "package"
$packagePath = Join-Path $testRoot "update.zip"
try {
    foreach ($directory in @(
        $installDir,
        $packageRoot,
        (Join-Path $installDir "config"),
        (Join-Path $installDir "prompts"),
        (Join-Path $installDir "logs"),
        (Join-Path $packageRoot "config"),
        (Join-Path $packageRoot "prompts")
    )) {
        [void][IO.Directory]::CreateDirectory($directory)
    }
    [IO.File]::WriteAllText((Join-Path $installDir ".vocekit-portable"), "portable")
    [IO.File]::WriteAllText((Join-Path $installDir "vocekit.exe"), "old-exe")
    [IO.File]::WriteAllText((Join-Path $installDir "Qt6Core.dll"), "old-core")
    [IO.File]::WriteAllText((Join-Path $installDir "config\settings.json"), "user-settings")
    [IO.File]::WriteAllText((Join-Path $installDir "prompts\qa.txt"), "user-prompt")
    [IO.File]::WriteAllText((Join-Path $installDir "logs\model-requests.jsonl"), "user-log")

    [IO.File]::WriteAllText((Join-Path $packageRoot "vocekit.exe"), "new-exe")
    [IO.File]::WriteAllText((Join-Path $packageRoot "Qt6Core.dll"), "new-core")
    [IO.File]::WriteAllText((Join-Path $packageRoot "new-runtime.dat"), "new-runtime")
    [IO.File]::WriteAllText((Join-Path $packageRoot "config\settings.json"), "package-settings")
    [IO.File]::WriteAllText((Join-Path $packageRoot "prompts\qa.txt"), "package-prompt")
    [IO.Compression.ZipFile]::CreateFromDirectory($packageRoot, $packagePath)
    $hash = (Get-FileHash -LiteralPath $packagePath -Algorithm SHA256).Hash

    & $updater `
        -PackagePath $packagePath `
        -InstallDir $installDir `
        -ExpectedSha256 $hash `
        -TargetVersion "9.9.9" `
        -WaitForProcessId 0 `
        -StateRoot (Join-Path $testRoot "state") `
        -NoRestart
    if ((Get-Content -LiteralPath (Join-Path $installDir "vocekit.exe") -Raw) -ne "new-exe") {
        throw "Runtime executable was not updated."
    }
    if ((Get-Content -LiteralPath (Join-Path $installDir "config\settings.json") -Raw) -ne "user-settings") {
        throw "User settings were overwritten."
    }
    if ((Get-Content -LiteralPath (Join-Path $installDir "prompts\qa.txt") -Raw) -ne "user-prompt") {
        throw "User prompt was overwritten."
    }
    if ((Get-Content -LiteralPath (Join-Path $installDir "logs\model-requests.jsonl") -Raw) -ne "user-log") {
        throw "User log was overwritten."
    }
    if ((Get-Content -LiteralPath (Join-Path $installDir "new-runtime.dat") -Raw) -ne "new-runtime") {
        throw "New runtime file was not installed."
    }

    [IO.File]::WriteAllText((Join-Path $installDir "vocekit.exe"), "stable-exe")
    [IO.File]::WriteAllText((Join-Path $installDir "Qt6Core.dll"), "stable-core")
    $rollbackFailed = $false
    try {
        & $updater `
            -PackagePath $packagePath `
            -InstallDir $installDir `
            -ExpectedSha256 $hash `
            -TargetVersion "9.9.10" `
            -WaitForProcessId 0 `
            -StateRoot (Join-Path $testRoot "state-rollback") `
            -FailureInjection "after-first-copy" `
            -NoRestart
    } catch {
        $rollbackFailed = $true
    }
    if (-not $rollbackFailed) {
        throw "Injected updater failure did not fail."
    }
    if ((Get-Content -LiteralPath (Join-Path $installDir "vocekit.exe") -Raw) -ne "stable-exe") {
        throw "Rollback did not restore the executable."
    }
    if ((Get-Content -LiteralPath (Join-Path $installDir "Qt6Core.dll") -Raw) -ne "stable-core") {
        throw "Rollback did not restore the runtime library."
    }

    $failed = $false
    try {
        & $updater `
            -PackagePath $packagePath `
            -InstallDir $installDir `
            -ExpectedSha256 ("0" * 64) `
            -TargetVersion "9.9.10" `
            -WaitForProcessId 0 `
            -StateRoot (Join-Path $testRoot "state-invalid") `
            -NoRestart
    } catch {
        $failed = $true
    }
    if (-not $failed) {
        throw "Updater accepted a package with the wrong SHA-256."
    }

    Write-Host "Updater helper tests passed."
}
finally {
    if ([IO.Directory]::Exists($testRoot)) {
        [IO.Directory]::Delete($testRoot, $true)
    }
}
