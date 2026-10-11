# renderer/06-resources: GPU memory, rings, heaps, transient RTs, staging, residency (package 06, 2026-10-10)

Scope: package 06 of `renderer/00-map.md` (RESOURCE rows of `re/scripts_renderer/partition.tsv`). Static work only (decompile
export `<re-scratch>\decomp`, `decomp_pool.cmd`, the dump's data). No game launched. Tags: **S** static, **M** measured (earlier
notes), **E** estimate. Builds on backend.md (rings, fences), 05-frame-graph.md (transient aliasing), root-cause-vram-streaming.md,
rebar-fix-v3.md, stutter.md §3.3, input-latency.md §3. Symbols: block `# renderer/resources` in `re/symbols/symbols.tsv`.

Struct names used below: **GfxDevice** = `[45BA588]` (g_RenderGlobals); **SynCtx** = GfxDevice+10h (the dispatcher's GFX
context; SynCtx+5A0h = GfxDevice+5B0h, the command-list ring); **R** = the Renderer (host+F8h).

## 1. Per-frame lifetimes (for frame-overlap)

The short answer: **almost nothing GPU-visible is tied to "frame end".** The GPU-visible rings are retired per *command list*
by the GFX list fence, not per frame. The only memory reset at frame end is the CPU-side token stream. The frame-slot (x3)
objects are the command allocators of the frame-slot work and the query read-back ranges.

### 1.1 Table

| memory | owner / where | allocator | size | reset / reuse rule | relative to frame end / GPU completion |
|---|---|---|---|---|---|
| **token command stream** (CPU only) | GfxDevice +3C30h base / +3C40h write | linear bump | `param+190h` (48 MB on pc2 CPU5) | **reset to base at every frame end** (CmdStreamEndFrame 418B94 -> 418C70), after the main thread drained the dispatcher (RingWaitIdleSpin 252730) | CPU lifetime only: everything in it (inline constant blobs of tok 0x66, 242EB8 8 KB-1.5 MB blobs) is copied by the dispatcher into GPU rings before the reset. **The drain is what makes the reset safe**; a pipelined frame end needs a double-buffered stream (backend.md §6 option 1). |
| **per-draw constants ring** (CBVs) | SynCtx+B28h (UploadRing) | **fence-tagged block ring**, UploadRingAllocate 1190C0 | SynCtx params +88h (bytes; not read statically) | each block carries the fence value of the list that last wrote it (tag = SynCtx+6D0h = the open list's fence value); a block is reusable when tag <= SynCtx+2510h (cached GetCompletedValue of the GFX list fence SynCtx+6C8h). Wrap = jump back to the first block (+38h). | **No frame boundary.** Reuse waits on *list* completion. Full ring -> CmdListRingSubmitIfBehind 945664 (submit up to the needed list) + 945610 (Sleep(0) spin on the fence), counter SynCtx+24A4h. Constants of frame N are safe until the GPU finishes the list that used them, whatever frame the CPU is on. |
| immediate-draw VB ring | SynCtx+2390h | same allocator (SynUploadRingAcquireBlocking 45B470, stride 18h) | params +10Ch | same fence rule; stall counter +24A8h | same: per list |
| immediate-draw IB ring | SynCtx+23D8h | same (u16 indices) | params +10Ch | same; stall counter +24ACh | same |
| **shader-visible CBV/SRV/UAV descriptor ring** | SynCtx+980h (DescRing over the heap at SynCtx+820h, count params +4Ch) | ShaderVisibleDescRingAlloc 1192B0: linear cursor +44h, capacity +3Ch, **one fence tag per descriptor** (+30h array), early wrap when `cap - cursor < +40h` (max request), wrap count +4Ch | params +4Ch descriptors | a range is free when the tag of its last slot <= completed GFX fence; allocation stamps every slot with the open list's fence value | **No frame boundary**; per list. Full -> submit + spin (counter +2494h). |
| sampler tables | SynCtx+AC0h ring (params +50h) / heap +988h | content-cached (1188AC) | params +50h | cached tables live until evicted; new ones from the ring with the same fence rule | per list |
| CPU staging descriptor tables | SynCtx+940h (stride +938h), heap +920h = 0x215 descriptors (CPU-only) | per-stage fixed slots | 533 | overwritten per bind, copied to the shader-visible ring at flush (CopyDescriptorsSimple) | CPU only, no GPU lifetime |
| KtglCache upload ring (page-pool / tile uploads, tok 0x95) | KtglCache+590h (CacheUpload u+10h) | UploadRingAllocate (KtglCacheUploadRingAllocOrSubmit 7F6A64) | ? | same GFX-fence rule; on failure submit + SynSubmitAndWaitFence **while holding the cache SRW shared** | per list |
| command lists + allocators (GFX path) | SynCtx+588h pool, list ring SynCtx+5A0h (128 lists on pc2) | ring, grows on demand (4931B0) | 128 | recycle when the slot's fence is complete (vfn+10h wait) | per list; CmdListRingFlushAndRecycle 4920E0 also runs at every slot advance (2EFB73C) |
| **frame-slot objects (3 slots)** | GfxDevice +4730h slot, +4240h BufferCount = 3, fence values +4320h[3] | modulo-3 slot | 3 | GfxFrameSlotAdvance 8BEC48 (after Present): slot = (slot+1) % 3, **wait GfxWaitFenceValue(+4320h[slot])** (= the GPU finished frame N-2), then: frame-record ring tick (8BEDC8: +3C1Ch++, entry `+3C20h[n % +3C18h]`), pipeline-stats readback for the slot (9AFED0), timestamp readback for the slot, **Reset of the slot's DIRECT allocator +4290h[slot]** (vfn+40h), CmdListRingFlushAndRecycle | the only GPU-lifetime objects keyed to frames; safe for 3 frames in flight |
| timestamp query heap | GfxDevice +4768h (heap), +4770h readback buffer, +4778h CPU copy, per slot range `[+4788h first, +478Ch last]` x16 B, +4034h queries per slot, +4790h[slot] used-bitmask | slot-partitioned | 3 x (+4034h) | resolve on the frame-end path (main thread, after the drain, backend.md §1); read back in GfxFrameSlotAdvance **after** the slot fence: Map(+4770h) -> memcpy -> Unmap, GetClockCalibration (queue vfn+80h into +4780h), range reset to {FFFFFFFF, 0}, bitmask cleared | read 2 frames late (slot reuse) |
| pipeline-statistics query heap | SynCtx+24D8h (CreateQueryHeap type 2, count = params +150h x params +7Ch slots), readback SynCtx+24E0h (committed READBACK, 58h B per query per slot), +24E8h[slot] used count, +24F0h[slot] accumulated 58h-byte sum | slot-partitioned | params +150h per slot | 9AFED0 at slot advance: map the slot's range, sum the 11 counters, zero the count | read after the slot fence |
| occlusion query heap | SynCtx+2500h (GfxInitCreateQueryHeap 2EE497C, type 0, readback count x 8 B committed READBACK) | - | params +154h | (users not traced) | - |
| **transient RTs (VideoMemoryKeeper)** | heaps registered in R+1EE530h (TransientSetAddKeeper 32F1FF4) | **persistent placed heaps**; RTs placed at fixed offsets; aliasing inside one frame by *first-use node index* | per keeper: max(KIDS size, GetResourceAllocationInfo(screen w/h >> shift, format)) | activation each frame at node k (`R+1EE568h == obj+138h`): tok 0x72 + **0x9D ALIASING barrier (before = NULL)** + **0x9F DiscardResource** (860C5C); R+1EE568h reset to 0 at RenderFrameBegin; on a resolution / scale change RenderFrameBegin calls 94541C, which re-places every keeper's resources (345CCEC) | **one physical copy for all frames in flight.** Safe today only because every user is on the DIRECT queue and the GPU runs frames in order, plus the discard at activation. No CPU access, so CPU-side overlap is free. If a transient RT is touched on an async-compute slot (VCLOUD / VFSCAT / ocean) the cross-queue order of frame N's last use vs frame N+1's activation must be fenced. |
| objdb RTs (history, G-buffer, post) | KIDS objects, whole session | committed / placed DEFAULT | - | never per frame | n/a (history RTs are read next frame by design) |
| **staging for buffer / index / texture maps** | 700 MB UPLOAD heap ring + 100 MB READBACK heap ring (UploadHeapSuballoc 62D688) | per-heap lock-free **bump arena with a live-allocation count**, reset to offset 0 when the count reaches 0 | heap count x size from device params +168h/+170h (UPLOAD), +178h/+180h (READBACK) | **synchronous**: map -> CPU writes -> unmap records CopyBufferRegion on a COPY-queue context (pool GfxDevice+43E8h), submits and **CPU-waits the copy fence** (5C0D4 -> GfxFencePollWorker 117D6C, Sleep(0)), then GpuAllocRelease 4F0FC frees the placement | lifetime ends inside the unmap call; no frame dependency. Readback maps do the reverse copy + wait before the CPU read. |
| UPLOAD-usage buffers (usage 2) mapped in place | placed in the same heap ring or committed | owned by the buffer object | - | CPU writes straight into GPU-visible memory; the *owner* must double-buffer | meshlet scene sets: 2 sets, swapped at MeshletFrameBegin (`+16A0B8 = (p-1)&1`, 08-meshlet §3.1); UI / particle VBs are a "fresh ring allocation each frame" (ui-mainthread). Any owner that rewrites a single-buffered UPLOAD buffer is a frame-overlap hazard to audit. |
| meshlet streaming feedback | 100 MB READBACK heap | written by MGLRC (uRequestCmds M+370h) | ~30 KB/frame | **read by MeshletRequestJob two frames later** (08-meshlet §4) | already 2-frame latency tolerant |
| per-frame CPU pools of the scene side | DrawRec pool (07-drawlists), meshlet request buffers (cleared at MeshletFrameBegin by MeshletRequestBuffersClear 5C708) | per frame | - | reset at frame begin by the main thread | CPU only, but tokens may carry pointers into them (backend.md §6 audit list) |

### 1.2 Rules that matter for overlapping frames

1. **GPU rings are already frame-agnostic.** CBV, immediate VB/IB, descriptor and KtglCache rings retire by list fence. Recording
   frame N+1 while the GPU runs N is safe for them; the cost is only a stall when a ring is full. All of them have **one producer,
   the dispatcher thread** (keyed by SynCtx+6D0h), so a second replay thread needs its own SynCtx-level rings and its own fence
   tag source (or a shared atomic fence counter).
2. **The token stream is the frame-scoped arena.** It is reset at frame end only after the drain. Overlapping frames on the CPU
   (main thread records N+1 while the dispatcher replays N) needs two streams (or a true ring with a read cursor), and every token
   payload pointer must point into the stream or into memory that lives >= 1 frame longer.
3. **Frame-slot objects** (DIRECT allocators of the frame-slot work, timestamp / pipeline-stats ranges, the frame-record ring)
   are 3-deep and waited at GfxFrameSlotAdvance. They already allow 3 frames in flight on the GPU.
4. **Transient RTs are single-copy.** No change needed for CPU overlap; for any GPU-side overlap (two frames' passes interleaved
   on different queues) they would need per-frame copies or a cross-queue fence at the first activation.
5. **Staging is synchronous** (copy + CPU wait inside unmap), so it never outlives the call. It is a stall source, not a lifetime
   hazard.
6. **Owner-double-buffered UPLOAD buffers** are the one class that must be audited per owner: meshlet scene sets (2), UI /
   particle streams (fresh allocation per frame), the ASI's CBV-mirror / SBM rings (own fences).
   - Open (E): the slot wait only guarantees that frame N-2 is done when frame N+1 starts recording, so a 2-set swap written by
     the CPU in place is safe only if something else keeps the GPU within one frame (the frame-latency wait in RenderWaitForFrame
     4A3334 / the swapchain waitable, input-latency.md). Not proven statically; a deeper CPU lead must re-check the meshlet sets.


### 1.3 Ring allocator details (S)

- **UploadRing** (structs at SynCtx+B28h / +2390h / +23D8h, KtglCache u+10h), created by SynUploadRingCreate 2EF3310: one UPLOAD buffer in state GENERIC_READ (AC3h), first tried as a placed resource from the GpuMem
  pool (62C618, section 2.2), else CreateCommittedResource. It is **persistently mapped** at creation.
  - Layout: +0 resource, +8 size, +10h CPU base, +18h GPU VA base, +28h node array (N x 28h B: {offset, free bytes, fence tag,
    prev, next}), +30h free-node list, +38h first node, +40h current node.
  - UploadRingAllocate 1190C0(ring, size, tag, completed, out {cpu, gpu}, out retryFence) allocates from the current node if its
    tag <= completed; otherwise it returns the tag to wait for. It merges following nodes whose tag <= completed
    (UploadRingConsumeBytes 119250), splits the remainder (UploadRingBlockSplit 7D7204), and goes back to the first node at the end.
  - CBV sizes are rounded up to 256 B (118E28).
- **DescRing** (SynCtx+980h): base index +38h, capacity +3Ch, max request +40h, cursor +44h, one u64 tag per descriptor at +30h,
  wrap count +4Ch. SynComputeStateReserveDescRingRange 11A08C is the compute variant.
- Fence tag source: SynCtx+6D0h = SynCtx+5D0h (list-ring fence counter) + 1. That is the value the open list signals when it closes
  (CmdListRingClose 490D5C: ++fence). The completed value is SynCtx+2510h, refreshed by FenceValueReachedOrPoll 4920A0.
- Async-compute slots (AsyncComputeCtxCreateQueue 2EF8448) own their rings, heaps and fences (backend.md) and use the same allocator.

## 2. Heaps and placement

### 2.1 Usage -> heap (S)
- GpuBuffer usage table `3BEA260` (12 B {heap type, initial state, CPU-mappable}): usage 0/1 DEFAULT, **2 UPLOAD (GENERIC_READ AC3h,
  mapped in place)**, **3 READBACK (COPY_DEST 400h)** (rebar-fix-v3 §2).
- Map modes `MapModeTable 3BE29D0` (2 B per mode {needs a read-back copy, needs an upload copy at unmap}): 0 {1,0}, 1 {1,1},
  2 {1,1}, 3 {0,1}, 4 and 5 {0,0}. Mode 4 means no staging, and its unmap is a no-op.

### 2.2 The GpuMem pool (DEFAULT / UPLOAD / READBACK placement) (S)
- The pool manager is `[GfxDevice+4610h]`. Every "committed" creation first tries **GpuMemTryPlaceFromPool 62C618**. It is skipped
  when GfxDevice+3F65h is set and the resource flags have any of bits 0-2.
  - GpuMemPoolPlaceByClass 62C664 picks a pool by resource class. GpuMemResourceHeapClass 50194 returns buffer 0, non-RT/DS
    texture 1, RT/DS texture 2, anything else 3 (not poolable).
  - It checks the size limit: pool+38h when unified, pool+40h+8*class per class.
  - Then GpuMemCreatePlacedResource 62C8E0 takes the pool's critical section, sub-allocates (62DD8C) and calls CreatePlacedResource
    (vfn E8h), or vfn 230h when a castable-format list is given.
  - On failure, CreateCommittedResource (vfn D8h) with the same desc.
- Pool heaps come from GpuMemCreateHeapWithPriority 2F1165C:
  - size = pages x 64 KB;
  - flags = `GpuMemHeapClassFlags 3C027E8`[class] | **800h CREATE_NOT_ZEROED**, where the classes are 0 any (tier 2), C0h buffers
    only, 44h non-RT/DS textures only, 84h RT/DS textures only (the tier-1 split, kept on a tier-2 device);
  - optional SetResidencyPriority (ID3D12Device1 vfn 170h) with a class-dependent priority.
- Release goes through GpuAllocRelease 4F0FC, by allocation kind, and then Release() of the resource. The kinds are:
  - 1 pool (`[GfxDevice+4688h][idx]`, 62D934 under the pool CS);
  - 2 UPLOAD heap ring (`[+4678h]`);
  - 3 READBACK heap ring (`[+4680h]`).
- **No deferred-free queue was found.** Callers release only after their own fence wait (synchronous staging) or at object
  teardown.

### 2.3 UPLOAD / READBACK heap rings (S)
- `UploadHeapRing_vtbl 409A748` {+8 Init 2F035C8, +10h Shutdown 2F03730, +18h Alloc 62D7F0, +20h Free 501C4 -> 878B70}.
  - Objects: GfxDevice+4618h (UPLOAD, pointer at +4678h) and +4648h (READBACK, pointer at +4680h).
  - N heaps (params +170h / +180h) of params +168h / +178h bytes each, linked in a circle of 38h-byte entries.
  - Measured totals: **UPLOAD 700 MB, READBACK 100 MB** (VS probe).
- Each heap is a **lock-free bump arena with a live count**. The state u64 at +8 holds the offset in the low bits (mask +18h =
  next power of two minus 1) and the count in the high bits (unit +20h).
  - UploadHeapBumpAlloc 62D8C0 aligns, bumps the offset and increments the count in one CAS.
  - UploadHeapFree 878B70 decrements the count and **resets the offset to 0 when the count reaches 0**.
  - Alloc walks the circle from the last heap that succeeded (+10h). A request larger than one heap, or no room anywhere, falls
    back to **TextureStagingAllocDedicated 9C2EE0, a CreateCommittedResource on the calling thread** (stutter.md §3.3).
- Consequences:
  - One long-lived allocation pins its heap, because the offset never resets. Persistent UPLOAD buffers placed here shrink the
    usable ring.
  - On no-ReBAR cards both rings sit in system memory. With ReBAR the UPLOAD ring moves to VRAM; the READBACK ring never does.

### 2.4 Transient RTs: VideoMemoryKeeper (S; answers 05-frame-graph's open question)
- A keeper is a KIDS object of type "VideoMemoryKeeper" (VideoMemoryKeeperRegisterType 244EE60). Its handler-table factory 259F5AC
  (slot 22 of 3B5B838) builds it when the owning graph asset loads, which is why it is not in the objdbs.
- The factory reads 7 params (PropTableReadParamsBatch16 2F3D08): byte size, screen-relative flag, x / y shift, two counts and the
  PixelFormat. For a screen-relative keeper it calls GetResourceAllocationInfo for a (W >> sx, H >> sy) texture of that format
  (W / H from R+960h / 964h); size = max(param, result).
- Memory comes from the GpuMem pool (VideoMemoryKeeperHeapAlloc 1896024 -> pool alloc 2F04B60) and is wrapped in a 28h-byte heap
  object (vtable 40EB360). The keeper (50h B) goes into the transient set R+1EE530h (TransientSetAddKeeper 32F1FF4, capacity +18h,
  count +1Ch).
- Resize: when W, H or the scale changed, RenderFrameBegin calls TransientSetResize 94541C(R+1EE530h, ..., W, H, scale). That calls
  345CCEC for each keeper: w = floor((W >> sx) * scale + 0.5), h likewise, then the resources are placed again.
- Per frame: activation at the first-use node index (05-frame-graph §4); 12 alias barriers + 12 discards per frame (FCAP1).

## 3. Texture and buffer creation paths (S)

| path | entry | memory | notes |
|---|---|---|---|
| textures (load / streaming) | GpuMemAllocResourceMemoryFenced 62B02C, the texture creator (callers 631AA4, GpuMemSizeTiledResourceAlloc 151140, GpuMemUploadTextureAndUnmap 62F86C) | pool placed or committed | tries the **4 KB small-resource alignment** (GetResourceAllocationInfo; accepted when alignment == 1000h); format through the table in 3.1 |
| streamed textures | reserved resources (CreateReservedResource_Backend 2F03F9C) + UpdateTileMappings | 64 KB tiles from the KtglCache page pool (`[45BA588]+61A0h`) | gpu-tiles.md; section 4 |
| render targets (objdb) | KIDS RT objects | RT/DS class pool, or committed | whole session |
| transient RTs | VideoMemoryKeeper (2.4) | pool | aliased |
| buffers | GpuBuffer {+30h staging, +58h size, +64h CPU-visible, +68h map mode, +70h/+78h range}; GpuBufferEnsureWidth 4E2F8 re-creates on growth | by the usage table | UPLOAD buffers are mapped in place |
| per-draw / immediate data | SynCtx rings (1.3) | UPLOAD buffer | retired per list |

### 3.1 KTGL PixelFormat -> DXGI (S; found: `KtglPixelFormatDxgiTable 3BEA400`)
- 28-byte entries indexed by the KTGL PixelFormat byte: {resource (typeless) format, SRV, SRV sRGB, RTV/DSV, RTV sRGB, stencil SRV,
  bits per pixel}.
- Readers: the texture creator 62B02C (`+0` = 0 means unsupported) and VideoMemoryKeeper (`+0Ch`, the RTV/DSV format).
- Decoded with DXGI numbering. Entries 0-8 repeat at 9-13 / 16-19 and the BC blocks repeat at 89-102.

| PixelFormat | DXGI (resource / SRV / RTV-DSV) | meaning |
|---|---|---|
| 0, 9 | 27 / 28 (29 sRGB) / 28 | RGBA8 |
| 1, 10 | 90 / 87 (91) / 87 | BGRA8 |
| 2, 11 | 39 / 41 / 41 | R32_FLOAT colour |
| 3, 12 / 4, 13 | 9 / 10 ; 1 / 2 | RGBA16F ; RGBA32F |
| 5, 19 | 44 / 46 / DSV 45, stencil 47 | D24S8 |
| 108, 109 | 19 / 21 / DSV 20, stencil 22 | D32S8 |
| 78, 79 | 39 / 41 / DSV 40 | D32 |
| 20, 21 / 22, 23 | 53 / 56 / DSV 55 ; 53 / 56 / 56 | D16 ; R16_UNORM |
| 6-8, 16-18, 89-91, 96-98 | 70-78 | BC1 / BC2 / BC3 (+sRGB) |
| 92, 99 / 93, 100 | 79 / 80 ; 82 / 83 | BC4 ; BC5 |
| 94, 101 / 37, 43 | 94 / 95 ; 94 / 96 | BC6H UF16 ; SF16 |
| 95, 102 | 97 / 98 (99) | BC7 |
| 15, 24 / 42, 114 | 65 ; 61 | A8 ; R8_UNORM |
| 25, 28 / 26, 29 | 85 ; 86 | B5G6R5 ; B5G5R5A1 |
| 33, 34 | 92 / 88 (93) | BGRX8 |
| 35, 36 / 105, 118 | 33 / 35 ; 33 / 34 | RG16_UNORM ; RG16F |
| 64, 66 | 24 | R10G10B10A2 |
| 65, 67 | 9 / 11 | RGBA16_UNORM |
| 70, 115 / 104, 117 | 49 ; 48 / 50 | RG8_UNORM ; RG8_UINT |
| 76, 77 | 15 / 16 | RG32F |
| 103, 116 | 27 / 30 | RGBA8_UINT |
| 106, 119 | 53 / 54 | R16F |
| 107, 120 | 26 | R11G11B10F |

- Not mapped yet: the PixelFormat enum names (the RT table, KIDS object 0x93fb3280). This table is the DXGI side.

## 4. Residency and the streaming budget (S + M, mostly from root-cause-vram-streaming.md §2)

- The DXGI budget is read in three places only: at device init (GfxInitQueryAdapterMemory 2EEBCCC: SetVideoMemoryReservation), by
  the boot "settings too heavy" dialog and by the options VRAM meter. **No pool is sized from the live budget.**
- Texture page pool (KtglCache `[45BA588]+61A0h`): 64 KB pages. Its size comes from the device create parameters and depends on the
  adapter (PC 2,342 MB, pc2 1,389 MB); it is fixed for the session.
- Request flow:
  1. TextureStreamUpdate 1E7050 runs per frame, called from MeshletUpdateSceneBuffers 4BBEC.
  2. TextureStreamTouchSlot 128EF0, TextureStreamVisitedCheck 129A28, then **TextureStreamRequestTiles 1290B8: <= 16 tiles per
     texture per frame, global cap +75A8h = 128.**
  3. The cache request ring feeds the tile-mapping job (UpdateTileMappings on the cache COPY queue, 2F110A8). At frame begin,
     CacheFlushOnGfx 2EF0938 takes what is left (ECL token 4 on DIRECT).
  4. Tile data goes through the cache upload COPY queue (2F13350) and tokens 0x95 / 0x96 on the dispatcher (the KtglCache upload
     ring, 1.1).
- Pressure: PagePoolOccupancyCheck 126C64 raises a mip bias while occupancy > 85 %. It never engaged on the measured devices.
- Residency API: GpuMemMakeResident 2F118CC (ID3D12Device vfn 110h, one object) / GpuMemEvict 2F11614.
  - Users: the page cache 491D70, the texture block allocator 127AEC / 128294, and pool heap create / destroy (2F0F5xx, 2F11534,
    2F109B0, 2EF0D20).
  - Measured: 30-37 MakeResident and 50-65 Evict per 8-minute run, at boot and loads only. There is no residency churn in gameplay.

## 5. Readback (S)
- The READBACK heap ring (100 MB, UploadHeapSuballoc with param 0) serves two users:
  - read-back maps of buffers / textures: copy GPU -> staging on the copy context, CPU wait, then map;
  - the **meshlet streaming feedback** (uRequestCmds M+370h, ~30 KB/frame, read two frames later).
- Committed READBACK buffers: timestamp results (GfxDevice+4770h), pipeline statistics (SynCtx+24E0h), occlusion (query object +28h).
- GPU atomics and writes into the READBACK heap stay in system memory even with ReBAR (root-cause-vram-streaming §0, hypothesis 3).

## 6. Staging and copy paths (S)

- **Copy-context pool** at GfxDevice+43E8h. GfxCopyCtxPoolAcquire 7F6200 waits on the pool semaphore (+18h), then takes a context.
  Each context is one COPY list over an 8-entry allocator / fence ring (CmdListRingCloseAndSubmit 5C268), on the COPY queue of
  GfxCreateCopyQueue8 2EE9EE8.
- Buffer path:
  1. GpuBufferMap 4FD84 / GpuIndexBufferMap 62CE7C take staging from UploadHeapSuballoc (or the READBACK ring for read modes).
  2. The CPU writes.
  3. GpuBufferUnmap 4FBD4 / GpuIndexBufferUnmap 62E4DC call GfxCopyCtxSubmitBufferCopiesAndWait 5C0D4: CopyBufferRegion, close,
     submit, then **GfxFencePollWorker 117D6C spins with Sleep(0) until the copy fence completes**.
  4. GpuAllocRelease frees the staging.
- Texture path:
  1. TextureMap 69A36C / TextureMapSubresource 69A494 take staging from TextureStagingAlloc 69A720 (heap ring, else dedicated).
  2. TextureUploadRect 699EC8 / TextureCopyRegion 69A1B4 do the same submit + wait.
  3. TextureStagingRelease 69A0B4 frees it, except persistent dedicated staging (tex+A8h = 1, e.g. the glyph atlas).
- Load-time uploads: GpuMemUploadTextureAndUnmap 62F86C / GpuMemMapAndUploadTexture 62ED60 call
  GfxCopyCtxSubmitTextureCopiesAndWait 5A830 (one CopyTextureRegion per subresource, footprints from 9E7744, submit + wait).
- In-frame copies on DIRECT: tokens 0x03 / 0x91 (SynCopyTextureRegion*, tracked states) and the CopyColorRenderTarget nodes.
- Cost model: every staging copy is a CPU-blocking round trip on the calling thread (the streaming jobs; [App]Game for UI and
  dynamic buffers). A full ring falls back to a committed allocation on that thread (stutter.md §3.3).

## 7. What a new backend would need from this layer

1. **Allocators to re-implement 1:1 (or replace):**
   - the fence-tagged UploadRing and DescRing, one per replay thread, tagged by that thread's list fence;
   - the per-heap bump arenas of the UPLOAD / READBACK rings;
   - the GpuMem pool (class-split placed heaps, CREATE_NOT_ZEROED, 64 KB pages) and its committed fallback.
2. **The lifetime contract (section 1):** list-fence retirement for rings, frame-end reset of the token stream, 3 frame slots for
   allocators and query read-back, single-copy transient RTs, synchronous staging. A pipelined or parallel backend must add a second
   token stream and per-thread rings. Nothing else in this layer is frame-scoped.
3. **Format and state tables:** KtglPixelFormatDxgiTable 3BEA400 (resource / SRV / RTV / DSV / stencil formats), the usage table
   3BEA260, MapModeTable 3BE29D0 and the heap-class flags 3C027E8.
4. **Transient aliasing:** keeper heaps + first-use node index + ALIASING(NULL) + Discard. A backend with its own barrier planner
   can keep the placement and replace the NULL-before barrier with explicit before/after pairs.
5. **Streaming interfaces to keep:**
   - reserved textures + UpdateTileMappings (cache COPY queue and the DIRECT frame-begin flush);
   - the page pool (it has no live-budget feedback);
   - the meshlet READBACK feedback with its 2-frame latency.
6. **Cheap wins this layer offers a new backend:**
   - asynchronous staging (retired by fence instead of a CPU spin);
   - GPU-upload heaps when ReBAR is exposed (OPTIONS16, never queried today);
   - more UPLOAD-ring heaps, to avoid the committed fallback;
   - CBVs in VRAM or as root constants. Caution: on pc2, rebar-fix-v3 / CbvMirror measured that copying them costs more than it
     saves.

## 8. Open items
- Exact byte sizes of the SynCtx rings (params +88h, +10Ch, +4Ch, +50h) and the UPLOAD heap count x size. They need one read of the
  live params blocks (the SynCtx create params, device params +168h..+180h). Statically only the 700 / 100 MB totals are known (M).
- Whether any keeper (transient) RT is used on an async-compute slot, which would be a cross-queue aliasing hazard.
- The PixelFormat enum names (RT table KIDS object 0x93fb3280), to label 3.1.
- The frame-lag bound for owner-double-buffered UPLOAD buffers (1.2 item 6).
