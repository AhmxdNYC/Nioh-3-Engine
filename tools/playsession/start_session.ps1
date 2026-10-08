<#
  start_session.ps1 - click "Nioh3 Session START" BEFORE launching the game (tools\playsession\README.md).
  1. refuses if a session is already running, if Nioh3.exe is running, or if another device (the Ally) holds the game lock
  2. backs up the save folder (%LOCALAPPDATA%\KoeiTecmo\NIOH3) to D:\nioh3-re\save-backups\<stamp>-session
  3. takes the bridge game lock for this PC (monitor.ps1 renews it while the session runs)
  4. keeps a copy of the game folder's Nioh3EngineFix.ini + .asi, installs the current build (mod\Nioh3EngineFix\bin) and writes the
     user's own play ini + measurement-only capture keys (session_lib.ps1 $CaptureKeys; AutoContinueLoad etc. stay as they were)
  5. starts PresentMon (waits for Nioh3.exe; every frame, v2 metrics incl. CPU/GPU busy+wait and input latency) and monitor.ps1
  6. prints the checklist. No input is sent to the game, ever.
  Test switches (tools\playsession\test_session.ps1): -NoLock, -NoPause, -GameDir/-SaveRoot/-BackupRoot/-CaptureRoot/-StateDir.
#>
param([switch]$NoPause, [switch]$NoLock, [string]$GameDir = "", [string]$SaveRoot = "", [string]$BackupRoot = "", [string]$CaptureRoot = "", [string]$StateDir = "")
$ErrorActionPreference = "Stop"
. (Join-Path $PSScriptRoot "session_lib.ps1")
$Host.UI.RawUI.WindowTitle = "Nioh 3 play session - START"
function Say([string]$s, [string]$c = "Gray") { Write-Host $s -ForegroundColor $c }
function Fail([string]$s) { Say ""; Say "NOT STARTED: $s" Red; Pause-End -NoPause:$NoPause; exit 1 }

Say "Nioh 3 play session - START" Cyan
Say "--------------------------------"
New-Item -ItemType Directory -Force $StateDir | Out-Null
if (Test-Path $ActiveFile) {
    $a = Get-Content $ActiveFile -Raw | ConvertFrom-Json
    Fail "a session is already running (started $($a.start)). Click 'Nioh3 Session STOP' first."
}
if (Get-Process -Name Nioh3 -ErrorAction SilentlyContinue) { Fail "Nioh 3 is running. Quit the game to the desktop, then click START again." }
if (-not (Test-Path $BinAsi)) { Fail "no mod build at $BinAsi" }
if (-not (Test-Path $PmExe)) { Fail "PresentMon is missing ($PmExe)" }
if (-not (Test-Path $GameDir)) { Fail "game folder not found: $GameDir" }
$asiNew = Get-AsiVersion $BinAsi

# ---- game lock ----
$lockHeld = $false
if (-not $NoLock) {
    $h = Get-LockHolder
    if ($h.ok -and $h.holder -and $h.holder -ne "pc") { Fail "the game is in use on '$($h.holder)' (bridge game lock). Close it there first, then click START again." }
    $r = Invoke-GameLock "acquire" 3600
    if ($r.code -eq 1) { Fail "the game lock is held by another device: $($r.out)" }
    if ($r.code -eq 0) { $lockHeld = $true; Say "game lock: taken for this PC" }
    else { Say "game lock: bridge not reachable ($($r.out)) - continuing without it" Yellow }
}

$stamp = Get-Date -Format "yyyyMMdd-HHmm"
$sess = Join-Path $CaptureRoot "$stamp-session"
if (Test-Path $sess) { $sess = Join-Path $CaptureRoot ("$stamp-session-" + (Get-Date -Format "ss")) }
$orig = Join-Path $sess "orig"
$asi = Join-Path $GameDir "Nioh3EngineFix.asi"; $ini = Join-Path $GameDir "Nioh3EngineFix.ini"
$changed = $false; $pm = $null; $mon = $null
try {
    New-Item -ItemType Directory -Force $orig | Out-Null
    # ---- save backup ----
    $bk = Join-Path $BackupRoot "$stamp-session"
    if (Test-Path $SaveRoot) {
        New-Item -ItemType Directory -Force $bk | Out-Null
        Copy-Item -Path (Join-Path $SaveRoot "*") -Destination $bk -Recurse -Force
        $src = Get-ChildItem $SaveRoot -Recurse -File | Measure-Object Length -Sum
        $dst = Get-ChildItem $bk -Recurse -File | Measure-Object Length -Sum
        if ($src.Count -ne $dst.Count -or $src.Sum -ne $dst.Sum) { throw "save backup incomplete ($($dst.Count)/$($src.Count) files)" }
        Say ("save backup: {0} ({1} files, {2:N0} MB)" -f $bk, $dst.Count, ($dst.Sum / 1MB))
    } else { throw "save folder not found: $SaveRoot" }

    # ---- mod files: keep the user's, install the build + capture keys ----
    $asiBefore = Get-AsiVersion $asi
    $hadAsi = Test-Path $asi; $hadIni = Test-Path $ini
    if ($hadAsi) { Copy-Item $asi (Join-Path $orig "Nioh3EngineFix.asi") -Force }
    if ($hadIni) { Copy-Item $ini (Join-Path $orig "Nioh3EngineFix.ini") -Force }
    $baseIni = if ($hadIni) { $ini } else { Join-Path $ws "mod\Nioh3EngineFix\Nioh3EngineFix.ini" }
    $lines = [IO.File]::ReadAllLines($baseIni)
    $keys = @()
    foreach ($k in $CaptureKeys) { $lines = Set-IniValue $lines $k[0] $k[1] $k[2]; $keys += "[$($k[0])] $($k[1])=$($k[2])" }
    $changed = $true
    [IO.File]::WriteAllLines($ini, $lines, (New-Object Text.UTF8Encoding($false)))
    Copy-Item $BinAsi $asi -Force
    Copy-Item $ini (Join-Path $sess "ini_used.ini") -Force
    Say "mod: $asiBefore -> $asiNew installed for the session; your ini + $($keys.Count) capture keys"

    # ---- game settings snapshot ----
    $cfg = Join-Path $SaveRoot "config.xml"; $gfx = ""
    if (Test-Path $cfg) {
        Copy-Item $cfg (Join-Path $sess "config.xml") -Force
        $x = Get-Content $cfg -Raw
        $gfx = (@("SCR_REZ", "REN_RAT", "REN_FPS", "PRESET", "VSYNC", "RESOLUTION_SCALING", "FRAME_GENERATION", "HDR") | ForEach-Object {
            $m = [regex]::Match($x, "<$_>([^<]*)</$_>"); if ($m.Success) { "$_=$($m.Groups[1].Value)" } }) -join ", "
    }

    # ---- PresentMon (waits for Nioh3.exe; follows relaunches) ----
    $pmCsv = Join-Path $sess "presentmon.csv"
    $pmArgs = "--process_name Nioh3.exe --output_file `"$pmCsv`" --v2_metrics --date_time --track_pc_latency --stop_existing_session --session_name $PmSession --no_console_stats"
    $pm = Start-Process $PmExe -ArgumentList $pmArgs -WindowStyle Hidden -PassThru
    Start-Sleep -Seconds 2
    if ($pm.HasExited) { throw "PresentMon exited at once (code $($pm.ExitCode))" }
    Say "PresentMon: recording (pid $($pm.Id)) -> presentmon.csv"

    # ---- monitor ----
    $monArgs = "-NoProfile -ExecutionPolicy Bypass -File `"$(Join-Path $PSScriptRoot 'monitor.ps1')`" -SessionDir `"$sess`" -GameDir `"$GameDir`"" + $(if ($lockHeld) { " -RenewLock" } else { "" })
    $mon = Start-Process powershell -ArgumentList $monArgs -WindowStyle Hidden -PassThru
    Say "monitor: running (pid $($mon.Id)) -> sysmon.csv, events.txt, mod log copies"

    $now = Get-Date
    $bias = [math]::Round(($now - $now.ToUniversalTime()).TotalSeconds)
    $info = [ordered]@{ start = $now.ToString("s"); start_ts = [double]([DateTimeOffset]$now).ToUnixTimeMilliseconds() / 1000; session_dir = $sess; save_backup = $bk
        game_dir = $GameDir; had_asi = $hadAsi; had_ini = $hadIni; asi_version = $asiNew; asi_version_before = $asiBefore; capture_keys = $keys; graphics = $gfx
        pm_pid = $pm.Id; mon_pid = $mon.Id; pm_bias_s = $bias; lock_held = $lockHeld; save_root = $SaveRoot; state_dir = $StateDir }
    Write-Json $info (Join-Path $sess "session.json")
    Write-Json $info $ActiveFile
} catch {
    Say "!! $_" Red
    if ($pm -and -not $pm.HasExited) { & $PmExe --terminate_existing_session --session_name $PmSession 2>$null | Out-Null; Stop-Process -Id $pm.Id -Force -ErrorAction SilentlyContinue }
    if ($mon -and -not $mon.HasExited) { Stop-Process -Id $mon.Id -Force -ErrorAction SilentlyContinue }
    if ($changed) {
        if (Test-Path (Join-Path $orig "Nioh3EngineFix.ini")) { Copy-Item (Join-Path $orig "Nioh3EngineFix.ini") $ini -Force } elseif (-not $hadIni) { Remove-Item $ini -Force -ErrorAction SilentlyContinue }
        if (Test-Path (Join-Path $orig "Nioh3EngineFix.asi")) { Copy-Item (Join-Path $orig "Nioh3EngineFix.asi") $asi -Force } elseif (-not $hadAsi) { Remove-Item $asi -Force -ErrorAction SilentlyContinue }
        Say "mod files put back as they were"
    }
    if ($lockHeld) { $null = Invoke-GameLock "release" }
    Fail "setup failed; nothing is left changed."
}

Say ""
Say "READY. Capturing: every frame (fps, lows, frame times, CPU/GPU busy, input latency), loads, boss fight, memory, PSO stutter, FnProbe, controller latency (PadLatency)." Green
if ($gfx) { Say "Game settings now: $gfx" }
Say ""
Say "What to do:" Cyan
Say "  1. Start Nioh 3 from Steam as usual (don't play on the Ally meanwhile)."
Say "  2. Load your save and play - the boss mission, or anything. Nothing to press for the capture."
Say "  3. When done: quit to the title screen, then exit the game to the desktop."
Say "  4. Double-click 'Nioh3 Session STOP' on the desktop. It puts your mod files back and writes the report."
Say ""
Say "If the game crashes, just do step 4. Session folder: $sess"
Pause-End -NoPause:$NoPause
exit 0
