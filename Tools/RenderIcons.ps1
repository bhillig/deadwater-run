<#
.SYNOPSIS
  Renders the meshes listed in Tools/Content/ui_icon_renders.json to transparent PNG icons (UDwIconStudioSubsystem):
  lit, orthographic, 2x supersampled. Runs the game in a small window (scene captures need the game loop).
  First output line is "ICONS PASS <count> <folder>" or "ICONS FAIL ...". Exit code 0 only on PASS.
.EXAMPLE
  ./Tools/RenderIcons.ps1                               # all icons -> Saved/Icons
  ./Tools/RenderIcons.ps1 -Out C:\temp\icons -Manifest .\my.json
#>
param(
    [string] $Manifest = '',
    [string] $Out = ''
)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'DwCommon.ps1')

$root = Get-DwRepoRoot
if (-not $Manifest) { $Manifest = Join-Path $root 'Tools\Content\ui_icon_renders.json' }
if (-not $Out) { $Out = Join-Path $root 'Saved\Icons' }
$Manifest = [IO.Path]::GetFullPath($Manifest)
$Out = [IO.Path]::GetFullPath($Out)
try {
    $editorCmd = Get-DwEditorCmd
} catch {
    Write-Output "ICONS FAIL ($($_.Exception.Message))"
    exit 2
}
$editor = Join-Path (Split-Path $editorCmd) 'UnrealEditor.exe'
New-Item -ItemType Directory -Force $Out | Out-Null
Get-ChildItem $Out -Filter 'T_ICON_*.png' -ErrorAction SilentlyContinue | Remove-Item -Force
$logDir = Join-Path $root 'Saved\Tools'
New-Item -ItemType Directory -Force $logDir | Out-Null
$log = Join-Path $logDir 'RenderIcons.log'

$gameArgs = @(
    (Get-DwProject), '/Game/DeadwaterRun/Maps/L_MainMenu', '-game', '-windowed', '-ResX=640', '-ResY=360', '-ForceRes',
    "-DwIconStudio=`"$Manifest`"", "-DwIconStudioOut=`"$Out`"", '-nosound', '-nosplash', '-unattended', "-abslog=$log"
)
$process = Start-Process -FilePath $editor -ArgumentList $gameArgs -PassThru -Wait
$result = Select-String -Path $log -Pattern 'ICONSTUDIO (PASS|FAIL).*' | Select-Object -Last 1
$count = @(Get-ChildItem $Out -Filter 'T_ICON_*.png' -ErrorAction SilentlyContinue).Count
if (-not $result -or $result.Matches[0].Groups[1].Value -ne 'PASS') {
    $why = $(if ($result) { $result.Matches[0].Value } else { "no result line, exit $($process.ExitCode)" })
    Write-Output "ICONS FAIL ($why)"
    Write-Output "  log: $log"
    exit 1
}
Write-Output "ICONS PASS $count $Out"
Write-Output "  log: $log"
exit 0
