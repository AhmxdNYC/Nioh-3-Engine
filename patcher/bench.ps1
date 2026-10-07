# 20 s PresentMon capture while the camera sweeps left/right, then print stats.
# usage: powershell -File bench.ps1 <label>
param([string]$Label = "run")
$ws  = "C:\Users\Ahmad Hamza\nioh3-engine"
$csv = "$ws\captures\bench-$Label.csv"
$pm  = Start-Process "$ws\tools\PresentMon.exe" -PassThru -WindowStyle Hidden -ArgumentList `
    "--process_name Nioh3.exe --output_file `"$csv`" --timed 20 --terminate_after_timed --stop_existing_session --no_console_stats"
Start-Sleep 2
Push-Location "$ws\tools"
dotnet fsi ui.fsx focus Nioh3 2>&1 | Out-Null
for ($k = 0; $k -lt 4; $k++) {
    dotnet fsi ui.fsx pan 12 220 10 2>&1 | Out-Null
    dotnet fsi ui.fsx pan -12 220 10 2>&1 | Out-Null
}
Pop-Location
$pm.WaitForExit(30000) | Out-Null
py "$ws\patcher\pmstats.py" $csv 2>&1 | Select-Object -Skip 2
