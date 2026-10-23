# sensor_collector.ps1 - standalone 1 Hz Windows sensor logger, HWiNFO-style CSV that tm.py sensors/ingest and run_summary.py accept.
#   powershell -NoProfile -ExecutionPolicy Bypass -File sensor_collector.ps1 -Out run.csv [-Seconds 600] [-GamePid <game pid>]
# Without -GamePid the GPU Engine wildcard is slow on a busy machine; every row carries its true sample time.
# Start-up takes ~10-15 s before the first row. Stop with Ctrl+C or -Seconds (a stop file "<Out>.stop" also ends it). Local clock, same as PresentMon/ISO windows.
# Columns (names chosen to match tm.SENS_PAT): Total CPU Usage [%], Core Clocks (avg) [MHz], GPU Core Load [%], GPU Memory Dedicated [MB].
# GPU load = sum of the 3D engine counters (of -GamePid when given, else all processes), capped at 100. GPU memory = largest adapter's
# dedicated usage. Temperatures and power are NOT available from WMI/perf counters: no column is written and <Out>.meta.json says so.
# Missing data is left BLANK (never 0): no valid GPU samples -> blank + reason in the meta; samples whose Status is not valid are dropped.
# -SelfTest runs the row builder on synthetic samples (no sampling, no Out file needed).
param([string]$Out = "", [int]$Seconds = 0, [int]$GamePid = 0, [switch]$SelfTest)
$ErrorActionPreference = "Stop"
$whyCount = @{}; $stamps = New-Object Collections.Generic.List[datetime]
$meta = [ordered]@{ started = (Get-Date -f s); interval_s = 1; pid_filter = $(if ($GamePid) { $GamePid } else { "none (all processes)" }); columns = [ordered]@{}; unavailable = [ordered]@{
    cpu_temp = "no WMI/perf counter for CPU die temperature (needs HWiNFO/vendor driver)"; gpu_temp = "no perf counter for GPU temperature"
    cpu_w = "no perf counter for CPU package power"; gpu_w = "no perf counter for GPU power"
    gpu_mhz = "no perf counter for GPU clock" }
    provenance = [ordered]@{
        "Total CPU Usage [%]" = "Processor Utility (_Total), capped at 100"
        "Core Clocks (avg) [MHz]" = "ESTIMATE: Win32_Processor MaxClockSpeed x Processor Performance %, not a measured clock"
        "GPU Core Load [%]" = "SUM of the 3D-engine utilization counters of the selected process (-GamePid) or of all processes, capped at 100; NOT whole-GPU utilization"
        "GPU Memory Dedicated [MB]" = "largest single adapter Dedicated Usage" }
    timing = "rows carry the actual counter sample time; the loop is a ~1 s request, not an exact 1 Hz clock. Gaps are summarised in timing_result at the end." }
$base = [double](Get-CimInstance Win32_Processor | Select-Object -First 1).MaxClockSpeed
function Build-Row($all) {
    $v = @{}; $why = @{}
    $s = @($all | Where-Object { $_.Status -eq 0 -or $_.Status -eq 1 })   # PDH valid / new data only
    $bad = @($all).Count - $s.Count; if ($bad) { $why["rejected_samples"] = $bad }
    $x = @($s | Where-Object Path -like "*% processor utility"); if ($x.Count) { $v["Total CPU Usage [%]"] = [math]::Min(100, [math]::Round($x[0].CookedValue, 1)) }
    $x = @($s | Where-Object Path -like "*% processor performance"); if ($x.Count -and $base) { $v["Core Clocks (avg) [MHz]"] = [math]::Round($base * $x[0].CookedValue / 100, 0) }
    $g = @($s | Where-Object { $_.Path -like "*utilization percentage" -and $_.InstanceName -like "*engtype_3D*" -and (-not $GamePid -or $_.InstanceName -like "pid_${GamePid}_*") })
    if ($g.Count) { $v["GPU Core Load [%]"] = [math]::Min(100, [math]::Round(($g | Measure-Object CookedValue -Sum).Sum, 1)) } else { $why["GPU Core Load [%]"] = "no valid 3D-engine sample for the selection" }
    $m = @($s | Where-Object { $_.Path -like "*dedicated usage" })
    if ($m.Count) { $v["GPU Memory Dedicated [MB]"] = [math]::Round((($m | Measure-Object CookedValue -Maximum).Maximum) / 1MB, 0) } else { $why["GPU Memory Dedicated [MB]"] = "no valid adapter sample" }
    return @{ v = $v; why = $why }
}
function Sm($path, $inst, $val, $st) { [pscustomobject]@{ Path = $path; InstanceName = $inst; CookedValue = $val; Status = $st } }
if ($SelfTest) {
    $base = 3700.0; $GamePid = 42
    $u = "\h\gpu engine(pid_42_x_engtype_3D)\utilization percentage"
    $r = Build-Row @((Sm $u "pid_42_luid_0_phys_0_eng_0_engtype_3D" 55 0), (Sm $u "pid_7_x_engtype_3D" 30 0))
    if ($r.v["GPU Core Load [%]"] -ne 55) { throw "pid filter: $($r.v['GPU Core Load [%]'])" }
    $r = Build-Row @((Sm $u "pid_7_x_engtype_3D" 30 0))
    if ($r.v.ContainsKey("GPU Core Load [%]") -or -not $r.why["GPU Core Load [%]"]) { throw "no selection must stay blank with a reason" }
    $r = Build-Row @((Sm $u "pid_42_x_engtype_3D" 80 2))
    if ($r.v.ContainsKey("GPU Core Load [%]") -or $r.why["rejected_samples"] -ne 1) { throw "bad status must be rejected" }
    $r = Build-Row @((Sm $u "pid_42_a_engtype_3D" 70 0), (Sm $u "pid_42_b_engtype_3D" 60 1))
    if ($r.v["GPU Core Load [%]"] -ne 100) { throw "cap 100" }
    $r = Build-Row @((Sm "\h\processor information(_total)\% processor performance" "_total" 110 0))
    if ($r.v["Core Clocks (avg) [MHz]"] -ne 4070) { throw "clock estimate" }
    "selftest ok"; return
}
if (-not $Out) { throw "-Out is required" }
$cands = [ordered]@{
    cpu_pct = @{ name = "Total CPU Usage [%]"; path = "\Processor Information(_Total)\% Processor Utility" }
    cpu_perf = @{ name = $null; path = "\Processor Information(_Total)\% Processor Performance" }
    gpu_load = @{ name = "GPU Core Load [%]"; path = "\GPU Engine(*)\Utilization Percentage" }
    gpu_mem = @{ name = "GPU Memory Dedicated [MB]"; path = "\GPU Adapter Memory(*)\Dedicated Usage" } }
$ok = @{}
$sets = @{}; foreach ($n in "Processor Information", "GPU Engine", "GPU Adapter Memory") { $sets[$n] = [bool](Get-Counter -ListSet $n -ErrorAction SilentlyContinue) }
$setOf = @{ cpu_pct = "Processor Information"; cpu_perf = "Processor Information"; gpu_load = "GPU Engine"; gpu_mem = "GPU Adapter Memory" }
foreach ($k in $cands.Keys) { $ok[$k] = $sets[$setOf[$k]]; if (-not $ok[$k]) { $meta.unavailable[$k] = "counter set missing: $($setOf[$k])" } }
# with -GamePid only that process's 3D engines are sampled (far cheaper than the (*) wildcard)
if ($GamePid) { $cands.gpu_load.path = "\GPU Engine(pid_${GamePid}_*engtype_3D)\Utilization Percentage" }
$cols = @()
if ($ok.cpu_pct) { $cols += "Total CPU Usage [%]" }
if ($ok.cpu_perf -and $base) { $cols += "Core Clocks (avg) [MHz]"; $meta.columns["Core Clocks (avg) [MHz]"] = "base-derived ESTIMATE: $base MHz x % Processor Performance" }
if ($ok.gpu_load) { $cols += "GPU Core Load [%]" }
if ($ok.gpu_mem) { $cols += "GPU Memory Dedicated [MB]" }
foreach ($c in $cols) { if (-not $meta.columns.Contains($c)) { $meta.columns[$c] = "perf counter" } }
$paths = @($cands.Keys | Where-Object { $ok[$_] } | ForEach-Object { $cands[$_].path })
if (-not $paths.Count) { throw "no counters available" }
$meta | ConvertTo-Json -Depth 4 | Set-Content "$Out.meta.json" -Encoding UTF8
$sw = [IO.StreamWriter]::new($Out, $false, [Text.UTF8Encoding]::new($false))
$sw.WriteLine((@("Date", "Time") + $cols) -join ","); $t0 = Get-Date
try {
  Get-Counter -Counter $paths -SampleInterval 1 -Continuous | ForEach-Object {
    $r = Build-Row $_.CounterSamples; $v = $r.v
    foreach ($k in $r.why.Keys) { $whyCount[$k] = 1 + [int]$whyCount[$k] }
    $now = $_.Timestamp; $stamps.Add($now)
    $sw.WriteLine((@($now.ToString("dd.MM.yyyy", [Globalization.CultureInfo]::InvariantCulture), $now.ToString("HH:mm:ss.f", [Globalization.CultureInfo]::InvariantCulture)) + ($cols | ForEach-Object { if ($v.ContainsKey($_)) { "$($v[$_])" } else { "" } })) -join ","); $sw.Flush()
    if (($Seconds -and ($now - $t0).TotalSeconds -ge $Seconds) -or (Test-Path "$Out.stop")) { break }
  }
} finally {
  $sw.Dispose()
  $gaps = @(); for ($i = 1; $i -lt $stamps.Count; $i++) { $gaps += ($stamps[$i] - $stamps[$i - 1]).TotalSeconds }
  $meta.timing_result = [ordered]@{ rows = $stamps.Count; max_gap_s = $(if ($gaps) { [math]::Round(($gaps | Measure-Object -Maximum).Maximum, 2) } else { $null })
      gaps_over_1_5s = @($gaps | Where-Object { $_ -gt 1.5 }).Count; blank_or_rejected_counts = $whyCount }
  $meta | ConvertTo-Json -Depth 5 | Set-Content "$Out.meta.json" -Encoding UTF8
}
