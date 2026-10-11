# renderer/01b: the replay side of the back end, handler by handler (tokens -> D3D12), with per-call waste

2026-10-10, package 01b (follows backend.md section 8 and replay-profile.md). Nioh3.exe Steam v2.02, RVAs (image base
0x7FF74F410000). Source: the bulk C export `<re-scratch>\decomp` plus capstone disassembly of `re\Nioh3.dump.exe` for the
functions the export does not have (no Ghidra run). Tags: **S** static (read here), **M** measured (RPF tables: PC =
replay-profile.md section 3, pc2 = `captures/pc2/rpf1/rpf_block_plain.txt`), **E** estimate.
Handler signature everywhere: `void h(SynCtx* c, u8** cur)`; header u32 = `op | arg << 16`.

## 0. Priority: the per-draw path of 6A SynTokDrawIndexed / 69 SynTokDraw (pc2: 54 % + 9 % of the replay thread)

pc2 (5600X, RX 6650 XT, PCIe 4.0 x8, ReBAR ON) M: 6A 1095 calls/f, self 11.18 ms/f = CPU 6.20 + D3D12 4.98, 10.2 us/call;
inside it CopyDescriptorsSimple 1.59 ms (x2350, 675 ns), CreateConstantBufferView 1.45 ms (x1875, 774 ns). PC (7600X,
RX 9070): 6A CPU 0.80 ms for 1880 draws = 0.43 us/draw; pc2 = 5.7 us/draw, 13x for a CPU ~1.3x slower per thread.

### 0.1 The path (S, every address read)

```
6A SynTokDrawIndexed 1167D4   payload 20 B: {hdr: arg = topology, u32 unused, u32 unused, u32 StartIndex, u32 IndexCount}
69 SynTokDraw        6BE12C   payload 12 B: {hdr: arg = topology, u32 StartVertex, u32 VertexCount}
 -> SynFlushGfxStateForDraw 1180D0 (c, topology)            [calls 1181B9 functor, 1182BB OMSetRenderTargets, 118344 RSSetViewports,
    |                                                        1183BB RSSetScissorRects, 118443/118519/118597 ResourceBarrier x1 each]
    | builds stage-program array {c+48h VS, +68h, +70h, +78h, +58h PS, +88h, +90h} (null -> c+2298h dummy)
    | loop: functor SynPsoFlushFunctorVtbl 3C332A8 -> SynFlushPsoAndBindings 118604, repeated while c+5C0h (list
    |       generation) changed under it (a ring-exhaustion submit inside cuts the list and everything is re-flushed)
    |   SynFlushCbvTables 118DA8  for stage < c+AF0h with CbvDirtyMask c+B70h[stage] != 0:
    |     SynUploadCbvTable 118E28 (c, &rootIdx c+AF4h, stage, layout, &fenceCache):
    |       for each CB i of layout (+38h: {u32 size, u32 slot} x +40h) whose slot bit is dirty:
    |         size256 = (size + 0xFF) & ~0xFF                    <- the FULL declared cbuffer, not the bytes written
    |         UploadRingAllocate 1190C0 (c+B28h, size256, ListStamp c+6D0h, completed fence)  [118EE8]
    |           (fail -> CmdListRingSubmitIfBehind 945664 + SynSubmitAndWaitFence 945610 + SynRefreshCompletedFence 9C9EF0, retry)
    |         Memcpy 0xB94250 (ring CPU ptr, shadow c+BB0h[stage*14+slot], size)   [118F4D]  <== CPU writes into the UPLOAD ring
    |         dev->CreateConstantBufferView({ring GPU VA, size256}, staging[stage*14+slot])  [118F83, vfn +88h]
    |         c+F30h[stage*14+slot] = size
    |       ShaderVisibleDescRingAlloc 1192B0 (c+980h, n = layout+44h, ...)  [118FD5]  (writes n u64 fence stamps into ring+30h[])
    |       dev->CopyDescriptorsSimple(n, ring slot, staging[stage*14], CBV_SRV_UAV)  [119069, vfn +C0h]  <== whole CBV table, n descs
    |       list->SetGraphicsRootDescriptorTable(rootIdx + stage, ring GPU handle)  [11908D, vfn +100h]
    |   SynFlushSrvTables 118C10   for stage with SrvTableDirty c+10F0h bit: ShaderVisibleDescRingAlloc(c+9D0h, n = layout+48h)
    |     [118CA2], CopyDescriptorsSimple(n, ..., staging[0x70 + stage*0x30]) [118D43] <== whole SRV table (up to 48),
    |     SetGraphicsRootDescriptorTable(c+AF8h + stage) [118D62]
    |   SynFlushGfxDescTableIfDirty 118BA0 -> 871ACC (UAV table, c+1D20h bit 0)
    |   SynFlushGfxDescTable 1188AC (samplers, c+1E38h): Memcpy of the 8-byte-per-slot sampler keys to the stack, PsoKeyHash,
    |     DescTableCache_LookupCopy 4940D8 -> hit: only SetGraphicsRootDescriptorTable; miss: ring alloc + CopyDescriptorsSimple
    |     (the ONLY table path with a content cache)
    |   topology != c+854h -> IASetPrimitiveTopology [1186AD, vfn +A0h], may set PSO dirty
    |   c+B24h & 10h -> SynUpdateRtWriteMaskDirty 11A218 (walks 32 VB-slot strides c+484h)
    |   c+B24h & 20h -> PsoGetOrCreateGfx 119670: PsoKeyHash 1199A4 (CRC32 over 128 B), EnterCriticalSection([c+818h]+10h),
    |     PsoCacheFindGfx 11976C (B-tree, copies the 0x90-byte record out), miss -> CreateGfxPso 9290D8 synchronously,
    |     LeaveCriticalSection; SetPipelineState [118731, vfn +C8h]
    | then if c+B24h & 0Fh (RT / viewport / scissor dirty): pass record c+18h[c+24h] (0x268 B): per RT a 1-barrier
    |   ResourceBarrier to RENDER_TARGET, DSV to DEPTH_WRITE/READ, OMSetRenderTargets, viewports (int -> float per call), scissors
 -> list->DrawIndexedInstanced(IndexCount, 1, StartIndex, 0, 0) [116832, vfn +68h] / DrawInstanced [6BE185, vfn +60h]; c+548h++
```

### 0.2 Which loads/stores touch GPU-visible or write-combined memory (S)

| site | memory | access | per draw | verdict |
|---|---|---|---|---|
| **Memcpy 118F4D** dst = CB upload ring c+B28h (SynUploadRingCreate 2EF3310: persistently mapped **UPLOAD** buffer) | UPLOAD heap: with ReBAR ON the driver puts it in **BAR VRAM** (rebar-fix-v3.md: DXGI non-local 919 -> 212 MB on pc2) | **write-only**, size = full declared cbuffer rounded to 256 B, regular AVX `vmovdqu` stores (MSVC memcpy: non-temporal only above 1.5 MB; it also stores the tail first and overlaps the last 32 B) | one per dirty CB slot (1875 in 6A on pc2 = 1.7 per draw; ~2.9 KB average per view by rebar-fix-v3.md = **~5.4 MB/frame across PCIe x8**) | **prime suspect for the 6.2 ms CPU**: 5.4 MB in ~6 ms = ~0.9 GB/s, the WC-over-PCIe store rate already seen on pc2 (rebar-fix-v3.md: ~0.45 GB/s copy bucket). Explains why the per-draw ratio (13x) is a bus ratio (PCIe 5.0 x16 on the PC vs 4.0 x8), not a CPU ratio |
| source of that Memcpy: CB shadow c+BB0h[stage*14+slot] (64 KB pages from allocator [c+10h] vfn+38h, SynEnsureCbShadowPage A397D8) | engine heap (inferred: tok 66, which only writes the shadow, is 316 ns on the PC and 455 ns on pc2 = CPU ratio, no bus ratio) | read | same | cached reads; not the problem |
| CreateConstantBufferView 118F83 dst = staging heap c+920h | **CPU-only** descriptor heap (SynCtxCreateSignaturesAndQueries 2EFEA14 line `GfxInitCreateDescriptorHeapWrap(c+920h, dev, type 0, shaderVisible 0)`, 533 descriptors) | driver write | 1.7 | the 774 ns inside 6A vs 409 ns elsewhere is most likely the **write-combining drain of the preceding Memcpy** being charged to the first serialising instruction inside the driver (E); the call itself writes system memory |
| CopyDescriptorsSimple 119069 / 118D43 / 118B28 dst = shader-visible ring heaps c+980h / c+9D0h / c+AA8h | shader-visible CBV/SRV/UAV and sampler heaps (on AMD: host-visible VRAM) | driver copy: reads the CPU staging heap, **writes VRAM through the BAR** | whole tables: CBV n = layout+44h (<= 14), SRV n = layout+48h (<= 48) per dirty stage | 675 ns per call inside 6A on pc2 (2350/f): bytes written across PCIe again (n x 32 B per table) |
| ShaderVisibleDescRingAlloc 1192B0 stamp loop | engine array ring+30h | write n u64 | per table | cheap, cached |
| UploadRingAllocate 1190C0 node walk | engine node array (28h B nodes) | read/write | per CB | cheap |
| nothing in the path **reads** a mapped UPLOAD or descriptor-heap address (S) | | | | no uncached read found; the pathology is stores into BAR memory |

So the 5.7 us/draw on pc2 is, on this reading, **bytes pushed across PCIe x8 per draw** (constants ~2.9 KB per dirty CB plus
descriptor tables), all as CPU stores into BAR-mapped memory, plus the serialisation that drains them inside the next driver
call. The PC does the same work at PCIe 5.0 x16.

**Cheapest confirmation (no new code):** pc2, ReBAR ON, CPU-bound Continue spot, RPF on, A/B `Profiler.VSForceSysUpload = 0/2`
(2 moves the UPLOAD heaps to system memory: rdna2-meshlet-slowdown.md). Prediction if right: 6A CPU per draw falls from
5.7 us to ~0.5-1 us and CreateConstantBufferView inside 6A from 774 ns to ~the 409 ns average; GPU time rises (per-draw CBVs
read over PCIe again, ~4 ms on pc2 by rebar-fix-v3.md), so it is a diagnostic, not the fix.

### 0.3 Fix candidates for this path (same picture), ranked by likely pc2 ms

| # | fix | where | same-picture argument | pc2 estimate (E) |
|---|---|---|---|---|
| F1 | **Skip no-op constant writes**: in the CB-write tokens (66 SynTokSetCbData / 60-67 SynSetCbDataStage 698F58, 2B / 37 / 39, 2C / 38 / 3A + 2E-36 SynUploadConstantsToRing 1158A0, 2D-35 SynCbShadowWriteStage 1159F0) compare the incoming bytes with the shadow and set the CbvDirtyMask bit only if they differ. A slot that is not dirty is not re-uploaded, gets no new CBV, and if no slot of the stage is dirty the table copy and SetGraphicsRootDescriptorTable are skipped too (existing logic in 118DA8) | 9 token bodies (memcmp before Memcpy); one shared helper | the GPU reads byte-identical constants from the previous ring copy, which stays valid: the ring is fenced per list (ListStamp) and the old CBV still sits in the staging table. **Caveat to verify:** a list cut (new list) must still re-set root tables. SynBeginList 49349C sets c+B24h = 7Fh and default root tables (c+B08h..B1Ch) but does not touch c+B70h / c+10F0h, so the re-dirtying after a cut happens elsewhere (candidates 9C9EF0 SynRefreshCompletedFence / the close callback 490E50; open): F1 must only drop a dirty bit whose slot was uploaded in the current list | depends on the redundancy ratio, unknown. **Count it first** (1 counter in the 9 bodies: writes, no-op writes, no-op bytes). Every no-op upload removed saves ~2.9 KB of BAR writes + 1 CBV + part of a table copy: at 50 % no-ops ~3-4 ms/f |
| F2 | **Upload only the bytes the shader can read**: copy min(size256, readExtent(shader, slot)) instead of the declared size | 118F4D size argument (r8) | needs, per shader object, the highest cbuffer offset its bytecode reads (offline DXBC/DXIL reflection over the shader cache); bytes past it are never read, so their ring content is irrelevant. CBV size must stay size256 (unchanged) | rebar-fix-v3.md: the 2.9 KB average is "mostly padding". If the read extent averages ~1 KB, ~2/3 of the 6A CPU: **~3-4 ms/f** |
| F3 | **Streaming copy into BAR**: replace the Memcpy at 118F4D (and 4959EC compute, 45B2E4 SynFlushUploadRingBinds, 7F6A64) with an aligned 64-byte `vmovntdq` loop (ring blocks are 256-aligned, sizes multiples of 256) and one `sfence` per draw | call-site patch | identical bytes, identical order of visibility to the GPU (the list is not submitted before the sfence) | removes the overlapped / out-of-order partial-line stores of MSVC memcpy; on WC over PCIe usually 1.5-3x store throughput: **~1.5-3 ms/f** of the 6.2 (E, harness-measurable on the PC with a mapped UPLOAD buffer) |
| F4 | **SRV/CBV table content cache** (like the sampler path 1188AC + DescTableCache 4940D8): key = hash of the stage's staging slots (BoundSrv c+1118h[stage*30h..], or the CBV GPU VAs); hit = reuse the shader-visible ring copy (still fenced in this list) and only SetGraphicsRootDescriptorTable | 118C10 / 118E28 | the GPU reads the same descriptors from an older copy of identical content | saves CopyDescriptorsSimple bytes into VRAM: up to 1.59 ms/f (6A) + 0.3 (69/7E) at full hit rate; realistic 30-60 % -> **0.5-1 ms/f** |
| F5 | **Per-bind staging copies straight into the draw's table**: drop the 1-descriptor CopyDescriptorsSimple 115DE8 per bind (12-16 k/f) and build each table at flush with one CopyDescriptors from the tracked source handles (c+1118h etc.) | 115DE8 callers + 118C10 | same descriptors land in the same table | 55 / 5F / 44 / 54 D3D12 parts: **~0.3-0.5 ms/f** on pc2 (CPU-heap writes, call overhead only) |
| F6 | **Memoise PsoGetOrCreateGfx** (PCM, exists) and skip SetPipelineState when the result equals the bound PSO | 119670 / 118731 | same PSO object | PCM measured -0.7 ms replay on pc2 earlier; SetPipelineState redundant 26.8 % on pc2 but only 0.009 ms |
| F7 | **Batch the per-RT 1-barrier ResourceBarrier calls** in 1180D0 (118443 / 118519 / 118597) into one call | 1180D0 | same barriers, same order, one call | barrier group 1.23 ms/f on pc2 total, most outside this path; small |

**Not candidates (S):** "keep a CPU shadow instead of reading mapped memory" - the engine already does (c+BB0h); nothing reads
the ring. "Cache CBVs instead of recreating" - a CBV is only recreated when its slot was re-uploaded to a new ring address;
caching only helps together with F1/F2 (an unchanged slot keeps its CBV). "Skip descriptor copies whose source equals the
destination" - the per-bind copies already compare the bound descriptor (c+1118h / +1718h / +1B98h / +1DB0h / +1D30h) and
return early; the redundant work is the whole-table re-copy (F4), not same-slot copies.

**Order:** (1) VSForceSysUpload diagnostic + the F1 counter + an F2 read-extent census in one pc2 run; (2) F3 (small, safe)
and F1 (if the counter shows no-ops); (3) F2 (needs the offline reflection tool); (4) F4/F5.

## 1. Reading the table

- **Stages** (index used by every per-stage array): 0 VS (program c+48h), 1 / 2 / 3 (c+68h / +70h / +78h; HS / DS / GS by
  position), 4 PS (c+58h), 5 / 6 (c+88h / +90h; amplification / mesh by position, DispatchMesh exists), 7 CS (c+80h).
  SynFlushGfxStateForDraw builds the graphics array in exactly this order (S).
- **Staging descriptor heap** c+920h (CPU-only, 533 entries, increment c+938h, base c+940h): CBV `stage*14 + slot`
  (0..0x6F), SRV `0x70 + stage*0x30 + slot` (48 per stage, to 0x1EF), UAV `0x1F0 + k*0x10 + slot` (k 0 = graphics,
  1 = compute; 16 each). Samplers in
  their own staging heap (base c+970h, `stage*16 + slot`). Shader-visible ring heaps: CBV tables c+980h, SRV tables c+9D0h,
  samplers c+AA8h (ring of 50h-byte heaps, index c+AB0h).
- **Bind caches** (compare-before-copy, S): SRV c+1118h[stage*30h+slot] (reset never per list), c+1718h (54/5C), c+1B98h
  (55/5D), UAV c+1D30h[k*10h+slot] (c+1DB0h = k 1, token 5F), samplers c+1E60h[stage*10h+slot]. Dirty masks: CBV c+B70h[stage]
  (bit per slot, bit 31 = table grew), SRV c+10F0h (bit per stage), UAV c+1D20h, samplers c+1E38h; uploaded table sizes
  c+B90h / c+10F4h / c+1D24h / c+1E3Ch per stage.
- **c+B24h DirtyFlags**: 1 RT, 2 depth state, 4 viewport, 8 scissor, 10h RT-write-mask / input layout, 20h graphics PSO,
  40h compute PSO, 10000h depth-read. PSO key: c+740h record (hash, 128-B key at c+748h: VS/PS/... program pointers
  c+748h..778h, packed state c+780h / 784h / 78Ch / 790h, RT formats c+794h..).
- **"F1"** in the waste column = the constant-write tokens of section 0.3 F1 (dirty even when unchanged).
- **Costs**: PC = replay-profile.md section 3 (top 20 with numbers; the next 10 are 0.04-0.06 ms/f each); pc2 =
  `captures/pc2/rpf1` (top 30); "<x" = not in that top list. Raw RPF numbers (probe cost ~12 ns per token and per D3D12
  call included).
- "symbols name" is the name in re/symbols/symbols.tsv (kept: the RPF tables and backend.md use it). "real role" gives the
  corrected name where the old one is wrong or vague ("=" = the old one is right); section 6 lists the renamed helpers.

## 2. All 185 handlers (S; every body read in the export or disassembled)

| op | handler | symbols name | real role | payload after the u32 header | D3D12 | state read / written (c = SynCtx) | waste / note | PC ms/f (n/f) | pc2 ms/f (n/f) |
|---|---|---|---|---|---|---|---|---|---|
| 00 | 7D9A60 | SynTokBeginRenderPass00 | SynTokBeginRenderPass | u32 flags (b0 = RT count n, b1 = flag, hi16 = arg), u32, u32, u64 DSV, n x u64 RTV (<= 8); 24+8n B | via SynBeginRenderPass 7D9EBC: barriers / clears of the pass | pushes a 0x268-B pass record (c+18h stack, index c+24h), RT/DS state; c+B24h /= RT dirty | memset 64 B of RTV array per pass (trivial) | <0.04 | <0.06 |
| 01 | 8F0B44 | SynTokFencePoint01 | = | u32 fence id; 8 B | list boundary (tail-jump SynTokFencePoint 8F0B5C) | list ring c+5A0h | - | <0.04 | <0.06 |
| 02 | 458E34 | SynCmdReplaySetRenderTargets | SynTokPushRenderTargets | u32 flags (b0 n, b1 clear?, b2, b3), u32, u32, u64 DSV, n x u64 RTV; if b1 == 0 + 8 u32 clear values | ClearRenderTargetView / ClearDepthStencilView, ResourceBarrier (SynClearRtvDsvTracked 4591DC) | SynBindRenderTargetsIfCompatible 4587AC / SynPushRenderTargetsEx 458A50: pass stack c+18h, RT format state c+794h.. | clears are real work; barrier per RT one call each | 0.131 (204) | 0.199 (197) |
| 03 | 45A020 | SynTokCopyTextureRegion03 | SynTokEndRenderPass | u32 flags (bit30 = copy/resolve the pass depth into its shadow); 8 B | ResourceBarrier x2, CopyTextureRegion (vfn +80h) | pops the pass stack (c+24h--), SynUpdatePassRtFormatState | end-of-pass; two separate 1-barrier calls | <0.04 | 0.136 (197) |
| 04 | 866638 | SynTokClearRtvDsv04 | = | hdr.b2 slot, hdr.b3 flag; u32, u32; 12 B | ClearRenderTargetView / ClearDepthStencilView + RENDER_TARGET / DEPTH_WRITE barrier | RT trackers of the current pass | real work (pc2 6.6 us/call is the driver clear) | 0.071 (14) | 0.109 (16) |
| 05 | 2F0D814 | SynTokClearRtvDsv05Entry | = | hdr.b2 slot, hdr.b3 flag; 4 x u32 clear colour / depth-stencil, u32; 24 B | as 04 (SynTokClearRtvDsv05 2EFBDA4) | as 04 | - | <0.04 | <0.06 |
| 06 | 2F0E420 | SynTokResetDefaultRenderState06 | = | none; 4 B | - (SynSetDefaultRenderState 2F0034C: 12 fixed-function defaults through the cached setters) | c+98h..+234h raster/blend/depth caches, c+780h/+784h PSO key bits, c+B24h /= 20h | each setter compares first; PSO dirty only on change | <0.04 | <0.06 |
| 07 | 45A42C | SynTokReadDescriptorList384 | SynTokSetViewports | hdr.a = n; n x 24 B {x, y, w, h, minZ, maxZ}; 4+24n B | none directly (RSSetViewports deferred to the draw flush) | SynSetViewportAndScreenToNdcConsts 45A4F8: pass record viewports, NDC constants (stage 0 CB) when ctx flag c+540h | memset 384 B per token | <0.04 | <0.06 |
| 08 | 45D020 | SynTokFlushDrawsIfStateChanged | SynTokSetRtBlendEnable | hdr.b2 RT index, hdr.b3 != 0 = enable; 4 B | - | c+170h[rt], c+790h blend-enable bits, c+B24h /= 20h | state compare first | <0.04 | <0.06 |
| 09 | 699B18 | SynTokSetPerRtValue09 | SynTokSetRtBlendState | hdr.a = RT (FFh = all 8); u32 packed blend state; 8 B | - | c+A4h[8] / c+79Ch[8], c+B24h /= 20h (SynTokSetPerRtValue09_Helper_1 699C40 for one RT) | all-RT form writes 8 slots | <0.04 | <0.06 |
| 0A | 111B3C | SynCmdReplaySetStateVariant16E | SynTokSetStateVariant16E | hdr.a != 0 = alternate render-state object set; 4 B | - | c+16Eh, re-selects the PS render-state object c+2308h[+38h][c+194h] (SynSetRenderStateObject 114D18), 111D88 | changing it re-evaluates PS layout dirtiness | <0.04 | <0.06 |
| 0B | 111CBC | SynTokSetAlphaTest0B | SynTokSetAlphaTest | hdr.b2 func, hdr.b3 ref; 4 B | - | c+E0h/E4h; SynUpdateAlphaTestConstant 111D88 writes c+85Ch to the PS constant slot (CB dirty) | alpha ref is a constant write: dirties a PS CB slot (re-upload of a whole CB on the next draw) | <0.04 | <0.06 |
| 0C | 45B208 | SynTokSetFlag16F | SynTokSetFlag16F | hdr.a != 0; 4 B | - | c+16Fh, c+780h bit0, c+B24h /= 20h | - | <0.04 | <0.06 |
| 0D | 45E52C | SynTokSetStateE8_0D | SynTokSetStateE8 | hdr.a (3 bits used); 4 B | - | c+E8h, c+780h bits 2-4, c+B24h /= 20h | - | <0.04 | <0.06 |
| 0E | 45E5E0 | SynTokFlushDrawsIfStateChangedB | SynTokSetDepthFlag188 | hdr.a != 0; 4 B | - | c+188h, c+780h bit1, c+B24h /= 22h (PSO + DSV state) | also forces the DSV re-transition check in 1180D0 | <0.04 | <0.06 |
| 0F | 797384 | SynTokFlushDrawsIfStateChangedC | SynTokSetFlag16D | hdr.a != 0; 4 B | - | c+16Dh, c+780h bit5, c+B24h /= 22h | - | <0.04 | <0.06 |
| 10 | 696DA8 | SynOMSetStencilRef | SynTokSetStencilRef | u32 {b0 func, b1 ref, b2 mask}; 8 B | OMSetStencilRef (vfn +C0h) on change | c+C4h/C8h/CCh, c+780h/784h, c+B24h /= 20h | stencil ref change also dirties the PSO key (func bits): a PSO lookup per change | <0.04 | <0.06 |
| 11 | 79E6B8 | SynCmdSetStencilOps | SynTokSetStencilOps | u32 {b0 fail, b1 zfail, hi16 pass}; 8 B | - | c+D0h..D8h, c+780h, c+785h stencil enable, c+B24h /= 20h (+2) | - | <0.04 | <0.06 |
| 12 | 79E5E8 | SynTokSetStateDC_12 | SynTokSetStencilWriteMask | hdr.a; 4 B | - | body 79E600: c+DCh, c+785h, c+B24h /= 20h | - | <0.04 | <0.06 |
| 13 | 2F0E1D8 | SynTokSetRasterMode9C_13 | = | hdr.a mode; 4 B | - | SynSetRasterModeAt9C 2F00284: c+9Ch, c+784h bit16 | - | <0.04 | <0.06 |
| 14 | 45B154 | SynTokSetStateBits98_14 | SynTokSetStateBits98 | hdr.a (2 bits); 4 B | - | c+98h, c+784h bits 17-18, c+B24h /= 20h | - | <0.04 | <0.06 |
| 15 | 861294 | SynTokSetFlag189_15 | SynTokSetViewportFromRtFlag | hdr.a != 0; 4 B | - | c+189h (1180D0 uses it to pick viewport source), c+B24h /= 8 | - | <0.04 | <0.06 |
| 16 | 458CC0 | SynTokReadDescriptorList256 | SynTokSetScissorRects | hdr.a = n; n x 16 B rects; 4+16n B | - | 458D8C: pass-record scissors, c+B24h /= 8 | memset 256 B per token | <0.04 | <0.06 |
| 17 | 45BB70 | SynTokSetPolygonOffsetEnable17 | = | hdr.a != 0; 4 B | - | SynSetPolygonOffsetEnable 45BAFC: c+18Bh, raster record, c+B24h /= 20h | - | <0.04 | <0.06 |
| 18 | 45BB8C | SynTokSetPolygonOffset18 | = | f32 slope, f32 bias; 12 B | - | SynSetPolygonOffset 45BBA8: c+198h/19Ch | - | <0.04 | <0.06 |
| 19 | 7865F4 | SynTokSetRtColorWriteMask19 | SynTokSetRtWriteMask | hdr.b2 RT, hdr.b3 mask (bit 28 extra); 4 B | - | c+178h[rt]/180h[rt], c+78Ch nibble, c+B24h /= 20h | - | <0.04 | <0.06 |
| 1A | 2F0DCEC | SynTokSetStateFlag16C_1A | = | hdr.a != 0; 4 B | - | SynSetStateFlag16C 2EFDB8C: c+16Ch, c+784h bit19 | - | <0.04 | <0.06 |
| 1B | A19564 | SynTokSetClipPlaneMask1B | = | u32 mask; 8 B | - | SynSetClipPlaneMask 45B078: c+1A0h, clip constants re-uploaded to stages 0/2/3 CBs | constant write per change | <0.04 | <0.06 |
| 1C | 9B48F0 | SynTokSetClipPlane1C | = | hdr.a index; 16 B plane; 20 B | - | SynSetClipPlaneVec4 9B4924: c+(idx+1Bh)*10h, constants of stages 0/2/3 | constant write per change | <0.04 | <0.06 |
| 1D | 2F0DD08 | SynTokSetStateFlag18A_1D | = | hdr.a != 0; 4 B | - | SynSetStateFlag18A 2EFDBE4: c+18Ah, c+780h bit30 | - | <0.04 | <0.06 |
| 1E | 2F0E48C | SynTokSetStateAt190_1E | = | u32; 8 B | - | SynSetStateAt190 2F00500: c+190h, c+780h bit31 | - | <0.04 | <0.06 |
| 1F | 2F0E444 | SynTokSetPointModeAt230_1F | = | hdr.a; 4 B | - | SynSetPointModeAt230 2F00430: c+230h, stage-3 constant | - | <0.04 | <0.06 |
| 20 | 2F0E42C | SynTokSetPointSizeAt234_20 | = | f32; 8 B | - | SynSetPointSizeAt234 2F003F0: c+234h, stage-3 constant | - | <0.04 | <0.06 |
| 21 | 45D148 | SynTokSetRenderStateMode | SynTokSetPsStateMode | hdr.a mode; u64 object (used when mode 5); 12 B | - (D3D12 seen in RPF comes from the nested flush of pending immediate draws) | c+194h, c+60h; selects the PS render-state object (SynSetRenderStateObject) | - | 0.04-0.06 | 0.101 (81) |
| 22 | 2F0E164 | SynTokSetShaderProgramAt50_22 | = | u64 program; 12 B | - | SynSetShaderProgramAt50 45D274: c+50h, viewport scale constants | - | <0.04 | <0.06 |
| 23 | 1164C4 | SynTokSetPipelineLayout | SynTokSetVertexShader | u64 shader object; 12 B | - | c+48h / PSO key c+748h, c+B24h /= 30h; compares the shader layout (+40h CB list, +44h CBV, +48h SRV, +4Ch UAV, +50h sampler counts) with the uploaded table sizes and sets c+B70h/10F0h/1D20h/1E38h dirty bits; copies +64h x 16 B of c+860h into a VS CB | every VS change marks tables dirty only if the new layout is LARGER than what is bound: good; but the c+860h block copy re-dirties a CB slot on every VS change | 0.04-0.06 | <0.06 |
| 24 | 45C7C4 | SynTokSetStageProgramAt68_24 | SynTokSetStage1Program | u64; 12 B | - | SynSetStageProgramAt68 45AEB0: c+68h/758h, 114B00 layout dirty bits | - | <0.04 | <0.06 |
| 25 | 45C7A0 | SynTokSetStageProgramAt70_25 | SynTokSetStage2Program | u64; 12 B | - | SynSetStageProgramAt70 45AE24: c+70h/760h, clip-plane constants | constant write | <0.04 | <0.06 |
| 26 | 45BC2C | SynTokSetStageProgramAt78_26 | SynTokSetStage3Program | u64; 12 B | - | SynSetStageProgramAt78 45ACD8: c+78h/768h, point params + 1/RT size constants | constant write | <0.04 | <0.06 |
| 27 | 2F0E210 | SynTokSetStageProgramAt88_27 | SynTokSetStage5Program | u64; 12 B | - | SynSetStageProgramAt88 97CA14: c+88h/770h | - | <0.04 | <0.06 |
| 28 | 2F0E274 | SynTokSetStageProgramAt90_28 | SynTokSetStage6Program | u64; 12 B | - | SynSetStageProgramAt90 97CA3C: c+90h/778h, clip constants | - | <0.04 | <0.06 |
| 29 | 114750 | SynCmdSetRootSignature | SynTokSetPixelShader | u64 shader object; 12 B | - | c+58h / PSO key c+750h, c+B24h /= 20h, layout dirty bits for stage 4, alpha-ref constant c+85Ch -> PS CB (SynCbSlotWriteStage) | name in symbols (SynCmdSetRootSignature) is wrong: it binds the PS. Writing c+85Ch on every PS change dirties a PS CB slot -> whole-CB re-upload at the next draw even if the value is the same | 0.081 (670) | 0.060 (515) |
| 2A | 1134E4 | SynTokSetProgramAt80 | SynTokSetComputeShader | u64; 12 B | - | c+80h / c+810h compute PSO key, c+B24h /= 40h, compute layout dirty bits (c+BACh..) | - | <0.04 | <0.06 |
| 2B | 115988 | SynTokSetCbDataSlot | SynTokCbShadowWriteStage0 | hdr.a = byte size n; u32 (slot<<28 / offset); n bytes (dword padded) | - | Memcpy into the VS CB shadow c+BB0h[slot] + offset, c+B70h[0] /= 1<<slot | F1: dirty even when the bytes are identical | 0.04-0.06 | <0.06 |
| 2C | 1146DC | SynTokUploadConstants2C_Stage0 | SynTokCbWriteArrayStage0 | hdr.a = element size e (bytes); u32 (slot<<28 / offset); u32 (first element lo16, count hi16); count x e bytes; dst = shadow + offset + index*max(e,16) | - | SynUploadConstantsToRing 1158A0 (misnamed: it writes the SHADOW, strided by max(e*4,16)), c+B70h[0] dirty | F1; one Memcpy per element when the element is < 16 B | 0.104 (1332) | 0.245 (910) |
| 2D | 907384 | SynTokCbShadowWrite_Stage1_2D | SynTokCbShadowWriteStage1 | as 2B | - | SynCbShadowWriteStage 1159F0, stage 1 | F1 | <0.04 | <0.06 |
| 2E | 2F0E264 | SynTokUploadConstants_Stage1_2E | SynTokCbWriteArrayStage1 | as 2C | - | SynUploadConstantsStageWorker 115814, stage 1 | F1 | <0.04 | <0.06 |
| 2F | 9055A0 | SynTokCbShadowWrite_Stage2_2F | SynTokCbShadowWriteStage2 | as 2B | - | stage 2 | F1 | <0.04 | <0.06 |
| 30 | 900824 | SynTokUploadConstants_Stage2_30 | SynTokCbWriteArrayStage2 | as 2C | - | stage 2 | F1 | <0.04 | <0.06 |
| 31 | 9061A4 | SynTokCbShadowWrite_Stage3_31 | SynTokCbShadowWriteStage3 | as 2B | - | stage 3 | F1 | <0.04 | <0.06 |
| 32 | 2F0E254 | SynTokUploadConstants_Stage3_32 | SynTokCbWriteArrayStage3 | as 2C | - | stage 3 | F1 | <0.04 | <0.06 |
| 33 | 2F0E234 | SynTokCbShadowWrite_Stage5_33 | SynTokCbShadowWriteStage5 | as 2B | - | stage 5 | F1 | <0.04 | <0.06 |
| 34 | 2F0E244 | SynTokUploadConstants_Stage5_34 | SynTokCbWriteArrayStage5 | as 2C | - | stage 5 | F1 | <0.04 | <0.06 |
| 35 | 2F0E298 | SynTokCbShadowWrite_Stage6_35 | SynTokCbShadowWriteStage6 | as 2B | - | stage 6 | F1 | <0.04 | <0.06 |
| 36 | 2F0E2A8 | SynTokUploadConstants_Stage6_36 | SynTokCbWriteArrayStage6 | as 2C | - | stage 6 | F1 | <0.04 | <0.06 |
| 37 | 115EA4 | SynTokSetCbData_Tok37 | SynTokCbShadowWriteStage4 | as 2B | - | PS shadow c+D70h[slot], c+B80h dirty | F1 | <0.04 | <0.06 |
| 38 | 11579C | SynTokUploadConstants38_Stage4 | SynTokCbWriteArrayStage4 | as 2C | - | PS shadow, c+B80h dirty | F1 (4461/f on the PC: most frequent CB write) | 0.081 (4461) | 0.096 (2827) |
| 39 | 11676C | SynTokSetCbData_Tok39 | SynTokCbShadowWriteStage7 | as 2B | - | CS shadow c+EC0h[slot], c+B8Ch dirty | F1 (5673/f on pc2) | <0.04 | 0.057 (5673) |
| 3A | 861CF8 | SynCmdReplaySetConstantsStage7 | SynTokCbWriteArrayStage7 | as 2C | - | CS shadow, c+B8Ch dirty | F1 | <0.04 | <0.06 |
| 3B | 45B034 | SynTokSetPtrAt38Dirty10 | SynTokSetInputLayout | u64 input layout; 12 B | - | c+38h (read by SynUpdateRtWriteMaskDirty: +18h count, +1Ch n slots, +20h slot bytes), c+B24h /= 10h | - | <0.04 | <0.06 |
| 3C | 116260 | SynTokSetVertexBufferTracked | = | hdr.b2 slot (5 bits), hdr.b3 stride; u32 offset; u64 buffer object; 16 B | ResourceBarrier (1 per change), IASetVertexBuffers(slot, 1) (vfn +160h) | c+260h[slot] obj, c+360h stride, c+3E0h offset, c+484h[slot] hdr (c+B24h /= 10h on change), obj+4Ch | skips when unchanged (good); one barrier call per VB | 0.146 (1945) | 0.269 (1048) |
| 3D | 114A20 | SynTokSetIndexBuffer | = | u64 index-buffer object; 12 B | ResourceBarrier (1, if state differs or new list stamp), IASetIndexBuffer (vfn +158h) | c+30h BoundIb; obj+20h state, obj+28h stamp vs ListStamp c+6D0h | unchanged -> skipped; pc2 667 ns/call = the driver (IASetIndexBuffer 272 ns) | 0.118 (1425) | 0.536 (805) |
| 3E | 8DAEEC | SynTokBindTexturesStage0 | SynTokBindTextureStateStage0 | hdr.a slot; u64 128-B record ptr, u64 sampler key, u32 view-select, u64 view; 32 B | via SynBindTextureSlot: CopyDescriptorsSimple(1) x2 (sampler, SRV), ResourceBarrier | SynBindTextureSlotStateBlockStage 115B08 stage 0 | memset 128 B + 128-B copy per token (see 44) | <0.04 | <0.06 |
| 3F | 2F0E2C8 | SynTokBindTextureSlotState_Stage1_3F | SynTokBindTextureStateStage1 | as 3E | as 3E | stage 1 | as 44 | <0.04 | <0.06 |
| 40 | 9142FC | SynTokBindTextureSlotState_Stage2_40 | SynTokBindTextureStateStage2 | as 3E | as 3E | stage 2 | as 44 | <0.04 | <0.06 |
| 41 | 2F0E200 | SynTokBindTextureSlotState_Stage3_41 | SynTokBindTextureStateStage3 | as 3E | as 3E | stage 3 | as 44 | <0.04 | <0.06 |
| 42 | 2F0E1B8 | SynTokBindTextureSlotState_Stage5_42 | SynTokBindTextureStateStage5 | as 3E | as 3E | stage 5 | as 44 | <0.04 | <0.06 |
| 43 | 2F0E308 | SynTokBindTextureSlotState_Stage6_43 | SynTokBindTextureStateStage6 | as 3E | as 3E | stage 6 | as 44 | <0.04 | <0.06 |
| 44 | 115A70 | SynTokReadStateBlock128Dispatch | SynTokBindTextureStateStage4 | hdr.a slot; u64 ptr to a 128-B texture-bind record (copied: +0 resource, +10h state ptr, +18h/+20h views, +3Eh flags, +48h sampler key, +68h ...), u64 sampler key (-> rec+48h), u32 view-select (merged into rec+3Eh bits), u64 view (-> rec+18h[sel]); 32 B | CopyDescriptorsSimple(1) for the sampler (SynTexDescCacheFindOrCreate 116608 -> staging sampler slot) and for the SRV (staging 0x70+stage*30h+slot), ResourceBarrier to PIXEL_SHADER_RESOURCE (80h) / NON_PIXEL (40h) | c+1E60h[stage*80h+slot] bound sampler, c+1118h[stage*180h+slot] bound SRV, c+10F0h / c+1E38h dirty bits; resource state via rec+10h | memset(128) immediately overwritten by the 128-B copy (dead store); the copy reads a pointer into engine material data (cache miss per bind); unchanged binds return after the compare (good). pc2 332 ns/call | 0.488 (4069) | 0.832 (2507) |
| 45 | 84196C | SynTokBindTextureSlotState_Stage7_45 | SynTokBindTextureStateStage7 | as 44 | as 44 | stage 7 (CS) | as 44 | 0.04-0.06 | 0.113 (721) |
| 46 | 747190 | SynTokClearTextureSlot_Stage0_46 | SynTokUnbindTextureStage0 | hdr.a slot; 4 B | CopyDescriptorsSimple(1) of the null SRV c+1D18h if not already null | c+1118h, c+10F0h | - | <0.04 | <0.06 |
| 47 | 2F0D7F4 | SynTokClearTextureSlot_Stage1_47 | SynTokUnbindTextureStage1 | as 46 | as 46 | SynClearTextureSlotStage 747214 | - | <0.04 | <0.06 |
| 48 | 747180 | SynTokClearTextureSlot_Stage2_48 | SynTokUnbindTextureStage2 | as 46 | as 46 | - | - | <0.04 | <0.06 |
| 49 | 2F0D7E4 | SynTokClearTextureSlot_Stage3_49 | SynTokUnbindTextureStage3 | as 46 | as 46 | - | - | <0.04 | <0.06 |
| 4A | 2F0D7D4 | SynTokClearTextureSlot_Stage5_4A | SynTokUnbindTextureStage5 | as 46 | as 46 | - | - | <0.04 | <0.06 |
| 4B | 2F0D804 | SynTokClearTextureSlot_Stage6_4B | SynTokUnbindTextureStage6 | as 46 | as 46 | - | - | <0.04 | <0.06 |
| 4C | 7471E8 | SynTokClearTextureSlot_Stage4_4C | SynTokUnbindTextureStage4 | as 46 | as 46 | - | - | 0.04-0.06 | <0.06 |
| 4D | 7471BC | SynCmdClearTextureSlotStage7 | SynTokUnbindTextureStage7 | as 46 | as 46 | - | - | <0.04 | <0.06 |
| 4E | 113BA4 | SynTokBindBufferSrv4E | SynTokBindBufferSrvStage0 | hdr.a slot; u64 buffer object; 12 B | CopyDescriptorsSimple(1) into staging 70h+slot, ResourceBarrier to NON_PIXEL_SHADER_RESOURCE (stamped) | c+1118h[slot], c+10F0h bit0; obj+20h state, obj+58h SRV, SynResGetStampPtr 114C04 walks the parent chain | SynResGetStampPtr walks obj+10h parents per bind (pointer chase) even when the bind is then skipped as unchanged: move the compare first | <0.04 | <0.06 |
| 4F | 989F2C | SynTokBindBufferSrv_Stage1_4F | SynTokBindBufferSrvStage1 | as 4E | as 4E | SynBindBufferSrvStage 114E24 stage 1 | as 4E | <0.04 | <0.06 |
| 50 | 90FA3C | SynTokBindBufferSrv_Stage2_50 | SynTokBindBufferSrvStage2 | as 4E | as 4E | stage 2 | as 4E | <0.04 | <0.06 |
| 51 | 9E5858 | SynTokBindBufferSrv_Stage3_51 | SynTokBindBufferSrvStage3 | as 4E | as 4E | stage 3 | as 4E | <0.04 | <0.06 |
| 52 | 2F0E188 | SynTokBindBufferSrv_Stage5_52 | SynTokBindBufferSrvStage5 | as 4E | as 4E | stage 5 | as 4E | <0.04 | <0.06 |
| 53 | 2F0E2D8 | SynTokBindBufferSrv_Stage6_53 | SynTokBindBufferSrvStage6 | as 4E | as 4E | stage 6 | as 4E | <0.04 | <0.06 |
| 54 | 115084 | SynTokBindBufferSrv54 | SynTokBindBufferSrvStage4 | as 4E | as 4E (PIXEL_SHADER_RESOURCE) | c+1718h[slot] (separate cache, not reset per list), staging 130h+slot, c+10F0h bit4 | as 4E | 0.068 (762) | 0.086 (723) |
| 55 | 815DB0 | SynCmdBindResourceSlot | SynTokBindBufferSrvStage7 | as 4E | as 4E | c+1B98h[slot], staging 1C0h+slot, c+10F0h bit7 | as 4E; 3620/f on pc2 (71 ns): the most frequent bind | 0.04-0.06 | 0.256 (3620) |
| 56 | 115398 | SynTokBindTextureSrv56 | SynTokBindTextureSrvStage0 | hdr.a slot; u64 texture view object (+18h -> {resource, .., +20h state, +24h target state}, +60h SRV); 12 B | CopyDescriptorsSimple(1), ResourceBarrier (no stamp) | c+1118h[slot], c+10F0h bit0 | - | 0.04-0.06 | <0.06 |
| 57 | 2F0E2B8 | SynTokBindTextureSrv_Stage1_57 | SynTokBindTextureSrvStage1 | as 56 | as 56 | SynBindTextureSrvStage 11518C | - | <0.04 | <0.06 |
| 58 | 2F0E1C8 | SynTokBindTextureSrv_Stage2_58 | SynTokBindTextureSrvStage2 | as 56 | as 56 | - | - | <0.04 | <0.06 |
| 59 | 2F0E1F0 | SynTokBindTextureSrv_Stage3_59 | SynTokBindTextureSrvStage3 | as 56 | as 56 | - | - | <0.04 | <0.06 |
| 5A | 2F0E1A8 | SynTokBindTextureSrv_Stage5_5A | SynTokBindTextureSrvStage5 | as 56 | as 56 | - | - | <0.04 | <0.06 |
| 5B | 2F0E2F8 | SynTokBindTextureSrv_Stage6_5B | SynTokBindTextureSrvStage6 | as 56 | as 56 | - | - | <0.04 | <0.06 |
| 5C | 8507B0 | SynTokBindTextureSrv5C | SynTokBindTextureSrvStage4 | as 56 | as 56 | c+1718h cache | - | <0.04 | <0.06 |
| 5D | 823534 | SynTokBindTextureSrv5D | SynTokBindTextureSrvStage7 | as 56 | as 56 | c+1B98h cache | - | <0.04 | <0.06 |
| 5E | 90445C | SynTokBindUav2Stamped_Stage0_5E | SynTokBindUavStage0 | hdr.a slot; u64 UAV descriptor, u64 resource, u64 state ptr, u64 stamp ptr; 36 B | CopyDescriptorsSimple(1) into staging (1Fh+stage)*10h+slot, ResourceBarrier to UNORDERED_ACCESS (stamped) | c+1D30h[stage*80h+slot], c+1D20h dirty | - | <0.04 | <0.06 |
| 5F | 113A60 | SynTokBindUavStamped | SynTokBindUavStage7 | as 5E | as 5E | c+1DB0h[slot] (stage 7), c+1D20h /= 2; null -> SynBindUavTracked with c+1E30h | 2538/f on pc2 | 0.121 (721) | 0.300 (2538) |
| 60 | 698DC0 | SynTokSetCbData_Stage0_60 | SynTokSetCbDataStage0 | hdr.a slot; u32 size (FFFFFFFFh = none); size bytes padded to 4 | - | SynSetCbDataStage 698F58: lazy 64-KB shadow page (SynEnsureCbShadowPage A397D8, under a lock at g_QpcTimer+90h), Memcpy, c+B70h dirty | F1 | <0.04 | <0.06 |
| 61 | 698DD0 | SynTokSetCbData_Stage1_61 | SynTokSetCbDataStage1 | as 60 | - | stage 1 | F1 | <0.04 | <0.06 |
| 62 | 698DE0 | SynTokSetCbData_Stage2_62 | SynTokSetCbDataStage2 | as 60 | - | stage 2 | F1 | <0.04 | <0.06 |
| 63 | 698DF0 | SynTokSetCbData_Stage3_63 | SynTokSetCbDataStage3 | as 60 | - | stage 3 | F1 | <0.04 | <0.06 |
| 64 | 2F0E198 | SynTokSetCbData_Stage5_64 | SynTokSetCbDataStage5 | as 60 | - | stage 5 | F1 | <0.04 | <0.06 |
| 65 | 2F0E2E8 | SynTokSetCbData_Stage6_65 | SynTokSetCbDataStage6 | as 60 | - | stage 6 | F1 | <0.04 | <0.06 |
| 66 | 698EAC | SynTokSetCbData | SynTokSetCbDataStage4 | as 60 | - | PS shadow c+D70h, c+B80h | F1 (PC 316 ns, pc2 455 ns/call) | 0.072 (229) | 0.094 (207) |
| 67 | 698E00 | SynTokSetCbData_Stage7_67 | SynTokSetCbDataStage7 | as 60 | - | CS shadow c+EC0h, c+B8Ch | F1 | <0.04 | <0.06 |
| 68 | 2F0E5AC | SynTokCopyBufferRegion68 | SynTokCopyBufferToStreamOut | hdr bits 16-22 SO slot; u32 size; u64 dst, u64 src, u64 ...; 32 B | ResourceBarrier x2, CopyBufferRegion, SOSetTargets (SynCopyBufferRegionTracked 2F0075C) | trackers | - | <0.04 | <0.06 |
| 69 | 6BE12C | SynTokDraw | = | hdr.a topology; u32 StartVertex, u32 VertexCount; 12 B | per-draw flush (section 0) + DrawInstanced(VertexCount, 1, StartVertex, 0) | section 0 | section 0: same path as 6A | 0.528 (164) | 1.872 (184) |
| 6A | 1167D4 | SynTokDrawIndexed | = | hdr.a topology; u32, u32 (unused by the replay), u32 StartIndex, u32 IndexCount; 20 B | section 0 + DrawIndexedInstanced(IndexCount, 1, StartIndex, 0, 0) | section 0; c+548h++ | section 0 (8 dead payload bytes per draw) | 1.617 (1880) | 11.181 (1095) |
| 6B | 87A4F4 | SynTokDrawIndexedBoundIB6B | SynTokDrawIndexedWholeIb | hdr.a topology; u32 instance count; 8 B | flush + DrawIndexedInstanced(IB index count c+30h->+60h, n, 0, 0, 0) (87A518) | section 0 | - | <0.04 | 0.078 (4) |
| 6C | 2F0DC54 | SynTokDrawIndexedInstanced6C | SynTokDrawIndexedInstanced | hdr.a topology; 2 x u32 unused, u32 StartIndex, u32 IndexCount, u32 InstanceCount; 24 B | flush + DrawIndexedInstanced (2EFD898) | section 0 | - | <0.04 | <0.06 |
| 6D | 9F3484 | SynTokDrawInstanced6D | SynTokDrawInstanced | hdr.a topology; 4 x u32; 20 B | flush + DrawInstanced (9F34E0) | section 0 | - | <0.04 | <0.06 |
| 6E | 6ECF48 | SynTokExecuteIndirect6E | SynTokExecuteIndirectDraw | hdr.a topology; u32 arg offset; u64 arg buffer object; 16 B | flush, ResourceBarrier to INDIRECT_ARGUMENT (stamped), ExecuteIndirect(c+6E8h DRAW signature, 1, buf, off) | section 0 | - | 0.04-0.06 | 0.085 (126) |
| 6F | 6EB908 | SynTokExecuteIndirect | SynTokExecuteIndirectDrawIndexed | as 6E | as 6E with c+6F0h DRAW_INDEXED signature | section 0 | max count 1, one call per indirect draw: batching needs the producer to emit counts (not same-binary) | 0.353 (249) | 0.475 (148) |
| 70 | 45B670 | SynTokBeginBlitCopyPass70 | SynTokBeginBlitCopyPass | none; 4 B | - (SynBeginBlitCopyPass 45AB1C saves state into c+510h..53Ch, binds blit VS/PS/VB/IB, NDC constants) | c+510h..+53Ch, c+540h | - | <0.04 | <0.06 |
| 71 | 45B67C | SynTokFlushTrackedCopyPrelude71 | SynTokEndBlitCopyPass | none; 4 B | IASetVertexBuffers / IASetIndexBuffer / state restore (SynFlushTrackedCopyPrelude 45AF68) | restores the state saved by 70 | restores every saved setter even if unchanged (each compares; cheap) | 0.04-0.06 | 0.069 (41) |
| 72 | 8741EC | FlushPendingDrawsIfAny | = | none; 4 B | - | flushes pending immediate draws c+2458h | - | <0.04 | <0.06 |
| 73 | 115F0C | SynTokImmediateQuad73 | SynTokImmediateQuad | 40 B of corner / colour / uv values; 44 B | via SynEnsureImmediateDrawSpace 1160B0 (may flush: IASet*, DrawIndexedInstanced) | immediate VB/IB rings c+2390h / c+23D8h (UPLOAD), count c+2458h | immediate geometry is written into UPLOAD memory: BAR stores with ReBAR (few per frame) | <0.04 | <0.06 |
| 74 | 2F0DA40 | SynTokImmediateQuad74 | SynTokImmediateQuadUv | 2 x 20 B; u32, u32; 52 B | as 73 | as 73 | as 73 | <0.04 | <0.06 |
| 75 | 7A6AB0 | SynTokImmediateQuad75 | SynTokImmediateQuadUvColor | 2 x 20 B; 5 x u32 | as 73 | as 73 | as 73 | <0.04 | <0.06 |
| 76 | 2F0DBB8 | SynTokImmediateTriangle76 | SynTokImmediateTriangle | 2 x 24 B; u32, u32 | as 73 | SynImmediateTriangleUniform 2EFD688 | as 73 | <0.04 | <0.06 |
| 77 | 2F0DB08 | SynTokImmediateTriangleVc77 | SynTokImmediateTriangleColored | 2 x 24 B; 12 B colours; u32 | as 73 | SynImmediateTriangleAppend 18997FC | as 73 | <0.04 | <0.06 |
| 78 | 2F0D9EC | SynTokImmediateLine78 | SynTokImmediateLine | 6 x u32; 28 B | as 73 | SynImmediateLineAppend 2EFD1D0 | as 73 | <0.04 | <0.06 |
| 79 | 2F0DAD0 | SynTokDrawVbIb79Entry | = | u32, u32, u64; 20 B | stamped VB / IB transitions, IASetVertexBuffers, IASetIndexBuffer, Draw(Indexed) (SynTokDrawVbIb79 2EFD430) | section 0 | - | <0.04 | <0.06 |
| 7A | 45C7E8 | SynTokCopyTrackedResource | = | hdr.a flags; 16 B + 16 B regions, u64; 44 B | ResourceBarrier, CopyTextureRegion or a blit draw (SynGenericCopyTracked 45C880, 1.9 KB) | stack-local trackers | 10/f; driver cost | 0.075 (10) | 0.174 (11) |
| 7B | 45DD48 | SynTokDepthCopy7B | SynTokDepthCopy | 2 x 16 B, u64; 44 B | as 7A (SynDepthCopyTracked 45DDC8) | - | - | <0.04 | <0.06 |
| 7C | 8BB170 | SynTokComputeBegin | = | none; 4 B | - | c+B24h /= 40h (compute PSO dirty), c+541h = 1 | forces a compute PSO lookup (hash + EnterCriticalSection + B-tree) and SetPipelineState at the next dispatch even when the CS did not change: skip when c+80h is unchanged since the last SetPipelineState | <0.04 | <0.06 |
| 7D | 8988A8 | SynTokComputeEnd | = | none; 4 B | ResourceBarrier(UAV, NULL) (SynUavBarrier 1139D4) | c+B24h /= 20h (graphics PSO dirty), c+541h = 0 | global UAV barrier per compute section + forced graphics PSO re-lookup | <0.04 | <0.06 |
| 7E | 58BDC4 | SynTokDispatch | = | u32 x, y, z; 16 B | SynFlushComputeState 58BE90: compute descriptor tables (CmdListFlushComputeDescTable_495414 functor 409A618), SetPipelineState, Dispatch (vfn +70h) | c+80h CS, c+B24h bit 40h, c+550h++ | PSO lookup under EnterCriticalSection on every 7C..7E section (see 7C); compute CBV tables are copied the same way as graphics (CmdListUploadComputeCbvTable 4959EC). pc2 1.7 us/call | 0.273 (265) | 1.794 (1030) |
| 7F | 2F0D970 | SynTokDispatchMesh7F | SynTokDispatchMesh | 4 x u32; 20 B | flush + ID3D12GraphicsCommandList6::DispatchMesh (QueryInterface + Release per call, 2EFD0A0) | section 0 | QueryInterface/Release per DispatchMesh: cache the List6 pointer per list | <0.04 | <0.06 |
| 80 | 2F0D9B4 | SynTokExecuteIndirect80Entry | = | u32 topology, u32 offset, u64 buffer; 20 B | stamped INDIRECT_ARGUMENT barrier, ExecuteIndirect (2EFD138) | section 0 | - | <0.04 | <0.06 |
| 81 | 58BC74 | SynTokExecuteIndirectDispatch | SynTokExecuteIndirectDispatch | u32 offset; u64 buffer; 16 B | compute flush, barrier, ExecuteIndirect(c+700h DISPATCH signature) | as 7E | as 7E | <0.04 | <0.06 |
| 82 | 9AFDF0 | SynTokPagePoolBegin82 | = | none; 4 B | - | c+543h = 1 (page-pool batch open) | - | <0.04 | <0.06 |
| 83 | 9A62FC | SynTokPagePoolEnd83 | = | none; 4 B | - (SynPagePoolEndBatch 9AFF24 -> SynSharedLockedUpdate_7F64DC under the KtglCache SRW, shared) | c+543h = 0 | SRW shared lock per batch | <0.04 | <0.06 |
| 84 | 2F0DCBC | SynTokSetStateFlag482_84 | = | hdr.a; 4 B | - | SynSetStateFlag482 2EFDADC: c+482h, c+784h bit20 | - | <0.04 | <0.06 |
| 85 | 2F0DCD4 | SynTokSetStateFlag483_85 | = | hdr.a; 4 B | - | SynSetStateFlag483 2EFDB34: c+483h, c+784h bit31 | - | <0.04 | <0.06 |
| 86 | 7F6118 | SynTokReadStateBlock128DispatchB | SynTokBindTextureStateAnyStage | hdr.b2 stage, hdr.b3 slot; then as 44 | as 44 | as 44 | as 44 | <0.04 | <0.06 |
| 87 | 698A44 | SynTokBindTexture87 | SynTokBindTextureNoSampler | hdr.b2 stage, hdr.b3 slot; 128-B record as 44 | CopyDescriptorsSimple(1), ResourceBarrier | c+1118h, c+10F0h | as 44 (memset + copy) | <0.04 | <0.06 |
| 88 | 698B90 | SynTokBindSrvDescriptor | SynTokBindNullSrv | hdr.b2 stage, hdr.b3 slot; 4 B | CopyDescriptorsSimple(1) of c+1D18h | c+1118h, c+10F0h | - | <0.04 | <0.06 |
| 89 | 2F0DCA4 | SynTokGpuDrivenBatch89Entry | = | hdr.a; 4 B (rest read by SynTokGpuDrivenBatch89 2EFD8DC) | buffer SRV barrier, UAV barrier, ExecuteIndirect | - | - | <0.04 | <0.06 |
| 8A | 113A0C | SynTokUavBarrier | SynTokUavBarrier | u64 resource; 12 B | ResourceBarrier(1, UAV) | - | one call per UAV barrier (375/f PC, 308/f pc2 at 543 ns): consecutive 8A tokens could go into one call; same barriers (driver cost is per barrier on AMD, so small) | 0.111 (375) | 0.167 (308) |
| 8B | 7A5D2C | SynTokBufferCopyTransitions | = | u32 dstOff, u32 srcOff, u32 size, u64 dst, u64 src; 28 B | COPY_DEST / COPY_SOURCE barriers, CopyBufferRegion (SynBufferCopyTransitions 7A5FB8) | trackers (stamped) | - | <0.04 | 0.085 (152) |
| 8C | 2F0D858 | SynCopyBufferRegion | = | u32 dstOff, u32 srcOff, u32 size, u64 dst, u64 src; 28 B | CopyBufferRegion (vfn +78h), no barrier | - | - | <0.04 | <0.06 |
| 8D | 2F0D8EC | SynTokCopyResource8D | = | u64 dst, u64 src; 20 B | CopyResource (SynCopyResource 2EFC04C) | - | - | <0.04 | <0.06 |
| 8E | A96D90 | SynTokSetMarkerIface | SynTokSetMarkerInterface | u64; 12 B | - | g_MarkerInterface 4BD09C0 | - | <0.04 | <0.06 |
| 8F | 6B8884 | SynTokCallbackWithCmdList8F | SynTokRunCmdListCallback | u64 function, u64 arg; 20 B | whatever the callback records: DLSS / XeSS / FSR evaluate (writers DlssExecute 6B64D8 etc.) | {list c+580h, c} passed | 1/f, 138 us on the PC: upscaler SDK, not engine waste | 0.138 (1) | <0.06 |
| 90 | 3D4FC4 | CmdSubStreamRun | CmdSubStreamRun | u64 async context, u64 begin, u64 end; 28 B | replays the async-compute sub-stream (table 3C02800, section 3) into the async context at ctx+68h | async slot context | runs ON the replay thread: async compute recording is serialised with the graphics replay | 0.143 (6) | 0.070 (4) |
| 91 | 9E2A74 | SynTokCopyTextureRegion91Entry | = | u64 dst, 32-B box, u64 src, 7 x u32 | COPY barriers, CopyTextureRegion (SynCopyTextureRegion91 9E2BA4) | - | - | <0.04 | <0.06 |
| 92 | 7F9A6C | SynTokCopyTextureRegion92 | = | 76 B (2 resources, subresources, box, footprint) | two separate 1-barrier calls + CopyTextureRegion + two restores (SynCopyTextureRegionTracked 7F9BF8) | - | 4 barrier calls per copy: batch 2+2 | <0.04 | <0.06 |
| 93 | 2F0DFEC | SynTokResolveSubresourceRegion93Entry | = | hdr.a != 0 adds a 16-B rect; 48-68 B | RESOLVE barriers, ResolveSubresourceRegion (2EFFF54) | - | - | <0.04 | <0.06 |
| 94 | 7B1750 | SynTokClearUavUint | = | 4 x u64, 4 x u32 values, u32 n rects, n x 16 B | UAV barrier, ClearUnorderedAccessViewUint (SynClearUavUintTracked 7B18E4) | - | _chkstk alloca per token | <0.04 | 0.062 (30) |
| 95 | 7F64B4 | SynTokPagePoolUpdate95 | = | none; 4 B | - (body 7F64C0 -> SynSharedLockedUpdate_7F64DC(PagePool g+61A0h): KtglCache upload under SRW shared, CopyBufferRegion batches) | KtglCache, upload ring | once per frame; can submit and spin on a fence when the cache upload ring is full | <0.04 | <0.06 |
| 96 | 2F0D750 | SynTokPagePoolTouch96 | = | u32 page, u32; 12 B | - (SynPagePoolTouch 2EFB97C) | page pool | - | <0.04 | <0.06 |
| 97 | 2F0D774 | SynTokCacheTileUnmap97 | = | u32, u32, u32, 16 B; 32 B | tile unmap (CacheUnmapOnCtx 2EFBA1C) | KtglCache | - | <0.04 | <0.06 |
| 98 | 891F24 | SynTokPagePoolPushRecord98 | = | hdr.b2, hdr.b3; u64; 12 B | - (PagePoolPushTextureUpdateC 891F80 under the SRW shared) | page pool | - | <0.04 | <0.06 |
| 99 | 981220 | SynTokFlushPendingTransitions99 | = | none; 4 B | COPY barriers + CopyBufferRegion (body 98122C -> SynBufferCopyWithBarriers 7A5DB8 on g+6870h) | - | - | <0.04 | <0.06 |
| 9A | 2F0D684 | SynTokCacheTileMap9A | = | u64, 16 B, u32 x2, 16 B, 20 B | tile map (SynCacheTileMapOnCtx 2EFB938) | KtglCache | - | <0.04 | <0.06 |
| 9B | 7F52AC | SynTokTransitionResource | SynTokTransitionResource | u64 resource, u64 state ptr, u32 target, u64 stamp ptr, u32 subresource; 32 B | ResourceBarrier(1) if the tracked state differs | tracked state, ListStamp | - | <0.04 | <0.06 |
| 9C | 7C67E0 | SynResourceBarrier_7C67E0 | SynTokResourceBarrierList | u32 n; n x 24 B {u64 res, u32 sub, u32 before, u32 after, pad} | ResourceBarrier(n) in ONE call (skips before == after) | - | already batched; 1 KB stack copy | <0.04 | <0.06 |
| 9D | 84EFF4 | SynTokResourceBarrierBlob9D | SynTokResourceBarrierBlob | blob (SynReadBarrierBlob 84F0B0: u32 n + n x 16 B) | ResourceBarrier (85033C) | - | - | <0.04 | <0.06 |
| 9E | 8F2C90 | SynTokWriteDwordToPtr9E | SynTokWriteDword | u64 address, u32 value; 16 B | - (CPU store) | arbitrary engine memory | - | <0.04 | <0.06 |
| 9F | 8962D8 | SynTokDiscardResource9F | SynTokDiscardResource | u64 resource; 12 B | DiscardResource(res, NULL) (vfn +198h) | - | - | <0.04 | <0.06 |
| A0 | A96D70 | SynTokSetMarkerReplay | SynTokSetMarkersEnabled | hdr.a; 4 B | - | c+542h | - | <0.04 | <0.06 |
| A1 | 2F0E318 | SynTokSetMarkerAnsiA1 | SynTokSetMarkerAnsi | hdr.a = dword length; u32 colour; string | SetMarker (vfn +1C0h) via SynSetMarkerAnsi 2F00300 | - | strncpy 1 KB to the stack before the marker check | <0.04 | <0.06 |
| A2 | 2F0E39C | SynTokSetMarkerWideA2 | SynTokSetMarkerWide | as A1, UTF-16 | SetMarker | - | as A1 | <0.04 | <0.06 |
| A3 | 2F0E4A4 | SynTokSetMarkerAnsiA3 | SynTokSetMarkerAnsiB | as A1 | SetMarker (2F00550) | - | as A1 | <0.04 | <0.06 |
| A4 | 2F0E528 | SynTokSetMarkerWideA4 | SynTokSetMarkerWideB | as A2 | SetMarker (2F00574) | - | as A1 | <0.04 | <0.06 |
| A5 | 114024 | SynTokBeginEvent | SynTokBeginEvent | hdr.a = dword length; u32 colour; string | BeginEvent (vfn +1C8h) or the marker interface, only when c+542h | - | memset 1 KB + copy of the string BEFORE SynBeginEvent checks c+542h: ~50-100 ns per event wasted when markers are off; move the check first | <0.04 | <0.06 |
| A6 | 113FC8 | SynTokEndEvent | SynTokEndEvent | none; 4 B | EndEvent (vfn +1D0h) when c+542h | - | - | <0.04 | <0.06 |
| A7 | 8CEBC0 | SynTokWriteMarkerBBBB | SynTokStreamWrapMarker | u32 (overwritten with BBBBBBBBh); 8 B | - | writes back into the stream | - | <0.04 | <0.06 |
| A8 | 495FC4 | SynTokCloseCmdListAndHash | = | u64 {u32 key, u32 slot / hi}; 12 B | Close (via SynCloseCmdList 496078) | stores the closed list into the device list cache (+3C20h hash slots) | list close is driver cost | 0.135 (11) | 0.175 (8) |
| A9 | 905480 | SynTokCloseListAndPushEclA9 | = | u64; 12 B | Close + ECL token 2 (SynCloseListAndPushEclTok2 1A86A2C) | list ring | each A9 splits the DIRECT list (cross-queue sync) | <0.04 | <0.06 |
| AA | 2F0E650 | SynTokCloseListIfFenceBehindAA | = | u64 fence ptr; 12 B | close + submit if the fence is behind (SynSubmitIfBehindFence 2F00A98) | - | - | <0.04 | <0.06 |
| AB | 8C16AC | SynTokReadThreeQwordsDispatch | SynTokCloseListAndPushEcl | 3 x u64; 28 B | Close, ECL token, recycle (CmdListRingCloseAndPushTokA 4900B8) | list ring | - | 0.04-0.06 | 0.062 (3) |
| AC | 2F0D634 | SynTokBeginQueryAC | SynTokBeginQuery | u32 type, u32 index, u64 heap; 20 B | BeginQuery (vfn +1A0h) | - | - | <0.04 | <0.06 |
| AD | 2F0DD80 | SynTokEndQueryAD | SynTokEndQuery | as AC | EndQuery (vfn +1A8h) | - | - | <0.04 | <0.06 |
| AE | 2F0DF30 | SynResolveQueryData | = | u32 type, u32 start, u32 n, u64 heap, u64 dst, u64 offset; 36 B | ResolveQueryData (vfn +1B0h) | - | - | <0.04 | <0.06 |
| AF | 7543BC | SynTokEndQueryTimestampAF | SynTokEndTimestampQuery | u32 index, u64 heap; 16 B | EndQuery(heap, TIMESTAMP, index) | - | - | <0.04 | <0.06 |
| B0 | 2F0D678 | SynTokBeginPipelineStatsB0 | = | none; 4 B | BeginQuery(PIPELINE_STATISTICS) (SynBeginPipelineStatsQuery 2EFB918) | c+24D4h.. | - | <0.04 | <0.06 |
| B1 | 2F0DDC4 | SynTokEndPipelineStatsB1 | = | none; 4 B | EndQuery (SynEndPipelineStatsQuery 2EFDCAC) | - | - | <0.04 | <0.06 |
| B2 | 2F0D5FC | SynTokQueryScopeBeginDefaultB2 | = | none; 4 B | query reset/begin on c+2500h | - | - | <0.04 | <0.06 |
| B3 | 2F0DD54 | SynTokQueryScopeEndDefaultB3 | = | none; 4 B | query end on c+2500h | - | - | <0.04 | <0.06 |
| B4 | 2F0D5D8 | SynTokQueryScopeBeginObjB4 | = | u64 query object; 12 B | begin (AddRef, store c+2508h) | - | - | <0.04 | <0.06 |
| B5 | 2F0DD30 | SynTokQueryScopeEndObjB5 | = | as B4 | end + release | - | - | <0.04 | <0.06 |
| B6 | 2F0D608 | SynTokQueryObjBeginSlotB6 | = | u64 object, u32 slot; 16 B | BeginQuery on the object slot | - | - | <0.04 | <0.06 |
| B7 | 2F0DD60 | SynTokQueryObjEndSlotB7 | = | u64 object; 12 B | EndQuery on the object slot | - | - | <0.04 | <0.06 |
| B8 | 2F0E45C | SynTokSetPredicationB8 | = | u64 buffer, u32 op; 16 B | PREDICATION barrier + SetPredication (2F00468) | - | - | <0.04 | <0.06 |

## 3. Shared helpers (S)

| helper | what it does | per-call cost driver |
|---|---|---|
| SynergisticDispatcherLoop 1A7C3E0 | `WaitForSingleObject(ring sem, INFINITE)`; pop the 32-B item {SynCtx*, begin, end, u8 submit}; `while (cur < end) table[*(u16*)cur](c, &cur)` through 3C02220 (one indirect call per token); on submit: flush immediate draws, SynCloseCmdList 496078, CmdListRingSubmitUpTo 4930C8; InFlight-- (locked) | one indirect call per token (35.7 k/f on the PC); handlers are short |
| SynReplayTokenRange 115750 (new name) | the same inner loop on {begin, end} (no static caller: reached through a pointer; RPF counts it as a nested replay) | - |
| SynReplayItemAndSubmit 495EFC (new name) | the loop body for one item {ctx, begin, end, submit} including the submit tail (no static caller) | - |
| SynFlushGfxStateForDraw 1180D0 + SynFlushPsoAndBindings 118604 (functor SynPsoFlushFunctorVtbl 3C332A8; twin 118758 for the topology-carrying draws) | section 0.1 | section 0.2 |
| SynUploadCbvTable 118E28 / SynFlushCbvTables 118DA8 | constants: shadow -> UPLOAD ring (full declared size), CBV per dirty slot, whole-table copy, root table | **bytes into BAR memory** (section 0) |
| SynFlushSrvTables 118C10 | whole SRV table (layout+48h descriptors) per dirty stage into ring c+9D0h, root table | CopyDescriptorsSimple bytes |
| SynFlushGfxDescTable 1188AC | samplers: 8-B keys -> PsoKeyHash -> DescTableCache (4940D8, B-tree 494AB8 / 494CDC) -> copy only on a miss | the model for F4 |
| SynCopyDescriptorSimple1 115DE8 | `dev->CopyDescriptorsSimple(1, dst, src, type)`: the per-bind staging copy (23 callers) | ~12-16 k calls/f (F5) |
| ShaderVisibleDescRingAlloc 1192B0 | allocation in a shader-visible ring heap; returns -1 plus the blocking fence when the slot to reuse is still in flight; writes one u64 ListStamp per descriptor into ring+30h[]; wrap count +4Ch | a refusal costs a list cut + `SynSubmitAndWaitFence` 945610 spin (counters c+2494h / c+2498h / c+24A0h / c+24A4h count them: free stats) |
| UploadRingAllocate 1190C0 (+ UploadRingConsumeBytes 119250, UploadRingBlockSplit 7D7204) | fence-tagged node ring over the persistently mapped UPLOAD buffer c+B28h (SynUploadRingCreate 2EF3310); recycles nodes whose fence <= completed | cheap; refusals as above |
| SynCbShadowWriteStage 1159F0, SynSetCbDataStage 698F58, SynUploadConstantsToRing 1158A0 (misnamed: writes the shadow), SynCbSlotWriteStage 1899E88 | all constant writes land in the 64-KB shadow page of (stage, slot), lazily allocated (SynEnsureCbShadowPage A397D8: allocator [c+10h] vfn+38h, size 10000h, align 10h, flags 5, double-checked under a lock at g_QpcTimer+90h) | memcpy into cached memory; the expensive copy is the later shadow -> ring one |
| SynTransitionResource 115E24 | `if (stampPtr && *stampPtr != ListStamp) { state = COMMON; *stampPtr = ListStamp }; if (state != target) ResourceBarrier(1, TRANSITION, all subresources)` | one D3D12 call per transition (45 callers); batching tested on the PC: the AMD cost is per barrier |
| SynResGetStampPtr 114C04 | walks obj+10h parents to the root resource, returns its stamp slot (+88h / +90h -> +28h) | pointer chase per bind, done before the "unchanged" early-out in 4E-55 |
| SynBindTextureSlot 115B9C | sampler (via SynTexDescCacheFindOrCreate 116608) + SRV staging copies, transition to PS / non-PS resource, dirty bits | compare-first; a miss costs 2 CopyDescriptorsSimple + 1 barrier |
| SynTokReadStateBlock128 189C0C4 | copies the 128-B record behind the stream pointer, then reads u64 sampler key, u32 view-select, u64 view | 128-B copy from engine material memory |
| PsoGetOrCreateGfx 119670 | CRC32 of the 128-B key (XOR fold when c+2490h == 0), EnterCriticalSection([c+818h]+10h), B-tree find 11976C (copies 90h B out), miss -> CreateGfxPso 9290D8 synchronously + PsoCacheInsertGfx 9B05BC (B-tree insert 9B05F8, grow 1899A54) | 1.2-1.7 k lookups/f; PCM memo exists |
| SynFlushComputeState 58BE90 | compute descriptor functor 409A618 (CmdListFlushComputeDescTable_495414, repeated while c+5C0h changes), then if c+B24h & 40h: PsoKeyHash(8 B), EnterCriticalSection, PsoComputeCacheFind 58C0D8, miss -> CreateComputePso 58C2B0 + PsoComputeCacheInsert 58BA50 (B-tree 58BAA0, trim 2EFA3A0 when full), SetPipelineState | the lookup runs after every 7C even for the same CS |
| SynFlushImmediateDraws 7A728C / SynEnsureImmediateDrawSpace 1160B0 | immediate-mode quads / triangles / lines collected in the UPLOAD rings c+2390h / c+23D8h, drawn when state changes (every state token checks c+2458h first) | the c+2458h test is in ~60 handlers (one load + branch) |
| SynCloseCmdList 496078 -> CmdListRingClose 490D5C | ClearState + Close, ECL token 0 | driver |

## 4. The async-compute sub-stream (token 90, table 3C02800, 23 entries; S)

Token 90 `{u64 ctx, u64 begin, u64 end}` replays [begin, end) with `table3C02800[*(u16*)cur](ctx + 68h, &cur)` (writers:
MergeRLR2PassExecuteImpl 153E98, RealtimeLocalReflectionPassExecute 295C44, 253F8C). ctx+68h is the async compute context
set up by AsyncComputeCtxCreateQueue 2EF8448 (own COMPUTE lists, root signature and heaps). Every sub-handler decodes its
payload and tail-calls a virtual of that object; the payload shapes mirror the main tokens:

| sub-op | handler | virtual | payload after the header | mirrors |
|---|---|---|---|---|
| 00 | 3D17FC | +10h | none | begin |
| 01 | 3D1808 | +18h | hdr.a | fence point (01) |
| 02 | 3D6260 | +20h | none | - |
| 03 | 2F149E4 | +28h | hdr.a (u8) | - |
| 04 | 829BD4 | +30h | u32 (colour), then hdr.a dwords of string | BeginEvent (A5) |
| 05 | 9AFE28 | +38h | none | EndEvent (A6) / end |
| 06 | 3D17D4 | +68h | u64 | - |
| 07 | 2F14A18 | +60h | u64 | - |
| 08 | 18E678 | +70h | none | - |
| 09 | 3D6214 | +78h | u64 | (CmdTokForwardU64ToVfunc78) |
| 0A | 3D61CC | +80h | hdr.a | - |
| 0B | 3D67F0 | +88h | hdr.a | - |
| 0C | 3D5B20 | +90h | hdr.a slot + 128-B state block (as 44) | texture bind |
| 0D | 2F14B58 | +98h | as 0C | texture bind (second kind) |
| 0E | 3D5BB0 | +A0h | hdr.a slot, u64 | buffer SRV bind (as 4E) |
| 0F | 2F14B24 | +A8h | hdr.a slot, u64 | texture SRV bind (as 56) |
| 10 | 3D6778 | +B0h | hdr.a, 3 x u64 | UAV bind (as 5E) |
| 11 | 8F35D8 | +B8h | hdr.a, u64 | - |
| 12 | 3D5988 | +C0h | hdr.a size, u32, data | SetCbData (66) |
| 13 | 3D59BC | +C8h | hdr.a elem size, u32 slot/offset, u32 index/count, data | CB array write (2C) |
| 14 | 3D4D2C | +D0h | u32 x, y, z | Dispatch (7E) |
| 15 | 2F14AC4 | ResourceBarrier on the context list (ctx+50h) | barrier blob (as 9D); 7B1B60 builds one ResourceBarrier(n) | barriers |
| 16 | 2F14948 | +E8h | u32, u32, u32, u64, u64 | CopyBufferRegion (8C) |

RPF counts the whole sub-stream as token 90's self time (PC 0.143 ms/f for 6 calls; pc2 0.070 for 4). The waste here is
structural: async compute work is recorded on the graphics replay thread.

## 5. Waste candidates ranked by likely ms on a mid-range PC (pc2 numbers; savings are E)

| rank | candidate | handlers | evidence | pc2 ms/f now (M) | likely saving (E) | same-picture fix | effort |
|---|---|---|---|---|---|---|---|
| 1 | constants pushed into BAR memory: full declared cbuffer per dirty slot | 6A, 69, 6B-6F, 79, 7E/81 (compute CBV table 4959EC) | section 0; 6A CPU 5.7 us/draw vs 0.43 on the PC | 6A+69 CPU 7.3, 7E CPU 1.2 | **3-5 ms** with F1+F2+F3 (F3 alone 1.5-3) | F1 skip unchanged writes, F2 read-extent clamp, F3 streaming stores | F3 small (call-site patch), F1 medium (9 token bodies), F2 large (offline reflection) |
| 2 | whole-table descriptor re-copies into the shader-visible heap (VRAM) | 6A, 69, 7E (CBV + SRV tables) | 6A: 2350 copies at 675 ns; group "descr copy" 2.26 ms | ~1.8 of 2.26 | **0.5-1 ms** | F4 table content cache (the sampler path already has one) | medium |
| 3 | CreateConstantBufferView per re-uploaded slot | 6A, 69, 7E | 4059/f, 409 ns (774 inside 6A) | 1.66 | follows rank 1: every skipped upload also skips its CBV (0.5-1 ms at 50 % no-ops) | F1/F2 | - |
| 4 | per-bind 1-descriptor staging copies | 44/3E-45/86/87, 4E-5F, 88 | ~16-17 k CopyDescriptorsSimple/f of 1 descriptor | ~0.4 | **0.3-0.5 ms** | F5 build tables with one CopyDescriptors at flush | medium |
| 5 | PSO lookup per state change (CRC32 128 B + critical section + B-tree); 26.8 % of SetPipelineState redundant on pc2 | 6A/69/7E via 0C-1E, 21-29 | PCM measured -0.7 ms replay on pc2 (backend.md 0.3) | inside 6A CPU | **0.3-0.7 ms** (PCM exists; the open question is conversion to fps) | PCM memo + skip same-PSO SetPipelineState | done (PCM) |
| 6 | forced compute PSO re-lookup at every compute section | 7C -> 7E/81 | 7C sets c+B24h bit 40h unconditionally | part of 7E 1.23 CPU | 0.1-0.2 ms | skip when c+80h equals the program of the last SetPipelineState in this list | small |
| 7 | one ResourceBarrier call per transition | 3C, 3D, 44, 4E-5F, 8A, 92, 1180D0 RT loop | 1095 calls/f, 1117 ns on pc2 (542 on the PC) | 1.22 | 0-0.6 ms (E): the PC test showed the AMD cost is per barrier | gpuopt Barriers (exists; measured worse on the PC and off, see docs/BENCHMARKS.md) | - |
| 8 | 3D IASetIndexBuffer 272 ns + barrier | 3D | 805/f | 0.54 | small: already skipped when unchanged | - | - |
| 9 | memset of 128 B overwritten at once (dead store) + 128-B record copy | 44, 3E-45, 86, 87, sub 0C/0D | 2.5 k/f on pc2 | 0.59 CPU (44) | ~0.05 ms | drop the memset; read the fields in place | tiny |
| 10 | SynResGetStampPtr parent walk before the unchanged-bind early-out | 4E-55 | 3.6 k/f (55) on pc2 | 0.14 CPU (55) | ~0.05 ms | compare first | tiny |
| 11 | 1 KB memset + string copy per BeginEvent / marker before the "markers on" check | A1-A5 | not in the top 30 | <0.06 | ~0.02-0.05 ms | check c+542h first | tiny |
| 12 | QueryInterface + Release per DispatchMesh | 7F | rare | - | ~0 | cache the List6 pointer | tiny |
| 13 | async compute sub-streams recorded on the replay thread | 90 | 4-6/f | 0.07-0.14 | structural (needs a second recording thread) | - | large |

**Not waste (S):** the dispatcher's token dispatch itself (one indirect call per token, ~35 k/f, short handlers); the
per-slot bind caches (they already skip same-descriptor copies); the stream format (8 dead bytes per 6A, ~15 KB/f).

**Rank 1 is the lever, and it is ReBAR-specific:** the same code is cheap on the PC because the PC's bus is 8-16x wider.
Confirm it on pc2 with `Profiler.VSForceSysUpload = 0/2` under RPF (section 0.2) before building F1-F3.
The shipped constant-upload fix (CBN/CBT) targets this path; its measured effect is in docs/BENCHMARKS.md (a gain on the
slow-link pc2 only, no change on the PC or the Ally).

## 6. Symbols, coverage, open items

- **Symbols** (block `# renderer/backend` of re/symbols/symbols.tsv; `build_exports.py` clean): 72 new rows. They cover the
  replay-side helpers that had no name (dispatcher sub-replays 115750 / 495EFC, pass-stack / viewport / state-setter bodies,
  PSO and descriptor-cache B-tree internals, CRT clock), the async sub-stream table `g_AsyncSubTokenTable` 3C02800, and 21 of
  its 23 sub-handlers (the other 2 were already named). One heuristic name was replaced in place (189BFB0 ->
  SynReadCursorCopyLocation). "Real role (01b)" notes were appended to 7 rows whose old names mislead: 114750 (binds the PS,
  not a root signature), 1164C4 (binds the VS), 1158A0 (writes the shadow, not the ring), 114D18, 45A020, 118E28, 115A70.
  The 185 handler names stay as they are because the RPF logs and backend.md section 8 key on them.
- **Coverage of the replay side:** the call closure from the 185 handlers plus the dispatcher loop, the draw functor
  3C332A8, the compute functor 409A618 and the sub-stream table holds 384 functions (~63 KB counted). All 384 now have
  real names (was 314; the PSO-cache internals among them count toward package 02).
- **Package 01 by bytes:** 11.76 % -> 11.96 % (renderer_progress.py). The replay side is only a small part of the package:
  BACKEND's 4931 partition functions are 345 core functions (seed + closure, 88 KB, now ~79 % named) and 4436 region-fill
  functions (1.40 MB, 7 % named). The region-fill functions only sit in the same address range ("GFX"); none of them is
  reachable from the replay thread. Raising the package % means naming that fill, which this decompile does not cover.
- **Missing from the export:** 115 of the 384 closure functions (mostly 2F0D5xx-2F0E6xx jump stubs and small tail-called
  bodies). Every one was disassembled from `re\Nioh3.dump.exe` with capstone instead. Nothing is blocked.
- **Open (needs a run, not more reading):** (1) pc2 `VSForceSysUpload = 0/2` under RPF to confirm the BAR-store cause of 6A;
  (2) counters for F1 (no-op CB writes) and for the table-cache hit rate of F4; (3) read-extent census for F2 (offline, over
  the shader cache); (4) who re-dirties the CBV/SRV masks after a list cut (needed for F1 correctness: 9C9EF0 / 490E50
  suspected, not traced); (5) the vtable class of the async context object at +68h (the 409A3B0 backend table matches the
  offsets but is the optional backend; sub-op meanings above come from payload shapes).
