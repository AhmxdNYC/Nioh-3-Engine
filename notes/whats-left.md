# What is left to optimize (2026-10-08, ASI 1.7.39)

Audit of gaps, not of built items. Scope: game-side only (ASI, shaders, game data, the game's own API use), same look.
Nothing was run in the game, no Ghidra run, nothing in `mod\`, `tools\testmatrix\` or `captures\` changed.

Inputs: remaining-work.md, pc-cpu-next.md, ally-gpu-budget.md, perf-coverage.md, cpu-hotspots-ally.md, vkd3d-audit.md,
gpu-scheduling.md, stutter.md, mainthread-logic.md, parallel-drawprep.md, STATUS.md, MODLOG.md. Digests run for this note:
- `py -I re\scripts_re\perf_coverage.py --self --precise --tsv` and `--precise --incl 60 --tsv`, joined with a grep of every
  RVA over `notes\*.md` (hot functions with 0-1 notes = never studied).
- **New:** `py -I re\scripts_cpu\sp_aggregate.py captures\ally\gpu_budget_1531\nioh3_ally_modlog_gpub-S1_1500.log 32 56 30`
  = the first SP digest at the **USER preset with GF on** (1.7.31, 25 x 5 s gameplay reports = GP2 windows 6-9). Every
  earlier Ally CPU number (cpu-hotspots-ally.md) is MAX, pre-GF, ASI 1.3.1. `perf_coverage.py` reads only the 5 old
  top-level Ally logs, so these USER numbers were in no table.
- GP2 `frame ... covered` lines of the same log; `gpuprof.cpp` l.608-630 (covered = DIRECT-queue intervals only).
- PC cpuprof r5/r6/r7/r9 per-thread self rows for the unstudied functions.

Tags: **M** measured, **E** estimate. "% main" = share of the SP/cpuprof samples of that thread (wall time).

**Update 2026-10-08 22:00 (real play session, `session-2026-10-08-perf.md`):** at the user's own PC settings (1440p,
75 %, XeSS, FSR FG, VSync ON) the PC is **GPU-bound** (GPU busy 90-92 %, real 100-118 fps, main thread waits 2.15 ms/frame
on the waitable), so the PC CPU rows below (#4-#7, #10, #11) are fps-neutral there; they still matter for abba2-like CPU-bound
settings and the Ally's future CPU ceiling. The only gameplay hitches were **first-use PSO compiles** (boss kill 98-167 ms,
worst create 153 ms; 25 creates >= 1 ms per session) -> new row **0 (PJ)**. Normal fights cost no more than exploration;
the boss arena does (GPU 7.1 vs 5.4 ms per presented frame). No SP in that run: no new per-function CPU shares.

## 0. Five facts the existing notes do not state

1. **Ally main thread at USER is 62 % game code (M, S1):** ~17.8 ms of draw prep per 28.7 ms frame; 31 % blocked in the GF
   event wait. So on the Ally the main thread alone caps at **~54-56 fps** at today's CPU clock: reaching 60 needs CPU
   draw-prep cuts too, not only GPU. Main-thread CPU fixes are not "fps-neutral on the Ally" in the long run.
2. **The Ally runs none of the PC CPU fixes:** S1 `stats/s exp` reads PP 0/0, FPC 0, M1 0, E1 0; the ALLY profile has
   PP = 0, AABB = 0 while PP is a PC KEEP. Measured targets on Ally main at USER: PP 2523FC 1.27 %, MA sites (4AA34 2.12 +
   419A8 0.75 + 4B104 0.65) 3.5 %, P1E7 family (1E7050 2.03 + 128EF0 1.22 + 1290B8 0.39) 3.6 %.
3. **R2a / R2b / R1 (vkd3d barrier fixes) never ran on the Ally**, the only device they target: all 20 Ally logs print
   `TraceRecords=0 R2a=0 R2b=0 R1=0`. STATUS lists them as "Wine-only no-ops" = dropped for the wrong reason (they are no-ops
   on Windows by design). vkd3d-audit B6 needs one Ally trace before deciding.
4. **The Ally GFX queue is idle or uncovered 1.4-2.3 ms per frame (mean 1.67 ms = 5.7 %, M, S1 windows 6-9):** span
   28.70-30.37 ms vs DIRECT-queue intervals 27.22-28.07 ms. That is the time outside every PSO interval: list heads before
   the first draw (COMMON->X barriers land here), ECL boundaries (~21-28 vkQueueSubmit2 with a serializing semaphore each),
   tail barriers. This is the "~5 % not attributed per PSO", and it is not in any per-PSO row.
5. **Ally logic still over-ticks at USER:** `game ticks 48.3/s vs rendered 34.8/s` (1.39x), worker wakes 23.5k/s (`WK wakes
   kept 23457`), WinMain pump 846 Sleep(1)/s (W3_TimeoutMs effectively 1 in that run).

## 1. Critical path per device

| device / scene | bounding chain (M) | what shortens it |
|---|---|---|
| PC abba2 scene, 1080p FSR 50, ~110-118 fps | KIDSTaskScheduler0 logic frame: serial self (ObbToAabb family 0.73 ms, BoundsFromPartObbs 71C60 0.28 + SinCos 54360 0.13 + Vec4CopyW0 0.12, UiNodeUpdateWorldMatrix 66D00 0.18, RollingAvg 0.11, JobQueuePoll 0.17) **+ job-join wait 1.44 ms** (NtWaitForAlertByThreadId 15.8 % of Scheduler0 in r6-A1) -> [App]Game waits 0.6-1.05 ms for the packet (D spin) | AABB (built), BP (new), UI skip (new), and the join: KIDSTaskScheduler1-6 are only **~24 % busy** while Scheduler0 waits, so the join waits on a **tail** (one long job or an uneven split), not on throughput. `JobParallelForSplit` 204584 halves a range until it is <= the grain at job+78h, so leaf cost varies with object mix. Needs JT (below) before any change |
| PC real play at the user's settings (1440p 75 %, XeSS, FSR FG, VSync ON; 2026-10-08) | GPU 90-92 % busy; rendered 100-118 fps; [App]Game waits 2.15 ms/frame on the waitable; 10-20 % of FG frames discarded by DWM (composed flip) | GPU passes (Ally list applies), PJ for hitches; VSync OFF + VRR is a user setting (user-settings.md PC 12) |
| PC render-bound scenes (r5) | [App]Game draw prep; 18-26 % of it in "other render-graph nodes" never attributed (535188's 53 KB frame defeats the 32 KB stack read, mainthread-logic.md 5) | deep-stack profile first (R10 below); PP/MA/P1E7/DQ already queued |
| PC GPU (r5 trace) | device-wide gated stall ~1 ms/frame (gpu-scheduling B2) | TileStats census (remaining-work 3.6, wired in dllmain now) |
| Ally USER | GPU 28.7 ms (99 %); dispatcher 38.5 % blocked on ring backpressure (CmdListRingSubmitUpTo) = GPU-bound confirmed; GFX-queue gaps 1.67 ms | GPU items only for fps today; main thread (17.8 ms) becomes the bound below ~18 ms GPU |
| Ally load | manager A in-flight cap, ReadFile (remaining-work 4.1) | in flight (LC / SR) |

## 2. Ranked candidates (new or mis-dropped; built-and-queued items are not repeated)

| # | candidate | device | measured target | est. saving | evidence | effort | game? |
|---|---|---|---|---|---|---|---|
| 0 | **PJ in real play** (built 1.7.31, `[Patches] PJ = 0`, never A/B'd): journal the runtime PSOs of one play session, prewarm them at the title, replay the same mission + boss | PC (Ally: PSO hitches not seen) | 247 runtime PSO creates per 22-min session, 25 >= 1 ms, boss-kill frames 98-167 ms, mission 53-120 ms (M, SPR) | the only > 33 ms gameplay frames: 6 of 6 (M); title prewarm time E | session-2026-10-08-perf.md s0/s1; stutter.md 6.1 | 0 build, 1 PC session pair (journal run, prewarm run) | y |
| 1 | **R2a/R1 Ally run**: one `[GpuOpt] Barriers=2 TraceRecords=200000` launch -> `vkd3d_cost.py` by caller2_rva (B6 thresholds), then ABBA R2=1 / R1=1 | Ally GPU | GFX gaps 1.67 ms + in-list COMMON drains (PC proxy 485 EOP waits/frame) | 0.2-0.8 ms E (0.25-1 fps) | fact 3/4; vkd3d-audit B4-B6 | 0 build, 1 Ally session | y |
| 2 | **GQ gap census** (GP2 add-on): per frame, per ECL: gap from previous list end to first stamp, tail after last stamp, marker of the list; log top gaps | Ally GPU (meas.) | 1.4-2.3 ms uncovered | enables ECL merge (vkd3d design 3) / R2; 0.3-1 ms E if gaps are per-submit | fact 4; gpuprof.cpp covered = DIRECT intervals | 3-4 h ASI | y (same session as 1) |
| 3 | **Ally CPU bundle**: PP + MA (+ AABB after PC verdict) in the ALLY profile, A/B on CPU W / GPU MHz / main busy | Ally power + future fps ceiling | main 1.27 + 3.5 % (+ logic 0.35 % AABB, MAX data) | ~1.3 ms main/frame (E from shares), 0.3-0.6 W E -> GPU MHz | fact 1/2; PP measured -0.26 ms on PC | ini only | y |
| 4 | **JT critical-path probe**: hook JobInvokeTimed 1113B4 / JobNodeExecute 111184 (the engine already brackets jobs with QPC when d+38h has a stats block) and log, per logic frame, start/end/thread of every job joined by Scheduler0 plus the join start/end | PC (Ally later) | join wait 1.44 ms/frame Scheduler0 | measurement; enables #6/#7 | §1; workers 24 % busy while Scheduler0 waits | 4-6 h ASI | y |
| 5 | **BP**: BoundsFromPartObbs 71C60 own loop + Vec4CopyW0 174610 + SinCosApprox 54360 as one SIMD cave (bit-exact like AABB) | PC logic (critical) | 0.28 + 0.12 + 0.13 = 0.53 ms Scheduler0 (6.1 % r6-A1) | 0.25-0.35 ms E logic -> fps where logic-bound | pc-cpu-next "next RE target"; not built | 6-8 h RE + cave + harness | n (build), y (A/B) |
| 6 | **JG** object-update split grain (job+78h at the fan-out root 182000/408FE0) or reorder: kick the longest job first | PC logic | part of the 1.44 ms join | 0.2-0.6 ms E (only if JT shows imbalance) | 204584 decompile: binary split down to a fixed grain | 2 h after JT | y |
| 7 | **UI skip**: UiNodeUpdateWorldMatrix 66D00 recomputes world = parent x local whenever bit 8 is clear; memo (parent world ptr+version, local) -> skip identical multiply | PC logic (critical) | 0.16-0.18 ms (2.0 % Scheduler0) | 0.06-0.1 ms E | perf-coverage pass 2 (behaviour), 5 notes cite it, none designs a fix | 4 h | y (A/B) |
| 8 | **R10 deep-stack profile** `cpuprof.fsx 20 <out> 1000 2 24 131072` (mainthread-logic 5; never run) | PC main | 18-26 % of [App]Game unattributed (~1.6-2.3 ms/frame) | measurement; picks the next main-thread target | parallel-drawprep 6 has the command only in a DE script | 20 min game | y |
| 9 | **SR111**: synchronous resource wait `111064` (`while (!ResourceIsReady) Sleep(1)`, cold chunk 194ADC1) under MeshletExtraDispatch 5DA48 | Ally main; loads; stutter | `[ntdll] <- 5DA48` 3.3 % of Ally main at USER (~0.95 ms/frame), 28 % in load phase G5 | 0-0.9 ms main E; load seconds E | decompiles 0x5da48 / 0x111064; attribution unverified (SP may skip the cold-chunk return) | FnProbe 111064 + 5DA48 first (ini), fix 4 h | y |
| 10 | **D2 probe**: count StageSlotTableUpdate rewrites whose 0x90-byte slot equals the old one (and the token re-sent) | PC main + dispatcher | 1D8720/1D84A0/1D8220/1D7EB0 0.46 ms PC; Ally 0.72 + 0.60 % main | 0-0.3 ms E | parallel-drawprep 5.3 "largest remaining eliminable block" | 3 h probe | y |
| 11 | **MC memcpy size histogram** for MemcpyErms B94230 / Memcpy B94250 callers (KTGL stream) -> small-copy AVX path if most copies are < 512 B | PC main, Ally main | PC 0.47 ms; Ally 3.9 % main (1.1 ms) | 0.1-0.25 ms E PC | "none safe" was about copy count, never about copy speed | 2 h probe + 4 h | y |
| 12 | **AS0 on the Ally** (built; PC "neutral") | Ally GPU | exe CS dfe80724 175.7 dispatches/frame 0.34 ms + effect chains on GFX | 0-0.3 ms E | ally-gpu-budget row 23; gpu-scheduling A4a | 0 build, 1 launch pair | y |
| 13 | **WG on the Ally** (built, never Ally-run) | Ally power | 23.5k wakes/s at USER (more than MAX's 14-16k) | 0.05-0.15 core E | fact 5 | 0 build | y |
| 14 | Obj worker block never studied: ObjEntryTableMatch 3AF00, ObjMarkDirtyRecursive 2040A0, ObjHandleResolve 1A188C, TransformedBoxAabb 6FED0, ObjLockWordSetAndRefresh 203B74, ObjPartsAnyVirt508 1A6FC4, ObjSnapTransform 203BD4 | PC workers | 0.25-0.65 % of each of KIDSTaskScheduler1-6 (~0.3 core total) | only if JT puts them on the join tail; 6FED0 may reuse the AABB SIMD | 0 notes each | 4 h RE | n |
| 15 | PlaceableDistanceTest 47550 (StageUpdate x8, 0.2-0.8 % each), ParamTableGet_C4984B63 1201F8 (1.14 % PC self, 1 note) | PC workers | see left | small | 0-1 notes | 2 h | n |
| 16 | MaterialBindS2FAcclParams 3C4B74 per-draw strncmp classify -> per-technique memo | Ally main (MAX 1.02 %, USER 0.24 %) / PC 0.37 % main | 0.05-0.07 ms PC | 0.03-0.05 ms E | perf-coverage pass 3 (strncmp on every draw) | 3 h | y |

Not worth more work (checked): the perf-rename-4 low-confidence bodies (AnimPoseEvalSkinningBatchHuge 2C5AA6C,
SceneVisibilityGatherLarge 556118, FrameTimeFloatSimStepLarge 398A54) are < 0.0001 % of samples; DrawEmit 1E80B0 per-draw
filter (necessary work); WaitDispatcherDrainSpin 418F40 at USER 2.7 % of Ally main = ~0.03 core; RollingAvg 12B440 (not
provably identical, 0.11 ms); PhotoModeCopyGbuffer 1239860 (0.76 % Ally self in the old MAX logs, not in the USER top 30).

### Ally power: spin / poll / wake sites with measured share (S1, USER, GF on)
| site | share (M) | waste? | lever |
|---|---|---|---|
| GF event wait `<- 1180A4` | 30.9 % main | no (blocked since GF) | done |
| `<- 5DA48` (111064 Sleep(1) loop?) | 3.3 % main | 1 kHz poll if real | #9 |
| WaitDispatcherDrainSpin 418F40 | 2.7 % main | spin, ~0.03 core | K1 not built; low |
| JobQueuePoll 411C0 + JobWorkerPoll 40800 | 0.70 + 0.40 % of Scheduler group | poll | J1 built, Ally off: add to bundle #3 |
| XtimeGetTicks (timed condvar) | 0.52 % Scheduler group | timed waits | none |
| worker wakes | 23,457/s | ~6k/s futile at MAX | WG #13 |
| WinMain pump | 846 Sleep(1)/s | poll | W3_TimeoutMs = 3 (profile, provisional) |
| logic ticks | 48.3/s vs 34.8 fps | work never shown | TG (opt-in, behaviour) |

## 3. GPU leftovers (Ally USER)
- Unattributed: GFX gaps 1.67 ms (fact 4, #1/#2). Separately, ~199 PSO buckets below the 0.03 ms log cutoff are inside
  "covered" but in no row (<= ~1 ms in total); GPUProf2_Top is clamped at 100 (gpuprof.cpp l.806), so listing them needs a larger clamp.
- Copy/resolve/clear 1.54 ms is mostly absorbed draws: the `GPUProf2_MinRun = 1` run (ally-gpu-budget 7 step 1) is still
  the only way to split it. Add it to the #1 session.
- Async overlap: fog + VCLOUD already async (3.3-3.5 ms async/frame). Compute still on GFX: MPC 1.37, MCLRC 0.65, GVD 0.36,
  dfe80724 0.34, RefProbe 1.26 ms. Only the effect slot (AS0, #12) has a ready path; RefProbe async is weeks (RP2).
- Redundant passes at USER: none found beyond the closed items (VideoOut gamma copy only in cutscenes, RCAS not a no-op).

## 4. Boss / combat / traversal session: what to capture (PC, tools\playsession)
The kit already sets `[Debug] FnProbe BossLog RematchLog PadLatency`, `[Profiler] MemStats LoadStats LoadGates`,
`[Patches] CpuStats TS SPR`. Add (measurement only):
```
[Profiler]
SP = 1
IntervalUs = 2000          ; half the default suspend rate: lower overhead over a long session
ReportSec = 10             ; boss fights are 1-3 min: 6-18 reports per fight, sliced by the BossLog start/end times
TopN = 40
StackBytes = 16384         ; max: fewer stale "<- fn" labels (5DA48-type)
WakeStats = 1
GPUProf = 2
GPUProf2_SampleEvery = 8
GPUProf2_Top = 100
GPUProf2_MinRun = 4        ; keep overhead at ~0.1 ms; MinRun 1 only in the shrine control segment
TileStats = 1              ; tile-mapping census (combat streams new effects / textures)
[Debug]
FnProbe = 1                ; default 14 RVAs + add 111064, 5DA48 (SR111) if the list is configurable
```
Protocol: 60 s standing at the shrine before the boss (control), then traversal to the boss (streaming), the fight,
one death + respawn if it happens (load + PSO), and 60 s after the fight. Keep the user's own graphics and fps cap (a
cap hides the CPU bound: note it in the report). Read-outs: per window SP main / Scheduler0 / KIDSTS shares vs the
control (combat should raise effects 365AA98, G1E particle draws, collision D80B0, AI/mission), GP2 rows that appear only
in the fight (new PSOs, particle and post passes), SPR runtime PSO creates >= 1 ms, TileStats bursts, frames > 2x median
from PresentMon matched to SP reports by time.

## 5. Stutter / frame pacing still unmeasured
- PC: 106 CPU-side spikes of 20-50 ms (~2.3/min uncapped, stutter.md 2.3) were never attributed; all PC captures are
  static 20 s spots. Traversal, combat with new effects and post-load are unmeasured (remaining-work 2.7).
- Likely causes, in order: first-use PSO creation (SPR counts it; PJ built, untested), synchronous resource waits on
  the main thread (111064 Sleep(1) family, #9; 1110C8 has the same idiom at 194AE38), texture tile-mapping bursts
  (TileStats), effect first-use loads, autosave.
- Gap in the tools: SP reports are 5-30 s aggregates, so a single 30 ms frame is invisible. A spike-triggered SP
  snapshot (keep the last 50 ms of samples in a ring; dump when PresentMon-equivalent frame time > 2x median) would
  name the code of each spike. Not built; 4-6 h ASI.

## 6. Top 3 to start next

**1. Ally barrier session (R2a/R1 + gap census).** Build GQ first (#2): in `gpuprof.cpp`, when a DIRECT list is
executed, keep its first and last stamp and log per frame the sum and the top 5 gaps between consecutive lists, with the
list's first marker. Then one Ally session at USER with ASI >= that build: launch 1 `Barriers=2 TraceRecords=200000
GPUProf=2 GPUProf2_MinRun=1` (trace + gap census + the pending copy/clear split), digest with `vkd3d_cost.py` grouped by
caller2_rva joined to `barrier_sites.tsv`; if COMMON->X on buffers mid-list >= 50/frame or read->read supersets >= 50/frame
(B6), run ABBA R2=1 and R1=1 (GPUProf=0, 2 x 30 s windows, KEEP at >= +0.1 fps in both pairs, shotdiff). Expected 0.2-0.8
ms; the gap census also says whether ECL merge (design 3) is worth its binding proof.

**2. PC logic chain: JT then BP.** JT: an ASI hook at JobNodeExecute 111184 entry/exit (register-safe thunk like FnProbe)
recording QPC, thread and the job's callback vtable for jobs pushed during Scheduler0's frame step, plus the join wait
start/end in Scheduler0; log per 30 s the job that finished last before each join (median / p90 duration, which
thread) and the fan-out leaf count and grain from job+78h. Run it in the abba2 scene in the same PC turn as Block A. In
parallel (no game) build BP: decompile 71C60, write a bit-exact SSE cave for the per-part OBB build + SinCos + W0 copy,
prove it with the AABB harness pattern (300k random inputs, 0 mismatches). If JT shows a fixed long job, reorder/kick it
first; if it shows uneven leaves, try a smaller grain (JG).

**3. Ally CPU bundle for watts and the 60 fps ceiling.** No build: ALLY profile gets PP = 1, MA = 1 (key not in the profile yet: add it), J1 = 1 (and AABB =
1 once the PC Block A verdict is in) as a bundle arm. One Ally session, USER preset, GF on, same spot: ABBA base vs
bundle, MangoHud cpu_power / gpu_power / gpu_core_clock / fps + SP main share. KEEP if CPU W drops and fps is not lower;
record the main-thread game-code share (target: 62 % -> ~58 %, i.e. -1.3 ms/frame), since that is the frame time the
Ally main thread will need when the GPU side gets under ~18 ms. Run WG on/off (#13) as a second pair in the same session.
