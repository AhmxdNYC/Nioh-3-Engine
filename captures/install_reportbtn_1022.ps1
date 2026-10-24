# waits for Nioh3 to close, then installs Training (autoruntime-all merge) + Rematch (report button) from nioh3-merge1025
$g="C:\Program Files (x86)\Steam\steamapps\common\Nioh3"; $r="C:\Users\Ahmad Hamza\nioh3-merge1025\mod"
$b="C:\Users\Ahmad Hamza\nioh3-engine\captures\pc\play-20261022-pre-report"; New-Item -ItemType Directory -Force $b|Out-Null
while(Get-Process Nioh3 -ErrorAction SilentlyContinue){Start-Sleep 10}
Copy-Item "$g\Nioh3Training.asi","$g\Nioh3BossRematch.asi" $b -Force
Copy-Item "$r\Nioh3BossRematch\bin\Nioh3BossRematch.asi" $g -Force
Copy-Item "$r\Nioh3Training\Nioh3Training.asi" $g -Force
"installed $(Get-Date) report-button + autoruntime-all" | Out-File "$b\installed.txt"
