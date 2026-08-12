$ErrorActionPreference = "Stop"
$buildScript = Join-Path (Split-Path -Parent $PSScriptRoot) "build-windows-speech-helper.ps1"

. $buildScript -DecisionTestMode

if ((Get-WindowsSpeechBuildDecision -ExitCode 0 -OutputText "") -ne "MSBuild") {
    throw "A successful MSBuild must use the MSBuild output."
}
if ((Get-WindowsSpeechBuildDecision -ExitCode 1 -OutputText "error MSB3644: reference assemblies missing") -ne "RoslynFallback") {
    throw "MSB3644 must select the Roslyn fallback."
}
if ((Get-WindowsSpeechBuildDecision -ExitCode 1 -OutputText "error CS1002: ; expected") -ne "Fail") {
    throw "A normal compiler failure must not select the Roslyn fallback."
}

Write-Host "Windows speech helper build decision tests: PASS"
