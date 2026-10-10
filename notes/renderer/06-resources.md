# renderer/06-resources: GPU memory, rings, heaps, transient RTs, staging, residency (package 06, 2026-10-10)

Scope: package 06 of `renderer/00-map.md` (RESOURCE rows of `re/scripts_renderer/partition.tsv`). Static work only (decompile
export `D:\nioh3-re\decomp`, `decomp_pool.cmd`, the dump's data). No game launched. Tags: **S** static, **M** measured (earlier
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
| per-frame CPU pools of the scene side | DrawRec pool (07-drawlists), meshlet request buffers (cleared at MeshletFrameBegin 5C708) | per frame | - | reset at frame begin by the main thread | CPU only, but tokens may carry pointers into them (backend.md §6 audit list) |

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

