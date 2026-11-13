param([Parameter(Mandatory)][string]$Arm, [string]$Row = "R7B35", [int]$RetryMaxSec = 240, [switch]$ColdJournal)
# pc_pjh_arm.ps1 (performance-pc #96049) - ONE launch = ONE arm of the PJH (PSO prewarm + hand-over) boss-route A/B on the Main PC.
#   Route: tools\rematch\fightnow_test.ps1 -Rows <Row> -Retry (Fight now into the arena, idle until the boss kills the player twice).
#   Build under test = the INSTALLED public build (copied to captures\pc-pjh-96049\build), restored afterwards (-KeepBuild:$false).
#   Arms: N = PJ 0 / PJH 0, P = PJH 1 (journal + prewarm + hand-over). All arms SPR = 1 (PSO create timing, measurement only).
#   -ColdJournal: move Nioh3EngineFix_pj aside first (J launch records the route); the original folder is put back by -RestoreJournal.
#   PresentMon runs for the whole launch (process name), stopped by exact PID afterwards.
$ErrorActionPreference = "Continue"
$repo = "C:\Users\Ahmad Hamza\nioh3-engine"; $GD = "C:\Program Files (x86)\Steam\steamapps\common\Nioh3"
$base = "$repo\captures\pc-pjh-96049"; $stamp = Get-Date -f yyyyMMdd-HHmmss; $out = "$base\$stamp-$Arm"; New-Item -ItemType Directory -Force $out | Out-Null
$pj = "$GD\Nioh3EngineFix_pj"; $pjBak = "$base\pj-orig"
if ($ColdJournal) {
  if (-not (Test-Path $pjBak)) { New-Item -ItemType Directory -Force $pjBak | Out-Null; robocopy $pj $pjBak /E /NFL /NDL /NP /NJH | Out-Null }
  Get-ChildItem $pj -File -EA SilentlyContinue | Remove-Item -Force; "journal cleared (original in $pjBak)" | Tee-Object "$out\arm.txt" -Append
}
$set = switch ($Arm) { "N" { @("Patches/SPR=1", "Patches/PJ=0", "Patches/PJH=0") } "N2" { @("Patches/SPR=1", "Patches/PJ=0", "Patches/PJH=0") } default { @("Patches/SPR=1", "Patches/PJH=1") } }
"arm $Arm row $Row set $($set -join ' ') journal: $((Get-ChildItem $pj -File -EA SilentlyContinue | % { "$($_.Name) $($_.Length)" }) -join ', ')" | Tee-Object "$out\arm.txt" -Append
$null = Start-Process powershell -WindowStyle Hidden -ArgumentList "-NoProfile -ExecutionPolicy Bypass -File `"C:\Users\Ahmad Hamza\universal-optimizer\tools\steam\steam_autoplay.ps1`" -Minutes 15"
$pm = Start-Process "$repo\tools\PresentMon.exe" -PassThru -WindowStyle Hidden -RedirectStandardError "$out\pm.err" -RedirectStandardOutput "$out\pm.out" -ArgumentList "--process_name Nioh3.exe --output_file `"$out\presentmon.csv`" --stop_existing_session --no_console_stats"
try {
  & "$repo\tools\rematch\fightnow_test.ps1" -Rows $Row -Retry -RetryMaxSec $RetryMaxSec -MaxMinutes 15 -WaitMinutes 1 -KeepBuild:$false `
    -PerfAsi "$base\build\Nioh3EngineFix.asi" -AddonAsi "$base\build\Nioh3BossRematch.asi" -PerfSet $set *>&1 | Tee-Object "$out\fightnow.txt"
} finally {
  if ($pm -and -not $pm.HasExited) { Stop-Process -Id $pm.Id -Force }
  $fn = Get-ChildItem "$repo\captures\rematch" -Directory -Filter "fightnow-*" | Sort-Object LastWriteTime | Select-Object -Last 1
  if ($fn) { Copy-Item "$($fn.FullName)\trace.txt", "$($fn.FullName)\engine_launch*.log" $out -EA SilentlyContinue }
  "done $(Get-Date -f HH:mm:ss) fightnow=$($fn.Name) asi=$((Get-FileHash "$GD\Nioh3EngineFix.asi").Hash.Substring(0,8)) ini PJH: $((Select-String -Path "$GD\Nioh3EngineFix.ini" -Pattern '^(PJ|PJH|SPR) *=' | % Line) -join ' ')" | Tee-Object "$out\arm.txt" -Append
}
