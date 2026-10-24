# Final 3-arm bench on pc: arm1 stock-like (mod loaded, all optimizations 0) / arm2 pure-perf / arm3 full; order 1,2,3,3,2,1. Run via the elevated task (needs PresentMon ETW).
$r = "C:\Users\Ahmad Hamza\nioh3-engine"; $f = "$r\captures\final-bench"
Start-Process powershell -WindowStyle Hidden -ArgumentList '-NoProfile','-ExecutionPolicy','Bypass','-File','C:/Users/Ahmad Hamza/universal-optimizer/tools/steam/steam_autoplay.ps1','-Minutes','90'
$sc = Start-Process powershell -WindowStyle Hidden -PassThru -ArgumentList '-NoProfile','-ExecutionPolicy','Bypass','-File',"$f\sensor_collector.ps1",'-Out',"$f\pc-sensors-B.csv",'-Seconds','3000'
& "$r\tools\rematch\audit_screen_run.ps1" -Device pc -Plan "$f\pc-plan-b" -ResultsCsv "$f\results-pc-final-B.csv" -PerfAsi "$f\asi\Nioh3EngineFix.asi" -MaxMinutes 90 -OwnerPid $PID
if ($sc -and -not $sc.HasExited) { New-Item "$f\pc-sensors-B.csv.stop" -ItemType File -Force | Out-Null }
Remove-Item "C:/Users/Ahmad Hamza/AppData/Local/uo-agentd/pc/test-active" -ErrorAction SilentlyContinue
