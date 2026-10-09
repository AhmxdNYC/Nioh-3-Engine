<#
  pc2_monitor.ps1 - background helper of a play session (started hidden by pc2_start_session.ps1, stopped by pc2_stop_session.ps1 via stop.flag).
  Every 5 s: Windows counters -> sysmon.csv (CPU total / performance %, Nioh3 CPU % + memory, GPU 3D engine % total and Nioh3,
  VRAM dedicated Nioh3 + adapter, RAM available, commit). Watches Nioh3.exe: at every exit copies Nioh3EngineFix.log (the mod
  overwrites it at each launch) to modlog_run<N>.log and notes the exit code in events.txt. Renews the bridge game lock every 20 min
  while it holds it. Never touches the game itself; no input of any kind.
#>
param([Parameter(Mandatory)][string]$SessionDir, [string]$GameDir = "", [switch]$RenewLock, [int]$IntervalSec = 5, [int]$MaxHours = 16)
$ErrorActionPreference = "Continue"
. (Join-Path $PSScriptRoot "pc2_session_lib.ps1")
$flag = Join-Path $SessionDir "stop.flag"
$csvPath = Join-Path $SessionDir "sysmon.csv"
$ev = Join-Path $SessionDir "events.txt"
$modLog = Join-Path $GameDir "Nioh3EngineFix.log"
function Ev([string]$s) { Add-Content -Path $ev -Value ("{0} {1}" -f (Get-Date -Format "yyyy-MM-ddTHH:mm:ss"), $s) }
try { [Diagnostics.Process]::GetCurrentProcess().PriorityClass = "BelowNormal" } catch {}

$cols = "time,game_running,cpu_total_pct,cpu_perf_pct,game_cpu_pct,gpu3d_total_pct,gpu3d_game_pct,vram_game_mb,vram_adapter_mb,game_ws_mb,game_private_mb,ram_avail_mb,commit_mb"
if (-not (Test-Path $csvPath)) { Set-Content -Path $csvPath -Value $cols -Encoding UTF8 }
$counters = @('\Processor(_Total)\% Processor Time', '\Processor Information(_Total)\% Processor Performance', '\Memory\Available MBytes',
              '\Memory\Committed Bytes', '\GPU Engine(*engtype_3D)\Utilization Percentage', '\GPU Process Memory(*)\Dedicated Usage',
              '\GPU Adapter Memory(*)\Dedicated Usage')
$run = 0; $proc = $null; $lastCpu = $null; $lastT = $null; $t0 = Get-Date; $lastRenew = Get-Date
Ev "monitor started (pid $PID)"
while (-not (Test-Path $flag) -and ((Get-Date) - $t0).TotalHours -lt $MaxHours) {
    $now = Get-Date
    # ---- game process tracking ----
    $g = Get-Process -Name Nioh3 -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($g -and (-not $proc -or $proc.Id -ne $g.Id)) {
        $run++; $proc = $g; $null = $proc.Handle   # open a handle now so the exit code stays readable
        $lastCpu = $null; Ev "game start run $run (pid $($g.Id))"
    }
    if ($proc -and $proc.HasExited) {
        $code = try { $proc.ExitCode } catch { "?" }
        Start-Sleep -Seconds 2
        $dst = Join-Path $SessionDir ("modlog_run{0}.log" -f $run)
        if (Test-Path $modLog) { Copy-Item $modLog $dst -Force; Ev "game exit run $run code $code; mod log copied -> $(Split-Path $dst -Leaf)" }
        else { Ev "game exit run $run code $code; no mod log at $modLog" }
        $proc = $null
    }
    # ---- counters ----
    $row = [ordered]@{ time = $now.ToString("yyyy-MM-ddTHH:mm:ss"); game_running = [int][bool]$proc }
    try {
        $cs = (Get-Counter -Counter $counters -ErrorAction SilentlyContinue).CounterSamples
        $v = { param($pat) ($cs | Where-Object { $_.Path -like $pat }) }
        $row.cpu_total_pct = [math]::Round((& $v '*\processor(_total)\% processor time').CookedValue, 1)
        $row.cpu_perf_pct = [math]::Round((& $v '*\processor information(_total)\% processor performance').CookedValue, 1)
        $row.ram_avail_mb = [math]::Round((& $v '*\memory\available mbytes').CookedValue, 0)
        $row.commit_mb = [math]::Round((& $v '*\memory\committed bytes').CookedValue / 1MB, 0)
        $eng = & $v '*\gpu engine(*'
        $row.gpu3d_total_pct = [math]::Round((($eng | Measure-Object CookedValue -Sum).Sum), 1)
        $row.vram_adapter_mb = [math]::Round(((& $v '*\gpu adapter memory(*' | Measure-Object CookedValue -Maximum).Maximum) / 1MB, 0)
        if ($proc) {
            $tag = "*pid_$($proc.Id)_*"
            $row.gpu3d_game_pct = [math]::Round((($eng | Where-Object { $_.InstanceName -like $tag } | Measure-Object CookedValue -Sum).Sum), 1)
            $row.vram_game_mb = [math]::Round((((& $v '*\gpu process memory(*') | Where-Object { $_.InstanceName -like $tag } | Measure-Object CookedValue -Sum).Sum) / 1MB, 0)
        }
    } catch { Ev "counter error: $_" }
    if ($proc) {
        try {
            $proc.Refresh()
            $cpu = $proc.TotalProcessorTime.TotalSeconds
            if ($null -ne $lastCpu) { $row.game_cpu_pct = [math]::Round(100 * ($cpu - $lastCpu) / [math]::Max(0.5, ($now - $lastT).TotalSeconds), 1) }
            $lastCpu = $cpu; $lastT = $now
            $row.game_ws_mb = [math]::Round($proc.WorkingSet64 / 1MB, 0)
            $row.game_private_mb = [math]::Round($proc.PrivateMemorySize64 / 1MB, 0)
        } catch {}
    }
    $line = ($cols.Split(",") | ForEach-Object { if ($row.Contains($_) -and $null -ne $row[$_]) { "$($row[$_])" } else { "" } }) -join ","
    Add-Content -Path $csvPath -Value $line
    # ---- lock lease ----
    if ($RenewLock -and ($now - $lastRenew).TotalMinutes -ge 20) {
        $r = Invoke-GameLock "renew" 3600; $lastRenew = $now; Ev "lock renew: code $($r.code)"
    }
    $sleep = $IntervalSec - ((Get-Date) - $now).TotalSeconds
    if ($sleep -gt 0) { Start-Sleep -Milliseconds ([int]($sleep * 1000)) }
}
if ($proc -and -not $proc.HasExited) { Ev "monitor stopping while the game still runs (run $run)" }
Ev "monitor stopped"
