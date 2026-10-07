# Round 2 A/B: P12 (worker-pool penalty lift, boot-time), F1 (FG presenter wait, in-session on/off), GHDM shader.
# All launches use the user's setup: configs\main-240-fg.xml + OptiScaler + all live patches.
$ws   = "C:\Users\Ahmad Hamza\nioh3-engine"
$cfg  = "$env:LOCALAPPDATA\KoeiTecmo\NIOH3\config.xml"
$ui   = "$ws\tools\ui.fsx"
$env:FT_ALLOW_WRITE = "1"
function Close-Game { Get-Process Nioh3 -ErrorAction SilentlyContinue | Stop-Process -Force; Start-Sleep 12 }
function Dialog { $d = dotnet fsi $ui dialog Nioh3 2>&1 | Select-String 'dialogs:'; "$d" }
function Cores { & powershell -NoProfile -File "$ws\patcher\cpuuse.ps1" 15 }
function Bench($l) { & powershell -NoProfile -File "$ws\patcher\bench.ps1" $l 2>&1 | Where-Object { $_ -match 'displayed|present interval|CPU busy|GPU busy' } }

function Run-Launch($name, [bool]$p12, [bool]$ghdm) {
    Close-Game
    if ($ghdm) { & powershell -NoProfile -File "$ws\gpu-shaders\apply_ghdm.ps1" } else { & powershell -NoProfile -File "$ws\gpu-shaders\apply_ghdm.ps1" -Revert }
    Copy-Item "$ws\configs\main-240-fg.xml" $cfg -Force
    "=== $name  P12=$p12  GHDM=$ghdm  $(Get-Date -f HH:mm:ss)"
    $log = "$env:TEMP\p12_$name.txt"
    if ($p12) {
        Remove-Item $log -ErrorAction SilentlyContinue
        $w = Start-Process dotnet -ArgumentList "fsi `"$ws\patcher\pool_patch.fsx`" 240" -RedirectStandardOutput $log -WindowStyle Hidden -PassThru
        $t0 = Get-Date
        while (-not ((Test-Path $log) -and (Get-Content $log -Raw) -match 'waiting') -and ((Get-Date) - $t0).TotalSeconds -lt 60) { Start-Sleep -Milliseconds 300 }
    }
    & powershell -NoProfile -File "$ws\tools\enter_game.ps1"
    if ($p12) { $w.WaitForExit(30000) | Out-Null; Get-Content $log | Where-Object { $_ -match 'P12' } }
    Start-Sleep 15
    Push-Location "$ws\patcher"
    dotnet fsi threads.fsx 2>&1 | Where-Object { $_ -match 'threads:|KIDS_RESOURCE|StageUpdate' }
    & powershell -NoProfile -File "$ws\patcher\patch_all.ps1"
    dotnet fsi fg_cave.fsx apply 2>&1 | Where-Object { $_ -match 'F1|fail|error' }
    Start-Sleep 5
    dotnet fsi fg_cave.fsx status 2>&1 | Select-Object -First 3
    Pop-Location
    Dialog
    "--- F1 on";  Bench "$name-f1on";  Cores
    Push-Location "$ws\patcher"; dotnet fsi fg_cave.fsx off 2>&1 | Where-Object { $_ -match 'F1' }; Pop-Location
    Start-Sleep 3
    "--- F1 off"; Bench "$name-f1off"; Cores
    Push-Location "$ws\patcher"; dotnet fsi fg_cave.fsx on 2>&1 | Where-Object { $_ -match 'F1' }; Pop-Location
    dotnet fsi $ui shot "$ws\captures\round2-$name.png" 0 0 2560 1440 2>&1 | Out-Null
    Dialog
}

Run-Launch "B1" $true  $false
Run-Launch "A1" $false $false
Run-Launch "B2" $true  $false
Run-Launch "A2" $false $false
Run-Launch "C1" $true  $true
Run-Launch "C2" $true  $true
Close-Game
& powershell -NoProfile -File "$ws\gpu-shaders\apply_ghdm.ps1" -Revert
Copy-Item "$ws\configs\main-240-fg.xml" $cfg -Force
"round2 done $(Get-Date -f HH:mm:ss); shader reverted to original until verified"
