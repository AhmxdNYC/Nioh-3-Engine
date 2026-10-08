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
