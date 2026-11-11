<#
.SYNOPSIS
  Creates or updates the placeholder combat content (DW-104) headless: GA_* abilities, DA_* definitions, items, units,
  DA_Encounter_Placeholder, BP_Ship_Placeholder, the three captains (DW-201), DA_GameData slots, and the DT_*Tuning rows
  they read; then the AI content (DW-106), the placeholder Kraken boss (DW-209) and the Synty art pass (DW-307).
  First output line is "CONTENT PASS" or "CONTENT FAIL ...". Exit code 0 only on PASS. Idempotent.
.EXAMPLE
  ./Tools/CreateCombatContent.ps1
#>
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'DwCommon.ps1')

try {
    Assert-DwEditorClosed
    $editor = Get-DwEditorCmd
} catch {
    Write-Output "CONTENT FAIL ($($_.Exception.Message))"
    exit 2
}

$logDir = Join-Path (Get-DwRepoRoot) 'Saved\Tools'
New-Item -ItemType Directory -Force $logDir | Out-Null
$log = Join-Path $logDir 'CreateCombatContent.log'
$script = Join-Path $PSScriptRoot 'Content\create_combat_content.py'

& $editor (Get-DwProject) -run=pythonscript "-script=$script" -unattended -nullrhi -nosplash -nop4 -nosound -stdout -FullStdOutLogOutput *> $log
$exit = $LASTEXITCODE

$lines = Get-Content $log | Where-Object { $_ -match 'DWCONTENT (.*)$' } | ForEach-Object { ($_ -replace '^.*DWCONTENT ', '') }
if (-not $lines) {
    Write-Output "CONTENT FAIL (script produced no output, editor exit $exit)"
    Write-Output "  log: $log"
    exit 1
}
$status = $lines[-1]
if ($status -ne 'CONTENT PASS') {
    Write-Output "CONTENT FAIL"
    $lines | Where-Object { $_ -match '^ERROR' } | ForEach-Object { Write-Output "  $_" }
    Write-Output "  log: $log"
    exit 1
}
Write-Output "CONTENT PASS"
$lines | Select-Object -SkipLast 1 | ForEach-Object { Write-Output "  $_" }
Write-Output "  log: $log"
exit 0
