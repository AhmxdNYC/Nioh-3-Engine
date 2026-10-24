# Final bench on pc. Same-config repeats stay in one launch. Warmup is 12s, each chunk stops at 10 min.
# Sensors run in parallel via LibreHardwareMonitor. If it is down, temp and power stay blank and the bench still runs.
$r = "C:\Users\Ahmad Hamza\nioh3-engine"; $f = "$r\captures\final-bench"
Start-Process powershell -WindowStyle Hidden -ArgumentList '-NoProfile','-ExecutionPolicy','Bypass','-File','C:/Users/Ahmad Hamza/universal-optimizer/tools/steam/steam_autoplay.ps1','-Minutes','15'
$sc = Start-Process powershell -WindowStyle Hidden -PassThru -ArgumentList '-NoProfile','-ExecutionPolicy','Bypass','-File',"$r\tools\sensors\lhm_log.ps1",'-Out',"$f\pc-sensors.csv",'-Seconds','1200'
& "$r\tools\rematch\audit_screen_run.ps1" -Device pc -Plan "$f\pc-plan" -ResultsCsv "$f\results-pc-final.csv" -PerfAsi "$f\asi\Nioh3EngineFix.asi" -WarmupSec 12 -SettleSec 4 -ChunkSec 600 -MaxMinutes 10 -OwnerPid $PID
if ($sc -and -not $sc.HasExited) { New-Item "$f\pc-sensors.csv.stop" -ItemType File -Force | Out-Null }
Remove-Item "C:/Users/Ahmad Hamza/AppData/Local/uo-agentd/pc/test-active" -ErrorAction SilentlyContinue
