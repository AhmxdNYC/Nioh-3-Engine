# kill_leftovers.ps1 - kill harness leftovers by the EXACT pids recorded in state\harness_pids\*.json (never by process name).
#   -OnlyOrphans : only entries whose owner pid is gone (default: every entry)
#   -DryRun      : list what would be killed
# A pid is killed only if its start time still matches the recorded one (pid reuse safe). Entries are deleted afterwards.
param([switch]$OnlyOrphans, [switch]$DryRun)
$dir = Join-Path (Split-Path -Parent $MyInvocation.MyCommand.Path) "state\harness_pids"
if (-not (Test-Path $dir)) { "no pid files ($dir)"; return }
function Test-Match($pid_, [string]$start) { $p = Get-Process -Id $pid_ -ErrorAction SilentlyContinue; if (-not $p) { return $null }; try { if ($p.StartTime.ToString("o") -eq $start) { return $p } } catch {}; return $null }
$n = 0
foreach ($f in Get-ChildItem $dir -Filter *.json) {
    $j = Get-Content $f.FullName -Raw | ConvertFrom-Json
    $owner = Test-Match $j.ownerPid $j.ownerStart
    if ($OnlyOrphans -and $owner) { "skip $($f.Name): owner $($j.ownerPid) alive"; continue }
    "entry $($f.Name): harness $($j.harnessPid) owner $($j.ownerPid) $(if($owner){'alive'}else{'GONE'})"
    $targets = @(@{ pid = $j.harnessPid; start = $j.harnessStart; name = "harness" }) + @($j.children) + @($j.games)
    foreach ($t in $targets) {
        if (-not $t -or -not $t.pid) { continue }
        $p = Test-Match $t.pid $t.start
        if ($p) { $n++; if ($DryRun) { "  would kill $($t.name) pid $($t.pid)" } else { Stop-Process -Id $t.pid -Force; "  killed $($t.name) pid $($t.pid)" } }
    }
    if (-not $DryRun) { Remove-Item $f.FullName -Force; Remove-Item ([IO.Path]::ChangeExtension($f.FullName, ".log")) -Force -ErrorAction SilentlyContinue }
}
"done: $n process(es) $(if($DryRun){'would be '})killed"
foreach ($l in Get-ChildItem $dir -Filter *.log -ErrorAction SilentlyContinue) { if (-not (Test-Path ([IO.Path]::ChangeExtension($l.FullName, ".json")))) { if (-not $DryRun) { Remove-Item $l.FullName -Force } } }   # orphan guard logs
