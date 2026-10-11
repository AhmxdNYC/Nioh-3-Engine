# GPU tile mappings (UpdateTileMappings) vs the device-wide GPU stall — 2026-10-07

Question (gpu-scheduling.md §B.2): are the ~1.35 ms/frame "gated" GPU stalls (GFX, compute and DMA all resume
together) caused by tiled-resource mapping updates? Static census (dump v2.02, Iced scans, Ghidra), no game run.
Scripts: `re\scripts_d3d12\tiles_scan.fsx` (candidate vtable calls), `tiles_census.fsx` (argument-shape check),
`tiles_dis.fsx`, `tiles_tokemit.fsx`; raw outputs in `<re-scratch>\tiles\`. Symbols: `re\symbols\incoming\gpu-tiles.tsv`.

## 1. Census

| API (queue vtbl slot) | engine call sites | queue | who |
|---|---|---|---|
| UpdateTileMappings (8, +40h) | **2**: `290FFB` (CacheTileBatchFlushDirect 290F94) and `2F0109D` (GfxExecUpdateTileMappings, ECL token 4) | cache **COPY** queue / **GFX** queue `[dev+4270h]` | KTGLGraph resource cache only |
| CopyTileMappings (9, +48h) | **1**: `2EFC08B` (ECL token 5) | GFX | cache, rare (2F10FA4 only) |
| CreateReservedResource (device +F0h) | 3 candidates: `2F03F9C` (backend texture creator), `62A191`, `62D679` | – | load time |
| GetResourceTiling (device +150h) | 151140, 24F408, 62B4E4, 2EE5C58 | – | – |
| Signal (14) / Wait (15) | ECL tokens 7/8 (4F91C0 / 4F93C8); direct calls in the cache job (290900) | all | see below |

All other `call [reg+40h/48h]` with enough stack arguments (52 + 115 candidates) fail the argument-shape check
(dword NumRanges at +30h, dword Flags at +50h).

**Two paths, one cache** (KtglCacheInit 2EF053C, object `[45BA588]+61A0h`, texture streaming with reserved textures):
- **Job path** — pool "KTGLGraph.Cache.UpdateTileMapping" (32 threads, one runs at a time under SRW c+CC0h):
  CacheTileMappingJob 126630 → CacheTileRingProcessCopyQueue 290900 drains the request ring: map/unmap entries →
  UpdateTileMappings on the cache's **own COPY queue** (created by 2F110A8; falls back to the GFX queue if creation
  fails), ring entries of state 3 → **queue->Wait(fence, value)** on that COPY queue, then **one Signal** per pass.
  Batches in flight are throttled by a fence ring (b+468h..488h).
- **Frame-begin path** — RenderFrameBegin 4A529C → RenderFrameBeginCacheFlush 32FE938 → **CacheFlushOnGfx 2EF0938 (x2)**.
  This is **not every frame**: 32FE938 is gated by MeshletManagerIsEnabled && 4A56F8, true only when the render size changed
  (`dev+40CCh/40D0h != cached +134h/+138h`) or a force bit is set (renderer/frame-overlap.md section 0). When it runs,
  whatever the job has not taken yet is recorded into the **GFX** command stream as ECL token 4 (UpdateTileMappings),
  followed by token 7 Signal (2 GFX Signals even with no tile change) + fence->SetEventOnCompletion. The ECL thread
  replays token 4 on the GFX queue **before the frame's first command lists**.
- Batching already exists: one call per run of (resource, heap, map-or-unmap); flush on resource change, heap change,
  map↔unmap switch or capacity. **Each region is one tile (1x1x1 box), each range one tile** (290DF0 / 290F10).
- Calls/frame are dynamic (streaming): ≤ 16 tile requests per texture per frame under a global budget
  (TextureStreamRequestTiles 1290B8). The engine counts tiles itself (cache stats +3Ch, snapshot per frame by
  PagePoolFrameUpdate 252510 → c+A10h). A runtime count needs the game: `[Profiler] TileStats = 1` (below).
- **The sun VSM does not use D3D12 tile mappings**: no UpdateTileMappings site outside the cache; the VSM page table
  is the engine's own (pm+40118h / +41720h, physical-page entries; shadows-culling.md §1.3).

## 2. Does it explain the stall?

**Partly plausible, not proven; refuted for the VSM.**
- For: the frame-begin path puts UpdateTileMappings at the **head of the GFX stream** (but only on render-size-change
  frames, section 1, which weakens this as a per-frame cause). On Windows a queue-side tile
  mapping is a VidMm paging operation (page-table update) that the queue must wait for, after the queue's earlier
  work; compute (async slots) and DMA lists are fenced against GFX, so they start together once GFX resumes. That
  matches r5 frames 23127/23128 ("first list started 1.4 ms after its submit on an idle GPU", "queued 1.77 ms ahead").
- Against / unknown: frame 23129's stall is at the 3rd list, not the head; the job path's COPY-queue updates are not
  waited on by GFX (no GPU Wait on the cache COPY fence found; completion is polled on the CPU), so they gate GFX only
  if the OS serialises paging work device-wide. RGP logs no tile ops, so the r5 trace cannot settle it.
- **Decisive test (one session, PC or Ally):** `[Profiler] TileStats = 2` → calls/frame per queue type, CPU µs inside
  UpdateTileMappings, plus a 256-call QPC trace to line up with an RGP capture (RGP queue events carry CPU QPC). If
  GFX UTM calls/frame ≈ 0, the hypothesis is refuted for the PC.
- Under vkd3d-proton (Ally) every UpdateTileMappings becomes a vkQueueBindSparse that serialises against the queue's
  submissions (semaphore wait/signal) and RADV applies binds with kernel VM updates — so per call it costs more than on
  Windows, and the GFX-queue calls at frame begin are the expensive ones.

## 3. Same-result fixes

| fix | same result? | status |
|---|---|---|
| **TileBatch**: merge consecutive UpdateTileMappings on the same queue (same resource, same Flags, same heap or one side NULL/SKIP-only, single-tile regions, ranges cover the regions exactly, **no tile in both**); the held call is issued before any other op on that queue (Signal, Wait, ECL, CopyTileMappings, an unmergeable UTM) and at every Present | yes: D3D12 applies region/range pairs in order and disjoint tiles commute; every fence/list on that queue still sees the final mapping | **built**, `[GpuOpt] TileBatch = 0` (1 merge, 2 count only); harness bit-exact |
| Dedup (skip a tile re-mapped to the page it already has) | needs the full mapping state; resource addresses are reused, so a shadow can be wrong | **counter only** (TileStats = 2, "redundant regions (est.)", inflated by address reuse) |
| Move frame-begin GFX updates to the cache COPY queue | changes queue + fence order (GFX would need a Wait on the COPY fence) | **not built**: not same-ordering; design only if TileStats shows GFX UTMs in the stalls |
| NO_HAZARD flag | skips the runtime's wait for prior work: only correct if no in-flight use of the pages (engine unmaps pages still in use and Waits) | rejected |
| Pre-mapped page pool | pages are already pre-allocated in heaps; what changes is each texture's own VA mapping, which cannot be pre-mapped without changing what the texture samples | not applicable |

The engine already batches per (resource, heap, map/unmap), so TileBatch only merges across map↔unmap and
NULL↔heap runs of one resource. Synthetic engine-like sequence: 14 calls → 9.

## 4. Expected gains (unmeasured)

- **PC** (GPU-bound at the test settings, about 100 fps; CPU-bound at high fps): ~0 fps when CPU-bound. When GPU-bound, at most the
  part of the 1.0–1.35 ms gated idle that is tile mapping; TileBatch removes only the merged calls' extra paging
  syncs → realistic 0–0.3 ms/frame. Only TileStats data can turn this into a number.
- **Ally** (GPU-bound, 16.3 fps at MAX ≈ 61 ms/frame): each removed vkQueueBindSparse saves one queue serialisation;
  worth it only if TileStats shows several GFX-queue updates per frame. Order of magnitude: 0–2 %.
- Next step: one Ally session with `TileStats = 2`, `TileBatch = 2` (counts merges, changes nothing); then A/B
  `TileBatch = 1` only if would-merge/frame > ~1.

## 5. Code

`mod\Nioh3EngineFix\src\tilestats.cpp/.h` (new, not wired in; see `mod\Nioh3EngineFix\tilestats_INTEGRATION.md`),
`tests\tilestats_harness.cpp` + `run_tilestats_harness.py`: real D3D12 device (RX 9070, tiled tier 4), reserved
1024² R32_UINT texture, 14 engine-style calls incl. overlap / heap switch / other resource / REUSE_SINGLE_TILE / null
counts / a 2x1 box / CopyTileMappings, read back per tile: **identical to the model for TileBatch 0, 1 and 2**, merged
5 (= dry-run would-merge 5), OnFrame issues a held call, /W4 clean. ALL OK.
