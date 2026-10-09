# run8b (PC, ASI 1.7.43, 2026-10-09 00:53-01:40): Block C capped at 60 fps, Block A "dead/hung"

Offline diagnosis. The game was not launched (the Ally held the lock). Sources: `captures\tm\abba2\run8b_*`, guard logs in
`tools\testmatrix\state\harness_pids\`, the Windows event logs, and the current display mode.

## 1. Block C: 60.00 fps in every window. Cause: the desktop was at 60 Hz

- Every C window log reads `vrr tier 60 cap 60 armed 60` (run7: `vrr tier 120 cap 240`), although test_base.ini has
  `[VRR] MaxFps = 240`. The mod is not at fault. It logs `VRR tier 120: armed=1 ... limiter cap 120` at boot, but the
  game itself chose its 60 tier.
- config.xml is not the cause. The test config keeps `REN_FPS 120`, `VSYNC ON`, `DISP_MODE BORDERLESS`, `MONITOR_ID MONITOR 1`,
  and Set-TestCfg changes only FG, REN_RAT and the upscaler. The FG state is not the cause either: FG is OFF in every C window.
- Display: there is now a single attached display, `\\.\DISPLAY1` (Acer XV272U W2). It runs 2560x1440 **@ 60 Hz**, although
  the panel offers **240 Hz** at that resolution. MODLOG 2026-10-06 14:15 records the earlier user-approved 60 -> 240 change.
  Unplugging the second monitor reset the mode to 60 Hz.
- Timeline: run8b J L1 and L2 (00:57-01:11) still read tier 120 cap 240. J L3 (launched 01:11, windows from 01:14) and all of
  Block C and P read tier 60. The title-to-field frame counter shows the same thing: about 11 000 frames at entry before the
  change, against about 3 000 after it. The refresh rate dropped at about 01:11-01:14.
- With the fixed report, `run8b_C` reads "CAPPED: 9 of 15 windows ... game capped at 60 fps (vrr tier 60)" and MA reads
  **INVALID**. The old report said DROP. Re-run C.

**Harness fix** (`harness_guard.ps1`, `abba2_pc.ps1`, `pc_retest_run8.ps1`, `abba2_report.py`):
- `Ensure-DisplayRefresh N` checks the attached displays (current mode and the highest refresh at that resolution). If the
  primary display runs below N Hz, it is raised to its highest refresh for the run. The change is dynamic, uses
  ChangeDisplaySettingsEx flags 0, and is never written to the registry. `Restore-DisplayRefresh` puts the old mode back in
  the end and cleanup paths. If the display cannot reach N (or `-NoForceRefresh` is passed), the run refuses to start and
  prints why.
  - `pc_retest_run8` raises the display to 240 Hz once for the whole run.
  - `abba2_pc` needs `max(120, -MaxFps)`.
  - `-DryRun` only reports the display state.
- `Assert-NotCapped` (abba2) runs after each warm-up and before every window. It reads the mod's latest
  `vrr tier T cap C` and aborts if `T < 120` or `C < max(120, -MaxFps)`, logging the cap, the tier and the current display
  modes. This is the authoritative check, because it covers the display, REN_FPS, VSync and the tier logic alike.
- `Set-TestCfg` logs REN_FPS, VSYNC, DISP_MODE, MONITOR_ID, SCR_REZ and DYNAMIC_FPS, and refuses to run if REN_FPS < 120.
- `abba2_report.py` rejects a window whose log shows a game cap below the run's cap, and adds a `CAPPED:` summary line.
  The selftest covers this case (`tier60 INVALID`).
- The user can restore the refresh permanently in Windows display settings: 240 Hz on the XV272U.

## 2. Block A "game dead/hung before L1_BASE_s1": a foreign harness closed the game. Not a mod bug

- Timeline: boot was fine. vpad title -> field happened at 00:55:01 (frame 11152, 1.7.43, all six items installed = 2), so
  this is not the 1.7.42 boot hang. The 60 s warm-up ran from 00:55:05 to 00:56:05, then `Game-Alive` was false. The
  Game-Alive version that ran logged no reason; the reason lines came only in 83fa80c.
- The process was already gone. A's `finally` Close-GameClean returned at once, with no 25 s wait and no 8 s sleep, and
  Block J started at 00:56:06. No crash evidence exists: no Application Error/Hang event for Nioh3, no WER report, and nothing
  new in `%LOCALAPPDATA%\KoeiTecmo\NIOH3\dump` (newest is 10-06) or in `CrashDumps`.
- The culprit is `state\harness_pids\pc_gpu_session_34440.log`. It is a `pc_gpu_session.ps1 -DryRun` (`minHeadroom=0GB` is the
  DryRun signature; committed "DryRun OK" in b55e96d at 00:58). It ran from 00:51:27, its guard saw A's game boot (commit
  jumps 45 -> 61 GB from 00:54:07), and it ended at **00:55:39**, inside A's warm-up. Its `Restore-All` calls `Close-Game`,
  which did `Get-Process Nioh3` -> CloseMainWindow -> Stop-Process on **every** Nioh3, by name, even in -DryRun. That closed
  A's game 26 s before the check.
- The mod log of that launch was overwritten by later launches. The rest of the evidence is consistent and needs no mod
  explanation, so there is no src change, no build and no proof boot. The next Block A re-run is the proof.

**Harness fix:**
- `pc_gpu_session.ps1`: `Close-Game` now acts only on the games this session launched (guard-registered pid with matching
  start time), and does nothing in -DryRun. The DryRun still ends "DryRun OK".
- `abba2_pc.ps1`:
  - The game's Process handle is kept, so "process gone" logs the exit code and time.
  - On a dead/hung abort, `Dead-Report` copies the game log to `abort_game.log` and traces its last lines.
  - It lists the other harness guard logs active since the launch (`Get-ForeignGuardActivity`) and any dumps written since.

## Next PC slot
`tools\testmatrix\pc_retest_run8.ps1 -Blocks A,C -Tag run8c`. It refuses or raises the refresh itself. Check that the trace shows
`L1 cap check OK: vrr tier 120 cap 240`.
