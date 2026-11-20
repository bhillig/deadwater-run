<#
.SYNOPSIS
  Headless run simulator (DW-212, Docs/Architecture/17-Testing-Simulator.md): plays -Runs seeds per captain through the
  real run, combat and AI code (UDwSimulateCommandlet), writes Saved/Sim/report.json and Saved/Sim/summary.csv.
  First output line is "SIM PASS <runs> runs | win <captain> x% ... | avg m min | stuck 0" or "SIM FAIL ...".
  Exit code 0 only on PASS (any stuck run fails). Deterministic: same build, data, seeds and policy = same report.
.EXAMPLE
  ./Tools/Simulate.ps1                                  # 100 seeds x 3 captains
  ./Tools/Simulate.ps1 -Runs 1000 -Captain TeamLeader   # one captain
  ./Tools/Simulate.ps1 -ImportTuning                    # sync DT_*Tuning from Tuning/*.json first (saves them; editor must be closed)
#>
param(
    [int] $Runs = 100,
    # All, or a captain short name: TeamLeader, Fearmongerer, Conqueror.
    [string] $Captain = 'All',
    [int] $Seed = 1,
    [string] $Policy = 'DA_SimPolicy_Default',
    [switch] $ImportTuning,
    # Spawn the art Blueprints instead of plain actors (slower, same results).
    [switch] $Art,
    [string] $Out = 'Saved/Sim/report.json'
)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'DwCommon.ps1')

try {
    # Importing saves the tuning tables: an open editor would hold them.
    if ($ImportTuning) { Assert-DwEditorClosed }
    $editor = Get-DwEditorCmd
} catch {
    Write-Output "SIM FAIL ($($_.Exception.Message))"
    exit 2
}

$root = Get-DwRepoRoot
$logDir = Join-Path $root 'Saved\Tools'
New-Item -ItemType Directory -Force $logDir | Out-Null
$log = Join-Path $logDir 'Simulate.log'
$outPath = if ([IO.Path]::IsPathRooted($Out)) { $Out } else { Join-Path $root $Out }
New-Item -ItemType Directory -Force (Split-Path $outPath) | Out-Null

$simArgs = @(
    (Get-DwProject), '-run=DwSimulate', "-Runs=$Runs", "-Captain=$Captain", "-Seed=$Seed", "-Policy=$Policy", "-Out=$outPath",
    '-unattended', '-nullrhi', '-nosplash', '-nop4', '-nosound', '-stdout', '-FullStdOutLogOutput'
)
if ($ImportTuning) { $simArgs += '-ImportTuning' }
if ($Art) { $simArgs += '-Art' }
& $editor @simArgs *> $log
$exit = $LASTEXITCODE

$lines = Get-Content $log | Where-Object { $_ -match 'DWSIM (.*)$' } | ForEach-Object { ($_ -replace '^.*DWSIM ', '') }
$first = $lines | Where-Object { $_ -match '^SIM (PASS|FAIL)' } | Select-Object -First 1
if (-not $first) {
    $why = ($lines | Select-Object -Last 1)
    Write-Output "SIM FAIL (no result line, editor exit $exit$(if ($why) { ": $why" }))"
    Write-Output "  log: $log"
    exit 1
}
Write-Output $first
$lines | Where-Object { $_ -ne $first } | ForEach-Object { Write-Output $_ }
Write-Output "  log: $log"
if ($first -match '^SIM PASS' -and $exit -eq 0) { exit 0 }
exit 1
