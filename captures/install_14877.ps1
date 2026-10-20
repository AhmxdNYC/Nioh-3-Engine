$g="C:\Program Files (x86)\Steam\steamapps\common\Nioh3"; $r="C:\Users\Ahmad Hamza\nioh3-seqfix\mod"
while(Get-Process Nioh3 -ErrorAction SilentlyContinue){Start-Sleep 5}
$b="C:\Users\Ahmad Hamza\nioh3-engine\captures\pc\play-20261020-pre14877"; New-Item -ItemType Directory -Force $b|Out-Null
Copy-Item "$g\Nioh3Training.asi" $b -Force
Copy-Item "$r\Nioh3Training\Nioh3Training.asi" $g -Force
"installed $(Get-Date) b52c451d" | Out-File "$b\installed.txt"
