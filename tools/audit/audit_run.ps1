# audit_run.ps1 - settings-audit Stage 1 runner (board task #108). Built on fightnow_test.ps1 (same safety: bridge game lock, save backup +
# exact restore, VirtualPad input only, pid-exact closes). Per scheduled run (captures\audit-screen\schedule.csv, this device): install that
# run's ini (+ VirtualPad/ACL), cold-launch, title -> Continue, add-on `fight R7B35` (Yoritomo fight-now), idle -WarmSec, PresentMon -CaptureSec,
# `exit`, close game. A death/load inside the window marks the run valid=0 (re-queued once). Resumable: runs with a row in the results csv are skipped.
# Metrics (avg_fps, low1_fps = 1000/p99 frame time, frametime_ms = mean) from PresentMon MsBetweenPresents over the capture. FG is OFF in config.xml.
#   powershell -NoProfile -ExecutionPolicy Bypass -File tools\audit\audit_run.ps1 -PlanRoot <worktree>\captures\audit-screen [-Device pc2] [-Only id,id] [-Limit N]
param([int]$OwnerPid = 0, [string]$PlanRoot = "", [string]$Device = "pc2", [string[]]$Only = @(), [int]$Limit = 0, [int]$WarmSec = 30, [int]$CaptureSec = 60,
      [string]$Row = "R7B35", [int]$WaitMinutes = 480, [int]$MaxMinutes = 600, [int]$LoadWaitSec = 240, [string]$PerfAsi = "", [string]$AddonAsi = "", [string]$ResultsCsv = "", [string]$PayloadDir = "", [switch]$DryRun)
$ErrorActionPreference = "Continue"
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
$tm = Join-Path (Split-Path -Parent $here) "testmatrix"
$repo = Split-Path -Parent (Split-Path -Parent $here)
$AddonDir = Join-Path $repo "mod\Nioh3BossRematch"
$GameDir = "C:\Program Files (x86)\Steam\steamapps\common\Nioh3"
$perfIni = "$GameDir\Nioh3EngineFix.ini"
$aAsi = "$GameDir\Nioh3BossRematch.asi"; $aIni = "$GameDir\Nioh3BossRematch.ini"; $aCat = "$GameDir\Nioh3BossRematch_bosses.tsv"; $aLog = "$GameDir\Nioh3BossRematch.log"
$srcAsi = if ($AddonAsi) { $AddonAsi } else { $aAsi }; $srcCat = $aCat   # default: the INSTALLED add-on/perf build + catalogue (the audit measures what is installed)
$saveDir = "$env:LOCALAPPDATA\KoeiTecmo\NIOH3"
# VirtualPad needs the perf ASI >= 1.7.37: the test installs this one for its run and puts the user's back afterwards
if (-not $PerfAsi) { $PerfAsi = "$GameDir\Nioh3EngineFix.asi" }
$KeepBuild = $false   # -PayloadDir files are an audit-only install: everything managed goes back afterwards
$ManagedFiles = @("Nioh3EngineFix.ini", "Nioh3EngineFix.asi", "Nioh3BossRematch.ini", "Nioh3BossRematch.asi", "Nioh3BossRematch_bosses.tsv", "Nioh3BossRematch_arenas.tsv",
                  "Nioh3Training.asi", "Nioh3Training_actions.tsv", "Nioh3Training_arenas.tsv", "Nioh3Training_bgm.tsv", "Nioh3Training_actionsets.tsv")
$KeepBuild = $true
$stamp = Get-Date -f yyyyMMdd-HHmmss
$out = Join-Path $repo "captures\audit-runs\$Device-$stamp"; New-Item -ItemType Directory -Force "$out\orig" | Out-Null
$csv = if ($ResultsCsv) { $ResultsCsv } else { Join-Path $repo "captures\audit-runs\$Device-results.csv" }
$saveBak = Join-Path $repo "captures\save-backups\$stamp-audit-run"
$trace = "$out\trace.txt"
function T([string]$s) { $l = "[{0}] {1}" -f (Get-Date -f HH:mm:ss), $s; [IO.File]::AppendAllText($trace, $l + "`r`n"); Write-Host $l }

# ---- wait for the user's game to be closed (never touch it) ----
$t0 = Get-Date
while (-not $DryRun -and (Get-Process Nioh3 -ErrorAction SilentlyContinue)) {
    if (((Get-Date) - $t0).TotalMinutes -gt $WaitMinutes) { T "Nioh3 still running after $WaitMinutes min - giving up (nothing touched)"; exit 2 }
    if ($OwnerPid -and -not (Get-Process -Id $OwnerPid -ErrorAction SilentlyContinue)) { T "owner $OwnerPid gone while waiting - exit"; exit 99 }
    Start-Sleep 30
}
T "Nioh3 is not running (waited $([math]::Round(((Get-Date) - $t0).TotalMinutes, 1)) min)"
if (-not $DryRun) { Start-Sleep 20 }   # the game may still be flushing its save right after the window closed
if (-not $DryRun -and (Get-Process Nioh3 -ErrorAction SilentlyContinue)) { T "Nioh3 started again - exit (nothing touched)"; exit 2 }

. "$tm\harness_guard.ps1"
. "$tm\vpad.ps1"
$uo = "C:\Users\User\projects\universal-optimizer\agent-bridge"
$env:UO_BRIDGE_TOKEN_FILE = "$env:APPDATA\uo-bridge\token"; $env:UO_AGENT_ID = "pc2"
function Lock([string]$a) { Push-Location $uo; try { $o = (py -m uo_bridge.gamelock $a nioh3-pc2 --ttl 14400 2>&1 | Out-String).Trim(); @{ code = $LASTEXITCODE; out = $o } } finally { Pop-Location } }
function Set-IniKey([string]$t, [string]$sec, [string]$key, [string]$val) {
    $nl = "`r`n"
    $m = [regex]::Match($t, "(?ms)^\[$([regex]::Escape($sec))\][^\r\n]*\r?\n(.*?)(?=^\[|\z)")
    if (-not $m.Success) { return $t.TrimEnd() + "$nl$nl[$sec]$nl$key = $val$nl" }
    $body = $m.Groups[1].Value
    $re = "(?m)^$([regex]::Escape($key))[ \t]*=.*?(\r?)$"
    if ([regex]::IsMatch($body, $re)) { $nb = [regex]::Replace($body, $re, "$key = $val`$1", 1) } else { $nb = "$key = $val$nl" + $body }
    return $t.Substring(0, $m.Groups[1].Index) + $nb + $t.Substring($m.Groups[1].Index + $m.Groups[1].Length)
}
function RMP([string]$cmd, [int]$timeoutMs = 5000) {
    if ($global:HG) { Assert-Owner }
    if ($DryRun) { T "  [dry] rm $cmd"; return "dry" }
    $p = New-Object IO.Pipes.NamedPipeClientStream(".", "nioh3bossrematch", [IO.Pipes.PipeDirection]::InOut)
    try {
        $p.Connect($timeoutMs)
        $w = New-Object IO.StreamWriter($p); $w.AutoFlush = $true; $rd = New-Object IO.StreamReader($p)
        $w.WriteLine($cmd)
        $t = $rd.ReadLineAsync(); if (-not $t.Wait($timeoutMs)) { return "error timeout" }
        return $t.Result
    } catch { return "error $($_.Exception.Message)" } finally { $p.Dispose() }
}
function F([string]$reply, [string]$key) { if ($reply -match "(?:^| )$key=(\S+)") { return $Matches[1] }; return "" }
function LastOf([string]$reply) { if ($reply -match ' last=(.*)$') { return $Matches[1] }; return "" }
function Wait-RM([scriptblock]$cond, [int]$sec, [int]$poll = 1) {
    $t0 = Get-Date
    do { $s = RMP status; if (& $cond $s) { return $s }; Start-Sleep $poll } while (((Get-Date) - $t0).TotalSeconds -lt $sec)
    return $null
}

$script:gamePid = 0; $script:locked = $false; $script:installed = $false; $script:saved = $false; $script:had = @{}; $script:launchN = 0
function Launch {
    Assert-Launch
    $before = VP-ForegroundInfo
    Start-Process powershell -ArgumentList '-NoProfile -ExecutionPolicy Bypass -File C:/Users/User/projects/universal-optimizer/tools/steam/steam_autoplay.ps1 -Minutes 15' -WindowStyle Hidden
    Start-Process "steam://rungameid/3681010"; $t0 = Get-Date; $p = $null
    while (((Get-Date) - $t0).TotalSeconds -lt 180) {
        $p = Get-Process Nioh3 -ErrorAction SilentlyContinue | Where-Object MainWindowHandle -ne 0 | Select-Object -First 1; if ($p) { break }
        $cc = "$(& powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $tm 'steam_cloud_click.ps1') 2>$null | Select-Object -First 1)"
        if ($cc -like 'clicked*') { T "  Steam cloud dialog: $cc" }
        Start-Sleep 3
    }
    if (-not $p) { Note-Crash "no window"; throw "no game window after 180 s" }
    $script:gamePid = $p.Id; $script:launchN++; Register-Game; T "launch #$($script:launchN): game pid $($p.Id)"
    Start-Sleep 3; VP-GiveBackFocus $before; Start-Sleep 1
    if (-not (VP-WaitPipe 180)) { throw "no VirtualPad pipe (perf ASI with VirtualPad = 1?)" }
    $ok = VP-TitleContinue 300 "$out\title_continue_$($script:launchN).txt"
    $pg = RMP ping; T "title -> Continue -> field: $ok; add-on: $pg"
    if ($pg -notmatch 'live=1') { throw "add-on not live: $pg" }
    if (-not (Wait-RM { param($x) (F $x 'player') -eq '1' -and (F $x 'loading') -eq '0' } 120)) { throw "no player after Continue" }
    Start-Sleep 5
    return $ok
}
function Close-Game {
    $p = if ($script:gamePid) { Get-Process -Id $script:gamePid -ErrorAction SilentlyContinue } else { $null }
    if ($p) { Note-Closed; $null = $p.CloseMainWindow(); $p.WaitForExit(45000) | Out-Null; if (-not $p.HasExited) { T "!! game did not exit in 45 s - killing pid $($p.Id)"; Stop-Process -Id $p.Id -Force }; Start-Sleep 8 }
    $script:gamePid = 0
    if (Test-Path $aLog) { Copy-Item $aLog "$out\addon_launch$($script:launchN).log" -Force -ErrorAction SilentlyContinue }
}
function Restore {
    if ($script:installed) {
        foreach ($n in $ManagedFiles) {
            if ($KeepBuild -and $n -in @("Nioh3BossRematch.asi", "Nioh3BossRematch_bosses.tsv")) { continue }
            $dst = Join-Path $GameDir $n
            if ($script:had[$n]) { Copy-Item "$out\orig\$n" $dst -Force } else { Remove-Item $dst -Force -ErrorAction SilentlyContinue }
        }
        $script:installed = $false; T "game folder restored (inis, perf ASI$(if (-not $KeepBuild) { ', add-on asi, catalogue' })); arena captures kept"
    }
    if ($script:saved -and -not (Get-Process Nioh3 -ErrorAction SilentlyContinue)) {
        $o = robocopy $saveBak $saveDir /MIR /R:2 /W:2 /NFL /NDL /NP /NJH; $code = $LASTEXITCODE
        T "save folder restored from $saveBak (robocopy /MIR exit $code)"
        $a = Get-ChildItem $saveBak -Recurse -File | ForEach-Object { "$($_.FullName.Substring($saveBak.Length))|$($_.Length)|$((Get-FileHash $_.FullName -Algorithm SHA1).Hash)" } | Sort-Object
        $b = Get-ChildItem $saveDir -Recurse -File | ForEach-Object { "$($_.FullName.Substring($saveDir.Length))|$($_.Length)|$((Get-FileHash $_.FullName -Algorithm SHA1).Hash)" } | Sort-Object
        $same = (@(Compare-Object $a $b).Count -eq 0)
        T "save restore verified byte for byte: $same ($($a.Count) files)"
        if (-not $same) { T "!! SAVE DIFFERS FROM THE BACKUP - backup kept at $saveBak" }
        $script:saved = $false
    }
    if ($script:locked) { $r = Lock "release"; $script:locked = $false; T "lock released: $($r.out)" }
}

# ---- schedule ----
if (-not $PlanRoot) { throw "-PlanRoot (captures\audit-screen of the plan branch) required" }
$sched = @(Import-Csv (Join-Path $PlanRoot "schedule.csv") | Where-Object { $_.device -eq $Device })
$tmpl = @{}; foreach ($r in (Import-Csv (Join-Path $PlanRoot "results-template.csv"))) { $tmpl[$r.run_id] = $r }
$Only = @($Only | ForEach-Object { $_ -split "," } | Where-Object { $_ })
$done = @{}; if (Test-Path $csv) { foreach ($r in (Import-Csv $csv)) { if ($r.valid -eq '1') { $done[$r.run_id] = $true } } }
$todo = @($sched | Where-Object { ($Only.Count -eq 0 -or $Only -contains $_.run_id) -and -not $done[$_.run_id] })
if ($Limit -gt 0) { $todo = @($todo | Select-Object -First $Limit) }
if (-not (Test-Path $csv)) { "run_id,fixture,ini_sha256,valid,avg_fps,low1_fps,frametime_ms,frames,min_fps,note" | Set-Content $csv }
$PmExe = Join-Path $repo "tools\PresentMon.exe"
function Get-PmMetrics([string]$f) {
    $o = & py -I (Join-Path $PSScriptRoot "pm_metrics.py") $f 2>&1 | Out-String
    return $o.Trim()
}
$trLog = "$GameDir\Nioh3Training.log"   # the Training add-on logs the boss heartbeat ('HB +N ms boss HP') while a fight is live; appended across launches
function Get-LogLen { try { return (Get-Item $trLog -ErrorAction Stop).Length } catch { return 0 } }
function Get-HbSince([long]$off) {   # heartbeat lines written after byte offset $off
    try { $fs = [IO.File]::Open($trLog, 'Open', 'Read', 'ReadWrite'); $null = $fs.Seek($off, 'Begin'); $sr = New-Object IO.StreamReader($fs); $all = $sr.ReadToEnd(); $sr.Dispose(); $fs.Dispose() } catch { return @() }
    return @($all -split "`n" | Where-Object { $_ -match '\] HB \+' })
}
function Run-One($s) {
    $armIni = Join-Path $PlanRoot $s.ini
    if (-not (Test-Path $armIni)) { throw "ini missing: $armIni" }
    $planSha = $tmpl[$s.run_id].ini_sha256
    $txt = Force-Acl (Force-Vpad ([IO.File]::ReadAllText($armIni)))
    [IO.File]::WriteAllText($perfIni, $txt, (New-Object Text.UTF8Encoding($false)))
    $cfgFg = [regex]::Match([IO.File]::ReadAllText("$saveDir\config.xml"), "<FRAME_GENERATION>([^<]*)<").Groups[1].Value
    T "== $($s.run_id) $($s.key) $($s.arm): ini $($s.ini) (plan sha $planSha); config FG=$cfgFg"
    $res = [ordered]@{ run_id = $s.run_id; sha = $planSha; valid = 0; avg = ""; low1 = ""; ft = ""; frames = ""; minfps = ""; note = "" }
    $null = Launch
    $g0 = [int](F (RMP "scan") 'gen'); $t0 = Get-Date
    do { $rr = RMP "row $Row"; if ([int](F $rr 'gen') -gt $g0) { break }; Start-Sleep 1 } while (((Get-Date) - $t0).TotalSeconds -lt 10)
    if ((F $rr 'status') -ne '2') { $res.note = "row $Row not available: $rr"; return $res }
    $hbOff = Get-LogLen
    $q = RMP "fight $Row"; T "  fight: $q"
    if ($q -notmatch '^queued') { $res.note = "refused: $q"; return $res }
    $st = Wait-RM { param($x) (LastOf $x) -match '^(started|refused)' } 20
    if ((LastOf $st) -notmatch '^started') { $res.note = "game thread: $(LastOf $st)"; return $res }
    # in-place arena swap (Nioh3Training) never sets fnow=pass: the fight is live once the add-on's boss heartbeat ('HB +') lines appear
    $tf = Get-Date; $live = $false
    while (((Get-Date) - $tf).TotalSeconds -lt ($LoadWaitSec + 60)) {
        if (@(Get-HbSince $hbOff).Count -gt 0) { $live = $true; break }
        if ((F (RMP status) 'fnow') -eq 'fail') { break }
        Start-Sleep 2
    }
    if (-not $live) { $res.note = "no boss heartbeat after fight now: $(RMP status)"; return $res }
    T "  fight live after $([math]::Round(((Get-Date) - $tf).TotalSeconds, 1)) s; warm-up $WarmSec s"
    $tw = Get-Date; $died = $false
    while (((Get-Date) - $tw).TotalSeconds -lt $WarmSec) { if ((F (RMP status) 'loading') -eq '1') { $died = $true; break }; Start-Sleep 2 }
    if ($died) { $res.note = "player died/loaded during warm-up"; return $res }
    $game = Get-Process -Id $script:gamePid -ErrorAction Stop
    $pmCsv = "$out\pm_$($s.run_id).csv"
    $pa = "--process_id $($game.Id) --output_file `"$pmCsv`" --v2_metrics --stop_existing_session --session_name Nioh3Audit --no_console_stats --timed $CaptureSec"
    $pm = Start-Process $PmExe -ArgumentList $pa -WindowStyle Hidden -PassThru -RedirectStandardOutput "$out\pm_$($s.run_id).out" -RedirectStandardError "$out\pm_$($s.run_id).err"
    $tc = Get-Date
    while (((Get-Date) - $tc).TotalSeconds -lt $CaptureSec + 2) { if ((F (RMP status) 'loading') -eq '1') { $died = $true }; Start-Sleep 3 }
    if (-not $pm.HasExited) { $pm.WaitForExit(15000) | Out-Null }
    if (-not $pm.HasExited) { try { Start-Process $PmExe -ArgumentList "--terminate_existing_session --session_name Nioh3Audit" -WindowStyle Hidden -Wait } catch {} }
    Copy-Item $perfIni "$out\applied_$($s.run_id).ini" -Force
    if ($died) { $res.note = "player died/loaded inside the capture window"; return $res }
    $m = Get-PmMetrics $pmCsv; T "  pm: $m"
    $j = $null; try { $j = $m | ConvertFrom-Json } catch {}
    if (-not $j -or -not $j.ok) { $res.note = "pm metrics: $m"; return $res }
    $res.avg = $j.avg_fps; $res.low1 = $j.low1_fps; $res.ft = $j.frametime_ms; $res.frames = $j.frames; $res.minfps = $j.min_fps
    $res.valid = 1
    return $res
}
Init-Guard -Name "audit_run" -OwnerPid $OwnerPid -MaxMinutes $MaxMinutes -Cleanup { Close-Game; Restore; Stop-TestPopup }
try {
    T "audit_run ${Device}: $($sched.Count) scheduled, $($todo.Count) to do; out=$out csv=$csv"
    foreach ($f in @($PmExe, $srcAsi, $srcCat, $perfIni, $PerfAsi)) { if (-not (Test-Path $f)) { throw "missing: $f" } }
    if ($DryRun) { T "[dry] would run: $(($todo | ForEach-Object run_id) -join ' ')"; return }
    if (Get-Process Nioh3 -ErrorAction SilentlyContinue) { throw "Nioh3 is running" }
    $r = Lock "acquire"; if ($r.code -ne 0) { throw "lock: $($r.out)" }; $script:locked = $true; T "lock: $($r.out)"
    New-Item -ItemType Directory -Force $saveBak | Out-Null
    $o = robocopy $saveDir $saveBak /MIR /R:2 /W:2 /NFL /NDL /NP /NJH; T "save folder backed up to $saveBak (robocopy exit $LASTEXITCODE)"
    if ($LASTEXITCODE -ge 8) { throw "save backup failed" }
    $script:saved = $true
    foreach ($n in $ManagedFiles) {
        $p = Join-Path $GameDir $n; $script:had[$n] = Test-Path $p; if ($script:had[$n]) { Copy-Item $p "$out\orig\" -Force }
    }
    $aText = if (Test-Path $aIni) { [IO.File]::ReadAllText($aIni) } else { [IO.File]::ReadAllText((Join-Path $AddonDir "Nioh3BossRematch.ini")) }
    foreach ($kv in @(@("Rematch", "Enable", "1"), @("Rematch", "FightNow", "1"), @("Rematch", "FightNowWarp", "1"), @("Remote", "Pipe", "1"), @("Menu", "Overlay", "0"))) {
        $aText = Set-IniKey $aText $kv[0] $kv[1] $kv[2]
    }
    $script:installed = $true
    [IO.File]::WriteAllText($aIni, $aText, (New-Object Text.UTF8Encoding($false)))
    if ($PayloadDir) { foreach ($pf in (Get-ChildItem $PayloadDir -File)) { if ($pf.Name -in $ManagedFiles -and $pf.Name -ne "Nioh3EngineFix.ini") { Copy-Item $pf.FullName (Join-Path $GameDir $pf.Name) -Force; T "  payload: $($pf.Name)" } } }
    elseif ($PerfAsi -ne "$GameDir\Nioh3EngineFix.asi") { Copy-Item $PerfAsi "$GameDir\Nioh3EngineFix.asi" -Force }
    Start-TestPopup -Name "Settings audit stage 1" -InputMode vpad
    $n = 0
    foreach ($s in $todo) {
        $n++; Set-TestStatus "run $n/$($todo.Count): $($s.run_id) $($s.key) $($s.arm)"
        $res = $null
        for ($try = 1; $try -le 2; $try++) {
            try { $res = Run-One $s } catch { T "!! $($_.Exception.Message)"; $res = [ordered]@{ run_id = $s.run_id; sha = $tmpl[$s.run_id].ini_sha256; valid = 0; avg = ""; low1 = ""; ft = ""; frames = ""; minfps = ""; note = "exception: $($_.Exception.Message)" } }
            Close-Game
            if ($res.valid -eq 1) { break }
            T "  attempt $try invalid: $($res.note)"
        }
        '"{0}","{1}","{2}","{3}","{4}","{5}","{6}","{7}","{8}","{9}"' -f $res.run_id, $tmpl[$s.run_id].fixture, $res.sha, $res.valid, $res.avg, $res.low1, $res.ft, $res.frames, $res.minfps, ($res.note -replace '"', "'") | Add-Content $csv
        T "  -> $($res.run_id) valid=$($res.valid) avg=$($res.avg) low1=$($res.low1) ft=$($res.ft)"
    }
} catch {
    T "!! $($_.Exception.Message)"
} finally {
    Close-Game
    Stop-TestPopup
    Restore
    Done-Guard
    T "csv: $csv"
}
