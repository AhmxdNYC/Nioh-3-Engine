# CPU-bound mid-range PCs with Resizable BAR: what holds pc2 at ~26 ms (2026-10-09, ASI 1.7.58 / 1.7.58b)

> **CORRECTION 2026-10-10 (notes/renderer/backend.md):** the replay thread is NOT blocked on barriers, a lock or a full submit queue. pc2 measured 0 queue-full waits and an uncontended lock; the replay thread is ~20 % idle, and the earlier '~19 % blocked' was the profiler misattributing its own idle wait. The real wall: the engine command buffer is wiped at every frame end, so the main thread waits ~5 ms for replay and frames never overlap. Fix direction = frame overlap (notes/renderer/frame-overlap.md).

Question (#1096): pc2 (Ryzen 5 5600X, RX 6650 XT) with ReBAR ON at the standard Continue save spot runs GPUBusy 19.1 ms but
frames ~26 ms (38 fps, 73-75 % GPU-bound). Which thread or sync holds the frame, and what is fixable mod-side?
Tags: **M** measured on pc2, **S** static (dump / disassembly), **E** estimate.

## 0. Answer

1. **The frame is bound by [App]Game waiting for the D3D12 replay thread (SynergisticDispatcherTask) to drain (M).**
   - TS: render frame B->P **25.3 ms**, waitable wait W->B **0.00**, logic frame C->C 13.2 ms (so not the logic, not the
     swapchain). PresentMon: CPUBusy 25.8 / CPUWait 0.25 / GPULatency 22 / GPUTime 19.4 ms: the GPU waits for work.
   - TSTATE: [App]Game **92-94 % CPU, READY 0.4 %** (it is never short of a core, and the engine already runs it,
     the dispatcher and the Present thread at priority 15). The process uses ~9 of 12 logical CPUs, system 85-89 %.
   - SP (Focus, 10 x 10 s): **23-24 % of [App]Game is WaitDispatcherDrainSpin 418F40** (+ fragments 418F90..9A) = ~6 ms per
     frame spinning at frame end until the dispatcher has replayed the frame. The dispatcher is 78 % busy.
   - Proof by A/B (CPU2, live ABBA x3): **W1B** removes main-thread work (the glyph padding loop) - fps **+0.1 %**, and the
     drain spin grew from ~23 % to ~30 % of [App]Game in the W1B-on windows. Main-thread savings are absorbed by the drain.
   - **RTP** (render threads to ABOVE_NORMAL, CPU1 ABBA x2): **+0.1 %** - they already run at 15; contention is not it.
   - **TG** (logic ticks follow the render rate, behaviour change, opt-in): **+2.6 % / +3.1 %** (CPU1 / CPU2, SE 0.1): fewer
     logic ticks (80 -> ~50/s) free cache / memory bandwidth for the two render threads. Real but small.
2. **Why ReBAR made pc2 CPU-bound:** ReBAR took 8 ms off the GPU (27 -> 19 ms) but nothing off the CPU replay; the
   dispatcher's ~20 ms of work per frame was hidden behind the 27 ms GPU before. The old stock-vs-mod pc2 session (#441,
   ReBAR off) already had B->P render 26-28 ms.
3. **Fix built: PCM** (1.7.58b, `[Patches] PCM`, live row, default 0) - a memo in front of the graphics-PSO cache B-tree on the
   dispatcher. pc2 CPU3, one launch, ABBA x4: **+0.5 % fps (+0.18 fps, SE 0.03, every pair positive), 98.3 % hits, 0 flushes**.
   Real but far below the >= 5 % bar: stays default OFF, not proposed for the release profile, so no Ally check (#954 is
   for fixes that ship). TG confirmed a 3rd time: **+3.3 %** (SE 0.03).
4. **What is left on the path is waiting, not computing (M, CPU3 SP):** inside the dispatcher's tail ~19 % of its time is
   blocked: `[ntdll] <- SynUavBarrier 1139D4` 7.5 % (ID3D12GraphicsCommandList::ResourceBarrier blocking inside the driver),
   `[ntdll] <- 7F6904` 6.1 % (under the shared SRW lock of SynSharedLockedUpdate 7F64DC: an exclusive holder elsewhere),
   `[ntdll] <- EclRingPush 86F760` 5.5 % (ECL ring full = submission backpressure). These, not lookups, are the next
   measurement target; a fix there is the only route to the ~50 fps main-thread ceiling at this spot (section 6).

## 1. Inputs
- #1096 / bridge file #179 `pc2_rebar_bb.zip` (5 launches B bb B bb B, ReBAR ON): OSD rendered 38 fps, **logic 75-83 ticks/s
  at the 240 cap, game speed 0.80-0.94x** (VRR clamps tick intervals > 20.8 ms = Min120 48), PresentMon above.
- #441 (`pc2-stock0020-mod0036.zip`, ReBAR OFF, cap 120): mod/stock 36.9 / 36.8 fps, logic 118-120 ticks/s vs 32-34
  rendered (3.5 logic ticks per shown frame), TS B->P render 26-28 ms, sysmon: the game at ~10 of 12 logical CPUs.
- #864/#877 VS1 (ReBAR OFF, cap 240, tour): 126-239 ticks/s vs 26-36 rendered fps.
- **CPU1** (#1102 -> #1109, file #180): ASI 1.7.58 (#176), BASE arm + SP Focus + TSTATE + JT + TS + live RTP / TG ABBA.
- **CPU2** (#1110 -> #1115, file #181): same without TSTATE (its ~16 ms snapshots, pinned to [App]Game's ideal CPU 11,
  distorted CPU1's main-thread profile), live W1B / TG ABBA x3.
- **CPU3** (#1117): PCM / TG ABBA x4, ASI 1.7.58b (#182).
- Logs + digests: `captures\pc2cpu\` (not committed). Digest: `py -I re\scripts_cpu\sp_aggregate.py <log> <first> <last> 40`.

## 2. Critical path per frame on pc2 (CPU2 steady windows, W1B off; ms = share x 25.9 ms)

| thread | share | ms/frame | what |
|---|---|---|---|
| [App]Game | 23.7 % | **6.1** | WaitDispatcherDrainSpin 418F40 (+418F90..9A): waits for the dispatcher (critical-path wait) |
| [App]Game | ~11 % | 2.8 | glyph padding clear 61B1C (W1B cave or W1 bytes; SP shows it as `[asi] <- 69A494 / 62000` when W1B is installed) |
| [App]Game | 7.9 % | 2.0 | UiQuadBuild 1CD690 (alpha read-modify-write on the mapped UI vertex buffer = an uncached read over PCIe when the UPLOAD heap is in VRAM) |
| [App]Game | 5.2 + 4.5 % | 2.5 | Memcpy B94250, `[ntdll] <- 4DE8C` (buffer unmap) |
| [App]Game | 3.6 + 3.1 % | 1.7 | 12AB0C (vfunc, vtable 38EB508) / 12AA5C (grid search: rows x cols of 64-byte cells, vfunc +A8h on type-2 cells; caller 4A2DC8 in the UI renderer) |
| [App]Game | ~40 % | ~10 | draw prep (DrawEmit 1E80B0, meshlet 4AA34 / 4BBEC, texture streaming 1E7050, stage slot tables 1D8xxx, ...) |
| dispatcher | 9.4 + 1.4 + 1.1 % | **3.1** | **PsoCacheBTreeFind 119820** + its memcmp (graphics PSO cache lookup per dirty draw) |
| dispatcher | 4.0 % | 1.0 | PsoComputeCacheBTreeFind 58C144 (compute twin) |
| dispatcher | 6.8 % | 1.8 | Memcpy (constant blobs copied into the shadow / upload ring, d3d12-submission.md 4) |
| dispatcher | ~15 % | 3.9 | driver: SynUploadCbvTable 118E28 (CreateConstantBufferView), UploadRingAllocate 1190C0, CopyDescriptorsSimple 115DE8, draw calls |
| dispatcher | ~23 % | 6.0 | blocked: `[ntdll] <- SynUavBarrier 1139D4` 7.7, `<- 7F6904` 6.2, `<- EclRingPush 86F760` 5.4, `<- SynergisticDispatcherLoop` 3.5 (semaphore / ring waits) |
| logic (KIDSTaskScheduler0) | 80 % busy | 13.2 ms/tick | ticks 75-83/s, 2.1 per rendered frame; not on the frame's path (TG shows the cost of sharing the CPU with it) |

The model that fits all three A/Bs: frame = (dispatcher idle while [App]Game has not yet flushed tokens) + dispatcher work.
Main-thread work done while the dispatcher still has a backlog only moves [App]Game into the drain spin. So:
- **Dispatcher work is the lever** (every ms it loses comes off the frame until the drain spin reaches ~0, i.e. until
  [App]Game's own ~19-20 ms becomes the bound: ceiling ~50 fps on pc2 at this spot).
- Main-thread UI fixes (W1B, E, C in ui-mainthread.md) only pay after that.

## 3. Top 3 CPU costs on the critical path and whether the mod can fix them

| # | cost (pc2) | fixable mod-side? | how | est. gain pc2 | weaker CPUs |
|---|---|---|---|---|---|
| 1 | Graphics + compute PSO cache lookups on the dispatcher: 119820 3.1 ms + 58C144 1.0 ms per frame | **yes, same behaviour** | **PCM** (built): memo of exact entry copies, flushed on any cache change; compute twin (8-byte keys, 58C0D8/58C144) not built yet | E: -1.5..-3 ms on the tail -> **+6..+12 %**; measured: section 5 | larger: the walk is memory-latency bound; on a smaller L3 it costs more per lookup |
| 2 | Constant / descriptor upload on the dispatcher: Memcpy 1.8 ms + CBV create / descriptor copies ~3.9 ms (mostly driver) | partly | skip the re-upload of an unchanged CBV blob (hash of the 0x66 token's payload vs the slot's last one -> reuse the last CBV / table); invasive, needs a proof that the ring slot is still alive; d3d12-submission.md 4 rated the copy chain "invasive" | E: 0.5-1.5 ms | same order |
| 3 | Logic ticking 2x per rendered frame (80 ticks/s at 38 fps) on a 6-core | only as behaviour change | **TG** (exists, opt-in): +2.6..+3.1 % measured; not same-look (logic dt 1/50 s instead of 1/80 s) | +3 % | more on 4-core / handheld (cpu-hotspots-ally.md 4: ~1-1.4 cores on the Ally) |
| (4) | Main-thread UI writes into UPLOAD memory that ReBAR moved to VRAM: 61B1C 2.8 + 1CD690 2.0 + unmap 1.2 ms | yes (W1B built; E, C designed) | only after #1/#2 shrink the drain | 0 today on pc2 (measured W1B +0.1 %) | on a CPU where [App]Game is the bound |

Not on the path / ruled out: thread priorities (RTP: the engine runs [App]Game / dispatcher / Present at 15 already),
CPU sets (none: process default, workers unrestricted), logic join waits (JT; the logic's 13 ms tick is off the frame path),
the swapchain (W->B 0, CPUWait 0.25 ms).

Side finding (not fixed here): at the 240 cap the logic ticks 75-83/s with irregular intervals and **game speed 0.80-0.94x**
(VRR clamps intervals above 1/Min120 = 20.8 ms). Players with an uncapped / 240 cap on a 6-core see slow motion at this
spot; cap 120 (#441) kept speed 1.00x.

## 4. Built (branch `cpu-midrange`, all default off)
| key | file | what | test |
|---|---|---|---|
| `[Profiler] ThreadState` (+ Us / Sec / Top / DelaySec) | `src\thrsched.cpp` | TSTATE census: per thread CPU %, running / READY / sleep / wait / suspended, csw, priority, CPU sets; sampler HIGHEST, not pinned, default every 100 ms (a snapshot costs up to ~16 ms on pc2) | `tests\run_thrsched_harness.py` (5 spinners on one CPU: READY seen, RTP raise / restore) |
| `[Profiler] Focus` | `src\sp.cpp` | SP samples KIDSTaskScheduler0 (group "logic") and the dispatcher every tick | log format unchanged otherwise; `sp_aggregate.py` reads "logic" |
| `[Patches] RTP`, `[Tuning] RTP_Level` | `src\thrsched.cpp` | render threads to ABOVE_NORMAL / HIGHEST, OFF restores | pc2 CPU1: **NEUTRAL** (+0.1 %) - keep off |
| **`[Patches] PCM`** | `src\psomemo.cpp` | graphics-PSO lookup memo at 1196BD (patches.json PCM1) | `tests\run_psomemo_harness.py`: 2M lookups vs a model incl. equal-hash keys, inserts, clears + refills: 0 mismatches; `test_patterns.py` ALL 245 SITES OK |
| - | `tools\testmatrix\cpu_live_sched.ps1` | the fastab LiveCtl ABBA method at any spot (here the Continue spot): diag phase + per-item ABBA windows, report with paired deltas | dry run against a fake ack writer |

## 5. PCM on pc2 (CPU3) and the Ally

| arm windows (fps, mod frame clock, 20 s each) | A | B | B - A |
|---|---|---|---|
| PCM ABBA x4 | 38.50 38.57 38.53 38.61 | 38.73 38.69 38.76 38.73 | **+0.18 fps (+0.5 %), SE 0.03** |
| TG ABBA x4 (same launch) | 38.67 38.71 38.68 38.70 | 39.89 40.00 40.05 39.94 | **+1.28 fps (+3.3 %), SE 0.03** |

- `stats/s PCM: switch ON | lookups 27314/s (~700 per frame), hits 98.3 %, misses 453/s, table flushes 0`; no crash, boot
  line `PCM: graphics-PSO lookup memo installed at PsoGetOrCreateGfx 1196BD`.
- SP: with PCM on, PsoCacheBTreeFind + its memcmp drop from ~3 % of the dispatcher to ~0.3 % (in this build's accounting;
  CPU2 on the 1.7.56 base read 9.4 + 1.4 + 1.1 % for the same rows, so the per-lookup cost moves between builds / runs),
  [App]Game's drain spin falls ~1 point. The saving is ~0.7 ms of dispatcher work, of which ~0.13 ms reaches the frame.
- Estimates for other CPUs (E): the B-tree walk is cache-miss bound (up to ~50 lines per lookup), so a CPU with less L3
  saves more per lookup, but the same blocking waits bound the frame: expect +0.5..+2 % on 4-6-core CPUs, ~0 where the
  GPU binds (ReBAR off mid-range, Ally USER).
- Ally (#954): not run - PCM does not meet the bar to ship. If it is ever bundled, the Ally check is: PCM = 2 in the ALLY
  profile, LiveCtl ABBA at the usual spot, plus a no-harm look at the picture (PCM cannot change pipelines by design).
- Mod overhead seen in passing: with the 1.7.57 base (SBM build) the dispatcher shows `[Nioh3EngineFix.asi] <-
  SynFlushCbvTables 118DA8` 5.9 % that the 1.7.56-base runs (CPU1/CPU2) do not have, at the same fps. Owner of the
  CBV-path hook (CM / SBM) should check that the hook does nothing per draw when CbvMirror / SBM are off.

## 6. Next (ranked)
1. **Who blocks the dispatcher** (~4.9 ms/frame on pc2): FnProbe / a QPC bracket on 1139D4 (ResourceBarrier: which
   barriers block in the driver, UAV on particle buffers per d3d12-submission.md 2?), 7F64DC (who holds c+CF8h
   exclusively, and when in the frame), 86F828 cold path 1921A43 (how long the ECL ring is full, and what the ECL thread
   waits on: queue Wait tokens = cross-queue syncs). Measurement only, one pc2 launch.
2. Then the matching fix: batch the dispatcher's single-UAV barriers (tok 0x8A / 0x7D) when the driver wait is per call;
   shorten the exclusive section of c+CF8h; or one more ECL ring slot / fewer serialising Wait tokens.
3. PCM compute twin (58C0D8 / 58C144) only with item 1 done (same ~0.5 % class).
4. When the drain spin is near 0, the main-thread UI items bound the frame: W1B (built), E, C (ui-mainthread.md).
5. TG for 6-core players as an opt-in recommendation (+3 %, behaviour change, docs only).

## 7. pc2 GPU with ReBAR ON: new top costs and what is still anomalous vs the RX 9070

Data: the #1096 session already ran GP2 per-PSO (MinRun 1, Top 100) at the Continue spot (bridge #179, `B_L03_BASE.log`,
7 gameplay windows). Digest: `py -I re\scripts_gpu\ally_budget.py gp2 <log> --min-draws 2000`; cross-system:
`py -I tools\xsys_compare.py --in pc=captures\xsys\pc-20261009-0638\run01.log --in pc2r=<B_L03> --in pc2off=captures\xsys\pc2-20261009-0616\run01.log --min-draws 2000 --base pc2r`.

- **Span 26.7 ms, covered only ~17.8 ms (66 %)**: ~9 ms per frame the graphics queue has no work = the CPU-side tail
  above. No single PSO is above 1.5 ms; the profile is flat.
- Top rows (ms/frame): copy/resolve/clear 1.47, FSR3 accumulate 0.81, meshlet G-buffer PB2 (70d0ee9f/05ad1715) 0.67,
  async cs 5473bfb7 0.62 (unnamed, only at this spot), VCLOUD b76874bb 0.62 (was 2.4-2.9 without ReBAR), main PB2 0.52,
  RefProbe 0.43 + 0.33 (gi replacement), MPC 0.40 + 0.33, G1E particles 0.36 (120 draws).
- **Family anomaly vs RX 9070 (speed-normalised, 1.0 = scales like the GPU):** meshlet LOD/culling 1.20 (was 5.6 without
  ReBAR), G-buffer 1.17 (1.77), shadow maps 0.97 (2.6), VCLOUD 1.9 (4.5): **the PCIe anomaly is gone.**
- **Still flagged (spot-confounded, needs the matched run):** copy/resolve/clear **1.73 ms vs 0.23 ms on the PC = ~3x worse
  than raw speed** (largest single row 0.40 ms, 9.8 per frame, no marker), and 0.97 ms of unnamed graphics rows (5x).
  Per-shader rows that differ (MPC a09d4223 0.33 ms, PB2TREE2 92139110 0.24 ms) have different draw counts at this spot.
- Queued: **XSYS2** (#1125) = the XSYS1 matched tour on pc2 with ReBAR ON, after the PC's 'nioh3' lock is released. If the
  copy family stays ~3x, the next step is a GP2 MinRun 1 split of the copy rows by marker / caller (gpu-unattributed.md).
- GPU work is not the frame's bound on pc2 today (section 0); GPU savings there pay only after the dispatcher tail shrinks.
