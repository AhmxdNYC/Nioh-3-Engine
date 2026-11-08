# board #130: final full-data 3-arm bench on pc, asset file 1860 (149e3fde). Order 1,2,3,3,2,1, warmup 15 s / capture 40 s, one launch per run.
$r="C:\Users\Ahmad Hamza\nioh3-engine"; $f="$r\captures\final-full\pc"; $b="$r\captures\test127\bak"
$game="C:\Program Files (x86)\Steam\steamapps\common\Nioh3"; $cfgDir="$env:LOCALAPPDATA\KoeiTecmo\NIOH3"
$marker="C:\Users\Ahmad Hamza\AppData\Local\uo-agentd\pc\test-active"; "ff130" | Set-Content $marker
Start-Process powershell -WindowStyle Hidden -ArgumentList '-NoProfile','-ExecutionPolicy','Bypass','-File','C:/Users/Ahmad Hamza/universal-optimizer/tools/steam/steam_autoplay.ps1','-Minutes','60'
try {
  & "$r\tools\rematch\audit_screen_run.ps1" -Device pc -Plan "$f\plan" -ResultsCsv "$f\results.csv" -PerfAsi "$r\captures\test127\asi\h3\Nioh3EngineFix-1.7.73-drs-hrabi-28358.asi" -MaxMinutes 60 -OwnerPid $PID
} finally {
  Get-Process Nioh3 -EA SilentlyContinue | Stop-Process -Force; Start-Sleep 5
  Copy-Item "$b\config.xml" "$cfgDir\config.xml" -Force
  Copy-Item "$b\Nioh3EngineFix.asi" "$game\Nioh3EngineFix.asi" -Force; Copy-Item "$b\Nioh3EngineFix.ini" "$game\Nioh3EngineFix.ini" -Force
  Remove-Item $marker -EA SilentlyContinue; "done" | Set-Content "$f\DONE"
}
