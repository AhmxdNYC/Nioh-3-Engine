# Round 4: physics solver threads 2 -> 4 (data/physics_threads.ps1). Same setup as round 3's P0 runs (default
# settings, no FG, live patches + F1) so the numbers compare with P0b. Then revert to the shipped data.
$ws  = "C:\Users\Ahmad Hamza\nioh3-engine"
$cfg = "$env:LOCALAPPDATA\KoeiTecmo\NIOH3\config.xml"
$ui  = "$ws\tools\ui.fsx"
$env:FT_ALLOW_WRITE = "1"
function Close-Game { Get-Process Nioh3 -ErrorAction SilentlyContinue | Stop-Process -Force; Start-Sleep 12 }
function Dialog { $d = dotnet fsi $ui dialog Nioh3 2>&1 | Select-String 'dialogs:'; "$d" }
function Run-Launch($name) {
    Close-Game
    & powershell -NoProfile -File "$ws\gpu-shaders\apply_ghdm.ps1" -Revert
    & powershell -NoProfile -File "$ws\optiscaler\apply.ps1" -Revert | Out-Null
    Copy-Item "$ws\configs\default-nofg-120.xml" $cfg -Force
    "=== $name  $(Get-Date -f HH:mm:ss)"
    & powershell -NoProfile -File "$ws\data\physics_threads.ps1" -Status
    & powershell -NoProfile -File "$ws\tools\enter_game.ps1" | Out-Null
    Start-Sleep 15
    Push-Location "$ws\patcher"
    & powershell -NoProfile -File "$ws\patcher\patch_all.ps1" | Where-Object { $_ -match 'fail|error|MISMATCH' }
    dotnet fsi fg_cave.fsx apply 2>$null | Where-Object { $_ -match 'F1' }
    Pop-Location
    Start-Sleep 5
    Dialog
    & powershell -NoProfile -File "$ws\patcher\bench.ps1" "r4-$name" 2>&1 | Where-Object { $_ -match 'displayed|GPU busy|CPU busy' }
    & powershell -NoProfile -File "$ws\patcher\cpuuse.ps1" 15
    dotnet fsi $ui shot "$ws\captures\round4-$name.png" 0 0 2560 1440 2>&1 | Out-Null
    Dialog
}
Close-Game
& powershell -NoProfile -File "$ws\data\physics_threads.ps1" -Threads 4
Run-Launch "phys4"
Close-Game
& powershell -NoProfile -File "$ws\data\physics_threads.ps1" -Revert
Run-Launch "phys2"
Close-Game
& powershell -NoProfile -File "$ws\optiscaler\apply.ps1" | Out-Null
Copy-Item "$ws\configs\main-240-fg.xml" $cfg -Force
& powershell -NoProfile -File "$ws\data\physics_threads.ps1" -Status
"round4 done $(Get-Date -f HH:mm:ss)"
