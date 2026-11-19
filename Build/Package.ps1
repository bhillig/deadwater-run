<#
.SYNOPSIS
  Packages Deadwater Run for Win64 (build, cook, stage, pak + IoStore, archive) through RunUAT BuildCookRun.
  First output line is "PACKAGE PASS ..." or "PACKAGE FAIL ...". Exit code 0 only on PASS.
  Docs/Architecture/18-Packaging-Release.md (D2). Brandon runs the Steam upload; this never uploads.
.EXAMPLE
  ./Build/Package.ps1                      # Shipping -> Build/Out/0.1.0-Shipping
  ./Build/Package.ps1 -Config Development  # dev build: console, logs, -DwCapture / dev commands
#>
param(
    [ValidateSet('Shipping', 'Development', 'Test')] [string] $Config = 'Shipping',
    # Skip the game-target compile (reuse the last one); the cook always runs.
    [switch] $SkipBuild
)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot '..\Tools\DwCommon.ps1')

$root = Get-DwRepoRoot
try {
    # The cook loads the editor modules: any running editor of this project would lock them or the assets.
    Assert-DwEditorClosed
    $engine = Get-DwEngineDir
} catch {
    Write-Output "PACKAGE FAIL ($($_.Exception.Message))"
    exit 2
}

$ini = Get-Content (Join-Path $root 'Config\DefaultGame.ini')
$version = ($ini | Where-Object { $_ -match '^\s*ProjectVersion\s*=' } | Select-Object -First 1) -replace '^\s*ProjectVersion\s*=\s*', ''
if (-not $version) { $version = '0.0.0' }

$outDir = Join-Path $root "Build\Out\$version-$Config"
$logDir = Join-Path $root 'Saved\Tools'
New-Item -ItemType Directory -Force $logDir | Out-Null
$log = Join-Path $logDir "Package-$Config.log"
if (Test-Path $outDir) { Remove-Item -Recurse -Force $outDir }

$uat = Join-Path $engine 'Engine\Build\BatchFiles\RunUAT.bat'
$uatArgs = @(
    'BuildCookRun', "-project=$(Get-DwProject)", '-noP4', '-utf8output', '-unattended',
    '-platform=Win64', "-clientconfig=$Config", '-target=DeadwaterRun',
    '-cook', '-stage', '-pak', '-iostore', '-compressed', '-prereqs',
    '-archive', "-archivedirectory=$outDir"
)
if (-not $SkipBuild) { $uatArgs += '-build' }
if ($Config -eq 'Shipping') { $uatArgs += @('-nodebuginfo', '-distribution') }

$start = Get-Date
$output = & $uat @uatArgs 2>&1 | ForEach-Object { "$_" }
$exit = $LASTEXITCODE
$output | Set-Content -Path $log -Encoding utf8
$minutes = [math]::Round(((Get-Date) - $start).TotalMinutes, 1)

$errors = @($output | Where-Object { $_ -match '\berror\b[: ]' -and $_ -notmatch 'Success - 0 error' } | Select-Object -First 20)
$exe = Join-Path $outDir 'Windows\DeadwaterRun.exe'

# Editor-only plugins must not ship (18 "Settings"): no trace of them in the staged build.
$leaks = @()
if (Test-Path $outDir) {
    $leaks = @(Get-ChildItem -Recurse -File $outDir | Where-Object { $_.FullName -match 'ModelContextProtocol|\\Terminal\\|AllToolsets|ModelingTools' } |
        ForEach-Object { $_.FullName.Substring($outDir.Length + 1) })
}

if ($exit -eq 0 -and (Test-Path $exe) -and $leaks.Count -eq 0) {
    $sizeMb = [math]::Round(((Get-ChildItem -Recurse -File $outDir | Measure-Object Length -Sum).Sum / 1MB), 0)
    Write-Output "PACKAGE PASS $version $Config ($sizeMb MB, $minutes min)"
    Write-Output "  exe: $exe"
} else {
    $why = if ($exit -ne 0) { "exit $exit" } elseif (-not (Test-Path $exe)) { 'no DeadwaterRun.exe in the archive' } else { "$($leaks.Count) editor-only plugin files staged" }
    Write-Output "PACKAGE FAIL ($why) $version $Config ($minutes min)"
    $errors | ForEach-Object { Write-Output "  $_" }
    $leaks  | Select-Object -First 20 | ForEach-Object { Write-Output "  staged: $_" }
}
Write-Output "  log: $log"
if ($exit -ne 0) { exit $exit }
if (-not (Test-Path $exe) -or $leaks.Count -gt 0) { exit 1 }
exit 0
