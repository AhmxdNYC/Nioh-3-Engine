# xworld_probe.ps1 - cross-world stage-load experiment (pc2 task #5116): drives Nioh3Training / Boss Rematch commands from a scenario file (lines: T <training cmd> | RM <addon cmd> | S <sec> | SHOT <name> | RMWAIT <sec> | POLL <sec> <every> <training cmd>), same harness as spawn_probe.ps1 (lock, save backup+restore, VirtualPad, inis).
# Per catalogue row with an arena point: `fight <id>` over the add-on pipe -> the mission loads at the boss start point -> the add-on
# warps into the arena trigger and verifies (status fnow=pass: a BossBattleMode after the warp; fnow=fail: none within 8 s, after up
# to 3 warps) -> `exit` (MissionRequestReturn, as the mission menu's leave) -> next row. Results: captures\rematch\fightnow-<stamp>.csv.
#
# Safety: never touches a running game - it WAITS until Nioh3 is closed (-WaitMinutes), then takes the bridge game lock 'nioh3'
# (these runs write saves: no other device may run the game). Save folder (%LOCALAPPDATA%\KoeiTecmo\NIOH3) copied to
# captures\save-backups\<stamp>-fightnow-test first and restored EXACTLY (robocopy /MIR) after the game is closed. Input only through
# VirtualPad (the perf ASI's pipe; no OS input except Steam's 'Play anyway' cloud dialog via steam_cloud_click.ps1). Processes are
# closed / killed only by the exact pid registered at launch (harness_guard). Game-folder files touched (both inis, the add-on asi,
# catalogue) are restored at the end, except the add-on build + catalogue under test stay installed when -KeepBuild (default on)
# and the arena captures file (new data) is kept.
#
#   powershell -NoProfile -ExecutionPolicy Bypass -File tools\rematch\fightnow_test.ps1 -OwnerPid <agent pid> [-Rows R1CBC,R15FF]
#        [-IncludeAtFight] [-WaitMinutes 480] [-MaxMinutes 240] [-LoadWaitSec 240] [-DryRun]
param([int]$OwnerPid = 0, [string]$Scenario = "", [string]$TrainingAsi = "", [int]$WatchSec = 30, [int]$WaitMinutes = 30, [int]$MaxMinutes = 60, [bool]$KeepBuild = $true, [string]$PerfAsi = "", [switch]$DryRun, [string]$RematchAsi = "", [string]$HoldFile = "", [switch]$IncludeAtFight, [string[]]$Rows = @("none"))
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
$tm = Join-Path (Split-Path -Parent $here) "testmatrix"
$repo = Split-Path -Parent (Split-Path -Parent $here)
$AddonDir = Join-Path $repo "mod\Nioh3BossRematch"
$GameDir = "C:\Program Files (x86)\Steam\steamapps\common\Nioh3"
$perfIni = "$GameDir\Nioh3EngineFix.ini"
$aAsi = "$GameDir\Nioh3BossRematch.asi"; $aIni = "$GameDir\Nioh3BossRematch.ini"; $aCat = "$GameDir\Nioh3BossRematch_bosses.tsv"; $aLog = "$GameDir\Nioh3BossRematch.log"
$srcAsi = if ($RematchAsi) { $RematchAsi } else { Join-Path $AddonDir "bin\Nioh3BossRematch.asi" }; $srcCat = Join-Path $AddonDir "Nioh3BossRematch_bosses.tsv"
$saveDir = "$env:LOCALAPPDATA\KoeiTecmo\NIOH3"
# VirtualPad needs the perf ASI >= 1.7.37: the test installs this one for its run and puts the user's back afterwards
if (-not $PerfAsi) { $PerfAsi = Join-Path $repo "mod\Nioh3EngineFix\bin\Nioh3EngineFix.asi" }
$stamp = Get-Date -f yyyyMMdd-HHmmss
$out = Join-Path $repo "captures\rematch\xworldprobe-$stamp"; New-Item -ItemType Directory -Force "$out\orig" | Out-Null
$csv = Join-Path $repo "captures\rematch\xworldprobe-$stamp.csv"
$saveBak = Join-Path $repo "captures\save-backups\$stamp-xworldprobe"
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
        foreach ($n in @("Nioh3EngineFix.ini", "Nioh3EngineFix.asi", "Nioh3BossRematch.ini", "Nioh3BossRematch.asi", "Nioh3BossRematch_bosses.tsv", "Nioh3Training.asi", "Nioh3Training.ini", "Nioh3Training_arenas.tsv")) {
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

# ---- rows: every R / H / D row with an arena point (+ the 'mission starts at the fight' rows with -IncludeAtFight) ----
$cat = Get-Content $srcCat | Where-Object { $_ -and -not $_.StartsWith("#") }
$hdr = $cat[0] -split "`t"; $ix = @{}; for ($i = 0; $i -lt $hdr.Count; $i++) { $ix[$hdr[$i]] = $i }
$all = foreach ($l in ($cat | Select-Object -Skip 1)) {
    $c = $l -split "`t"
    [pscustomobject]@{ id = $c[$ix.id]; name = $c[$ix.name]; route = $c[$ix.route]; arena = ($c[$ix.arena_x] -ne '-' -or ($c[$ix.gate_id] -ne '-' -and $c[$ix.gate_kind] -ne 'box')); atfight = ($c[$ix.arena_src] -like 'mission start*');
                       conf = $c[$ix.arena_conf]; src = "$($c[$ix.arena_src]) gate=$($c[$ix.gate_id])/$($c[$ix.gate_kind])" }
}
$Rows = @()
$sel = @()

Init-Guard -Name "xworld_probe" -OwnerPid $OwnerPid -MaxMinutes $MaxMinutes -MinHeadroomGB 12 -MinCurrentGB 8 -Cleanup { Close-Game; Restore; Stop-TestPopup }
"id,name,arena_conf,result,fnow,warps,load_s,seconds_to_fight,player_at_fight,note" | Set-Content $csv
$sum = [ordered]@{ rows = $sel.Count; pass = 0; fail = 0; skip = 0 }
$tCmd = "$GameDir\Nioh3Training.cmd"; $tLog = "$GameDir\Nioh3Training.log"
$ui = Join-Path $repo "tools\ui.fsx"
function ActShot([int]$sec, [string]$tag) {
    # pc2 #9702: screenshot on the first appearance of each boss action id (ACTLOG lines of the ActionLog asi) -> <out>cts\<tag>_<id>_a/b.jpg
    Add-Type -AssemblyName System.Drawing, System.Windows.Forms
    $dir = Join-Path $out "acts"; New-Item -ItemType Directory -Force $dir | Out-Null
    $seen = @{}; $pos = if (Test-Path $tLog) { (Get-Item $tLog).Length } else { 0 }; $t1 = Get-Date
    function Snap([string]$f) { try { $b = New-Object Drawing.Bitmap 1920, 1080; $g = [Drawing.Graphics]::FromImage($b); $g.CopyFromScreen(0, 0, 0, 0, $b.Size); $sm = New-Object Drawing.Bitmap $b, 960, 540; $sm.Save($f, [Drawing.Imaging.ImageFormat]::Jpeg); $sm.Dispose(); $g.Dispose(); $b.Dispose() } catch { T "  snap fail: $($_.Exception.Message)" } }
    while (((Get-Date) - $t1).TotalSeconds -lt $sec) {
        if (-not (Get-Process Nioh3 -ErrorAction SilentlyContinue)) { break }
        if ($global:HG) { Assert-Owner }
        $fs = [IO.File]::Open($tLog, 'Open', 'Read', 'ReadWrite'); $fs.Seek($pos, 'Begin') | Out-Null; $sr = New-Object IO.StreamReader($fs); $txt = $sr.ReadToEnd(); $pos = $fs.Position; $sr.Close()
        foreach ($m in [regex]::Matches($txt, 'ACTLOG boss \w+ chr (\d+): action (0x[0-9A-Fa-f]+)')) {
            $id = $m.Groups[2].Value; if ($seen.ContainsKey($id)) { continue }; $seen[$id] = 1
            Snap (Join-Path $dir "${tag}_${id}_a.jpg"); Start-Sleep -Milliseconds 500; Snap (Join-Path $dir "${tag}_${id}_b.jpg")
        }
        Start-Sleep -Milliseconds 100
    }
    T "  actshot: $($seen.Count) ids captured: $($seen.Keys -join ' ')"
}
function TC([string]$c) { [IO.File]::WriteAllText($tCmd, $c + "`r`n"); T "  training cmd: $c"; Start-Sleep 2 }
function Shot([string]$n) { try { $f = Join-Path $out "$n.png"; dotnet fsi $ui focus Nioh3 2>&1 | Out-Null; dotnet fsi $ui shot $f 0 0 1920 1080 2>&1 | Out-Null } catch {} }
try {
    T "xworld_probe: scenario $Scenario; out=$out"
    if (-not (Test-Path "$GameDir\Nioh3Training.asi")) { throw "Nioh3Training.asi not installed" }
    if (Get-Process Nioh3 -ErrorAction SilentlyContinue) { throw "Nioh3 is running" }
    $r = Lock "acquire"; if ($r.code -ne 0) { throw "lock: $($r.out)" }; $script:locked = $true; T "lock: $($r.out)"
    New-Item -ItemType Directory -Force $saveBak | Out-Null
    $o = robocopy $saveDir $saveBak /MIR /R:2 /W:2 /NFL /NDL /NP /NJH; T "save folder backed up to $saveBak (robocopy exit $LASTEXITCODE)"
    if ($LASTEXITCODE -ge 8) { throw "save backup failed" }
    $script:saved = $true
    foreach ($n in @("Nioh3EngineFix.ini", "Nioh3EngineFix.asi", "Nioh3BossRematch.ini", "Nioh3BossRematch.asi", "Nioh3BossRematch_bosses.tsv", "Nioh3Training.asi", "Nioh3Training.ini", "Nioh3Training_arenas.tsv")) {
        $p = Join-Path $GameDir $n; $script:had[$n] = Test-Path $p; if ($script:had[$n]) { Copy-Item $p "$out\orig\" -Force }
    }
    $aText = if (Test-Path $aIni) { [IO.File]::ReadAllText($aIni) } else { [IO.File]::ReadAllText((Join-Path $AddonDir "Nioh3BossRematch.ini")) }
    foreach ($kv in @(@("Rematch", "Enable", "1"), @("Remote", "Pipe", "1"), @("Menu", "Overlay", "0"), @("Debug", "BossLog", "1"))) { $aText = Set-IniKey $aText $kv[0] $kv[1] $kv[2] }
    $pText = Force-Acl (Force-Vpad ([IO.File]::ReadAllText($perfIni)))
    $script:installed = $true
    [IO.File]::WriteAllText($aIni, $aText, (New-Object Text.UTF8Encoding($false))); [IO.File]::WriteAllText($perfIni, $pText, (New-Object Text.UTF8Encoding($false)))
    Copy-Item $PerfAsi "$GameDir\Nioh3EngineFix.asi" -Force
    if ($TrainingAsi) { $td = Split-Path $TrainingAsi; foreach ($e in @("Nioh3Training.ini", "Nioh3Training_arenas.tsv")) { if (Test-Path "$td\$e") { Copy-Item "$td\$e" "$GameDir\$e" -Force } }; Copy-Item $TrainingAsi "$GameDir\Nioh3Training.asi" -Force; T "installed test Nioh3Training.asi from $TrainingAsi" }
    $logStart = if (Test-Path $tLog) { (Get-Item $tLog).Length } else { 0 }
    Start-TestPopup -Name "Boss spawn probe" -InputMode vpad
    $null = Launch
    TC "stage"; TC "pos"; Shot "00_field"
    foreach ($ln in (Get-Content $Scenario)) {
        $ln = $ln.Trim(); if (-not $ln -or $ln.StartsWith("#")) { continue }
        if (-not (Get-Process Nioh3 -ErrorAction SilentlyContinue)) { T "!! game gone before: $ln"; break }
        $op, $arg = $ln -split "\s+", 2
        T "== $ln"
        switch ($op) {
            "T" { TC $arg }
            "RM" { $q = RMP $arg; T "  rm: $q" }
            "S" { Start-Sleep ([int]$arg) }
            "SHOT" { Shot $arg }
            "ACTSHOT" { $a2 = $arg -split "\s+"; ActShot ([int]$a2[0]) $a2[1] }
            "VP" { $r = VP ($arg -split "\s+"); T "  vp: $($r.reply)" }
            "RMWAIT" { $x = Wait-RM { param($y) (F $y 'loading') -eq '0' -and (F $y 'player') -eq '1' } ([int]$arg) 2; T "  rm wait: $(if ($x) { $x } else { 'timeout' })" }
            "POLL" { $p3 = $arg -split "\s+", 3; $t1 = Get-Date; while (((Get-Date) - $t1).TotalSeconds -lt [int]$p3[0]) { if (-not (Get-Process Nioh3 -ErrorAction SilentlyContinue)) { break }; TC $p3[2]; Start-Sleep ([int]$p3[1]) } }
        }
    }
    if ($HoldFile) {
        Set-Content $HoldFile "delete this file (or close the game) to end the hold; saves are restored afterwards"
        T "HOLD: game left running for hand play; delete $HoldFile to finish"
        while ((Test-Path $HoldFile) -and (Get-Process Nioh3 -ErrorAction SilentlyContinue)) { Start-Sleep 10; if ($global:HG) { Assert-Owner } }
        T "hold ended"
    }
} catch {
    T "!! $($_.Exception.Message)"
} finally {
    if (Test-Path $tLog) { $fs = [IO.File]::Open($tLog, 'Open', 'Read', 'ReadWrite'); $fs.Seek($logStart, 'Begin') | Out-Null; $sr = New-Object IO.StreamReader($fs); [IO.File]::WriteAllText("$out\training.log", $sr.ReadToEnd()); $sr.Close() }
    Remove-Item $tCmd -Force -ErrorAction SilentlyContinue
    Close-Game
    Stop-TestPopup
    Restore
    Done-Guard
    T "done; training log slice: $out\training.log"
}
