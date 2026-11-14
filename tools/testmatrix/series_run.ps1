# series_run.ps1 - self-owned supervisor for multi-arm game-test series (#102201 / root cause #102189).
#
# Problem it fixes: a worker (short-lived claude session) passed its OWN pid as -OwnerPid to each arm runner. When the worker
# exited, harness_guard's Assert-Owner aborted the running arm, the next arm never started, and the lock + test-active marker
# were left behind.
#
# How: the worker calls this script once. It writes a spec file and starts a DETACHED supervisor (Win32_Process.Create: the
# parent is WmiPrvSE, so it is outside the worker's job object and survives the worker / its shell being killed), prints the
# supervisor pid + status file, and returns. The supervisor:
#   - acquires the game lock (bridge gamelock, same holder as the arm runners), renews it every -RenewSec while arms run,
#   - writes the test-active marker (first word = seat) and refreshes it with each renew,
#   - runs the arms one after the other; '{OWNER}' in an arm command is replaced by the SUPERVISOR's pid, so each arm's
#     harness guard follows the supervisor (owner-death safety stays: supervisor dies -> the arm's guard cleans up),
#   - releases the lock and deletes the marker at the end or on failure (try/finally),
#   - starts a detached WATCHDOG that waits for the supervisor pid; if the supervisor dies without writing 'SERIES END',
#     the watchdog releases the lock and deletes the marker.
# The worker only polls the status file (one line per event).
#
#   powershell -NoProfile -ExecutionPolicy Bypass -File tools\testmatrix\series_run.ps1 -Name load-abba -Device pc2 -Seat pc2/1 `
#       -Arm 'bash C:/nioh3-re/t/arm.sh 1 A 1 {OWNER}','bash C:/nioh3-re/t/arm.sh 2 B 1 {OWNER}' [-LockGame nioh3-pc2]
#       (from bash/-File: one -Arm string, arms separated by ';;')
#       [-Marker <path>] [-StatusFile <path>] [-ArmMinutes 40] [-RenewSec 120] [-LockTtl 1800] [-KeepGoing] [-NoLock]
# Output: 'SUPERVISOR pid=<n> status=<file>'. Status lines: START, LOCK, ARM i START/END, RENEW (every 5th), SERIES END ok=..
param([string]$Name = "series", [string]$Device = "pc2", [string]$Seat = "", [string[]]$Arm = @(), [string]$LockGame = "",
      [string]$Marker = "", [string]$StatusFile = "", [int]$ArmMinutes = 40, [int]$RenewSec = 120, [int]$LockTtl = 1800,
      [switch]$KeepGoing, [switch]$NoLock, [string]$Spec = "", [switch]$Supervise, [int]$Watch = 0)
$ErrorActionPreference = "Stop"
$self = $MyInvocation.MyCommand.Path
$repo = Split-Path -Parent (Split-Path -Parent (Split-Path -Parent $self))
$uo = Join-Path (Split-Path -Parent $repo) "universal-optimizer\agent-bridge"

function Spawn-Detached([string]$cmdLine) {
    $r = Invoke-CimMethod -ClassName Win32_Process -MethodName Create -Arguments @{ CommandLine = $cmdLine; CurrentDirectory = $repo }
    if ($r.ReturnValue -ne 0) { throw "Win32_Process.Create failed ($($r.ReturnValue)): $cmdLine" }
    return [int]$r.ProcessId
}
function PsArgs([string]$extra) { "powershell.exe -NoProfile -ExecutionPolicy Bypass -WindowStyle Hidden -File `"$self`" $extra" }

# ---------------------------------------------------------------- worker mode: write spec, detach, return
if (-not $Supervise -and -not $Watch) {
    # powershell -File passes a comma list as ONE string: arms may also be separated by ';;'
    $Arm = @($Arm | ForEach-Object { $_ -split ';;' } | ForEach-Object { $_.Trim() } | Where-Object { $_ })
    if (-not $Arm.Count) { throw "no -Arm given" }
    $stamp = Get-Date -f yyyyMMdd-HHmmss
    $dir = Join-Path $env:LOCALAPPDATA "uo-agentd\$Device\series"; New-Item -ItemType Directory -Force $dir | Out-Null
    if (-not $StatusFile) { $StatusFile = Join-Path $dir "$Name-$stamp.status.txt" }
    if (-not $Marker) { $Marker = Join-Path $env:LOCALAPPDATA "uo-agentd\$Device\test-active" }
    if (-not $LockGame) { $LockGame = "nioh3-$Device" }
    if (-not $Seat) { $Seat = "$Device/?" }
    $specFile = Join-Path $dir "$Name-$stamp.spec.json"
    [ordered]@{ name = $Name; device = $Device; seat = $Seat; arms = $Arm; lockGame = $LockGame; marker = $Marker; status = $StatusFile
                armMinutes = $ArmMinutes; renewSec = $RenewSec; lockTtl = $LockTtl; keepGoing = [bool]$KeepGoing; noLock = [bool]$NoLock } |
        ConvertTo-Json | Set-Content -Encoding UTF8 $specFile
    $sp = Spawn-Detached (PsArgs "-Supervise -Spec `"$specFile`"")
    Write-Output "SUPERVISOR pid=$sp status=$StatusFile"
    exit 0
}

$S = Get-Content -Raw $Spec | ConvertFrom-Json
function St([string]$m) { $l = "[{0}] {1}" -f (Get-Date -f "yyyy-MM-dd HH:mm:ss"), $m; for ($i = 0; $i -lt 5; $i++) { try { [IO.File]::AppendAllText($S.status, $l + "`r`n"); return } catch { [Threading.Thread]::Sleep(100) } } }
function Lock([string]$a) {
    if ($S.noLock) { return @{ code = 0; out = "nolock $a" } }
    $env:UO_BRIDGE_TOKEN_FILE = "$env:APPDATA\uo-bridge\token"; $env:UO_AGENT_ID = $S.device
    Push-Location $uo
    try { $ttl = if ($a -eq "release") { @() } else { @("--ttl", "$($S.lockTtl)") }
          $o = (& py -m uo_bridge.gamelock $a $S.lockGame @ttl 2>&1 | Out-String).Trim(); @{ code = $LASTEXITCODE; out = $o } }
    finally { Pop-Location }
}

# ---------------------------------------------------------------- watchdog mode: wait for the supervisor, clean up if it died
if ($Watch) {
    $Supervisor = ""
    while (Get-Process -Id $Watch -ErrorAction SilentlyContinue) { [Threading.Thread]::Sleep(2000) }
    $txt = if (Test-Path $S.status) { Get-Content -Raw $S.status } else { "" }
    if ($txt -notmatch "SERIES END") {
        $r = Lock "release"; St "WATCHDOG: supervisor $Watch died without SERIES END -> lock release: $($r.out)"
        if ((Test-Path $S.marker) -and ((Get-Content -Raw $S.marker) -match "supervisor=$Watch\b")) { Remove-Item $S.marker -Force; St "WATCHDOG: marker deleted" }
        St "SERIES END ok=0 (watchdog cleanup)"
    }
    exit 0
}

# ---------------------------------------------------------------- supervisor mode
$me = $PID; $locked = $false; $ok = $true; $n = 0
function Touch-Marker { Set-Content -Encoding ASCII $S.marker "$($S.seat) $($S.name) supervisor=$me arms=$($S.arms.Count) refreshed=$(Get-Date -f s)" }
try {
    St "START $($S.name) supervisor=$me arms=$($S.arms.Count) lock=$($S.lockGame) marker=$($S.marker)"
    $wd = Spawn-Detached (PsArgs "-Watch $me -Spec `"$Spec`""); St "watchdog pid=$wd"
    $r = Lock "acquire"; if ($r.code -ne 0) { throw "lock: $($r.out)" }; $locked = $true; St "LOCK acquired: $($r.out)"
    New-Item -ItemType Directory -Force (Split-Path -Parent $S.marker) | Out-Null; Touch-Marker
    foreach ($a in $S.arms) {
        $n++; $cmd = $a.Replace("{OWNER}", "$me"); $log = "$($S.status).arm$n.log"
        St "ARM $n START: $cmd"
        $p = Start-Process cmd.exe -ArgumentList "/d /c $cmd > `"$log`" 2>&1" -PassThru -WindowStyle Hidden
        $t0 = Get-Date; $last = Get-Date; $k = 0
        while (-not $p.HasExited) {
            [Threading.Thread]::Sleep(1000)
            if (((Get-Date) - $last).TotalSeconds -ge $S.renewSec) {
                $last = Get-Date; $k++; $r = Lock "acquire"; Touch-Marker
                if ($r.code -ne 0) { St "!! RENEW failed: $($r.out)" } elseif ($k % 5 -eq 1) { St "RENEW ok (#$k)" }
            }
            if (((Get-Date) - $t0).TotalMinutes -gt $S.armMinutes) { St "!! ARM $n over $($S.armMinutes) min - killing tree $($p.Id)"; & taskkill /T /F /PID $p.Id | Out-Null; break }
        }
        $p.WaitForExit(); $code = $p.ExitCode
        St "ARM $n END exit=$code ($([math]::Round(((Get-Date) - $t0).TotalSeconds)) s) log=$log"
        if ($code -ne 0) { $ok = $false; if (-not $S.keepGoing) { St "stop: arm $n failed"; break } }
    }
} catch { $ok = $false; St "ERROR: $($_.Exception.Message)" }
finally {
    if ($locked) { $r = Lock "release"; St "LOCK released: $($r.out)" }
    if ((Test-Path $S.marker) -and ((Get-Content -Raw $S.marker) -match "supervisor=$me\b")) { Remove-Item $S.marker -Force; St "marker deleted" }
    St "SERIES END ok=$([int]$ok) arms_run=$n"
}
