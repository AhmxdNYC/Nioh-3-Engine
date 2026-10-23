# ibaraki_bench.ps1 - OFF/ON FPS comparison in the Ibaraki Doji fight (board task 49), built from fightnow_test.ps1 (same safety: game lock, saves backed up/restored byte for byte, VirtualPad input only).
# Arms: OFF = the 10 CPU keys W1,W2,W3,R1,L1,WK,FG,W6,GF,WL = 0, ON = 1 (same as the Ally report, bridge file 443). Each arm = fresh launch, `fight <Row>` over the add-on pipe, scripted block/attack loop, PresentMon + process CPU for MeasureSec.
# fightnow_test.ps1 - UNATTENDED self-test of Nioh3BossRematch "Fight now (in the arena)" on this PC (notes\boss-rematch-build.md).
# Per catalogue row with an arena point: `fight <id>` over the add-on pipe -> the mission loads at the boss start point -> the add-on
# warps into the arena trigger and verifies (status fnow=pass: a BossBattleMode after the warp; fnow=fail: none within 8 s, after up
# to 3 warps) -> `exit` (MissionRequestReturn, as the mission menu's leave) -> next row. Results: captures\rematch\fightnow-<stamp>.csv.
#
# Safety: never touches a running game - it WAITS until Nioh3 is closed (-WaitMinutes), then takes the bridge game lock 'nioh3'
# (these runs write saves: no other device may run the game). Save folder (%LOCALAPPDATA%\KoeiTecmo\NIOH3) copied to
# captures\save-backups\<stamp>-ibabench first and restored EXACTLY (robocopy /MIR) after the game is closed. Input only through
# VirtualPad (the perf ASI's pipe; no OS input except Steam's 'Play anyway' cloud dialog via steam_cloud_click.ps1). Processes are
# closed / killed only by the exact pid registered at launch (harness_guard). Game-folder files touched (both inis, the add-on asi,
# catalogue) are restored at the end, except the add-on build + catalogue under test stay installed when -KeepBuild (default on)
# and the arena captures file (new data) is kept.
#
#   powershell -NoProfile -ExecutionPolicy Bypass -File tools\rematch\fightnow_test.ps1 -OwnerPid <agent pid> [-Rows R1CBC,R15FF]
#        [-IncludeAtFight] [-WaitMinutes 480] [-MaxMinutes 240] [-LoadWaitSec 240] [-DryRun]
param([int]$OwnerPid = 0, [string]$Device = "pc", [string]$Plan = "", [string]$ResultsCsv = "", [string]$Row = "R7B35", [int]$MeasureSec = 60, [int]$SettleSec = 8, [int]$WarmupSec = 30, [int]$MaxRetries = 1, [int]$MaxRuns = 0,
      [int]$WaitMinutes = 480, [int]$MaxMinutes = 240, [int]$LoadWaitSec = 240, [string]$PerfAsi = "", [bool]$KeepBuild = $true, [switch]$DryRun)
$ErrorActionPreference = "Stop"
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
. "$here\audit_capture_helpers.ps1"
# Fail before waiting, launching, locks or save changes; no unattended UAC prompt.
if (-not $DryRun) {
    $identity=[Security.Principal.WindowsIdentity]::GetCurrent()
    $principal=New-Object Security.Principal.WindowsPrincipal($identity)
    $plu=$principal.IsInRole("Performance Log Users")  # ETW sessions are also allowed for this group (stage1 ran so)
    if(-not $plu -and -not $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)){throw "AUDIT-FATAL: run this audit from an elevated owner shell for ETW capture"}
}
$tm = Join-Path (Split-Path -Parent $here) "testmatrix"
$repo = Split-Path -Parent (Split-Path -Parent $here)
$AddonDir = Join-Path $repo "mod\Nioh3BossRematch"
$GameDir = "C:\Program Files (x86)\Steam\steamapps\common\Nioh3"
$perfIni = "$GameDir\Nioh3EngineFix.ini"
$aAsi = "$GameDir\Nioh3BossRematch.asi"; $aIni = "$GameDir\Nioh3BossRematch.ini"; $aCat = "$GameDir\Nioh3BossRematch_bosses.tsv"; $aLog = "$GameDir\Nioh3BossRematch.log"
$srcCat = Join-Path $AddonDir "Nioh3BossRematch_bosses.tsv"
$saveDir = "$env:LOCALAPPDATA\KoeiTecmo\NIOH3"
# VirtualPad needs the perf ASI >= 1.7.37: the test installs this one for its run and puts the user's back afterwards
if (-not $PerfAsi) { $PerfAsi = "$GameDir\Nioh3EngineFix.asi" }
$stamp = Get-Date -f yyyyMMdd-HHmmss
$out = Join-Path $repo "captures\audit-screen\run-$Device-$stamp"; New-Item -ItemType Directory -Force "$out\orig" | Out-Null
$csv = ""
$saveBak = Join-Path $repo "captures\save-backups\$stamp-ibabench"
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
$uo = "C:\Users\Ahmad Hamza\universal-optimizer\agent-bridge"
$env:UO_BRIDGE_TOKEN_FILE = "$env:APPDATA\uo-bridge\token"; $env:UO_AGENT_ID = "pc"
function Lock([string]$a) { Push-Location $uo; try { $o = (py -m uo_bridge.gamelock $a nioh3-$Device --ttl 14400 2>&1 | Out-String).Trim(); @{ code = $LASTEXITCODE; out = $o } } finally { Pop-Location } }
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
    & (Join-Path $PSScriptRoot '../steam/start_autoplay.ps1')
    Start-Process "steam://rungameid/3681010"; $t0 = Get-Date; $p = $null
    while (((Get-Date) - $t0).TotalSeconds -lt 180) {
        $p = Get-Process Nioh3 -ErrorAction SilentlyContinue | Where-Object MainWindowHandle -ne 0 | Select-Object -First 1; if ($p) { break }
        $cc = "$(& powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $tm 'steam_cloud_click.ps1') 2>$null | Select-Object -First 1)"
        if ($cc -like 'clicked*') { T "  Steam cloud dialog: $cc" }
        Start-Sleep 3
    }
    if (-not $p) { Note-Crash "no window"; throw "no game window after 180 s" }
    $script:gamePid = $p.Id; $script:launchN++; Register-Game; T "launch #$($script:launchN): game pid $($p.Id)"
    Start-Sleep 3; Set-AuditForeground $script:gamePid; Start-Sleep 1
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
    if(Test-Path "$GameDir\Nioh3Training.log"){Copy-Item "$GameDir\Nioh3Training.log" "$out\training_launch$($script:launchN).log" -Force -ErrorAction SilentlyContinue}
    if (Test-Path $aLog) { Copy-Item $aLog "$out\addon_launch$($script:launchN).log" -Force -ErrorAction SilentlyContinue }
}
function Restore {
    if ($script:installed) {
        foreach ($n in @("Nioh3EngineFix.ini", "Nioh3EngineFix.asi", "Nioh3BossRematch.ini", "Nioh3BossRematch.asi", "Nioh3BossRematch_bosses.tsv")) {
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

# ---- Ibaraki bench ----
$Keys = @("W1","W2","W3","R1","L1","WK","FG","W6","GF","WL")
function Set-Keys([string]$t, [string]$v) { foreach ($k in $Keys) { $t = [regex]::Replace($t, "(?m)^($k[ \t]*=[ \t]*)\d+", "`${1}$v") }; return $t }
function Shot([string]$name) {
    try { Add-Type -AssemblyName System.Windows.Forms, System.Drawing -ErrorAction Stop
        $b = [Windows.Forms.SystemInformation]::VirtualScreen; $bmp = New-Object Drawing.Bitmap $b.Width, $b.Height
        $g = [Drawing.Graphics]::FromImage($bmp); $g.CopyFromScreen($b.Left, $b.Top, 0, 0, $bmp.Size); $bmp.Save("$out\$name.png", [Drawing.Imaging.ImageFormat]::Png); $g.Dispose(); $bmp.Dispose() } catch { T "  shot $name failed: $($_.Exception.Message)" }
}
# scripted fight input, one pass ~6 s: attack strings, guard, dodge, step
function Combat-Pass {
    foreach ($c in @("press RB 80","wait 450","press RB 80","wait 450","press RB 80","wait 500","hold LB","wait 1100","release LB","stick L 0 -1 400","press RB 80","wait 500","press A 80","wait 700")) { $null = VP $c }
}
function Pos([string]$s) { return (F $s 'pos') }
$armCsv = "$out\arms.csv"; "arm,n,seconds,avg_fps,low1_fps,cpu_cores,cpu_pct_one_core,loads,valid,note" | Set-Content $armCsv
Init-Guard -Name "ibaraki_bench" -OwnerPid $OwnerPid -MaxMinutes $MaxMinutes -Cleanup { Close-Game; Restore; Stop-TestPopup }
try {
    T "audit_screen_run: device $Device row $Row measure $MeasureSec s out=$out"
    foreach ($f in @($srcCat, $perfIni, $PerfAsi)) { if (-not (Test-Path $f)) { throw "missing: $f" } }
    if ($DryRun) { T "[dry] ok"; return }
    if (Get-Process Nioh3 -ErrorAction SilentlyContinue) { throw "Nioh3 is running" }
    $r = Lock "acquire"; if ($r.code -ne 0) { throw "lock: $($r.out)" }; $script:locked = $true; T "lock: $($r.out)"
    New-Item -ItemType Directory -Force $saveBak | Out-Null
    $o = robocopy $saveDir $saveBak /MIR /R:2 /W:2 /NFL /NDL /NP /NJH; T "save folder backed up to $saveBak (robocopy exit $LASTEXITCODE)"
    if ($LASTEXITCODE -ge 8) { throw "save backup failed" }
    $script:saved = $true
    foreach ($n in @("Nioh3EngineFix.ini", "Nioh3EngineFix.asi", "Nioh3BossRematch.ini", "Nioh3BossRematch.asi", "Nioh3BossRematch_bosses.tsv")) {
        $p = Join-Path $GameDir $n; $script:had[$n] = Test-Path $p; if ($script:had[$n]) { Copy-Item $p "$out\orig\" -Force }
    }
    $script:installed = $true
    $aText = if (Test-Path $aIni) { [IO.File]::ReadAllText($aIni) } else { [IO.File]::ReadAllText((Join-Path $AddonDir "Nioh3BossRematch.ini")) }
    foreach ($kv in @(@("Rematch", "Enable", "1"), @("Rematch", "FightNow", "1"), @("Rematch", "FightNowWarp", "1"), @("Remote", "Pipe", "1"), @("Menu", "Overlay", "0"), @("Debug", "RematchLog", "0"))) { $aText = Set-IniKey $aText $kv[0] $kv[1] $kv[2] }
    [IO.File]::WriteAllText($aIni, $aText, (New-Object Text.UTF8Encoding($false)))
    $baseIni = Force-Acl (Force-Vpad ([IO.File]::ReadAllText("$out\orig\Nioh3EngineFix.ini")))
    Start-TestPopup -Name "Ibaraki bench" -InputMode vpad
    if (-not $Plan) { $Plan = Join-Path $repo "captures\audit-screen" }
    if (-not $ResultsCsv) { $ResultsCsv = Join-Path $Plan "results-$Device.csv" }
    $sched = @(Import-Csv (Join-Path $Plan "schedule.csv") | Where-Object { $_.device -eq $Device })
    $tmpl = @{}; foreach ($r in (Import-Csv (Join-Path $Plan "results-template.csv"))) { $tmpl[$r.run_id] = $r }
    if (-not (Test-Path $ResultsCsv)) { "run_id,fixture,ini_sha256,valid,avg_fps,low1_fps,frametime_ms,key,arm,frames,loads,note" | Set-Content $ResultsCsv }
    $done = @{}; foreach ($r in (Import-Csv $ResultsCsv)) { if ($r.valid -eq "1") { $done[$r.run_id] = 1 } }
    $armN = 0
    $ran = 0
    foreach ($run in $sched) {
        $arm = $run.arm; $armN++; $tag = $run.run_id
        if ($done[$tag]) { T "skip $tag (already valid)"; continue }
        if ($MaxRuns -and $ran -ge $MaxRuns) { T "chunk of $MaxRuns runs done - stop (re-run for the next chunk)"; break }
        $ran++
        if (Test-Path (Join-Path $repo ".handoff\PAUSED")) { T "PAUSED - stop"; break }
        Set-TestStatus "run $armN/$($sched.Count): $($run.key) $arm"
        T "== run $tag $($run.key) $arm"
        $iniText = Force-Acl (Force-Vpad ([IO.File]::ReadAllText((Join-Path $Plan $run.ini))))
        [IO.File]::WriteAllText($perfIni, $iniText, (New-Object Text.UTF8Encoding($false)))
        Copy-Item $perfIni "$out\$tag.ini" -Force
        $tag = $run.run_id
        $pm=$null; $cp=$null; $fatal=$false
        try {
            for ($att = 1; $att -le 2; $att++) {   # a launch that stalls before the player loads (about every 2nd run on pc) is retried once
                try { $null = Launch; break } catch { T "  launch attempt $att failed: $($_.Exception.Message)"; if ($att -ge 2) { throw }; Close-Game }
            }
            Set-AuditForeground $script:gamePid
            $trainingLog="$GameDir\Nioh3Training.log"
            $fightOffset=Get-AuditLogOffset $trainingLog
            $q = RMP "fight $Row"; T "  fight: $q"
            if ($q -notmatch '^queued') { throw "fight refused: $q" }
            $end = Wait-RM { param($x) (F $x 'fnow') -in @('pass','fail') -or (LastOf $x) -match '^(started.*in place|refused)' } 30 1
            if (-not $end -or (LastOf $end) -match '^refused' -or (F $end 'fnow') -eq 'fail') { throw "fight not entered: $(RMP status)" }
            $swapMode=(F $end 'fnow') -ne 'pass'
            if ($swapMode) {   # no-load swap: the boss is live when Nioh3Training logs boss HP heartbeats
                $fresh=''; $hbT=[Diagnostics.Stopwatch]::StartNew()
                while(-not(Test-AuditBossHeartbeat $fresh) -and $hbT.Elapsed.TotalSeconds -lt 60){
                    Start-Sleep 2
                    $fresh += Read-AuditLogDelta $trainingLog ([ref]$fightOffset)
                }
                $fresh | Set-Content "$out\$tag-spawn-evidence.log"
                if(-not(Test-AuditBossHeartbeat $fresh)){throw "AUDIT-FATAL: swap acknowledged but no NEW live boss heartbeat: $(RMP status)"}
            }
            T "  in the fight: $(RMP status)"
            Start-Sleep $SettleSec
            $w0 = Get-Date; while (((Get-Date) - $w0).TotalSeconds -lt $WarmupSec) { Combat-Pass }
            $p0 = Pos (RMP status); $null = VP "stick L 0 1 1200"; Start-Sleep 1; $p1 = Pos (RMP status)
            T "  gate: player=$(F (RMP status) 'player') fnow=$(F (RMP status) 'fnow') pos $p0 -> $p1"
            Shot "$tag-gate"
            if($swapMode){
                $fresh=Read-AuditLogDelta $trainingLog ([ref]$fightOffset)
                if(-not(Test-AuditBossHeartbeat $fresh)){throw 'AUDIT-FATAL: no new boss heartbeat during warmup'}
            }
            Set-AuditForeground $script:gamePid
            $gp = $script:gamePid
            $pmCsv = "$out\$tag-presentmon.csv"
            $session="NiohAudit-$PID-$([guid]::NewGuid().ToString('N'))"
            $pmErr="$out\$tag-presentmon.stderr.log"
            $pmArgs=Get-AuditPresentArgs $gp $pmCsv $MeasureSec $session
            T "  PresentMon session=$session pid=$gp args=$pmArgs"
            $captureStart=Get-Date
            try { $pm = Start-Process "$repo\tools\PresentMon.exe" -PassThru -WindowStyle Hidden -ArgumentList $pmArgs -RedirectStandardOutput "$out\$tag-presentmon.stdout.log" -RedirectStandardError $pmErr } catch { throw "AUDIT-FATAL: PresentMon could not start: $($_.Exception.Message)" }
            Register-Child $pm "PresentMon"
            Start-Sleep -Milliseconds 500
            $pm.Refresh()
            if($pm.HasExited){Assert-AuditCaptureResult $true $pm.ExitCode $pmCsv $pmErr; throw 'AUDIT-FATAL: PresentMon exited before capture started'}
            $cpuOut = "$out\$tag-cpu.json"
            $cp = Start-Process py -PassThru -WindowStyle Hidden -RedirectStandardOutput "$out\$tag-cpu.out" -ArgumentList "-I `"$tm\process_cpu_window.py`" --pid $gp --seconds $MeasureSec --out `"$cpuOut`" --label $tag"
            Register-Child $cp "cpuwindow"
            $t0 = $captureStart; $lastBoss=$t0; $bossDelta=''; $loads = 0; $prevL = '0'; $shots = 0
            while (((Get-Date) - $t0).TotalSeconds -lt $MeasureSec) {
                Assert-AuditForeground $gp
                Combat-Pass
                Assert-AuditForeground $gp
                $pm.Refresh()
                if($pm.HasExited -and ((Get-Date)-$t0).TotalSeconds -lt ($MeasureSec-2)){throw "AUDIT-FATAL: PresentMon exited early ($($pm.ExitCode)); see $pmErr"}
                if($swapMode){
                    $bossDelta+=Read-AuditLogDelta $trainingLog ([ref]$fightOffset)
                    if(Test-AuditBossHeartbeat $bossDelta){$lastBoss=Get-Date;$bossDelta=''}
                    if(((Get-Date)-$lastBoss).TotalSeconds -gt 15){throw 'AUDIT-FATAL: no live boss heartbeat for 15 seconds during capture'}
                }
                $sx = RMP status; $l = F $sx 'loading'
                if ($l -eq '1' -and $prevL -ne '1') { $loads++; T "  !! loading during measure at +$([int](((Get-Date) - $t0).TotalSeconds)) s: $(LastOf $sx)" }
                $prevL = $l
                if (((Get-Date) - $t0).TotalSeconds -ge ($shots + 1) * 20) { $shots++; Shot "$tag-mid$shots" }
            }
            $pmDone=$pm.WaitForExit(30000)
            $pmCode=if($pmDone){$pm.ExitCode}else{-1}
            Assert-AuditCaptureResult $pmDone $pmCode $pmCsv $pmErr
            $cp.WaitForExit(30000) | Out-Null
            Shot "$tag-end"
            T "  end status: $(RMP status)"
            $rows = @(Import-Csv $pmCsv)
            if(-not $rows.Count -or $rows[0].PSObject.Properties.Name -notcontains 'MsBetweenPresents'){throw 'AUDIT-FATAL: missing v1 frame-time column/rows in PresentMon CSV'}
            $col = if ($rows[0].PSObject.Properties.Name -contains 'MsBetweenPresents') { 'MsBetweenPresents' } else { 'msBetweenPresents' }
            $ms = @($rows | ForEach-Object { [double]$_.$col } | Where-Object { $_ -gt 0 })
            if($ms.Count -lt 2){throw 'AUDIT-FATAL: fewer than two positive frame times'}
            $sec = ($ms | Measure-Object -Sum).Sum / 1000.0
            $fps = [math]::Round($ms.Count / $sec, 2)
            $sorted = $ms | Sort-Object -Descending; $k = [math]::Max(1, [int][math]::Ceiling($ms.Count * 0.01))
            $low1 = [math]::Round(1000.0 / (($sorted | Select-Object -First $k | Measure-Object -Average).Average), 2)
            $cj = if (Test-Path $cpuOut) { (Get-Content $cpuOut | Select-Object -Last 1 | ConvertFrom-Json) } else { $null }
            $cpuTxt = if ($cj) { ($cj | ConvertTo-Json -Compress -Depth 4) } else { "" }
            $valid = ($loads -eq 0 -and $ms.Count -gt 500)
            T "  ARM $tag : avg_fps=$fps low1_fps=$low1 frames=$($ms.Count) over $([math]::Round($sec,1)) s loads=$loads valid=$valid cpu=$cpuTxt"
            $ft = [math]::Round(1000.0 / $fps, 4)
            ('{0},{1},{2},{3},{4},{5},{6},{7},{8},{9},{10},' -f $tag, $tmpl[$tag].fixture, $tmpl[$tag].ini_sha256, [int]$valid, $fps, $low1, $ft, $run.key, $arm, $ms.Count, $loads) | Add-Content $ResultsCsv
        } catch { $fatal=$_.Exception.Message -like 'AUDIT-FATAL:*'; T "!! run $tag failed: $($_.Exception.Message)"; "$tag,$($tmpl[$tag].fixture),$($tmpl[$tag].ini_sha256),0,,,,$($run.key),$arm,,,$($_.Exception.Message -replace '[,\r\n]',' ')" | Add-Content $ResultsCsv }
        # Only capture children created by this run; never terminate other ETW consumers.
        foreach($child in @($pm,$cp)){if($child){$child.Refresh();if(-not $child.HasExited){Stop-Process -Id $child.Id -Force -ErrorAction SilentlyContinue}}}
        Close-Game
        if($fatal){throw 'AUDIT-FATAL: batch stopped after infrastructure failure; inspect per-run capture logs before retry'}
    }
} catch { $script:auditFailed=$true; T "!! $($_.Exception.Message)" }
finally { Close-Game; Stop-TestPopup; Restore; Done-Guard; T "results: $ResultsCsv" }

if($script:auditFailed){exit 1}
