# Main loop / timing
CPU share 2.9 % (Ally 3.1 %, PC 2.7 %); coverage 98.9 %. Threads: [App]Game (main/game thread), WinMain thread (message pump, start 38E1290).

## Role
`MainLoopFrame` 1A74130 is the [App]Game per-frame function (called from 27049CC <- 134EBD0). Game logic is NOT on this thread: [App]Game is a render task-graph executor
(3D1F20 -> 112DD8 -> 113204 per node); logic runs on Scheduler0 via the frame step 5ACF80.

## Key data and behaviour
- Stock game (the mod's VRR, L1, W3 and R1 fixes, on by default, change the next three points): simulation step is `scale / fps` with `fps = G+0x614` (target tier 30/60/120 from REN_FPS), never measured elapsed time. Dynamic FPS controller 5AF4E4 switches 120/60/30 from CPU-only frame time (`mon+0x74`).
- Frame limiter: QPC busy-wait after the update (5AD292, slow path 19DF57E); steals jobs or `Sleep(0)` (916EBC) while waiting; stores the interval at [r15+24h].
- 1 ms retry busy-wait when the frame function is not ready: constant 1A744E4, loop 1A74BD0..1A74C48 (0 % in r5, 5-10 % in r6/r7).
- FenceWait Sleep(0) spin 7D9E1C (Ally ~58 % of [App]Game before the GF patch). Present wait 7D9B6C -> 7D9E90.
- `StageObjLodUpdateRange` 469C0: KIDSTS StageUpdate worker, distance/sqrt per object vs the camera (g_pLod...). `PauseSourcesAnyActive` F33A0 scans 237 handler slots (table 45C84A8).

## Hot functions
| RVA | name | % |
|---|---|---|
| 1A74130 | MainLoopFrame | 1.63 |
| 469C0 | StageObjLodUpdateRange | 0.53 |
| 205840 | ObjVirtUpdateLocked (area heuristic) | 0.23 |
| 13233B0 | ObjBucketsRefreshDirtyCollectActive | 0.13 |
| F33A0 | PauseSourcesAnyActive | 0.09 |

## What drives the cost
Mostly waiting on QPC/Sleep (spin by design) and the tier limiter; judder at 60-120 comes from step != interval, not from cost. The 1.63 % self of 1A74130 is the busy-wait body.

## Deeper notes
notes/frame-timing.md, cpu-hotspots-ally.md (spin, WinMain pump W3), mainthread-logic.md, input-thread.md, input-latency.md, stutter.md.
