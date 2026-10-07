# Round 9: (1) core reservation for the main thread (coreaff.fsx) and (2) main-thread patches A+D (mainthread_live.fsx),
# both live A/B/A/B inside one CPU-bound launch (default settings, no FG); (3) WDO#28 fast sun-shadow blur shader,
# 4 launches (modified preset, no FG) comparing GPU busy. The panel (running) applies the standard live fixes.
$ws  = "C:\Users\Ahmad Hamza\nioh3-engine"
$cfg = "$env:LOCALAPPDATA\KoeiTecmo\NIOH3\config.xml"
$ui  = "$ws\tools\ui.fsx"
$env:FT_ALLOW_WRITE = "1"
function Close-Game { Get-Process Nioh3 -ErrorAction SilentlyContinue | Stop-Process -Force; Start-Sleep 12 }
function Dialog { $d = dotnet fsi $ui dialog Nioh3 2>&1 | Select-String 'dialogs:'; "$d" }
function Bench($l) {
    "--- $l $(Get-Date -f HH:mm:ss)"
    & powershell -NoProfile -File "$ws\patcher\bench.ps1" "r9-$l" 2>&1 | Where-Object { $_ -match 'displayed|GPU busy|CPU busy' }
}
function Enter($config, [bool]$opti) {
    Close-Game
    if ($opti) { & powershell -NoProfile -File "$ws\optiscaler\apply.ps1" | Out-Null } else { & powershell -NoProfile -File "$ws\optiscaler\apply.ps1" -Revert | Out-Null }
    Copy-Item "$ws\configs\$config" $cfg -Force
    & powershell -NoProfile -File "$ws\tools\enter_game.ps1" | Out-Null
    Start-Sleep 30
    Dialog
}
Push-Location "$ws\patcher"
# ---- (1)+(2): one CPU-bound launch
& powershell -NoProfile -File "$ws\gpu-shaders\apply_wdo28.ps1" -Revert | Out-Null
"=== CPU launch (default settings, no FG)"
Enter "default-nofg-120.xml" $false
Bench "cpu-base1"
dotnet fsi coreaff.fsx apply 0 2>$null | Select-String 'applied|main thread'
Bench "core0-on1"
dotnet fsi coreaff.fsx revert 2>$null | Select-String 'reverted'
Bench "core0-off"
dotnet fsi coreaff.fsx apply 0 2>$null | Select-String 'applied'
Bench "core0-on2"
dotnet fsi coreaff.fsx revert 2>$null | Select-String 'reverted'
Bench "cpu-base2"
dotnet fsi mainthread_live.fsx apply A,D 2>$null | Select-String 'patched|UNKNOWN|fail'
Bench "AD-on1"
dotnet fsi mainthread_live.fsx revert A,D 2>$null | Select-String 'reverted|fail'
Bench "AD-off"
dotnet fsi mainthread_live.fsx apply A,D 2>$null | Select-String 'patched|fail'
Bench "AD-on2"
dotnet fsi gallop_live.fsx apply 2>$null | Select-String "gallop|fail"
Bench "ADB-on1"
dotnet fsi gallop_live.fsx revert 2>$null | Select-String "gallop|fail"
Bench "ADB-Boff"
dotnet fsi gallop_live.fsx apply 2>$null | Select-String "gallop|fail"
Bench "ADB-on2"
dotnet fsi coreaff.fsx apply 0 2>$null | Select-String "applied"
Bench "ALL-core0"
dotnet fsi coreaff.fsx revert 2>$null | Select-String "reverted"
$mc = "$ws\re\scripts_meshlet\meshlet_caves.fsx"
dotnet fsi $mc apply C419,P4AA,P1E7 2>$null | Select-String "applied|cave|fail|error|refus"
dotnet fsi $mc off C419,P4AA,P1E7 2>$null | Select-String "off|fail"
Bench "mesh-off1"
dotnet fsi $mc on C419,P4AA,P1E7 2>$null | Select-String "on|fail"
Bench "mesh-on1"
dotnet fsi $mc off C419,P4AA,P1E7 2>$null | Select-String "off|fail"
Bench "mesh-off2"
dotnet fsi $mc on C419,P4AA,P1E7 2>$null | Select-String "on|fail"
Bench "mesh-on2"
dotnet fsi w1b.fsx apply 2>$null | Select-String "W1b|fail"
Bench "w1b-on1"
dotnet fsi w1b.fsx revert 2>$null | Select-String "W1|fail"
Bench "w1b-off"
dotnet fsi w1b.fsx apply 2>$null | Select-String "W1b|fail"
Bench "w1b-on2"
Push-Location "$ws\patcher"; dotnet fsi cpuprof.fsx 10 "$ws\captures\cpuprof-r9-all.txt" 2>&1 | Out-Null; Pop-Location
Dialog
# ---- (3): WDO#28 shader, GPU busy on the modified preset
foreach ($r in @(@("wdo-orig1", $false), @("wdo-fast1", $true), @("wdo-orig2", $false), @("wdo-fast2", $true))) {
    Close-Game
    if ($r[1]) { & powershell -NoProfile -File "$ws\gpu-shaders\apply_wdo28.ps1" } else { & powershell -NoProfile -File "$ws\gpu-shaders\apply_wdo28.ps1" -Revert }
    "=== $($r[0])"
    Enter "main-240.xml" $true
    dotnet fsi $ui shot "$ws\captures\round9-$($r[0]).png" 0 0 2560 1440 2>&1 | Out-Null
    Bench $r[0]
    Dialog
}
Pop-Location
Close-Game
& powershell -NoProfile -File "$ws\gpu-shaders\apply_wdo28.ps1" -Revert | Out-Null
& powershell -NoProfile -File "$ws\optiscaler\apply.ps1" | Out-Null
Copy-Item "$ws\configs\main-240-fg.xml" $cfg -Force
"round9 done $(Get-Date -f HH:mm:ss)"
