# PC 26 s load: where the "~15 s unexplained" went, what really bounds it, next fix (2026-11-12)

Offline only: existing logs + notes + RVAs already in the notes. No game run, no build, no bridge message.
Item: HIGH "loading ~15 s of a 26 s load unexplained, save ~10 s" (#79534/#80342, role-performance.md, perf-advice item 11).
Phase tool used: `py -I tools/loading/ally_load_phases.py <EngineFix log>` (works on PC logs too).

## 0. Answer
- **The "15 s unexplained" was not load work. It was the tip page waiting for the harness's key press.** The number
  comes from the 1.7.16 runs `captures/pc/loadstats/loadstats_{BASE_a,BASE_b,GL1,LC,LW}.log` (loading-bottleneck.md
  §2 row 1). 1.7.16 had no AutoContinue (1.7.22) and no LoadGates (1.7.17). So the window ran until
  `loadstats_ab.ps1` / `Ensure-InGame` (patcher/common.ps1:18, Space/Esc every ~4-5 s) happened to land a Space press.
  The Ally's "fixed 118.7 s" was the same artefact (loading-ally-fixed.md §7.1).
- Every later PC log has AutoContinueLoad=1 and LoadGates=1: 119 PC windows with LoadGates, 11-36 s.
  - In all of them the clear comes 1.7-1.8 s after the last LoadIsLoadEnd eval.
  - Manager A's "last busy" equals that last eval.
  - There is no idle post-drain gap on PC.
- A real 26 s PC load is fully accounted for. It is ~24 s of resource-manager A running at its in-flight cap of 10,
  then 1.7 s of tip-page close. What is still open is *why each manager-A request is slow*, not *where the time goes*.

## 1. The 26 s broken into phases (seconds from the 58A set; source = log line or derived)
| phase | 1.7.16 BASE_a (the "26 s") | titlectl 25.7 s | ow185-7356 26.2 s | dpt-29589 29.7 s | source |
|---|---|---|---|---|---|
| loading UI before the set / after the clear (fixed fades) | 2.10 / 0.59 | same | same | same | loading-bottleneck §1, MENU lines |
| set -> stage ready | 0.6 | 0.8 | 1.0 | 0.6 | `stage ready +x` |
| pre-gate: no LoadIsLoadEnd eval yet, mgr A at cap | not logged | to +8.8 (~8.0) | to +8.2 (~7.2) | to +6.7 (~6.1) | first gate fail (tool) |
| G2 StageAreaListBusy | - | 2.71 | 3.98 | 7.50 | LoadGates held |
| G8 StageAreasObjectsReady (area objects wait for their resources) | - | 7.52 | 8.75 | 6.14 | LoadGates held |
| G9 first zero + G5 ObjectListAnyNotReady + G7 (2nd mgr-A batch) | - | 0.19+2.22+0.01 (G9 stale tail removed) | 0.56+2.42+0.01 | 0.25+7.04 | tool |
| uncredited (unhooked LoaderBusy 3B2970 check) | - | 0.7 | 0.6 | 0.3 | derived |
| **load work total (= mgr A last busy = last eval)** | **9.9** | **23.9** | **24.5** | **27.9** | log |
| post-eval: AutoContinue answer -> clear | - | 1.8 | 1.7 | 1.8 | log |
| **tip page waiting for the harness key (the old "unexplained")** | **~15.4** | 0 | 0 | 0 | derived: window - mgr drain |

Same pattern on the other PC loads:
- `captures/tm/load/ep0_launch1_load1.log` (17.3 s, 1 s ticks):
  - Manager A has `A act 10` in every tick from +2 to +15. It drains at +15.6, and the clear is at +17.3.
  - Pre-gate 7.7 s, G8 4.5 s, G5 2.3 s.
- Same route, launch to launch: 13.5 / 14.2 / 15.0 / 17.3 s, so noise is about ±2 s.
- Fight-now first entry (`captures/rematch/fightnow-20261105-140459/engine_launch1.log` #2): 26.0 s.
  - Manager A is busy to +25.4 s, at cap 93 % of the time, with 846 pending on average.
  - It reads 3.27 GB. Retries take 3.3-8.6 s.

Measured and ruled out on PC (do not redo):
- PSO: 0.00 CPU-s in the window (8-9 compute runtime creates), and PJ/PJH pause during loads.
- Inflate: 4.3-6.6 CPU-s at most 4-wide, ≤1.4 s of wall time even if serial. ZI is written off.
- Disk throughput: 1.9-3.3 GB per load, 2-13 s in-call summed over all threads, NVMe.
- Sleep(1) manager polls: LW gave 0.
- GL1: 0.
- EP/SR/ReadFileEx on Windows: ≤0.5 s of thread time (pc-vs-ally-load.md).
- AQ: PC -4.9 (default-on-candidates); RC/PF: no paper win on Windows.
- Memory: RAM available 6-11 GB in both fast and slow runs; no correlation.
- G1 path init: never failed. Event scene: 0 s. Cover frame caps and VSM budget patches: rejected, not same-result.

## 2. Best hypothesis for the remaining ~20-24 s (manager-A request latency at cap 10)
1. **What is known.**
   - The window is manager-A throughput: 10 in flight, 400-1100 pending, at cap 85-93 % of the window.
   - Per-request latency is what varies. Across 119 PC windows, the 11-15 s loads read archive data at 14-18 us per
     call and "other" data at ~4 us. Most 25-36 s loads run 30-100 us and 8-15 us (e.g. titlectl 39 / 17.9 us).
   - Fewer MB are read in the slow runs, so it is per-request time, not volume.
2. **More in flight alone did not help.** 1.7.16 LC=16 raised in-flight 9.6 -> 15.0, but busy time went 8.7 -> 11.3 s
   (single run). The steps run on TrySync, pool 6, **3 threads at every WorkerCap** (loading.md §2). So a deeper
   queue only waits on the same 3 threads, or on a serial stage after them.
3. **The CPU is not idle.**
   - The main PC is a Ryzen 5 7600X (6C/12T): 7.3-9.8 of 12 logical cores are busy during loads.
   - About 7 of those are the full scene rendering behind the opaque cover at 110-130 fps (gameplay level). The load
     itself adds only 1-3 cores.
   - pc2 (5600X, 6C/12T) is the same shape.
4. Three candidates for the serial limit, in order. SP cannot separate them because it samples named threads only and
   TrySync threads are unnamed:
   - **(a) The 3 TrySync step threads** are busy or blocked in reads. Fix: TSY + LC.
   - **(b) Per-frame main-thread resource setup / synchronous copy-and-wait.** The Ally shows the main thread 49 % in
     GfxFencePollWorker 117D6C, called from the 5C0D4 / 5C228 / 41FD9C / 62EBB0 / TextureCopyRegion 69A1B4 helpers
     (loading-ally-fixed §8.9). There each request took ~26 ms, about one frame. Fix: raise the setup budget per frame
     while 58A is set (LB, loading-bottleneck §4).
   - **(c) CPU contention with the hidden-scene render.** Fix: render less behind the cover. This only becomes a
     candidate if (b) is ruled out, because a cap lowers fps and (b) would then slow the load.

## 3. Top fix: TSY=6 + LC=32 on the first load (both already built, default 0, boot-time; untried-ideas #8)
- Build cost: zero. TSY (1.7.25, pool table 4B723B8) and LC=N (1.7.30, 1FEE98 / 1FF3BA immediates) exist.
  - Neither has ever been measured on a PC load with the work-end metric.
  - TSY was only fps-screened (audit-final-20261023 PC: DROP_SCREEN, +1.2 % fps, noise).
  - The only PC LC run was the harness-bound 1.7.16 window, which could not move by design.
- Estimate:
  - If (a) holds, doubling step threads with 2-3x more in flight cuts the cap-bound ~20 s by a third to a half:
    **about 4-8 s saved on a 26 s load** (realistic 3-6 s; Fight-now first entry about the same).
  - If (b) or (c) holds: 0 or slightly worse. The A/B shows which.
  - The old claim "save ~10 s" (role-performance.md) is too high. It assumed an idle 15 s gap that does not exist.
- Plan:
  - Step 1: pc2 A/B (§4A).
  - Step 2: if work end drops ≥2 s (outside the ±2 s noise, n≥4 per arm), repeat it on the main PC and on the Ally.
  - Step 3: small build "LC loads-only". LC also applies to gameplay streaming, so it gets the same switchable
    58A-gated path as LoadOnlyCpuFixes, with a test_patterns proof that the off state equals stock.
  - Step 4: ship TSY=6 + LC loads-only default-on per device.
  - If 0: run §4B. If B names the main-thread copy waits (b), design LB.
- Fix #2 (only after §4): an arm with `MaxFps = 0` (unlocked) vs 120 during the same route.
  - If the load gets faster unlocked, requests are frame-paced: (b) is confirmed and the lever is a loads-only uncap.
  - If it gets slower, the problem is CPU contention: (c) is confirmed and the lever is to skip or cap the hidden
    render while the cover is opaque.

## 4. Capture recipe for performance-pc2 (nothing below exists yet: no ETW .etl in captures/)
**A. Log A/B (no build, ~1 h).** Use the current release ASI and the standard Continue save.
- Base ini keys (both arms): `[General] AutoContinueLoad = 1`; `[Profiler] LoadStats = 3`, `LoadGates = 1`,
  `ReadProbe = 1`, `MemStats = 1`.
- Arms:
  - B = `TSY = 0`, `LC = 0`.
  - A = `TSY = 6`, `LC = 32`. Confirm the "TSY: TrySync pool sized to 6" and "LC ... cap 10 -> 32" lines.
- Order: ABBA ABBA, 8 launches. One title -> Continue load per launch; always the first load after launch, so
  file-cache state matches.
- Then the same ABBA on Fight-now first entry:
  `powershell -File tools\rematch\fightnow_test.ps1 -OwnerPid <pid> -Rows R7B35`
  (Yoritomo, the 26 s first-load case; next.md:520).
- Read per load: `ally_load_phases.py` -> "last observed eval" (the work end) and the pre-gate / G8 / G5 seconds;
  LoadStats `in-flight avg A`, `pending avg A`, archive us/call; MB read within 1 % between arms.
- Optional third arm: `MaxFps = 0`, TSY/LC off (§3 fix #2).

**B. ETW CPU + wait trace (needed only if A is flat; names the serial stage).**
- Tool: Windows built-in WPR, elevated prompt on pc2. Base arm ini, same Continue save.
- Start at the title screen *before* Continue:
  `wpr -start CPU -start FileIO -start DiskIO -filemode -recordtempto D:\etw`
- Press Continue. Stop 3 s after the log prints `AutoContinue #1: load done`:
  `wpr -stop D:\etw\load_pc2_base.etl "pc2 title->Continue base"`
  - Keep the trace under ~40 s so the file stays under ~2 GB.
  - Copy the `.etl` and the EngineFix log to `captures\pc2\etw-load-<date>\`.
- Analysis in WPA (or `wpaexporter`):
  - **CPU Usage (Precise)**, grouped by Process > Thread (start address) > Ready/Wait stack, for the TrySync threads.
    These are the 3 JobWorker threads that run ResourceStepJob_Run 1343D00; identify them by that frame.
  - Per thread: % running vs waiting, and the top wait stacks: ReadFile 312F7C/740392 vs fence/event waits.
  - The same for the main thread: share of time in GfxFencePollWorker 117D6C / FenceWait, and its callers.
  - Verdict:
    - TrySync threads >70 % running or in ReadFile -> (a), TSY.
    - Main thread waiting on the copy fences while TrySync is mostly idle -> (b), LB.
    - All 12 logical cores ready-queued with render threads on top -> (c).
- Optional: PresentMon is not needed. The load metric is the log's work end, not frames.
