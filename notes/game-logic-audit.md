# Game-logic audit: waste, serialization and timing bugs in Nioh 3's own logic (2026-10-09, ASI 1.7.45 branch `gamelogic`)

Scope: the game's own CPU logic, its thread hand-offs and timing primitives. Not GPU shaders, not PSO/stutter, not render
resolution (other agents). Static work on the dump (Ghidra headless, `re\decomp.cmd`) + existing captures: JT run8b
(`captures\tm\abba2\run8b_J\L2_JT_s*.log`, PC abba2 scene, logic-bound 112 ticks/s), the Ally GQ log (bridge file #119,
1.7.43 USER Futamata), the Ally PROTON_LOG summary (bridge #393). The game was not run.
Tags: **M** measured, **S** static (decompile/dump), **E** estimate. Read with whats-left.md, gamelogic.md, gq-jt-probes.md.

## 0. Ranked findings

| # | finding | where | evidence | expected noticeable effect | action |
|---|---|---|---|---|---|
| 1 | **The logic frame starts by waiting for the previous tick's MissionThread** (0.30-0.33 ms of an 8.9 ms tick = 3.4-3.7 %). gamelogic.md had the two [App] thread objects swapped and called it "off the critical path" | join `MissionThreadJoin` 5AAED4 = first call of FrameEarlyUpdate; kick `MissionThreadKick` 233BB4 = last call of GameUpdateTick | JT run8b site 1 (M), decompile (S) | only where the logic thread bounds fps (PC abba2-like / weak CPUs): up to ~3 % fps; 0 when GPU-bound | measure: GL1 (built) with JT in every arm; `JT_Thread = [App]MissionThread` (section 1) |
| 2 | Ally GFX queue idles **0.52-0.81 ms/frame at one fixed point**, `GFM_TextureClear -> App_CopyMRLROcnScene`, 0.95-1.0 times per frame. Regular, mid-frame (list 7-9 of 15-18): fits a **cross-queue Wait on the async ocean slot (6)** better than a CPU-late submit | async slot 6 producer 1A25524, consumer waits GfxWaitAsyncFence 4F8F38 | Ally GQ (M), async notes (S) | Ally +1.8-2.8 % fps if the wait goes away (E) | **TESTED 2026-10-09 (bridge #536): NEUTRAL, hypothesis FALSIFIED.** AS=2 AS_Slots=-6 ON/OFF/OFF/ON 35.0/35.1/35.1/35.1 fps; ocean producer 0.0 calls/s at Futamata Level136 where the gap was measured, so the gap is not the slot-6 wait. Remaining: CPU-late submit or another fence |
| 3 | **Timer-resolution flapping**: the HID pad-reader threads do `timeBeginPeriod(1); Sleep(4); timeEndPeriod(1)` per loop (~250 changes/s per thread); since Windows 10 2004 a process that holds no period gets **15.6 ms Sleep(1)** (measured on this PC: 15.3-15.9 ms, even with the global timer at 1 ms) | HidReaderThread0/1/2 AEF900 / AF07D0 / AF14E0, g_HidTimerPeriod 44B68F0 = 1 | dump (S), timerres harness (M) | hitch/pacing: every game `Sleep(1)` that lands outside a HID begin/end window (main-thread sync resource waits 111064 / 1110C8, loader idle polls 1FEE68, paused-pool 1A6BC42, frame-packet loop 1A746D7) costs up to 15.6 ms instead of ~1-2; 0 if something else in the process already holds 1 ms | **built: TR** (`[Patches] TR`, live row), the stats line says per session whether it matters |
| 4 | Mod's own `OutputDebugStringA` per log line: 91 calls/s in the Ally PROTON_LOG run, each a raised exception + DBWIN lookup through wineserver | dllmain.cpp `Log()` | Ally #393 (M) | Proton CPU/wineserver traffic in diagnostic runs; ~0 in play | **built: `[Debug] SkipDebugString`** |
| 5 | Scheduler0 waits 0.28-0.29 ms/tick for the character-update completion; the signal job (`JobFnEventSet` FE79C0, 8 us) starts 261-272 us after the wait began while workers are 118 % busy = **throughput**, not order | CharacterUpdateEventJoin 90CA90 | JT site 2 (M) | cut character-job cost (MQ, AABB, BPO already built) | none new |
| 6 | StageUpdate bucket join 0.12-0.13 ms/tick, throughput (workers 107 % busy) | 39FDBC wait, ObjBucketsUpdateAllObjects 1323370 | JT site 3 (M) | - | none new |
| 7 | Bug: DirectInput joystick poll with an uninitialised `PCTimer` prints "PCTimer class not initialised." per poll and returns dt = 0 -> the 0.27 / 0.34 s hold-repeat never advances for that slot | DInputJoyPoll 437508 -> PCTimerTick 437F7C | decompile (S) | rare (needs an active DirectInput slot that skipped 1FDAD3C / 1FDAFB0 init); not seen in any log | note only |
| 8 | Logic keeps ticking at full rate with time scale 0 while paused; menus render the world behind the UI at 85-99 % GPU | G+624 zeroed by the pause flags, all jobs still run | frame-timing.md 2, session-2026-10-08-perf (M) | Ally watts in menus only | not built (a menu cap changes menu smoothness: opt-in idea) |

Checked and left (same-behaviour waste too small or already covered): O(n^2) pause gate F2E68/F33A0 (GL2, ~0.06 ms/tick
on Scheduler0, needs a per-handler side-effect audit); GlobalEntityFindById deque search (GL3, ~0.085 ms/tick, needs a writer
census); RenderFrameSettingsApply every tick (cheap); spin/Sleep(0) sites (cpu-hotspots-ally 8: < 0.5 % of a core after
GF/W6/W3/L1); per-frame allocations (linear arenas, `ViewFrameArrays_AllocFromLinearArena`; no heap churn in any profile);
the game's only OutputDebugStringA sites are 437F7C (bug 7) and the HRT init failure 1FDB24C (never on real hardware);
frame-count UI fades and `GetNumberToBeAddedToTimerByFPS` (frame-timing 8.5, cosmetic); EVENT_FPS 30 in cutscenes and the
time-effect half rate (design, frame-timing 8.3/8.4); Dynamic FPS deciding from CPU time only (users keep it OFF; VRR replaces it).

## 1. (a) MissionThread on the logic critical path

**Corrected thread map** (S; symbols renamed, `re\symbols\incoming\merged\*game-logic-audit*`):
```
logic frame N (KIDSTaskScheduler0)
  FrameEarlyUpdate 5AC9EC -> MissionThreadJoin 5AAED4: if g_MissionPending 45C6E82: KtglEventWait(4709998)   <-- JT site 1
  ... frame graph: PerFrameUpdate 39E4E4 -> MissionThreadArm 39E104 (dt params -> 4709A94.., g_MissionArmed, gate 4709AB0)
      GameUpdateTick 2338B0: ... GameModeDrawThreadKick 233810 / GameModeDrawThreadJoin 233888 (JT site 5, 0.065 ms)
                             ... MissionThreadKick 233BB4 (last call: start event 47098F8, g_MissionPending = 1)
  FrameEnd 5ACF80 (limiter)
[App]MissionThread (g_MissionThreadObj 47098F0, body MissionThreadEntry E5E060 -> MissionThreadRunFrame E63140):
  waits the character-update events of [4745348] (+900h / +5C0h / +1448h) and 45C3760+338h, PreUpdate E63350,
  gate 4709AB0, MissionThreadUpdate E64700, then sets the done event 4709998 (E5E0CC)
```
The object gamelogic.md called g_MissionThreadObj (4709B60) is the **[App]GameModeDrawThread** (its join 233888 is released
by E6FC3F on GameModeDrawThread in JT). The MissionThread is kicked at the **end** of GameUpdateTick and joined at the
**start** of the next logic frame, so its ~1.3 ms of PC work (gamelogic.md 14 % of a core at 110 ticks/s) only overlaps
with the rest of the frame graph + FrameEnd. In the logic-bound abba2 scene that is not enough: **0.296-0.334 ms/tick**
blocked (p50 297-338 us, 0.99 waits/tick, 3 windows, M), released 100 % by the MissionThread; during the wait the other job
threads are 56-57 % busy (1.4 threads) and **no job runs anywhere for 32-34 % of it** (M) - the machine is idle while the
logic thread waits for one serial helper thread.

**Can the work overlap?** Not with a same-behaviour patch:
- Moving the join later inside FrameEarlyUpdate needs a proof that none of its ~45 callees reads or writes mission state
  (scripts, task lists, actors, spawn tables) - not provable statically.
- Kicking earlier (right after MissionThreadArm) would run the mission step concurrently with the rest of GameUpdateTick
  (listeners, deferred calls, slot flushes) - a data race by construction.
- The MissionThread itself first joins the character-update events of the same tick (E63140 preamble), so part of its
  latency is the character jobs (JT site 2), which are throughput-bound.
So the levers are making the mission step shorter and measuring what it waits on:
1. **GL1** (built 1.7.14, software prefetch in the two task-list walks, the hottest part of PreUpdate/Update; live counts
   showed 4380 proximity entries, 4-5x the estimate that rated it ~0.05 W) was only judged by LoadStats and launch-to-launch
   fps (5 % noise). With JT in every arm the site-1 ms/tick is a direct, low-noise metric.
2. **`[Profiler] JT_Thread = [App]MissionThread`** (ini only): the same probe watching the MissionThread shows whether it waits
   on its start event (kicked late), the character events, the gate 4709AB0, or computes. If it is mostly computing,
   the next targets are E266B0 / 2866F0 / 20B900 (gamelogic.md GL1 design incl. the 20B900 segmented walk, not built).
Expected (E): GL1 -30..45 % of the walks = ~0.1-0.2 ms of the ~1.3 ms mission step -> site 1 0.30 -> ~0.15 ms/tick, i.e.
~1.5 % logic frame in CPU-bound scenes; fps-neutral when GPU-bound (pc2 HIGH, Ally USER, PC at the user's 1440p).

## 2. (b) Ally GFX gap at GFM_TextureClear -> App_CopyMRLROcnScene

Ally GQ (1.7.43, USER, Futamata, 13 x 30 s windows; M): after the scene settles the top edge is always
`[GFM_TextureClear -> App_CopyMRLROcnScene]` = 0.52 / 0.65 / 0.70 / 0.70 / 0.81 / 0.62 / 0.66 ms/frame, 0.95-1.00 gaps/frame,
mean 533-836 us, max 2.2-4.5 ms, always between list 7-9 of 15-18 (the middle of the frame), both lists from the ECL thread
(4F9114). gq-jt-probes.md 5 calls mean >> 150 us "CPU-late", but that rule was written for irregular gaps; this one is
**regular, once per frame, at a fixed pass boundary**. App_CopyMRLROcnScene is the ocean ("Ocn") scene copy; ocean is one of
the three passes KT ships on async compute (slot 6, producer 1A25524; gpu-async-wins.md), and a consumer's
`GfxWaitAsyncFence` 4F8F38 records a queue Wait (token A9) that splits the list and idles the DIRECT queue until the compute
queue signals. GQ cannot tell a queue Wait from a late ECL (it sees only DIRECT list stamps).
**Test (no build, AS is in 1.7.44+):** `[Patches] AS = 2`, `[Tuning] AS_Slots = -6` (slot 6 synchronous while the AS row is ON),
live ABBA of the AS row at a water scene (Futamata), GPU-bound, GPUProf = 0: fps + (one window with GPUProf = 2, GQ = 1) the
edge's ms/frame. Sync ocean costs its compute time on GFX instead of the wait: keep whichever is faster. Same for -4 (fog)
and -3 (VCLOUD) on the Ally (gpu-async-wins row 3). If the edge does not move with -6, it is CPU-late after all: then the
producer is the ECL replay of the main thread's stream and the next probe is a CPU timestamp per ECL vs the GPU clock.

## 3. TR - the HID threads' timer-resolution flapping (built)

HidReaderThread1 AF07D0 (S, dump bytes at AF088D): `for (;;) { poll; timeBeginPeriod([44B68F0] = 1); Sleep(4); timeEndPeriod(1) }`;
HidReaderThread0/2 do the same around their Sleep(2) retries. All 5 timeBeginPeriod and 5 timeEndPeriod call sites in the
exe are in these three threads (no other code asks for a period). Consequences on Windows 10 2004+ (per-process timer
resolution):
- The process holds 1 ms only while a HID thread is inside its Sleep; every other game thread's `Sleep(n)` / timed wait
  issued in a gap, or **all of them when the HID threads are not running** (the loop runs while AE4B90 returns nonzero,
  likely "a HID device is open"; input-thread.md: Sony pads use this path), get the default 15.6 ms. Harness on this PC (Win 11, global timer at 1.000 ms because
  other apps hold it): a process holding no period sleeps **15.3-15.9 ms in Sleep(1)**, 2.0 ms with a hold (M).
- ~250-750 resolution changes/s (each NtSetTimerResolution when the minimum changes; ~1 us per stock begin+end pair here).
Game Sleep(1) users this affects: the main thread's synchronous resource waits (`while (!ResourceIsReady) Sleep(1)` 111064,
same idiom 1110C8; stutter.md 5), the frame-packet loop 1A746D7, ResourceLoad idle poll 1FEE68 (load latency), the job
pool's paused loop 1A6BC42, W3's timed MsgWait (W3_TimeoutMs) and stock Sleep(1)/(2..) polls (cpu-hotspots-ally 8).
Not known: whether something else in the user's process (FSR/Streamline/driver/overlay) already holds 1 ms; the PC
playsession's 700 W3 waits/s suggest ~1 ms granularity in that session.

**TR** (`src\timerres.cpp`, `[Patches] TR = 0|1|2`, menu row "Steady timer resolution"): hooks Nioh3.exe's two WINMM import
slots; ON = one `timeBeginPeriod(1)` held for the session and the game's own begin/end pairs (period >= 1) become no-ops,
balanced per thread with a TLS depth (a switch flip between a begin and its end can never leave an unmatched call); OFF
(2) = calls pass, counters only. Stats line every 30 s: `stats/s TR: game timeBeginPeriod N/s ... | system timer ... |
Sleep(1) here: median X us, max Y us` - the last field is the real per-process granularity right now (8 Sleep(1) on the
mod's stats thread), so TR = 2 alone answers "does this PC have the problem". Same output for the HID threads (they asked
for 1 ms); under Proton nothing changes (Wine timers are precise). Offline: `tests\run_timerres_harness.py` ALL PASSED
(off/on/flip/4-thread stress with 200 flips, winmm refcount balanced afterwards, skipped pair 17-19 ns vs 1.04 us stock;
dump: only the HID threads import/call them, AF07D0 loop shape, g_HidTimerPeriod = 1).

## 4. SkipDebugString (built)
`Log()` mirrored every mod log line to OutputDebugStringA. Under Proton that is RaiseException(DBG_PRINTEXCEPTION_C) + a
DBWIN mapping open through wineserver per line; the Ally PROTON_LOG run counted 91/s (profilers on). `[Debug]
SkipDebugString = 1` keeps lines in the log file only (OutputDebugStringA still when a debugger is attached). Default 0 =
unchanged. It matters in profiling runs (SP / GPUProf / FnProbe), not in play (~1 line/s).

## 5. (c) display delay, (d) pacing - status
- (c) VrrPresent (1.7.44) is built, not A/B'd; notes\vrr-present.md 3 has the protocol (needs in-game VSync ON + a VRR
  panel). It is the largest measured latency lever (+7.4 ms p50 display latency from the game's own SyncInterval flip).
- (d) Limiter: stock QPC spin + Sleep(0)/job steal; L1 sleeps on a high-resolution waitable timer (immune to the timer
  resolution, so TR does not change L1/F1/DS). Spikes: only first-use PSO compiles in real play (other agent); TR is the
  one new pacing lever from this audit (Sleep(1) granularity on the main thread's synchronous resource waits).

## 6. pc2 queue (sent as [QUEUE] on bridge channel nioh3)
ASI 1.7.45 from branch `gamelogic` (merged to main), bridge file in the [QUEUE] message. Each step is one abba2 run:
1. CPU-bound logic: base ini = the abba2 base + `[Profiler] JT = 1`, `JT_ReportSec = 10`, `[Patches] TR = 2`;
   `pc2_abba2_pc.ps1 -Arms BASE,GL1,BASE,GL1,BASE -Live TR -MaxFps 240 -RenRat 50 -Input vpad -RunName gl1_tr_jt`.
   Read: per arm `py -I tools\gqjt_digest.py <log> --skip 1` site 1 (5AAEED) ms/tick + site 2; fps median/p99; TR live
   quads (ON-OFF fps, p99, frame-time stdev); the `stats/s TR:` lines (game calls/s, Sleep(1) median/max with TR off).
2. `JT_Thread = [App]MissionThread` in the base ini, `-Arms BASE -Live none` (one launch): the MissionThread's own wait sites.
3. If pc2's panel has VRR and its VSync can be ON: VrrPresent per notes\vrr-present.md 3.

## 7. Files
- `mod\Nioh3EngineFix\src\timerres.{h,cpp}` (new), `dllmain.cpp` (TR start + stats line, SkipDebugString, 1.7.45),
  `menu.{h,cpp}` (TR row), `build.bat` / `CMakeLists.txt`, `Nioh3EngineFix.ini` (TR, SkipDebugString),
  `tests\timerres_harness.cpp` + `tests\run_timerres_harness.py`, `tools\testmatrix\matrix.json` (TR item; GL1 note).
- Symbols: MissionThreadJoin 5AAED4, MissionThreadKick 233BB4, MissionThreadArm 39E104, g_MissionThreadObj 47098F0,
  g_MissionPending / g_MissionArmed 45C6E82/83, GameModeDrawThreadKick/Join 233810/233888, g_GameModeDrawThreadObj 4709B60,
  CharacterUpdateEventJoin 90CA90, JobFnEventSet FE79C0, PCTimerTick 437F7C, g_HidTimerPeriod 44B68F0.
- Ally GQ log: bridge file #119 (downloaded on this PC to `captures\ally\gq_1743\`, not committed).
