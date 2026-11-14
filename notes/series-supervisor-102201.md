# series_run.ps1 offline proof (#102201, 2026-11-14 ~3:41-3:45 pm EST, PC2, no game)

Test 1: 4 dummy arms (harness_guard Init-Guard -OwnerPid {OWNER}, 40 s guarded sleep each), lock nioh3-seriestest, RenewSec 20. Launching bash shell killed (taskkill /T) at ~56 s, during arm 2. Supervisor finished all 4 arms, renewed, released, deleted marker.
```
[2026-11-14 12:41:43] START dummy4 supervisor=8616 arms=4 lock=nioh3-seriestest marker=C:/nioh3-re/t102201/test-active
[2026-11-14 12:41:44] watchdog pid=11276
[2026-11-14 12:41:44] LOCK acquired: acquired (device pc2)
[2026-11-14 12:41:44] ARM 1 START: powershell -NoProfile -ExecutionPolicy Bypass -File C:/nioh3-re/t102201/arm.ps1 -N 1 -OwnerPid 8616
[2026-11-14 12:42:05] RENEW ok (#1)
[2026-11-14 12:42:25] ARM 1 END exit=0 (41 s) log=C:/nioh3-re/t102201/status.txt.arm1.log
[2026-11-14 12:42:25] ARM 2 START: powershell -NoProfile -ExecutionPolicy Bypass -File C:/nioh3-re/t102201/arm.ps1 -N 2 -OwnerPid 8616
[2026-11-14 12:42:45] RENEW ok (#1)
[2026-11-14 12:43:06] ARM 2 END exit=0 (41 s) log=C:/nioh3-re/t102201/status.txt.arm2.log
[2026-11-14 12:43:06] ARM 3 START: powershell -NoProfile -ExecutionPolicy Bypass -File C:/nioh3-re/t102201/arm.ps1 -N 3 -OwnerPid 8616
[2026-11-14 12:43:26] RENEW ok (#1)
[2026-11-14 12:43:47] ARM 3 END exit=0 (41 s) log=C:/nioh3-re/t102201/status.txt.arm3.log
[2026-11-14 12:43:47] ARM 4 START: powershell -NoProfile -ExecutionPolicy Bypass -File C:/nioh3-re/t102201/arm.ps1 -N 4 -OwnerPid 8616
[2026-11-14 12:44:07] RENEW ok (#1)
[2026-11-14 12:44:27] ARM 4 END exit=0 (41 s) log=C:/nioh3-re/t102201/status.txt.arm4.log
[2026-11-14 12:44:28] LOCK released: 
[2026-11-14 12:44:28] marker deleted
[2026-11-14 12:44:28] SERIES END ok=1 arms_run=4
arm 1 start owner=8616 alive=True
arm 1 done owner alive=True
arm 2 start owner=8616 alive=True
arm 2 done owner alive=True
arm 3 start owner=8616 alive=True
arm 3 done owner alive=True
arm 4 start owner=8616 alive=True
arm 4 done owner alive=True
```

Test 2: supervisor killed mid-arm 1 -> arm guard aborted (owner gone, cleanup ran); watchdog released lock + deleted marker.
```
[2026-11-14 12:44:36] START dummy4 supervisor=18532 arms=4 lock=nioh3-seriestest marker=C:/nioh3-re/t102201/wd/test-active
[2026-11-14 12:44:37] watchdog pid=9024
[2026-11-14 12:44:37] LOCK acquired: acquired (device pc2)
[2026-11-14 12:44:37] ARM 1 START: powershell -NoProfile -ExecutionPolicy Bypass -File C:/nioh3-re/t102201/arm.ps1 -N 1 -OwnerPid 18532
[2026-11-14 12:44:49] WATCHDOG: supervisor 18532 died without SERIES END -> lock release: 
[2026-11-14 12:44:49] WATCHDOG: marker deleted
[2026-11-14 12:44:49] SERIES END ok=0 (watchdog cleanup)
[12:44:37] GUARD: init name=dummy1 harness=11552 owner=18532 maxMinutes=5 minHeadroom=18GB maxCrashes=2
arm 1 start owner=18532 alive=True
[12:44:48] GUARD: ABORT: owner pid 18532 is gone
arm 1 CLEANUP (owner gone)
```

Lock status after both: holder None (task 449, channel nioh3-seriestest).

## Re-proof after regression #102230 (2026-11-14 3:53-3:57 pm EST)
Fixes: cmd /d /s /c with outer quotes; UO_SERIES_KEEPLOCK=1 in arm env (gamelock release no-op, universal-optimizer fleet-v2 295436e6; soulcore_form honours it); watchdog waits for the arm process tree; Split-IniArg keeps dotted values (59.94); CfgXml logs a missing tag; bridge dir fallback for worktrees.

Run 1: arm 2 = quoted path with a space, launcher killed at ~58 s (12:54:46 PST):
```
[2026-11-14 12:53:48] START dummy4b supervisor=20120 arms=4 lock=nioh3-seriestest marker=C:/nioh3-re/t102201b/r1/test-active
[2026-11-14 12:53:48] watchdog pid=23068
[2026-11-14 12:53:48] LOCK acquired: acquired (device pc2)
[2026-11-14 12:53:48] ARM 1 START: powershell -NoProfile -ExecutionPolicy Bypass -File C:/nioh3-re/t102201b/arm2.ps1 -N 1 -OwnerPid 20120 -CleanupSec 0
[2026-11-14 12:53:48] ARM 1 pid=664
[2026-11-14 12:54:09] RENEW ok (#1)
[2026-11-14 12:54:30] ARM 1 END exit=0 (42 s) log=C:/nioh3-re/t102201b/r1/status.txt.arm1.log
[2026-11-14 12:54:30] ARM 2 START: "C:/nioh3-re/t102201b/q dir/arm.cmd" 2 20120
[2026-11-14 12:54:30] ARM 2 pid=16448
[2026-11-14 12:54:50] RENEW ok (#1)
[2026-11-14 12:55:12] ARM 2 END exit=0 (42 s) log=C:/nioh3-re/t102201b/r1/status.txt.arm2.log
[2026-11-14 12:55:12] ARM 3 START: powershell -NoProfile -ExecutionPolicy Bypass -File C:/nioh3-re/t102201b/arm2.ps1 -N 3 -OwnerPid 20120
[2026-11-14 12:55:12] ARM 3 pid=20732
[2026-11-14 12:55:32] RENEW ok (#1)
[2026-11-14 12:55:54] ARM 3 END exit=0 (42 s) log=C:/nioh3-re/t102201b/r1/status.txt.arm3.log
[2026-11-14 12:55:54] ARM 4 START: powershell -NoProfile -ExecutionPolicy Bypass -File C:/nioh3-re/t102201b/arm2.ps1 -N 4 -OwnerPid 20120
[2026-11-14 12:55:54] ARM 4 pid=19964
[2026-11-14 12:56:14] RENEW ok (#1)
[2026-11-14 12:56:40] ARM 4 END exit=0 (46 s) log=C:/nioh3-re/t102201b/r1/status.txt.arm4.log
[2026-11-14 12:56:40] LOCK released: 
[2026-11-14 12:56:40] marker deleted
[2026-11-14 12:56:40] SERIES END ok=1 arms_run=4
arm 1 start owner=20120 alive=True KEEPLOCK=1
arm 1 runner-style release: release skipped: series supervisor holds the lock (UO_SERIES_KEEPLOCK=1)
arm 1 holder after: 'pc2'
arm 1 done owner alive=True
arm.cmd args: 2 20120
arm 2 start owner=20120 alive=True KEEPLOCK=1
arm 2 runner-style release: release skipped: series supervisor holds the lock (UO_SERIES_KEEPLOCK=1)
arm 2 holder after: 'pc2'
arm 2 done owner alive=True
arm 3 start owner=20120 alive=True KEEPLOCK=1
arm 3 runner-style release: release skipped: series supervisor holds the lock (UO_SERIES_KEEPLOCK=1)
arm 3 holder after: 'pc2'
arm 3 done owner alive=True
arm 4 start owner=20120 alive=True KEEPLOCK=1
arm 4 runner-style release: release skipped: series supervisor holds the lock (UO_SERIES_KEEPLOCK=1)
arm 4 holder after: 'pc2'
arm 4 done owner alive=True
```
Watchdog: supervisor killed 12:57:00, arm cleanup 15 s:
```
[2026-11-14 12:56:49] START dummy4b supervisor=25432 arms=4 lock=nioh3-seriestest marker=C:/nioh3-re/t102201b/wd/test-active
[2026-11-14 12:56:49] watchdog pid=23356
[2026-11-14 12:56:50] LOCK acquired: acquired (device pc2)
[2026-11-14 12:56:50] ARM 1 START: powershell -NoProfile -ExecutionPolicy Bypass -File C:/nioh3-re/t102201b/arm2.ps1 -N 1 -OwnerPid 25432 -CleanupSec 15
[2026-11-14 12:56:50] ARM 1 pid=18056
[2026-11-14 12:57:18] WATCHDOG: supervisor 25432 died; waited 17 s for arm tree 18056 to exit
[2026-11-14 12:57:19] WATCHDOG: supervisor 25432 died without SERIES END -> lock release: 
[2026-11-14 12:57:19] WATCHDOG: marker deleted
[2026-11-14 12:57:19] SERIES END ok=0 (watchdog cleanup)
[12:56:50] GUARD: init name=dummy1 harness=17740 owner=25432 maxMinutes=5 minHeadroom=18GB maxCrashes=2
arm 1 start owner=25432 alive=True KEEPLOCK=1
arm 1 runner-style release: release skipped: series supervisor holds the lock (UO_SERIES_KEEPLOCK=1)
arm 1 holder after: 'pc2'
[12:57:01] GUARD: ABORT: owner pid 25432 is gone
arm 1 CLEANUP start 12:57:01
arm 1 CLEANUP done 12:57:16
```
Lock holder None after both. Split-IniArg 'VRR.MaxFps=59.94' -> VRR|MaxFps|59.94.
