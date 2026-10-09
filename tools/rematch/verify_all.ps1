# verify_all.ps1 - UNATTENDED per-boss verifier for the Nioh3BossRematch add-on (notes\boss-rematch-addon.md s.6).
# PC game turn only; needs the game lock (uo_bridge gamelock) - never run while someone else holds the game.
# Input only through VirtualPad (the perf ASI's pipe \\.\pipe\nioh3vpad: no OS input, the game never needs the foreground) and the
# add-on's own pipe \\.\pipe\nioh3bossrematch. It never needs a win: per catalogue row it
#   scan -> row status (R/H/D: must be cleared on this journey, else "skip") -> `start <id>` (R-b direct / E1 redirect) ->
#   wait for the mission to load (add-on status last="in mission") -> wait up to -FightWaitSec for a BossBattleMode start whose
#   placements (Character::Create in the 60 s before it) contain the row's expected placement id (the HP bar) -> quit (-QuitScript
#   vpad file to the title + Continue, or close + relaunch) -> the add-on must report the row exited / restored ->
#   append the result to results.tsv and flip verified=1 in the add-on's Nioh3BossRematch_verified.tsv (repo + game folder).
# A row whose fight needs a player to walk to the boss (story missions) ends "loaded, fight not reached": verified stays 0 unless
# -AcceptStart (R rows only: the game's own replay of a cleared mission that loaded is accepted as a start check).
# Guard (harness_guard.ps1): owner pid checked before every command and while waiting, -MaxMinutes, commit headroom before each
# launch, max 2 crashes, pid file; quiet: no OS input; restore of every file it touched (ini x2, asi, catalogue, verified) in finally.
#
#   powershell -NoProfile -File tools\rematch\verify_all.ps1 -OwnerPid <pid> [-Rows R0034,E07] [-Routes R,H,D,E] [-SkipVerified]
#        [-FightWaitSec 90] [-LoadWaitSec 240] [-AcceptStart] [-QuitScript cmds.txt] [-MaxMinutes 360] [-DryRun]
# Needs: the add-on built (mod\Nioh3BossRematch\bin), the perf ASI installed (VirtualPad, KeepRunningUnfocused), a save standing in
# the field / at a shrine, the bosses beaten on that journey, a Picture Scroll in the inventory for E rows.
param([int]$OwnerPid = 0, [int]$MaxMinutes = 360, [string[]]$Rows = @(), [string[]]$Routes = @("R", "H", "D", "E"), [switch]$SkipVerified,
      [int]$FightWaitSec = 90, [int]$LoadWaitSec = 240, [switch]$AcceptStart, [string]$QuitScript = "", [switch]$DryRun,
      [string]$AddonDir = "")
$ErrorActionPreference = "Continue"
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
$tm = Join-Path (Split-Path -Parent $here) "testmatrix"
. "$tm\harness_guard.ps1"
. "$tm\vpad.ps1"
$repo = Split-Path -Parent (Split-Path -Parent $here)
if (-not $AddonDir) { $AddonDir = Join-Path $repo "mod\Nioh3BossRematch" }
$GameDir = "C:\Program Files (x86)\Steam\steamapps\common\Nioh3"
$perfIni = "$GameDir\Nioh3EngineFix.ini"
$aAsi = "$GameDir\Nioh3BossRematch.asi"; $aIni = "$GameDir\Nioh3BossRematch.ini"; $aCat = "$GameDir\Nioh3BossRematch_bosses.tsv"
$aVer = "$GameDir\Nioh3BossRematch_verified.tsv"; $aLog = "$GameDir\Nioh3BossRematch.log"
$srcAsi = Join-Path $AddonDir "bin\Nioh3BossRematch.asi"; $srcCat = Join-Path $AddonDir "Nioh3BossRematch_bosses.tsv"; $srcIni = Join-Path $AddonDir "Nioh3BossRematch.ini"
$repoVer = Join-Path $AddonDir "Nioh3BossRematch_verified.tsv"
$stamp = Get-Date -f yyyyMMdd_HHmmss
$out = if ($DryRun) { Join-Path $env:TEMP "rematch_verify_dry_$stamp" } else { "C:\Users\Ahmad Hamza\nioh3-engine\captures\rematch\verify_$stamp" }
New-Item -ItemType Directory -Force "$out\orig" | Out-Null
$trace = "$out\trace.txt"; $resTsv = "$out\results.tsv"
function T([string]$s) { $l = "[{0}] {1}" -f (Get-Date -f HH:mm:ss), $s; [IO.File]::AppendAllText($trace, $l + "`r`n"); Write-Host $l }
$uo = "C:\Users\Ahmad Hamza\universal-optimizer\agent-bridge"
$env:UO_BRIDGE_TOKEN_FILE = "$env:APPDATA\uo-bridge\token"; $env:UO_AGENT_ID = "pc"
function Lock([string]$a) { Push-Location $uo; try { $o = (py -m uo_bridge.gamelock $a nioh3 --ttl 3600 2>&1 | Out-String).Trim(); @{ code = $LASTEXITCODE; out = $o } } finally { Pop-Location } }

# ---- ini helper (section-aware) ----
function Set-IniKey([string]$t, [string]$sec, [string]$key, [string]$val) {
    $nl = "`r`n"
    $m = [regex]::Match($t, "(?ms)^\[$([regex]::Escape($sec))\][^\r\n]*\r?\n(.*?)(?=^\[|\z)")
    if (-not $m.Success) { return $t.TrimEnd() + "$nl$nl[$sec]$nl$key = $val$nl" }
    $body = $m.Groups[1].Value
    $re = "(?m)^$([regex]::Escape($key))[ \t]*=.*?(\r?)$"
    if ([regex]::IsMatch($body, $re)) { $nb = [regex]::Replace($body, $re, "$key = $val`$1", 1) } else { $nb = "$key = $val$nl" + $body }
    return $t.Substring(0, $m.Groups[1].Index) + $nb + $t.Substring($m.Groups[1].Index + $m.Groups[1].Length)
}

# ---- the add-on pipe ----
function RMP([string]$cmd, [int]$timeoutMs = 5000) {
    if ($global:HG) { Assert-Owner }
    if ($DryRun) {
        $r = switch -regex ($cmd) {
            '^ping' { "pong Nioh3BossRematch (dry) live=1" }
            '^scan' { "ok gen=1" }
            '^row (\S+)' { "id=$($Matches[1]) route=? status=2 gen=2 (dry)" }
            '^start (\S+)' { "queued $($Matches[1]) kind=2 (dry)" }
            '^status' { "state=0 row=- fights=1 infight=0 emaki=0 loading=0 player=1 stage=1 gate=ok placements=[ 0(-1.0s)] last=exited (dry)" }
            default { "ok (dry)" }
        }
        T "  [dry] rm $cmd -> $r"; return $r
    }
    $p = New-Object IO.Pipes.NamedPipeClientStream(".", "nioh3bossrematch", [IO.Pipes.PipeDirection]::InOut)
    try {
        $p.Connect($timeoutMs)
        $w = New-Object IO.StreamWriter($p); $w.AutoFlush = $true; $rd = New-Object IO.StreamReader($p)
        $w.WriteLine($cmd)
        $t = $rd.ReadLineAsync(); if (-not $t.Wait($timeoutMs)) { return "error timeout" }
        return $t.Result
    } catch { return "error $($_.Exception.Message)" } finally { $p.Dispose() }
}
function RM-Field([string]$reply, [string]$key) { if ($reply -match "(?:^| )$key=(\S+)") { return $Matches[1] }; return "" }
function RM-Last([string]$reply) { if ($reply -match ' last=(.*)$') { return $Matches[1] }; return "" }
function RM-Placements([string]$reply) { if ($reply -match 'placements=\[([^\]]*)\]') { return @([regex]::Matches($Matches[1], '(\d+)\(') | ForEach-Object { [uint32]$_.Groups[1].Value }) }; return @() }
function Wait-RM([scriptblock]$cond, [int]$sec, [int]$poll = 2) {
    if ($DryRun) {  # simulated replies: the first that satisfies the condition (a dry run walks the success path)
        foreach ($c in @("state=2 row=x fights=1 infight=0 placements=[] last=started x (dry)", "state=2 row=x fights=1 infight=0 placements=[] last=in mission x (dry)",
                         "state=2 row=x fights=9 infight=1 placements=[ $($script:dryExpect)(-2.4s)] last=in mission x (dry)", "state=0 row=- fights=9 infight=0 placements=[] last=exited x (dry)")) {
            if (& $cond $c) { T "  [dry] status -> $c"; return $c }
        }
        return $null
    }
    $t0 = Get-Date
    do { $s = RMP status; if (& $cond $s) { return $s }; Start-Sleep $poll } while (((Get-Date) - $t0).TotalSeconds -lt $sec)
    return $null
}

# ---- game helpers ----
$script:gamePid = 0; $script:locked = $false; $script:installed = $false; $script:launchN = 0; $script:had = @{}
function Launch {
    if ($DryRun) { $script:launchN++; T "[dry] launch #$($script:launchN): Assert-Launch ($(Get-CommitInfo)), steam://rungameid/3681010, focus back, vpad + add-on pipes, title -> Continue"; return $true }
    Assert-Launch
    $before = VP-ForegroundInfo
    Start-Process "steam://rungameid/3681010"; $t0 = Get-Date; $p = $null
    while (((Get-Date) - $t0).TotalSeconds -lt 180) { $p = Get-Process Nioh3 -ErrorAction SilentlyContinue | Where-Object MainWindowHandle -ne 0 | Select-Object -First 1; if ($p) { break }; Start-Sleep 3 }
    if (-not $p) { Note-Crash "no window"; throw "no game window after 180 s" }
    $script:gamePid = $p.Id; $script:launchN++; Register-Game; T "launch #$($script:launchN): game pid $($p.Id)"
    Start-Sleep 3; VP-GiveBackFocus $before; Start-Sleep 1
    if (-not (VP-WaitPipe 180)) { throw "no VirtualPad pipe (perf ASI with VirtualPad = 1?)" }
    $ok = VP-TitleContinue 300 "$out\title_continue_$($script:launchN).txt"
    $pg = RMP ping; T "title -> Continue -> field: $ok; add-on: $pg"
    if ($pg -notmatch 'live=1') { throw "add-on not live: $pg (see Nioh3BossRematch.log)" }
    return $ok
}
function Close-Game {
    if ($DryRun) { T "[dry] close the game"; return }
    $p = if ($script:gamePid) { Get-Process -Id $script:gamePid -ErrorAction SilentlyContinue } else { $null }
    if ($p) { Note-Closed; $null = $p.CloseMainWindow(); $p.WaitForExit(30000) | Out-Null; if (-not $p.HasExited) { T "!! game did not exit in 30 s - killing"; Stop-Process -Id $p.Id -Force }; Start-Sleep 5 }
    $script:gamePid = 0
    if (Test-Path $aLog) { Copy-Item $aLog "$out\addon_launch$($script:launchN).log" -Force -ErrorAction SilentlyContinue }
}
# leave the mission: QuitScript to the title + Continue, else close + relaunch; then the add-on must be idle again
function Leave-Mission {
    if ($QuitScript -and -not $DryRun) {
        $o = (py -I (Join-Path $tm "vpad.py") --owner-pid $global:HG.ownerPid run $QuitScript 2>&1 | Out-String); T "  quit script: $($o.Trim())"
        $t0 = Get-Date; $title = $false
        while (((Get-Date) - $t0).TotalSeconds -lt 90) { if ((VP-State) -match 'player=0') { $title = $true; break }; Start-Sleep 2 }
        if ($title -and (VP-TitleContinue 300 "$out\title_continue_q$($script:launchN).txt")) {
            $s = Wait-RM { param($x) (RM-Field $x 'state') -eq '0' } 60
            return [pscustomobject]@{ how = "quit script"; idle = [bool]$s; last = (RM-Last $s) }
        }
        T "  quit script did not reach the title - closing the game"
    }
    Close-Game
    $null = Launch
    return [pscustomobject]@{ how = "relaunch"; idle = $true; last = "process restart (table is boot data: nothing persists)" }
}
function Restore {
    if ($DryRun) { return }
    if ($script:installed) {
        foreach ($n in @("Nioh3EngineFix.ini", "Nioh3BossRematch.asi", "Nioh3BossRematch.ini", "Nioh3BossRematch_bosses.tsv")) {
            $dst = Join-Path $GameDir $n
            if ($script:had[$n]) { Copy-Item "$out\orig\$n" $dst -Force } else { Remove-Item $dst -Force -ErrorAction SilentlyContinue }
        }
        $script:installed = $false; T "game folder restored (inis, add-on asi, catalogue); verified file kept: $aVer"
    }
    if ($script:locked) { $r = Lock "release"; $script:locked = $false; T "lock released: $($r.out)" }
}
function Load-Verified([string]$f) { $h = @{}; if (Test-Path $f) { foreach ($l in Get-Content $f) { $c = $l -split "`t"; if ($c.Count -ge 2 -and $c[0] -ne "id") { $h[$c[0]] = $l } } }; return $h }
function Save-Verified([hashtable]$h, [string]$f) {
    $lines = @("id`tverified`tdate`troute`tstart`tload`tfight`texpected`tplacements`tnote") + ($h.Keys | Sort-Object | ForEach-Object { $h[$_] })
    [IO.File]::WriteAllLines($f, $lines, (New-Object Text.UTF8Encoding($false)))
}

# ---- rows ----
$cat = Get-Content $srcCat | Where-Object { $_ -and -not $_.StartsWith("#") }
$hdr = $cat[0] -split "`t"; $ix = @{}; for ($i = 0; $i -lt $hdr.Count; $i++) { $ix[$hdr[$i]] = $i }
$all = foreach ($l in ($cat | Select-Object -Skip 1)) { $c = $l -split "`t"; [pscustomobject]@{ id = $c[$ix.id]; name = $c[$ix.name]; route = $c[$ix.route]; placement = $(if ($c[$ix.route] -eq 'E0') { '-' } else { $c[$ix.placement_id] }); verified = $c[$ix.verified] } }
$Rows = @($Rows | ForEach-Object { $_ -split "," } | Where-Object { $_ }); $Routes = @($Routes | ForEach-Object { $_ -split "," } | Where-Object { $_ })
$routeMap = @{ "R" = "R"; "H" = "H"; "D" = "D"; "E" = "E0" }
$want = @($Routes | ForEach-Object { $routeMap[$_] })
$sel = @($all | Where-Object { $_.route -ne "X" -and $want -contains $_.route -and ($Rows.Count -eq 0 -or $Rows -contains $_.id) })
$ver = Load-Verified $repoVer
if ($SkipVerified) { $sel = @($sel | Where-Object { -not ($ver[$_.id] -and ($ver[$_.id] -split "`t")[1] -eq "1") -and $_.verified -ne "1" }) }

Init-Guard -Name "rematch_verify" -OwnerPid $OwnerPid -MaxMinutes $MaxMinutes -Cleanup { Close-Game; Restore; Stop-TestPopup }
$R = [ordered]@{ dry = [bool]$DryRun; out = $out; rows = $sel.Count; pass = 0; loaded = 0; fail = 0; skip = 0 }
try {
    $est = [math]::Round($sel.Count * (75 + [math]::Min($FightWaitSec, 90) + 45) / 60.0)
    T "verify_all: $($sel.Count) rows ($(($sel | Group-Object route | ForEach-Object { "$($_.Name) $($_.Count)" }) -join ', ')), fight wait $FightWaitSec s, accept-start $([bool]$AcceptStart), estimated ~$est min; out=$out"
    foreach ($f in @($srcAsi, $srcCat, $srcIni, (Join-Path $tm "vpad.py"), $perfIni)) { if (-not (Test-Path $f)) { if ($DryRun) { T "  [dry] missing (would abort): $f" } else { throw "missing: $f" } } }
    if ($QuitScript -and -not (Test-Path $QuitScript)) { throw "missing -QuitScript $QuitScript" }
    # test inis
    $aText = Get-Content $srcIni -Raw
    foreach ($kv in @(@("Rematch", "Enable", "1"), @("Rematch", "DirectStart", "1"), @("Rematch", "HiddenDuels", "0"), @("Rematch", "ShowUnverified", "1"),
                      @("Remote", "Pipe", "1"), @("Menu", "Overlay", "0"), @("Debug", "BossLog", "1"), @("Debug", "RematchLog", "1"))) { $aText = Set-IniKey $aText $kv[0] $kv[1] $kv[2] }
    [IO.File]::WriteAllText("$out\test_addon.ini", $aText, (New-Object Text.UTF8Encoding($false)))
    $pText = if (Test-Path $perfIni) { [IO.File]::ReadAllText($perfIni) } else { "" }
    $pText = Force-Acl (Force-Vpad $pText)
    [IO.File]::WriteAllText("$out\test_perf.ini", $pText, (New-Object Text.UTF8Encoding($false)))
    T "test inis: add-on Enable/DirectStart/ShowUnverified/Pipe/BossLog/RematchLog = 1, Overlay = 0; perf VirtualPad/KeepRunningUnfocused/AutoContinueLoad = 1"
    if ($DryRun) { T "[dry] no lock, no backup, no install, no game" }
    else {
        if (Get-Process Nioh3 -ErrorAction SilentlyContinue) { throw "Nioh3 is running" }
        $r = Lock "acquire"; if ($r.code -ne 0) { throw "lock: $($r.out)" }; $script:locked = $true; T "lock: $($r.out)"
        foreach ($n in @("Nioh3EngineFix.ini", "Nioh3BossRematch.asi", "Nioh3BossRematch.ini", "Nioh3BossRematch_bosses.tsv")) {
            $p = Join-Path $GameDir $n; $script:had[$n] = Test-Path $p; if ($script:had[$n]) { Copy-Item $p "$out\orig\" -Force }
        }
        $sb = "D:\nioh3-re\save-backups\$stamp-rematchverify"; New-Item -ItemType Directory -Force $sb | Out-Null
        Copy-Item "$env:LOCALAPPDATA\KoeiTecmo\NIOH3\*" $sb -Recurse -Force; T "save folder copied to $sb"
        $script:installed = $true
        [IO.File]::WriteAllText($aIni, $aText, (New-Object Text.UTF8Encoding($false))); [IO.File]::WriteAllText($perfIni, $pText, (New-Object Text.UTF8Encoding($false)))
        Copy-Item $srcAsi $aAsi -Force; Copy-Item $srcCat $aCat -Force
        if (Test-Path $repoVer) { Copy-Item $repoVer $aVer -Force }
        Start-TestPopup -Name "Boss rematch verify_all" -InputMode vpad
    }
    "id`troute`tresult`tstart`tload`tfight`texpected`tplacements`tleave`tnote" | Set-Content $resTsv
    if (-not (Launch)) { throw "not in the field" }
    $n = 0
    foreach ($row in $sel) {
        $n++; Set-TestStatus "row $n/$($sel.Count): $($row.id) $($row.name)"
        T "== $n/$($sel.Count) $($row.id) [$($row.route)] $($row.name) (expected placement $($row.placement))"
        $script:dryExpect = if ($row.placement -match '^\d+$') { $row.placement } else { "0" }
        $res = [ordered]@{ id = $row.id; route = $row.route; result = ""; start = 0; load = 0; fight = 0; expected = $row.placement; placements = ""; leave = ""; note = "" }
        # status on this journey
        if ($row.route -ne "E0") {
            $g0 = [int](RM-Field (RMP "scan") 'gen')
            $t0 = Get-Date
            do { $rr = RMP "row $($row.id)"; if ($DryRun -or [int](RM-Field $rr 'gen') -gt $g0) { break }; Start-Sleep 1 } while (((Get-Date) - $t0).TotalSeconds -lt 10)
            T "  $rr"
            if ((RM-Field $rr 'status') -ne '2') { $res.result = "skip"; $res.note = "not cleared on this journey (status $(RM-Field $rr 'status'))"; $R.skip++ }
        }
        if (-not $res.result) {
            $s0 = RMP status; $f0 = [int](RM-Field $s0 'fights')
            $q = RMP "start $($row.id)"; T "  start: $q"
            if ($q -notmatch '^queued') { $res.result = "fail"; $res.note = "start refused: $q"; $R.fail++ }
            else {
                $st = Wait-RM { param($x) (RM-Last $x) -match '^(started|refused)' } 15
                $last = RM-Last $st; T "  request: $last"
                if ($last -notmatch '^started') { $res.result = "fail"; $res.note = "game thread: $last"; $R.fail++ }
                else {
                    $res.start = 1
                    $in = Wait-RM { param($x) (RM-Last $x) -match '^in mission' } $LoadWaitSec
                    if (-not $in) { $res.result = "fail"; $res.note = "no mission load in $LoadWaitSec s"; $R.fail++ }
                    else {
                        $res.load = 1; T "  loaded: $(RM-Last $in)"
                        $fs = Wait-RM { param($x) [int](RM-Field $x 'fights') -gt $f0 } $FightWaitSec
                        if ($fs) {
                            $res.fight = 1; $pl = RM-Placements $fs; $res.placements = ($pl -join " ")
                            $ok = (-not $row.placement -or $row.placement -eq "-" -or ($pl -contains [uint32]$row.placement))
                            $res.result = if ($ok) { "pass" } else { "fight, other placement" }
                            if ($ok) { $R.pass++ } else { $R.fail++ }
                            T "  BossBattleMode: placements [$($res.placements)] expected $($row.placement) -> $($res.result)"
                        } else {
                            $res.result = "loaded, fight not reached"; $R.loaded++
                            if ($AcceptStart -and $row.route -eq "R") { $res.note = "accepted as a start check (-AcceptStart)" }
                        }
                    }
                }
            }
            if ($res.start) { $lv = Leave-Mission; $res.leave = "$($lv.how) idle=$($lv.idle) $($lv.last)"; T "  leave: $($res.leave)" }
        }
        ($res.Values | ForEach-Object { "$_" }) -join "`t" | Add-Content $resTsv
        $isOk = $res.result -eq "pass" -or ($AcceptStart -and $row.route -eq "R" -and $res.load -eq 1)
        if ($res.result -ne "skip") {
            $ver[$row.id] = "{0}`t{1}`t{2}`t{3}`t{4}`t{5}`t{6}`t{7}`t{8}`t{9}" -f $row.id, $(if ($isOk) { 1 } else { 0 }), (Get-Date -f yyyy-MM-dd), $row.route, $res.start, $res.load, $res.fight, $res.expected, $res.placements, ($res.result + " " + $res.note).Trim()
            $vf = if ($DryRun) { "$out\Nioh3BossRematch_verified.tsv" } else { $repoVer }
            Save-Verified $ver $vf
            if (-not $DryRun) { Copy-Item $vf $aVer -Force }
        }
    }
} catch {
    T "!! $($_.Exception.Message)"; $R.error = $_.Exception.Message
} finally {
    Close-Game
    Stop-TestPopup
    Restore
    Done-Guard
    $R | ConvertTo-Json -Depth 4 | Set-Content "$out\result.json"
    T "result: $($R | ConvertTo-Json -Compress -Depth 4)"
    T "out: $out (results.tsv; verified file: $(if ($DryRun) { "$out\Nioh3BossRematch_verified.tsv" } else { $repoVer }))"
}
