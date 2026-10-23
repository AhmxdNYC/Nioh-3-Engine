$g="C:\Program Files (x86)\Steam\steamapps\common\Nioh3"
while(Get-Process Nioh3 -ErrorAction SilentlyContinue){Start-Sleep 10}
Copy-Item "C:\Users\Ahmad Hamza\nioh3-merge1025\mod\Nioh3Training\Nioh3Training_actionsets.tsv" $g -Force
"installed $(Get-Date)" | Out-File "C:\Users\Ahmad Hamza\nioh3-engine\captures\pc\play-20261022-pre-report\actionsets-installed.txt"
