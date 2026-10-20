$g="C:\Program Files (x86)\Steam\steamapps\common\Nioh3"; $r="C:\Users\Ahmad Hamza\nioh3-hpki-b\mod"
while(Get-Process Nioh3 -ErrorAction SilentlyContinue){Start-Sleep 5}
$b="C:\Users\Ahmad Hamza\nioh3-engine\captures\pc\play-20261020-pre14655"; New-Item -ItemType Directory -Force $b|Out-Null
foreach($f in "Nioh3Training.asi","Nioh3Training.ini","Nioh3BossRematch.asi"){Copy-Item "$g\$f" $b -Force}
Copy-Item "$r\Nioh3Training\Nioh3Training.asi" $g -Force; Copy-Item "$r\Nioh3BossRematch\bin\Nioh3BossRematch.asi" $g -Force
"installed $(Get-Date) 953c3c2b" | Out-File "$b\installed.txt"
