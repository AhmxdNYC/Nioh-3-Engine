# test_guard.ps1 - dry-run self test of harness_guard.ps1 / kill_leftovers.ps1. Launches NO game, sends NO input, touches no ini.
#   powershell -NoProfile -ExecutionPolicy Bypass -File test_guard.ps1
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
$g = Join-Path $here "harness_guard.ps1"; $fail = 0
function Run([string]$name, [string]$code, [int]$expect, [hashtable]$env_ = @{}, [int]$maxSec = 60) {
    $tmp = Join-Path $env:TEMP "hgtest_$name.ps1"; Set-Content $tmp ". '$g'`n$code" -Encoding UTF8
    $psi = New-Object Diagnostics.ProcessStartInfo "powershell.exe", "-NoProfile -ExecutionPolicy Bypass -File `"$tmp`""
    $psi.UseShellExecute = $false; $psi.RedirectStandardOutput = $true; foreach ($k in $env_.Keys) { $psi.EnvironmentVariables[$k] = $env_[$k] }
    $t0 = Get-Date; $p = [Diagnostics.Process]::Start($psi); $out = $p.StandardOutput.ReadToEnd()
    if (-not $p.WaitForExit($maxSec * 1000)) { $p.Kill() }
    $sec = [int]((Get-Date) - $t0).TotalSeconds; $ok = ($p.ExitCode -eq $expect)
    "{0,-4} {1,-22} exit={2} (want {3}) {4}s | {5}" -f $(if ($ok) { "PASS" } else { "FAIL" }), $name, $p.ExitCode, $expect, $sec, (($out -split "`n" | Where-Object { $_ -match "GUARD" } | Select-Object -Last 1) -replace "\s+$", "")
    if (-not $ok) { $script:fail++ }
}
# 1 dead owner at start
Run "dead_owner_at_start" 'Init-Guard -Name t1 -OwnerPid 999999; "SHOULD NOT REACH"; Assert-Launch; "LAUNCH"' 99
# 2 owner dies mid-wait (long Start-Sleep must notice within ~2 s)
$o = Start-Process powershell -ArgumentList "-NoProfile -Command Start-Sleep 600" -PassThru -WindowStyle Hidden
$k = Start-Job { param($id) Start-Sleep 6; Stop-Process -Id $id -Force } -ArgumentList $o.Id
Run "owner_dies_mid_sleep" "Init-Guard -Name t2 -OwnerPid $($o.Id); Start-Sleep 120; 'SHOULD NOT REACH'" 99 @{} 60
Wait-Job $k | Out-Null; Remove-Job $k
# 3 low headroom -> abort at Assert-Launch (before any launch)
Run "low_headroom" "Init-Guard -Name t3 -OwnerPid $PID; Assert-Launch; 'LAUNCHED?'" 99 @{ HG_MIN_HEADROOM_GB = "9999"; HG_HEADROOM_WAIT_SEC = "6" }
# 4 headroom ok (needs 1 GB) -> proceeds
Run "headroom_ok" "Init-Guard -Name t4 -OwnerPid $PID; Assert-Launch; 'GUARD: reached launch point'; Done-Guard" 0 @{ HG_MIN_HEADROOM_GB = "1" }
# 5 two crashes -> no relaunch
Run "two_crashes" "Init-Guard -Name t5 -OwnerPid $PID; Note-Crash a; Assert-Launch; 'GUARD: relaunch 1 allowed'; Note-Crash b; Assert-Launch; 'SHOULD NOT REACH'" 99 @{ HG_MIN_HEADROOM_GB = "1" }
# 6 MaxMinutes
Run "max_minutes" "Init-Guard -Name t6 -OwnerPid $PID -MaxMinutes 0.05; Start-Sleep 60; 'SHOULD NOT REACH'" 99 @{} 40
# 7 pid file + kill_leftovers kills exact registered pid and removes the entry
$victim = Start-Process powershell -ArgumentList "-NoProfile -Command Start-Sleep 600" -PassThru -WindowStyle Hidden
$bystander = Start-Process powershell -ArgumentList "-NoProfile -Command Start-Sleep 600" -PassThru -WindowStyle Hidden
# a "harness" that registers only the victim, then dies without cleanup (leaves its pid file behind)
Run "leftover_harness" "Init-Guard -Name t7 -OwnerPid $PID; Register-Child (Get-Process -Id $($victim.Id)) victim; [Environment]::Exit(0)" 0
$left = @(Get-ChildItem (Join-Path $here "state\harness_pids") -Filter "t7_*.json")
"pidfile left behind: $($left.Count)"
& "$here\kill_leftovers.ps1" -DryRun | Select-Object -Last 3
& "$here\kill_leftovers.ps1" | Select-Object -Last 3
Start-Sleep 1
$vDead = -not (Get-Process -Id $victim.Id -ErrorAction SilentlyContinue); $bAlive = [bool](Get-Process -Id $bystander.Id -ErrorAction SilentlyContinue)
$gone = @(Get-ChildItem (Join-Path $here "state\harness_pids") -Filter "t7_*.json").Count -eq 0
$ok7 = $vDead -and $bAlive -and $gone -and $left.Count -eq 1
"{0,-4} kill_leftovers: victim killed=$vDead, unregistered bystander untouched=$bAlive, entry removed=$gone" -f $(if ($ok7) { "PASS" } else { "FAIL" })
if (-not $ok7) { $fail++ }
Stop-Process -Id $bystander.Id -Force -ErrorAction SilentlyContinue
# 8 measuring lock: held while running, removed by Done-Guard and by Guard-Fail (run7 quiet-machine guard)
$lk = Join-Path ([IO.Path]::GetTempPath()) "nioh3_measuring.lock"
Run "lock_done" "Init-Guard -Name t8 -OwnerPid $PID; Set-MeasureLock x; if ((Test-MeasureLock).pid -ne `$PID) { exit 5 }; Done-Guard; if (Test-Path '$lk') { exit 6 }; 'GUARD: lock cleared'" 0
Run "lock_fail" "Init-Guard -Name t9 -OwnerPid $PID -MaxMinutes 0.02; Set-MeasureLock x; Start-Sleep 30" 99 @{} 40
$ok9 = -not (Test-Path $lk); "{0,-4} lock removed after Guard-Fail: $ok9" -f $(if ($ok9) { "PASS" } else { "FAIL" }); if (-not $ok9) { $fail++ }
# 9 foreign load: a 1-core spin started AFTER the exclude set was taken is foreign -> noisy; with a fresh exclude it is ours -> quiet
Run "quiet_guard" ("`$ex = Get-QuietExclude; `$p = Start-Process powershell -ArgumentList '-NoProfile -Command `$t=Get-Date; while(((Get-Date)-`$t).TotalSeconds -lt 7){}' -PassThru -WindowStyle Hidden; " +
    "Start-Sleep -Milliseconds 800; `$a = Measure-Foreign 2 `$ex; `$b = Measure-Foreign 2 (Get-QuietExclude); 'GUARD: foreign ' + `$a.cores + ' noisy=' + `$a.noisy + ' / own ' + `$b.cores + ' noisy=' + `$b.noisy; " +
    "`$p.WaitForExit(10000) | Out-Null; if (`$a.noisy -and -not `$b.noisy) { exit 0 } else { exit 7 }") 0
"== $(if ($fail) { "$fail FAILED" } else { 'ALL PASS' })"
exit $fail