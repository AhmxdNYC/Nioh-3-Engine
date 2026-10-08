# abba_pc.ps1 - PC repeat test per notes\pc-keep-candidates.md section 4.
#   -Block A : one launch, in-process ABBA-ABBA per item (MQ, PP, B, GF), OFF sentinel after each item.
#   -Block B : cross-launch BASE,REC,BASE,REC,BASE,REC (stand 2x20 s + move 20 s).
# Lock: caller holds the nioh3 game lock (renewed here). Results: captures\tm\abba\ ; analysis: abba_report.py
param([ValidateSet("A","B")][string]$Block = "A", [int]$WindowSec = 30, [int]$Pairs = 3, [switch]$DryRun,
      [int]$OwnerPid = 0, [int]$MaxMinutes = 90)   # OwnerPid 0 = parent process (the agent shell); see harness_guard.ps1
$ErrorActionPreference = "Continue"
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
. "C:\Users\Ahmad Hamza\nioh3-engine\patcher\common.ps1"
$GameDir = "C:\Program Files (x86)\Steam\steamapps\common\Nioh3"; $ini = "$GameDir\Nioh3EngineFix.ini"; $logf = "$GameDir\Nioh3EngineFix.log"
$out = "$ws\captures\tm\abba"; New-Item -ItemType Directory -Force $out | Out-Null
$map = Get-Content "$here\menu_map.json" -Raw | ConvertFrom-Json
$uoBridge = "C:\Users\Ahmad Hamza\universal-optimizer\agent-bridge"
$env:UO_BRIDGE_TOKEN_FILE = "$env:APPDATA\uo-bridge\token"; $env:UO_AGENT_ID = "pc"
$fullmod = "$here\fullmod_ini_1716.ini"
$script:gamePid = 0
$trace = "$out\trace_$Block.txt"
. "$here\harness_guard.ps1"
if (-not $DryRun) { Init-Guard -Name "abba_$Block" -OwnerPid $OwnerPid -MaxMinutes $MaxMinutes -Cleanup { Close-GameClean; try { Set-Ini @{ Barriers = 0 } } catch {} } }
function T([string]$s) { $l = "[{0}] {1}" -f (Get-Date -f HH:mm:ss), $s; Add-Content $trace $l; $l }
# SAFETY (2026-10-08): never send input unless Nioh3 is running AND in the foreground. ui.fsx re-checks the foreground
# before every click/key and exits 3 (REFUSED) otherwise; any refusal or a dead game aborts the whole block instead of
# looping (the old harness kept clicking/typing into VS Code after the game died or lost focus).
function U {
    Assert-Owner
    $cmd = [string]$args[0]
    if ($cmd -ne "shot" -and -not (Get-Process Nioh3 -ErrorAction SilentlyContinue)) {
        T "!! ABORT: Nioh3 is not running (before 'U $($args -join ' ')') - no input sent, exiting"; throw "HARNESS-ABORT: game not running" }
    $o = dotnet fsi $ui @args 2>&1 | Out-String
    if ($LASTEXITCODE -eq 3 -and $cmd -ne "shot" -and (Try-Refocus $ui)) { $o = dotnet fsi $ui @args 2>&1 | Out-String }
    if ($LASTEXITCODE -ne 3 -and $cmd -ne "shot") { Mark-Input }
    if ($LASTEXITCODE -eq 3) { T "!! ABORT: $($o.Trim()) - exiting"; throw "HARNESS-ABORT: input refused (game not in foreground)" }
}
function Renew { Push-Location $uoBridge; try { py -m uo_bridge.gamelock renew nioh3 --ttl 5400 | Out-Null } finally { Pop-Location } }
function Set-Ini([hashtable]$kv, [string]$base = $fullmod) {   # kv keys "Key" -> value (key names unique in the ini)
    $t = [IO.File]::ReadAllText($base)
    foreach ($k in $kv.Keys) {
        if ($t -match "(?m)^$k = ") { $t = $t -replace "(?m)^$k = .*", "$k = $($kv[$k])" } else { throw "ini key $k missing" }
    }
    [IO.File]::WriteAllText($ini, $t, (New-Object Text.UTF8Encoding($false)))
}
function Close-GameClean {
    Note-Closed
    $p = if ($script:gamePid) { Get-Process -Id $script:gamePid -ErrorAction SilentlyContinue } else { $null }
    if ($p) {
        $null = $p.CloseMainWindow(); $p.WaitForExit(25000) | Out-Null
        if (-not $p.HasExited) { Stop-Process -Id $script:gamePid -Force }   # exact PID only
        Start-Sleep 8
    }
    $script:gamePid = 0
}
function Start-Play {
    Assert-Launch
    $t0 = Get-Date
    Start-Process "steam://rungameid/3681010"
    $p = $null
    while (((Get-Date) - $t0).TotalSeconds -lt 180) { $p = Get-Process Nioh3 -ErrorAction SilentlyContinue | Where-Object MainWindowHandle -ne 0 | Select-Object -First 1; if ($p) { break }; Start-Sleep 3 }
    if (-not $p) { Note-Crash "no game window after launch"; return $false }
    $script:gamePid = $p.Id; Register-Game
    Start-Sleep 25
    U focus Nioh3; U click 1280 887; Start-Sleep 22
    U focus Nioh3; U click 1280 1170; Start-Sleep 7
    U focus Nioh3; U click 380 1024; Start-Sleep 25
    U focus Nioh3; U key 20; Start-Sleep 5; U key 20; Start-Sleep 4
    return (Ensure-InGame)
}

# ---- robust liveness / memory guard / draws parsing -------------------------------------------
$script:MemGuardMB = 2048
function Game-Alive {   # process exists, responding, and the mod log advanced recently (a dead/hung game = stale log)
    $p = if ($script:gamePid) { Get-Process -Id $script:gamePid -ErrorAction SilentlyContinue } else { Get-Process Nioh3 -ErrorAction SilentlyContinue | Select-Object -First 1 }
    if (-not $p -or $p.HasExited -or -not $p.Responding) { return $false }
    return (((Get-Date) - (Get-Item $logf).LastWriteTime).TotalSeconds -lt 45)
}
function Test-InGameR {   # Game-Alive first (distinguishes crash from menu), then screenshot check with retries + refocus
    if (-not (Game-Alive)) { $script:lastWhy = "game dead/hung or log stale"; return $false }
    for ($i = 0; $i -lt 3; $i++) { U focus Nioh3; Start-Sleep 1; if (Test-InGame) { return $true }; Start-Sleep 2 }
    $script:lastWhy = "HUD not detected"; return $false
}
function Get-MemInfo([string]$text) {   # last 'stats mem' line -> @{commit; limit; avail}
    $m = [regex]::Matches($text, 'stats mem:.*?\| sys RAM avail (\d+) MB, commit (\d+)/(\d+) MB')
    if ($m.Count -eq 0) { return $null }
    $g = $m[$m.Count - 1].Groups
    @{ avail = [int]$g[1].Value; commit = [int]$g[2].Value; limit = [int]$g[3].Value }
}
function Mem-Guard {   # returns $true if OK; $false = abort (system commit within 2 GB of limit)
    $fs = [IO.File]::Open($logf, "Open", "Read", "ReadWrite"); try { $n = [Math]::Min($fs.Length, 262144); $null = $fs.Seek(-$n, "End"); $tx = (New-Object IO.StreamReader($fs)).ReadToEnd() } finally { $fs.Dispose() }
    $mi = Get-MemInfo $tx
    if (-not $mi) { return $true }
    if (($mi.limit - $mi.commit) -lt $script:MemGuardMB) { T "!! MEM GUARD: commit $($mi.commit)/$($mi.limit) MB, headroom $($mi.limit - $mi.commit) MB < $script:MemGuardMB - aborting"; return $false }
    return $true
}
function Parse-Draws([string]$text) {   # OSD stats lines: '... draws N, skipped M' -> mean draws/frame
    $v = @([regex]::Matches($text, 'OSD: stats:.*?\bdraws (\d+)') | ForEach-Object { [double]$_.Groups[1].Value })
    if ($v.Count -eq 0) { return $null }
    [pscustomobject]@{ n = $v.Count; mean = [Math]::Round(($v | Measure-Object -Average).Average, 1); min = ($v | Measure-Object -Minimum).Minimum; max = ($v | Measure-Object -Maximum).Maximum }
}
if ($DryRun) {
    $tx = [IO.File]::ReadAllText($logf)
    $d = Parse-Draws $tx; $mi = Get-MemInfo $tx
    "DryRun: parse OK. block=$Block log=$($tx.Length) bytes; draws=" + $(if ($d) { "n=$($d.n) mean=$($d.mean) min=$($d.min) max=$($d.max)" } else { "none" }) +
      "; mem=" + $(if ($mi) { "commit $($mi.commit)/$($mi.limit) headroom $($mi.limit - $mi.commit) MB guard=" + (($mi.limit - $mi.commit) -ge $script:MemGuardMB) } else { "none" }) +
      "; gameAlive=$(Game-Alive)"
    return
}
function Log-Len { (Get-Item $logf).Length }
function Log-From([long]$off) { $fs = [IO.File]::Open($logf, "Open", "Read", "ReadWrite"); try { $null = $fs.Seek($off, "Begin"); (New-Object IO.StreamReader($fs)).ReadToEnd() } finally { $fs.Dispose() } }
function Toggle([string]$id, [string]$want) {
    $m = $map.items.$id; $off = Log-Len
    U keyseq @($m.keys)
    for ($k = 0; $k -lt 10; $k++) { Start-Sleep -Milliseconds 500; $tx = Log-From $off; if ($tx -match ("MENU: " + [regex]::Escape($m.label) + " -> " + $want)) { Add-Content "$out\menu_lines.txt" (($tx -split "`n" | Where-Object { $_ -match "MENU: " + [regex]::Escape($m.label) }) -join "`n"); return $true } }
    T "!! toggle $id -> $want NOT confirmed"; return $false
}
function Window([string]$name) {
    $csv = "$out\$name.csv"; $off = Log-Len
    $pm = Start-Process "$ws\tools\PresentMon.exe" -PassThru -WindowStyle Hidden -ArgumentList "--process_name Nioh3.exe --output_file `"$csv`" --timed $WindowSec --terminate_after_timed --stop_existing_session --no_console_stats"
    Register-Child $pm "PresentMon"
    Start-Sleep ($WindowSec + 1); $pm.WaitForExit(30000) | Out-Null
    Start-Sleep 2
    $wt = Log-From $off; Set-Content "$out\$name.log" $wt
    if ($wt.Trim().Length -lt 20) { T "!! window $name log EMPTY (game stalled/dead?)" }
    $d = Parse-Draws $wt
    if ($d) { T "$name draws/frame mean $($d.mean) (min $($d.min) max $($d.max), n=$($d.n))"; Add-Content "$out\draws.csv" "$name,$($d.n),$($d.mean),$($d.min),$($d.max)" } else { T "$name draws: no OSD stats lines" }
}
if (-not (Test-Path "$here\state\blockA_ini_orig.ini")) { Copy-Item $ini "$here\state\blockA_ini_orig.ini" }
try {
    if ($Block -eq "A") {
        Set-Ini @{ B = 1; PP = 1; GF = 1; MQ = 2; CpuStats = 1; ReportSec = 10; Barriers = 0 }
        if (-not (Start-Play)) { T "boot failed"; return }
        T "in game"; Renew
        U keyseq @($map.init); Start-Sleep 1
        $state = @{ B = "ON"; PP = "ON"; GF = "ON"; MQ = "OFF" }
        foreach ($i in "B", "PP", "GF") { if (-not (Toggle $i "OFF")) { T "desync"; return }; $state[$i] = "OFF" }
        T "all OFF; warm-up 60 s"; Start-Sleep 60
        Window "A_warm_OFF"
        foreach ($item in "MQ", "PP", "B", "GF") {
            $seq = "off", "on", "on", "off", "off", "on", "on", "off"; $n = 0
            foreach ($s in $seq) {
                $n++; $want = $s.ToUpper()
                if ($state[$item] -ne $want) { if (-not (Toggle $item $want)) { T "desync at $item w$n"; return }; $state[$item] = $want }
                Start-Sleep 5
                if (-not (Mem-Guard)) { return }
                if (-not (Test-InGameR)) { T "not in game before $item w$n ($script:lastWhy)"; if (-not (Game-Alive)) { T "game dead - abort"; return }; if (-not (Ensure-InGame)) { T "still not in game - abort"; return } }
                Window "A_${item}_w${n}_$s"; T "$item w$n $s done"; Renew
            }
            Window "A_${item}_sentinel_off"; T "$item sentinel done"
            if ($item -eq "B") {   # coordinator: screenshot pair B=0 vs B=1, same camera spot
                $bc = "$ws\captures\pc\b_check"; New-Item -ItemType Directory -Force $bc | Out-Null
                U shot "$bc\B0_a.png" 0 0 2560 1440; Start-Sleep 2; U shot "$bc\B0_b.png" 0 0 2560 1440
                if (Toggle "B" "ON") { $state["B"] = "ON"; Start-Sleep 5; U shot "$bc\B1_a.png" 0 0 2560 1440; Start-Sleep 2; U shot "$bc\B1_b.png" 0 0 2560 1440
                    if (Toggle "B" "OFF") { $state["B"] = "OFF" }; Start-Sleep 5; U shot "$bc\B0_c.png" 0 0 2560 1440 }
                T "B screenshots done"
            }
        }
    } else {
        $five = @{ B = 1; PP = 1; GF = 1; MQ = 1; W6 = 1; PT1 = 0; DL = 0; CpuStats = 1; ReportSec = 10; Barriers = 0 }
        $zero = @{ B = 0; PP = 0; GF = 0; MQ = 0; W6 = 0; PT1 = 0; DL = 0; CpuStats = 1; ReportSec = 10; Barriers = 0 }
        foreach ($i in 1..$Pairs) {
            foreach ($v in "BASE", "REC") {
                Renew; T "launch $v$i"
                Set-Ini $(if ($v -eq "REC") { $five } else { $zero })
                if (-not (Start-Play)) { T "boot failed $v$i - abort (no relaunch loop)"; return }
                Start-Sleep 60
                if (-not (Mem-Guard)) { Close-GameClean; return }
                $WindowSec = 20
                foreach ($w in "stand1", "stand2") { Window "B_${v}${i}_$w" }
                if (Test-InGameR) {
                    $csv = "$out\B_${v}${i}_move1.csv"; $off = Log-Len
                    $pm = Start-Process "$ws\tools\PresentMon.exe" -PassThru -WindowStyle Hidden -ArgumentList "--process_name Nioh3.exe --output_file `"$csv`" --timed 20 --terminate_after_timed --stop_existing_session --no_console_stats"
                    Register-Child $pm "PresentMon"
                    Start-Sleep 2; Push-Location "$ws\tools"; U focus Nioh3
                    for ($k = 0; $k -lt 4; $k++) { U pan 12 220 10; U pan -12 220 10 }
                    Pop-Location; $pm.WaitForExit(40000) | Out-Null; Start-Sleep 2; Log-From $off | Set-Content "$out\B_${v}${i}_move1.log"
                }
                T "$v$i done"; Close-GameClean
            }
        }
    }
} catch {
    T "harness aborted: $($_.Exception.Message)"
} finally {
    Close-GameClean
    Set-Ini @{ Barriers = 0 }
    T "ini restored to fullmod_ini_1716 with Barriers=0"
    Done-Guard
}
