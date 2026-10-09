# harness_guard.ps1 - safety guard shared by EVERY script that launches Nioh3 or sends it input (dot-source it).
# Added 2026-10-08 after two incidents: the agent that started abba_pc.ps1 died, the harness lived on, relaunched the
# game itself and drove input; system commit hit 51.5/51.5 GB and the game crashed.
#
# Rules enforced:
#  (a) an owner PID (the agent's shell / claude process) must be alive before every launch, every input and (via a
#      Start-Sleep shadow) every few seconds of waiting; owner gone -> cleanup + exit 99.  Hard -MaxMinutes self-exit.
#      PID reuse is detected by comparing the owner's process start time.
#  (b) before a launch: commit headroom (Win32_OperatingSystem FreeVirtualMemory = commit limit - committed) >= 18 GB,
#      else wait up to 5 min, else abort.  The page file is system managed and can grow; we never touch it.
#  (c) never relaunch after 2 game crashes in one run (game pid disappeared without us closing it, or a reported boot failure).
#  (d) PID file tools\testmatrix\state\harness_pids\<name>_<pid>.json (harness, owner, game, PresentMon pids) for kill_leftovers.ps1.
#
# Usage in a script:   . "$here\harness_guard.ps1"; Init-Guard -Name abba_pc -OwnerPid $OwnerPid -MaxMinutes $MaxMinutes -Cleanup { ...restore ini... }
#   Assert-Launch (before every game start), Register-Game after it, Note-Closed when WE close it, Note-Crash on a boot failure,
#   Done-Guard in the final finally.  Test overrides (env): HG_MIN_HEADROOM_GB, HG_HEADROOM_WAIT_SEC.
$script:HGdir = Join-Path (Split-Path -Parent $MyInvocation.MyCommand.Path) "state\harness_pids"
$global:HG = $null
$script:RealSleep = Get-Command Start-Sleep -CommandType Cmdlet

function Get-ParentPid([int]$p = $PID) { (Get-CimInstance Win32_Process -Filter "ProcessId=$p" -ErrorAction SilentlyContinue).ParentProcessId }
function HG-Log([string]$s) { $l = "[{0}] GUARD: {1}" -f (Get-Date -f HH:mm:ss), $s; Write-Host $l; if ($global:HG -and $global:HG.logFile) { Add-Content $global:HG.logFile $l } }
function HG-Save {
    if (-not $global:HG) { return }
    $o = [ordered]@{ name = $global:HG.name; harnessPid = $PID; harnessStart = $global:HG.selfStart.ToString("o"); ownerPid = $global:HG.ownerPid
        ownerStart = $global:HG.ownerStart.ToString("o"); started = $global:HG.t0.ToString("o"); deadline = $global:HG.deadline.ToString("o")
        games = @($global:HG.games); children = @($global:HG.children); launches = $global:HG.launches; crashes = $global:HG.crashes }
    $o | ConvertTo-Json -Depth 5 | Set-Content $global:HG.file -Encoding UTF8
}
function Test-OwnerAlive {
    if (-not $global:HG) { return $true }
    $p = Get-Process -Id $global:HG.ownerPid -ErrorAction SilentlyContinue
    if (-not $p) { return $false }
    try { return ($p.StartTime -eq $global:HG.ownerStart) } catch { return $true }   # access denied -> trust the pid
}
function Init-Guard {
    param([Parameter(Mandatory)][string]$Name, [int]$OwnerPid = 0, [int]$MaxMinutes = 90, [scriptblock]$Cleanup = $null,
          [double]$MinHeadroomGB = 18, [int]$MaxCrashes = 2)
    if ($OwnerPid -le 0) { $OwnerPid = [int](Get-ParentPid) }
    $op = Get-Process -Id $OwnerPid -ErrorAction SilentlyContinue
    if (-not $op) { Write-Host "GUARD: owner pid $OwnerPid is not running - refusing to start"; [Environment]::Exit(99) }
    New-Item -ItemType Directory -Force $script:HGdir | Out-Null
    $me = Get-Process -Id $PID
    $env_min = $env:HG_MIN_HEADROOM_GB; if ($env_min) { $MinHeadroomGB = [double]$env_min }
    $global:HG = @{ name = $Name; ownerPid = $OwnerPid; ownerStart = $op.StartTime; selfStart = $me.StartTime; t0 = (Get-Date); deadline = (Get-Date).AddMinutes($MaxMinutes)
        cleanup = $Cleanup; minHeadroomGB = $MinHeadroomGB; maxCrashes = $MaxCrashes; games = @(); children = @(); launches = 0; crashes = 0; aborting = $false
        file = (Join-Path $script:HGdir "${Name}_$PID.json"); logFile = (Join-Path $script:HGdir "${Name}_$PID.log") }
    HG-Save
    HG-Log "init name=$Name harness=$PID owner=$OwnerPid maxMinutes=$MaxMinutes minHeadroom=${MinHeadroomGB}GB maxCrashes=$MaxCrashes"
}
function Guard-Fail([string]$why, [int]$code = 99) {
    if (-not $global:HG) { Write-Host "GUARD-FAIL (no guard): $why"; [Environment]::Exit($code) }
    if ($global:HG.aborting) { [Environment]::Exit($code) }
    $global:HG.aborting = $true
    HG-Log "ABORT: $why"
    Clear-MeasureLock
    try { if ($global:HG.cleanup) { & $global:HG.cleanup } } catch { HG-Log "cleanup error: $_" }
    Kill-Registered
    try { Remove-Item $global:HG.file -Force -ErrorAction SilentlyContinue } catch {}
    [Environment]::Exit($code)
}
function Kill-Registered {   # exact pids only, and only if the process still has the recorded start time
    foreach ($g in @($global:HG.games) + @($global:HG.children)) {
        $p = Get-Process -Id $g.pid -ErrorAction SilentlyContinue
        if ($p) { try { if ($p.StartTime.ToString("o") -eq $g.start) { Stop-Process -Id $g.pid -Force; HG-Log "killed $($g.name) pid $($g.pid)" } } catch {} }
    }
}
function Assert-Owner {
    if (-not $global:HG -or $global:HG.aborting) { return }
    if (-not (Test-OwnerAlive)) { Guard-Fail "owner pid $($global:HG.ownerPid) is gone" }
    if ((Get-Date) -gt $global:HG.deadline) { Guard-Fail "MaxMinutes reached" }
}
# Start-Sleep shadow: sleeps in <=2 s slices and re-checks the owner / deadline, so a dead owner is noticed mid-wait.
function Start-Sleep {
    param([Parameter(Position = 0)][double]$Seconds = 0, [int]$Milliseconds = 0)
    $total = $Seconds + $Milliseconds / 1000.0
    if (-not $global:HG -or $global:HG.aborting) { if ($total -gt 0) { & $script:RealSleep -Milliseconds ([int]($total * 1000)) }; return }
    $end = (Get-Date).AddSeconds($total)
    do { Assert-Owner; $left = ($end - (Get-Date)).TotalSeconds; if ($left -le 0) { break }; & $script:RealSleep -Milliseconds ([int]([Math]::Min(2, $left) * 1000)) } while ($true)
}
# The page file is system managed and grows on demand (up to min(3x RAM, free disk)), so the CURRENT commit limit is not the ceiling.
# effective headroom = min(limit + max(0, C: free - 15 GB reserve), 3 x RAM) - used ; ALSO require current (limit - used) >= 6 GB.
function Get-CommitNumbers {
    $os = Get-CimInstance Win32_OperatingSystem
    $limit = $os.TotalVirtualMemorySize / 1MB; $used = ($os.TotalVirtualMemorySize - $os.FreeVirtualMemory) / 1MB; $ram = $os.TotalVisibleMemorySize / 1MB
    $cfree = (Get-PSDrive C).Free / 1GB
    $grow = [Math]::Max(0, $cfree - 15)
    $ceiling = [Math]::Min($limit + $grow, 3 * $ram)
    [pscustomobject]@{ limit = $limit; used = $used; ram = $ram; cfree = $cfree; current = ($limit - $used); effective = ([Math]::Max($limit, $ceiling) - $used) }
}
function Get-CommitHeadroomGB { [Math]::Round((Get-CommitNumbers).effective, 1) }
function Get-CommitInfo { $c = Get-CommitNumbers; "commit {0:N1}/{1:N1} GB, current headroom {2:N1} GB, effective headroom {3:N1} GB (C: free {4:N1} GB, RAM {5:N1} GB)" -f $c.used, $c.limit, $c.current, $c.effective, $c.cfree, $c.ram }
function Wait-Headroom {
    $need = $global:HG.minHeadroomGB; $needCur = 6.0; if ($env:HG_MIN_CURRENT_GB) { $needCur = [double]$env:HG_MIN_CURRENT_GB }
    $maxWait = 300; if ($env:HG_HEADROOM_WAIT_SEC) { $maxWait = [int]$env:HG_HEADROOM_WAIT_SEC }
    $t0 = Get-Date
    while ($true) {
        $c = Get-CommitNumbers
        if ($c.effective -ge $need -and $c.current -ge $needCur) { HG-Log "headroom OK ($(Get-CommitInfo); need effective >= $need, current >= $needCur)"; return }
        if (((Get-Date) - $t0).TotalSeconds -ge $maxWait) { Guard-Fail "commit headroom too low after ${maxWait}s: $(Get-CommitInfo); need effective >= $need and current >= $needCur" }
        HG-Log "waiting for commit headroom: $(Get-CommitInfo)"
        Start-Sleep 15
    }
}# Called before EVERY game launch (and cheap enough to call before input bursts too).
function Assert-Launch {
    if (-not $global:HG) { return }
    Assert-Owner
    foreach ($g in @($global:HG.games)) {   # registered game that vanished without us closing it = a crash
        if (-not $g.closed -and -not $g.counted -and -not (Get-Process -Id $g.pid -ErrorAction SilentlyContinue)) { $g.counted = $true; $global:HG.crashes++; HG-Log "game pid $($g.pid) disappeared (crash #$($global:HG.crashes))" }
    }
    if ($global:HG.crashes -ge $global:HG.maxCrashes) { Guard-Fail "$($global:HG.crashes) game crashes in this run - not relaunching" }
    Wait-Headroom
    Assert-Owner
    $global:HG.launches++; HG-Save
}
function Register-Game {   # after a launch: record every Nioh3 process (exact pid + start time)
    if (-not $global:HG) { return }
    foreach ($p in @(Get-Process Nioh3 -ErrorAction SilentlyContinue)) {
        if (-not ($global:HG.games | Where-Object { $_.pid -eq $p.Id })) { $global:HG.games += @(@{ pid = $p.Id; name = "Nioh3"; start = $p.StartTime.ToString("o"); closed = $false; counted = $false }) }
    }
    HG-Save
}
function Register-Child([System.Diagnostics.Process]$p, [string]$name = "child") {   # e.g. PresentMon
    if (-not $global:HG -or -not $p) { return }
    $global:HG.children += @(@{ pid = $p.Id; name = $name; start = $p.StartTime.ToString("o") }); HG-Save
}
function Note-Closed { if ($global:HG) { foreach ($g in $global:HG.games) { $g.closed = $true }; HG-Save } }   # WE are closing the game: not a crash
function Note-Crash([string]$why = "") { if ($global:HG) { $global:HG.crashes++; HG-Log "crash #$($global:HG.crashes): $why"; HG-Save } }
function Done-Guard([switch]$KeepFile) {   # -KeepFile: a launcher that leaves the game running keeps its pid file so kill_leftovers.ps1 can find the game
    Clear-MeasureLock
    if ($global:HG) { HG-Log "done"; if (-not $KeepFile) { try { Remove-Item $global:HG.file -Force -ErrorAction SilentlyContinue } catch {} }; $global:HG = $null } }

# Foreground-loss policy (coordinator 2026-10-08): when ui.fsx REFUSES input (game not foreground), refocus and retry ONLY if no human
# touched the PC since our own last input (system idle >= time since our last input, minus 3 s slack); otherwise abort as before.
$script:lastSent = $null; $script:refocusN = 0
function Mark-Input { $script:lastSent = Get-Date }
function Test-NoHumanActive {
    $o = (dotnet fsi (Join-Path (Split-Path -Parent (Split-Path -Parent $script:HGdir)) "..\ui.fsx") idle 2>&1 | Out-String)
    if ($o -notmatch 'idle ([\d.]+)') { return $false }
    $idle = [double]$Matches[1]; $ours = if ($script:lastSent) { ((Get-Date) - $script:lastSent).TotalSeconds } else { 1e9 }
    HG-Log ("refusal check: system idle {0:N1}s, since our last input {1:N1}s" -f $idle, [Math]::Min($ours, 99999))
    if (-not $script:lastSent) { return ($idle -ge 60) }
    return ($idle -ge ($ours - 3))
}
function Try-Refocus([string]$ui) {   # returns $true if refocused and a retry is allowed
    if ($script:refocusN -ge 6) { return $false }
    if (-not (Test-NoHumanActive)) { return $false }
    $script:refocusN++; HG-Log "no human active -> refocusing Nioh3 (#$($script:refocusN))"
    $null = dotnet fsi $ui focus Nioh3 2>&1; Start-Sleep 1; return $true
}

# AutoContinueLoad is OFF by default since 1.7.33 (user option); harnesses that drive loads need it on. Sets/adds [General] AutoContinueLoad = 1 in ini text.
function Force-Acl([string]$t) {
    if ($t -match '(?m)^AutoContinueLoad *=') { return ($t -replace '(?m)^AutoContinueLoad *=.*', 'AutoContinueLoad = 1') }
    if ($t -match '(?m)^\[General\]') { return ($t -replace '(?m)^\[General\][ \t]*(\r?)$', "[General]`$1`nAutoContinueLoad = 1`$1") }
    return $t + "`r`n[General]`r`nAutoContinueLoad = 1`r`n"
}

# ---- quiet-machine guard + measuring lock (2026-10-08, run7: other agents' build.bat / harnesses / dotnet fsi proofs swung fps
# 107-127 inside one launch and made DQ / P1E7 / MA INVALID). Foreign load = CPU time used by every process EXCEPT Nioh3, the
# harness's own process tree (its dotnet fsi testpopup / ui / vpad.py children) and its registered children (PresentMon),
# over a ~2 s interval (TotalProcessorTime deltas; processes we may not read, e.g. MsMpEng, are invisible).
# 'Noisy' = foreign load > QG_MAX_CORES logical cores (default 1.0 of 12) or one build/analysis-type process (cl, link, msbuild,
# ml64, java = Ghidra, dotnet, py, node, ...) > QG_HEAVY_CORES (default 0.25). Kernel / game-driven processes (Idle, System,
# Registry, dwm, audiodg, csrss) are ignored. Harness use: Wait-Quiet before a window, Measure-Foreign samples during it.
$script:QGHeavy = @("cl", "link", "lib", "msbuild", "ml64", "ml", "cvtres", "rc", "mspdbsrv", "vctip", "cmake", "ninja", "clang", "clang-cl",
    "lld-link", "rustc", "cargo", "java", "javaw", "dotnet", "fsi", "py", "python", "pythonw", "node", "devenv", "7z", "tar", "git", "rg", "ghidra*", "analyzeHeadless*")
$script:QGIgnore = @("Idle", "System", "Registry", "dwm", "audiodg", "csrss", "Nioh3", "PresentMon")
function QG-MaxCores { if ($env:QG_MAX_CORES) { [double]$env:QG_MAX_CORES } else { 1.0 } }
function QG-HeavyCores { if ($env:QG_HEAVY_CORES) { [double]$env:QG_HEAVY_CORES } else { 0.25 } }
function QG-IsHeavy([string]$n) { foreach ($h in $script:QGHeavy) { if ($n -like $h) { return $true } }; return $false }
# pids that are ours: this harness and all its descendants, its registered children, every Nioh3 process
function Get-QuietExclude([int[]]$extra = @()) {
    $ex = @{}; $ex[$PID] = 1
    $all = @(Get-CimInstance Win32_Process -Property ProcessId, ParentProcessId -ErrorAction SilentlyContinue)
    $kids = @{}
    foreach ($p in $all) { $k = [int]$p.ParentProcessId; if (-not $kids.ContainsKey($k)) { $kids[$k] = New-Object System.Collections.ArrayList }; [void]$kids[$k].Add([int]$p.ProcessId) }
    $q = New-Object System.Collections.Queue; $q.Enqueue($PID)
    while ($q.Count) { $c = $q.Dequeue(); if ($kids.ContainsKey($c)) { foreach ($k in $kids[$c]) { if (-not $ex.ContainsKey($k) -and $k -ne $c) { $ex[$k] = 1; $q.Enqueue($k) } } } }
    if ($global:HG) { foreach ($c in @($global:HG.children)) { $ex[[int]$c.pid] = 1 } }
    foreach ($p in @(Get-Process Nioh3 -ErrorAction SilentlyContinue)) { $ex[$p.Id] = 1 }
    foreach ($e in $extra) { if ($e) { $ex[[int]$e] = 1 } }
    $ex
}
function QG-Snap {
    $h = @{}
    foreach ($p in [System.Diagnostics.Process]::GetProcesses()) {
        try { $h[$p.Id] = @($p.ProcessName, $p.TotalProcessorTime.TotalSeconds, $p.StartTime) } catch {}   # access denied -> invisible
        $p.Dispose()
    }
    $h
}
# one sample over ~$sec seconds: @{ cores; top; heavy; noisy }
function Measure-Foreign([double]$sec = 2.0, $exclude = $null) {
    if (-not $exclude) { $exclude = Get-QuietExclude }
    $a = QG-Snap; $sw = [Diagnostics.Stopwatch]::StartNew()
    Start-Sleep -Milliseconds ([int]($sec * 1000))
    $b = QG-Snap; $dt = $sw.Elapsed.TotalSeconds
    $per = @{}
    foreach ($id in $b.Keys) {
        if ($exclude.ContainsKey($id)) { continue }
        $n = $b[$id][0]; if ($script:QGIgnore -contains $n) { continue }
        $d = if ($a.ContainsKey($id) -and $a[$id][2] -eq $b[$id][2]) { $b[$id][1] - $a[$id][1] } else { $b[$id][1] }   # new process: all of its CPU
        if ($d -le 0) { continue }
        if ($per.ContainsKey($n)) { $per[$n] += $d } else { $per[$n] = $d }
    }
    $cores = 0.0; foreach ($v in $per.Values) { $cores += $v / $dt }
    $heavy = @($per.Keys | Where-Object { (QG-IsHeavy $_) -and ($per[$_] / $dt) -gt (QG-HeavyCores) })
    $top = (($per.GetEnumerator() | Sort-Object Value -Descending | Select-Object -First 4 | ForEach-Object { "{0} {1:N2}c" -f $_.Key, ($_.Value / $dt) }) -join ", ")
    [pscustomobject]@{ cores = [Math]::Round($cores, 2); top = $top; heavy = ($heavy -join ","); noisy = ($cores -gt (QG-MaxCores) -or $heavy.Count -gt 0) }
}
# wait until 2 consecutive samples are quiet (logged every 30 s while waiting); returns the seconds waited, -1 = still noisy
# after QG_WAIT_MIN minutes (default 15; the caller measures anyway and marks the window noisy if it stays noisy)
function Wait-Quiet([string]$why = "", [scriptblock]$log = $null) {
    $maxMin = if ($env:QG_WAIT_MIN) { [double]$env:QG_WAIT_MIN } else { 15 }
    $t0 = Get-Date; $ok = 0; $lastLog = [datetime]::MinValue; $ex = Get-QuietExclude
    while ($true) {
        $m = Measure-Foreign 2.0 $ex
        if (-not $m.noisy) { if (++$ok -ge 2) { break } } else { $ok = 0 }
        $w = ((Get-Date) - $t0).TotalSeconds
        if ($m.noisy -and ((Get-Date) - $lastLog).TotalSeconds -ge 30) {
            $l = "QUIET: waiting $([int]$w) s ($why): foreign {0:N2} cores [{1}]{2}" -f $m.cores, $m.top, $(if ($m.heavy) { " heavy: $($m.heavy)" } else { "" })
            if ($log) { & $log $l } else { HG-Log $l }
            $lastLog = Get-Date; Set-MeasureLock "waiting for a quiet machine ($why)"
        }
        if ($w -gt $maxMin * 60) {
            $l = "QUIET: still noisy after $maxMin min ($why) - measuring anyway"
            if ($log) { & $log $l } else { HG-Log $l }
            return -1
        }
        if ($w -gt 20 -and $ok -eq 0) { $ex = Get-QuietExclude }   # long wait: refresh our own tree
    }
    return [Math]::Max(0, [Math]::Round(((Get-Date) - $t0).TotalSeconds - 4, 0))
}
# %TEMP%\nioh3_measuring.lock (JSON with the harness pid) while a harness measures: other agents must not start builds / Ghidra /
# dotnet fsi proofs / long py jobs while it exists AND its pid is alive (Test-MeasureLock). Removed by Done-Guard / Guard-Fail.
$script:MeasureLock = Join-Path ([IO.Path]::GetTempPath()) "nioh3_measuring.lock"
function Set-MeasureLock([string]$state = "measuring") {
    $o = [ordered]@{ pid = $PID; name = $(if ($global:HG) { $global:HG.name } else { "harness" }); owner = $(if ($global:HG) { $global:HG.ownerPid } else { 0 })
        since = $(if ($global:HG) { $global:HG.t0.ToString("o") } else { (Get-Date).ToString("o") }); updated = (Get-Date).ToString("o"); state = $state
        rule = "Nioh3 fps measurement running: no build.bat / cl / link / Ghidra / dotnet fsi proofs / long py jobs until this file is gone or its pid is dead" }
    try { [IO.File]::WriteAllText($script:MeasureLock, ($o | ConvertTo-Json -Compress), (New-Object Text.UTF8Encoding($false))) } catch {}
}
function Clear-MeasureLock {
    try { if (Test-Path $script:MeasureLock) { $o = Get-Content $script:MeasureLock -Raw | ConvertFrom-Json; if ([int]$o.pid -eq $PID) { Remove-Item $script:MeasureLock -Force } } } catch {}
}
function Test-MeasureLock {   # for other agents / scripts: the live lock object, or $null (no file, or its pid is gone)
    try { $o = Get-Content $script:MeasureLock -Raw -ErrorAction Stop | ConvertFrom-Json; if (Get-Process -Id ([int]$o.pid) -ErrorAction SilentlyContinue) { return $o } } catch {}
    return $null
}

# Set-WeakAffinity - optional "weak CPU" simulation (-Affinity N). Per-process affinity of the Nioh3 process only (dies with the process;
# no system-wide setting, power plan or clock change). Picks N logical CPUs on distinct physical cores first (SMT siblings are assumed
# adjacent: logical 2k,2k+1 = core k when logical = 2 x cores), then siblings if N > cores. Logs the mask.
function Set-WeakAffinity([int]$N, [scriptblock]$Log = { param($m) Write-Host $m }) {
    if ($N -le 0) { return }
    $p = Get-Process Nioh3 -ErrorAction SilentlyContinue | Select-Object -First 1
    if (-not $p) { & $Log "AFFINITY: no Nioh3 process, nothing set"; return }
    $cpu = @(Get-CimInstance Win32_Processor)
    $cores = ($cpu | Measure-Object NumberOfCores -Sum).Sum; $logi = ($cpu | Measure-Object NumberOfLogicalProcessors -Sum).Sum
    if ($logi -gt 64) { $logi = 64 }
    $smt = if ($cores -gt 0 -and $logi -ge 2 * $cores) { [int]($logi / $cores) } else { 1 }
    $order = @(); for ($s = 0; $s -lt $smt; $s++) { for ($c = 0; $c -lt $cores; $c++) { $order += ($c * $smt + $s) } }
    if ($N -gt $order.Count) { $N = $order.Count }
    $mask = [int64]0; foreach ($i in $order[0..($N - 1)]) { $mask = $mask -bor ([int64]1 -shl $i) }
    try { $p.ProcessorAffinity = [IntPtr]$mask; & $Log ("AFFINITY: Nioh3 pid {0} -> {1} logical CPUs, mask 0x{2:X} (cpu list {3}; machine {4} cores / {5} logical, smt {6}); per-process, reverts when the game exits" -f $p.Id, $N, $mask, ($order[0..($N - 1)] -join ","), $cores, $logi, $smt) }
    catch { & $Log "AFFINITY: failed to set mask: $_" }
}

# ---- display refresh guard (run8b, 2026-10-09: notes\run8b-analysis.md) --------------------------------------------------------------
# Unplugging the second monitor left the XV272U (240 Hz) desktop at 60 Hz. The game then picks its 60 fps tier (log 'vrr tier 60 cap 60')
# and every window of Block C read 60.00 fps although [VRR] MaxFps = 240. Get-DisplayModes reads every attached display (current mode +
# the highest refresh offered at the current resolution); Ensure-DisplayRefresh raises the game's display to >= NeedHz for the run
# (dynamic mode change, NOT saved to the registry; Restore-DisplayRefresh puts the old mode back) and returns $false with the reason in
# $script:DisplayWhy when it cannot. Read-only with -NoForce.
if (-not ("N3Disp" -as [type])) {
    Add-Type -TypeDefinition @"
using System; using System.Runtime.InteropServices; using System.Collections.Generic;
public static class N3Disp {
    [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode)]
    public struct DEVMODE {
        [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 32)] public string dmDeviceName;
        public short dmSpecVersion, dmDriverVersion, dmSize, dmDriverExtra; public int dmFields;
        public int dmPositionX, dmPositionY, dmDisplayOrientation, dmDisplayFixedOutput;
        public short dmColor, dmDuplex, dmYResolution, dmTTOption, dmCollate;
        [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 32)] public string dmFormName;
        public short dmLogPixels; public int dmBitsPerPel, dmPelsWidth, dmPelsHeight, dmDisplayFlags, dmDisplayFrequency;
        public int dmICMMethod, dmICMIntent, dmMediaType, dmDitherType, dmReserved1, dmReserved2, dmPanningWidth, dmPanningHeight;
    }
    [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode)]
    public struct DISPLAY_DEVICE {
        public int cb; [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 32)] public string DeviceName;
        [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 128)] public string DeviceString; public int StateFlags;
        [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 128)] public string DeviceID;
        [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 128)] public string DeviceKey;
    }
    [DllImport("user32.dll", CharSet = CharSet.Unicode)] public static extern bool EnumDisplayDevices(string dev, int i, ref DISPLAY_DEVICE dd, int flags);
    [DllImport("user32.dll", CharSet = CharSet.Unicode)] public static extern bool EnumDisplaySettings(string dev, int mode, ref DEVMODE dm);
    [DllImport("user32.dll", CharSet = CharSet.Unicode)] public static extern int ChangeDisplaySettingsEx(string dev, ref DEVMODE dm, IntPtr hwnd, int flags, IntPtr lp);
    public static DEVMODE New() { var d = new DEVMODE(); d.dmSize = (short)Marshal.SizeOf(typeof(DEVMODE)); return d; }
    public static List<string> Attached() {   // "name|primary 0/1|monitor"
        var r = new List<string>();
        for (int i = 0; i < 16; i++) {
            var dd = new DISPLAY_DEVICE(); dd.cb = Marshal.SizeOf(typeof(DISPLAY_DEVICE));
            if (!EnumDisplayDevices(null, i, ref dd, 0)) break;
            if ((dd.StateFlags & 1) == 0) continue;   // DISPLAY_DEVICE_ATTACHED_TO_DESKTOP
            var mon = new DISPLAY_DEVICE(); mon.cb = Marshal.SizeOf(typeof(DISPLAY_DEVICE));
            string mname = EnumDisplayDevices(dd.DeviceName, 0, ref mon, 0) ? mon.DeviceString : "?";
            r.Add(dd.DeviceName + "|" + (((dd.StateFlags & 4) != 0) ? "1" : "0") + "|" + mname);
        }
        return r;
    }
    public static DEVMODE Current(string dev) { var d = New(); EnumDisplaySettings(dev, -1, ref d); return d; }   // ENUM_CURRENT_SETTINGS
    public static DEVMODE Best(string dev, int w, int h, int bpp) {   // highest refresh offered at this resolution / depth
        var best = New(); best.dmDisplayFrequency = 0;
        for (int i = 0; ; i++) { var d = New(); if (!EnumDisplaySettings(dev, i, ref d)) break;
            if (d.dmPelsWidth == w && d.dmPelsHeight == h && d.dmBitsPerPel == bpp && d.dmDisplayFrequency > best.dmDisplayFrequency) best = d; }
        return best;
    }
}
"@
}
function Get-DisplayModes {   # one object per attached display: dev, primary, monitor, w, h, hz, maxHz (at the current resolution)
    foreach ($a in [N3Disp]::Attached()) {
        $f = $a -split '\|', 3; $c = [N3Disp]::Current($f[0]); $b = [N3Disp]::Best($f[0], $c.dmPelsWidth, $c.dmPelsHeight, $c.dmBitsPerPel)
        [pscustomobject]@{ dev = $f[0]; primary = ($f[1] -eq "1"); monitor = $f[2]; w = $c.dmPelsWidth; h = $c.dmPelsHeight; hz = $c.dmDisplayFrequency; maxHz = $b.dmDisplayFrequency }
    }
}
function Format-DisplayModes { (@(Get-DisplayModes) | ForEach-Object { "{0}{1} {2} {3}x{4} @ {5} Hz (max {6} Hz)" -f $_.dev, $(if ($_.primary) { "*" } else { "" }), $_.monitor, $_.w, $_.h, $_.hz, $_.maxHz }) -join "; " }
$script:DisplayRestore = $null; $script:DisplayWhy = ""
function Ensure-DisplayRefresh([int]$NeedHz, [scriptblock]$Say = { param($m) Write-Host $m }, [switch]$NoForce) {
    $all = @(Get-DisplayModes)
    & $Say "DISPLAY: $(Format-DisplayModes) - need >= $NeedHz Hz on the game display (primary)"
    $d = $all | Where-Object primary | Select-Object -First 1
    if (-not $d) { $script:DisplayWhy = "no primary display found"; return $false }
    if ($all.Count -gt 1) { & $Say "DISPLAY: $($all.Count) displays attached - the game is assumed on the primary $($d.dev) (config.xml MONITOR_ID decides)" }
    if ($d.hz -ge $NeedHz) { $script:DisplayWhy = ""; return $true }
    if ($NoForce -or $d.maxHz -lt $NeedHz) {
        $script:DisplayWhy = "display $($d.dev) $($d.monitor) runs $($d.w)x$($d.h) @ $($d.hz) Hz (max $($d.maxHz) Hz at this resolution) < $NeedHz Hz needed: the game would cap at a $($d.hz) fps tier$(if ($NoForce) { ' (-NoForceRefresh)' })"
        return $false
    }
    $cur = [N3Disp]::Current($d.dev); $best = [N3Disp]::Best($d.dev, $cur.dmPelsWidth, $cur.dmPelsHeight, $cur.dmBitsPerPel)
    $best.dmFields = 0x5C0000   # DM_BITSPERPEL | DM_PELSWIDTH | DM_PELSHEIGHT | DM_DISPLAYFREQUENCY
    $rc = [N3Disp]::ChangeDisplaySettingsEx($d.dev, [ref]$best, [IntPtr]::Zero, 0, [IntPtr]::Zero)   # flags 0 = dynamic, not saved to the registry
    & $script:RealSleep -Seconds 3
    $now = [N3Disp]::Current($d.dev).dmDisplayFrequency
    if ($rc -ne 0 -or $now -lt $NeedHz) { $script:DisplayWhy = "display $($d.dev) stays at $now Hz after the switch to $($best.dmDisplayFrequency) Hz (ChangeDisplaySettingsEx $rc)"; return $false }
    if (-not $script:DisplayRestore) { $script:DisplayRestore = @{ dev = $d.dev; mode = $cur } }
    & $Say "DISPLAY: $($d.dev) $($d.monitor) raised $($d.hz) -> $now Hz for this run (dynamic; the old mode is put back at the end). The desktop was at $($d.hz) Hz: set the refresh in Windows display settings."
    $script:DisplayWhy = ""; return $true
}
function Restore-DisplayRefresh([scriptblock]$Say = { param($m) Write-Host $m }) {
    if (-not $script:DisplayRestore) { return }
    $m = $script:DisplayRestore.mode; $m.dmFields = 0x5C0000
    $rc = [N3Disp]::ChangeDisplaySettingsEx($script:DisplayRestore.dev, [ref]$m, [IntPtr]::Zero, 0, [IntPtr]::Zero)
    & $Say "DISPLAY: $($script:DisplayRestore.dev) put back to $($m.dmDisplayFrequency) Hz (ChangeDisplaySettingsEx $rc)"
    $script:DisplayRestore = $null
}
# The game's own frame cap as the mod logs it every 30 s ('OSD: stats: ... vrr tier T cap C armed A'): the authoritative check that a run
# is not capped below -MaxFps (display refresh, REN_FPS, VSync or the VRR tier logic - whatever the cause, the number is what counts).
function Get-VrrCap([string]$logText) {
    $m = [regex]::Matches($logText, 'vrr tier (\d+) cap (\d+) armed (\d+)')
    if ($m.Count -eq 0) { return $null }
    $g = $m[$m.Count - 1].Groups; [pscustomobject]@{ tier = [int]$g[1].Value; cap = [int]$g[2].Value; armed = [int]$g[3].Value }
}
# Which other harnesses ended / ran while our game was up (run8b A: a pc_gpu_session -DryRun's Restore-All closed every Nioh3 by name).
function Get-ForeignGuardActivity([datetime]$since) {
    Get-ChildItem $script:HGdir -Filter *.log -ErrorAction SilentlyContinue | Where-Object { $_.LastWriteTime -ge $since -and (-not $global:HG -or $_.FullName -ne $global:HG.logFile) } |
        ForEach-Object { "$($_.BaseName) (last line: $((Get-Content $_.FullName -Tail 1) -replace '\s+', ' '))" }
}
