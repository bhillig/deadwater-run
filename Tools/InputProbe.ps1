<#
.SYNOPSIS
  Input regression check (DW-107): plays L_Sea in a game window and injects real Slate key/mouse events through the
  viewport, CommonUI and Enhanced Input (UDwDevInputProbeSubsystem): Tab, 1-6, Esc, right-click, left-click select and
  move, Space, and a click on the HUD's End Turn button. Moves the OS cursor while it runs.
  First output line is "INPUT PASS n/n" or "INPUT FAIL ...". Exit code 0 only on PASS.
.EXAMPLE
  ./Tools/InputProbe.ps1
#>
param(
    [int] $Seed = 7,
    # Extra engine arguments, e.g. an -ini: override to A/B a config change.
    [string[]] $ExtraArgs = @()
)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'DwCommon.ps1')

try {
    $editorCmd = Get-DwEditorCmd
} catch {
    Write-Output "INPUT FAIL ($($_.Exception.Message))"
    exit 2
}
$editor = Join-Path (Split-Path $editorCmd) 'UnrealEditor.exe'
$root = Get-DwRepoRoot
$logDir = Join-Path $root 'Saved\Tools'
New-Item -ItemType Directory -Force $logDir | Out-Null
$log = Join-Path $logDir 'InputProbe.log'
if (Test-Path $log) { Remove-Item $log -Force }

$gameArgs = @(
    (Get-DwProject), '/Game/DeadwaterRun/Maps/L_Sea', '-game', '-windowed', '-ResX=1600', '-ResY=900',
    '-DwInputProbe', "-DwSeed=$Seed", '-nosound', '-nosplash', '-unattended', "-abslog=$log"
) + $ExtraArgs
$process = Start-Process -FilePath $editor -ArgumentList $gameArgs -PassThru
if (-not $process.WaitForExit(180000)) {
    Stop-Process -Id $process.Id -Force
    Write-Output "INPUT FAIL (timed out after 180 s)"
    Write-Output "  log: $log"
    exit 1
}

$lines = @(Get-Content $log | Where-Object { $_ -match 'DWINPUT (PASS|FAIL|RESULT|cursor) ' } | ForEach-Object { ($_ -replace '^.*DWINPUT ', '') })
$result = $lines | Where-Object { $_ -match '^RESULT ' } | Select-Object -Last 1
if (-not $result) {
    Write-Output "INPUT FAIL (probe produced no result, exit $($process.ExitCode))"
    Write-Output "  log: $log"
    exit 1
}
$counts = ($result -replace '^RESULT ', '')
$failed = @($lines | Where-Object { $_ -match '^FAIL ' })
if ($failed.Count -eq 0) { Write-Output "INPUT PASS $counts" } else { Write-Output "INPUT FAIL $counts" }
$lines | Where-Object { $_ -notmatch '^RESULT ' } | ForEach-Object { Write-Output "  $_" }
Write-Output "  log: $log"
if ($failed.Count -gt 0) { exit 1 }
exit 0
