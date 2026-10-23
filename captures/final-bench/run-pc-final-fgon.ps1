# Final 3-arm bench on pc with game frame generation ON (config.xml FRAME_GENERATION=FSR); config restored afterwards.
$cfg = "$env:LOCALAPPDATA\KoeiTecmo\NIOH3\config.xml"; $bak = "C:\Users\Ahmad Hamza\nioh3-engine\captures\final-bench\config.xml.prefg"
Copy-Item $cfg $bak -Force
try {
  $x = [IO.File]::ReadAllText($cfg) -replace '<FRAME_GENERATION>[^<]*</FRAME_GENERATION>','<FRAME_GENERATION>FSR</FRAME_GENERATION>'
  [IO.File]::WriteAllText($cfg,$x,(New-Object Text.UTF8Encoding($false)))
  $r = "C:\Users\Ahmad Hamza\nioh3-engine"; $f = "$r\captures\final-bench"
  Start-Process powershell -WindowStyle Hidden -ArgumentList '-NoProfile','-ExecutionPolicy','Bypass','-File','C:/Users/Ahmad Hamza/universal-optimizer/tools/steam/steam_autoplay.ps1','-Minutes','90'
  $sc = Start-Process powershell -WindowStyle Hidden -PassThru -ArgumentList '-NoProfile','-ExecutionPolicy','Bypass','-File',"$f\sensor_collector.ps1",'-Out',"$f\pc-sensors-fgon.csv",'-Seconds','3000'
  & "$r\tools\rematch\audit_screen_run.ps1" -Device pc -Plan "$f\pc-plan" -ResultsCsv "$f\results-pc-final-fgon.csv" -PerfAsi "$f\asi\Nioh3EngineFix.asi" -MaxMinutes 90 -OwnerPid $PID
  if ($sc -and -not $sc.HasExited) { New-Item "$f\pc-sensors-fgon.csv.stop" -ItemType File -Force | Out-Null }
} finally {
  Copy-Item $bak $cfg -Force
  Remove-Item "C:/Users/Ahmad Hamza/AppData/Local/uo-agentd/pc/test-active" -ErrorAction SilentlyContinue
}
