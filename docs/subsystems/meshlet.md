# Meshlet streaming bookkeeping
CPU share 11.7 % (Ally 10.2 %, PC 13.3 %); coverage 99.9 %. Threads: KIDSTS[s66:TS_MESHLET_REQ] x10 (~13 % busy each, 78 % blocked) and [App]Game (render-frame-begin 4A529C -> 5D214, ~10 % of main).

## Role
Meshlet LOD runs on the GPU (`MeshletResolveLOD`); the GPU decides residency every frame and the CPU re-issues per-entry dispatch args and loads pages. It is an O(N)
scan every frame by design. The 4BBEC part (material texture streaming) is on the Streaming page.

## Key data
- Manager M = base+169DD0: pending counter M+4C4 (gates 4BA8C register), M+4C0, flag M+1AC&40000000h, resource count `[M+30h]+190h`, entries `[M+30h]+1B0h[idx]` = {?, cost +4, bucket +8}.
- Per-entry flag shift register (ready bit0, bit7:=bit6, ...) updated by 419A8. Per-page u16 countdowns (5DF70).
- Double-buffered request lists r+58h/r+60h swapped by 7B1614. Pages are 64 KB chunks claimed from a bitmap (49D5C issues the load).
- Parity bit 0x16A0B8 selects the double-buffered GPU upload set (4DB6C -> 4DD4C).

## Hot functions
| RVA | name | % |
|---|---|---|
| 48E90 | MeshletRequestWorker (worker; re-sums page pool capacity per sub-chunk, M1) | 5.39 |
| 4AA34 | MeshletCheckRequestLoad (main thread, builds dispatch args) | 2.10 |
| 5DF70 | MeshletPageCountdownTick | 1.67 |
| 419A8 | MeshletUpdateEntryFlags | 0.99 |
| 4DB6C | MeshletUploadBuffers | 0.35 |
| 4A2D0 / 4B104 / 49950 | RequestJob / RequestLoad / MarkRequestedAddCost | 0.31 / 0.14 / 0.16 |

## What drives the cost
Number of active meshlet entries/objects; cache misses on scattered objects plus locked RMWs (`lock cmpxchg` loads). 48E90 costs 0.34-0.77 core. Offline-verified
low-risk patches exist (A1/A2 atomic-load rewrite, P4AA prefetch, C419, M1 pool-sum cache); expected fps effect is small.

## Deeper notes
notes/meshlet-mainthread.md (P1E7/P4AA/C419/S1E7), worker-threads.md (M1, M2, J1), mainthread-drawprep.md (A), gpu-culling-rewrite.md (GPU side), shadows-culling.md.
