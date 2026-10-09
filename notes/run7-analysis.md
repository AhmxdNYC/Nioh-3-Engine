# run7 (PC, ASI 1.7.39, 2026-10-08): verdicts, AABB toggle failure, noise guard

Runs: `captures\tm\abba2\run7_A` (Block A: `-Live AABB,DQ,P1E7 -LaunchFlags none -MaxFps 240 -Input vpad`) and `run7_C`
(Block C: `-Arms BASE,MA,BASE,MA,BASE`). Re-analysed with the fixed report (cap rule below) on copies; captures untouched.

## 1. Verdicts

| item | report as run | fixed cap rule | reading |
|---|---|---|---|
| AABB | not measured (toggle "NOT confirmed", skipped) | - | **AABB was ON for the whole of Block A** (root cause, section 2), so the BASE windows were not stock and DQ / P1E7 were measured with AABB ON. Re-run. |
| DQ | INVALID (1 valid quad) | INVALID (q3 now fails on OFF-OFF 5.7 %, not the cap) | The one valid quad (q2: -5.8 %) does **not** show that DQ ON is worse. See below. Re-run. |
| P1E7 | INVALID (1 valid quad) | INVALID | Only the quiet quad q3 is valid: +0.25 %, p99 -1 %. Neutral so far. Measured with DQ and AABB ON. |
| MA | INVALID (L3 BASE had 1 valid window) | DROP: d +1.43 % / -2.48 % vs interpolated BASE | BASE launches went 117.5 -> 120.3 -> 125.6 (range 6.9 %), and windows inside one launch swung 4-7 %. No evidence of a gain, but this run is too noisy to call it neutral. Re-run with the guard. |

**DQ ON looked worse only during the noisy stretch.** After its quads the harness leaves DQ ON (the `drawprep` line reads
`DQ ON` in all 12 P1E7 windows). The quiet tail of the run (P1E7 q2_2 to q3_4, 20:09-20:13) ran 124.2-125.3 fps **with DQ ON**,
against 121.0-122.5 for the DQ-off L1 BASE windows. The DQ quads (19:56-20:05) sat at 107-123 fps, and system RAM
available fell from 7.9 to 6.6 GB around 19:59, a foreign allocation of about 1.3 GB. The mechanism reads back correctly:
`early retries` grow only in ON windows and stay frozen across OFF windows.

**System commit was near its limit for the whole run.** It sat at about 98 % (54.5-56.1 of 55.0-56.9 GB; the page file
grew during the run), and the game itself held only 16.6 GB private. About 38 GB was held by other processes, which
backs up the "other agents were working" diagnosis.

## 2. AABB: root cause and source fix

`Prepare` installed every `-Live` item missing from the base ini as `= 2` (installed, switched off), assuming that ASI
1.7.39 honours 2. It does not for AABB. `dllmain.cpp` installs it with `aabb_enable = 1` for any nonzero value. Only ED,
WG, DS, CP, SPF, MQ, PT1, DL, DQ and P1E7 (and BPO in 1.7.40) set their switch from `== 1`. B, PP, FPC, M1, X1, E1, GF and
WL have the same "2 = ON" behaviour, and `matrix.json` already reflects this: AABB has no `live_installed_value`.

What happened in run7: AABB booted ON. `vpad menu set AABB 1` reached `menu::RunRemote`, which presses only when the
state differs (`if ((cur != 0) != (q.value != 0)) Change(id, 0);`), so nothing changed and no `MENU: ... -> ON` line was
logged. The reply said `(value 1)`, but the harness ignored it, waited 6 s for the log line and skipped the item.

The source fix below is against integ `5b271f5`. It is **not applied** and is for the src owner to merge:

```diff
--- a/mod/Nioh3EngineFix/src/dllmain.cpp
+++ b/mod/Nioh3EngineFix/src/dllmain.cpp
@@ -1717,9 +1717,10 @@
     });
     exDone[menu::kExpAABB] = expGroup("AABB", cfg.aabb != 0, [&]() {
         aabb_resume = (uint64_t)(uintptr_t)(findItem("AABB")->addr + 5);  // 6EA55 push rbp
-        aabb_enable = 1;
+        aabb_enable = cfg.aabb == 1 ? 1u : 0u;  // 2 = installed with the switch off (live A/B from the menu / vpad)
         return true;
     });
+    if (exDone[menu::kExpAABB]) Log("AABB: SSE character AABB cave installed, switch %s", aabb_enable ? "ON" : "off");
     exDone[menu::kExpM1] = expGroup("M1", cfg.m1 != 0, [&]() {
--- a/mod/Nioh3EngineFix/src/menu.cpp
+++ b/mod/Nioh3EngineFix/src/menu.cpp
@@ -1210,7 +1210,7 @@
         if ((e == kExpED || e == kExpWG || e == kExpDS || e == kExpCP || e == kExpSPF || e == kExpMQ || e == kExpPT1 || e == kExpDL ||
-             e == kExpDQ || e == kExpP1E7 || e == kExpBPO) && g_host.exp[e].enable &&
+             e == kExpDQ || e == kExpP1E7 || e == kExpBPO || e == kExpAABB) && g_host.exp[e].enable &&
             !*g_host.exp[e].enable && v == 0)
```

The cave already tests `aabb_enable` on every call (`caves.asm` `test byte ptr [aabb_enable], 1`), so this is a pure
switch change with no new bytes. When the fixed ASI ships:
- add `"live_installed_value": "2"` to AABB in `tools\testmatrix\matrix.json`;
- update the ini comment to `; 1 = on, 2 = installed, switched off (live A/B)`.

## 3. Harness fixes (applied, `tools\testmatrix`)

- **Install values.** `abba2_pc.ps1` `Prepare` writes 2 only for items whose matrix entry has `live_installed_value` 2.
  Every other item gets 1 and boots ON. A base-ini 2 on an item without 2-support also counts as ON.
- **Boot read-back (vpad).** After the boot, each live item is read with `menu get` and corrected if it differs. Items
  that boot ON are switched OFF before the BASE windows, so the BASE windows are stock.
- **Toggle confirmation.** `Toggle` also accepts the `menu set` read-back `(value N)` when it already equals the target
  (a no-op logs no MENU line), and it now puts the reply in the failure trace.
- **`-SrcRoot <tree>`.** `menu_map.py` now takes `--menu-cpp`, so menu rows and the matrix can come from the tree the
  installed ASI was built from, for example `D:\nioh3-wt\integ` for 1.7.40 BPO. Main's `menu.cpp` has no BPO row.
  `menu_map.py` also carries integ's identical rematch hunk, so the later merge is clean.
- **Cap rule (`abba2_report.py`).** `-MaxFps 240` did lift the cap: every OSD stats line in run7 reads
  `vrr tier 120 cap 240`, windows reached 125-129 fps, and the ini key `[VRR] MaxFps = 240` is correct. The bug was in
  `window_ok`, which hard-coded 120 and rejected 119.5 / 120.05 fps windows "at the 120 fps cap". It now rejects only
  within 1 % of the run's real cap, read from `test_base.ini`.

## 4. Quiet-machine guard (`harness_guard.ps1`, used by `abba2_pc.ps1`)

**How load is measured.** `Measure-Foreign` takes two `TotalProcessorTime` snapshots of every process about 2 s apart.
Foreign load is the sum over all processes except:
- Nioh3;
- the harness's own process tree, including its dotnet fsi testpopup, ui and vpad.py children;
- registered children such as PresentMon;
- Idle, System, Registry, dwm, audiodg and csrss.

**When it counts as noisy.** A sample is noisy when foreign load is above 1.0 logical core (`QG_MAX_CORES`, or
`-QuietMaxCores`), or when any build or analysis process (cl, link, msbuild, ml64, java, dotnet, py, node, ...) uses
more than 0.25 core (`QG_HEAVY_CORES`).

**Before each window.** `Wait-Quiet` waits for 2 quiet samples in a row and writes a `QUIET: waiting ...` trace line
every 30 s with the top processes. After 15 min (`QG_WAIT_MIN`) it measures anyway.

**During each window.** The window is sampled every 2 s. It is noisy when 2 or more samples are noisy, or one sample is
above 2 cores. A noisy window is redone (up to `-MaxNoisyRedo`, default 2) and logged in the manifest as
`kind=noisy_discard`. If it is still noisy after that, it is kept with `noisy=true` and `abba2_report.py` rejects it.
`report.txt` has a `noisy windows:` section, and each window line shows `fgn <max foreign cores>` and `NOISY`.
`-NoQuietGuard` turns all of this off.

**Lock file.** `%TEMP%\nioh3_measuring.lock` (`C:\Users\Ahmad Hamza\AppData\Local\Temp\`) holds JSON with the harness
pid and its state. It exists from the harness start until `Done-Guard` or `Guard-Fail` removes it. Agents must not
start build.bat, Ghidra, dotnet fsi proofs or long py jobs while it exists **and** its pid is alive. To check from
PowerShell: `. tools\testmatrix\harness_guard.ps1; Test-MeasureLock` (returns `$null` = free).

**Selftests (all pass).**
- `abba2_report.py --selftest`, including new cases: cap 240 does not reject 120 fps, a noisy window rejects its quad
  and is listed, and the same data at cap 120 rejects.
- `menu_map.py --selftest`.
- `test_guard.ps1`: ALL PASS, including lock_done, lock_fail and quiet_guard. In quiet_guard, a foreign 1-core spin
  reads as noisy at 1.23 cores, and the same spin as the harness's own child reads 0.2 cores and quiet.
- DryRun `-Live AABB,BPO,DQ,P1E7 -Input vpad -MaxFps 240 -SrcRoot D:\nioh3-wt\integ`: AABB=1 boots ON; BPO, DQ and P1E7
  are 2 and boot OFF.

## 5. Next run

Rerun Block A with `-Live AABB,BPO,DQ,P1E7 -SrcRoot D:\nioh3-wt\integ` once the 1.7.40 ASI is installed. Then rerun
Block C. Keep the machine quiet: honour the lock.
