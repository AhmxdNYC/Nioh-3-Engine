# infl.ps1 - pc/1 #111856: Inflate default-ON live check, Main PC. EF 6b3bb311 (pc/inflate-on-1823 14293fa9), other ASIs as installed.
# Arm A: 1.8.22 release ini (explicit Inflate = 0, no InflateDefaultV) -> expect migration to Inflate = 1 + marker, "Inflate: ON".
# Arm B: set Inflate = 0 (marker kept) -> relaunch -> must stay 0, no "Inflate: ON". Restore everything.
param([int]$OwnerPid = 0, [string]$Arm = "A", [int]$N = 40)
$ErrorActionPreference = "Continue"
$tm = "C:\Users\Ahmad Hamza\nioh3-engine\tools\testmatrix"
. "$tm\harness_guard.ps1"
. "$tm\vpad.ps1"
$G = "C:\Program Files (x86)\Steam\steamapps\common\Nioh3"
$base = "C:\Users\Ahmad Hamza\nioh3-engine\Temp\h98bis"; $Stage = "$base\stage"
$out = "$base\arm$Arm"; New-Item -ItemType Directory -Force "$out\orig" | Out-Null
$trace = "$out\trace.txt"
function T([string]$s) { $l = "[{0}] {1}" -f (Get-Date -f HH:mm:ss), $s; [IO.File]::AppendAllText($trace, $l + "`r`n"); Write-Host $l }
$uo = "C:\Users\Ahmad Hamza\universal-optimizer\agent-bridge"
$env:UO_BRIDGE_TOKEN_FILE = "$env:APPDATA\uo-bridge\token"; $env:UO_AGENT_ID = "pc"
function Lock([string]$a) { Push-Location $uo; try { $o = (py -m uo_bridge.gamelock $a nioh3 --ttl 3600 2>&1 | Out-String).Trim(); @{ code = $LASTEXITCODE; out = $o } } finally { Pop-Location } }
$inis = @("Nioh3EngineFix.ini","Nioh3CamKiProbe.ini","Nioh3Training.ini","Nioh3BossRematch.ini","Nioh3Training_learned.ini")
$logs = @("Nioh3EngineFix.log","Nioh3CamKiProbe.log","Nioh3Training.log","Nioh3BossRematch.log")
$ini = "$G\Nioh3EngineFix.ini"
$script:gamePid = 0; $script:locked = $false; $script:installed = $false
$saveDir = "$env:LOCALAPPDATA\KoeiTecmo\NIOH3"; $script:saveBak = $null
function Close-Game {
    $p = if ($script:gamePid) { Get-Process -Id $script:gamePid -ErrorAction SilentlyContinue } else { $null }
    if ($p) { Note-Closed; $null = $p.CloseMainWindow(); $p.WaitForExit(30000) | Out-Null; if (-not $p.HasExited) { T "!! game did not exit in 30 s - killing $($p.Id)"; Stop-Process -Id $p.Id -Force }; Start-Sleep 5 }
    $script:gamePid = 0
}
function Restore {
    if ($script:installed -and -not (Get-Process Nioh3 -ErrorAction SilentlyContinue)) {
        foreach ($f in @("Nioh3EngineFix.asi") + $inis) { if (Test-Path "$out\orig\$f") { Copy-Item "$out\orig\$f" "$G\$f" -Force } }
        $script:installed = $false; T "EF asi + inis restored: EF $((Get-FileHash "$G\Nioh3EngineFix.asi").Hash.Substring(0,8).ToLower())"
    }
    if ($script:saveBak -and -not (Get-Process Nioh3 -ErrorAction SilentlyContinue)) { robocopy $script:saveBak $saveDir /MIR /R:2 /W:2 /NFL /NDL /NP /NJH | Out-Null; T "saves restored robocopy $LASTEXITCODE"; $script:saveBak = $null }
    if ($script:locked) { $r = Lock "release"; $script:locked = $false; T "lock released: $($r.out)" }
}
function ReadShared([string]$f) { if (-not (Test-Path $f)) { return "" }; $fs = [IO.File]::Open($f, "Open", "Read", "ReadWrite"); try { (New-Object IO.StreamReader($fs)).ReadToEnd() } finally { $fs.Dispose() } }
function IniKeys { $t = [IO.File]::ReadAllText($ini); $m = [regex]::Matches($t, "(?m)^(MaxFps|Tier120|Tier120Floor|GovFpsFix|GovTrace|VirtualPad|StockSites) *=.*$"); ($m | ForEach-Object { $_.Value.Trim() }) -join "; " }

function SetVrr([string]$k, [string]$v) { $t = [IO.File]::ReadAllText($ini)
    if ($t -match "(?m)^$k *=") { $t = $t -replace "(?m)^$k *=.*", "$k = $v" } else { $t = $t -replace "(?m)^\[VRR\]\r?\n", "[VRR]`r`n$k = $v`r`n" }
    [IO.File]::WriteAllText($ini, $t, (New-Object Text.UTF8Encoding($false))) }
Init-Guard -Name "h98bis-$Arm" -OwnerPid $OwnerPid -MaxMinutes 14 -Cleanup { Close-Game; Restore }
$result = [ordered]@{ arm = $Arm }
try {
    if (Get-Process Nioh3 -ErrorAction SilentlyContinue) { throw "Nioh3 is running" }
    $r = Lock "acquire"; if ($r.code -ne 0) { throw "lock: $($r.out)" }; $script:locked = $true; T "lock: $($r.out)"
    foreach ($f in @("Nioh3EngineFix.asi") + $inis) { if (Test-Path "$G\$f") { Copy-Item "$G\$f" "$out\orig\" -Force } }
    foreach ($l in $logs) { if (Test-Path "$G\$l") { Copy-Item "$G\$l" "$out\orig\$l.before" -Force } }
    $stamp = Get-Date -f yyyyMMdd_HHmmss; $sb = "D:\nioh3-re\save-backups\$stamp-h98bis$Arm"; New-Item -ItemType Directory -Force $sb | Out-Null
    robocopy $saveDir $sb /MIR /R:2 /W:2 /NFL /NDL /NP /NJH | Out-Null; $script:saveBak = $sb; T "saves copied to $sb (robocopy $LASTEXITCODE)"
    $script:installed = $true
    Copy-Item "$Stage\Nioh3EngineFix.asi" "$G\Nioh3EngineFix.asi" -Force
    $t = [IO.File]::ReadAllText($ini); $t = Force-Vpad $t
    if ($t -match "(?m)^AutoContinueLoad *=") { $t = $t -replace "(?m)^AutoContinueLoad *=.*", "AutoContinueLoad = 1" }
    [IO.File]::WriteAllText($ini, $t, (New-Object Text.UTF8Encoding($false)))
    SetVrr "Tier120" "1"; SetVrr "MaxFps" "60"; SetVrr "GovTrace" "1"
    SetVrr "Tier120Floor" "0"; SetVrr "GovFpsFix" "0"
    $sites = @{ D = "P3a,P3b,P3c,P3d"; E = "P3e,P3f,P3g,P6"; F = "P8a,P8b,P8c,P8d,P9" }[$Arm]; if (-not $sites) { throw "unknown arm $Arm" }; SetVrr "StockSites" $sites
    T "installed EF $((Get-FileHash "$G\Nioh3EngineFix.asi").Hash.Substring(0,8).ToLower()); ini: $(IniKeys)"
    & "C:\Program Files (x86)\Steam\steam.exe" -shutdown; $s0 = Get-Date; while ((Get-Process steam -ErrorAction SilentlyContinue) -and ((Get-Date) - $s0).TotalSeconds -lt 60) { Start-Sleep 2 }
    Start-Process "C:\Program Files (x86)\Steam\steam.exe" -ArgumentList "-silent"; Start-Sleep 45; T "steam restarted"
    Remove-Item "$G\Nioh3EngineFix.log" -Force -ErrorAction SilentlyContinue
    Assert-Launch
    & "C:\Users\Ahmad Hamza\nioh3-engine\tools\steam\start_autoplay.ps1"
    Start-Process "steam://rungameid/3681010"; $t0 = Get-Date; $p = $null
    while (((Get-Date) - $t0).TotalSeconds -lt 180) { $p = Get-Process Nioh3 -ErrorAction SilentlyContinue | Where-Object MainWindowHandle -ne 0 | Select-Object -First 1; if ($p) { break }; Start-Sleep 3 }
    if (-not $p) { Note-Crash "no window"; throw "no game window after 180 s" }
    $script:gamePid = $p.Id; Register-Game; T "game pid $($p.Id)"
    if (-not (VP-WaitPipe 180)) { throw "no pipe" }
    $ok = VP-TitleContinue 300 "$out\title.txt"; T "title -> field: $ok; state $(VP-State)"
    if (-not $ok) { throw "not in the field" }
    Start-Sleep 8
    T "sampler start"
    py -I "$base\scripts\hold.py" $Arm $N $out *>&1 | ForEach-Object { T "  $_" }
    $alive = [bool](Get-Process -Id $script:gamePid -ErrorAction SilentlyContinue)
    Close-Game
    $log = ReadShared "$G\Nioh3EngineFix.log"; Set-Content "$out\EF.log" $log
    $result.alive = $alive; $result.vrr = (($log -split "`r?`n" | Where-Object { $_ -match "StockSites|Tier120Floor|GovFpsFix|tier" } | Select-Object -First 8) -join " | ")
} catch {
    T "!! $($_.Exception.Message)"; $result.error = $_.Exception.Message
} finally {
    Close-Game; Restore
    foreach ($l in $logs) { if (Test-Path "$out\orig\$l.before") { Copy-Item "$out\orig\$l.before" "$G\$l" -Force } }
    Done-Guard
    $result | ConvertTo-Json -Depth 4 | Set-Content "$out\result.json"
    T "result: $($result | ConvertTo-Json -Compress -Depth 4)"
}
