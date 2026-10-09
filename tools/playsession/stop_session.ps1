<#
  stop_session.ps1 - click "Nioh3 Session STOP" AFTER quitting the game (tools\playsession\README.md).
  Stops PresentMon and monitor.ps1, keeps the mod log of every launch, puts the game folder's Nioh3EngineFix.ini + .asi back exactly as
  they were before START (byte-checked), releases the bridge game lock, then runs summarize.py -> report.md in the session folder.
  Refuses (changes nothing) while Nioh3.exe is still running. Test switches: -NoPause, -StateDir.
#>
param([switch]$NoPause, [string]$StateDir = "", [string]$StashRoot = "", [string]$OptiSrc = "")
$ErrorActionPreference = "Stop"
. (Join-Path $PSScriptRoot "session_lib.ps1")
$Host.UI.RawUI.WindowTitle = "Nioh 3 play session - STOP"
function Say([string]$s, [string]$c = "Gray") { Write-Host $s -ForegroundColor $c }

Say "Nioh 3 play session - STOP" Cyan
Say "--------------------------------"
function Report-Restore($rr, [string]$dir) {
    foreach ($n in $rr.restored) { Say "   restored $n (sha256 verified)" }
    foreach ($n in $rr.already) { Say "   already in place $n" }
    if ($rr.problems) { foreach ($p in $rr.problems) { Say "   PROBLEM: $p" Red }; Say "Stash kept (not marked restored): $dir" Red }
    else { Say ("STOCK stash restored: {0} file(s) put back, {1} already in place; the game folder is as before START" -f $rr.restored.Count, $rr.already.Count) Green }
}
if (-not (Test-Path $ActiveFile)) {
    $lo = @(Get-LeftoverStash)
    if ($lo.Count) {   # crash / reboot / lost state: the stock stash is still restorable from its manifest
        if (Get-Process -Name Nioh3 -ErrorAction SilentlyContinue) { Say "Nioh 3 is running. Exit it, then click STOP again to restore the stock stash." Red; Pause-End -NoPause:$NoPause; exit 1 }
        $bad = 0
        foreach ($s in $lo) { Say "Unfinished STOCK stash found: $($s.FullName)" Yellow; $rr = Invoke-StockRestore $s.FullName; Report-Restore $rr $s.FullName; if ($rr.problems) { $bad = 1 } }
        Pause-End -NoPause:$NoPause; exit $bad
    }
    Say "No session is running (nothing to stop)." Yellow
    $last = Get-ChildItem $CaptureRoot -Directory -ErrorAction SilentlyContinue | Sort-Object LastWriteTime -Descending | Select-Object -First 1
    if ($last -and (Test-Path (Join-Path $last.FullName "report.md"))) { Say "Last report: $(Join-Path $last.FullName 'report.md')" }
    Pause-End -NoPause:$NoPause; exit 0
}
$A = Get-Content $ActiveFile -Raw | ConvertFrom-Json
if (Get-Process -Name Nioh3 -ErrorAction SilentlyContinue) {
    Say "Nioh 3 is still running. Quit to the title screen, exit the game to the desktop, then click STOP again." Red
    Say "(Nothing was changed; the capture keeps running.)"
    Pause-End -NoPause:$NoPause; exit 1
}
$sess = $A.session_dir; $GameDir = $A.game_dir
$orig = Join-Path $sess "orig"
$asi = Join-Path $GameDir "Nioh3EngineFix.asi"; $ini = Join-Path $GameDir "Nioh3EngineFix.ini"
$problems = @()

# ---- stop the capture ----
try { & $PmExe --terminate_existing_session --session_name $PmSession 2>$null | Out-Null } catch {}
$pm = Get-Process -Id $A.pm_pid -ErrorAction SilentlyContinue
if ($pm -and $pm.ProcessName -like "PresentMon*") { if (-not $pm.WaitForExit(8000)) { Stop-Process -Id $pm.Id -Force -ErrorAction SilentlyContinue } }
Set-Content (Join-Path $sess "stop.flag") "stop" -Encoding ASCII
$mon = Get-Process -Id $A.mon_pid -ErrorAction SilentlyContinue
if ($mon -and $mon.ProcessName -like "powershell*") { if (-not $mon.WaitForExit(20000)) { Stop-Process -Id $mon.Id -Force -ErrorAction SilentlyContinue } }
Say "capture stopped (PresentMon, monitor)"

# ---- last mod log (monitor copies one per game exit; keep the current one if it is newer and different) ----
$isStock = ($A.mode -eq "stock")
$cur = Join-Path $GameDir "Nioh3EngineFix.log"
$copies = @(Get-ChildItem $sess -Filter "modlog_run*.log" -ErrorAction SilentlyContinue | Sort-Object Name)
if ($isStock) { Say "stock session: no mod log expected" }
elseif (Test-Path $cur) {
    $startT = [datetime]$A.start
    $isNew = (Get-Item $cur).LastWriteTime -ge $startT
    $same = $false
    foreach ($c in $copies) { if ((Get-FileHash $c.FullName).Hash -eq (Get-FileHash $cur).Hash) { $same = $true } }
    if ($isNew -and -not $same) { Copy-Item $cur (Join-Path $sess ("modlog_run{0}.log" -f ($copies.Count + 1))) -Force; Say "mod log kept" }
}
$copies = @(Get-ChildItem $sess -Filter "modlog_run*.log" -ErrorAction SilentlyContinue)
if (-not $isStock) { Say ("game launches with a mod log: {0}" -f $copies.Count) }
if (-not $isStock -and $copies.Count -eq 0) { $problems += "no mod log from this session (was the game started?)" }

# ---- put the user's mod files back ----
function Restore-One([string]$name, [string]$dst, [bool]$had) {
    $src = Join-Path $orig $name
    if ($had) {
        Copy-Item $src $dst -Force
        if ((Get-FileHash $src).Hash -ne (Get-FileHash $dst).Hash) { return "$name restore mismatch" }
    } elseif (Test-Path $dst) { Remove-Item $dst -Force }
    return $null
}
if ($isStock) {
    $sd = if ($A.stash_dir) { $A.stash_dir } else { "" }
    if (-not $sd -or -not (Test-Path (Join-Path $sd "manifest.json"))) { $problems += "stock stash manifest not found ($sd)"; Say "PROBLEM: stock stash manifest not found: $sd" Red }
    else { $rr = Invoke-StockRestore $sd; Report-Restore $rr $sd; foreach ($p in $rr.problems) { $problems += "stock restore: $p" } }
} else {
foreach ($p in @((Restore-One "Nioh3EngineFix.ini" $ini ([bool]$A.had_ini)), (Restore-One "Nioh3EngineFix.asi" $asi ([bool]$A.had_asi)))) { if ($p) { $problems += $p } }
if ($A.had_addon_ini) {  # 1.7.41 add-on ini (boss loggers); its log is kept like the mod log
    $aLog = Join-Path $GameDir "Nioh3BossRematch.log"; if (Test-Path $aLog) { Copy-Item $aLog (Join-Path $sess "addonlog.log") -Force }
    if ($p = Restore-One "Nioh3BossRematch.ini" (Join-Path $GameDir "Nioh3BossRematch.ini") $true) { $problems += $p }
}
Say ("mod files put back: ASI {0}, your ini (byte-identical)" -f (Get-AsiVersion $asi)) $(if ($problems -match "restore") { "Red" } else { "Gray" })
}

# ---- lock ----
if ($A.lock_held) {
    $h = Get-LockHolder
    if ($h.holder -eq "pc") { $r = Invoke-GameLock "release"; Say "game lock: released (code $($r.code))" }
    else { Say "game lock: not held by this PC any more (holder: $($h.holder))" Yellow }
}

# ---- finish ----
$S = Get-Content (Join-Path $sess "session.json") -Raw | ConvertFrom-Json
$now = Get-Date
$S | Add-Member -Force stop $now.ToString("s")
$S | Add-Member -Force stop_ts ([double]([DateTimeOffset]$now).ToUnixTimeMilliseconds() / 1000)
$S | Add-Member -Force problems $problems
Write-Json $S (Join-Path $sess "session.json")
Move-Item $ActiveFile (Join-Path $sess "active.json") -Force
Remove-Item (Join-Path $sess "stop.flag") -Force -ErrorAction SilentlyContinue
$pmCsv = Join-Path $sess "presentmon.csv"
if (Test-Path $pmCsv) { Say ("PresentMon capture: {0:N1} MB" -f ((Get-Item $pmCsv).Length / 1MB)) } else { Say "PresentMon capture: none (no frames seen)" Yellow }
Say ""
Say "Writing the report..." Cyan
& py -I (Join-Path $PSScriptRoot "summarize.py") $sess
if ($problems) { Say ""; Say ("Problems: " + ($problems -join "; ")) Red }
Say ""
Say "Done. Report: $(Join-Path $sess 'report.md')" Green
Pause-End -NoPause:$NoPause
exit 0
