<#
.SYNOPSIS
  Plays L_Sea in a game window, stages the combat interaction, saves a screenshot and quits (UDwDevCaptureSubsystem).
  First output line is "CAPTURE PASS <file>" or "CAPTURE FAIL ...". Exit code 0 only on PASS.
.EXAMPLE
  ./Tools/Capture.ps1                       # plain fight
  ./Tools/Capture.ps1 -Demo move -Seed 7    # captain selected, path to the farthest reachable tile
  ./Tools/Capture.ps1 -Demo aim             # sniper aiming: targets and damage preview
  ./Tools/Capture.ps1 -Demo cannon          # gunner at a cannon: enemy ship targeted
  ./Tools/Capture.ps1 -Demo enemy           # player passes twice; the crew AI plays two enemy turns (frames <file>-01.png, ...)
#>
param(
    [ValidateSet('none', 'move', 'aim', 'cannon', 'enemy')] [string] $Demo = 'none',
    [int] $Seed = 7,
    [float] $Delay = 6,
    [string] $Out = ''
)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'DwCommon.ps1')

try {
    $editorCmd = Get-DwEditorCmd
} catch {
    Write-Output "CAPTURE FAIL ($($_.Exception.Message))"
    exit 2
}
$editor = Join-Path (Split-Path $editorCmd) 'UnrealEditor.exe'
$root = Get-DwRepoRoot
if (-not $Out) { $Out = Join-Path $root "Saved\Captures\combat-$Demo.png" }
$Out = [IO.Path]::GetFullPath($Out)
New-Item -ItemType Directory -Force (Split-Path $Out) | Out-Null
if (Test-Path $Out) { Remove-Item $Out -Force }
Get-ChildItem (Split-Path $Out) -Filter "$([IO.Path]::GetFileNameWithoutExtension($Out))-??.png" -ErrorAction SilentlyContinue | Remove-Item -Force
$logDir = Join-Path $root 'Saved\Tools'
New-Item -ItemType Directory -Force $logDir | Out-Null
$log = Join-Path $logDir 'Capture.log'

$gameArgs = @(
    (Get-DwProject), '/Game/DeadwaterRun/Maps/L_Sea', '-game', '-windowed', '-ResX=1600', '-ResY=900',
    "-DwCapture=$Out", "-DwCaptureDelay=$Delay", "-DwCaptureDemo=$Demo", "-DwSeed=$Seed",
    '-nosound', '-nosplash', '-unattended', "-abslog=$log"
)
$process = Start-Process -FilePath $editor -ArgumentList $gameArgs -PassThru -Wait
if (-not (Test-Path $Out)) {
    Write-Output "CAPTURE FAIL (no screenshot, exit $($process.ExitCode))"
    Write-Output "  log: $log"
    exit 1
}
Write-Output "CAPTURE PASS $Out"
$stem = [IO.Path]::GetFileNameWithoutExtension($Out)
Get-ChildItem (Split-Path $Out) -Filter "$stem-??.png" -ErrorAction SilentlyContinue | Sort-Object Name | ForEach-Object { Write-Output "  frame: $($_.FullName)" }
Write-Output "  log: $log"
exit 0
