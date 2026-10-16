# fightnow_test.ps1 - UNATTENDED self-test of Nioh3BossRematch "Fight now (in the arena)" on this PC (notes\boss-rematch-build.md).
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
#        [-AgentId pc-rematch -PcLock nioh3-pc] [-SaveCopy D:\nioh3-re\save-backups] [-AsiPath <other build>]
# -PcLock: also queue for and hold the PC's own game lock (other PC agents' harnesses use it); -AgentId: the bridge id to hold the
# locks as (a distinct id per agent, or the lock tool treats another 'pc' holder as us). -SaveCopy: a second copy of the save backup.
# CSV column 'spawn' (1.4.0): arena = the load ended with the player in the arena (start point override), start = at the start point
# (warp fallback), - = no override for that row.
param([int]$OwnerPid = 0, [string[]]$Rows = @(), [switch]$IncludeAtFight, [int]$WaitMinutes = 480, [int]$MaxMinutes = 240,
      [int]$LoadWaitSec = 240, [switch]$Retry, [int]$WalkSec = 0, [int]$RetryMaxSec = 600, [bool]$KeepBuild = $true, [string]$PerfAsi = "", [string[]]$PerfSet = @(), [string]$AddonAsi = "",
      [string[]]$TrainCmds = @(), [int]$TrainWaitSec = 45, [switch]$DryRun, [string]$AgentId = "pc",
      [string]$PcLock = "", [string]$SaveCopy = "", [string]$AsiPath = "")
if ($AsiPath -and -not $AddonAsi) { $AddonAsi = $AsiPath }   # -AsiPath (combo-3301b name) = -AddonAsi
# -AddonAsi <path>: test this add-on build instead of mod\Nioh3BossRematch\bin (use with -KeepBuild:$false to put the user's back)
# -TrainCmds "create 375545","scriptcreate sub_event_boss_battle_start","mission 1CBC 67064","wait 10": EXPERIMENTAL training pipe commands
#   ([Training] LiveCommands = 1 in the test ini) sent after the fight; each is timed via 'trainstatus' and screenshotted (notes\training-live-4341.md)
# -PerfSet "Section/Key=Value",... : extra keys for the TEST perf ini only (e.g. Profiler/LoadStats=1); the user's ini is restored after
$ErrorActionPreference = "Continue"
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
$tm = Join-Path (Split-Path -Parent $here) "testmatrix"
$repo = Split-Path -Parent (Split-Path -Parent $here)
$AddonDir = Join-Path $repo "mod\Nioh3BossRematch"
$GameDir = "C:\Program Files (x86)\Steam\steamapps\common\Nioh3"
$perfIni = "$GameDir\Nioh3EngineFix.ini"
$aAsi = "$GameDir\Nioh3BossRematch.asi"; $aIni = "$GameDir\Nioh3BossRematch.ini"; $aCat = "$GameDir\Nioh3BossRematch_bosses.tsv"; $aLog = "$GameDir\Nioh3BossRematch.log"
$srcAsi = if ($AddonAsi) { $AddonAsi } else { Join-Path $AddonDir "bin\Nioh3BossRematch.asi" }; $srcCat = Join-Path $AddonDir "Nioh3BossRematch_bosses.tsv"
$saveDir = "$env:LOCALAPPDATA\KoeiTecmo\NIOH3"
# VirtualPad needs the perf ASI >= 1.7.37: the test installs this one for its run and puts the user's back afterwards
if (-not $PerfAsi) { $PerfAsi = Join-Path $repo "mod\Nioh3EngineFix\bin\Nioh3EngineFix.asi" }
$stamp = Get-Date -f yyyyMMdd-HHmmss
$out = Join-Path $repo "captures\rematch\fightnow-$stamp"; New-Item -ItemType Directory -Force "$out\orig" | Out-Null
$csv = Join-Path $repo "captures\rematch\fightnow-$stamp.csv"
$saveBak = Join-Path $repo "captures\save-backups\$stamp-fightnow-test"
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
$env:UO_BRIDGE_TOKEN_FILE = "$env:APPDATA\uo-bridge\token"; $env:UO_AGENT_ID = $AgentId
function Lock([string]$a, [string]$key = "nioh3") { if ($env:FN_NOLOCK) { return @{ code = 0; out = "skipped (FN_NOLOCK: devices may run the game concurrently)" } }; Push-Location $uo; try { $o = (py -m uo_bridge.gamelock $a $key --ttl 14400 2>&1 | Out-String).Trim(); @{ code = $LASTEXITCODE; out = $o } } finally { Pop-Location } }
function Wait-PcLock {   # queue behind the other PC agents: enqueue, then acquire when it is our turn (a lock held by another id = wait)
    $q = Lock "enqueue" $PcLock; T "queued for '$PcLock' as $AgentId : $($q.out)"; $t0 = Get-Date
    while ($true) {
        $r = Lock "acquire" $PcLock
        if ($r.code -eq 0 -and -not (Get-Process Nioh3 -ErrorAction SilentlyContinue)) { $script:pcLocked = $true; T "lock '$PcLock': $($r.out)"; return }
        if ($r.code -eq 0) { $null = Lock "release" $PcLock }   # got it but a game is still open: give it back and wait
        if (((Get-Date) - $t0).TotalMinutes -gt $WaitMinutes) { $null = Lock "dequeue" $PcLock; throw "no turn on '$PcLock' in $WaitMinutes min ($($r.out))" }
        if ($OwnerPid -and -not (Get-Process -Id $OwnerPid -ErrorAction SilentlyContinue)) { $null = Lock "dequeue" $PcLock; throw "owner gone while queued" }
        Start-Sleep 30
    }
}
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


Add-Type -AssemblyName System.Windows.Forms, System.Drawing
function Shot([string]$name) {   # full primary screen (the game window) -> $out\shots\<name>.png
    try {
        $d = Join-Path $out "shots"; New-Item -ItemType Directory -Force $d | Out-Null
        $b = [Windows.Forms.Screen]::PrimaryScreen.Bounds; $bmp = New-Object Drawing.Bitmap $b.Width, $b.Height
        $g = [Drawing.Graphics]::FromImage($bmp); $g.CopyFromScreen($b.Location, [Drawing.Point]::Empty, $b.Size)
        $bmp.Save((Join-Path $d "$name.png"), [Drawing.Imaging.ImageFormat]::Png); $g.Dispose(); $bmp.Dispose(); T "  shot $name.png"
    } catch { T "  shot ${name} failed: $_" }
}
function Run-Train {   # EXPERIMENTAL training commands, one at a time: send, then poll trainstatus 2x/s; screenshots before / +3 s / end
    $i = 0
    foreach ($tc in @($TrainCmds | ForEach-Object { $_ -split ';' } | ForEach-Object { $_.Trim() } | Where-Object { $_ })) {
        $i++; $tag = "train{0:D2}_{1}" -f $i, (($tc -replace '[^A-Za-z0-9]+', '_').Trim('_'))
        if ($tc -match '^wait (\d+)$') { T "  [train $i] wait $($Matches[1]) s"; Start-Sleep ([int]$Matches[1]); Shot $tag; continue }
        Set-TestStatus "train $i/$($TrainCmds.Count): $tc"
        Shot "${tag}_0before"; $s0 = RMP status; T "  [train $i] before: $s0"
        $tq = Get-Date; $q = RMP $tc; T "  [train $i] '$tc' -> $q"
        if ($q -notmatch '^queued') { $script:trainRes += "$tc => $q"; continue }
        $shot3 = $false; $last = ""; $t0 = Get-Date
        while (((Get-Date) - $t0).TotalSeconds -lt $TrainWaitSec) {
            Start-Sleep -Milliseconds 500
            $ts = RMP trainstatus; if ($ts -and $ts -notmatch '^error') { $last = $ts }
            if (-not $shot3 -and ((Get-Date) - $tq).TotalSeconds -ge 3) { Shot "${tag}_1plus3s"; $shot3 = $true }
            if (-not (Get-Process -Id $script:gamePid -ErrorAction SilentlyContinue)) { T "  [train $i] !! game process gone"; break }
        }
        $s1 = RMP status; Shot "${tag}_2end"
        T "  [train $i] after $([math]::Round(((Get-Date) - $tq).TotalSeconds, 1)) s: $last"
        T "  [train $i] status: $s1"
        $script:trainRes += "$tc => $q | $last | $(LastOf $s1)"
        if (-not (Get-Process -Id $script:gamePid -ErrorAction SilentlyContinue)) { $script:trainRes += "GAME EXITED after '$tc'"; break }
        # let a load it started finish before the next command
        $null = Wait-RM { param($x) (F $x 'loading') -eq '0' -and (F $x 'player') -eq '1' } 180 1
    }
}
Add-Type -Namespace FnW -Name U -MemberDefinition '[DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr h,int c); [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);' -ErrorAction SilentlyContinue
function Fix-Focus {   # FN_FOCUS=1: minimize Steam windows, bring the game to the foreground (validity: fullscreen + focused)
    if (-not $env:FN_FOCUS) { return }
    foreach ($sp in @(Get-Process steam, steamwebhelper -ErrorAction SilentlyContinue | Where-Object MainWindowHandle -ne 0)) { [void][FnW.U]::ShowWindow($sp.MainWindowHandle, 6) }
    $gp = Get-Process Nioh3 -ErrorAction SilentlyContinue | Where-Object MainWindowHandle -ne 0 | Select-Object -First 1
    if ($gp) { [void][FnW.U]::ShowWindow($gp.MainWindowHandle, 9); [void][FnW.U]::SetForegroundWindow($gp.MainWindowHandle) }
}
$script:trainRes = @()
$script:gamePid = 0; $script:locked = $false; $script:pcLocked = $false; $script:installed = $false; $script:saved = $false; $script:had = @{}; $script:launchN = 0
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
    if (Test-Path "$GameDir\Nioh3EngineFix.log") { Copy-Item "$GameDir\Nioh3EngineFix.log" "$out\engine_launch$($script:launchN).log" -Force -ErrorAction SilentlyContinue }
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
    if ($script:pcLocked) { $r = Lock "release" $PcLock; $script:pcLocked = $false; T "lock '$PcLock' released: $($r.out)" }
    elseif ($PcLock -and -not $DryRun) { $null = Lock "dequeue" $PcLock }   # stopped while still in the line: leave it
}

# ---- rows: every R / H / D row with an arena point (+ the 'mission starts at the fight' rows with -IncludeAtFight) ----
$cat = Get-Content $srcCat | Where-Object { $_ -and -not $_.StartsWith("#") }
$hdr = $cat[0] -split "`t"; $ix = @{}; for ($i = 0; $i -lt $hdr.Count; $i++) { $ix[$hdr[$i]] = $i }
$all = foreach ($l in ($cat | Select-Object -Skip 1)) {
    $c = $l -split "`t"
    [pscustomobject]@{ id = $c[$ix.id]; name = $c[$ix.name]; route = $c[$ix.route]; arena = ($c[$ix.arena_x] -ne '-' -or ($c[$ix.gate_id] -ne '-' -and $c[$ix.gate_kind] -ne 'box')); atfight = ($c[$ix.arena_src] -like 'mission start*');
                       conf = $c[$ix.arena_conf]; src = "$($c[$ix.arena_src]) gate=$($c[$ix.gate_id])/$($c[$ix.gate_kind])" }
}
$Rows = @($Rows | ForEach-Object { $_ -split "," } | Where-Object { $_ })
$sel = @($all | Where-Object { $_.route -in @("R", "H", "D") -and ($_.arena -or ($IncludeAtFight -and $_.atfight)) -and ($Rows.Count -eq 0 -or $Rows -contains $_.id) })

Init-Guard -Name "fightnow_test" -OwnerPid $OwnerPid -MaxMinutes $MaxMinutes -Cleanup { Close-Game; Restore; Stop-TestPopup }
"id,name,arena_conf,result,fnow,warps,spawn,load_s,seconds_to_fight,player_at_fight,note" | Set-Content $csv
$sum = [ordered]@{ rows = $sel.Count; pass = 0; fail = 0; skip = 0 }
try {
    T "fightnow_test: $($sel.Count) rows: $(($sel | ForEach-Object id) -join ' '); out=$out csv=$csv"
    foreach ($f in @($srcAsi, $srcCat, $perfIni, $PerfAsi)) { if (-not (Test-Path $f)) { throw "missing: $f" } }
    if ($DryRun) { T "[dry] would: lock, back up $saveDir -> $saveBak, install $srcAsi, test inis, launch, run $($sel.Count) rows, close, restore"; return }
    if ($PcLock) { Wait-PcLock }
    if (Get-Process Nioh3 -ErrorAction SilentlyContinue) { throw "Nioh3 is running" }
    $r = Lock "acquire"; if ($r.code -ne 0) { throw "lock: $($r.out)" }; $script:locked = $true; T "lock: $($r.out)"
    New-Item -ItemType Directory -Force $saveBak | Out-Null
    $o = robocopy $saveDir $saveBak /MIR /R:2 /W:2 /NFL /NDL /NP /NJH; T "save folder backed up to $saveBak (robocopy exit $LASTEXITCODE)"
    if ($LASTEXITCODE -ge 8) { throw "save backup failed" }
    $script:saved = $true
    if ($SaveCopy) { $sc = Join-Path $SaveCopy "$stamp-fightnow-test"; $o = robocopy $saveDir $sc /MIR /R:2 /W:2 /NFL /NDL /NP /NJH; T "second save copy: $sc (robocopy exit $LASTEXITCODE)"; if ($LASTEXITCODE -ge 8) { throw "save copy failed" } }
    foreach ($n in @("Nioh3EngineFix.ini", "Nioh3EngineFix.asi", "Nioh3BossRematch.ini", "Nioh3BossRematch.asi", "Nioh3BossRematch_bosses.tsv")) {
        $p = Join-Path $GameDir $n; $script:had[$n] = Test-Path $p; if ($script:had[$n]) { Copy-Item $p "$out\orig\" -Force }
    }
    $aText = if (Test-Path $aIni) { [IO.File]::ReadAllText($aIni) } else { [IO.File]::ReadAllText((Join-Path $AddonDir "Nioh3BossRematch.ini")) }
    foreach ($kv in @(@("Rematch", "Enable", "1"), @("Rematch", "FightNow", "1"), @("Rematch", "FightNowWarp", "1"), @("Rematch", "ArenaCapture", "1"),
                      @("Rematch", "ShowAll", "1"), @("Remote", "Pipe", "1"), @("Menu", "Overlay", "0"), @("Debug", "BossLog", "1"), @("Debug", "RematchLog", "0"),
                      @("Training", "LiveCommands", $(if ($TrainCmds.Count) { "1" } else { "0" })))) {
        $aText = Set-IniKey $aText $kv[0] $kv[1] $kv[2]
    }
    $pText = Force-Acl (Force-Vpad ([IO.File]::ReadAllText($perfIni)))
    foreach ($ps in @($PerfSet | ForEach-Object { $_ -split "," } | Where-Object { $_ })) {
        if ($ps -match '^\s*([^/]+)/([^=]+)=(.*)$') { $pText = Set-IniKey $pText $Matches[1].Trim() $Matches[2].Trim() $Matches[3].Trim(); T "test perf ini: [$($Matches[1].Trim())] $($Matches[2].Trim()) = $($Matches[3].Trim())" }
        else { throw "bad -PerfSet entry: $ps" }
    }
    $script:installed = $true
    [IO.File]::WriteAllText($aIni, $aText, (New-Object Text.UTF8Encoding($false))); [IO.File]::WriteAllText($perfIni, $pText, (New-Object Text.UTF8Encoding($false)))
    Copy-Item $srcAsi $aAsi -Force; Copy-Item $srcCat $aCat -Force; Copy-Item $PerfAsi "$GameDir\Nioh3EngineFix.asi" -Force
    T "installed $srcAsi + catalogue, perf ASI $PerfAsi (VirtualPad; the user's comes back after); test inis: FightNow/FightNowWarp/ArenaCapture/Pipe/BossLog = 1, Overlay = 0; perf VirtualPad/KeepRunningUnfocused"
    Start-TestPopup -Name "Fight now self-test" -InputMode vpad
    $null = Launch
    $n = 0
    foreach ($row in $sel) {
        $n++; Set-TestStatus "row $n/$($sel.Count): $($row.id) $($row.name)"
        T "== $n/$($sel.Count) $($row.id) $($row.name) [arena $($row.conf): $($row.src)]"
        $res = [ordered]@{ id = $row.id; name = $row.name; arena_conf = $row.conf; result = ""; fnow = ""; warps = ""; spawn = ""; load = ""; seconds = ""; pos = ""; note = "" }
        # wait for the add-on to be idle and the player in the field
        $idle = Wait-RM { param($x) (F $x 'state') -eq '0' -and (F $x 'player') -eq '1' -and (F $x 'loading') -eq '0' -and (F $x 'gate') -eq 'ok' } 90
        if (-not $idle) { T "  not idle: $(RMP status) - relaunching"; Close-Game; $null = Launch }
        $g0 = [int](F (RMP "scan") 'gen'); $t0 = Get-Date
        do { $rr = RMP "row $($row.id)"; if ([int](F $rr 'gen') -gt $g0) { break }; Start-Sleep 1 } while (((Get-Date) - $t0).TotalSeconds -lt 10)
        T "  $rr"
        if ((F $rr 'status') -ne '2') { $res.result = "skip"; $res.note = "not cleared on this journey"; $sum.skip++ }
        else {
            Fix-Focus
            if ($env:FN_PRECMD) { [IO.File]::WriteAllText("$GameDir\Nioh3Training.cmd", "$($env:FN_PRECMD)`r`n"); T "  precmd (simulated menu open): $($env:FN_PRECMD)"; Start-Sleep $(if ($env:FN_PREWAIT) { [int]$env:FN_PREWAIT } else { 5 }) }
            $q = RMP "fight $($row.id)"; T "  fight: $q"
            if ($q -notmatch '^queued') { $res.result = "FAIL"; $res.note = "refused: $q"; $sum.fail++ }
            else {
                $tq = Get-Date
                if ($WalkSec -gt 0) { Start-Sleep 3; T "  walk forward $WalkSec s (no-load summon spacing test)"; $w = VP stick L 0 1 ($WalkSec * 1000); T "  walk: $($w.reply)" }
                $st = Wait-RM { param($x) (LastOf $x) -match '^(started|refused)' } $(if ($env:FN_SWAP) { 3 } else { 20 })
                $swapped = (Test-Path $aLog) -and ((Get-Content $aLog -Tail 40) -match "F $($row.id): Nioh3Training swapped the boss in place")
                if ((LastOf $st) -notmatch '^started' -and $swapped) {   # no-load swap: judge by Nioh3Training BOSSMON (boss-to-player distance)
                    $tlog = "$GameDir\Nioh3Training.log"; $tcmd = "$GameDir\Nioh3Training.cmd"
                    [IO.File]::WriteAllText($tcmd, "mon`r`n"); Start-Sleep 0
                    $w = VP stick L 0 1 3000; T "  swap: walked 3 s: $($w.reply)"; [IO.File]::WriteAllText($tcmd, "mon`r`n"); Start-Sleep 1; for ($k = 0; $k -lt 24; $k++) { $null = VP press RB 120; Start-Sleep -Milliseconds 600 }; T "  swap: 24 RB presses"; Start-Sleep 6; Shot "swap_$($row.id)"
                    $bm = @(Get-Content $tlog -ErrorAction SilentlyContinue | Where-Object { $_ -match 'BOSSMON \+' } | Select-Object -Last 40)
                    foreach ($l in (($bm | Select-Object -First 3) + ($bm | Select-Object -Last 3))) { T "  $l" }
                    $ds = @($bm | ForEach-Object { if ($_ -match 'dist ([\d.]+) m') { [double]$Matches[1] } })
                    $mn = if ($ds.Count) { ($ds | Measure-Object -Minimum).Minimum } else { -1 }
                    $res.note = "swap BOSSMON rows=$($ds.Count) first=$($ds[0]) last=$($ds[-1]) min=$mn"
                    if ($ds.Count -and ($mn -lt 3.5 -or $ds[-1] -lt $ds[0] - 1)) { $res.result = "PASS"; $sum.pass++ } else { $res.result = "FAIL"; $sum.fail++ }
                    T "  -> $($res.result) $($res.note)"
                    if ($env:FN_OWORLD) {   # Nioh3Training.cmd file commands ';'-separated (e.g. cross-world oworld, then train <R-id>), timed by the Training log
                        $n0 = @(Get-Content $tlog -ErrorAction SilentlyContinue).Count; $sh = 0
                        foreach ($oc in @($env:FN_OWORLD -split ';' | ForEach-Object { $_.Trim() } | Where-Object { $_ })) {
                            $tc0 = Get-Date; [IO.File]::WriteAllText($tcmd, "$oc`r`n"); T "  tcmd: wrote '$oc'"
                            while (((Get-Date) - $tc0).TotalSeconds -lt $(if ($env:FN_WAIT) { [int]$env:FN_WAIT } else { 40 })) { Start-Sleep 5; $sh++; Fix-Focus; if ($sh % 3 -eq 1) { Shot ("tcmd_{0:D2}" -f $sh) } }
                        }
                        $nl = @(Get-Content $tlog -ErrorAction SilentlyContinue | Select-Object -Skip $n0)
                        foreach ($l in (@($nl | Where-Object { $_ -notmatch 'BOSSMON|PHASE mission' } | Select-Object -First 50) + @($nl | Where-Object { $_ -match 'BOSSMON' } | Select-Object -Last 12))) { T "  TLOG $l" }
                        T "  tcmd status: $(RMP status)"
                    }
                    Start-Sleep 3; $x = RMP "exit"; T "  exit: $x"; Start-Sleep 20
                }
                elseif ((LastOf $st) -notmatch '^started') { $res.result = "FAIL"; $res.note = "game thread: $(LastOf $st)"; $sum.fail++ }
                else {
                    $end = Wait-RM { param($x) (F $x 'fnow') -in @('pass', 'fail') -or ((F $x 'state') -eq '0' -and (F $x 'fnow') -ne 'armed' -and (F $x 'fnow') -ne 'warped') } ($LoadWaitSec + 60) 1
                    $res.fnow = F $end 'fnow'; $res.warps = F $end 'warps'; $res.spawn = F $end 'spawn'; $res.pos = F $end 'pos'; $lm = F $end 'loadms'; if ($lm) { $res.load = [math]::Round([int]$lm / 1000.0, 1) }
                    $res.seconds = [math]::Round(((Get-Date) - $tq).TotalSeconds, 1)
                    if ($res.fnow -eq 'pass') { $res.result = "PASS"; $sum.pass++ }
                    else { $res.result = "FAIL"; $res.note = if ($end) { LastOf $end } else { "no result in $($LoadWaitSec + 60) s: $(RMP status)" }; $sum.fail++ }
                    T "  -> $($res.result) fnow=$($res.fnow) warps=$($res.warps) spawn=$($res.spawn) load=$($res.load) s pos=$($res.pos) after $($res.seconds) s; $(LastOf $end)"
                    if ($Retry -and $res.fnow -eq 'pass') {   # stand idle until the boss kills the player twice; time every load (retry timing)
                        T "  retry mode: idle until 2 deaths (max $RetryMaxSec s); polling status 2x/s"
                        $tr = Get-Date; $prevL = '0'; $loads = 0; $tl = $null
                        while ($loads -lt 2 -and ((Get-Date) - $tr).TotalSeconds -lt $RetryMaxSec) {
                            $sx = RMP status; $l = F $sx 'loading'; $now = [math]::Round(((Get-Date) - $tr).TotalSeconds, 1)
                            if ($l -eq '1' -and $prevL -ne '1') { $tl = Get-Date; T "  RETRY load start at +$now s: $(LastOf $sx)" }
                            if ($l -ne '1' -and $prevL -eq '1') { $loads++; T "  RETRY #$loads load end at +$now s (load $([math]::Round(((Get-Date) - $tl).TotalSeconds, 1)) s) player=$(F $sx 'player') fnow=$(F $sx 'fnow')" }
                            $prevL = $l; Start-Sleep -Milliseconds 500
                        }
                        T "  retry mode done: $loads loads observed in $([math]::Round(((Get-Date) - $tr).TotalSeconds, 1)) s"
                    }
                    if ($TrainCmds.Count) { Run-Train; T "  train results:"; foreach ($tr in $script:trainRes) { T "    $tr" } }
                    Start-Sleep 4
                    $x = RMP "exit"; T "  exit: $x"
                    $back = Wait-RM { param($y) (F $y 'state') -eq '0' -and (F $y 'loading') -eq '0' -and (F $y 'player') -eq '1' } 150 2
                    if (-not $back) {
                        T "  did not leave the mission (status $(RMP status)) - retrying exit once, then a relaunch"
                        $null = RMP "exit"; $back = Wait-RM { param($y) (F $y 'state') -eq '0' -and (F $y 'loading') -eq '0' } 120 2
                        if (-not $back) { Close-Game; $null = Launch }
                    }
                }
            }
        }
        '"{0}","{1}","{2}","{3}","{4}","{5}","{6}","{7}","{8}","{9}","{10}"' -f $res.id, $res.name, $res.arena_conf, $res.result, $res.fnow, $res.warps, $res.spawn, $res.load, $res.seconds, $res.pos, ($res.note -replace '"', "'") | Add-Content $csv
    }
} catch {
    T "!! $($_.Exception.Message)"; $sum.error = $_.Exception.Message
} finally {
    Close-Game
    Stop-TestPopup
    Restore
    Done-Guard
    T "summary: $($sum | ConvertTo-Json -Compress)"
    T "csv: $csv"
}
