# Round 5: profile the fully-optimized game (panel applies all live fixes + fast shader).
#   CPU: patcher\cpuprof.fsx sampling for 20 s  ->  captures\cpuprof-r5.txt
#   GPU: one RGP frame via the dev-mode daemon  ->  captures\rgp\profiling_*.rgp
# Config: modified preset, no frame gen (cleanest main-thread picture).
$ws  = "C:\Users\Ahmad Hamza\nioh3-engine"
$cfg = "$env:LOCALAPPDATA\KoeiTecmo\NIOH3\config.xml"
$rgp = "$ws\captures\rgp"
Get-Process Nioh3 -ErrorAction SilentlyContinue | Stop-Process -Force; Start-Sleep 10
Copy-Item "$ws\configs\main-240.xml" $cfg -Force
"rgp daemon: starting"
Remove-Item "$rgp\daemon.state" -ErrorAction SilentlyContinue
Start-Process powershell -WindowStyle Hidden -ArgumentList '-NoProfile','-ExecutionPolicy','Bypass','-File',"`"$ws\patcher\gpu\rgp_daemon.ps1`""
$t0 = Get-Date
while (-not ((Test-Path "$rgp\daemon.state") -and (Get-Content "$rgp\daemon.state") -match 'WAITING') -and ((Get-Date) - $t0).TotalSeconds -lt 60) { Start-Sleep 1 }
"rgp daemon state: $(Get-Content "$rgp\daemon.state" -ErrorAction SilentlyContinue)"
& powershell -NoProfile -File "$ws\tools\enter_game.ps1"
Start-Sleep 45      # the panel applies the live fixes ~20 s after the window appears
$d = dotnet fsi "$ws\tools\ui.fsx" dialog Nioh3 2>&1 | Select-String 'dialogs:'; "$d"
dotnet fsi "$ws\tools\ui.fsx" shot "$ws\captures\round5-ingame.png" 0 0 2560 1440 2>&1 | Out-Null
Push-Location "$ws\patcher"
"fixes in this process:"
dotnet fsi fg_cave.fsx status 2>$null | Select-Object -First 1
dotnet fsi pace_sample.fsx 2 2>$null | Select-Object -First 2
"--- cpu profile 20 s"
dotnet fsi cpuprof.fsx 20 "$ws\captures\cpuprof-r5.txt" 2>&1 | Select-Object -Last 5
Pop-Location
"--- rgp capture"
$before = @(Get-ChildItem $rgp -Filter *.rgp | Select-Object -ExpandProperty Name)
New-Item "$rgp\RGP_TRIGGER" -ItemType File -Force | Out-Null
$t0 = Get-Date
while (((Get-Date) - $t0).TotalSeconds -lt 240) {
    $new = Get-ChildItem $rgp -Filter *.rgp | Where-Object { $before -notcontains $_.Name }
    if ($new -and (Get-Content "$rgp\daemon.state") -match 'IDLE') { break }
    Start-Sleep 3
}
Get-ChildItem $rgp -Filter *.rgp | Where-Object { $before -notcontains $_.Name } | ForEach-Object { "new trace: $($_.FullName) $([math]::Round($_.Length/1MB)) MB" }
Get-Content "$rgp\daemon.log" -Tail 3
Get-Process Nioh3 -ErrorAction SilentlyContinue | Stop-Process -Force; Start-Sleep 8
Get-Process RadeonDeveloperPanelCLI -ErrorAction SilentlyContinue | Stop-Process -Force
Get-CimInstance Win32_Process -Filter "Name='powershell.exe'" | Where-Object { $_.CommandLine -match 'rgp_daemon' } | ForEach-Object { Stop-Process -Id $_.ProcessId -Force }
Copy-Item "$ws\configs\main-240-fg.xml" $cfg -Force
"round5 done $(Get-Date -f HH:mm:ss)"
