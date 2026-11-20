<#
.SYNOPSIS
  Imports the approved SFX pick list (Docs/Audio/SfxPickList.md, DW-311) as SW_* sound waves under
  Content/DeadwaterRun/Audio/SFX/<Category>/, assigning sound class, concurrency, looping and compression from
  Tools/Content/audio_pick_manifest.csv. The pack path is a parameter and is never stored in game content.
  Run Tools/CreateAudioContent.ps1 first (the classes and concurrency assets must exist).
  First output line is "IMPORT PASS ..." or "IMPORT FAIL ...". Exit code 0 only on PASS. Idempotent: existing
  assets are not re-imported (use -Force) but their settings are re-applied.
.PARAMETER Set
  Sfx (default): Tools/Content/audio_pick_manifest.csv into /Game/DeadwaterRun/Audio/SFX; PackRoot = the Pirate Game pack.
  Music: Tools/Content/music_pick_manifest.csv (Docs/Audio/MusicPickList.md) into /Game/DeadwaterRun/Audio/Music as
  SW_Music_*; PackRoot = the Epic Battle Orchestral Music Pack folder.
.PARAMETER PackRoot
  Folder of the pack for the chosen -Set; searched recursively for the WAV names (read only; nothing is copied into the repo).
.PARAMETER StopgapRoot
  Music set only: the Pirate Game pack root, used by the Stopgap rows (tavern loops for Menu and Map).
.PARAMETER IncludeStopgap
  Music set only: also import the Stopgap rows (requires -StopgapRoot).
.PARAMETER IncludeAlternates
  Also import the alternate rows (named SW_<Cue>_Alt_NN). Off by default.
.PARAMETER Only
  Regex; only manifest rows whose AssetName or Cue match are imported.
.PARAMETER ContentRoot
  Destination /Game folder. Default /Game/DeadwaterRun/Audio/SFX. Use another folder for throwaway tests.
.PARAMETER Force
  Re-import files even when the asset already exists.
.EXAMPLE
  ./Tools/ImportAudio.ps1 -PackRoot 'D:\Packs\Pirate_Game_16bit\Pirate_Game_16bit\Pirate_Game_16bit'
  ./Tools/ImportAudio.ps1 -PackRoot $pack -Only 'Cannon' -ContentRoot /Game/_SW_Throwaway
#>
param(
    [ValidateSet('Sfx', 'Music')] [string] $Set = 'Sfx',
    [Parameter(Mandatory = $true)] [string] $PackRoot,
    [string] $StopgapRoot = '',
    [switch] $IncludeStopgap,
    [string] $Manifest = '',
    [switch] $IncludeAlternates,
    [string] $Only = '',
    [string] $ContentRoot = '',
    [switch] $Force
)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'DwCommon.ps1')

try {
    Assert-DwEditorClosed
    $editor = Get-DwEditorCmd
    if (-not (Test-Path -LiteralPath $PackRoot -PathType Container)) { throw "PackRoot not found: $PackRoot" }
    if (-not $Manifest) {
        $Manifest = Join-Path $PSScriptRoot $(if ($Set -eq 'Music') { 'Content\music_pick_manifest.csv' } else { 'Content\audio_pick_manifest.csv' })
    }
    if (-not $ContentRoot) {
        $ContentRoot = if ($Set -eq 'Music') { '/Game/DeadwaterRun/Audio/Music' } else { '/Game/DeadwaterRun/Audio/SFX' }
    }
    if (-not (Test-Path -LiteralPath $Manifest)) { throw "Manifest not found: $Manifest" }
    if ($IncludeStopgap) {
        if ($Set -ne 'Music') { throw "-IncludeStopgap applies to -Set Music only" }
        if (-not $StopgapRoot -or -not (Test-Path -LiteralPath $StopgapRoot -PathType Container)) { throw "-IncludeStopgap needs an existing -StopgapRoot" }
    }
} catch {
    Write-Output "IMPORT FAIL ($($_.Exception.Message))"
    exit 2
}

# The pack is data only: this reads file names and nothing else from it.
$index = @{}
Get-ChildItem -LiteralPath $PackRoot -Recurse -File -Filter *.wav | ForEach-Object { $index[$_.Name] = $_.FullName }
$stopIndex = @{}
if ($IncludeStopgap) {
    Get-ChildItem -LiteralPath $StopgapRoot -Recurse -File -Filter *.wav | ForEach-Object { $stopIndex[$_.Name] = $_.FullName }
}

$rows = @(Import-Csv $Manifest | Where-Object {
    ($_.Import -eq '1' -or ($IncludeAlternates -and $_.Role -eq 'Alternate') -or ($IncludeStopgap -and $_.Role -eq 'Stopgap')) -and
    (-not $Only -or $_.AssetName -match $Only -or $_.Cue -match $Only)
})
if (-not $rows) { Write-Output "IMPORT FAIL (no manifest rows selected)"; exit 2 }

$items = @(); $missing = @(); $altCounter = @{}
foreach ($r in $rows) {
    # Stopgap rows come from the Pirate Game pack; every other row from -PackRoot.
    $rowIndex = if ($r.Role -eq 'Stopgap') { $stopIndex } else { $index }
    if (-not $rowIndex.ContainsKey($r.Source)) { $missing += $r.Source; continue }
    $name = $r.AssetName
    if (-not $name) {
        # Alternates get a stable name from their cue: SW_<CueTail>_Alt_NN
        $tail = ($r.Cue -replace '\(.*$', '' -replace '[^A-Za-z0-9]+', ' ').Trim()
        $words = $tail -split ' ' | Select-Object -Last 2
        $stem = ($words | ForEach-Object { $_.Substring(0, 1).ToUpper() + $_.Substring(1) }) -join ''
        if (-not $altCounter.ContainsKey($stem)) { $altCounter[$stem] = 0 }
        $altCounter[$stem]++
        $name = 'SW_{0}_Alt_{1:D2}' -f $stem, $altCounter[$stem]
    }
    $items += [ordered]@{
        asset = $name; category = $r.Category; source = $rowIndex[$r.Source]; loop = [int]$r.Loop
        sound_class = $r.SoundClass; concurrency = $r.Concurrency; quality = [int]$r.Quality; loading = $r.Loading
    }
}
if ($missing) {
    Write-Output "IMPORT FAIL ($($missing.Count) source file(s) not found under the pack root)"
    $missing | Select-Object -First 10 | ForEach-Object { Write-Output "  missing: $_" }
    exit 1
}

$logDir = Join-Path (Get-DwRepoRoot) 'Saved\Tools'
New-Item -ItemType Directory -Force $logDir | Out-Null
$planPath = Join-Path $logDir 'audio_import_plan.json'
([ordered]@{ root = $ContentRoot.TrimEnd('/'); force = [bool]$Force; items = $items } | ConvertTo-Json -Depth 5) |
    Set-Content -Encoding UTF8 $planPath
$env:DW_AUDIO_IMPORT_PLAN = $planPath

$log = Join-Path $logDir 'ImportAudio.log'
$script = Join-Path $PSScriptRoot 'Content\import_audio.py'
& $editor (Get-DwProject) -run=pythonscript "-script=$script" -unattended -nullrhi -nosplash -nop4 -nosound -stdout -FullStdOutLogOutput *> $log
$exit = $LASTEXITCODE

$lines = @(Get-Content $log | Where-Object { $_ -match 'DWCONTENT (.*)$' } | ForEach-Object { ($_ -replace '^.*DWCONTENT ', '') })
if (-not $lines) {
    Write-Output "IMPORT FAIL (script produced no output, editor exit $exit)"
    Write-Output "  log: $log"
    exit 1
}
if ($lines[-1] -ne 'CONTENT PASS') {
    Write-Output "IMPORT FAIL"
    $lines | Where-Object { $_ -match '^ERROR' } | ForEach-Object { Write-Output "  $_" }
    Write-Output "  log: $log"
    exit 1
}
Write-Output "IMPORT PASS ($($items.Count) planned)"
$lines | Select-Object -SkipLast 1 | ForEach-Object { Write-Output "  $_" }
Write-Output "  log: $log"
exit 0
