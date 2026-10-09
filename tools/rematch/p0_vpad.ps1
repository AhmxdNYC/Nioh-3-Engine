# SUPERSEDED (2026-10-08, Nioh3EngineFix 1.7.41): BossLog / RematchLog / the rematch moved to the add-on Nioh3BossRematch.asi, so
# this script's perf-ini keys no longer log anything. Use tools/rematch/verify_all.ps1 (add-on pipe) instead. Kept for reference.
# p0_vpad.ps1 - UNATTENDED Phase 0 log capture for the boss rematch (notes\boss-rematch-build.md s.6, design s.8 P0-5 / P0-1).
# PC game turn only. Drives Nioh 3 ONLY through the mod's VirtualPad pipe (no SendInput, no clicks, game never needs the
# foreground). Pure log capture: it opens menus and starts ONE cleared mission replay, waits for its stage, then leaves; the
# fights themselves are the user's boss session.
#
#   lock -> backup ini / asi (+ save folder copy) -> test ini ([Debug] BossLog = RematchLog = VirtualPad = KeepRunningUnfocused = 1,
#   AutoContinueLoad = 1, [Menu] Enabled = 1, [Rematch] Enable = 1 for the R-a fallback) + catalogue -> launch, focus handed back
#   -> title -> Continue -> field
#   A  (P0-5)  interact (B, then Y) while standing at the shrine -> game menu open? -> Down x ShrineDown -> A -> replay-menu lines
#              (`RematchLog L7: MissionReplayMenu_Setup / replay menu open / MissionSelectListBuild / BattleScrollTop_Select`);
#              one more A when the Battle Scroll top menu sits in between. Not reached -> back out, then the mod's own R-a
#              (`menu set Rematch 1` = MenuOpenMissionReplay(0, 0, 6), the same call the game's script makes) unless -NoModFallback.
#   B  (L7)    Down x ListDown, then A up to StartPresses times until `RematchLog L7: MissionRequestById / MissionRequestEnqueue`
#              appears (the replay start = Q10, what R-b needs).
#   C          wait for the stage (`RematchLog L4: IN MISSION` + field=1 for 5 s), 20 s of log, then quit to title
#              (-QuitScript vpad command file, verified by player=0) or close the game normally (default; no kill unless it hangs).
#   D  (P0-1, only with -PictureScroll)  relaunch / Continue, then -ScrollScript (vpad command file, e.g. the item menu path) or,
#              when step A saw the Battle Scroll top menu, shrine -> Battle Scroll -> option 1 (Picture Scroll) -> A until
#              `RematchLog L2: EmakiMissionRequest`; wait for the stage; close. A Picture Scroll may be CONSUMED (Q6): opt-in only.
#   -> close, logs to captures\rematch\p0_<stamp>, ini / asi / catalogue restored, lock released.
# Guard (harness_guard.ps1): owner pid checked before every command and while waiting, -MaxMinutes, commit headroom before each
# launch, max 2 crashes, pid file for kill_leftovers.ps1. Every step is log-verified; a failed checkpoint backs out with B.
#
#   powershell -NoProfile -File tools\rematch\p0_vpad.ps1 -OwnerPid <agent pid> [-AsiPath <asi>] [-DryRun]
#        [-ShrineDown 3] [-ListDown 0] [-StartPresses 4] [-InteractBtns B,Y] [-NoModFallback] [-PictureScroll [-ScrollScript f]]
#        [-QuitScript f] [-MaxMinutes 40]
# Needs: a save made standing at a shrine (Continue must put the player there), Battle Scroll unlocked (story flag 0xAB: after
# "Crucible Manifested"), at least one cleared mission in the replay list, ASI >= 1.7.40 (mod fallback) / >= 1.7.21 (logs).
param([int]$OwnerPid = 0, [int]$MaxMinutes = 40, [string]$AsiPath = "", [switch]$DryRun,
      [string[]]$InteractBtns = @("B", "Y"), [int]$ShrineDown = 3, [int]$ListDown = 0, [int]$StartPresses = 4,
      [switch]$NoModFallback, [switch]$PictureScroll, [string]$ScrollScript = "", [string]$QuitScript = "", [int]$LoadWaitSec = 240)
$ErrorActionPreference = "Continue"
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
$tm = Join-Path (Split-Path -Parent $here) "testmatrix"
. "$tm\harness_guard.ps1"
. "$tm\vpad.ps1"
$repo = Split-Path -Parent (Split-Path -Parent $here)
$GameDir = "C:\Program Files (x86)\Steam\steamapps\common\Nioh3"
$ini = "$GameDir\Nioh3EngineFix.ini"; $asi = "$GameDir\Nioh3EngineFix.asi"; $logf = "$GameDir\Nioh3EngineFix.log"; $catDst = "$GameDir\Nioh3EngineFix_bosses.tsv"
if (-not $AsiPath) { $AsiPath = Join-Path $repo "mod\Nioh3EngineFix\bin\Nioh3EngineFix.asi" }
$catSrc = Join-Path $repo "mod\Nioh3EngineFix\Nioh3EngineFix_bosses.tsv"
$stamp = Get-Date -f yyyyMMdd_HHmmss
$out = if ($DryRun) { Join-Path $env:TEMP "rematch_p0_dry_$stamp" } else { "C:\Users\Ahmad Hamza\nioh3-engine\captures\rematch\p0_$stamp" }
New-Item -ItemType Directory -Force "$out\orig" | Out-Null
$trace = "$out\trace.txt"
function T([string]$s) { $l = "[{0}] {1}" -f (Get-Date -f HH:mm:ss), $s; [IO.File]::AppendAllText($trace, $l + "`r`n"); Write-Host $l }
$uo = "C:\Users\Ahmad Hamza\universal-optimizer\agent-bridge"
$env:UO_BRIDGE_TOKEN_FILE = "$env:APPDATA\uo-bridge\token"; $env:UO_AGENT_ID = "pc"
function Lock([string]$a) { Push-Location $uo; try { $o = (py -m uo_bridge.gamelock $a nioh3 --ttl 3600 2>&1 | Out-String).Trim(); @{ code = $LASTEXITCODE; out = $o } } finally { Pop-Location } }

# ---- ini text helpers (section-aware: Enabled / Enable exist in several sections) ----
function Set-IniKey([string]$t, [string]$sec, [string]$key, [string]$val) {
    $nl = "`r`n"
    $m = [regex]::Match($t, "(?ms)^\[$([regex]::Escape($sec))\][^\r\n]*\r?\n(.*?)(?=^\[|\z)")
    if (-not $m.Success) { return $t.TrimEnd() + "$nl$nl[$sec]$nl$key = $val$nl" }
    $body = $m.Groups[1].Value
    $re = "(?m)^$([regex]::Escape($key))[ \t]*=.*?(\r?)$"
    if ([regex]::IsMatch($body, $re)) { $nb = [regex]::Replace($body, $re, "$key = $val`$1", 1) }
    else { $nb = "$key = $val$nl" + $body }
    return $t.Substring(0, $m.Groups[1].Index) + $nb + $t.Substring($m.Groups[1].Index + $m.Groups[1].Length)
}
function Make-TestIni([string]$t) {
    $t = Force-Acl (Force-Vpad $t)
    foreach ($kv in @(@("Debug", "BossLog", "1"), @("Debug", "RematchLog", "1"), @("Debug", "VirtualPad", "1"), @("Debug", "KeepRunningUnfocused", "1"),
                      @("General", "AutoContinueLoad", "1"), @("Menu", "Enabled", "1"),
                      @("Rematch", "Enable", $(if ($NoModFallback) { "0" } else { "1" })), @("Rematch", "HiddenDuels", "0"), @("Rematch", "DirectStart", "0"))) {
        $t = Set-IniKey $t $kv[0] $kv[1] $kv[2]
    }
    return $t
}
function Ini-Get([string]$t, [string]$sec, [string]$key) {
    $m = [regex]::Match($t, "(?ms)^\[$([regex]::Escape($sec))\][^\r\n]*\r?\n(.*?)(?=^\[|\z)")
    if ($m.Success -and $m.Groups[1].Value -match "(?m)^$([regex]::Escape($key))[ \t]*=[ \t]*([^\r\n;]*)") { return $Matches[1].Trim() }
    return "(missing)"
}

# ---- game / log helpers (DryRun: simulated, nothing is sent and the game is never started) ----
$script:gamePid = 0; $script:locked = $false; $script:installed = $false; $script:catAdded = $false; $script:launchN = 0
$script:dryLog = New-Object System.Collections.Generic.List[string]
function Log-Len { if ($DryRun) { return [long]$script:dryLog.Count }; if (Test-Path $logf) { return (Get-Item $logf).Length }; return 0L }
function Log-From([long]$off) {
    if ($DryRun) { return ($script:dryLog | Select-Object -Skip $off) -join "`n" }
    if (-not (Test-Path $logf)) { return "" }
    $fs = [IO.File]::Open($logf, "Open", "Read", "ReadWrite"); try { $null = $fs.Seek($off, "Begin"); (New-Object IO.StreamReader($fs)).ReadToEnd() } finally { $fs.Dispose() }
}
# first log line matching $re after offset $off within $sec seconds (owner-checked waits), else $null
function Wait-Log([string]$re, [int]$sec, [long]$off) {
    if ($DryRun) { $l = "[dry] (simulated log line for /$re/)"; $script:dryLog.Add($l); return $l }
    $t0 = Get-Date
    do {
        $hit = (Log-From $off) -split "`n" | Where-Object { $_ -match $re } | Select-Object -First 1
        if ($hit) { return $hit.Trim() }
        Start-Sleep 1
    } while (((Get-Date) - $t0).TotalSeconds -lt $sec)
    return $null
}
function VPx {   # VP with a trace line; DryRun prints only
    $line = ($args | ForEach-Object { "$_" }) -join " "
    if ($DryRun) { T "  [dry] vpad $line"; return [pscustomobject]@{ ok = $true; reply = "ok (dry)" } }
    $r = VP @args; T "  vpad $line -> $($r.reply)"; return $r
}
function St { if ($DryRun) { return "frame=1 loading=0 paused=0 player=1 field=1 menu_pause=0 (dry)" }; return (VP-State) }
function St-Field([string]$s) { return ($s -match 'field=1') }
function St-Menu([string]$s) { return ($s -match 'menu_pause=1') }
function Press([string]$b, [int]$n = 1, [double]$gap = 0.6) { for ($i = 0; $i -lt $n; $i++) { $null = VPx press $b 120; Start-Sleep $gap } }
# B until the player is back in the field (field=1 for 2 s), at most $max presses
function Back-Out([int]$max = 6) {
    if ($DryRun) { T "  [dry] back out (B until field=1)"; return $true }
    for ($i = 0; $i -lt $max; $i++) {
        if (St-Field (St)) { Start-Sleep 2; if (St-Field (St)) { T "  back in the field"; return $true } }
        Press B 1 1.2
    }
    $s = St; T "  back-out ended: $s"; return (St-Field $s)
}
function Wait-Field([int]$sec, [int]$hold = 5) {
    if ($DryRun) { T "  [dry] wait field=1 for $hold s"; return $true }
    $t0 = Get-Date; $since = $null
    while (((Get-Date) - $t0).TotalSeconds -lt $sec) {
        if (St-Field (St)) { if (-not $since) { $since = Get-Date }; if (((Get-Date) - $since).TotalSeconds -ge $hold) { return $true } } else { $since = $null }
        Start-Sleep 2
    }
    return $false
}
function Launch {
    if ($DryRun) { $script:launchN++; T "[dry] launch #$($script:launchN): Assert-Launch ($(Get-CommitInfo)), steam://rungameid/3681010, give focus back, wait pipe, title-continue"; return $true }
    Assert-Launch
    $before = VP-ForegroundInfo; T "foreground before launch: $($before.name)"
    Start-Process "steam://rungameid/3681010"; $t0 = Get-Date; $p = $null
    while (((Get-Date) - $t0).TotalSeconds -lt 180) { $p = Get-Process Nioh3 -ErrorAction SilentlyContinue | Where-Object MainWindowHandle -ne 0 | Select-Object -First 1; if ($p) { break }; Start-Sleep 3 }
    if (-not $p) { Note-Crash "no window"; throw "no game window after 180 s" }
    $script:gamePid = $p.Id; $script:launchN++; Register-Game; T "launch #$($script:launchN): game pid $($p.Id)"
    Start-Sleep 3; VP-GiveBackFocus $before; Start-Sleep 1
    if (-not (VP-WaitPipe 180)) { throw "no VirtualPad pipe (is the ASI installed with VirtualPad = 1?)" }
    T "ping: $((VP ping).reply)"
    $ok = VP-TitleContinue 300 "$out\title_continue_$($script:launchN).txt"
    T "title -> Continue -> field: $ok; state $(St)"
    return $ok
}
function Close-Game {
    if ($DryRun) { T "[dry] close the game (CloseMainWindow, 30 s, then kill)"; return }
    $p = if ($script:gamePid) { Get-Process -Id $script:gamePid -ErrorAction SilentlyContinue } else { $null }
    if ($p) { Note-Closed; $null = $p.CloseMainWindow(); $p.WaitForExit(30000) | Out-Null; if (-not $p.HasExited) { T "!! game did not exit in 30 s - killing"; Stop-Process -Id $p.Id -Force }; Start-Sleep 5 }
    $script:gamePid = 0
    if (Test-Path $logf) { Copy-Item $logf "$out\Nioh3EngineFix_launch$($script:launchN).log" -Force -ErrorAction SilentlyContinue }
}
function Restore {
    if ($DryRun) { return }
    if ($script:installed) {
        if (Test-Path "$out\orig\Nioh3EngineFix.ini") { Copy-Item "$out\orig\Nioh3EngineFix.ini" $ini -Force }
        if (Test-Path "$out\orig\Nioh3EngineFix.asi") { Copy-Item "$out\orig\Nioh3EngineFix.asi" $asi -Force }
        if ($script:catAdded) { Remove-Item $catDst -Force -ErrorAction SilentlyContinue }
        elseif (Test-Path "$out\orig\Nioh3EngineFix_bosses.tsv") { Copy-Item "$out\orig\Nioh3EngineFix_bosses.tsv" $catDst -Force }
        $script:installed = $false; T "ini + asi + catalogue restored"
    }
    if ($script:locked) { $r = Lock "release"; $script:locked = $false; T "lock released: $($r.out)" }
}

# ---- the steps ----
$R = [ordered]@{ dry = [bool]$DryRun; out = $out }
$replayRe = 'RematchLog L7: (MissionReplayMenu_Setup|replay menu open|MissionSelectListBuild|BattleScrollTop_Select)'
function Open-ShrineReplay {   # step A: returns "shrine", "mod" or ""
    $off = Log-Len
    $opened = $false
    foreach ($b in $InteractBtns) {
        Press $b 1 3.0
        $s = St; T "  after interact '$b': $s"
        if ($DryRun -or (St-Menu $s)) { $opened = $true; T "  a game menu is open after '$b' (shrine menu presumed)"; break }
    }
    $R.shrine_menu = $opened
    if ($opened) {
        Press DOWN $ShrineDown 0.5
        Press A 1 0.5
        $hit = Wait-Log $replayRe 6 $off
        if (-not $hit) { T "  no replay-menu line yet - one more A (Battle Scroll top menu, option 0 = replay)"; Press A 1 0.5; $hit = Wait-Log $replayRe 6 $off }
        if ($hit) {
            $R.battle_scroll_top = [bool]((Log-From $off) -match 'BattleScrollTop_Select')
            T "  replay menu reached from the shrine: $hit"; return "shrine"
        }
        T "  shrine item $ShrineDown did not open the replay menu - backing out"
        $null = Back-Out
    } else { T "  no game menu after $($InteractBtns -join ',') - not standing at a shrine?" }
    if ($NoModFallback) { return "" }
    T "  fallback: the mod's R-a (menu set Rematch 1 -> MenuOpenMissionReplay(0, 0, 6))"
    $off = Log-Len
    $null = VPx menu set Rematch 1
    $hit = Wait-Log 'Rematch: (R-a MenuOpenMissionReplay|refused)' 12 $off
    T "  mod: $hit"
    if ($hit -and $hit -notmatch 'refused') { $h2 = Wait-Log $replayRe 6 $off; if ($h2) { T "  replay menu via the mod: $h2"; return "mod" } }
    return ""
}
function Start-Replay {   # step B: returns the L7 request line or $null
    Start-Sleep 3   # list build + getters lines
    if ($ListDown -gt 0) { Press DOWN $ListDown 0.5 }
    $off = Log-Len
    for ($i = 1; $i -le $StartPresses; $i++) {
        Press A 1 0.5
        $hit = Wait-Log 'RematchLog L7: (MissionRequestById|MissionRequestEnqueue)' 4 $off
        if ($hit) { T "  replay START (A x${i}): $hit"; return $hit }
        T "  A x${i} - no request yet"
    }
    T "  no replay request after $StartPresses A presses - backing out"
    $null = Back-Out
    return $null
}
function Wait-Stage([string]$what) {   # step C: the stage of the started mission
    $off0 = $script:startOff
    $in = Wait-Log 'RematchLog L4: IN MISSION' $LoadWaitSec $off0
    $f = Wait-Field 120 5
    T "  $what stage: IN MISSION line: $([bool]$in), field: $f"
    Start-Sleep 20
    return [bool]$in
}
function Quit-ToTitle {
    if ($QuitScript -and (Test-Path $QuitScript)) {
        if ($DryRun) { T "[dry] quit script $QuitScript" ; return $true }
        $o = (py -I (Join-Path $tm "vpad.py") --owner-pid $global:HG.ownerPid run $QuitScript 2>&1 | Out-String); T "  quit script: $($o.Trim())"
        $t0 = Get-Date
        while (((Get-Date) - $t0).TotalSeconds -lt 60) { $s = St; if ($s -match 'player=0') { T "  at the title: $s"; return $true }; Start-Sleep 2 }
        T "  quit script did not reach the title - closing the game instead"
    }
    Close-Game
    return $false
}

Init-Guard -Name "rematch_p0" -OwnerPid $OwnerPid -MaxMinutes $MaxMinutes -Cleanup { Close-Game; Restore; Stop-TestPopup }
try {
    T "p0_vpad: out=$out dry=$([bool]$DryRun) asi=$AsiPath shrineDown=$ShrineDown listDown=$ListDown startPresses=$StartPresses interact=$($InteractBtns -join ',') modFallback=$(-not $NoModFallback) pictureScroll=$([bool]$PictureScroll)"
    foreach ($f in @($AsiPath, $catSrc, (Join-Path $tm "vpad.py"), $ini)) { if (-not (Test-Path $f)) { throw "missing: $f" } }
    if ($ScrollScript -and -not (Test-Path $ScrollScript)) { throw "missing -ScrollScript $ScrollScript" }
    if ($QuitScript -and -not (Test-Path $QuitScript)) { throw "missing -QuitScript $QuitScript" }
    $ver = (Select-String -Path (Join-Path $repo "mod\Nioh3EngineFix\src\dllmain.cpp") -Pattern '#define MOD_VERSION "([^"]+)"').Matches[0].Groups[1].Value
    T "source ASI version $ver (the asi at -AsiPath must be built from it); $(Get-CommitInfo)"
    $test = Make-TestIni ([IO.File]::ReadAllText($ini))
    [IO.File]::WriteAllText("$out\test_ini.ini", $test, (New-Object Text.UTF8Encoding($false)))
    T ("test ini: " + ((@("Debug:BossLog", "Debug:RematchLog", "Debug:VirtualPad", "Debug:KeepRunningUnfocused", "General:AutoContinueLoad", "Menu:Enabled",
                         "Rematch:Enable", "Rematch:HiddenDuels", "Rematch:DirectStart") | ForEach-Object { $p = $_ -split ':'; "[$($p[0])] $($p[1])=$(Ini-Get $test $p[0] $p[1])" }) -join "  "))
    if ($DryRun) { T "[dry] no lock, no backup, no install; game dir untouched" }
    else {
        if (Get-Process Nioh3 -ErrorAction SilentlyContinue) { throw "Nioh3 is running" }
        $r = Lock "acquire"; if ($r.code -ne 0) { throw "lock: $($r.out)" }; $script:locked = $true; T "lock: $($r.out)"
        Copy-Item $ini "$out\orig\" -Force; Copy-Item $asi "$out\orig\" -Force
        if (Test-Path $catDst) { Copy-Item $catDst "$out\orig\" -Force } else { $script:catAdded = $true }
        $sb = "D:\nioh3-re\save-backups\$stamp-rematchp0"; New-Item -ItemType Directory -Force $sb | Out-Null
        Copy-Item "$env:LOCALAPPDATA\KoeiTecmo\NIOH3\*" $sb -Recurse -Force; T "save folder copied to $sb"
        $script:installed = $true
        [IO.File]::WriteAllText($ini, $test, (New-Object Text.UTF8Encoding($false))); Copy-Item $AsiPath $asi -Force; Copy-Item $catSrc $catDst -Force
        T "installed $AsiPath + test ini + catalogue"
        Start-TestPopup -Name "Rematch Phase 0 capture" -InputMode vpad
    }
    # ---- launch 1: P0-5 + L7 replay start ----
    Set-TestStatus "launch 1: title -> field"
    if (-not (Launch)) { throw "not in the field" }
    Start-Sleep 5
    Set-TestStatus "P0-5: shrine -> Battle Scroll -> replay menu"
    $via = Open-ShrineReplay
    $R.replay_menu = $via
    if ($via) {
        Set-TestStatus "L7: start one cleared mission"
        $script:startOff = Log-Len
        $req = Start-Replay
        $R.l7_request = $req
        if ($req) { Set-TestStatus "waiting for the replay stage"; $R.replay_in_mission = Wait-Stage "replay" }
    }
    $atTitle = Quit-ToTitle
    # ---- optional P0-1: Picture Scroll ----
    if ($PictureScroll) {
        Set-TestStatus "P0-1: Picture Scroll"
        if (-not $ScrollScript -and -not $R.battle_scroll_top -and -not $DryRun) { T "P0-1 skipped: no -ScrollScript and no Battle Scroll top menu seen (Picture Scroll half not unlocked?)" }
        else {
            $ok = if ($atTitle) { if ($DryRun) { $true } else { VP-TitleContinue 300 "$out\title_continue_p01.txt" } } else { Launch }
            if ($ok) {
                Start-Sleep 5
                $off = Log-Len; $script:startOff = $off
                if ($ScrollScript) {
                    if ($DryRun) { T "[dry] scroll script $ScrollScript" } else { $o = (py -I (Join-Path $tm "vpad.py") --owner-pid $global:HG.ownerPid run $ScrollScript 2>&1 | Out-String); T "  scroll script: $($o.Trim())" }
                } else {
                    foreach ($b in $InteractBtns) { Press $b 1 3.0; if ($DryRun -or (St-Menu (St))) { break } }
                    Press DOWN $ShrineDown 0.5; Press A 1 1.5      # Battle Scroll
                    Press DOWN 1 0.5; Press A 1 1.5                # option 1 = Picture Scroll (Emaki)
                }
                $l2 = $null
                for ($i = 0; $i -lt 4 -and -not $l2; $i++) { $l2 = Wait-Log 'RematchLog L2: EmakiMissionRequest' 4 $off; if (-not $l2) { Press A 1 0.5 } }
                $R.l2_request = $l2
                if ($l2) { T "  Picture Scroll START: $l2"; $R.scroll_in_mission = Wait-Stage "Picture Scroll" } else { T "  no EmakiMissionRequest - backing out"; $null = Back-Out }
            }
            Close-Game
        }
    }
} catch {
    T "!! $($_.Exception.Message)"; $R.error = $_.Exception.Message
} finally {
    Close-Game
    Stop-TestPopup
    if (-not $DryRun) {
        $all = Get-ChildItem "$out\Nioh3EngineFix_launch*.log" -ErrorAction SilentlyContinue | ForEach-Object { Get-Content $_.FullName }
        $keep = $all | Where-Object { $_ -match 'RematchLog|BossLog|Rematch:|VPad|MENU: Boss rematch|Nioh3EngineFix 1\.' }
        Set-Content "$out\rematchlog.txt" $keep
        $R.lines = @{ menu = @($keep | Where-Object { $_ -match $replayRe }).Count; getters = @($keep | Where-Object { $_ -match 'L7: getters' }).Count
                      l7_request = @($keep | Where-Object { $_ -match 'L7: (MissionRequestById|MissionRequestEnqueue)' }).Count
                      in_mission = @($keep | Where-Object { $_ -match 'IN MISSION' }).Count; l2 = @($keep | Where-Object { $_ -match 'L2: EmakiMissionRequest' }).Count
                      l3 = @($keep | Where-Object { $_ -match 'L3: MissionSetupEmaki' }).Count }
    }
    Restore
    Done-Guard
    $R | ConvertTo-Json -Depth 4 | Set-Content "$out\result.json"
    T "result: $($R | ConvertTo-Json -Compress -Depth 4)"
    T "out: $out"
}
