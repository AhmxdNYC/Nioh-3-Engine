# Cheaper game-logic ticks at a preserved 60 Hz (2026-10-17)

Offline study for pc task #10160 / user #5024. No game access, installs, benchmarks or settings changes. Source reviewed at 10610bc. M = existing measured evidence; E = planning estimate, not a result.

## Recommendation

Validate the existing AABB first, then BPO on top of it. Their historical PC estimates are **0.50-0.60 + 0.12-0.25 ms of logic work per tick**. At a preserved 60 ticks/s that would remove **37-51 CPU ms/s on Scheduler0** (3.7-5.1 percentage points of one core), if the old scene's per-tick work carries over. It does not establish a 10-20% whole-process reduction or an FPS gain. Both patches and JT already exist; the build proposals in whats-left.md rows 4-6 are stale.

Before changing job scheduling, use JT to distinguish mission-thread work, character dependencies and stage dependencies. The old **1.44 ms/frame is a sampled wait budget, not recoverable computation**. Newer recorded JT windows give about **1.09 ms/tick**; the largest individual wait is a serial mission helper, not a range job. Do not spend a new implementation on blanket smaller grains.

## Units and evidence limits

The original PC abba2 evidence is 1080p FSR 50%, roughly 110-118 rendered fps and 112-115 logic ticks/s, not a controlled 60 Hz measurement. The estimates below use its cost per update as a proxy; a 60 Hz tick has different overlap, pacing slack and possibly different object work. At 60 rendered fps and 60 ticks/s, ms/tick equals amortized ms/rendered frame. At another render rate F, CPU work per displayed frame = ms/tick * 60/F. Critical-path wall-time savings cannot be obtained by that conversion.

A sleeping join spends elapsed time but does not consume equivalent running CPU. Worker job durations below are inclusive elapsed time and may contain helping, nested jobs and waits. Never add those durations to derive process CPU. Old PC user settings (1440p 75%, XeSS, FSR FG, VSync ON) were GPU-bound; historical Ally USER was also GPU-bound. PC2's older ReBAR scene was renderer-bound. These device observations are historical, not assertions about today's installed build.

## Rechecked wait evidence

Reproduced offline:

`py -I tools/gqjt_digest.py captures/tm/abba2/run8b_J/L2_JT_s2.log --skip 1`

Two retained windows: median blocked **1.085 ms/tick**, 7.09 waits/tick, per-tick p90 1.556 ms. The three largest waits sum to 0.749 ms/tick; the other sites account for approximately 0.336 ms/tick. This is not a decomposition of the older 1.44 ms sample estimate from a different run.

| Wait / producer | M ms/tick | Implication |
|---|---:|---|
| MissionThreadJoin 5AAED4; waker MissionThreadEntry E5E0CC | 0.325 | Released by MissionThread in 100% of sampled waits, outside generic jobs. Range grain cannot shorten this directly. |
| CharacterUpdateEventJoin 90CA90; signal FE79C0 | 0.291 | The 8 us signal job starts about 272 us after wait entry. That is a completion dependency, not evidence the signal may safely be scheduled earlier. Inspect upstream work. |
| Stage bucket join 39FDBC; waker 1323396 | 0.133 | Inspect stage/collision work and dependency completion; changing order alone is not justified. |

Useful attribution, not exclusive CPU: ObjTimedUpdateJob 7E35E4 14.14 ms/tick summed; range split 204584/body 616760 9.17; CollisionUpdateSerial 319D5C 7.37; ObjRegUpdateJob 111EA4 7.16; StageUpdateJobEntry FADEA0 5.44. The range row has about 91 leaves/tick, grain 16, mean leaf range 12.5. It is not obviously a few coarse leaves starving the pool.

**JT caveats found in source and raw logs:**

- jt.cpp adds overlapping inclusive job intervals; its busy percentage divides by average participating threads. The reported 118% character / 107% stage values cannot be literal core utilization. game-logic-audit.md labels these throughput-bound; treat that as a hypothesis, not proof of saturation. Nested helping can double-count. Reconstruct per-thread interval unions and runnable/idle state before a JG decision.
- Raw s2 windows show dropped jobs 29,132,327, unchanged across the three reports (also present in late s1). This is not evidence of new drops in the selected steady windows; it is a reason to require stable counter deltas and complete interval coverage before trusting tails. Wait/wake drops are zero; several waits lack a waker. Do not discard all wait timings because of a cumulative job-drop count.
- Generic JobWorkerPoll coverage omits resource-task and meshlet inline paths. A long unrelated meshlet job overlapping a wait is not proof it delays that wait.
- The digest decorates some decimal durations as symbols, e.g. 3837(IsRunningUnderWine+2607) us. Read the numeric duration from the raw log; this is formatting, not an actual call attribution.

## Ranked options

Savings are E on PC unless explicitly marked M. Per-frame columns assume 60 rendered fps and 60 ticks/s. Zero remains a possible outcome for every unvalidated proposal.

| Priority | Candidate and implementation state | Expected saved ms/tick (= ms/frame at 60/60) | Risk / next decisive evidence |
|---|---|---|---|
| 1 | **AABB**, existing SIMD ObbToAabb family | **0.50-0.60 logic work**, from 0.6-0.8 historical cost and offline arithmetic speedup; actual frame saving 0 up to the remaining logic bottleneck | Low arithmetic risk with existing exactness proof; validate correct binary and true live OFF/ON. Handoff reports PC2 +6.3%, but raw arms/settings were not reverified here; not a fresh result or a PC/Ally guarantee. |
| 2 | **BPO**, existing part-bounds memo/copy/load caves, tested incrementally after AABB | **0.12-0.25 logic work**: Q ~0.10-0.13, L 0-0.06, V 0.02-0.08; do not sum upper endpoints independently | Low-to-medium. Memo needs repeat keys/MXCSR correctness; cache collision and insert overhead can erase gain. L removes StoreLoad ordering, so arithmetic equivalence alone is insufficient concurrency proof. Start Q-only (mask 1), then V (mask 5), then L (mask 7) after ordering review. |
| 3 | **CP / SPF**, existing collision/placeable prefetch | **0-0.15 critical-path planning range**; no defensible current worker CPU ms/tick conversion without matched tick rate | Low-to-medium; excess prefetch can increase cache/bandwidth pressure. CP's old synthetic real-code loop was 16-27% cheaper, not a game result. Target character/stage tails and measured worker CPU, retaining every collision/update. |
| 4 | **Mission walk prefetch / GL1 extension**; GL1 built, segmented 20B900 walk extension proposed | **0-0.20 mission work**, old hypothesis 0.10-0.20; exposed join reduction 0-0.15 and possibly zero at 60 Hz | Medium. Existing GL1 was neutral/negative in older trials and stays off. Reprofile MissionThread first; do not move its join/kick across script dependencies. |
| 5 | **UI world-matrix memo**, 66D00, proposed | **0-0.10 logic work**, target 0.16-0.18 | Medium/high. Needs parent/local generation and invalidation, parent pointer lifetime, same FP behavior, and preservation of side effects. Build only after confirming unchanged matrices dominate current samples. |
| 6 | **JG narrower grain / earlier independent heavy job**, proposed | **0 demonstrated; conditional 0-0.20 elapsed tail** for a proven affected site | High ordering/overhead risk. Current data do not support the old 0.2-0.6 promise. Gate on reliable per-thread unions, dependency identity, leaf distribution and idle eligible workers. Never move the completion signal ahead of the work. |

Already-on LK/J1/WG are baseline choices, not additional gains. The handoff lists them ON on PC. The BPO L sites must be audited against that actual baseline rather than counting generic atomic-load savings twice. AABB reduces the child conversion inside the BPO subtree; count only BPO's incremental savings, not the full subtree again.

The worker study's older CP estimates of 0.10-0.18 core and SPF 0.02-0.04 core are useful capacity hints, but not 60 Hz measurements. Do not divide by today's tick rate when the originating sample rate and baseline differ. Parameter memo (PL), hinted collision search and pause-list caching require mutation/ordering proof and are lower priority than testing built candidates. Rolling-average math and changing collision frequency have no safe same-behavior case here.

## Concrete next task for the game owner

1. Select one repeatable scene; preserve all 60 Hz logic updates, simulation time and combat semantics. Record device/CPU/GPU, binary hashes, INI, render/logic rates, scene, FG, cap and resolution. Keep TG OFF; do not reduce tick rate to obtain a saving. Compare identical baselines except the candidate.
2. Validate AABB OFF vs ON, then AABB ON/BPO OFF vs AABB ON/BPO ON. Use Q-only then Q+V and full BPO if needed. Require fresh live toggle acknowledgements; AABB=2 installs the toggle OFF, while 0 can mean not installed. Keep all other fixes constant.
3. Short attribution pass: JT=1, JT_ReportSec=10 and SP focused on logic; separately JT_Thread=[App]MissionThread. Check tick counts, game speed, drop-counter deltas, join site medians/p90 and sample attribution. Compare profiler OFF vs ON overhead before using profile times as an absolute budget.
4. Timing pass with heavy probes OFF: paired repeated arms, external frame times/p99 plus per-thread or process CPU time per logic tick. At a 60 fps cap an unchanged FPS result is expected; use CPU work and deadline misses. At a logic-bound view measure frame-time savings separately. Reject scene drift/missing captures and report OFF and ON with settings per device.
5. Only after AABB/BPO, test CP/SPF individually if collision/stage tails remain; request a new offline JT-union analysis/tooling task before any scheduling change. Existing lead owns every game launch and benchmark.

No new performance result, default change or deployment is claimed. The strongest near-term target is **0.62-0.85 ms/tick of PC logic work** from two existing options, conditional on baseline and scene. Do not add mission waits, worker CPU and this serial-work estimate into one advertised speedup.

## Evidence pointers

- notes/whats-left.md rows 4-7: initial hypotheses, superseded build status.
- notes/pc-cpu-next.md: PC attribution/AABB estimate, invalid earlier toggle trials.
- notes/bounds-partobbs.md: BPO implementation, offline proof, incremental 0.12-0.25 estimate (replaces old BP 0.25-0.35).
- notes/game-logic-audit.md section 1: corrected mission-thread identity/dependencies.
- notes/gq-jt-probes.md and mod/Nioh3EngineFix/src/jt.cpp, jt.asm: actual hook sites, inclusive times, busy formula and coverage.
- captures/tm/abba2/run8b_J/L2_JT_s2.log: raw local evidence and reproduced digest above (capture not committed).
- notes/worker-jobs-opt.md: prefetch mechanisms and historical synthetic checks.
- notes/pc-keep-candidates.md: GL1 neutral/negative; notes/cpu-bound-midrange.md including its renderer correction: device critical paths differ.
- notes/handoff-codex-cpu-ready-aabb.md: prior AABB toggle/read-back pitfalls; .handoff/open.md #5024: preserved tick-rate requirement.
