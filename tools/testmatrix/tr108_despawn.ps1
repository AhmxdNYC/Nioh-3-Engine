# tr108.ps1 - pc/1 #112540: GH #108 dead-boss despawn live check, Main PC. TR #8252 d3d783e6 (reports/108 219ab858), other ASIs as installed.
# Summons via the menu slot path (cmd 'slot 0 <R-id>'), kills, then 'despawn all' ~1 s later (menu and command both use the deferred path). 5x,
# then a live boss despawn, then 'slotdel 0' on a dead summon, then one more summon. Restores TR + inis + saves.
param([int]$OwnerPid = 0, [string]$Arm = "L", [string]$Boss = "R1359")
$ErrorActionPreference = "Continue"
$tm = "C:\Users\Ahmad Hamza\nioh3-engine\tools\testmatrix"
. "$tm\harness_guard.ps1"
. "$tm\vpad.ps1"
$G = "C:\Program Files (x86)\Steam\steamapps\common\Nioh3"
$base = "C:\Users\Ahmad Hamza\nioh3-engine\Temp\tr108live"; $Stage = "$base\stage"
$out = "$base\run$Arm"; New-Item -ItemType Directory -Force "$out\orig" | Out-Null
$trace = "$out\trace.txt"
function T([string]$s) { $l = "[{0}] {1}" -f (Get-Date -f HH:mm:ss), $s; [IO.File]::AppendAllText($trace, $l + "`r`n"); Write-Host $l }
$uo = "C:\Users\Ahmad Hamza\universal-optimizer\agent-bridge"
$env:UO_BRIDGE_TOKEN_FILE = "$env:APPDATA\uo-bridge\token"; $env:UO_AGENT_ID = "pc"
function Lock([string]$a) { Push-Location $uo; try { $o = (py -m uo_bridge.gamelock $a nioh3 --ttl 3600 2>&1 | Out-String).Trim(); @{ code = $LASTEXITCODE; out = $o } } finally { Pop-Location } }
$inis = @("Nioh3EngineFix.ini","Nioh3CamKiProbe.ini","Nioh3Training.ini","Nioh3BossRematch.ini","Nioh3Training_learned.ini")
$logs = @("Nioh3EngineFix.log","Nioh3CamKiProbe.log","Nioh3Training.log","Nioh3BossRematch.log")
$bins = @("Nioh3EngineFix.asi","Nioh3Training.asi")
$ini = "$G\Nioh3EngineFix.ini"
$script:gamePid = 0; $script:locked = $false; $script:installed = $false
$saveDir = "$env:LOCALAPPDATA\KoeiTecmo\NIOH3"; $script:saveBak = $null
function H8([string]$f) { (Get-FileHash $f).Hash.Substring(0,8).ToLower() }
function Close-Game {
    $p = if ($script:gamePid) { Get-Process -Id $script:gamePid -ErrorAction SilentlyContinue } else { $null }
    if ($p) { Note-Closed; $null = $p.CloseMainWindow(); $p.WaitForExit(30000) | Out-Null; if (-not $p.HasExited) { T "!! game did not exit in 30 s - killing $($p.Id)"; Stop-Process -Id $p.Id -Force }; Start-Sleep 5 }
    $script:gamePid = 0
}
function Restore {
    if ($script:installed -and -not (Get-Process Nioh3 -ErrorAction SilentlyContinue)) {
        foreach ($f in $bins + $inis) { if (Test-Path "$out\orig\$f") { Copy-Item "$out\orig\$f" "$G\$f" -Force } }
        $script:installed = $false; T "asi + inis restored: EF $(H8 "$G\Nioh3EngineFix.asi") TR $(H8 "$G\Nioh3Training.asi")"
    }
    if ($script:saveBak -and -not (Get-Process Nioh3 -ErrorAction SilentlyContinue)) { robocopy $script:saveBak $saveDir /MIR /R:2 /W:2 /NFL /NDL /NP /NJH | Out-Null; T "saves restored robocopy $LASTEXITCODE"; $script:saveBak = $null }
    if ($script:locked) { $r = Lock "release"; $script:locked = $false; T "lock released: $($r.out)" }
}
function ReadShared([string]$f) { if (-not (Test-Path $f)) { return "" }; $fs = [IO.File]::Open($f, "Open", "Read", "ReadWrite"); try { (New-Object IO.StreamReader($fs)).ReadToEnd() } finally { $fs.Dispose() } }
Init-Guard -Name "tr108-$Arm" -OwnerPid $OwnerPid -MaxMinutes 14 -Cleanup { Close-Game; Restore }
$result = [ordered]@{ run = $Arm; boss = $Boss }
$t00 = Get-Date
try {
    if (Get-Process Nioh3 -ErrorAction SilentlyContinue) { throw "Nioh3 is running" }
    $r = Lock "acquire"; if ($r.code -ne 0) { throw "lock: $($r.out)" }; $script:locked = $true; T "lock: $($r.out)"
    foreach ($f in $bins + $inis) { if (Test-Path "$G\$f") { Copy-Item "$G\$f" "$out\orig\" -Force } }
    foreach ($l in $logs) { if (Test-Path "$G\$l") { Copy-Item "$G\$l" "$out\orig\$l.before" -Force } }
    $stamp = Get-Date -f yyyyMMdd_HHmmss; $sb = "D:\nioh3-re\save-backups\$stamp-tr108$Arm"; New-Item -ItemType Directory -Force $sb | Out-Null
    robocopy $saveDir $sb /MIR /R:2 /W:2 /NFL /NDL /NP /NJH | Out-Null; $script:saveBak = $sb; T "saves copied to $sb (robocopy $LASTEXITCODE)"
    $script:installed = $true
    Copy-Item "$Stage\Nioh3Training.asi" "$G\Nioh3Training.asi" -Force
    $t = [IO.File]::ReadAllText($ini); $t = Force-Vpad $t
    if ($t -match "(?m)^AutoContinueLoad *=") { $t = $t -replace "(?m)^AutoContinueLoad *=.*", "AutoContinueLoad = 1" }
    [IO.File]::WriteAllText($ini, $t, (New-Object Text.UTF8Encoding($false)))
    T "installed TR $(H8 "$G\Nioh3Training.asi") EF $(H8 "$G\Nioh3EngineFix.asi")"
    & "C:\Program Files (x86)\Steam\steam.exe" -shutdown; $s0 = Get-Date; while ((Get-Process steam -ErrorAction SilentlyContinue) -and ((Get-Date) - $s0).TotalSeconds -lt 60) { Start-Sleep 2 }
    Start-Process "C:\Program Files (x86)\Steam\steam.exe" -ArgumentList "-silent"; Start-Sleep 45; T "steam restarted"
    Remove-Item "$G\Nioh3EngineFix.log","$G\Nioh3Training.log","$G\Nioh3Training.cmd" -Force -ErrorAction SilentlyContinue
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
    T "steps start"
    $tl = "$G\Nioh3Training.log"; $tc = "$G\Nioh3Training.cmd"
    function Cmd([string]$c) { $w0 = Get-Date; while ((Test-Path $tc) -and ((Get-Date) - $w0).TotalSeconds -lt 10) { Start-Sleep -Milliseconds 100 }; [IO.File]::WriteAllText($tc, "$c`r`n"); T "cmd: $c" }
    function LogLen { (ReadShared $tl).Length }
    function Since([int]$n) { $s = ReadShared $tl; if ($s.Length -gt $n) { $s.Substring($n) } else { "" } }
    function Alive { [bool](Get-Process -Id $script:gamePid -ErrorAction SilentlyContinue) }
    function WaitLog([int]$n, [string]$rx, [int]$sec) { $w0 = Get-Date; while (((Get-Date) - $w0).TotalSeconds -lt $sec) { if ((Since $n) -match $rx) { return $true }; if (-not (Alive)) { return $false }; Start-Sleep -Milliseconds 200 }; $false }
    function Verd([string]$seg) { $d = $seg -match 'boss is dead [(]HP 0'; $x = $seg -match 'DESPAWN deferred .*native delete 2BF7E8 called'; $im = $seg -match 'DESPAWN #[0-9]+ [^ ]+: native delete 2BF7E8 called'; return ("alive={0} defer={1} deleted={2} immediate={3}" -f (Alive), $d, $x, $im) }
    $script:ns = 0
    function Summon { $script:ns++; $n = LogLen; Cmd "slot 0 $Boss"; Start-Sleep 15; $seg = Since $n; Set-Content "$out\summon$($script:ns).txt" $seg
        $ok = (Alive) -and ($seg -match 'MENU summon slot A [^ ]+ -> 1') -and ($seg -match 'CreateDynamic'); T "summon $($script:ns) ok=$ok"; $ok }
    $res = [ordered]@{}
    for ($i = 1; $i -le 5; $i++) {
        if (-not (Summon)) { $res["dead$i"] = "no summon"; T "dead $i -> no summon"; if (-not (Alive)) { break }; continue }
        $n = LogLen; Cmd "kill last"; $null = WaitLog $n "(?i)kill" 5; Start-Sleep -Milliseconds 1000
        Cmd "despawn all"; $null = WaitLog $n "DESPAWN deferred" 12; Start-Sleep 2
        $seg = Since $n; Set-Content "$out\dead$i.txt" $seg
        $v = Verd $seg; $res["dead$i"] = $v; T "dead $i -> $v"
        if (-not (Alive)) { break }
    }
    if ((Alive) -and (Summon)) { $n = LogLen; Cmd "despawn all"; Start-Sleep 3; $seg = Since $n; Set-Content "$out\live.txt" $seg
        $res.live = Verd $seg; T "live -> $($res.live)" }
    if ((Alive) -and (Summon)) { $n = LogLen; Cmd "kill last"; $null = WaitLog $n "(?i)kill" 5; Start-Sleep -Milliseconds 1000; Cmd "slotdel 0"; $null = WaitLog $n "DESPAWN deferred" 12; Start-Sleep 2; $seg = Since $n; Set-Content "$out\slot.txt" $seg
        $res.slot = Verd $seg; T "slotdel -> $($res.slot)" }
    if (Alive) { $ok = Summon; $res.after = "summon=$ok alive=$(Alive)"; Cmd "despawn all"; Start-Sleep 3; T "after -> $($res.after)" }
    $result.steps = $res
    $alive = Alive
    Close-Game
    Set-Content "$out\TR.log" (ReadShared $tl); Set-Content "$out\EF.log" (ReadShared "$G\Nioh3EngineFix.log")
    $result.alive = $alive
    $result.crashfiles = @(Get-ChildItem "$G\Nioh3*crash*" -ErrorAction SilentlyContinue | Where-Object LastWriteTime -gt $t00).Count
} catch {
    T "!! $($_.Exception.Message)"; $result.error = $_.Exception.Message
} finally {
    Close-Game; Remove-Item "$G\Nioh3Training.cmd" -Force -ErrorAction SilentlyContinue; Restore
    foreach ($l in $logs) { if (Test-Path "$out\orig\$l.before") { Copy-Item "$out\orig\$l.before" "$G\$l" -Force } }
    Done-Guard
    $result | ConvertTo-Json -Depth 4 | Set-Content "$out\result.json"
    T "result: $($result | ConvertTo-Json -Compress -Depth 4)"
}
