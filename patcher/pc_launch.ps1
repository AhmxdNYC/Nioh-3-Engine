# usage: pc_launch.ps1 <label> <seconds> [menu]   (launch, enter game, shot, PresentMon, optional F10 menu shot)
param([string]$L, [int]$Sec = 60, [switch]$Menu, [switch]$NoLaunch, [int]$OwnerPid = 0, [int]$MaxMinutes = 30)
. "$PSScriptRoot\common.ps1"
. "$PSScriptRoot\..\tools\testmatrix\harness_guard.ps1"
Init-Guard -Name "pc_launch" -OwnerPid $OwnerPid -MaxMinutes $MaxMinutes
$gd = "C:\Program Files (x86)\Steam\steamapps\common\Nioh3"
$out = "$ws\captures\pc"
if (-not $NoLaunch) {
  Close-Game
  Remove-Item "$gd\Nioh3EngineFix.log" -EA 0
  & powershell -NoProfile -File "$ws\tools\enter_game.ps1" -OwnerPid $HG.ownerPid | Out-Null
  Start-Sleep 20
}
"ingame=$(Ensure-InGame)"
U shot "$out\$L-overlay.png" 0 0 2560 1440
if ($Menu) {
  U key 79; Start-Sleep 2; U shot "$out\$L-menu.png" 0 0 2560 1440; U key 79; Start-Sleep 2
}
$csv = "$out\$L.csv"
$pm = Start-Process "$ws\tools\PresentMon.exe" -PassThru -WindowStyle Hidden -ArgumentList "--process_name Nioh3.exe --output_file `"$csv`" --timed $Sec --terminate_after_timed --stop_existing_session --no_console_stats"
Register-Child $pm "PresentMon"
Start-Sleep 2
Push-Location "$ws\tools"
$t0 = Get-Date
while (((Get-Date) - $t0).TotalSeconds -lt ($Sec - 4)) {
  dotnet fsi ui.fsx pan 12 220 10 2>&1 | Out-Null
  dotnet fsi ui.fsx pan -12 220 10 2>&1 | Out-Null
}
Pop-Location
$pm.WaitForExit(30000) | Out-Null
"ingame_after=$(Test-InGame "$out\$L-after.png")"
py "$ws\patcher\pmstats.py" $csv 2>&1 | Select-Object -Skip 2
Copy-Item "$gd\Nioh3EngineFix.log" "$out\$L.log" -EA 0
Done-Guard
