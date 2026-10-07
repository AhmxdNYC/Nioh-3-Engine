# Round 3 (frame gen OFF, steadier numbers): GHDM shader on the GPU-heavier modified preset; P12 and CPU_SPEC_CLASS
# on the CPU-bound default settings. All launches get the live patches (patch_all + F1).
$ws  = "C:\Users\Ahmad Hamza\nioh3-engine"
$cfg = "$env:LOCALAPPDATA\KoeiTecmo\NIOH3\config.xml"
$ui  = "$ws\tools\ui.fsx"
$env:FT_ALLOW_WRITE = "1"
function Close-Game { Get-Process Nioh3 -ErrorAction SilentlyContinue | Stop-Process -Force; Start-Sleep 12 }
function Dialog { $d = dotnet fsi $ui dialog Nioh3 2>&1 | Select-String 'dialogs:'; "$d" }
function Run-Launch($name, $config, [bool]$opti, [bool]$p12, [bool]$ghdm) {
    Close-Game
    if ($ghdm) { & powershell -NoProfile -File "$ws\gpu-shaders\apply_ghdm.ps1" } else { & powershell -NoProfile -File "$ws\gpu-shaders\apply_ghdm.ps1" -Revert }
    if ($opti) { & powershell -NoProfile -File "$ws\optiscaler\apply.ps1" | Out-Null } else { & powershell -NoProfile -File "$ws\optiscaler\apply.ps1" -Revert | Out-Null }
    $dx = Test-Path "C:\Program Files (x86)\Steam\steamapps\common\Nioh3\dxgi.dll"
    Copy-Item "$ws\configs\$config" $cfg -Force
    "=== $name  cfg=$config  OptiScaler=$dx  P12=$p12  GHDM=$ghdm  $(Get-Date -f HH:mm:ss)"
    $log = "$env:TEMP\p12_r3_$name_$(Get-Random).txt"
    if ($p12) {
        $w = Start-Process dotnet -ArgumentList "fsi `"$ws\patcher\pool_patch.fsx`" 240" -RedirectStandardOutput $log -WindowStyle Hidden -PassThru
        $t0 = Get-Date
        while (-not ((Test-Path $log) -and (Get-Content $log -Raw) -match 'waiting') -and ((Get-Date) - $t0).TotalSeconds -lt 60) { Start-Sleep -Milliseconds 300 }
    }
    & powershell -NoProfile -File "$ws\tools\enter_game.ps1" | Out-Null
    if ($p12) { $w.WaitForExit(30000) | Out-Null; Get-Content $log | Where-Object { $_ -match 'P12' } }
    Start-Sleep 15
    Push-Location "$ws\patcher"
    dotnet fsi pools.fsx 2>$null | Select-Object -Skip 1 -First 2
    & powershell -NoProfile -File "$ws\patcher\patch_all.ps1" | Where-Object { $_ -match 'fail|error|MISMATCH' }
    dotnet fsi fg_cave.fsx apply 2>$null | Where-Object { $_ -match 'F1' }
    Pop-Location
    Start-Sleep 5
    Dialog
    & powershell -NoProfile -File "$ws\patcher\bench.ps1" "r3-$name" 2>&1 | Where-Object { $_ -match 'displayed|GPU busy|CPU busy' }
    & powershell -NoProfile -File "$ws\patcher\cpuuse.ps1" 15
    dotnet fsi $ui shot "$ws\captures\round3-$name.png" 0 0 2560 1440 2>&1 | Out-Null
    Dialog
}
# GHDM: modified preset, no FG (GPU ~ CPU), P12 off to isolate
Run-Launch "G0a" "main-240.xml" $true $false $false
Run-Launch "G1a" "main-240.xml" $true $false $true
Run-Launch "G0b" "main-240.xml" $true $false $false
Run-Launch "G1b" "main-240.xml" $true $false $true
# P12 and CPU_SPEC_CLASS: default settings, no FG, CPU-bound, stock shader
Run-Launch "P0a" "default-nofg-120.xml" $false $false $false
Run-Launch "P1a" "default-nofg-120.xml" $false $true  $false
Run-Launch "S1a" "default-nofg-120-cpuhigh.xml" $false $false $false
Run-Launch "P0b" "default-nofg-120.xml" $false $false $false
Run-Launch "P1b" "default-nofg-120.xml" $false $true  $false
Run-Launch "S1b" "default-nofg-120-cpuhigh.xml" $false $false $false
Close-Game
& powershell -NoProfile -File "$ws\gpu-shaders\apply_ghdm.ps1" -Revert
& powershell -NoProfile -File "$ws\optiscaler\apply.ps1" | Out-Null
Copy-Item "$ws\configs\main-240-fg.xml" $cfg -Force
"round3 done $(Get-Date -f HH:mm:ss); restored OptiScaler + main-240-fg + original shader"
