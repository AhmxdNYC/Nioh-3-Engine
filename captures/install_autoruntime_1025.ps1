# waits for Nioh3 to close, then installs merged autoruntime+polish Training asi (pc2/autoruntime 5fd71463 + pc/polish-23924)
$g="C:\Program Files (x86)\Steam\steamapps\common\Nioh3"; $s="C:\Users\Ahmad Hamza\nioh3-merge1025\mod\Nioh3Training"
$b="C:\Users\Ahmad Hamza\nioh3-engine\captures\pc\play-20261022-pre-autoruntime"
while(Get-Process Nioh3 -ErrorAction SilentlyContinue){Start-Sleep 10}
Copy-Item "$g\Nioh3Training.asi" "$b\Nioh3Training.asi.prev" -Force
Copy-Item "$s\Nioh3Training.asi","$s\Nioh3Training_arenas.tsv" $g -Force
"installed $(Get-Date) autoruntime+polish" | Out-File "$b\installed.txt"
