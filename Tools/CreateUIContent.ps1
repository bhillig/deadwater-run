<#
.SYNOPSIS
  Creates or updates the UI content (DW-107) headless: CommonUI input data, WBP_* widget blueprints, DA_UIScreenSet,
  DA_UISoundMap (created once) and the DA_GameData.UIScreenSet slot (UDwUIContentLibrary).
  Run after Tools/CreateCombatContent.ps1 (it needs DA_GameData). Idempotent.
  First output line is "CONTENT PASS" or "CONTENT FAIL ...". Exit code 0 only on PASS.
.EXAMPLE
  ./Tools/CreateUIContent.ps1
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
$log = Join-Path $logDir 'CreateUIContent.log'
$script = Join-Path $PSScriptRoot 'Content\create_ui_content.py'

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
