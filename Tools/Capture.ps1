<#
.SYNOPSIS
  Plays L_Sea in a game window, stages the combat interaction, saves a screenshot and quits (UDwDevCaptureSubsystem).
  First output line is "CAPTURE PASS <file>" or "CAPTURE FAIL ...". Exit code 0 only on PASS.
  The window cannot be larger than the desktop; capture 2560x1440 on a 1440p (or larger) display.
.EXAMPLE
  ./Tools/Capture.ps1                       # plain fight
  ./Tools/Capture.ps1 -Demo move -Seed 7    # captain selected, path to the farthest reachable tile
  ./Tools/Capture.ps1 -Demo aim             # sniper aiming: targets and damage preview
  ./Tools/Capture.ps1 -Demo cannon          # gunner at a cannon: enemy ship targeted
  ./Tools/Capture.ps1 -Demo hit             # sniper shot just landed: floating text, HP bars, spent action greyed
  ./Tools/Capture.ps1 -Demo endturn         # turn ended: the turn banner for the next side
  ./Tools/Capture.ps1 -Demo enemy           # player passes twice; the crew AI plays two enemy turns (frames <file>-01.png, ...)
  ./Tools/Capture.ps1 -Demo run             # a run (DW-201/203): map readout, fight 1, rewards, level-up, back on the map, fight 2 (frames -01..-05)
  ./Tools/Capture.ps1 -Demo voyage          # a whole chart through the run screens (DW-202/211): map at every stop, first rewards/level-up/shop, the first event's choices and its result (DW-205), the Kraken fight; ends on the Victory summary
  ./Tools/Capture.ps1 -Demo boss            # DW-301: the same voyage, frames from the last stop only: map with only the Kraken, sniper aiming at the body, swordsman aiming at a tentacle, the initial-phase turn (one tentacle cut), the enraged turn (body staged at 28%); ends on the Victory summary
  ./Tools/Capture.ps1 -Demo shop            # DW-204: fights until there is gold to shop, then the shop screen: arrival, after buying (repair, recruit, upgrade, item), a move being taught on the shop screen (learner picked), the map; ends in the next fight (recruit aboard, third cannon)
  ./Tools/Capture.ps1 -Demo captain -Captain Fearmongerer   # DW-206..208: the captain's showcase in fight 1, a frame per turn; Fearmongerer: Smoke and Fire until the crew surrenders
  ./Tools/Capture.ps1 -Demo shell           # DW-210 from the main menu: menu, settings, captain select, Set Sail, map, pause, abandon, summary, New Run
  ./Tools/Capture.ps1 -Demo enemy -Encounter Cutthroats   # DW-305: the dev fight against one crew (Deckhands, PowderMonkeys, Sharpshooters, Cutthroats, DreadCrew)
  ./Tools/Capture.ps1 -Demo aim -ResX 2560 -ResY 1440   # HUD readability at 1440p
  ./Tools/Capture.ps1 -Mood Storm           # the storm look (default Day; Random rolls from the seed)
  ./Tools/Capture.ps1 -Zoom 0.3 -Tile "0,1" # close-up of a tile for art review
#>
param(
    [ValidateSet('none', 'move', 'aim', 'cannon', 'hit', 'endturn', 'enemy', 'run', 'voyage', 'boss', 'shop', 'shell', 'captain')] [string] $Demo = 'none',
    # Start a run with this captain (TeamLeader, Fearmongerer, Conqueror) instead of the dev fight. -Demo run defaults to TeamLeader.
    [string] $Captain = '',
    # Dev fight against this crew (DA_Encounter_<Encounter>) instead of DA_GameData.DevEncounter.
    [string] $Encounter = '',
    # Level to open: Sea (L_Sea) or MainMenu (L_MainMenu). -Demo shell defaults to MainMenu.
    [ValidateSet('', 'Sea', 'MainMenu')] [string] $Map = '',
    [int] $Seed = 7,
    [float] $Delay = 6,
    [int] $ResX = 1600,
    [int] $ResY = 900,
    [string] $Out = '',
    # Sea mood by name (DA_SeaMood_<Mood>): Day, Storm. 'Random' rolls from the seed like a real fight.
    [string] $Mood = 'Day',
    # Close-up for art review: fraction of the framed camera distance (0 = normal framing), focused on -Tile "x,y".
    [float] $Zoom = 0,
    [string] $Tile = '',
    # Extra engine arguments, e.g. '-ExecCmds="gc.TimeBetweenPurgingPendingKillObjects 3"' to shake out GC bugs.
    [string[]] $ExtraArgs = @()
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
$outStem = $(if ($Encounter) { "combat-$Demo-$Encounter" } else { "combat-$Demo" })
if (-not $Out) { $Out = Join-Path $root "Saved\Captures\$outStem-${ResX}x${ResY}.png" }
$Out = [IO.Path]::GetFullPath($Out)
New-Item -ItemType Directory -Force (Split-Path $Out) | Out-Null
if (Test-Path $Out) { Remove-Item $Out -Force }
Get-ChildItem (Split-Path $Out) -Filter "$([IO.Path]::GetFileNameWithoutExtension($Out))-??.png" -ErrorAction SilentlyContinue | Remove-Item -Force
$logDir = Join-Path $root 'Saved\Tools'
New-Item -ItemType Directory -Force $logDir | Out-Null
$log = Join-Path $logDir 'Capture.log'

if (-not $Map) { $Map = $(if ($Demo -eq 'shell') { 'MainMenu' } else { 'Sea' }) }
$gameArgs = @(
    (Get-DwProject), "/Game/DeadwaterRun/Maps/L_$Map", '-game', '-windowed', "-ResX=$ResX", "-ResY=$ResY", '-ForceRes',
    "-DwCapture=$Out", "-DwCaptureDelay=$Delay", "-DwCaptureDemo=$Demo", "-DwSeed=$Seed",
    '-nosound', '-nosplash', '-unattended', "-abslog=$log"
)
if ($Mood -and $Mood -ne 'Random') { $gameArgs += "-DwMood=$Mood" }
if ($Demo -in @('run', 'voyage', 'boss', 'shop', 'captain') -and -not $Captain) { $Captain = 'TeamLeader' }
if ($Captain) { $gameArgs += "-DwCaptain=$Captain" }
if ($Encounter) { $gameArgs += "-DwEncounter=$Encounter" }
if ($Zoom -gt 0) { $gameArgs += "-DwCaptureZoom=$Zoom" }
if ($Tile) { $gameArgs += "-DwCaptureTile=$Tile" }
if ($ExtraArgs) { $gameArgs += $ExtraArgs }
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
