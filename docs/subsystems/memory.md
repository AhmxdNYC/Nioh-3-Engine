# Memory (page pool, set-associative cache, heap allocator)
CPU share 3.7 % (Ally 3.3 %, PC 4.2 %); coverage 99.2 %. Threads: [App]Game (page-pool sweep), any thread for allocation.

## Role
Two things live here. (1) The GPU page pool bookkeeping run each frame by the main thread. (2) The allocator stack: one 6 GiB commit at `ArenaInit` 294C2E0 (VirtualAlloc 294D214), ~62 pool wrappers, small-block front end 12E36C.

## Key data
- Page pool at device g+61A0: fixed buckets x ways with `lastUsedFrame` stamps (capacity-bounded LRU). Frame update 252510 -> 2522C4 -> `PagePoolCountUsed` 2523FC counts entries with `lastUsedFrame >= frame`; the result feeds the occupancy heuristic 126C64 (device+0x6CD0).
- `SetAssocCacheLookup` 2515B0: bucket = key % c->nsets (127 fast path), linear scan of c->nways 64-byte entries at c+10h for entry[0]==key. `PageCacheFindOwned` 251554: owner +28h, id +30h, returns entry+4.
- Arena: object static 475A440 (pointer in [4B50348], flag +3F5F00), registry arena+3F5C50. Heap object (0xB0 B, vtable 408F940): payload +10h, header size +18h, region base +20h, bins +38h (16 x 8 B), used +58h, mutex +60h.
  Chunk header {prev, size (neg = allocated), bin prev, bin next}; payload at +20h. `HeapFree` 130D0C (eager coalesce), `HeapAllocLocked` 31242C, `HeapReset` 9C0A94.
- Lock-free slabs: `LockFreeSlabAllocSmall` 12F22C (size <= 0x4C0), Medium 202FF4, SizeClass 12E4D0; `ObjHandleRelease` 47DF8 (1150 callers, strong count in high 31 bits).
- Nothing is ever decommitted (VirtualFree only at shutdown 1FE6948), so freed pages stay dirty; on the Ally this drives swap on stage changes.

## Hot functions
| RVA | name | % |
|---|---|---|
| 2523FC | PagePoolCountUsed (not debug-only) | 1.89 |
| 2515B0 | SetAssocCacheLookup | 1.20 |
| 47550 | PlaceableDistanceTest (area heuristic, really visibility) | 0.21 |
| 47DF8 | ObjHandleRelease | 0.14 |
| 12FBDD0 | ObjPoolReuseIdleForTypeIds | 0.09 |

## What drives the cost
Page-pool size x types x slots scanned every frame (2.2-3.0 % of main); the PP option runs it every 4th frame (default OFF; paired A/B tests on PC showed no measurable change). Allocation itself is cheap.

## Deeper notes
notes/load-memory.md (heap layout, sweeper), memory-leaks.md (page pool is bounded LRU), parallel-drawprep.md (PP), mainthread-drawprep.md (2523FC), stutter.md.
