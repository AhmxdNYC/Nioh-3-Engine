# Real play session 2026-10-08 (PC): performance digest

Session `captures\playsession\20261008-2059-session` (20:59-21:22, one launch, exit code 0). ASI 1.7.39, the user's own
settings: 2560x1440, REN_RAT 75, XeSS, **FSR frame generation**, VSYNC ON, HDR, 120 fps cap, 240 Hz VRR desktop.
Content: hub, Battle Scroll replay of "The Hamamatsu Incident" (mission 52), boss Jakotsu-baba 21:19:07-21:19:34, return
to the hub. Profilers: FnProbe, BossLog, RematchLog, PadLatency, MemStats, LoadStats, LoadGates, CpuStats, TS, SPR
(**no SP / GPUProf**, so there are no per-function CPU shares from real play).
Digests (scratch scripts over `presentmon.csv` + `modlog_run1.log` + `sysmon.csv`, reusing `tools\playsession\summarize.py`):
phase table, per-30 s join of PresentMon + mod lines, spike list for frames > 33 ms, displayed vs presented, combat proxy.
Tags: **M** measured here, **E** estimate.

## 0. Headline
1. **The PC at the user's settings is GPU-bound, not CPU-bound (M).** With FG on, GPU busy = 90 % of wall time in the
   mission and 92 % in the boss fight (sum of PresentMon GPUBusy / wall); real (rendered) rate 100-118 fps, below the
   120 cap; the main thread waits **2.15 ms per frame** on the frame-latency waitable (TS W->B; 0.5 ms in menus, 0 at the
   title). abba2 (1080p, FSR 50 %, FG off) is logic-thread bound; real play at 1440p/75 % + FG is not.
2. **The only gameplay hitches are first-use PSO compiles (M).** 14 min of mission: 3 frames > 33 ms (63, 53, 120 ms at
   21:05:12-28, the windows with 2 + 6 runtime PSO creates, avg 30 ms / 10.5 ms). Boss: 3 frames of 98, 112 and 167 ms
   at 21:19:31-32, the 1 s before the kill flag (SetMissionFlag 118 at 21:19:32.37); that SPR window has 29 PSO creates, 20 of
   them >= 1 ms, worst **153 ms**. Session total 247 runtime PSO creates, 25 >= 1 ms. No memory, GC-like or streaming
   hitches in play.
3. **No FG-off gameplay was recorded (M).** The game's FRAME_GENERATION slot read FSR (2) at every check (the mod logs
   any change; none), `config.xml` is unchanged since 2026-10-06, and the FSR presenter thread presented in every gameplay
   second. The only non-FG frames are menus, loads and cutscenes, where the game pauses FG itself. The user's toggles
   probably happened inside the 67 s options visit at 21:20:48-21:21:54 and were reverted before leaving the menu (two 64-81 ms menu
   frames at 21:20:56 / 21:21:19 fit a swapchain rebuild, E), or went through the driver (AMD AFMF is invisible to the game
   and to this PresentMon build).
4. **10-20 % of the presented frames never reach the screen (M).** Every gameplay frame was `Composed: Flip`, SyncInterval 1
   (in-game VSync ON makes DWM compose). Presented 166 fps -> displayed 149 fps in the mission; the drop rate stepped
   from 3-6 % to 17-21 % at 21:12:50 (no event logged) and stayed there through the boss (19.8 %), back to 3 % in the hub.

## 1. Per phase (PresentMon; FG on unless noted; "GPU" = GPUBusy summed / wall time)

| phase | presented fps | displayed fps | p99 ms | 0.1 % low (1000/p99.9) | frames > 33 ms | GPU % | not displayed |
|---|---|---|---|---|---|---|---|
| title + menus (FG not running) | 120.5 | 113.6 | 13.25 | 8.8 | 24 (boot) | 50 | 5.7 % |
| load 1, save -> hub, 20.3 s | 107.7 | 107.1 | 31.1 | 28.0 | 10 | 84 | 0.6 % |
| hub (FG) | 159-162 | 143-145 | 9.6-10.1 | 70-81 | 0 | 85-86 | 9-12 % |
| mission-select menu (FG paused) | 104.4 | 104.1 | 11.6 | 82.5 | 0 | 99.5 | 0.3 % |
| load 2, mission, 31.7 s | 107.5 | 107.2 | 24.5 | 12.2 | 8 | 91 | 0.3 % |
| **mission, 14 min, menus removed** | **166.1** | **148.6** | **10.3** | **75.7** | **3** | **90** | **10.5 %** |
| - fights (>= 60 button presses / 30 s, 11 windows) | 166.2 | 155.2 | 10.2 | 78.2 | 0 | 91 | |
| - exploration (< 30 / 30 s, 9 windows) | 168.0 | 142.8 | 10.1 | 76.8 | 1 | 89 | |
| load 3, boss gate, 5.0 s | 74.6 | 72.8 | 28.1 | 33.7 | 0 | 99 | 2.4 % |
| **boss fight, 26.8 s** | **129.6** | **103.9** | **14.3** | **36.6** | **3** | **92** | **19.8 %** |
| post-boss to clear, 20 s | 151.6 | 125.7 | 10.7 | 73.1 | 0 | 95 | 17.1 % |
| clear screen + load 4, 39.3 s | 99.6 | 98.6 | 26.0 | 7.9 | 14 | 81 | 1.0 % |
| hub after (FG) | 158.5 | 144.5 | 11.2 | 66.0 | 0 | 88 | 8.9 % |
| options menu 67 s (FG paused) | 99.4 | 99.3 | 12.4 | 44.8 | 4 | 89 | 0.1 % |

- **Real (rendered) rate** (mod OSD, 30 s): hub capped at ~120; mission 100-118 (median ~106); boss 88-93 (FG-state lines);
  FG ratio 1.42-1.69, never 2.0. Normal fights cost nothing extra over exploration (166 vs 168 presented fps); the boss arena
  does (GPU 7.1 vs 5.4 ms per presented frame).
- **CPU vs GPU:** GPU-bound in play (above). The CPU is far from idle: Windows counters show the process at **8.6-9.4 logical
  cores** (CPU total 86-93 % of the 7600X) in hub / mission / boss, vs 8.4-8.5 cores in the older profile (cpu-profile.md);
  mostly worker spinning (WK/WG territory), not frame time on a desktop. TS medians in the mission: logic frame 9.34 ms,
  update+limiter 9.21, logic->waitable 5.99, **waitable wait 2.15**, render 9.62 ms.
- **Spikes per minute** (> 2x local median and > +8 ms, on presented frames of ~6 ms): 5.7 in the mission, 44.6 in the
  boss fight (the 3 PSO frames + the 20 % display drops), 263 in the 5 s boss-gate load.
- **Menus and load screens are GPU-heavy:** 99-107 fps at 85-99 % GPU busy (the world renders behind the UI). No fps effect;
  on the Ally that is wasted watts during menus (loads are frame-bound there, so no cap is proposed, loading-bottleneck.md).
- **Loads** (LoadStats/LoadGates): 20.3 s (gate G9 LoadCharacterReady held 9.75 s), 31.7 s mission (G9 17.45 s), 5.0 s boss
  gate (G5 ObjectListAnyNotReady 1.36 s), 39.3 s mission-clear -> hub (G2 StageAreaListBusy 16.9 s); 0 PSOs inside loads,
  2.0-2.3 GB read per long load. `ResourceLoadTaskStateStep` ran only in the first 2.5 min (boot/first-load task type).
- **Memory** (MemStats / counters): working set 5.3 -> 6.86 GB, +11 MB/min in the mission, commit 19.1 GB max, VRAM
  10.0-10.35 GB of a 15.4 GB budget, RAM available >= 7.0 GB. No pressure, no memory-timed spikes.
- **Upload heap** (SPR): 310 dedicated fallbacks, 3 >= 1 ms (worst 9.5 ms, 21:18:29-59); not visible as frames > 33 ms.

## 2. Input latency per FG state

| state | TS sample->present | TS input->display | PadLatency input->present p50 / p99 | PresentMon present->display p50 | estimated pad -> screen |
|---|---|---|---|---|---|
| mission, FG on (28 windows) | 26.9 ms | 29.2 ms | 30.5 / 43.0 ms (5391 pad changes) | 8.6 ms | **~44 ms** (report.md) |
| boss window, FG on | 31.1 | 35.2 | 33.7 / 51.3 | 10.9 | ~49 ms |
| options menu, FG paused, ~99 fps | 23.9 | 24.6 | 29.8 (2 changes, not usable) | 10.8 | - |
| title, FG not running, 120 fps | 21.3 | 29.6 | 12.3 (2 changes) | 1.9 (independent flip) | - |

- Fights vs exploration: pad input->present p50 30.9 vs 29.4 ms (same).
- **FG cost (E):** at a similar base rate (99 fps menu vs ~106 fps play) FG adds ~3 ms sample->present and ~4.5 ms
  input->display. Different scenes, so +-2 ms. The bigger fixed cost is the composed flip (VSync ON): the title's
  independent-flip frames reach the screen 1.9 ms after Present, gameplay frames 8.6 ms (input-latency.md: VSync OFF + VRR
  saved ~16 ms at a 60 cap on this PC).
- Mouse/keyboard all-input-to-photon (PresentMon): 10.8 ms p50 in play (31 samples), 13.4 ms in the options menu.

## 3. FnProbe: the last 14 remaining-work 10.1 functions, named from live data
All 14 hooked; 13 hit, `2C5AA6C` never (dead/rare). Callers re-resolved to their containing functions (`re\out\functions.tsv`):
the report's nearest-symbol labels with large offsets were wrong for 7 of them. Vtable slots checked in `re\Nioh3.dump.exe`.

| RVA | name (was) | live context | conf. |
|---|---|---|---|
| 167C50 | SmallIdStateStructCtor | 284 M calls, 99 % ResourceDependencyResolveLoop+66h; 140-225 k/s play, up to 2 M/s in loads | high |
| 59F498 | RigConstraintAccumulateChildOffsets | recursive, world-position args; 1.1 k/s title -> 46 k/s mission -> 57 k/s boss (scales with characters) | medium |
| F68D8 | **ConditionCheckById** (SwitchTableDispatch_641Cases) | (self, condId < 641h, param, record), via thunk F6878 = vtbl 38FE698 +138h; 28 k/s in play | medium |
| 2002D4 | ResourceLoadTaskStateStep | vtbl 38FD188 +10h; manager-A loader thread; only the first 2.5 min | medium |
| 57DA04 | DrawChainU16InsertRelink | ~30 per rendered frame, only SceneViewBuildDrawLists | high |
| 4F9750 | **RenderObjPrepareVirt4D0** (GfxViewPassGridRecord15KStack) | vtbl 38F25B8 +4D0h (its +4C0h = StackBatch13KBuildAndSubmit); ~10 objects per frame | low |
| 471E68 | ReflectionViewSetupBuildViews | ~6 per rendered frame (cube faces, E) | medium |
| 23BF1C | ShapeComputeLocalBoundsAndBasis | via ShapeBoundsRefreshIfState1 23B62C; ~5 per frame; [r9] = 500000.0 | medium |
| 7D48D4 | **AudioPanAngleToSpeakerGains** (AimAngleToBlendWeightClamp) | audio thread, AudioVoiceMixOutputCrossfade 99 %; speaker angles 30/110/150 deg | medium |
| 398A54 | FrameTimeFloatSimStepLarge | exactly 2 per rendered frame, two state blocks 6F8h apart (45C0840 / 45C0F38) | low |
| 383800 | SlotResourceTableResolveAndBind | ~2 per frame from EC90C (2-slot loop) | medium |
| 93AEC4 | SubItemParamsPushByHashVirt12 | exactly 1 per rendered frame (vtbl 40EF390 +60h) | high |
| 556118 | **EntryGridUnlockStateRefresh** (SceneVisibilityGatherLarge) | 50-100/s in loads and menus, ~1/s in play: unlock-state grid, not visibility | low |
| 2C5AA6C | AnimPoseEvalSkinningBatchHuge | 0 calls in 22 min | low |

New rows: `ConditionCheckSlotParamThunk` F6878, `ConditionListEvaluate5` 2B2D88, `ShapeBoundsRefreshIfState1` 23B62C.
Files: `re\symbols\incoming\merged\*fnprobe-session*.tsv` (applied with apply_renames.py + merge_incoming.py).
`perf_coverage.py --precise`: still **100.00 %** (Ally, PC, combined; 0 unnamed). Badge: 6913 / 211206 functions named.
None of the 14 is a CPU hotspot in play: the busiest (167C50, 2 M calls/s in loads) is ~1 % of a core at most (E, ~5 ns/call).

## 4. Real play vs the abba2 test scene (what changes on the critical path)
- **Different bound:** abba2 = logic thread (job-join wait 1.44 ms + ObbToAabb/BP); real play at the user's settings =
  GPU (90-92 %). PC CPU items (AABB, BP, JT/JG, UI skip, DQ) stay valid for abba2-like CPU-bound settings and for the
  Ally's future CPU ceiling, but are **fps-neutral at the user's own PC settings with FG on**.
- **CpuStats counters match abba2:** PT1 lock pairs 105-208 k / 30 s (abba2 164 k), DL lookups skipped 1.5-2.2 M / 30 s
  (abba2 1.75 M), MQ 75-100 M calls / 30 s. No new CPU work pattern visible; without SP no new function shares exist.
- **New on the critical path:** first-use PSO compiles in the render thread (boss kill: 98-167 ms frames), and the
  presentation path (composed flip + FG pacing: 10-20 % of generated frames discarded, i.e. GPU work for nothing).
- Next capture should add `[Profiler] SP = 1` (whats-left 4 settings) so the boss / fight windows get CPU shares.

## 5. Verdict for the user (PC)
Full text in `user-settings.md` (PC table, rows 11-13). In short: FG on gives ~160 presented / ~150 visible fps from ~105
real; smoothness is good (1 % low ~97, only PSO hitches); it costs ~4-5 ms of input delay (E); the PC is GPU-bound with it.
Biggest measured lever is not FG but **VSync OFF with FreeSync** (composed flip -> independent flip).
