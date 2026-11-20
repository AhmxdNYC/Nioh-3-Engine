# training_gate_live.ps1 - live run of the Training session release gate (human #122006, TASK #122016). Main PC, one game, its own launch.
# Scoring rules + menu steps: training_gate.py (py -I training_gate.py plan). This script only owns the session: refuses when ANY Nioh3 runs
# (a human game is never touched), takes the game lock, copies saves / ASIs / inis, installs -Stage ASIs (optional), forces VirtualPad,
# launches, Continue -> field, then per case: marks the log offsets, waits for the case's lines while the runner drives the menu with
# vpad (-Drive menu) or drives it itself through the Training cmd file (-Drive cmd, cases 3 and 4 only), cuts caseN\{tr,br,ef}.txt and
# scores it. Cases 3/4: ef.txt starts at the TRAIN stop line, then the script opens and closes the mod menu once (F10 via vpad) so the
# 'opened on the ... page' line is seen. Always restores saves, ASIs, inis and the lock. Exit: 0 all PASS, 1 a FAIL, 2 NOT RUN / abort.
#   powershell -NoProfile -ExecutionPolicy Bypass -File training_gate_live.ps1 -OwnerPid <runner pid> [-Cases 1,3,4,5,2] [-Stage DIR]
#       [-Drive menu|cmd] [-Boss R-id (cmd drive only; any summonable row)] [-CaseSec 240]
param([int]$OwnerPid = 0, [int[]]$Cases = @(1, 3, 4, 5, 2), [string]$Stage = "", [ValidateSet("menu", "cmd")][string]$Drive = "menu",
    [string]$Boss = "", [int]$CaseSec = 240, [int]$MaxMinutes = 40)
$ErrorActionPreference = "Continue"
$tm = $PSScriptRoot
. "$tm\harness_guard.ps1"
. "$tm\vpad.ps1"
$gate = Join-Path $tm "training_gate.py"
$G = "C:\Program Files (x86)\Steam\steamapps\common\Nioh3"
$out = "D:\nioh3-re\training-gate\" + (Get-Date -f yyyyMMdd-HHmmss) + "-$PID"; New-Item -ItemType Directory -Force "$out\orig" | Out-Null
$trace = "$out\trace.txt"
function T([string]$s) { $l = "[{0}] {1}" -f (Get-Date -f HH:mm:ss), $s; [IO.File]::AppendAllText($trace, $l + "`r`n"); Write-Host $l }
$uo = "C:\Users\Ahmad Hamza\universal-optimizer\agent-bridge"
$env:UO_BRIDGE_TOKEN_FILE = "$env:APPDATA\uo-bridge\token"; $env:UO_AGENT_ID = "pc"
function Lock([string]$a) { Push-Location $uo; try { $o = (py -m uo_bridge.gamelock $a nioh3 --ttl 3600 2>&1 | Out-String).Trim(); @{ code = $LASTEXITCODE; out = $o } } finally { Pop-Location } }
$inis = @("Nioh3EngineFix.ini", "Nioh3CamKiProbe.ini", "Nioh3Training.ini", "Nioh3BossRematch.ini", "Nioh3Training_learned.ini")
$bins = @("Nioh3EngineFix.asi", "Nioh3Training.asi", "Nioh3BossRematch.asi", "Nioh3CamKiProbe.asi")
$LOG = @{ tr = "$G\Nioh3Training.log"; br = "$G\Nioh3BossRematch.log"; ef = "$G\Nioh3EngineFix.log" }
$tc = "$G\Nioh3Training.cmd"
$saveDir = "$env:LOCALAPPDATA\KoeiTecmo\NIOH3"
$script:gamePid = 0; $script:locked = $false; $script:installed = $false; $script:saveBak = $null
function H8([string]$f) { (Get-FileHash $f).Hash.Substring(0, 8).ToLower() }
function ReadShared([string]$f) { if (-not (Test-Path $f)) { return "" }; $fs = [IO.File]::Open($f, "Open", "Read", "ReadWrite"); try { (New-Object IO.StreamReader($fs)).ReadToEnd() } finally { $fs.Dispose() } }
function Alive { $script:gamePid -and [bool](Get-Process -Id $script:gamePid -ErrorAction SilentlyContinue) }
function Close-Game {
    $p = if ($script:gamePid) { Get-Process -Id $script:gamePid -ErrorAction SilentlyContinue } else { $null }
    if ($p) { Note-Closed; $null = $p.CloseMainWindow(); $p.WaitForExit(30000) | Out-Null; if (-not $p.HasExited) { T "!! game did not exit in 30 s - killing $($p.Id)"; Stop-Process -Id $p.Id -Force }; Start-Sleep 5 }
    $script:gamePid = 0
}
function Restore {
    if (Get-Process Nioh3 -ErrorAction SilentlyContinue) { T "!! a Nioh3 is still running: restore skipped"; return }
    if ($script:installed) {
        foreach ($f in $bins + $inis) { if (Test-Path "$out\orig\$f") { Copy-Item "$out\orig\$f" "$G\$f" -Force } }
        $script:installed = $false; T ("restored: " + (($bins | Where-Object { Test-Path "$G\$_" } | ForEach-Object { "$_ $(H8 "$G\$_")" }) -join ", "))
    }
    if ($script:saveBak) { robocopy $script:saveBak $saveDir /MIR /R:2 /W:2 /NFL /NDL /NP /NJH | Out-Null; T "saves restored robocopy $LASTEXITCODE"; $script:saveBak = $null }
    if ($script:locked) { $r = Lock "release"; $script:locked = $false; T "lock released: $($r.out)" }
}
function Marks { $m = @{}; foreach ($k in $LOG.Keys) { $m[$k] = (ReadShared $LOG[$k]).Length }; $m }
function Since($m, [string]$k) { $s = ReadShared $LOG[$k]; if ($s.Length -gt $m[$k]) { $s.Substring($m[$k]) } else { "" } }
function WaitRx($m, [string]$k, [string]$rx, [int]$sec) { $w0 = Get-Date; while (((Get-Date) - $w0).TotalSeconds -lt $sec) { if ((Since $m $k) -match $rx) { return $true }; if (-not (Alive)) { return $false }; Start-Sleep -Milliseconds 300 }; $false }
function Cmd([string]$c) { $w0 = Get-Date; while ((Test-Path $tc) -and ((Get-Date) - $w0).TotalSeconds -lt 10) { Start-Sleep -Milliseconds 100 }; [IO.File]::WriteAllText($tc, "$c`r`n"); T "cmd: $c" }
function Steps([int]$n) { (py -I $gate plan $n 2>&1 | Out-String).Trim() }
# the line each case waits for before it is cut (training_gate.py scores the cut, these only end the wait)
$DONE = @{ 1 = @("tr", "MENU training here mission 0x[0-9A-Fa-f]+ -> |FIGHTNOW \S+: native-first|Training pick: training here");
           2 = @("br", "Fight again direct \S+: MissionRequestById|native gate retry: home is up|request refused|no longer own");
           3 = @("tr", "TRAIN stop \(despawn all|TRAIN stop \(last boss removed\)");
           4 = @("tr", "TRAIN stop \(boss defeated, body removed\)");
           5 = @("tr", "TRAIN arena session \S+: round 1|FIGHTNOW \S+: native-first|arena route refused") }
Init-Guard -Name "training-gate" -OwnerPid $OwnerPid -MaxMinutes $MaxMinutes -Cleanup { Close-Game; Restore }
$result = [ordered]@{ capture = $out; drive = $Drive; cases = [ordered]@{} }
$worst = 2
try {
    if (Get-Process Nioh3 -ErrorAction SilentlyContinue) { throw "a Nioh3 is running (human or other test): nothing touched" }
    if ($Drive -eq "cmd" -and (-not $Boss -or @($Cases | Where-Object { $_ -notin 3, 4 }).Count)) { throw "-Drive cmd covers cases 3 and 4 only and needs -Boss" }
    $r = Lock "acquire"; if ($r.code -ne 0) { throw "lock: $($r.out)" }; $script:locked = $true; T "lock: $($r.out)"
    foreach ($f in $bins + $inis) { if (Test-Path "$G\$f") { Copy-Item "$G\$f" "$out\orig\" -Force } }
    $sb = "D:\nioh3-re\save-backups\" + (Get-Date -f yyyyMMdd_HHmmss) + "-trgate"; New-Item -ItemType Directory -Force $sb | Out-Null
    robocopy $saveDir $sb /MIR /R:2 /W:2 /NFL /NDL /NP /NJH | Out-Null; $script:saveBak = $sb; T "saves copied to $sb (robocopy $LASTEXITCODE)"
    $script:installed = $true
    if ($Stage) { foreach ($f in $bins) { if (Test-Path "$Stage\$f") { Copy-Item "$Stage\$f" "$G\$f" -Force } } }
    $ini = "$G\Nioh3EngineFix.ini"; $t = [IO.File]::ReadAllText($ini); $t = Force-Vpad $t
    if ($t -match "(?m)^AutoContinueLoad *=") { $t = $t -replace "(?m)^AutoContinueLoad *=.*", "AutoContinueLoad = 1" }
    [IO.File]::WriteAllText($ini, $t, (New-Object Text.UTF8Encoding($false)))
    $result.build = (($bins | Where-Object { Test-Path "$G\$_" } | ForEach-Object { "$_ $(H8 "$G\$_")" }) -join ", "); T "build: $($result.build)"
    Remove-Item $LOG.tr, $LOG.ef, $tc -Force -ErrorAction SilentlyContinue
    Assert-Launch
    $auto = "C:\Users\Ahmad Hamza\nioh3-engine\tools\steam\start_autoplay.ps1"; if (Test-Path $auto) { & $auto }
    Start-Process "steam://rungameid/3681010"; $t0 = Get-Date; $p = $null
    while (((Get-Date) - $t0).TotalSeconds -lt 180) { $p = Get-Process Nioh3 -ErrorAction SilentlyContinue | Where-Object MainWindowHandle -ne 0 | Select-Object -First 1; if ($p) { break }; Start-Sleep 3 }
    if (-not $p) { Note-Crash "no window"; throw "no game window after 180 s" }
    $script:gamePid = $p.Id; Register-Game; T "game pid $($p.Id)"
    if (-not (VP-WaitPipe 180)) { throw "no vpad pipe" }
    if (-not (VP-TitleContinue 300 "$out\title.txt")) { throw "not in the field" }
    T "in the field; state $(VP-State)"; Start-Sleep 8
    foreach ($n in $Cases) {
        if (-not (Alive)) { $result.cases["$n"] = "NOT RUN (game gone)"; continue }
        $d = "$out\case$n"; New-Item -ItemType Directory -Force $d | Out-Null
        $m = Marks; $k = $DONE[$n][0]; $rx = $DONE[$n][1]
        Set-Content "$out\WAITING.txt" ("case $n, " + (Get-Date -f HH:mm:ss) + ", up to $CaseSec s`r`n" + (Steps $n))
        T "case $n start ($Drive): $((Steps $n) -replace "`r?`n", ' | ')"
        if ($Drive -eq "cmd") {
            $m0 = Marks; Cmd "train $Boss"; $null = WaitRx $m0 "tr" "TRAIN " 20; Start-Sleep 5
            if ($n -eq 3) { Cmd "despawn all" } else { Cmd "kill last" }
        }
        $seen = WaitRx $m $k $rx $CaseSec
        $efMark = Marks
        if (($n -in 3, 4) -and $seen) { Start-Sleep 3; $null = VP key 0x44; Start-Sleep 2; $null = VP key 0x44; Start-Sleep 2 }  # DIK_F10 open, then close
        if (-not $seen) { T "case ${n}: the case line did not appear in $CaseSec s" }
        foreach ($l in "tr", "br") { [IO.File]::WriteAllText("$d\$l.txt", (Since $m $l)) }
        [IO.File]::WriteAllText("$d\ef.txt", $(if ($n -in 3, 4) { Since $efMark "ef" } else { Since $m "ef" }))
        $v = (py -I $gate score $out --case $n 2>&1 | Out-String).Trim(); $result.cases["$n"] = $v; T $v
        Remove-Item "$out\WAITING.txt" -Force -ErrorAction SilentlyContinue
        if ($n -ne $Cases[-1]) { Cmd "despawn all"; Start-Sleep 3 }  # next case starts with no session
    }
    $result.alive = Alive
    Close-Game
    foreach ($k in $LOG.Keys) { Set-Content "$out\$k-full.log" (ReadShared $LOG[$k]) }
    $result.crashfiles = @(Get-ChildItem "$G\Nioh3*crash*" -ErrorAction SilentlyContinue | Where-Object LastWriteTime -gt $t0).Count
    $sa = @("score", $out); foreach ($n in $Cases) { $sa += @("--case", "$n") }; $s = (py -I $gate @sa 2>&1 | Out-String); $worst = $LASTEXITCODE
    Set-Content "$out\score.txt" $s
} catch {
    T "!! $($_.Exception.Message)"; $result.error = $_.Exception.Message; $worst = 2
} finally {
    Close-Game; Remove-Item $tc -Force -ErrorAction SilentlyContinue; Restore
    Done-Guard
    $result.exit = $worst
    $result | ConvertTo-Json -Depth 4 | Set-Content "$out\result.json"
    T "result: $($result | ConvertTo-Json -Compress -Depth 4)"
}
exit $worst
