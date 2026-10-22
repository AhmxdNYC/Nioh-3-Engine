$g="C:\Program Files (x86)\Steam\steamapps\common\Nioh3"; $r="C:\Users\Ahmad Hamza\nioh3-padviz\mod"
$p=Get-Process Nioh3 -ErrorAction SilentlyContinue; if($p){$p|Stop-Process -Force; Start-Sleep 6}
$b="C:\Users\Ahmad Hamza\nioh3-engine\captures\pc\play-20261022-pre-padskin"; New-Item -ItemType Directory -Force $b|Out-Null
foreach($f in "Nioh3EngineFix.asi","Nioh3EngineFix.ini"){Copy-Item "$g\$f" $b -Force -ErrorAction SilentlyContinue}
Copy-Item "$r\Nioh3EngineFix\bin\Nioh3EngineFix.asi" $g -Force
Start-Process powershell -ArgumentList '-NoProfile','-ExecutionPolicy','Bypass','-File','C:/Users/Ahmad Hamza/universal-optimizer/tools/steam/steam_autoplay.ps1','-Minutes','15' -WindowStyle Hidden
Start-Process "steam://rungameid/3681010"
"installed $(Get-Date) padskin 06fbb93c" | Out-File "$b\installed.txt"
