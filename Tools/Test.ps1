<#
.SYNOPSIS
  Runs automation tests headless. First output line is "TEST PASS n/n" or "TEST FAIL ...". Exit code 0 only on PASS.
  Report: Saved/Automation/report (index.json + index.html).
.EXAMPLE
  ./Tools/Test.ps1
  ./Tools/Test.ps1 -Filter DeadwaterRun.Rules
#>
param(
    [string] $Filter = 'DeadwaterRun'
)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'DwCommon.ps1')

try {
    $editor = Get-DwEditorCmd
} catch {
    Write-Output "TEST FAIL ($($_.Exception.Message))"
    exit 2
}

$root      = Get-DwRepoRoot
$reportDir = Join-Path $root 'Saved\Automation\report'
$logDir    = Join-Path $root 'Saved\Tools'
New-Item -ItemType Directory -Force $logDir | Out-Null
$log = Join-Path $logDir 'Test.log'
if (Test-Path $reportDir) { Remove-Item -Recurse -Force $reportDir }

$editorArgs = @(
    (Get-DwProject),
    "-ExecCmds=Automation RunTests $Filter;Quit",
    '-TestExit=Automation Test Queue Empty',
    "-ReportExportPath=$reportDir",
    '-unattended', '-nullrhi', '-nosplash', '-nop4', '-nosound', '-stdout', '-FullStdOutLogOutput'
)
& $editor @editorArgs *> $log
$exit = $LASTEXITCODE

$index = Join-Path $reportDir 'index.json'
if (-not (Test-Path $index)) {
    Write-Output "TEST FAIL (no report written, editor exit $exit)"
    Write-Output "  log: $log"
    exit 1
}

# index.json is written with a BOM; strip it before parsing.
$json   = (Get-Content $index -Raw -Encoding utf8).TrimStart([char]0xFEFF) | ConvertFrom-Json
$tests  = @($json.tests)
$passed = @($tests | Where-Object { $_.state -eq 'Success' })
$failed = @($tests | Where-Object { $_.state -ne 'Success' })
$total  = $tests.Count

if ($total -gt 0 -and $failed.Count -eq 0 -and $exit -eq 0) {
    Write-Output "TEST PASS $($passed.Count)/$total ($Filter)"
} elseif ($total -eq 0) {
    Write-Output "TEST FAIL (0 tests matched '$Filter', editor exit $exit)"
} else {
    Write-Output "TEST FAIL $($passed.Count)/$total ($Filter, editor exit $exit)"
}
foreach ($t in $tests) {
    Write-Output ("  [{0}] {1}" -f $t.state, $t.fullTestPath)
    foreach ($e in @($t.entries | Where-Object { $_.event.type -eq 'Error' })) { Write-Output "      $($e.event.message)" }
}
Write-Output "  report: $reportDir"
Write-Output "  log: $log"

if ($total -eq 0 -or $failed.Count -gt 0 -or $exit -ne 0) { exit 1 }
exit 0
