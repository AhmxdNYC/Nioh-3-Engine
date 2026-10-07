# Round 6: Param::Parallel 2710f411 ParallelUnitNum 0 -> 32 (object update / script evaluation split into jobs).
# Default settings, no FG, all live fixes, fast shader. A/B/A/B launches; each: crash check, bench, CPU profile of
# the main thread (cpuprof, 10 s). Ends with the shipped system DB restored.
$ws  = "C:\Users\Ahmad Hamza\nioh3-engine"
$cfg = "$env:LOCALAPPDATA\KoeiTecmo\NIOH3\config.xml"
$ui  = "$ws\tools\ui.fsx"
$env:FT_ALLOW_WRITE = "1"
function Close-Game { Get-Process Nioh3 -ErrorAction SilentlyContinue | Stop-Process -Force; Start-Sleep 12 }
function Dialog { $d = dotnet fsi $ui dialog Nioh3 2>&1 | Select-String 'dialogs:'; "$d" }
function Run-Launch($name, [int]$units) {
    Close-Game
    & powershell -NoProfile -File "$ws\data\sysdb_tweaks.ps1" -ObjectUpdateUnits $units | Select-Object -Last 1
    & powershell -NoProfile -File "$ws\optiscaler\apply.ps1" -Revert | Out-Null
    Copy-Item "$ws\configs\default-nofg-120.xml" $cfg -Force
    "=== $name  ObjectUpdateUnits=$units  $(Get-Date -f HH:mm:ss)"
    & powershell -NoProfile -File "$ws\tools\enter_game.ps1" | Out-Null
    Start-Sleep 15
    if (-not (Get-Process Nioh3 -ErrorAction SilentlyContinue)) { "GAME EXITED during load"; return }
    Dialog
    Push-Location "$ws\patcher"
    & powershell -NoProfile -File "$ws\patcher\patch_all.ps1" | Where-Object { $_ -match 'fail|error|MISMATCH' }
    dotnet fsi fg_cave.fsx apply 2>$null | Where-Object { $_ -match 'F1' }
    Pop-Location
    Start-Sleep 5
    & powershell -NoProfile -File "$ws\patcher\bench.ps1" "r6-$name" 2>&1 | Where-Object { $_ -match 'displayed|GPU busy|CPU busy' }
    & powershell -NoProfile -File "$ws\patcher\cpuuse.ps1" 15
    Push-Location "$ws\patcher"
    dotnet fsi cpuprof.fsx 10 "$ws\captures\cpuprof-r6-$name.txt" 2>&1 | Out-Null
    Pop-Location
    $g = Select-String "$ws\captures\cpuprof-r6-$name.txt" -Pattern '^\d+\s+[\d.]+\s+[\d.]+\s+\S+\s+\d+\s+\S+\s+\[App\]Game' | Select-Object -First 1
    "main thread: $($g.Line)"
    dotnet fsi $ui shot "$ws\captures\round6-$name.png" 0 0 2560 1440 2>&1 | Out-Null
    # extra soak: 60 s with camera movement, crash check
    for ($k = 0; $k -lt 4; $k++) {
        dotnet fsi $ui focus Nioh3 2>&1 | Out-Null
        dotnet fsi $ui pan 25 300 12 2>&1 | Out-Null
        Start-Sleep 12
        if (-not (Get-Process Nioh3 -ErrorAction SilentlyContinue)) { "GAME EXITED during soak"; break }
    }
    Dialog
}
Run-Launch "A1" 0
Run-Launch "B1" 32
Run-Launch "A2" 0
Run-Launch "B2" 32
Close-Game
& powershell -NoProfile -File "$ws\data\sysdb_tweaks.ps1" -Revert | Select-Object -Last 1
& powershell -NoProfile -File "$ws\optiscaler\apply.ps1" | Out-Null
Copy-Item "$ws\configs\main-240-fg.xml" $cfg -Force
"round6 done $(Get-Date -f HH:mm:ss)"
