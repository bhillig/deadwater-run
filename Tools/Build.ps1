<#
.SYNOPSIS
  Builds DeadwaterRunEditor. First output line is "BUILD PASS (N warnings)" or "BUILD FAIL ...". Exit code 0 only on PASS.
.EXAMPLE
  ./Tools/Build.ps1
  ./Tools/Build.ps1 -Config DebugGame
#>
param(
    [ValidateSet('Development', 'DebugGame')] [string] $Config = 'Development',
    [string] $Target = 'DeadwaterRunEditor',
    [switch] $AllowWarnings
)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'DwCommon.ps1')

try {
    # Only the editor running this configuration has these modules loaded.
    if ($Target -like '*Editor') { Assert-DwEditorClosed -Name $(if ($Config -eq 'Development') { 'UnrealEditor' } else { "UnrealEditor-Win64-$Config" }) }
    $engine = Get-DwEngineDir
} catch {
    Write-Output "BUILD FAIL ($($_.Exception.Message))"
    exit 2
}

$logDir = Join-Path (Get-DwRepoRoot) 'Saved\Tools'
New-Item -ItemType Directory -Force $logDir | Out-Null
$log = Join-Path $logDir "Build-$Target-$Config.log"

$buildBat = Join-Path $engine 'Engine\Build\BatchFiles\Build.bat'
$output = & $buildBat $Target Win64 $Config "-Project=$(Get-DwProject)" -WaitMutex -NoHotReloadFromIDE 2>&1 | ForEach-Object { "$_" }
$exit = $LASTEXITCODE
$output | Set-Content -Path $log -Encoding utf8

# Warnings that count: compiler/UHT warnings in OUR files ("<repo>\Source\...(12): warning C4996") and linker warnings.
# Epic's engine headers raise deprecation warnings we can't fix (19 on a Game target with 5.8.4); those are reported
# separately and never fail the build. Toolchain notices ("compiler ... is not a preferred version") are ignored.
$repoPattern = [regex]::Escape((Get-DwRepoRoot))
$allWarnings = @($output | Where-Object { $_ -match '\(\d+(,\d+)?\)\s*:\s*warning' -or $_ -match '\bwarning LNK\d+' })
$warnings    = @($allWarnings | Where-Object { $_ -match "^\s*$repoPattern" -or $_ -match '\bwarning LNK\d+' })
$engineWarns = $allWarnings.Count - $warnings.Count
$errors      = @($output | Where-Object { $_ -match '\(\d+(,\d+)?\)\s*:\s*(fatal )?error' -or $_ -match '\berror LNK\d+' })

if ($exit -eq 0 -and ($warnings.Count -eq 0 -or $AllowWarnings)) {
    Write-Output "BUILD PASS ($($warnings.Count) warnings) $Target $Config"
} else {
    Write-Output "BUILD FAIL (exit $exit, $($errors.Count) errors, $($warnings.Count) warnings) $Target $Config"
}
$errors   | Select-Object -First 20 | ForEach-Object { Write-Output "  $_" }
$warnings | Select-Object -First 20 | ForEach-Object { Write-Output "  $_" }
if ($engineWarns -gt 0) { Write-Output "  ($engineWarns warnings from engine headers ignored, see log)" }
Write-Output "  log: $log"

if ($exit -ne 0) { exit $exit }
if ($warnings.Count -gt 0 -and -not $AllowWarnings) { exit 1 }
exit 0
