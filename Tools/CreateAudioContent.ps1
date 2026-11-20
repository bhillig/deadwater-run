<#
.SYNOPSIS
  Creates or updates the audio foundation (DW-113) headless: sound classes SC_Master/Music/SFX/UI, sound mixes
  SM_Base and SM_Pause, and concurrency SCON_Hits/Cannon/UI under Content/DeadwaterRun/Audio/.
  DA_AudioCueMap and DA_MusicSet are not created here (their classes arrive with DW-114).
  First output line is "CONTENT PASS" or "CONTENT FAIL ...". Exit code 0 only on PASS. Idempotent.
.EXAMPLE
  ./Tools/CreateAudioContent.ps1
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
$log = Join-Path $logDir 'CreateAudioContent.log'
$script = Join-Path $PSScriptRoot 'Content\create_audio_content.py'

& $editor (Get-DwProject) -run=pythonscript "-script=$script" -unattended -nullrhi -nosplash -nop4 -nosound -stdout -FullStdOutLogOutput *> $log
$exit = $LASTEXITCODE

$lines = Get-Content $log | Where-Object { $_ -match 'DWCONTENT (.*)$' } | ForEach-Object { ($_ -replace '^.*DWCONTENT ', '') }
$lines = @($lines | Where-Object { $_ -notmatch '^-?(BEGIN|END)$' -and $_ -notmatch '^DWCONTENT-' })
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
