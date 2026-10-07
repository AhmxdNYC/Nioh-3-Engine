# Round 7: procedural foliage/prop scatter distances x0.6 on all maps (data\tools\n3foliage.py) vs shipped.
# Default settings, no FG, all live fixes, fast shader. A/B/A/B. Ends with the shipped data restored.
$ws  = "C:\Users\Ahmad Hamza\nioh3-engine"
$pkg = "C:\Program Files (x86)\Steam\steamapps\common\Nioh3\package"
$bak = "$ws\data\foliage_backup"
$cfg = "$env:LOCALAPPDATA\KoeiTecmo\NIOH3\config.xml"
$ui  = "$ws\tools\ui.fsx"
$env:FT_ALLOW_WRITE = "1"
function Close-Game { Get-Process Nioh3 -ErrorAction SilentlyContinue | Stop-Process -Force; Start-Sleep 12 }
function Dialog { $d = dotnet fsi $ui dialog Nioh3 2>&1 | Select-String 'dialogs:'; "$d" }
function Run-Launch($name, [double]$scale) {
    Close-Game
    py -I "$ws\data\tools\n3foliage.py" apply "$pkg" "$bak" $scale 2>&1 | Select-Object -Last 1
    & powershell -NoProfile -File "$ws\optiscaler\apply.ps1" -Revert | Out-Null
    Copy-Item "$ws\configs\default-nofg-120.xml" $cfg -Force
    "=== $name  foliage scale=$scale  $(Get-Date -f HH:mm:ss)"
    & powershell -NoProfile -File "$ws\tools\enter_game.ps1" | Out-Null
    Start-Sleep 15
    if (-not (Get-Process Nioh3 -ErrorAction SilentlyContinue)) { "GAME EXITED during load"; return }
    Push-Location "$ws\patcher"
    & powershell -NoProfile -File "$ws\patcher\patch_all.ps1" | Where-Object { $_ -match 'fail|error|MISMATCH' }
    dotnet fsi fg_cave.fsx apply 2>$null | Where-Object { $_ -match 'F1' }
    Pop-Location
    Start-Sleep 5
    Dialog
    & powershell -NoProfile -File "$ws\patcher\bench.ps1" "r7-$name" 2>&1 | Where-Object { $_ -match 'displayed|GPU busy|CPU busy' }
    & powershell -NoProfile -File "$ws\patcher\cpuuse.ps1" 15
    Push-Location "$ws\patcher"
    dotnet fsi cpuprof.fsx 10 "$ws\captures\cpuprof-r7-$name.txt" 2>&1 | Out-Null
    Pop-Location
    $g = Select-String "$ws\captures\cpuprof-r7-$name.txt" -Pattern '^\d+\s+[\d.]+\s+[\d.]+\s+\S+\s+\d+\s+\S+\s+\[App\]Game' | Select-Object -First 1
    "main thread: $($g.Line)"
    dotnet fsi $ui shot "$ws\captures\round7-$name.png" 0 0 2560 1440 2>&1 | Out-Null
    Dialog
}
Run-Launch "A1" 1.0
Run-Launch "B1" 0.6
Run-Launch "A2" 1.0
Run-Launch "B2" 0.6
Close-Game
py -I "$ws\data\tools\n3foliage.py" revert "$pkg" "$bak" 2>&1 | Select-Object -Last 1
& powershell -NoProfile -File "$ws\optiscaler\apply.ps1" | Out-Null
Copy-Item "$ws\configs\main-240-fg.xml" $cfg -Force
"round7 done $(Get-Date -f HH:mm:ss)"
