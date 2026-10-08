# Collision / Physics (CollisionUpdate)
CPU share 12.1 % (Ally 2.3 %, PC 21.9 %); coverage 99.7 %. Threads: KIDSTS[s9:CollisionUpdate] x9 (~1 core total), kicked from Scheduler0. The CustomOccluderRayCast thread is separate.

## Role
Per frame and per collision manager (list `[45B9E58]+540h`, count +560h): gather colliders near the player, then re-evaluate each. Physics MaxThreadCount (system.rdb)
2 to 4 was tested: no gain, reverted.

## Pipeline and data
- `CollisionGatherInRange` 43CEC (grid cells around player `g9E58+1B10h`, extra points +1B20h): for every collider in range with `+A5h==0` sets +A5h=1 and pushes it onto `mgr+150h`. Cells left behind get +A5h=+A6h=1.
- Job 319E00: N item jobs + 5 strided jobs (319D5C, stride 5); waits in 3E7CC.
- `CollisionObjectUpdate` D80B0(mgr,out,start), i += 5: skip if +A5h==0; owner lookup via inline std::map find at `[[mgr]+30h]+28h` (key +0Ch/+08h), then `CollisionMapFind` D8D90 (flat sorted array [..]+180h).
- `CollisionPartMapFind` D8740: lower_bound over 16-byte {part ptr, record} at [m+10h], count [m+18h].
- `CollisionShapeGetWorldPos` D8B70: std::map traversal, then a position function from the table at 3BE3780 (e.g. `CollisionShapePos_Type26` 2D9EE0).
- `PlaceableCollisionUpdate` D8E00 (5.8 KB) rebuilds placeable collision lists on StageUpdate; `CollisionIsBeyondDistance` D89E0 (max(16000,[+98h])+radius).
- Spatial queries: linear octree `OctreeQueryVolumeCollect` 165088 (depth <= 6), AABB slab lists 1675F0/166F20, `CollisionFindClosestPointInCandidates` 199180.

## Hot functions
| RVA | name | % |
|---|---|---|
| D80B0 | CollisionObjectUpdate | 4.97 |
| D8B70 | CollisionShapeGetWorldPos | 2.43 |
| D8740 | CollisionPartMapFind | 1.01 |
| D8D90 | CollisionMapFind | 0.89 |
| 43CEC | CollisionGatherInRange | 0.66 |
| D8E00 | PlaceableCollisionUpdate | 0.50 |

## What drives the cost
Dependent pointer chasing (std::map, scattered colliders) over every collider near the player, re-evaluated every frame by design. The PC share is far higher than the
Ally's (Ally profile is dominated by spin/wait). Candidate C1: prefetch collider i+5/i+10 at the D80B0 loop head (not built).

## Deeper notes
notes/worker-threads.md section 4, cpu-profile.md (thread table), job-system.md (pool sizing for the CollisionUpdate threads).
