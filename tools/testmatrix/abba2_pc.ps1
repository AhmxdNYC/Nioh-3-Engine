# abba2_pc.ps1 - drift-aware PC flag A/B (replaces abba_pc.ps1 Block A, 2026-10-08). Analysis: abba2_report.py (doc there).
#   Why: run 4a2b108 measured the in-game FSR frame-gen regime (FG flapped paused/flapping/steady -> 'steps' of 15-35 %),
#   and no flag was ever toggled (key list passed as one argument, Toggle failure swallowed as a truthy array).
#   Method: CPU-bound test condition via config.xml (backed up + restored): FRAME_GENERATION OFF (FG engagement flaps and
#   pins the rendered rate near 120), and -RenRat/-Upscaler (default 50 / FSR) because at the user's REN_RAT 75 + XeSS the
#   PC renders 114 fps GPU-bound (gpu busy 8.7 ms = frame time; run1 L1) where a CPU flag cannot move fps. No camera input after spawn,
#   60 s warm-up; live items = ON OFF OFF ON quads (-Quads per item, short windows), each toggle confirmed by its
#   'MENU: <label> -> X' log line, state read back from the stats lines; ini-only items = relaunch per arm in the order
#   BASE F1 F2 F2 F1 BASE (fixed spawn = the save's load point), 3 windows per launch. Drift/step detection: see report.
# Guard: harness_guard.ps1 (owner pid, MaxMinutes, commit headroom, crash limit, pid files). User activity (GetLastInputInfo):
#   while Nioh3 has focus the idle clock is not usable (run1: idle 4.4 s = time since the game lost focus), so it is read only
#   when Nioh3 is NOT foreground (refusal / focus check): wait 3 s; input inside those 3 s = a human -> abort; else refocus
#   (max 6). Run it from a HIDDEN console (Start-Process powershell -WindowStyle Hidden) so dotnet children open no window.
# Lock: caller holds the nioh3 game lock (renewed here).
# -Input vpad (ASI >= 1.7.37, tools\testmatrix\vpad.ps1): no OS input - the title / Continue step and every menu toggle go
#   through the mod's VirtualPad pipe (VirtualPad = 1 + KeepRunningUnfocused = 1 in the test ini); no foreground, no idle wait,
#   no refocus; windows are kept when the user works on the PC (foreground / idle recorded in the manifest as user_active).
#   Default -Input ui = the SendInput path above, unchanged.
# 1.7.39 (notes\pc-cpu-next.md): -Arms BASE,DQ,P1E7,BASE,P1E7,DQ,BASE = explicit launch order (3+ BASE launches -> the
#   report compares each arm with the BASE interpolated between its neighbours). -MaxFps 240 writes [VRR] MaxFps so the
#   120 cap does not compress CPU gains (run5/run6 sat at 116-118.5 fps). A -Live item that is 0 / missing in the base ini
#   is installed switched off (= 2) for the run, so DQ / P1E7 / AABB (live rows since 1.7.39) need no ini edit.
param([int]$OwnerPid = 0, [int]$MaxMinutes = 120, [int]$WindowSec = 20, [int]$Quads = 3, [int]$QuadsB = 2,
      [string[]]$Live = @("B", "PP", "GF", "MQ"), [string[]]$LaunchFlags = @("DQ", "P1E7"), [switch]$DryRun, [string]$RunName = "",
      [int]$RenRat = 50, [string]$Upscaler = "FSR", [int]$IdleNeed = 120, [int]$IdleWaitMin = 30,
      [Alias("Input")][ValidateSet("ui", "vpad")][string]$InputMode = "ui", [string[]]$Arms = @(), [int]$MaxFps = 0,
      [string]$SrcRoot = "", [switch]$NoQuietGuard, [int]$MaxNoisyRedo = 2, [double]$QuietMaxCores = 0, [string]$BaseIniFile = "", [int]$Affinity = 0, [switch]$NoForceRefresh)
# run8b (2026-10-09, notes\run8b-analysis.md): the desktop fell to 60 Hz (second monitor unplugged) and Block C measured 60.00 fps
#   everywhere ('vrr tier 60 cap 60'). Before the first launch the game display must run >= max(120, -MaxFps) Hz (raised for the run
#   unless -NoForceRefresh, old mode put back at the end), and before every window the mod's 'vrr tier T cap C' must read
#   tier >= 120 and cap >= that number - otherwise HARNESS-ABORT with the reason. A dead game logs why (exit code, other harnesses
#   that ended meanwhile, newest dumps) and the game log is kept as abort_game.log.   # -Affinity N: weak-CPU simulation, Nioh3 process pinned to N logical CPUs (distinct cores first)
# run7 (2026-10-08, notes\run7-analysis.md):
#   -SrcRoot <checkout/worktree> = the tree the installed ASI was built from (e.g. D:\nioh3-wt\integ for 1.7.40 BPO): menu rows
#   come from its mod\...\menu.cpp and live-item install values from its tools\testmatrix\matrix.json (default: this repo).
#   Live items missing from the base ini are installed at the matrix's live_installed_value (2 = installed, switch off) ONLY
#   when the item has one; items whose ASI ignores 2 (AABB, B, PP, GF, ... in 1.7.39: any nonzero = installed ON) get 1 and
#   boot ON. With -Input vpad the real boot state is read back ('menu get') before the first toggle, and a 'menu set' whose
#   read-back value already equals the target counts as confirmed (the mod logs no MENU line for a no-op).
#   Quiet-machine guard (harness_guard.ps1): before each window wait until foreign CPU load is quiet; sample it during the
#   window; a noisy window is redone (max -MaxNoisyRedo) or kept with noisy=true (rejected + listed by abba2_report.py).
#   %TEMP%\nioh3_measuring.lock holds the harness pid while the run is active. -NoQuietGuard disables the wait/sampling.
if ($QuietMaxCores -gt 0) { $env:QG_MAX_CORES = [string]$QuietMaxCores }
$QuietGuard = -not $NoQuietGuard
$vp = $InputMode -eq "vpad"
$ErrorActionPreference = "Continue"
$Arms = @($Arms -split "," | ForEach-Object { $_.Trim() } | Where-Object { $_ })
if ($Arms.Count) {   # explicit order; the launch flags are its non-BASE arms
    if ($Arms[0] -ne "BASE" -or $Arms[-1] -ne "BASE") { throw "-Arms must start and end with BASE" }
    $LaunchFlags = @($Arms | Where-Object { $_ -ne "BASE" } | Select-Object -Unique)
}
$Live = @($Live -split "," | ForEach-Object { $_.Trim() } | Where-Object { $_ -and $_ -ne "none" }); $LaunchFlags = @($LaunchFlags -split "," | ForEach-Object { $_.Trim() } | Where-Object { $_ -and $_ -ne "none" })   # -File passes "B,GF" as one string
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
. "C:\Users\Ahmad Hamza\nioh3-engine\patcher\common.ps1"
$GameDir = "C:\Program Files (x86)\Steam\steamapps\common\Nioh3"; $ini = "$GameDir\Nioh3EngineFix.ini"; $logf = "$GameDir\Nioh3EngineFix.log"
$baseIni = "$GameDir\Nioh3EngineFix.ini.preboss.bak"
if ($BaseIniFile) { $baseIni = $BaseIniFile }   # run8: -DryRun while the game dir is stashed (vanilla play session) has no preboss.bak
if (-not $RunName) { $RunName = "run_" + (Get-Date -f yyyyMMdd_HHmm) }
$out = "$ws\captures\tm\abba2\$RunName"; New-Item -ItemType Directory -Force $out | Out-Null
$uoBridge = "C:\Users\Ahmad Hamza\universal-optimizer\agent-bridge"
$env:UO_BRIDGE_TOKEN_FILE = "$env:APPDATA\uo-bridge\token"; $env:UO_AGENT_ID = "pc"
$cfgBak = "$cfg.abba2.bak"
$script:gamePid = 0; $script:gameProc = $null; $script:launchT = Get-Date
$NeedHz = [Math]::Max(120, $MaxFps)   # display refresh and in-game cap this run needs (tier 120; -MaxFps lifts the cap above it)
$trace = "$out\trace.txt"
. "$here\harness_guard.ps1"
. "$here\vpad.ps1"
function T([string]$s) { $l = "[{0}] {1}" -f (Get-Date -f HH:mm:ss), $s; try { [IO.File]::AppendAllText($trace, $l + "`r`n") } catch {}; Write-Host $l }

# ---- ini / config helpers ---------------------------------------------------------------------------------------
function Make-IniText([hashtable]$kv) {   # base = the user's installed ini (preboss.bak); keys missing in it go under [Patches]
    $t = [IO.File]::ReadAllText($baseIni)
    foreach ($k in $kv.Keys) {
        if ($t -match "(?m)^$k\s*=") { $t = $t -replace "(?m)^$k\s*=.*", "$k = $($kv[$k])" }
        else { $t = $t -replace "(?m)^\[Patches\]\s*$", "[Patches]`r`n$k = $($kv[$k])" }
        if ($t -notmatch "(?m)^$k = $([regex]::Escape([string]$kv[$k]))") { throw "ini key $k not set" }
    }
    $t = Force-Acl $t   # off by default since 1.7.33; the harness needs it
    if ($vp) { $t = Force-Vpad $t }   # -Input vpad: VirtualPad = 1, KeepRunningUnfocused = 1
    $t
}
function Set-TestIni([hashtable]$kv) { [IO.File]::WriteAllText($ini, (Make-IniText $kv), (New-Object Text.UTF8Encoding($false))) }
function Set-TestCfg {   # back up the game's config.xml once, then the test condition (restored in Restore-All)
    if (-not (Test-Path $cfgBak)) { Copy-Item $cfg $cfgBak }
    $n = [IO.File]::ReadAllText($cfg)
    foreach ($kv in @(@("FRAME_GENERATION", "OFF"), @("REN_RAT", [string]$RenRat), @("RESOLUTION_SCALING", $Upscaler))) {
        $n = $n -replace "<$($kv[0])>[^<]+</$($kv[0])>", "<$($kv[0])>$($kv[1])</$($kv[0])>"
        if ($n -notmatch "<$($kv[0])>$($kv[1])</$($kv[0])>") { throw "config.xml has no $($kv[0])" }
    }
    [IO.File]::WriteAllText($cfg, $n, (New-Object Text.UTF8Encoding($false)))
    $g = @{}; foreach ($k in "REN_FPS", "VSYNC", "DISP_MODE", "MONITOR_ID", "SCR_REZ", "DYNAMIC_FPS") { $g[$k] = if ($n -match "<$k>([^<]*)<") { $Matches[1] } else { "?" } }
    T ("config.xml: " + (($g.Keys | Sort-Object | ForEach-Object { "$_=$($g[$_])" }) -join " "))
    if ($g.REN_FPS -match '^\d+$' -and [int]$g.REN_FPS -lt 120) { throw "HARNESS-ABORT: config.xml REN_FPS $($g.REN_FPS) < 120: the game would cap below the 120 tier - refusing to measure" }
}
function Restore-All {
    Restore-DisplayRefresh { param($m) T $m }
    if (Test-Path $cfgBak) { Copy-Item $cfgBak $cfg -Force; Remove-Item $cfgBak -Force; T "config.xml restored" }
    if (Test-Path $baseIni) { Copy-Item $baseIni $ini -Force; T "ini restored from preboss.bak" }
}

# ---- input (fixed: flat args, exit codes checked, refusal handling keeps the code) ---------------------------------
function UI {
    Assert-Owner
    $flat = @(); foreach ($a in $args) { foreach ($x in @($a)) { $flat += [string]$x } }
    $cmd = $flat[0]
    if ($cmd -ne "shot" -and -not (Get-Process Nioh3 -ErrorAction SilentlyContinue)) { throw "HARNESS-ABORT: game not running before '$cmd'" }
    $o = dotnet fsi $ui @flat 2>&1 | Out-String; $code = $LASTEXITCODE
    if ($code -eq 3 -and $cmd -ne "shot") {
        if (Refocus-Policy "refused $cmd") { $o = dotnet fsi $ui @flat 2>&1 | Out-String; $code = $LASTEXITCODE }
    }
    if ($code -eq 3) { throw "HARNESS-ABORT: input refused ($($o.Trim()))" }
    if ($code -ne 0) { throw "HARNESS-ABORT: ui.fsx $cmd failed (exit $code): $($o.Trim())" }
    if ($cmd -notin "shot", "focus", "idle") { Mark-Input }
}
function Fg-Name { $o = dotnet fsi $ui fg 2>&1 | Out-String; if ($o -match 'foreground: pid=\d+ (\S+)') { $Matches[1] } else { "?" } }
$script:refocus = 0
function Idle-Sec { $o = dotnet fsi $ui idle 2>&1 | Out-String; if ($o -match 'idle ([\d.]+)') { [double]$Matches[1] } else { 0 } }
# Nioh3 is not foreground. Never take focus from an active user (coordinator 2026-10-08): wait until GetLastInputInfo idle
# >= $IdleNeed s (poll 15 s, give up after $IdleWaitMin min = abort), then refocus. Max 12 refocuses per run.
function Refocus-Policy([string]$why) {
    $fg = Fg-Name; $idle = Idle-Sec
    T ("focus lost ($why): foreground={0}, system idle {1:N1}s - waiting for idle >= {2}s" -f $fg, $idle, $IdleNeed)
    $t0 = Get-Date
    while ($idle -lt $IdleNeed) {
        if (((Get-Date) - $t0).TotalMinutes -gt $IdleWaitMin) { throw "HARNESS-ABORT: user active for > $IdleWaitMin min (foreground $fg)" }
        Start-Sleep 15; $idle = Idle-Sec
    }
    if ((Fg-Name) -eq "Nioh3") { return $true }
    if (++$script:refocus -gt 12) { throw "HARNESS-ABORT: focus lost more than 12 times" }
    $null = dotnet fsi $ui focus Nioh3 2>&1; Start-Sleep 2
    $ok = ((Fg-Name) -eq "Nioh3"); T "refocus after idle $([Math]::Round($idle))s: $ok"; return $ok
}
function Assert-Focus([string]$why) { if ($vp) { return }; if ((Fg-Name) -ne "Nioh3") { if (-not (Refocus-Policy $why)) { throw "HARNESS-ABORT: cannot refocus Nioh3" } } }
# ---- game control ------------------------------------------------------------------------------------------------
function Renew { Push-Location $uoBridge; try { py -m uo_bridge.gamelock renew nioh3 --ttl 5400 | Out-Null } finally { Pop-Location } }
function Close-GameClean {
    Note-Closed
    $p = if ($script:gamePid) { Get-Process -Id $script:gamePid -ErrorAction SilentlyContinue } else { $null }
    if ($p) { $null = $p.CloseMainWindow(); $p.WaitForExit(25000) | Out-Null; if (-not $p.HasExited) { Stop-Process -Id $script:gamePid -Force }; Start-Sleep 8 }
    $script:gamePid = 0
}
function Start-Play-Vpad {   # -Input vpad: no clicks, no focus, no idle wait
    Assert-Launch
    $before = VP-ForegroundInfo
    $t0 = Get-Date
    & (Join-Path $PSScriptRoot '../steam/start_autoplay.ps1')
    Start-Process "steam://rungameid/3681010"
    $p = $null
    while (((Get-Date) - $t0).TotalSeconds -lt 180) { $p = Get-Process Nioh3 -ErrorAction SilentlyContinue | Where-Object MainWindowHandle -ne 0 | Select-Object -First 1; if ($p) { break }; Start-Sleep 3 }
    if (-not $p) { Note-Crash "no game window after launch"; return $false }
    $script:gamePid = $p.Id; $script:gameProc = $p; $null = $p.Handle; $script:launchT = $t0; Register-Game;   # Handle kept open: ExitCode stays readable if ($Affinity -gt 0) { Set-WeakAffinity $Affinity { param($m) T $m } }
    Start-Sleep 3; VP-GiveBackFocus $before   # the new game window took the foreground once: hand it back to the user
    if (-not (VP-WaitPipe 180)) { T "!! no VirtualPad pipe (is the ASI >= 1.7.37 installed?)"; return $false }
    T "vpad: $((VP ping).reply)"
    $ok = VP-TitleContinue 300 "$out\title_continue.txt"
    T "vpad title -> field: $ok; $(VP-State)"
    return $ok
}
function Start-Play {
    if ($vp) { return (Start-Play-Vpad) }
    Assert-Launch
    $t0 = Get-Date; $idle = Idle-Sec   # the boot clicks take focus: only launch when nobody has touched the PC for $IdleNeed s
    while ($idle -lt $IdleNeed) {
        if (((Get-Date) - $t0).TotalMinutes -gt $IdleWaitMin) { throw "HARNESS-ABORT: user active for > $IdleWaitMin min before a launch" }
        T ("launch waits: system idle {0:N0}s < {1}s" -f $idle, $IdleNeed); Start-Sleep 30; $idle = Idle-Sec
    }
    $t0 = Get-Date
    & (Join-Path $PSScriptRoot '../steam/start_autoplay.ps1')
    Start-Process "steam://rungameid/3681010"
    $p = $null
    while (((Get-Date) - $t0).TotalSeconds -lt 180) { $p = Get-Process Nioh3 -ErrorAction SilentlyContinue | Where-Object MainWindowHandle -ne 0 | Select-Object -First 1; if ($p) { break }; Start-Sleep 3 }
    if (-not $p) { Note-Crash "no game window after launch"; return $false }
    $script:gamePid = $p.Id; $script:gameProc = $p; $null = $p.Handle; $script:launchT = $t0; Register-Game;   # Handle kept open: ExitCode stays readable if ($Affinity -gt 0) { Set-WeakAffinity $Affinity { param($m) T $m } }
    Start-Sleep 25
    UI focus Nioh3; UI click 1280 887; Start-Sleep 22
    UI focus Nioh3; UI click 1280 1170; Start-Sleep 7
    UI focus Nioh3; UI click 380 1024; Start-Sleep 25
    UI focus Nioh3; UI key 20; Start-Sleep 5; UI key 20; Start-Sleep 4
    for ($i = 0; $i -lt 3; $i++) { UI focus Nioh3; Start-Sleep 1; if (Test-InGame) { return $true }; Start-Sleep 3 }
    T "not in gameplay after the boot clicks (game menu / tip screen?) - Ensure-InGame (Space / Esc)"   # run3: the game menu was open
    Assert-Focus "boot recover"
    return (Ensure-InGame)
}
function Game-Alive {
    $p = if ($script:gamePid) { Get-Process -Id $script:gamePid -ErrorAction SilentlyContinue } else { $null }
    if (-not $p -or $p.HasExited) {
        $ec = try { if ($script:gameProc -and $script:gameProc.HasExited) { "exit code 0x{0:X8} at {1:HH:mm:ss}" -f $script:gameProc.ExitCode, $script:gameProc.ExitTime } else { "exit code unknown" } } catch { "exit code unreadable" }
        T "Game-Alive: process gone (pid $script:gamePid, $ec)"; return $false
    }
    if (-not $p.Responding) { T "Game-Alive: process not responding (pid $script:gamePid)"; return $false }
    $age = ((Get-Date) - (Get-Item $logf).LastWriteTime).TotalSeconds
    if ($age -ge 45) { T "Game-Alive: log stale for $([int]$age) s ($logf)"; return $false }
    return $true
}
function Assert-NotCapped([string]$where) {   # the mod's 30 s stats line: 'vrr tier T cap C armed A' (last one in the log)
    $v = Get-VrrCap (Log-From ([Math]::Max(0, (Log-Len) - 65536)))
    if (-not $v) { T "!! cap check ($where): no 'vrr tier' stats line in the log yet"; return $null }
    if ($v.tier -lt 120 -or $v.cap -lt $NeedHz) {
        throw "HARNESS-ABORT: game capped at $($v.cap) fps (vrr tier $($v.tier), armed $($v.armed)) < $NeedHz needed ($where) - refusing to measure. Display now: $(Format-DisplayModes)"
    }
    return $v
}
function Dead-Report {   # why the game is gone: kept log tail, other harnesses active since the launch, newest dumps
    try { Copy-Item $logf "$out\abort_game.log" -Force; T "game log kept: $out\abort_game.log; last lines:"; Get-Content $logf -Tail 4 | ForEach-Object { T "   | $_" } } catch {}
    $fa = @(Get-ForeignGuardActivity $script:launchT); if ($fa.Count) { T "other harnesses active since the launch ($($script:launchT.ToString('HH:mm:ss'))): $($fa -join '; ')" }
    $d = @(Get-ChildItem "$env:LOCALAPPDATA\KoeiTecmo\NIOH3\dump", "$env:LOCALAPPDATA\CrashDumps" -File -ErrorAction SilentlyContinue | Where-Object { $_.LastWriteTime -ge $script:launchT })
    T "crash dumps since the launch: $(if ($d.Count) { ($d | ForEach-Object { $_.FullName }) -join ', ' } else { 'none' })"
}
function Log-Len { (Get-Item $logf).Length }
function Log-From([long]$off) { $fs = [IO.File]::Open($logf, "Open", "Read", "ReadWrite"); try { $null = $fs.Seek($off, "Begin"); (New-Object IO.StreamReader($fs)).ReadToEnd() } finally { $fs.Dispose() } }
function Mem-Guard {   # 'stats mem' line: system commit within 2 GB of the limit -> abort
    $tx = Log-From ([Math]::Max(0, (Log-Len) - 65536))
    $m = [regex]::Matches($tx, 'stats mem:.*?\| sys RAM avail (\d+) MB, commit (\d+)/(\d+) MB')
    if ($m.Count -eq 0) { return }
    $g = $m[$m.Count - 1].Groups; $head = [int]$g[3].Value - [int]$g[2].Value
    if ($head -lt 2048) { $c = Get-CommitNumbers; if ($c.effective -lt 6) { throw "HARNESS-ABORT: commit headroom $head MB (effective $([Math]::Round($c.effective,1)) GB)" } }
}
function Boot-Info {   # version + DQ/P1E7 install lines from the first part of the log
    $tx = Log-From 0; if ($tx.Length -gt 400000) { $tx = $tx.Substring(0, 400000) }
    $v = if ($tx -match 'Nioh3EngineFix (\d+\.\d+\.\d+)') { $Matches[1] } else { "?" }
    $dq = if ($tx -match 'DQ: main-loop retry quantum .*?\(switch (ON|off)\)') { $Matches[1].ToUpper() } else { $null }
    $pf = if ($tx -match 'P1E7: texture-loop prefetch .*?\(switch (ON|off)\)') { $Matches[1].ToUpper() } else { $null }
    $ma = if ($tx -match 'MA: 25 main-thread locked atomic loads') { "ON" } else { "OFF" }
    @{ ver = $v; dq = $dq; p1e7 = $pf; ma = $ma }
}
function Manifest([hashtable]$h) { Add-Content "$out\manifest.jsonl" ($h | ConvertTo-Json -Compress) }
function Window([string]$name, [hashtable]$meta) {   # a window with user input or without Nioh3 in front is discarded + repeated
    $pmRedo = 0
    $nz = 0   # noisy redos (quiet-machine guard), counted apart from the focus/input discards
    for ($rep = 0; $rep -lt 4; ) {
        if (-not (Game-Alive)) { throw "HARNESS-ABORT: game dead/hung before $name" }
        $null = Assert-NotCapped "before $name"
        Mem-Guard
        Assert-Focus "before window $name"
        $qw = 0
        if ($QuietGuard) { Set-TestStatus "waiting for a quiet machine ($name)"; $qw = Wait-Quiet $name { param($l) T $l } }
        Set-MeasureLock "measuring window $name"
        $csv = "$out\$name.csv"; $off = Log-Len; $w0 = Get-Date; Set-TestStatus "measuring window $name"
        $pm = Start-Process "$ws\tools\PresentMon.exe" -PassThru -WindowStyle Hidden -ArgumentList "--process_name Nioh3.exe --output_file `"$csv`" --timed $WindowSec --terminate_after_timed --session_name Nioh3Abba2 --stop_existing_session --no_console_stats"   # own session name: a foreign PresentMon --stop_existing_session on the default name cannot kill it (run8b: 6 of 15 csvs missing/truncated)
        Register-Child $pm "PresentMon"
        $smp = @(); $end = $w0.AddSeconds($WindowSec + 1)
        if ($QuietGuard) {   # ~2 s foreign-CPU samples for the whole window (our tree + PresentMon + Nioh3 excluded)
            $ex = Get-QuietExclude @($pm.Id)
            while (($left = ($end - (Get-Date)).TotalSeconds) -gt 0.3) { $smp += Measure-Foreign ([Math]::Min(2.0, $left)) $ex }
        } else { Start-Sleep ($WindowSec + 1) }
        $pm.WaitForExit(30000) | Out-Null; Start-Sleep 1
        $csvLines = if (Test-Path $csv) { @(Get-Content $csv -TotalCount 40).Count } else { 0 }
        if ($csvLines -lt 40 -and $pmRedo -lt 2) {   # PresentMon wrote nothing / was cut off (run8b): repeat the window instead of recording NO DATA
            $pmRedo++; T "!! window ${name}: PresentMon csv missing/truncated ($csvLines lines) - redo $pmRedo/2"
            if (Test-Path $csv) { Move-Item $csv "$out\${name}_pmfail$pmRedo.csv" -Force -ErrorAction SilentlyContinue }
            continue
        }
        $fgEnd = Fg-Name; $idle = Idle-Sec; $span = ((Get-Date) - $w0).TotalSeconds
        $bad = @($smp | Where-Object { $_.noisy }); $fmax = 0.0; foreach ($x in $smp) { if ($x.cores -gt $fmax) { $fmax = $x.cores } }
        $worst = $smp | Sort-Object cores -Descending | Select-Object -First 1
        # noisy: >= 2 noisy samples, or one sample above twice the core limit (a lone short blip is tolerated)
        $noisy = $bad.Count -ge 2 -or ($bad.Count -ge 1 -and $fmax -gt 2 * (QG-MaxCores))
        $nzTxt = if ($bad.Count) { "{0}/{1} samples noisy, max {2:N2} cores [{3}]{4}" -f $bad.Count, $smp.Count, $fmax, $worst.top, $(if ($worst.heavy) { " heavy: $($worst.heavy)" } else { "" }) } else { "" }
        if ($vp -or ($fgEnd -eq "Nioh3" -and $idle -ge $span)) {   # vpad: user input never reaches the game; recorded, not discarded
            if ($noisy -and $nz -lt $MaxNoisyRedo) {
                Move-Item $csv "$out\${name}_noisy$nz.csv" -Force -ErrorAction SilentlyContinue
                Manifest @{ name = "${name}_noisy$nz"; kind = "noisy_discard"; window = $name; fgn_max = $fmax; noise = $nzTxt; t = (Get-Date -f HH:mm:ss) }
                T "!! window $name noisy ($nzTxt) - redo"; $nz++; continue
            }
            Set-Content "$out\$name.log" (Log-From $off)
            $meta.focus_end = $fgEnd; $meta.idle_end = [Math]::Round($idle, 1); $meta.repeats = $rep
            if ($vp) { $meta.input = "vpad"; $meta.user_active = ($idle -lt $span) }
            if ($QuietGuard) { $meta.fgn_max = $fmax; $meta.noisy = $noisy; $meta.noise = $nzTxt; $meta.noisy_redos = $nz; $meta.quiet_wait_s = $qw }
            $meta.name = $name; $meta.t = (Get-Date -f HH:mm:ss); Manifest $meta
            if ($noisy) { T "!! window $name kept NOISY after $nz redo(s) ($nzTxt) - the report rejects it" } else { T "window $name done$(if ($fmax -gt 0) { ' (foreign max {0:N2} cores)' -f $fmax })" }
            return
        }
        Move-Item $csv "$out\${name}_discarded$rep.csv" -Force -ErrorAction SilentlyContinue
        T ("!! window $name discarded (foreground at end {0}, input {1:N1}s ago inside the {2:N0}s window) - repeat" -f $fgEnd, $idle, $span)
        $rep++
    }
    throw "HARNESS-ABORT: window $name disturbed 4 times"
}
$script:menu = $null
# Nav keys must never reach the game: F10 alone first, and the rest only after the mod logged 'MENU: opened' (run2: the
# F10 right after a refocus was lost - DirectInput re-acquires on activation - and Up/Enter opened the GAME's menu).
function Send-MenuKeys([string[]]$keys) {
    $k = @($keys); if ($k[0] -ne "79") { throw "menu key list does not start with F10" }
    for ($try = 1; $try -le 2; $try++) {
        Assert-Focus "menu open"; Start-Sleep -Milliseconds 1500   # let DirectInput re-acquire after any focus change
        $off = Log-Len
        UI keyseq "79"
        for ($i = 0; $i -lt 8; $i++) { Start-Sleep -Milliseconds 250; $tx = Log-From $off; if ($tx -match "MENU: opened") { break } }
        if ($tx -match "MENU: opened") {
            $rest = @($k | Select-Object -Skip 1)
            UI keyseq $rest
            return $true
        }
        if ($tx -match "game menu signal now OPEN") { T "!! the game's own menu opened instead of the mod menu"; return $false }
        T "!! F10 not seen by the mod (try $try)"
    }
    return $false
}
function Toggle([string]$id, [string]$want) {   # returns [bool]; confirmed only by the mod's 'MENU: <label> -> <want>' line
    Assert-Focus "before toggle $id"
    $m = $script:menu.items.$id; $off = Log-Len
    $rv = $null
    if ($vp) {   # the mod sets the item on its Present thread (same code + log line as a menu press); no menu navigation
        $r = VP-MenuSet $id $(if ($want -eq "ON") { "1" } else { "0" })
        if (-not $r.ok) { T "!! toggle $id -> ${want}: $($r.reply)"; return $false }
        if ($r.reply -match '\(value (-?\d+)\)') { $rv = [int]$Matches[1] }   # menu::RunRemote re-reads the live switch after the set
    } elseif (-not (Send-MenuKeys @($m.keys))) { T "!! toggle $id -> ${want}: mod menu did not open"; return $false }
    for ($k = 0; $k -lt 12; $k++) {
        Start-Sleep -Milliseconds 500
        $tx = Log-From $off
        if ($tx -match ("MENU: " + [regex]::Escape($m.label) + " -> " + $want)) { Add-Content "$out\menu_lines.txt" "[$(Get-Date -f HH:mm:ss)] $id -> $want"; return $true }
    }
    # run7: AABB = 2 boots ON in ASI 1.7.39 (only ED/WG/DS/CP/SPF/MQ/PT1/DL/DQ/P1E7 honour 2), so 'menu set AABB 1' was a no-op:
    # RunRemote presses only when the state differs, hence no MENU line. The read-back value is the live switch itself.
    if ($null -ne $rv -and (($rv -ne 0) -eq ($want -eq "ON"))) {
        T "toggle $id -> ${want}: no MENU line, switch already $want (read-back '$($r.reply)') - accepted"
        Add-Content "$out\menu_lines.txt" "[$(Get-Date -f HH:mm:ss)] $id -> $want (read-back, no-op)"; return $true
    }
    T "!! toggle $id -> $want NOT confirmed$(if ($vp) { " (reply '$($r.reply)')" })"; return $false
}
function VP-ItemState([string]$id) {   # live switch state via 'menu get' (vpad): "ON" / "OFF" / $null
    $r = VP menu get $id
    if ($r.ok -and $r.reply -match '\(value (-?\d+)\)') { if ([int]$Matches[1] -ne 0) { "ON" } else { "OFF" } } else { T "!! menu get $id failed: $($r.reply)"; $null }
}

# ---- dry run: everything except launching/input --------------------------------------------------------------------
$labels = @{}; $script:bootState = @{}
function Prepare {
    $test = @{ CpuStats = 1; ReportSec = 10; Barriers = 0 }
    if ($MaxFps -gt 0) { $test.MaxFps = $MaxFps }
    foreach ($f in $LaunchFlags) { if ($Live -contains $f) { throw "$f is both -Live and a launch flag" } }
    $base0 = [IO.File]::ReadAllText($baseIni)
    $root = if ($SrcRoot) { $SrcRoot } else { $ws }
    $menuCpp = "$root\mod\Nioh3EngineFix\src\menu.cpp"; $matrix = "$root\tools\testmatrix\matrix.json"
    if (-not (Test-Path $menuCpp)) { throw "no menu.cpp under -SrcRoot $root" }
    if (-not (Test-Path $matrix)) { $matrix = "$here\matrix.json" }
    $mx = @{}; foreach ($it in (Get-Content $matrix -Raw | ConvertFrom-Json).items) { $mx[$it.id] = $it }
    $two = @{}   # items whose ASI honours 2 = installed with the switch off (matrix live_installed_value)
    foreach ($i in $Live) { if ($mx[$i] -and $mx[$i].live_installed_value -eq "2") { $two[$i] = $true } }
    foreach ($i in $Live) {   # not installed in the base ini -> installed for this run: switched off when the ASI supports it
        if (-not ($base0 -match "(?m)^$i\s*=\s*[12]")) { $test[$i] = $(if ($two[$i]) { 2 } else { 1 }) }
    }
    $off = @{}; foreach ($f in @("DQ", "P1E7") + $LaunchFlags) { if ($Live -notcontains $f) { $off[$f] = 0 } }
    $tmpIni = "$out\test_base.ini"; [IO.File]::WriteAllText($tmpIni, (Make-IniText ($test + $off)), (New-Object Text.UTF8Encoding($false)))
    py -I "$here\menu_map.py" --ini $tmpIni --no-batch --device pc --menu-cpp $menuCpp --matrix $matrix -o "$out\menu_map.json" | Out-Null
    if ($LASTEXITCODE -ne 0) { throw "menu_map.py failed" }
    $script:menu = Get-Content "$out\menu_map.json" -Raw | ConvertFrom-Json
    $bt = [IO.File]::ReadAllText($tmpIni)
    foreach ($i in $Live) {   # boot state from the ini (1 = installed+ON, 2 = installed+OFF where supported, 0 = not installed)
        if (-not $script:menu.items.$i) { throw "no menu row for $i (installed ASI newer than $($root)? pass -SrcRoot)" }; $labels[$i] = $script:menu.items.$i.label
        $v = if ($bt -match "(?m)^$i\s*=\s*(\d)") { $Matches[1] } else { "0" }
        if ($v -eq "0") { throw "$i = 0 in the base ini: not installed, cannot toggle live" }
        $script:bootState[$i] = $(if ($v -eq "2" -and $two[$i]) { "OFF" } else { "ON" })   # 2 on an item without 2-support = ON
    }
    return $test
}
$test = Prepare
if ($DryRun) {
    $kv = $test.Clone(); foreach ($f in $LaunchFlags) { $kv[$f] = $(if ($f -eq $LaunchFlags[0]) { 1 } else { 0 }) }
    $txt = Make-IniText $kv
    "DryRun OK: out=$out input=$InputMode"
    if ($vp) { "  vpad ini: " + (("VirtualPad", "KeepRunningUnfocused", "AutoContinueLoad") | ForEach-Object { if ($txt -match "(?m)^$_ = (.*)$") { "$_=$($Matches[1].Trim())" } else { "$_ MISSING" } }) -join " " }
    "  ini: " + (($kv.Keys | Sort-Object | ForEach-Object { $k = $_; if ($txt -match "(?m)^$k = (.*)$") { "$k=$($Matches[1].Trim())" } }) -join " ")
    "  menu rows: " + (($Live | ForEach-Object { "$_ '$($labels[$_])' boot=$($script:bootState[$_]) keys=$(@($script:menu.items.$_.keys).Count)" }) -join "; ")
    $flat = @(); foreach ($a in @("keyseq", @($script:menu.items.MQ.keys))) { foreach ($x in @($a)) { $flat += [string]$x } }
    "  UI arg flattening: $($flat.Count) args (first '$($flat[0])', second '$($flat[1])')"
    $c = [IO.File]::ReadAllText($cfg); "  config.xml now: " + (("FRAME_GENERATION", "REN_RAT", "RESOLUTION_SCALING" | ForEach-Object { if ($c -match "<$_>([^<]+)<") { "$_=$($Matches[1])" } }) -join " ") + " -> test: FRAME_GENERATION=OFF REN_RAT=$RenRat RESOLUTION_SCALING=$Upscaler"
    "  foreground now: $(Fg-Name)"
    $da = if ($Arms.Count) { $Arms } elseif ($LaunchFlags.Count) { @("BASE") + $LaunchFlags + $LaunchFlags[($LaunchFlags.Count - 1)..0] + @("BASE") } else { @("BASE") }
    "  quiet guard: " + $(if ($QuietGuard) { $m = Measure-Foreign 2.0; "on (max {0} cores, heavy {1} cores, redo {2}); now foreign {3:N2} cores [{4}] -> {5}" -f (QG-MaxCores), (QG-HeavyCores), $MaxNoisyRedo, $m.cores, $m.top, $(if ($m.noisy) { "NOISY" } else { "quiet" }) } else { "off" })
    "  lock file: $script:MeasureLock (now: $(if (Test-MeasureLock) { 'held by pid ' + (Test-MeasureLock).pid } else { 'free' }))"
    $dok = Ensure-DisplayRefresh $NeedHz { param($m) Write-Host "  $m" } -NoForce
    "  display check (read-only): " + $(if ($dok) { "OK (>= $NeedHz Hz)" } else { "a real run would $(if ($NoForceRefresh) { 'REFUSE' } else { 'raise the refresh for the run' }): $script:DisplayWhy" })
    "  arms: " + (($da | ForEach-Object -Begin { $n = 0 } -Process { $n++; "L$n $_" + $(if ($n -eq 1) { "(+live $($Live -join ','))" } else { "" }) }) -join " ") + $(if ($MaxFps -gt 0) { "  [VRR] MaxFps $MaxFps" } else { "" })
    Remove-Item $out -Recurse -Force
    return
}

Init-Guard -Name "abba2" -OwnerPid $OwnerPid -MaxMinutes $MaxMinutes -Cleanup { Close-GameClean; Restore-All; Stop-TestPopup }
Start-TestPopup -Name "abba2 flag A/B" -InputMode $InputMode   # status popup; input blocking only if the user clicks it
Set-MeasureLock "starting"   # %TEMP%\nioh3_measuring.lock until Done-Guard / Guard-Fail
T "abba2 start: input=$InputMode out=$out live=$($Live -join ',') launch=$($LaunchFlags -join ',') window=${WindowSec}s quads=$Quads cfg: FG OFF REN_RAT $RenRat $Upscaler"
$LaunchFlags = @($LaunchFlags | Where-Object { $_ -and $_ -ne "none" }); $Live = @($Live | Where-Object { $_ -and $_ -ne "none" })   # -LaunchFlags none = live items only (one launch)
$arms = if ($Arms.Count) { $Arms } elseif ($LaunchFlags.Count) { @("BASE") + $LaunchFlags + $LaunchFlags[($LaunchFlags.Count - 1)..0] + @("BASE") } else { @("BASE") }
try {
    Set-TestCfg
    if (-not (Ensure-DisplayRefresh $NeedHz { param($m) T $m } -NoForce:$NoForceRefresh)) { throw "HARNESS-ABORT: refusing to measure: $script:DisplayWhy" }
    $L = 0
    foreach ($arm in $arms) {
        $L++; Renew
        $kv = $test.Clone(); foreach ($f in $LaunchFlags) { $kv[$f] = $(if ($f -eq $arm) { 1 } else { 0 }) }
        Set-TestIni $kv
        T "launch L$L $arm"; Set-TestStatus "launch L$L of $($arms.Count) ($arm)"
        if (-not (Start-Play)) { T "boot failed L$L - abort (no relaunch loop)"; break }
        $bi = Boot-Info; T "L$L boot: ver $($bi.ver) DQ $($bi.dq) P1E7 $($bi.p1e7) MA $($bi.ma)"
        if ($L -eq 1 -and $vp -and $Live.Count -gt 0) {   # read the real boot state back; live items that boot ON go OFF so the BASE windows are stock
            foreach ($i in $Live) {
                $rs = VP-ItemState $i
                if ($rs -and $rs -ne $script:bootState[$i]) { T "!! $i boots $rs (ini expectation $($script:bootState[$i]))"; $script:bootState[$i] = $rs }
                if ($script:bootState[$i] -eq "ON") { if (Toggle $i "OFF") { $script:bootState[$i] = "OFF" } else { T "!! $i stays ON for the BASE windows" } }
            }
            T ("L1 live boot state: " + (($Live | ForEach-Object { "$_ $($script:bootState[$_])" }) -join ", "))
        }
        T "L$L warm-up 60 s (no input)"; Start-Sleep 60
        $vc = Assert-NotCapped "L$L after warm-up"; if ($vc) { T "L$L cap check OK: vrr tier $($vc.tier) cap $($vc.cap) (need >= $NeedHz)" }
        for ($s = 1; $s -le 3; $s++) {
            Window "L${L}_${arm}_s$s" @{ kind = "launch"; launch = $L; arm = $arm; state = $(if ($arm -eq "BASE") { "off" } else { "on" }); boot_dq = $bi.dq; boot_p1e7 = $bi.p1e7; boot_ma = $bi.ma; ver = $bi.ver }
        }
        if ($L -eq 1 -and $Live.Count -gt 0) {
            $script:liveOff = Log-Len
            if (-not $vp) {   # vpad sets items by key: no menu cursor to initialise
                if (-not (Send-MenuKeys @($script:menu.init))) { throw "HARNESS-ABORT: mod menu did not open for init" }; Start-Sleep 1
                $tx = Log-From $script:liveOff; if ($tx -notmatch "MENU: closed") { throw "HARNESS-ABORT: menu init did not close the menu" }
            }
            $state = @{}; foreach ($i in $Live) { $state[$i] = $script:bootState[$i] }
            foreach ($item in $Live) {
                if ($state[$item] -ne "ON") { if (-not (Toggle $item "ON")) { T "!! $item cannot be switched ON - item skipped"; continue }; $state[$item] = "ON" }
                $nq = if ($item -eq "B") { $QuadsB } else { $Quads }
                $broken = $false
                for ($q = 1; $q -le $nq -and -not $broken; $q++) {
                    $pos = 0
                    foreach ($s in "on", "off", "off", "on") {
                        $pos++; $want = $s.ToUpper(); $ok = $true
                        if ($state[$item] -ne $want) { $ok = Toggle $item $want; if ($ok) { $state[$item] = $want } else { $broken = $true; break } }
                        Start-Sleep 4
                        Window "L1_${item}_q${q}_${pos}_$s" @{ kind = "live"; launch = 1; arm = "BASE"; item = $item; quad = $q; pos = $pos; state = $s; toggle_ok = $ok }
                        Renew
                    }
                }
                if ($state[$item] -ne "ON") { if (Toggle $item "ON") { $state[$item] = "ON" } else { T "!! $item left $($state[$item]) - stopping the live block"; break } }
            }
        }
        Close-GameClean
    }
} catch {
    T "harness aborted: $($_.Exception.Message)"
    if ($_.Exception.Message -match 'dead/hung') { Dead-Report }
} finally {
    Close-GameClean
    Restore-All
    Stop-TestPopup
    Done-Guard
    T "done; analysis: py -I $here\abba2_report.py $out"
}
