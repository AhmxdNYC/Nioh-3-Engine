# pg.ps1 - pc/1 #116769: PadListGuard boot-crash live check, Main PC. EF a8d37067 (#8446, reports/115 bd9166d3), other ASIs as installed.
# Boot A: VirtualPad = 2 (merge: real pad stays) -> title -> Continue -> field -> move/attack 30 s -> menu open/close -> quit.
# Boot B: VirtualPad = 0 (real pad only) -> boot to title, 75 s -> quit. Restore EF + inis + saves.
param([string]$Arm = "A")
$ErrorActionPreference = "Continue"
$tm = "C:\Users\Ahmad Hamza\nioh3-engine\tools\testmatrix"
. "$tm\harness_guard.ps1"
. "$tm\vpad.ps1"
$G = "C:\Program Files (x86)\Steam\steamapps\common\Nioh3"
$Stage = "D:\nioh3-re\padguard-116769\stage"; $RD = "D:\nioh3-re\padguard-116769"
$out = "$RD\b-$Arm"; New-Item -ItemType Directory -Force "$out\orig" | Out-Null
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
    $w0 = Get-Date; while ((Get-Process Nioh3 -ErrorAction SilentlyContinue) -and ((Get-Date) - $w0).TotalSeconds -lt 90) { Start-Sleep 2 }; Start-Sleep 5
    if ($script:installed -and -not (Get-Process Nioh3 -ErrorAction SilentlyContinue)) {
        foreach ($f in @("Nioh3EngineFix.asi") + $inis) { if (Test-Path "$out\orig\$f") { for ($k = 0; $k -lt 10; $k++) { try { Copy-Item "$out\orig\$f" "$G\$f" -Force -ErrorAction Stop; break } catch { Start-Sleep 3 } } } }
        $script:installed = $false; T "EF asi + inis restored: EF $((Get-FileHash "$G\Nioh3EngineFix.asi").Hash.Substring(0,8).ToLower())"
    }
    if ($script:saveBak -and -not (Get-Process Nioh3 -ErrorAction SilentlyContinue)) { robocopy $script:saveBak $saveDir /MIR /R:2 /W:2 /NFL /NDL /NP /NJH | Out-Null; T "saves restored robocopy $LASTEXITCODE"; $script:saveBak = $null }
    if ($script:locked) { $r = Lock "release"; $script:locked = $false; T "lock released: $($r.out)" }
}
function ReadShared([string]$f) { if (-not (Test-Path $f)) { return "" }; $fs = [IO.File]::Open($f, "Open", "Read", "ReadWrite"); try { (New-Object IO.StreamReader($fs)).ReadToEnd() } finally { $fs.Dispose() } }
function SetKey([string]$t, [string]$sec, [string]$k, [string]$v) {
    if ($t -match "(?m)^$k *=") { return ($t -replace "(?m)^$k *=.*", "$k = $v") }
    if ($t -match "(?m)^\[$sec\]") { return ($t -replace "(?m)^\[$sec\]\r?\n", "[$sec]`r`n$k = $v`r`n") }
    return $t + "`r`n[$sec]`r`n$k = $v`r`n"
}
Init-Guard -Name "pg116769-$Arm" -OwnerPid 0 -MaxMinutes 12 -Cleanup { Close-Game; Restore }
$result = [ordered]@{ arm = $Arm }
try {
    if (Get-Process Nioh3 -ErrorAction SilentlyContinue) { throw "Nioh3 is running" }
    $r = Lock "acquire"; if ($r.code -ne 0) { throw "lock: $($r.out)" }; $script:locked = $true; T "lock: $($r.out)"
    foreach ($f in @("Nioh3EngineFix.asi") + $inis) { if (Test-Path "$G\$f") { Copy-Item "$G\$f" "$out\orig\" -Force } }
    foreach ($lg in $logs) { if (Test-Path "$G\$lg") { Copy-Item "$G\$lg" "$out\orig\$lg.before" -Force } }
    $stamp = Get-Date -f yyyyMMdd_HHmmss; $sb = "D:\nioh3-re\save-backups\$stamp-pg116769$Arm"; New-Item -ItemType Directory -Force $sb | Out-Null
    robocopy $saveDir $sb /MIR /R:2 /W:2 /NFL /NDL /NP /NJH | Out-Null; $script:saveBak = $sb; T "saves copied to $sb (robocopy $LASTEXITCODE)"
    $script:installed = $true
    Copy-Item "$Stage\Nioh3EngineFix.asi" "$G\Nioh3EngineFix.asi" -Force
    $t = [IO.File]::ReadAllText($ini)
    $vp = if ($Arm -eq "A") { "2" } else { "0" }
    $t = SetKey $t "Debug" "VirtualPad" $vp; $t = SetKey $t "Debug" "KeepRunningUnfocused" "1"
    [IO.File]::WriteAllText($ini, $t, (New-Object Text.UTF8Encoding($false)))
    $keys = ([regex]::Matches($t, "(?m)^(VirtualPad|KeepRunningUnfocused|PadListGuard|AutoContinueLoad|HoldAttackFix) *=.*$") | ForEach-Object { $_.Value.Trim() }) -join "; "
    T "installed EF $((Get-FileHash "$G\Nioh3EngineFix.asi").Hash.Substring(0,8).ToLower()); ini: $keys"; $result.ini = $keys
    Remove-Item "$G\Nioh3EngineFix.log" -Force -ErrorAction SilentlyContinue
    Assert-Launch
    & "C:\Users\Ahmad Hamza\nioh3-engine\tools\steam\start_autoplay.ps1"
    Start-Process "steam://rungameid/3681010"; $t0 = Get-Date; $p = $null
    while (((Get-Date) - $t0).TotalSeconds -lt 180) { $p = Get-Process Nioh3 -ErrorAction SilentlyContinue | Where-Object MainWindowHandle -ne 0 | Select-Object -First 1; if ($p) { break }; Start-Sleep 3 }
    if (-not $p) { Note-Crash "no window"; throw "no game window after 180 s" }
    $script:gamePid = $p.Id; Register-Game; T "game pid $($p.Id)"; $result.pid = $p.Id
    if ($Arm -eq "A") {
        if (-not (VP-WaitPipe 180)) { throw "no pipe" }
        $ok = VP-TitleContinue 300 "$out\title.txt"; T "title -> field: $ok; state $(VP-State)"; $result.field = $ok
        if (-not $ok) { throw "not in the field" }
        Start-Sleep 3; $s0 = VP-State; T "state before play: $s0"
        $t1 = Get-Date
        while (((Get-Date) - $t1).TotalSeconds -lt 30) {
            $null = VP stick L 0 1 1200; $null = VP press X 120; $null = VP stick L 1 0 800; $null = VP press X 120; $null = VP press Y 150; $null = VP stick L 0 -1 1200; $null = VP press A 100
        }
        $s1 = VP-State; T "state after 30 s play: $s1"; $result.pos = "$s0 => $s1"
        $m1 = VP press START 120; Start-Sleep 2; $sm = VP-State; T "menu open ($($m1.reply)): $sm"
        $null = VP press B 120; Start-Sleep 2; $null = VP press B 120; Start-Sleep 2; $sc = VP-State; T "menu close: $sc"
        $result.menu = "open: $sm | close: $sc"
    } else {
        Start-Sleep 75
        if (-not (Get-Process -Id $script:gamePid -ErrorAction SilentlyContinue)) { throw "game exited during title wait" }
    }
    $alive = [bool](Get-Process -Id $script:gamePid -ErrorAction SilentlyContinue); $result.alive = $alive
    Close-Game
    $log = ReadShared "$G\Nioh3EngineFix.log"; Set-Content "$out\EF.log" $log
    $result.pg = (($log -split "`r?`n" | Where-Object { $_ -match "PADGUARD|crash|exception|unordered_map" } | Select-Object -First 12) -join " | ")
    $result.vpline = (($log -split "`r?`n" | Where-Object { $_ -match "VirtualPad|VPad: (real|mode)" } | Select-Object -First 3) -join " | ")
} catch {
    T "!! $($_.Exception.Message)"; $result.error = $_.Exception.Message
    $log = ReadShared "$G\Nioh3EngineFix.log"; if ($log) { Set-Content "$out\EF.log" $log; $result.pg = (($log -split "`r?`n" | Where-Object { $_ -match "PADGUARD|crash|exception" } | Select-Object -First 12) -join " | ") }
} finally {
    Close-Game; Restore
    foreach ($lg in $logs) { if (Test-Path "$out\orig\$lg.before") { Copy-Item "$out\orig\$lg.before" "$G\$lg" -Force } }
    Done-Guard
    $result | ConvertTo-Json -Depth 4 | Set-Content "$out\result.json"
    T "result: $($result | ConvertTo-Json -Compress -Depth 4)"
}
