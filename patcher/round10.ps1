# Round 10 (every bench gated on real gameplay via ingame.py; screenshots kept in captures\checks).
# Modified preset, no FG, OptiScaler, panel fixes + fast GHDM shader.
#  L1 (RivaTuner as-is): live A/B of patch B (gallop) and AABB (logic_live), then a deep 128 KB-stack profile.
#  Then waits for the user to exit MSI Afterburner + RivaTuner (polls up to 25 min).
#  L2 RTSS off, VSync ON; L3 RTSS off, VSync OFF; L4..L7 WDO#28 blur orig/fast/orig/fast (RTSS off).
. "$PSScriptRoot\common.ps1"
$base = Get-Content "$ws\configs\main-240.xml" -Raw
function Enter-Game($vsync = "ON") {
    Close-Game
    & powershell -NoProfile -File "$ws\optiscaler\apply.ps1" | Out-Null
    [IO.File]::WriteAllText($cfg, ($base -replace '<VSYNC>[^<]*</VSYNC>', "<VSYNC>$vsync</VSYNC>"))
    & powershell -NoProfile -File "$ws\tools\enter_game.ps1" | Out-Null
    Start-Sleep 30
    Dialog
}
$logic = "$ws\re\scripts_logic\logic_live.fsx"
Push-Location "$ws\patcher"
& powershell -NoProfile -File "$ws\gpu-shaders\apply_wdo28.ps1" -Revert | Out-Null
"=== L1 RTSS=$([bool](Get-Process RTSS -ErrorAction SilentlyContinue)) vsync=ON $(Get-Date -f HH:mm:ss)"
Enter-Game "ON"
Bench-Checked "r10-L1-base"
dotnet fsi gallop_live.fsx apply 2>$null | Select-String "gallop|fail"
Bench-Checked "r10-B-on1"
dotnet fsi gallop_live.fsx revert 2>$null | Select-String "gallop|fail"
Bench-Checked "r10-B-off1"
dotnet fsi gallop_live.fsx apply 2>$null | Select-String "gallop|fail"
Bench-Checked "r10-B-on2"
dotnet fsi gallop_live.fsx revert 2>$null | Select-String "gallop|fail"
Bench-Checked "r10-B-off2"
dotnet fsi gallop_live.fsx apply 2>$null | Select-String "gallop|fail"
dotnet fsi $logic apply 2>$null | Select-String "appl|fail|error"
Bench-Checked "r10-AABB-on1"
dotnet fsi $logic off 2>$null | Select-String "off|fail"
Bench-Checked "r10-AABB-off1"
dotnet fsi $logic on 2>$null | Select-String "on|fail"
Bench-Checked "r10-AABB-on2"
"--- deep profile (128 KB stacks) $(Get-Date -f HH:mm:ss)"
if (Ensure-InGame) { dotnet fsi cpuprof.fsx 20 "D:\nioh3-re\logic\cpuprof-r10-deep.txt" 1000 2 24 131072 2>&1 | Select-Object -Last 2 }
Dialog
"=== waiting for the user to exit MSI Afterburner + RivaTuner $(Get-Date -f HH:mm:ss)"
$t0 = Get-Date
while ((Get-Process RTSS, MSIAfterburner -ErrorAction SilentlyContinue) -and ((Get-Date) - $t0).TotalMinutes -lt 25) { Start-Sleep 10 }
"RTSS running: $([bool](Get-Process RTSS -ErrorAction SilentlyContinue))  $(Get-Date -f HH:mm:ss)"
"=== L2 RTSS off, vsync ON"
Enter-Game "ON"
Bench-Checked "r10-L2-rtssoff-vsON-1"
Bench-Checked "r10-L2-rtssoff-vsON-2"
"=== L3 RTSS off, vsync OFF"
Enter-Game "OFF"
Bench-Checked "r10-L3-rtssoff-vsOFF-1"
Bench-Checked "r10-L3-rtssoff-vsOFF-2"
foreach ($r in @(@("orig1", $false), @("fast1", $true), @("orig2", $false), @("fast2", $true))) {
    Close-Game
    if ($r[1]) { & powershell -NoProfile -File "$ws\gpu-shaders\apply_wdo28.ps1" | Out-Null } else { & powershell -NoProfile -File "$ws\gpu-shaders\apply_wdo28.ps1" -Revert | Out-Null }
    "=== WDO $($r[0]) $(& powershell -NoProfile -File "$ws\gpu-shaders\apply_wdo28.ps1" -Status)"
    Enter-Game "ON"
    Bench-Checked "r10-wdo-$($r[0])"
    U shot "$ws\captures\round10-wdo-$($r[0]).png" 0 0 2560 1440
}
Pop-Location
Close-Game
& powershell -NoProfile -File "$ws\gpu-shaders\apply_wdo28.ps1" -Revert | Out-Null
Copy-Item "$ws\configs\main-240-fg.xml" $cfg -Force
"round10 done $(Get-Date -f HH:mm:ss)"
