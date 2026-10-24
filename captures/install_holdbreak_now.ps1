Add-Type 'using System;using System.Runtime.InteropServices;public class I2{[StructLayout(LayoutKind.Sequential)]public struct L{public uint s;public uint t;}[DllImport("user32.dll")]public static extern bool GetLastInputInfo(ref L l);}'
function Idle{999}
$g="C:\Program Files (x86)\Steam\steamapps\common\Nioh3"; $r="C:\Users\Ahmad Hamza\nioh3-pstats\mod"
$t0=Get-Date
while($true){ $p=Get-Process Nioh3 -ErrorAction SilentlyContinue
  if(-not $p){break}
  if($true){ $p|Stop-Process -Force; Start-Sleep 6; break }
  if(((Get-Date)-$t0).TotalHours -gt 6){exit}
  Start-Sleep 10 }
$b="C:\Users\Ahmad Hamza\nioh3-engine\captures\pc\play-20261022-pre-holdbreak"; New-Item -ItemType Directory -Force $b|Out-Null
foreach($f in "Nioh3EngineFix.asi","Nioh3EngineFix.ini","Nioh3Training.asi","Nioh3Training.ini","Nioh3BossRematch.asi","Nioh3BossRematch.ini"){Copy-Item "$g\$f" $b -Force -ErrorAction SilentlyContinue}
Copy-Item "$r\Nioh3EngineFix\bin\Nioh3EngineFix.asi" $g -Force
Copy-Item "$r\Nioh3Training\Nioh3Training.asi" $g -Force
Copy-Item "$r\Nioh3BossRematch\bin\Nioh3BossRematch.asi" $g -Force
Start-Process powershell -ArgumentList '-NoProfile','-ExecutionPolicy','Bypass','-File','C:/Users/Ahmad Hamza/universal-optimizer/tools/steam/steam_autoplay.ps1','-Minutes','15' -WindowStyle Hidden
Start-Process "steam://rungameid/3681010"
"installed+relaunched $(Get-Date) integ-22826 (EngineFix+Training+Rematch)" | Out-File "$b\installed.txt"

