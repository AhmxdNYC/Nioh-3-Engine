# Jobs / KIDS scheduler (and object-update workers)
CPU share 19.0 % (Ally 17.3 %, PC 20.8 %); coverage 99.7 %. Threads: KIDSTaskScheduler0 (frame step), KIDSTaskScheduler1..6, KIDSTS[s2:StageUpdate] x8, all job pools.

## Role
KIDS/KTGL job system: pools of worker threads with per-pool queues plus priority bitmaps. Scheduler0 runs the frame step 5ACF80 -> 5AC9EC -> 5651D0 ->
GameModeUpdateMain 39E4E4 and joins everything; workers run object update, placeable visibility, collision, effects, meshlet requests.

## Key data and behaviour
- Pool queues + priority bitmaps polled with `lock cmpxchg [m],0` (atomic-load idiom), so cache lines bounce (`JobQueuePoll` 411C0, `JobWorkerPoll_40800`, 403A0, 3E7CC).
- Push (3FAD8/417A0) never makes a syscall; wake is "wake one on first pop", cascading. No spin before blocking. `SizeWorkerPools` 1FE2FD4 (clamp site 1FE300B): the default clamp of 32 gives 127 threads across the 14 weighted pools on a 16-thread APU (a model of the pool sizes, not a measured thread count); WorkerCap 8 gives 63, 10 gives 78. WorkerCap is off by default.
- Parallel-for: `JobParallelForSplit` 204584 claims flag +7Ch, halves index range [+70h,+74h).
- Object model: part slots o+240h (count o+120h), flags o+4Ch, dirty o+388h, param table [o+3D30h] scanned by hash (`ParamTableGetU32_D15EC139` 121E90, `ParamTableGet_C4984B63` 1201F8).
- Placeables: `PlaceableVisibilityUpdateWorker` DA830 (bit vectors world+1668h/+1680h), `PlaceableDistanceLod` DBA70, `PlaceableUpdateDetail` DBB90.

## Hot functions
| RVA | name | % |
|---|---|---|
| 418F40 | WaitDispatcherDrainSpin (main-thread end-of-frame spin on the dispatcher ring, no PAUSE) | 6.54 |
| 411C0 / 40800 | JobQueuePoll / JobWorkerPoll | 2.49 / 0.78 |
| 121FC0 | ObjUpdateWorkerBody (17.5 KB per-object update on workers) | 1.14 |
| 121E90 / 1201F8 | param-table hash scans | 0.90 / 0.59 |
| DBA70 / DA830 / DBB90 | placeable LOD / visibility / detail | 0.58 / 0.47 / 0.31 |
| 204584 | JobParallelForSplit | 0.55 |
| 2040A0 / 484C0 | ObjMarkDirtyRecursive / ObjPartAcquireShared | 0.41 / 0.36 |

## What drives the cost
418F40 is waiting on the dispatcher (K1 flush limit decides how much is left to drain). Pollers are idle-worker cost and scale with thread count (hence WorkerCap).
Object update scales with loaded objects; Scheduler0 is 79-83 % busy with ~2 ms/frame slack at 60 fps. Logic ticks 44.6/s vs 16 rendered fps on the Ally (over-ticking).

## Deeper notes
notes/job-system.md (WC, WK, wake policy), worker-threads.md (J1, M1, thread table), worker-jobs-opt.md, worker-power.md, cpu-hotspots-ally.md (over-ticking, TG),
mainthread-logic.md (ObbToAabb, rolling avg), frame-timing.md, d3d12-submission.md (K1/S1 for 418F40).
