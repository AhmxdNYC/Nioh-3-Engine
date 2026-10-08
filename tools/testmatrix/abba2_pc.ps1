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
param([int]$OwnerPid = 0, [int]$MaxMinutes = 120, [int]$WindowSec = 20, [int]$Quads = 3, [int]$QuadsB = 2,
      [string[]]$Live = @("B", "PP", "GF", "MQ"), [string[]]$LaunchFlags = @("DQ", "P1E7"), [switch]$DryRun, [string]$RunName = "",
      [int]$RenRat = 50, [string]$Upscaler = "FSR", [int]$IdleNeed = 120, [int]$IdleWaitMin = 30,
      [Alias("Input")][ValidateSet("ui", "vpad")][string]$InputMode = "ui")
$vp = $InputMode -eq "vpad"
$ErrorActionPreference = "Continue"
$Live = @($Live -split "," | ForEach-Object { $_.Trim() } | Where-Object { $_ -and $_ -ne "none" }); $LaunchFlags = @($LaunchFlags -split "," | ForEach-Object { $_.Trim() } | Where-Object { $_ -and $_ -ne "none" })   # -File passes "B,GF" as one string
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
. "C:\Users\Ahmad Hamza\nioh3-engine\patcher\common.ps1"
$GameDir = "C:\Program Files (x86)\Steam\steamapps\common\Nioh3"; $ini = "$GameDir\Nioh3EngineFix.ini"; $logf = "$GameDir\Nioh3EngineFix.log"
$baseIni = "$GameDir\Nioh3EngineFix.ini.preboss.bak"
if (-not $RunName) { $RunName = "run_" + (Get-Date -f yyyyMMdd_HHmm) }
$out = "$ws\captures\tm\abba2\$RunName"; New-Item -ItemType Directory -Force $out | Out-Null
$uoBridge = "C:\Users\Ahmad Hamza\universal-optimizer\agent-bridge"
$env:UO_BRIDGE_TOKEN_FILE = "$env:APPDATA\uo-bridge\token"; $env:UO_AGENT_ID = "pc"
$cfgBak = "$cfg.abba2.bak"
$script:gamePid = 0
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
}
function Restore-All {
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
    Start-Process "steam://rungameid/3681010"
    $p = $null
    while (((Get-Date) - $t0).TotalSeconds -lt 180) { $p = Get-Process Nioh3 -ErrorAction SilentlyContinue | Where-Object MainWindowHandle -ne 0 | Select-Object -First 1; if ($p) { break }; Start-Sleep 3 }
    if (-not $p) { Note-Crash "no game window after launch"; return $false }
    $script:gamePid = $p.Id; Register-Game
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
    Start-Process "steam://rungameid/3681010"
    $p = $null
    while (((Get-Date) - $t0).TotalSeconds -lt 180) { $p = Get-Process Nioh3 -ErrorAction SilentlyContinue | Where-Object MainWindowHandle -ne 0 | Select-Object -First 1; if ($p) { break }; Start-Sleep 3 }
    if (-not $p) { Note-Crash "no game window after launch"; return $false }
    $script:gamePid = $p.Id; Register-Game
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
    if (-not $p -or $p.HasExited -or -not $p.Responding) { return $false }
    return (((Get-Date) - (Get-Item $logf).LastWriteTime).TotalSeconds -lt 45)
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
    @{ ver = $v; dq = $dq; p1e7 = $pf }
}
function Manifest([hashtable]$h) { Add-Content "$out\manifest.jsonl" ($h | ConvertTo-Json -Compress) }
function Window([string]$name, [hashtable]$meta) {   # a window with user input or without Nioh3 in front is discarded + repeated
    for ($rep = 0; $rep -lt 4; $rep++) {
        if (-not (Game-Alive)) { throw "HARNESS-ABORT: game dead/hung before $name" }
        Mem-Guard
        Assert-Focus "before window $name"
        $csv = "$out\$name.csv"; $off = Log-Len; $w0 = Get-Date
        $pm = Start-Process "$ws\tools\PresentMon.exe" -PassThru -WindowStyle Hidden -ArgumentList "--process_name Nioh3.exe --output_file `"$csv`" --timed $WindowSec --terminate_after_timed --stop_existing_session --no_console_stats"
        Register-Child $pm "PresentMon"
        Start-Sleep ($WindowSec + 1); $pm.WaitForExit(30000) | Out-Null; Start-Sleep 1
        $fgEnd = Fg-Name; $idle = Idle-Sec; $span = ((Get-Date) - $w0).TotalSeconds
        if ($vp -or ($fgEnd -eq "Nioh3" -and $idle -ge $span)) {   # vpad: user input never reaches the game; recorded, not discarded
            Set-Content "$out\$name.log" (Log-From $off)
            $meta.focus_end = $fgEnd; $meta.idle_end = [Math]::Round($idle, 1); $meta.repeats = $rep
            if ($vp) { $meta.input = "vpad"; $meta.user_active = ($idle -lt $span) }
            $meta.name = $name; $meta.t = (Get-Date -f HH:mm:ss); Manifest $meta
            T "window $name done"; return
        }
        Move-Item $csv "$out\${name}_discarded$rep.csv" -Force -ErrorAction SilentlyContinue
        T ("!! window $name discarded (foreground at end {0}, input {1:N1}s ago inside the {2:N0}s window) - repeat" -f $fgEnd, $idle, $span)
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
    if ($vp) {   # the mod sets the item on its Present thread (same code + log line as a menu press); no menu navigation
        $r = VP-MenuSet $id $(if ($want -eq "ON") { "1" } else { "0" })
        if (-not $r.ok) { T "!! toggle $id -> ${want}: $($r.reply)"; return $false }
    } elseif (-not (Send-MenuKeys @($m.keys))) { T "!! toggle $id -> ${want}: mod menu did not open"; return $false }
    for ($k = 0; $k -lt 12; $k++) {
        Start-Sleep -Milliseconds 500
        $tx = Log-From $off
        if ($tx -match ("MENU: " + [regex]::Escape($m.label) + " -> " + $want)) { Add-Content "$out\menu_lines.txt" "[$(Get-Date -f HH:mm:ss)] $id -> $want"; return $true }
    }
    T "!! toggle $id -> $want NOT confirmed"; return $false
}

# ---- dry run: everything except launching/input --------------------------------------------------------------------
$labels = @{}; $script:bootState = @{}
function Prepare {
    $test = @{ CpuStats = 1; ReportSec = 10; Barriers = 0 }
    $tmpIni = "$out\test_base.ini"; [IO.File]::WriteAllText($tmpIni, (Make-IniText ($test + @{ DQ = 0; P1E7 = 0 })), (New-Object Text.UTF8Encoding($false)))
    py -I "$here\menu_map.py" --ini $tmpIni --no-batch --device pc -o "$out\menu_map.json" | Out-Null
    if ($LASTEXITCODE -ne 0) { throw "menu_map.py failed" }
    $script:menu = Get-Content "$out\menu_map.json" -Raw | ConvertFrom-Json
    $bt = [IO.File]::ReadAllText($tmpIni)
    foreach ($i in $Live) {   # boot state from the ini (1 = installed+ON, 2 = installed+OFF, 0 = not installed: cannot be switched live)
        if (-not $script:menu.items.$i) { throw "no menu row for $i" }; $labels[$i] = $script:menu.items.$i.label
        $v = if ($bt -match "(?m)^$i\s*=\s*(\d)") { $Matches[1] } else { "0" }
        if ($v -eq "0") { throw "$i = 0 in the base ini: not installed, cannot toggle live" }
        $script:bootState[$i] = $(if ($v -eq "2") { "OFF" } else { "ON" })
    }
    return $test
}
$test = Prepare
if ($DryRun) {
    $kv = $test + @{ DQ = 1; P1E7 = 0 }
    $txt = Make-IniText $kv
    "DryRun OK: out=$out input=$InputMode"
    if ($vp) { "  vpad ini: " + (("VirtualPad", "KeepRunningUnfocused", "AutoContinueLoad") | ForEach-Object { if ($txt -match "(?m)^$_ = (.*)$") { "$_=$($Matches[1].Trim())" } else { "$_ MISSING" } }) -join " " }
    "  ini: " + (($kv.Keys | Sort-Object | ForEach-Object { $k = $_; if ($txt -match "(?m)^$k = (.*)$") { "$k=$($Matches[1].Trim())" } }) -join " ")
    "  menu rows: " + (($Live | ForEach-Object { "$_ '$($labels[$_])' boot=$($script:bootState[$_]) keys=$(@($script:menu.items.$_.keys).Count)" }) -join "; ")
    $flat = @(); foreach ($a in @("keyseq", @($script:menu.items.MQ.keys))) { foreach ($x in @($a)) { $flat += [string]$x } }
    "  UI arg flattening: $($flat.Count) args (first '$($flat[0])', second '$($flat[1])')"
    $c = [IO.File]::ReadAllText($cfg); "  config.xml now: " + (("FRAME_GENERATION", "REN_RAT", "RESOLUTION_SCALING" | ForEach-Object { if ($c -match "<$_>([^<]+)<") { "$_=$($Matches[1])" } }) -join " ") + " -> test: FRAME_GENERATION=OFF REN_RAT=$RenRat RESOLUTION_SCALING=$Upscaler"
    "  foreground now: $(Fg-Name)"
    "  arms: L1 BASE(+live $($Live -join ',')) " + (($LaunchFlags + ($LaunchFlags[($LaunchFlags.Count-1)..0]) | ForEach-Object -Begin { $n = 1 } -Process { $n++; "L$n $_" }) -join " ") + " L$($LaunchFlags.Count * 2 + 2) BASE"
    Remove-Item $out -Recurse -Force
    return
}

Init-Guard -Name "abba2" -OwnerPid $OwnerPid -MaxMinutes $MaxMinutes -Cleanup { Close-GameClean; Restore-All }
T "abba2 start: input=$InputMode out=$out live=$($Live -join ',') launch=$($LaunchFlags -join ',') window=${WindowSec}s quads=$Quads cfg: FG OFF REN_RAT $RenRat $Upscaler"
$LaunchFlags = @($LaunchFlags | Where-Object { $_ -and $_ -ne "none" }); $Live = @($Live | Where-Object { $_ -and $_ -ne "none" })   # -LaunchFlags none = live items only (one launch)
$arms = if ($LaunchFlags.Count) { @("BASE") + $LaunchFlags + $LaunchFlags[($LaunchFlags.Count - 1)..0] + @("BASE") } else { @("BASE") }
try {
    Set-TestCfg
    $L = 0
    foreach ($arm in $arms) {
        $L++; Renew
        $kv = $test.Clone(); foreach ($f in $LaunchFlags) { $kv[$f] = $(if ($f -eq $arm) { 1 } else { 0 }) }
        Set-TestIni $kv
        T "launch L$L $arm"
        if (-not (Start-Play)) { T "boot failed L$L - abort (no relaunch loop)"; break }
        $bi = Boot-Info; T "L$L boot: ver $($bi.ver) DQ $($bi.dq) P1E7 $($bi.p1e7)"
        T "L$L warm-up 60 s (no input)"; Start-Sleep 60
        for ($s = 1; $s -le 3; $s++) {
            Window "L${L}_${arm}_s$s" @{ kind = "launch"; launch = $L; arm = $arm; state = $(if ($arm -eq "BASE") { "off" } else { "on" }); boot_dq = $bi.dq; boot_p1e7 = $bi.p1e7; ver = $bi.ver }
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
} finally {
    Close-GameClean
    Restore-All
    Done-Guard
    T "done; analysis: py -I $here\abba2_report.py $out"
}
