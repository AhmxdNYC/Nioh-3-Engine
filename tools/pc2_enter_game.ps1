# Launch Nioh 3 via Steam and click through to gameplay (autosave notice -> title -> Continue -> tip -> popup).
param([int]$OwnerPid = 0, [int]$MaxMinutes = 30)   # OwnerPid 0 = parent process; see tools\testmatrix\harness_guard.ps1
$ui = "C:\Users\User\projects\nioh3-engine\tools\ui.fsx"
. "C:\Users\User\projects\nioh3-engine\tools\testmatrix\harness_guard.ps1"
Init-Guard -Name "enter_game" -OwnerPid $OwnerPid -MaxMinutes $MaxMinutes
function U { Assert-Owner; dotnet fsi $ui @args 2>&1 | Out-Null }
Assert-Launch
& (Join-Path $PSScriptRoot 'steam/start_autoplay.ps1')
Start-Process "steam://rungameid/3681010"
$t0 = Get-Date
while (-not (Get-Process Nioh3 -ErrorAction SilentlyContinue | Where-Object MainWindowHandle -ne 0) -and ((Get-Date) - $t0).TotalSeconds -lt 150) { Start-Sleep 3 }
Register-Game; if (-not (Get-Process Nioh3 -ErrorAction SilentlyContinue)) { Note-Crash "no game window after launch" }
Start-Sleep 25
U focus Nioh3; U click 1280 887; Start-Sleep 22
U focus Nioh3; U click 1280 1170; Start-Sleep 7
U focus Nioh3; U click 380 1024; Start-Sleep 25
U focus Nioh3; U key 20; Start-Sleep 5; U key 20; Start-Sleep 4
U shot "$env:TEMP\enter_game.png" 640 300 1280 700
"Nioh3 pid $((Get-Process Nioh3).Id); screenshot $env:TEMP\enter_game.png"
Done-Guard -KeepFile
