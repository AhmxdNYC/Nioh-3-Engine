# renderer/13-main-submit: the main-thread render-submission path, its costs, and what to cut next (2026-10-10)

Scope: everything the [App]Game (main) thread does to turn draw lists into KTGL tokens, from SceneRenderView down to
the kick into the dispatcher ring, plus the per-frame meshlet scene-buffer sync wait. It extends 07-drawlists.md (lists,
DrawObj, per-draw record), backend.md (stream, dispatcher) and 01b-replay-handlers.md (the replay side of the same tokens).
Static analysis only, from the bulk export `<re-scratch>\decomp` and the dump (no live Ghidra session). Costs come from
existing profiles. Tags: **[dec]** read in a decompile or the disassembly,
**[inf]** inferred, **[M]** measured.

Symbols: symbols.tsv blocks `# renderer/drawlists` (+85 functions, +7 fields) and `# renderer/meshlet` (+15
functions, +30 fields, 2 renames: MeshletManagerInit 32F8670, MeshletRecordSceneSetSyncPoint 5E370). Renderer core
functions named: 2311 -> 2382 (39.0 -> 40.2 %); renderer bytes named 20.5 -> 20.7 %. The prototype is on the private branch `rdraw-main`:
ASI 1.7.58-rdm1, `src\msubmit.*`.

## 0. Answer

1. **Path [dec].** Per frame the order is:
   - **Worker side.** SceneViewBuildDrawLists 57DB88 runs on a KIDS worker. It builds the lists (07 §3.1) and **sorts them
     there**, through ViewSortAllDrawLists 18D3AAC → DrawListDedupeAndSort 3B3B4 → a sort-spec table, e.g.
     DrawListSortByMaterialKey 54A94C → RadixSortRecsByKeyAtC 54AB78. The radix sort is LSD, 8 bits per pass. Above
     ParallelSortPrimitiveUnitNum (128) items it is split into KIDS child jobs. **Sorting is not on the main thread.**
   - **Main thread.** For every graph View node: RenderNodeExecutePerView 10F060 → SceneRenderView 535188 (25 walk sites)
     → DrawListWalk 39E20 (groups by DrawObj+18h, material filters, MaterialTechniqueBind 3C77F4 / Unbind 3C5D64) →
     DrawEmitSubPasses 1DC2F0 (pass-major) or DrawEmitObjectMajor 5EAD78 → DrawEmit 1E80B0 (per DrawObj and pass: packet
     loop, shadow compares, state tokens, constant and texture binds) → DrawSubmit 1D2F60 → token 6A/69/6B/6C written
     inline at KtglContext+3C40h.
   - **Kick.** Every 64 draws (CmdFlushLimit) KtglCmdStreamFlush 254110 claims a 32-byte slot in the kick ring +3BF8h,
     publishes the segment `[+3C48h, +3C40h)` and calls ReleaseSemaphore(+3BF0h). That is 36-39 kicks per frame on pc2.
     The flush spins `Sleep(0)` only when the ring is full (0 % measured on pc2).
2. **Where the main thread's time goes [M].** Two different machines tell two different stories:
   - **A PC with the card in a CPU x16 slot** (RX 9070, r5-r7 SP medians, main frame ~8.4 ms): the walk is ~35 %
     inclusive, but self time is spread thin over ~15 functions of 0.1-0.6 ms, all bound by cache misses. The largest
     pieces are per-draw state work (DrawEmit + state-commit + slot tables ≈ 1.3 ms) and constant data copied into
     the token stream (memcpy ≈ 0.47 ms).
   - **pc2** (5600X + RX 6650 XT, ReBAR on, SP CPU2): the largest non-wait items are **CPU accesses to write-combined
     upload pages that live in VRAM**: UiQuadBuild 2.1 ms, the glyph atlas 2.3 ms, GpuBufferUnmapRange's ntdll time
     1.1 ms, Memcpy 1.4 ms. pc2's card sits in a B550 chipset slot (~0.8 GB/s), so these
     numbers are inflated by the link. They are not typical mid-range figures. The ranking below is therefore in CPU ms
     per device.
3. **Ranked candidates.** See §5 for the table; this is the short version.
   - **(1) Per-draw state work, prefetch:** ≈ 1.3 ms PC / 2.1 ms Ally; 0-0.3 ms est. Earlier prefetch caves were neutral, and the skip-unchanged commit (D2) is ruled out.
   - **(2) Constant blobs copied into the stream:** 0.47 / 1.06 ms; 0.1-0.25 / 0.2-0.5 est. Hand-off to cbv-upload.
   - **(3) Meshlet frame-begin O(N) loops:** ≈ 0.6 ms PC; PP is built (off by default: paired tests showed no fps change,
     docs/BENCHMARKS.md), the rest are designs.
   - **(4) Texture-streaming touch:** 0.43 ms PC; P1E7 is built and still needs its A/B.
   - **(5) UI write-combined writes, UQS (built here) + C:** 0.26 ms PC / 4.4 ms pc2 (link-bound).
   - **(6) Meshlet scene-buffer wait:** 0 in stock, ~3.1 ms/f with FO + fo9 (§4).
4. **Built [M offline]: UQS.** UiQuadBuild now builds in a cached scratch block and does one streaming copy before Unmap.
   The harness runs the original v2.02 bytes against the caves on 20k random UI layers: every byte and register is
   equal. The bench (write-combined system memory) gives 1.4-1.6 ms → 0.07-0.1 ms for a 2048-quad layer with half
   the quads fading. Live A/B and pc2 spec: §6.

## 1. Per-frame flow (main thread) [dec]

```
KIDS worker: SceneViewBuildTask_Run 2E33A4 -> SceneViewBuildDrawLists 57DB88
    jobs 4674EC (view constant blocks View+4040/40C0/4140/41C0), 50284C (meshlet batch lists, DrawBatchListComputePoolSizes),
         828928 (skinning: SkeletonLocalToModelPose sub-jobs)
    per view: ViewSortAllDrawLists 18D3AAC -> DrawListDedupeAndSort 3B3B4 (lists 2..22, VSM +46C0/+46C8, +4468) ; JobGroupWait
main:  render graph walk -> RenderNodeExecutePerView 10F060 -> SceneRenderView 535188
    ViewSetupCameraConstantsAndTargets 532F78, RenderCtxBeginPassTarget 52FCB4 / RenderCtxSetColorTargets 1EB584
    per bucket / layer / cascade: DrawListWalk 39E20
        group = equal DrawObj+18h (DrawListGroupLength 3BC0C: binary search in mode 3)
        filters (07 §3.2) -> DrawStateSetActiveMaterial 1DAE90 -> MaterialTechniqueBind 3C77F4
        passes = Material vtbl+88h -> DrawEmitSubPasses 1DC2F0 | DrawEmitObjectMajor 5EAD78
            BeginPass vtbl+A8h ; for each DrawObj of the group: DrawEmit 1E80B0 ; EndPass vtbl+B8h
        MaterialUnbindTechnique 3C5D64
DrawEmit 1E80B0, per DrawPacket (0x40 B):
    tok 3D IB / 3B layout / 3C VB per slot (shadow compares KtglContext+2550/2558/2780..)
    state word -> tok 14 / 13 / 17 + KtglSetDepthBias 45C3C4 (tok 18) / 85
    DrawBindTextureTable 1FE6F0 (TextureTableEquals 1193B40 first), DrawStateApplyUvTransformParams 1CC2E4,
    DrawBindMaterialParams 1E6664 -> ConstBlockSetFloat 1DD6D0 / ConstBlockSetVec4 D6ABC (dirty-tracked slots)
    DrawStateFlushDirty 1E0B64 / ShaderConstFlush 1DF880 -> ShaderConstWriteStream 1D7340 (constant blobs inline: Memcpy)
    StageBindingsApply2568 1D6E64 -> StageSlotTableUpdate2568 1D84A0 (and 2578 1D8720 / 6780 1D8220 / 5F20 1D7EB0)
    DrawSubmit 1D2F60
DrawSubmit 1D2F60:
    DrawPacket+38h sub-object -> its own pass loop (vtbl +130h..+168h), no draw token
    highlight mode 1 / 2: KtglSetAlphaTest0B 1DE1E8, KtglSetDepthFlag0E 1DE290, KtglSetColorMaskSlot 1F7270, const 24h
    packet+28h != 0BBBB55h -> KtglSetRtBlendState09 1DDCD4
    material "Meshlet" -> MeshletDrawRecord 1D2AAC | DrawMeshletPathAux 32F5248 (ExecuteIndirect, package 08)
    indexed (packet+0Ch bit 31): instances = packet+24h
        0 -> inline tok 6A {topo<<16|6Ah, +14h, +18h, +1Ch, +20h} (or KtglEmitDrawIndexed6A 1D42D8)
        >0, count 0 -> KtglEmitDrawIndexedWholeIb6B 8ABA8C ; >0 -> KtglEmitDrawIndexedInstanced6C 2F5EA88
    non-indexed -> KtglEmitDraw69 1D2870
    ++KtglContext+3C78h (DrawCount), ++3C74h (CmdCountSinceFlush); >= CmdFlushLimit 3C70h -> KtglCmdStreamFlush 254110
KtglCmdStreamFlush 254110:
    free = +3C38h (end) - +3C40h < +3C2Ch -> 828E74 + 66424A (stream wrap; CmdStreamWrapCheck 418E74 path)
    ring full (+3C04h == +3C08h + +3C00h) -> Sleep(0) spin (timed by KtglTimerAccumulate)
    slot = (atomic +3C04h++) % +3C00h; KickRec {ctx+3BE0h+10h, +3C48h, +3C40h, final} ; ++3C10h ; ReleaseSemaphore(+3BF0h)
    +3C48h = +3C40h ; +3C74h = 0
```

A token write reloads and stores the stream pointer +3C40h after every dword. The compiler cannot keep it in a register
because of aliasing. That costs 5 load/store pairs per 6A, about 1 ns each, which is negligible.

## 2. Sort (07 §6 Q1) [dec]

- **Where:** DrawListDedupeAndSort 3B3B4 runs per non-empty list. It is called from ViewSortAllDrawLists 18D3AAC (inline in
  57DB88, once per view in the `scene+3BCA0h` mask). It is also called from 33E59A0 / 33E5AF4, which are not exported;
  those are probably the per-light lists under View+4028h and the extra renderers [inf]. Main lists 0/1 are not reached
  by 18D3AAC. Their sort site is not found yet.
- **What 3B3B4 does:**
  - Dedupes by DrawObj record id (+1Ch or +14h) with a bitset, guarded by a CAS on `L+0Ch`.
  - Runs up to 20 sort passes taken from a u16 sort-spec array (the ViewParam sort method), through a function table
    (33B63B4, 33B4434, 33B47B4, 54A94C, 33B4CF4 … 33B7374, 3CA00).
  - Writes the result back to `L+8` and the count.
- **Material key sort:** DrawListSortByMaterialKey 54A94C writes `DrawRec+0Ch` = the first material id of DrawObj+18h,
  then RadixSortRecsByKeyAtC 54AB78 runs. This is why list mode 3 is "contiguous by +18h" and DrawListGroupLength can
  binary-search it.
- **Cost:** none on the main thread. It runs before the main thread consumes the lists, inside SceneViewBuildTask's job
  group. **A faster sort cannot shorten the main thread.**

## 3. Data structures (new in this package)

**KtglContext stream / kick fields** (extends backend.md):

| off | meaning |
|---|---|
| +3BF0 | KickSemaphore: one release per kick |
| +3BF8 | KickRing: array of 32-B KickRec |
| +3C00 | DispatchRingCap |
| +3C04 / +3C08 | KickWriteIndex (atomic) / read index |
| +3C10 | pending kicks (atomic ++) |
| +3C2C | minimum free bytes before a wrap |
| +3C38 | stream end |
| +3C40 | CmdStreamPtr |
| +3C48 | CmdSegmentStart |
| +3C68 | last kicked end |
| +3C70 | CmdFlushLimit (64) |
| +3C74 | CmdCountSinceFlush |
| +3C78 | DrawCount |
| +3C7C | kicks |

**KickRec** (32 B): `{u64 ctx+3BE0h+10h, u8* segStart, u8* segEnd, u32 final (flush(ctx, 1) = frame end), u32 0}`.

**UiLayer** (the object UiQuadBuild 1CD690 builds):

| off | meaning |
|---|---|
| +28 / +60 | element list head / sentinel. Node: +8 next, +10 element (vtbl +8 type, +40 alpha), +18 producer (vtbl 0 = append quads), +24 vertex count |
| +100 | quad-group source (vtbl +18 count, +8 get(i) → {u32, u32 quads}) |
| +1C0 | producer context {layer, &node+10h} |
| +1E0 | buffer pool handle (UiBufferPoolAcquire 9261DC → {VB, IB}) |
| +358 / +360 | VB / IB write cursor (mapped pointers) |
| +378 / +380 | VB / IB objects |
| +394 | capacity |
| +39C | bytes appended |

Vertex = 0x2C bytes (color RGBA at +24h, so alpha is at +27h); quad = 4 vertices = 0xB0 bytes. The second pass runs when
`[[4BD0AC0]+368h]` is set: index float at +28h of each vertex, and `alpha *= element alpha` when that is not 1.0.

## 4. Meshlet scene-buffer sync wait (MeshletFrameBegin 5D214 / MeshletUpdateSceneBuffers 4BBEC)

Source: a static pass over the export and the dump. M = MeshletManager = MeshletSys+169DD0h, so the two sets are M+1E0h /
M+260h, CurSceneSet = M+2E0h and SceneSetParity = M+2E8h.

**Path [dec]:**
- **Swap.** MeshletFrameBegin 5D214 at 5D2F9..5D321, only when its 4th argument isSecondary is 0:
  `p = (p-1)&1; [M+2E0h] = M+1E0h + p*80h`.
- **Wait.** MeshletUpdateSceneBuffers 4BBEC at 4C674..4C68B:
  `rsi = [M+2E0h]; if ([rsi+60h]) SyncPointWait([M+1C8h], [rsi+60h])`. 5D41C and 4C690 are return addresses, not
  functions.
- **Record.** At frame end: RenderWaitForFrame → MeshletRequestLoadStep 5DC18 records MGLRC (4B104), then
  MeshletRecordSceneSetSyncPoint 5E370 stores `[M+2E0h]+60h = SyncPointRecord(M+1C8h)` (5E3A0). It skips the store when
  M+1ACh bit 40000000h is set. **Frame F therefore waits for the point recorded at the end of F-2, which is the same set.**
- **SyncPointRecord 5F17C.** Emits tok 72h, then takes SyncTableAllocEntry 418DD0 from the table
  `[d+3C20h + (d+3C1Ch % d+3C18h)*18h]`. The handle is `(idx|80000000h)<<32 | tableFrameId`. Then it emits tok A8h
  {frame, idx}. The replay thread fills the slot's list fence when it reaches A8h.
- **SyncPointWait 3D2B90**, in three steps:
  1. KtglWaitQueryOrKick 3D2C64 → SyncPointIsComplete 3D2CE8. This returns at once if the handle is 0, if the table was
     recycled, or if `slotFence <= GetCompletedValue(d+6D8h)`. Otherwise it kicks (KtglCmdStreamFlush(d, 1)).
  2. SyncPointWaitReplayed 7D99FC spins on Sleep(0) while the stream marker AAAAAAAAh is unreplayed. This time goes to
     d+3CD0h, which BKS reports as "handle wait".
  3. GfxWaitFenceValue polls the list fence with Sleep(0). This time goes to d+3CE8h, which BKS reports as "sync-point
     GPU wait". **That counter is the 3.1 ms/f.**

**Set (0x80 B, created in MeshletManagerInit 32F8670):**

| off | field | memory | accessed by |
|---|---|---|---|
| +00 | flags | CPU | |
| +04 | ReqCount | CPU | |
| +08 / +10 | request lists A / B | GPU-written | MGLRC writes them; MeshletRequestJob reads them 2 frames later |
| +18 | instance transforms (nInst*48 B) | UPLOAD | CPU writes |
| +20 / +28 | prefix / object table A | UPLOAD | CPU writes |
| +30 | instance offsets | UPLOAD | CPU writes |
| +38 / +40 | tables B | UPLOAD | |
| +48 | 32 UPLOAD slot buffers (MeshletAddSlotBuffers 32F21E0) | UPLOAD | |
| +50 | dispatch offsets | UPLOAD | |
| +58 | payload→MRLOD | UPLOAD | |
| +60 | sync-point handle | CPU | |
| +68 | LOD clipmap output | GPU | |
| +70 | LOD reset counter | GPU-written | CPU reads and zeroes it 2 frames later (4C703) |
| +78 | mode-7 readback | GPU | |

The UPLOAD buffers come from GpuBufferCreate 82A690 (usage 2) and are mapped in place by GpuBufferMapMode 4DF64 /
GpuBufferMapWriteSized 86F5DC. They are not renamed. **That in-place reuse is why the wait exists.** SBM (rebar-fix-v3.md)
mirrors these same buffers.

**Readers [dec]:**
- of [M+2E0h]: 4BBEC; 5E370; MGLRC; MeshletRequestJob 4A305; MeshletGetSlotBuffer 7E1320; MPC 5301FC; LOD 531948;
  MIC 53FD3C; MeshletResolveLodMainView 904610; the clipmap LOD resolve; **DrawEmit 1E83F1 (+40h)**; 32F546A.
- of the parity bit: the 2-slot arrays MeshletRes::ReqSlot obj+C8h[p] (MGLRC 4B456, 4B820, 4B0B4, 48FA6, 4A976 / 4A9F9,
  cold 1928D4D / 1928F48; obj+D0h follows directly), MeshletRes::ParityDesc obj+80h[p] (4AF28, cold 19287AC / 1928B1B)
  and MeshletPart::ParityShadow +90h[p] (4D7B4).
- Hard-coded 2s: 32F87D8, 3302AD8, 4C6A1, 32F221F, 19B2581 / 19B26F7.
- No token payload carries the set or the parity.

**Options:**
- **Skip the wait when the fence has passed:** the engine already does this (3D2CE8).
- **Drop the kick + spin:** saves about 0 ms, because the 3.1 ms is in the GPU-fence counter.
- **Drop the wait itself:** **unsafe.** The CPU would rewrite the instance transforms while frame F-2's GPU may still read
  them, and it would read half-written request lists.
- **A. Three sets (keeps the protocol):** hard, an estimated 3-6 days. It needs:
  - a mod-owned 3rd set cloned with the same creators;
  - a swap cave at 5D2F9 (41 B);
  - every 2-slot array widened to a side table at ~12 sites; the 3rd ParityDesc / part-shadow allocations are not found
    yet;
  - the loops at 4C853 / 4C8E5 / 32F221F / 4A4D54 / 3302AD8;
  - SyncTableCount >= 4, or a recycled table silently skips the wait.

  **Same-picture rating: medium.** Every GPU→CPU readback (streaming requests, the LOD flag, mode 7) arrives one frame
  later, so streaming and LOD timing is not bit-exact.
- **B. Cheap partial (recommended first):** when the sync point is not complete yet, run TextureStreamUpdate 1E7050
  (~0.4-1 ms/f) *before* the wait (cave at 4C67B, 21 B). Then skip its normal call at 4CADE (`E8 6D A5 19 00`). Only the
  order changes, so the picture is the same. To check first: 1E7050 must not read anything written between 4C67B and
  4CADE (the +70h readback, M+1ACh flags 400h/2000h, part bit 20h, part shadows). It hides up to ~1 ms of the 3.1 ms
  wait.
- **C.** Replace the Sleep(0) poll with an event wait (call 3D2C50). This frees a core and gives no latency gain unless
  cores are short.

**Status:** the wait is 0 in stock play on every device measured so far (pc2 FOV1, BKS "fov:" line). It only appears
under frame overlap (experimental, off by default), which faulted on pc2 (FO4). Option B belongs with the frame-overlap
work (frame-overlap.md). Nothing was built here.

## 5. Ranked candidates (main-thread CPU ms per frame)

Sources:
- PC = RX 9070 box, card in a CPU x16 slot: pc-cpu-next.md §2, r5-r7 SP medians, main frame ~8.4 ms.
- Ally = ally-drawprep-2 / 07 §4 (216 windows, 34 fps).
- pc2 = SP CPU2 (cpu-bound-midrange.md), 25.9 ms frames. **The card is in a chipset slot (~0.8 GB/s)**, so pc2's
  write-combined/VRAM items are link-inflated. They are listed for completeness, not as typical.

| # | candidate | what the main thread does now | PC ms | Ally ms | pc2 ms (SP self) | fix (same picture) | est. saving PC / Ally | status |
|---|---|---|--:|--:|--:|---|---|---|
| 1 | per-draw state work: cache misses on DrawObj slots, packets, material, slot tables | DrawEmit self 0.57; StageSlotTableUpdate family 0.46 (1D8720 0.16, 1D84A0 0.15, 1D8220 0.08, 1D7EB0 0.07); DrawStateCommit / FlushDirty / BindMaterialParams ~0.3 | ≈1.3 | ≈2.1 (DrawEmit 0.99 + state-commit 1.1) | ≈1.6 (DrawEmit 0.52, slot tables 0.42, ShaderConstWriteStream 0.14, BindMaterialParams 0.13, FlushDirty 0.11, Commit 0.10, BindResourceSlot 0.08 …) | software prefetch of item i+1's DrawObj (+18h material, +58h packets, +60h states) and of its first packet / material lines in DrawEmitSubPasses 1DC2F0 (`**items[i]`); loads only, so no semantic risk. ally-drawprep-2 G2 is the same idea: est. 0-0.2 ms, and the round-9 prefetch caves were neutral or negative. Skip-unchanged stage commits (D2) are already ruled out (the applies skip at set level; 1D8720 *is* the per-slot compare) | 0-0.3 / 0-0.2 (E) | design; research |
| 2 | constant blobs copied inline into the token stream (~10 MB/frame on pc2, BKS streamKB/f) | Memcpy B94250 + MemcpyErms B94230 from ShaderConstWriteStream 1D7340 | 0.47 | 1.06 | 1.37 | emit-side dedupe: skip the copy when a slot's blob equals the last one sent (the replay side already skips the same-list no-op: CBN, 20 % of uploads). ally-drawprep-2 M said NO because the stream changes (token 66h copies the blob per call). CBN's harness shows how to prove the GPU-visible result is equal anyway | 0.1-0.25 / 0.2-0.5 (E) | **see cbv-upload.md** |
| 3 | meshlet frame begin, O(N) per frame | MeshletCheckRequestLoad 4AA34 0.21, PagePoolCountUsed 2523FC 0.21, 419A8, 4BBEC part loop, 5DF70 | ≈0.6 | - | 1.0 | PP (built, count cache); the others: meshlet-mainthread.md designs | 0.2 (PP, E) | PP built, off by default, no measured fps change (docs/BENCHMARKS.md); rest open |
| 4 | texture-streaming touch per material texture | TextureStreamUpdate 1E7050 0.18 + 128EF0 0.16 + 1290B8 0.09 | 0.43 | - | 0.35 | P1E7 prefetch | 0.05-0.2 | built, no valid A/B yet |
| 5 | UI: WC upload pages written / read by the CPU | UiQuadBuild 1CD690 0.12; WcUploadZeroFill 61B1C 0.14 (W1B) + GlyphCacheAllocSlot 62000 | 0.26 | 0.15-0.25 (W1B est.) | 2.1 + 2.3 (link) | **UQS** (built here, §6) + W1B (built) + C (atlas not wiped every frame, ui-mainthread.md) | 0.1 + 0.1 / more with fading UI | **UQS built**; C designed |
| 6 | technique bind / unbind per group | MaterialTechniqueBind 3C77F4 6.7 % incl. on the PC | ≈0.55 incl. | 0.10 (S2F) | 0.08 (S2F + self) | skip re-bind when consecutive groups share technique + permutation (needs 02-materials proof) | 0.1-0.2 (E) | open |
| 7 | meshlet scene-buffer sync wait | wait for the GPU fence of the set's previous use | 0 stock | 0 | 0 stock; **3.1 ms/f with FO + fo9** (PC, 8 ms lag emulator) | §4 B: run TextureStreamUpdate before the wait when the point is not complete (order only); §4 A: 3 sets (hard, not bit-exact timing) | 0 stock / B up to ~1 ms, A up to 3 ms with FO | frame-overlap area: design in §4 |
| 8 | UI cell-grid broadcast (UiCellGridBroadcast 12AA5C + vfunc 12AB0C) | per frame | <0.1 (not in the PC top) | - | 1.6 | needs RE of the vfunc (probably a hit-test per cell) | ? | open |
| - | list sort | KIDS worker (§2) | 0 main | 0 | 0 | - | - | not a main-thread item |

**Reading:** on a properly slotted PC no single same-picture item on the main submission path is worth more than about
0.5 ms. Rows 1 + 2 + 3 together are ~2.4 ms PC / ~3+ ms Ally of cache-miss and copy work.

pc2's main thread after CBN is ~19-20 ms of work. About 8 ms of that is link-bound write-combined/VRAM traffic (row 5,
Memcpy, GpuBufferUnmapRange's ntdll time). In a CPU x16 slot that part should mostly vanish (E). Then pc2 would be
replay- or GPU-bound, not main-bound. **Re-profile pc2 in a CPU x16 slot before any further main-thread work for it.**

## 6. Prototype UQS (branch rdraw-main, ASI 1.7.58-rdm1)

Design and code: `mod\Nioh3EngineFix\src\msubmit.{h,cpp,asm}`, ini `[Patches] UQS = 0` (1 = on, 2 = installed + off),
`[Tuning] UQS_CapMB = 16`, live `Tuning.UQS = 0/1`, a 30 s stats line `stats/s msubmit: UQS ...`. patches.json group
UQS has 4 check sites plus FNV deps over the whole function; `tests\test_patterns.py` reports ALL 249 SITES OK.

- **Entry 1CD74F** (`mov rbp,[rsi]; test; je`, 12 B): when the switch is on, the map size `[VB+58h]` fits the scratch,
  and a `lock cmpxchg` busy flag is free, then `[obj+358h]` = rbp = scratch + (real & 0FFFFh). The low 16 address bits
  are preserved, so any alignment arithmetic is unchanged.
- **Exits 1CD942 / 19602C8 / 19602DC** (`mov rcx,[rdi+378h]` before each GpuBufferUnmap): if this build owns the scratch,
  copy `max(cursor, rbp) - scratch` bytes, clamped to the map size, with `rep movsb` + sfence. Then rebase the cursor,
  release, and run the displaced instruction.
- **Fallbacks** to the stock path: switch off, map > cap, busy (another thread or a nested build), exit of a build that
  did not take the scratch.
- **Same picture:** the GPU reads only what Unmap publishes. Every byte the producers and pass 2 wrote is copied before
  Unmap. Bytes nobody wrote were undefined before too.

**Offline proof [M]** (`tests\run_msubmit_harness.py`): the real UiQuadBuild bytes 1CD690..1CD987 + 19602C8..19602FE run at
their RVAs in a reserved region, with stubs for the 7 callees. 20,000 random layers cover:
- quads and skipped elements, fading alphas, pass 2 on and off, empty groups;
- producer failure (exit 3), IB map failure (exit 2), VB map failure, no VB.

Each layer runs with the original bytes and with the caves (on; off; scratch too small; scratch busy). The VB at Unmap,
the VB afterwards, the 0x400-B layer object, the return value, the unmap counts, the producers' cursor low bits and all
non-volatile GPRs are equal: **ALL PASSED** (19,041 scratch builds, 1,514 producer-failure and 625 IB-failure exits,
12,738 with pass 2).

Mutation check: halving the copy fails 528 checks. Not rebasing the cursor is unobservable, because the game zeroes it
after Unmap.

Bench (`--bench`, PAGE_WRITECOMBINE system memory, 2048 quads with half of them fading, build call only):
**original 1425-1588 µs, UQS 68-97 µs** per build. WC system memory is what an upload page is without ReBAR, and on APUs.
A VRAM page behind PCIe is slower again.

PC live A/B: **no in-game result yet.** An ABBA x4 run (SP on) installed the patch and ran stably with the switch off
(~100-127 builds/s, max map 1 MB, so the 16 MB scratch fits), but its report was never analysed. Next: the fps / GPU
no-harm report and the UiQuadBuild main-thread share per arm (`sp_by_arm.py <log> UQS 1CD690`).

pc2 (planned, not run):
- ASI 1.7.60-fo9-rdm1 (branch `rdraw-main-fo9` 15fab3c: UQS on top of the fo-efxlag branch with CBN/CBT).
- Spec: FO off, CBN = CBT = 1, UQS = 2, SP on, ABBA x4 Tuning.UQS 0/1. Report fps, CPUBusy, sp_by_arm UiQuadBuild
  + drain share, and the BKS drain.
- pc2's chipset slot inflates every WC/VRAM item, so this belongs in a re-baseline after the card moves to a CPU x16 slot.
  FO itself faulted on pc2 (FO4).

## 7. Next / hand-offs
- **Prototype 2: prefetch in DrawEmitSubPasses 1DC2F0** (row 1). Site: the `**items[i]` load in the per-object loop.
  Prefetch DrawObj(i+1) lines 0/1/2 and, one iteration later, its packet array and material.
- **cbv-upload work (cbv-upload.md):** an emit-side constant dedupe (row 2) would save the main memcpy and the replay copy together.
- **frame-overlap work (frame-overlap.md):** the 3-set design for the scene-buffer wait (§4).
- **pc2:** re-profile the main thread with the card in a CPU x16 slot (SP Focus, the CPU2 recipe).
- Open: the sort site of main lists 0/1; 33E59A0 / 33E5AF4; 15 DRAW functions still unnamed.

## 8. Files
- `research/renderer/13-main-submit.md` (this note); 07-drawlists.md §6 Q1 points here.
- symbols.tsv blocks `# renderer/drawlists`, `# renderer/meshlet`.
- Private branch `rdraw-main`: `mod/Nioh3EngineFix/src/msubmit.{h,cpp,asm}`, `tests/msubmit_harness.{cpp,asm}`,
  `tests/run_msubmit_harness.py`, `tools/testmatrix/sp_by_arm.py`, patches.json group UQS, live_keys.json UQS.
