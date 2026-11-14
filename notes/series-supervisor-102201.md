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
