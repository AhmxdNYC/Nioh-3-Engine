# renderer/frame-overlap: overlapping frame N's D3D12 replay with frame N+1's recording (Stage 1 lifetime audit, Stage 2 design, gate)

2026-10-10. Nioh3.exe Steam v2.02, RVAs (Ghidra base 0x7FF74F410000). Builds on backend.md (token stream, replay thread, the
per-frame drain; read that first). Tags: **S** static (decompile / disassembly, cited RVA), **M** measured, **E** estimate.

Status: **experimental, off by default** (`[GpuOpt] FrameOverlap = 0` installs nothing) and not merged as a feature. Measured
native effect: pc2 +0.4 % (section 7.3), PC about 0 while GPU-bound (section 7.1); the Ally was not run. The larger percentages
in sections 0 and 5.3 are design estimates (E), not results. See docs/BENCHMARKS.md.

Branch `frame-overlap` (from `renderer-backend` 384844e). Scripts (scratch, not committed):
`tokload.py` (payload loads per handler, capstone), `tokemit.py` (token emitters: `mov dword [..], imm` near a +3C40h write).

## 0. Answer (Stage 1)

1. **Token payloads are not the main risk (S).** Of the 185 handlers, ~95 carry only inline data, ~60 carry pointers to
   persistent engine objects (resources, views, programs, layouts, query heaps) or to dispatcher-owned rings that are already
   fence-retired on the replay side. **12 handlers point at per-frame state that the main thread rewrites or resets at the frame
   boundary** (section 2): upscaler callback arguments (0x8F), sub-stream ranges (0x90), PagePool / KtglCache tokens (0x82/83/95/96/97/98/9A),
   frame-slot-indexed queries (AC/AD/AF/B0/B1), the sync-point table (A8/AA) and CPU state write-backs (0x9E). Each has a known fix.
2. **The real risk is the frame-boundary protocol (S).** The drain is not only "the stream is reset". Between the drain and the
   first token of N+1 the main thread runs ~15 steps that touch dispatcher-owned state (SynCtx list ring, query heaps, the frame
   slot index +4730h read by the dispatcher, the back buffer, the sync-point table) and the KtglCache / PagePool frame protocol
   (texture streaming), all relying on "the dispatcher is idle here". Section 3 lists them. Overlap therefore means **moving the
   frame tail (Present and everything after the drain) onto the replay thread as an in-order item**, splitting the frame slot
   into a record slot and a replay slot, and re-plumbing the PagePool frame-begin work. Double-buffering the stream alone gives
   zero overlap and data races.
3. **Two main-thread GPU waits inside the frame can eat the gain (S, to measure):** sync points (`5F17C` handle + `3D2B90` wait:
   kick, spin until replayed, then wait the list's GPU fence). MeshletUpdateSceneBuffers 4BBEC uses one **every frame** to reuse
   its scene-buffer set (2 sets, `+16A0B0` parity, 08-meshlet.md). With overlap the main thread reaches it earlier relative to
   the GPU, so it waits longer unless the sets go to 3. The engine times these waits itself (GfxDevice+3CD0h handle wait,
   +3CE8h sync-point GPU wait); BKS now reports them ("BKS: fov:", built, section 6). **This is the go/no-go gate for Stage 3.**
4. **Stage 1 verdict: GO for design + gate (gate cleared 2026-10-10, pc2 FOV1: 0.00 ms/f main-thread GPU waits -> Stage 3 GO).**
   Effort estimate **5-7 weeks**, risk medium-high, concentrated in the PagePool / KtglCache frame protocol and FG / Streamline
   present handling. Design-time estimate (E) for pc2 if the gate is clean: +15-25 % (the moved tail adds ~0.5-1.5 ms to the
   replay thread); Ally ~0 (GPU-bound); PC RX 9070 some, at high fps only. **The prototype measured +0.4 % on pc2** (section
   7.3): the estimate assumed the drain was pure waste, which it is not.
5. CacheFlushOnGfx 2EF0938 is **not** run every frame (gpu-tiles.md). Its only per-frame caller,
   RenderFrameBeginCacheFlush 32FE938, is gated by MeshletManagerIsEnabled && 4A56F8, which is true only when the render size
   changed (`dev+40CCh/40D0h != cached +134h/+138h`) or a force bit is set; the other callers are KtglCacheShutdown 1896AC8 and
   the renderer dtor 32F0420.

## 1. What the main thread does at the frame boundary (S)

`GfxDevicePresent 3D62E4` (main, after the graph walk), then `RenderFrameBegin 4A529C` of the next frame:

| # | step | function | touches | overlap fix |
|---|---|---|---|---|
| E1 | final kick, **drain spin**, stream reset to base, generation +3C60h += 1, stats snapshot +3D00h | CmdStreamEndFrame 418B94 / CmdStreamFinalKick 418C70 / WaitDispatcherDrainSpin 418F40 | stream +3C30h..+3C68h | **double-buffer** the stream (two halves of the +3C28h buffer); the 0xA7 handle scheme (+3C58h, generation nibble, `HandleSlotIsNotFreed 58C434`) must carry the half |
| E2 | reset every active sub-stream (+74D8h[+3E28h], `+55h` active) to its base | 253F2C -> 2543A0 | sub-stream write ptr +28h | **double-buffer** each sub-stream (or defer the reset to after replay of N: workers of N+1 must not write first) |
| E3 | timestamp ResolveQueryData for the frame slot (bitset +4788h/+478Ch/+4790h[slot], heap +4768h -> readback +4770h) on SynCtx's list | 3D62E4 body | SynCtx list +590h, slot +4730h | move to the **replay-side tail** with the replay slot |
| E4 | back buffer -> PRESENT (state 0) | SynTransitionResource 115E24 on SynCtx | resource state in the object, SynCtx | tail |
| E5 | close + submit SynCtx's list, spin until the ECL thread is idle; returns the frame fence -> +4320h[slot] | GfxSubmitFrameCmdList 252758 | CmdListRing | tail |
| E6 | PSO-cache maintenance when SynCtx+7D0h/+7D1h: may **wait the GPU** for this frame's fence, then prune the B-tree | 8CDA94 | SynCtx +708h | tail (rare) |
| E7 | KtglCache frame end: tag pending cache work with this frame's fence, wake the tile-mapping job (+758h/+788h/+6F8h, event +728h) | 12682C | PagePool +6A8h/+608h/+A8h/+8F0h, SRW +CC0h/+CD0h | tail; **ordering against E12 is the hard part** |
| E8 | Present (direct, or hand-off to the present thread when +3F80h), Streamline / Reflex callbacks 3FE0998/9A8/9B0 | 3D62E4 body | swapchain +4278h | tail (or the engine's own present thread) |
| E9 | slot = (slot+1) % 3, **wait slot fence** (GPU done with N-2), sync-table frame++ and reset (8BEDC8, +3C1Ch), SynPipelineStatsReadbackSlot(SynCtx) 9AFED0, timestamp readback copy, GetTimestampFrequency, **Reset allocator +4290h[slot]**, CmdListRing recycle 4920E0 on SynCtx | GfxFrameSlotAdvance 8BEC48 | slot +4730h, SynCtx | **split**: record slot ++ and the N-2 fence wait stay on main; allocator reset, list-ring recycle, record rebuild, readback move to the tail with the replay slot |
| B1 | latency markers, XeLL sleep / Reflex, Streamline frame token | 4A529C head | frame ids | keep; frame ids must match E8's markers |
| B2 | RenderFrameBeginCacheFlush: sync point (5F17C + 3D2B90 = **waits the GPU**), CacheFlushOnGfx x2 (uses SynCtx from main, ECL tile tokens, handle drain) | 32FE938 / 2EF0938 | SynCtx, ECL ring, PagePool | only on a render-size change -> **fall back to a full drain that frame** |
| B3 | present-pending spin (+7728h) | SpinWaitFlagSleep0 7D9E90 in WaitSubmitThread 7D9B6C | present thread | wait for present of N-1 only |
| B4 | PagePoolFrameUpdate: SRW +D08h barrier, stats +8F8h -> +A10h, PagePoolScanAll 2522C4, TextureBlockFrameUpdate 127334, **tok 0x95** | 252510 | PagePool, TexBlock (+74E0h) | see section 4 |
| B5 | `GetCurrentBackBufferIndex` -> back-buffer RT object +4698h[i] into the first pass record (CmdStreamBeginFrame 7D9C78) | WaitSubmitThread | swapchain | **predict** (i+1) % BufferCount (flip model is round-robin); verify vs the FG proxy swapchain |
| B6 | PagePool page ops: tok 0x82, per entry 2EF0D20 (records page tokens), tok 0x83, under SRW +D08h exclusive | 7D9C24 | PagePool +D28h list, +1128h count | section 4 |
| B7 | meshlet frame begin: set swap, **scene-buffer upload with a sync-point GPU wait** for the set's previous use | MeshletFrameBegin 5D214 / MeshletUpdateSceneBuffers 4BBEC | 2 sets, UPLOAD heap | **3 sets** (or accept the longer wait; the gate measures it) |

## 2. Lifetime table: 185 handlers (S)

Classes. **I** = inline payload only (values, constant blobs, strings, barrier blobs): safe once the stream is double-buffered.
**P** = pointer to a persistent engine object (resource / view / program / layout / render-state / query heap / PSO key): safe as long
as the object is not destroyed at the boundary; RTs are session-lifetime (05-frame-graph.md 4), VideoMemoryKeeper transients only
alias GPU memory (0x9D aliasing + 0x9F discard, GPU-ordered), resizes take the B2 fallback. **R** = dispatcher-owned ring / state,
retired on the replay side by fence (UploadRingAllocate 1190C0, ShaderVisibleDescRingAlloc 1192B0, immediate VB c+2458h,
CmdListRing): unchanged order on the replay thread, so safe; a ring indexed by the frame slot needs the replay slot.
**T** = transient per-frame state the main thread rewrites / resets at the boundary: needs the listed fix.

| handlers | class | referenced data | lifetime | fix |
|---|---|---|---|---|
| 01 06 08-0F 10-15 17-20 (state setters), 04 05 (clears), 09 19 1A-1F 84 85 | I | inline values | stream | none |
| 2B 2C-36 37 38 39 3A 60-67 | I + R | inline constant blobs -> shadow c+BB0h/D70h/EC0h -> upload ring 1190C0 | stream / fence-retired ring | none |
| 46-4D (clear texture slot) 70 71 72 7C 7D 99 A6 A7 | I | none / inline | stream | none |
| 69-6D 7F (draws, mesh) 7E (dispatch) | I + R | inline args; state flush uses descriptor ring, PSO cache | stream / ring | none |
| 73-78 (immediate quads / tris / lines) | I + R | inline vertices -> immediate VB c+2458h | stream / dispatcher ring | none |
| A0-A5 (markers, events) | I | inline strings / colour | stream | none |
| 9C 9D | I (+P) | inline barrier blobs (resource ptrs inside are persistent / VideoMemoryKeeper) | stream | none |
| 00 02 | P / **T** | DSV / RTV handles of the pass record | RT session-lifetime; the **back buffer** index comes from B5 | predict the index (B5) |
| 03 07 16 44 45 86 3E-43 87 88 4E-5D 55 5E 5F | P | texture / buffer / UAV objects, CPU descriptor handles | session / streaming objects (state tracked in the object) | none (streaming frees are fence-polled, GpuMemPollFence 630FC4) |
| 21 22 23 24-28 29 2A 3B | P | render-state object, shader programs, layout, root signature | session | none |
| 3C 3D 79 | P | VB / IB objects (incl. GpuBuffer dynamic buffers written through UPLOAD staging) | object persistent; contents slot / ring | none (see 06-resources per-frame section for the UPLOAD ring reuse distance) |
| 68 6E 6F 80 81 8A 8B 8C 8D 91 92 93 94 9B 9F | P | copy / indirect-arg / barrier / discard resources | persistent or aliased transient | none |
| 7A 7B AB 89 | P | handles, 3 qwords to 8A00B8 with list-ring sub-objects, GPU-driven batch | session / R | none (verify 89) |
| 8E | P | global marker interface [4BD09C0] | session | none |
| AC AD AF B6 B7 B4 B5 | P + **T** | query heap / query objects (AddRef'd); **timestamp index = slot * count + i** | heap session; index per frame slot | replay slot (E3/E9 split) |
| B0 B1 B2 B3 + list begin 4933F4 / resolve 48F178 / SynCmdListOnReset 492220 | R + **T** | pipeline-stats ring c+24D8h **indexed by GfxDevice+4730h** read on the dispatcher | per frame slot | the dispatcher must read the replay slot (6 read sites) |
| B8 | P | predication buffer + offset table | object | none |
| A8 | R + **T** | writes the list fence into sync table [+3C20h + (frame % +3C18h) * 18h] if the handle's frame still matches | table rotates in E9 | table ring must keep >= 3 frames (count logged by BKS fov) |
| AA | **T** | pointer to a u64 inside the sync-table ring (2FCEE4 / 18C313C) | rotates in E9 | same as A8 |
| A9 01 | R | close list + ECL wait token / fence point | dispatcher | none |
| 8F | **T** | `fn(list, arg)`: DLSS 6B64D8 arg = global 39D44F8, XeSS 18D66E0 global 39D3DB0, FSR 341F704 global 39D3E88 (main rewrites jitter / sharpness right after the token), FF2759 / 32F58C8 object args | **one global per upscaler, rewritten every frame** | **copy**: snapshot the arg block per frame (redirect the token's arg to a 2-deep copy) |
| 90 | **T** | sub-stream {object, begin, end} (+74D8h objects, handler table 3C02800) | reset at Present (E2) | **double-buffer / defer reset** |
| 9E | **T** | `*(u32*)p = v` into an object state word (GPU culling setup 298990, 2998B0, 454F7C, 922DB0, 38C730C: obj+20h, values 80h / C0h) | written at replay; main polls it | keep order; verify each reader tolerates the write one frame later (5 emitters) |
| 95 | **T** | PagePool (+61A0h) shared-locked update 7F64DC: page lists, cache records +590h/+6D0h | PagePool frame protocol | section 4 |
| 96 97 98 9A 82 83 | **T** | PagePool touch / tile unmap / push record {u64,u32} into +6E0h (+6F4h count, under SRW +CF8h **shared**) / tile map / batch begin-end | consumed by main-thread frame begin (B4/B6, CacheFlushOnGfx 2EF0938 reads +6D0h under shared too) | section 4 |

Counts: I ~95, P ~60, R-only ~10, **T 20 handlers in 8 groups**.

## 3. Reset points (S)

| object | reset / reuse point | today guarded by | with overlap |
|---|---|---|---|
| command stream (+3C30h) | CmdStreamFinalKick 418C70 at Present | drain 418F40 | two halves; main waits only for replay of N-1 before reusing a half |
| stream wrap (near end) | KtglCmdStreamFlush -> 418E74 (0xA7 + 0xAAAAAAAA marker) + 7D99FC handle spin | handle | unchanged (mid-frame) |
| sub-streams (+74D8h) | 2543A0 at Present; wrap via 418E74 + 7D99FC | drain | double-buffer |
| dispatcher ring (+3BE0h) | ring, consumer semaphore | - | unchanged |
| CmdListRing / allocators | recycle 4920E0 (fence), grows on demand | fence | unchanged, but called from E9 on main today -> tail |
| per-slot DIRECT allocators +4290h[3] | Reset in E9 | slot fence | tail |
| upload ring 1190C0 / descriptor ring 1192B0 / immediate VB | fence-retired on replay | fence | unchanged |
| UPLOAD heap ring (+4678h/+4680h, 62D7F0 alloc, 878B70 free) | **freed immediately at unmap**; reuse distance = ring capacity | ring size | +1 frame of pressure; check capacity (06-resources) |
| timestamp heap / readback (+4768h/+4770h) | per slot, resolved E3, read back E9 | slot fence | record/replay slot split |
| pipeline-stats ring c+24D8h | per slot via +4730h | slot | replay slot |
| sync-point tables (+3C20h, count +3C18h, frame +3C1Ch) | E9 rotates and zeroes the new frame's table | drain | needs >= 3 tables (logged) |
| meshlet scene-buffer sets (2) | set swap in MeshletFrameBegin, guarded by a sync-point GPU wait | GPU fence | 3 sets or longer waits |
| PagePool records +6D0h | consumed in CacheFlushOnGfx / 7F64DC; pushed by tok 0x98 | drain + shared SRW | section 4 |
| frame slot fences +4320h[3] | written E5, waited E9 | - | main waits N-2 (written by tail N-2, done before tail N-1) |

## 4. The PagePool / KtglCache frame protocol (S, the hard part)

Writers and readers of the texture-streaming page pool (`GfxDevice+61A0h`) at the boundary today:
main B4 PagePoolFrameUpdate (snapshot, scan, TextureBlockFrameUpdate, emits 0x95), main B6 7D9C24 (page ops between 0x82/0x83),
replay 0x95 (7F64DC under +CF8h shared: 7F6904, 2F15A34-style record consumption), replay 0x96/0x98/0x97/0x9A mid-frame, main E7
12682C (tag with the frame fence, wake the job), the tile-mapping job threads (COPY queue, SRW +CC0h). Today the drain makes
"replay of N" -> E7(N) -> B4/B6(N+1) a strict order. Two ways to keep it:
- **(a) run B4 and B6 on the replay thread** as part of the tail, after E7, with their token emissions (0x95, 0x82/83 + page tokens)
  turned into direct handler calls on SynCtx (they are the first thing N+1 replays anyway). Main-thread readers of PagePool state
  during recording (TextureStreamUpdate 1E7050, TextureStreamRequestTiles 1290B8, cache insert/evict under +CF8h exclusive) then
  run concurrently with these frame updates instead of after them: needs a reader audit (~20 functions) and probably taking
  +CF8h exclusive around the moved block. One frame of extra streaming latency (benign).
- (b) keep them on main and make main wait for "tail N done" before B4: no overlap (B4 is at the very start of the frame).
Design choice: (a).

## 5. Stage 2 design (FO, `[GpuOpt] FrameOverlap = 0`)

Inputs since Stage 1: pc2 FOV1: main-thread handle wait **0.00 ms/f**, sync-point GPU wait **0.00 ms/f**, present
thread 1, drain ~6.0 ms/f, PresentMon CPUBusy 20.45 / GPUTime 17.64 ms. 06-resources.md 1: every GPU ring retires per command-list
fence, staging is synchronous, only the token stream is reset at frame end, 3 frame slots, transient RTs single-copy and DIRECT-only.

### 5.1 Shape: "tail on the replay thread + a mid-frame barrier"

```
 main      | rec N .........................| E | rec N+1 (part 1) ......| B | rec N+1 (part 2) ...............| E |
 dispatcher|    replay N ......................| TAIL N |  replay N+1 part 1 ...  replay N+1 part 2 ........| TAIL N+1|
                                                ^ Present N                     ^ B = barrier: main waits "TAIL N done"
```
- **E (main, frame end N), replaces GfxDevicePresent 3D62E4** (patched at its only per-frame entry, the `jmp 3D62E4` at the end of
  GfxDevicePresentVfn 5DAF3; `param_2` is always NULL there -> the default present params at dev+4220h):
  1. wait TAIL N-1 done (normally already true), then wait the GPU fence of frame N-2 (the same CPU lead as stock: stock waits it in
     GfxFrameSlotAdvance right after Present).
  2. **E7 on main with a forward fence** (section 4): reimplementation of 12682C in which the cache job's fence tag
     (126744 writes `{3, &fence, value}`) points at the ASI's own `ID3D12Fence` `g_ovFence` with value N instead of the frame-slot
     fence `[dev+4310h] / +4320h[slot]` (not known before TAIL N submits). TAIL N signals `g_ovFence = N` on the GFX queue right
     after the frame's last list, so the COPY-queue `Wait` of the tile job means exactly "GPU finished frame N", as stock. Keeps the
     stock order E7(N) -> PagePoolFrameUpdate(N+1) -> 7D9C24(N+1) on the main thread.
  3. sync-point table advance (first half of 8BEDC8: `+3C1Ch++`, zero the new table) on main, same position as stock.
  4. stream: final kick exactly as stock (stretch, submit = 1), then a **0x8F token `{TailFn, &frame[N & 3]}`** in its own stretch
     (submit = 0), then **switch halves** (below) instead of drain + reset, generation += 1, the stock stats bookkeeping
     (+3C84h / +3D88h / +3C74h, the CmdStreamEndFrame snapshot +3D00h and zeroing of +3C78h..).
- **TAIL N (replay thread, inside the 0x8F handler, list L2 open exactly as stock main sees it):** the stock body of
  GfxDevicePresent after CmdStreamEndFrame minus the parts moved to main: E3 timestamp resolve, E4 back buffer -> PRESENT (index
  from the real GetCurrentBackBufferIndex = frame N's, checked against main's prediction), E5 GfxSubmitFrameCmdList (close + submit
  + ECL idle), slot fence +4320h[slot], `GfxQueue->Signal(g_ovFence, N)`, E6 8CDA94, E8 present hand-off (present thread when +3F80h,
  else direct Present + Streamline callbacks, as stock), E9 GfxFrameSlotAdvance without the +3C1Ch part (slot ++, slot-fence wait,
  9AFED0, timestamp read-back, allocator +4290h[slot] Reset, list-ring recycle 2EFB73C), +7708h timer. Then `tailDone = N`.
  Everything that touches SynCtx / the slot index / the allocators stays on the replay thread, so it never races (the slot index
  +4730h is read by the replay thread at every list reset, SynCmdListOnReset 492220: it must only change between frames there).
- **B (main, barrier) in frame N+1:** at the first node whose global index (R+1EE568h) >= K, via the Renderer vfn+130h slot
  (transient activation 113150, called before every node by FrameStepRender 112DD8), and at the latest at E of N+1: wait
  `tailDone >= N`, then reset the sub-streams (E2, 2543A0 per active +74D8h entry) that stock reset at Present. Main never writes a
  sub-stream before B (checked: write pointer unchanged since E, else log + fallback).
  **Everything main does after B is in the stock situation** (frame N fully replayed, presented, slot advanced). K is the knob:
  `Tuning.FO_Pct` (live, % of last frame's node count, default 25, 0 = no overlap but the tail still runs on the replay thread
  = the correctness arm P0). Late-frame code (upscalers' 0x8F globals, FG, UI glyph atlas, post) is always after B.
- **Frame begin N+1 (before B), call-site patches in WaitSubmitThread 7D9B6C:** skip the present-pending spin (TAIL owns present
  ordering); **predicted back-buffer index** `(idx(N) + 1) % BufferCount` (flip model and vkd3d / dxvk DXGI are round-robin; TAIL
  verifies every frame); PagePoolFrameUpdate and 7D9C24 run under the **PP lock** (below).
- **PP lock** (one SRWLOCK of the ASI): main takes it around E7 / PagePoolFrameUpdate / 7D9C24; the replay thread takes it in
  wrappers around the PagePool token handlers 0x82 0x83 0x95 0x96 0x97 0x98 0x9A (handler table 3C02220 patched). This restores
  the mutual exclusion the drain gave (the CPU page-table mirror +590h, the +6A8h job ring and +D28h list are touched by both sides
  without a common engine lock). Order effect: PagePool tokens of frame N that replay after E7(N) see frame stamp N+1 and are
  tagged behind fence N (more conservative), maps arrive up to one frame later (streaming latency, no visual hazard).
- **Handles:** HandleSlotIsNotFreed 58C434 and HandlePoolSlotIsLive 58B76C (entry jmp) accept the previous generation while
  TAIL N is not done (marker still 0xAAAAAAAA in the other half = not replayed); stock behaviour for everything else.
- **Stream halves:** base +3C30h stays the full buffer start (handles stay `offset from base | gen`), `end` +3C38h = end of the
  current half (2 x 24 MB on pc2: 10 MB/frame). A mid-frame wrap (stock: marker + wait + reset to base) still works; at E, if the
  frame's data spans both halves, that frame takes the stock drain path.

### 5.2 Fallbacks (hard, all to the stock code path)
Key 0 (default) = no patch at all. With 1: per-frame stock path (drain) when: RenderFrameBeginCacheFlush runs (render-size change;
patched call waits tailDone first), device teardown / reset paths 2EE8B70 / 2EEC4C8 (wait tailDone, then stock), timestamp or
pipeline-statistics queries in use (+4788h range or SynCtx+24D4h non-zero: FO off for the session, logged), back-buffer prediction
mismatch (FO off), sub-stream written before B (FO off), frame data spanning both halves, TAIL watchdog (> 500 ms: log + wait).
Self-check of every patched byte run at boot or nothing is installed.

### 5.3 What each device should gain, and why (E)

| device | bound today | what FO removes | expected avg | expected 1 % low / p99 | latency |
|---|---|---|---|---|---|
| **pc2** (5600X + 6650 XT, ReBAR, Windows) | CPU: main 20.5 ms + drain 6 ms = 26 ms; GPU 17.6 ms | the drain: main records N+1 while the replay thread finishes N | 26 -> ~20.5-21.5 ms: +20-25 % (E; measured +0.4 %, section 7.3) | better: replay-side spikes (PSO creation on the dispatcher, ring-full submit spins, tile cache work in tok 0x95) no longer stall main up to K; p99 improves at least as much as the mean | per frame pipeline unchanged; main starts N+1 up to ~6 ms earlier, frames 5-6 ms shorter: input-to-photon about equal (+-3 ms) while CPU-bound |
| **Ally** (25 W, Proton / vkd3d-proton) | CPU at low TDP: both threads slower, and the frame tail is heavier under vkd3d (ExecuteCommandLists = vkQueueSubmit through the ECL thread, the ECL idle spin in GfxSubmitFrameCmdList, Present = vkQueuePresentKHR) | the same drain **plus** the tail (submit spin + present hand-off) leaves the main thread | +10-25 % (E, not run) at 25 W when CPU-bound (the larger the tail, the larger the gain); ~0 at settings where the GPU binds | the vkd3d submit / present hitches move off the main thread: 1 % lows should gain more than the mean | as pc2; when GPU-bound keep MaxLatency so the extra CPU lead does not queue a frame |
| **PC** (RX 9070, Windows) | GPU-bound at the test settings (~100 fps); CPU-bound at high fps / low res / FG | drain share at high fps (replay thread ~ main) | **0 when GPU-bound** (measured about 0, section 7.1), +10-20 % (E) where CPU-bound | CPU-bound spikes as pc2 | GPU-bound: auto-off (slot-fence wait > 1 ms/f on main -> K = 0) so no added queueing |

Portability: no Windows-only mechanism. The forward fence is a plain ID3D12Fence: `Signal` on the GFX queue after the frame's
lists, `Wait` on the COPY queue issued by the engine's job (vkd3d-proton: timeline semaphores, wait-before-signal supported).
Queue calls from the replay thread are already the stock pattern (ECL thread). The back-buffer prediction relies only on the
documented round-robin GetCurrentBackBufferIndex (DXGI flip model, dxvk/vkd3d swapchain, FSR / Streamline proxies) and is
verified every frame.

### 5.4 Test plan (Stage 3)
1. Offline: harness with a fake engine (stream halves + handle generations + tail ordering + PP lock + fallbacks), test_patterns.
2. pc2 fastab (one launch, tour mode, CPU-bound spot): items `Tuning.FO_Pct=0/25` (P0 tail-only vs overlap) and FrameOverlap
   boot arms (BASE vs FO) by abba; fps, frametime p99 / 1 % low, PresentMon AllInputToPhotonLatency; screenshot diff A vs B at
   fixed spots; 30-min tour crash / device-removed / log-error watch.
3. **Ally gain test** (not only no-harm): 25 W, test settings, same fastab items; report avg, 1 % low, latency.
4. PC no-harm at the test settings (GPU-bound) + one CPU-bound config (FG off, low res) for the gain.

## 6. Gate built: BKS "fov:" line (1.7.58-fov1, branch frame-overlap)

`[Profiler] Backend = 1` now logs a third line per window:
`BKS: fov: handle wait X ms/f (n/f, max) | sync-point GPU wait X ms/f (n/f, in N % of frames, max) | stat3CB8 X ms/f | sync tables
T frame slots S sub-streams U present thread P`, from the engine's own per-frame snapshot (+3D40h..+3D80h, copies of the live
stats +3CB8h / +3CD0h (7D99FC) / +3CE8h (3D2B90)). No hooks. Harness: `tests/run_bkstats_harness.py` ALL PASSED (fov values
reproduced); `tests/test_patterns.py` ALL 245 SITES OK.

**Gate rule:** with the CPU-bound pc2 spot, if sync-point GPU wait < 0.5 ms/frame and sync tables >= 3, the overlap ceiling stands
(Stage 3 go). If the sync-point wait is several ms (main already waiting for the GPU every frame), the prize shrinks by that amount
and B7 (3 scene-buffer sets) becomes step 1.

## 7. Stage 3 prototype: FO 1.7.59-fo1 (branch frame-overlap fc1fad8)

`src/frameoverlap.{h,cpp}`; `[GpuOpt] FrameOverlap = 0` (default: nothing installed), 1 = installed + ON, 2 = installed + OFF;
live `Tuning.FO = 0/1` (sampled at the next frame end), `Tuning.FO_Pct = 0..90` (barrier position). Log: `FO: frame overlap
installed (13 code sites, 7 token handlers, 2 vtable slots)`, `FO: armed: stream halves ...` at the first overlapped frame,
`stats/s FO:` every 30 s (FO / stock frames, barrier wait, GPU-lead wait, TAIL time, PP-lock waits, skipped sub-stream resets,
previous-generation handle checks, back-buffer checks), `FO: OFF for this session (reason)` on a hard fallback.

| patch (v2.02, all byte-checked before anything is written) | what |
|---|---|
| `jmp` 5DAF3 (GfxDevicePresentVfn -> 3D62E4) | E (FoPresent): stock path or the overlapped frame end |
| `jmp`/`call` 30A6931, 32FAE39, 32FBDBE (the other GfxDevicePresent callers) | wait TAIL, then stock |
| `call` 7D9B91 / 7D9BA3 / 7D9C07, `call [rax+120h]` 7D9BC7 (WaitSubmitThread) | present spin skipped after an FO frame, PagePoolFrameUpdate / 7D9C24 under the PP lock, predicted back-buffer index |
| `call` 4A53D0 (RenderFrameBeginCacheFlush), 2EE8B92 (CmdStreamEndFrame in 2EE8B70), 2EEC4F9 (teardown spin) | wait TAIL first (stock situation) |
| entry 58C434 / 58B76C (12-byte absolute jmp, functions rebuilt) | handle checks with the previous generation |
| Renderer vtables 40D4A78 / 3AAF600 slot +130h (113150) | barrier B at node index >= K |
| token table 3C02220 entries 82 83 95 96 97 98 9A | PP lock around the PagePool handlers |

Engine functions the TAIL / E call (unchanged engine code): DispatcherRingPush 25426C, SynTransitionResource 115E24,
GfxSubmitFrameCmdList 252758, 8CDA94, SpinWaitFlagSleep0 7D9E90, GfxWaitFenceValue 7D9E1C (own events), 9AFED0, 2EFB73C,
KtglCache pieces 1263F0 1264F0 126480 250650 12645C 126744 1280C8 126984 126A40 1269D4, 253F2C; D3D12 / DXGI vtable calls as stock
(ResolveQueryData, Map / Unmap, GetTimestampFrequency, allocator Reset, Present, GetCurrentBackBufferIndex) plus
`GfxQueue->Signal(g_ovFence, N)`.

Offline proof: `tests/run_frameoverlap_harness.py` ALL PASSED - a fake replay thread that replays a sequence-numbered token stream
slower than it is recorded (any token overwritten before replay breaks the sequence): 40 overlapped frames with zero sequence
errors, every TAIL after its frame's tokens, sub-stream resets only after TAIL N, PagePool handler vs main PP work never
concurrent, allocator reset per TAIL, KtglCache tag = {forward fence holder, frame number} and the real D3D12 fence reaching it
on a real queue; a frame that wraps in the high half and runs past the middle takes the stock path and overlap resumes; live
switch off / on; a previous-generation 0xA7 handle is "not freed" while TAIL is pending and "freed" after; an injected
back-buffer index jump switches FO off for the session. `tests/test_patterns.py` ALL 245 SITES OK, `run_bkstats_harness.py` ALL
PASSED.

Runs: pc2 FO1: boot FrameOverlap = 2, live `Tuning.FO = 0/1` and `Tuning.FO_Pct = 0/25` ABBA x4 at the
CPU-bound Continue spot, screenshots per arm, 10-min FO = 1 watch.

fo2..fo6 (handle stub, slot split, KtglCache frame end on the TAIL, sub-stream halves): see the commit messages 5253423..37806b7.
fo6pc: 3556 FO frames clean, then `back-buffer prediction mismatch (predicted 0, real 2)` -> session OFF -> 0 frames, watchdog,
`cache SRW +CF8h held 100 %`.

### 7.1 fo7 (1.7.59-fo7, branch frame-overlap 350738c..): the fo6pc hang, back buffer, watchdog

PC test vehicle: live `Tuning.FO_Test` (1 session OFF from the TAIL, 2 wrong prediction + OFF = the fo6pc sequence, 3 / 10
5.6 s / 8 s main stall -> watchdog, 4 wrong prediction, 5 / 6 switch off / on, 7 stall with the watchdog suppressed, 8 the
watchdog's restore while rendering, 11 the restore in groups with a frames check per group, 9 re-arm, **100 = self-test of all**,
each step checks the engine frame counter +3C1Ch). Watchdog: thread dump of every game thread (RtlVirtualUnwind; Nioh3 RVAs),
KtglCache / FO lock words, `GetDeviceRemovedReason`, at 5 s and 15 s. Root causes, all found in game with it:

1. **A frame recorded for a non-current back buffer removes the D3D12 device (the fo6pc "hang").** Forced wrong prediction with
   FO staying ON (no switch at all): 0 frames within 2 frames; main in FrameStepRender 112F6E -> ... -> 4DF64
   `ID3D12Resource::Map` failed -> GfxGetDeviceRemovedReason -> fatal 18956D0 -> RaiseException -> crash dialog modal loop
   (USER32); the tile job (492310 -> 4F610 -> 4DF64 -> 18956D0) parked in the same handler **holding +CF8h shared** (lock word
   0x11) = fo6pc's "CF8h held 100 %". Reason code (later runs) 887A002B ACCESS_DENIED: the GPU wrote a compositor-owned buffer.
   The FO->stock switch was never the problem (live switch off/on and TAIL-issued OFF pass). **Fix: no predicted index reaches
   the GPU unverified.** Frame N+1's tokens replay only after TAIL N, so TAIL N waits for present N to retire and reads frame
   N+1's real index (per-frame CAS handshake `FrameRec.bbState`): main uses it if TAIL N was first; else main predicts (newest
   real index + one rotation per frame since) and TAIL N verifies before anything of N+1 replays; a wrong one is fixed by
   presenting the current, PRESENT-state buffer again (one repeated older image) until the chain stands on the predicted index
   (max 6, then session OFF: a chain that will not rotate is the one case left). TAIL presents with params snapshotted at E;
   swapchain / resource overrides take the stock path. Why fo6 drifted: the old predictor was a running +1 per frame begin,
   wrong after any frame end without a frame-begin index read (or one non-rotating present); the anchored predictor is immune
   to the first, the verify step to both. PC self-test: forced misses caught + resynced with 1 extra present, 0 frames in a
   wrong buffer.
2. **Stock Present after an FO frame** read `GetCurrentBackBufferIndex` (3D6474) before its own present spin (stock relies on
   WaitSubmitThread's spin, skipped after an FO frame): PrepareStock now waits for the pending present and restores the stream
   end / sub-stream extents (harness: removing the spin makes 6 checks fail).
3. **Watchdog restore broke rendering** (device removed even without a stall, 887A002B), bisected in game (restore in 8 groups):
   FoGetBB cached the index per FO frame number; with FoPresent (5DAF3) restored first that number froze and the still-patched
   FoGetBB returned a stale index while the chain rotated. Cache now only on the predicted path; Restore puts 5DAF3 back last
   (session already OFF; the last frame still gets PrepareStock). Mirror bug at install: 5DAF3 was written first, so an FO frame
   could end while WaitSubmitThread still read the index the stock way -> **5DAF3 is written last** ("the switch that creates FO
   frames"). The 6-byte site 7D9BC7 is now `nop; call` (return address = site end, never inside bytes a restore rewrites).
4. Test infrastructure: Tour head check accepts Nioh3BossRematch's 14-byte entry hook at 909218 (E9 + NOP padding); fo6pc did
   not hit it (load-order race between the two add-ons).

PC results (RX 9070, tour mode 7292, test settings 2560x1440 XeSS, FG off; captures/pc/fastab/2026101x-*-fo7pc*):
- fo7pc-selftest7: **ALL 17 steps PASS** (switch off/on, forced miss resynced, TAIL-issued OFF, fo6pc sequence, staged restore,
  all-at-once restore, 5.6 s stall -> watchdog -> main wakes mid-restore -> stock, stall without watchdog, 8 s stall, re-arm
  after each). Harness ALL PASSED (phases 1-10 + 9b), test_patterns ALL 245 SITES OK.
- fo7pc (`captures/pc/fastab/20261010-0720-fo7pc`): FO = 1 for 10.8 min (settle 600 s) then `Tuning.FO = 1/0` ABBA x4:
  **0 OFF / watchdog / ERROR events**, ~75.7 k FO frames, back buffer: real index published by TAIL before the frame began on
  ~87 % of frames, the rest predicted + verified, 0 resyncs, 0 wrong; 4 live FO->stock and 4 stock->FO switches clean; TAIL
  0.8 ms per FO frame on the replay thread (it now waits for its present). **FO 1 -> 0: fps -0.51 % +- 0.17 SE (100.2 -> 99.7),
  frame time +0.051 ms** = a no-harm result on this GPU-bound PC (expected ~0, section 5.3), not a feature gain. Game exited by
  the tour; the installed mod files (ASI, ini Tour = 0, Nioh3BossRematch.*) verified byte-identical after every run.
- Left: a swapchain that refuses to rotate for 7 presents in a row (session OFF, that frame is not saved); FG / proxy
  swapchains untested (present-param overrides take the stock path); pc2 (CPU-bound, where FO should pay) and Ally not run.

### 7.2 fo8 (1.7.59-fo8): pc2 fo2 gate failure = the replay thread CRASHED when it lags; PagePool page ops are the cause

pc2 FO2 (fo7, 5600X / 6650 XT, ~38 fps, drain 5.9 ms/f): self-test step 6 (switch on) -> 0 frames after 30 FO frames, TAIL
pending forever, main in the sync-point wait 3D2B90 -> GfxWaitFenceValue (null event = Sleep(0) poll of a list fence). The
replay thread was missing from the dump (80-line display cap). **Reproduced on the PC** with a replay-lag emulator
(`[Tuning] FO_ReplayDelayUs` = busy wait in the 0x82 handler, i.e. at the start of each frame's replay; 8000 us ~ pc2's
backlog): the replay thread is in the engine's crash dialog (dispatcher 2EEDFC2 -> 1A7C467 -> fault). Not a deadlock: main
polls a fence only the dead replay thread could submit. A vectored handler now reports replay faults (token, cursor, stream /
half / sub-stream recorder, bytes, payload objects, last 8 frame ends): garbage opcodes in frame N's main-stream data (twice:
token 5850 in the FIRST segment of frame N, the frame-begin page tokens' region) and binds of wiped objects (tok 0x45 texture
-> driver AV in ResourceBarrier; tok 0x5F UAV object zeroed). All with main in the frame-begin of N+1 while frame N replays.

Bisect under the 8 ms lag (PC, tour 7292): K 25 % -> fault after 71 / 160 FO frames; K 0 -> after 1529; **B4 + B6 wait for
TAIL N (`FO_PPWait = 1`): 30.8 k FO frames, 0 faults; only B6 waits (`= 2`, B4 PagePoolFrameUpdate still overlapped), K 0:
28.7 k, 0; `= 2` with K 25 %: ~31 k, 0.** So the PagePool page ops (B6, 7D9C24: tokens 0x82 / page ops / 0x83 under SRW
+D08h) must not run while frame N replays - the section-4 "reader audit" was never done and the PP lock only serialises
handler execution, not the frame-to-frame data. Default now `[Tuning] FO_PPWait = 2` (B6 waits for TAIL N; B4, B5 and the
graph up to K stay overlapped). Proper fix (let B6 overlap) needs RE of 7D9C24 / 2EF0D20 and the 0x82/0x83/0x95 handlers.

Also: sub-stream crossing guard (the first FO frame after stock frames could have sub-stream data past the recorder's middle;
the next frame then reused the half it started in - harness phase 11 reproduces it with a lagging replay: without the guard
2 FAILs), thread dump prints main + replay first and every game thread, dispatcher ring state, removed reason; watchdog says
when the replay thread faulted (nothing FO can recover: the engine's crash handler owns the game). Harness ALL PASSED,
test_patterns ALL 245 SITES OK.

PC fo8 results (8 ms emulated replay lag): fo8 default (B6 waits, K 25 %) ~35 k FO frames, 0 faults, ABBA x3 FO 1 -> 0
-0.16 % +- 0.07 SE (64.4 -> 64.3 fps) - no gain, as expected: the emulator makes the replay thread the bottleneck, which FO
cannot hide; it reproduces pc2's lag for safety, not pc2's main-bound shape (main 20.5 ms + drain 6 ms). Self-test with the
lag (fo7 build + crossing guard): ALL 17 PASS once, but the hazard is intermittent (the B6 bisect above is the evidence).
**Expect a smaller gain than section 5.3 on pc2:** with B6 waiting, only frame begin up to B6 (B1-B5, incl. B4
PagePoolFrameUpdate) overlaps frame N's tail; the graph up to K still overlaps only the time after B6. Making B6 overlappable
(RE of the page-op recording + the 0x82/0x83/0x95 replay; or deferring B6 to the barrier) is the next FO work item.

### 7.3 Verdict: FO's measured ceiling on pc2 and what +15-25 % would take (pc2 FO3, fo8; captures/pc2/fo3)

Measured (5600X / 6650 XT, CPU-bound spot, 3 ABBA pairs): **FO +0.4 % (38.83 vs 38.68 fps, SE 0.02)**, 46.9 k FO frames stable,
FO_PPWait 2 vs 1 +0.5 % (overlapping B4 is worth nothing measurable). Per frame (25.7 ms):

| | stock (FO 0) | FO 1 |
|---|---|---|
| main thread waits | drain 5.75 ms | B6 wait for TAIL N 3.29 ms + effect sync-point GPU wait 2.27 ms = **5.56 ms** |
| replay thread busy (1 - starved) | 80 % = 20.5 ms | 84 % = 21.6 ms (incl. TAIL 0.5 ms) |
| GPU busy (PresentMon GPUBusy) | 19.4 ms | 19.6 ms (GPUTime 22.5 = +2.8 ms GPUWait: idle gaps, GPU waiting for work) |
| GPULatency / DisplayLatency | 21.8 / 46.0 ms | 19.4 / 46.2 ms |

Critical path: the drain did not disappear, it moved. At E(N) the single replay thread still has ~5.8 ms of frame N to replay
(backlog ~13 segments; it is busy 84 % and starved only while waiting for main's kicks). With B6 waiting (fo8 safety), main
only overlaps B1-B5 (~2.5 ms) and then waits 3.3 ms for TAIL N. The 2.5 ms it gained is then lost at the **effect system's
per-frame GPU->CPU sync point**: a task (3D2198 -> 3D21D4 -> 3D2650, run from 1A749D0) records a sync point every frame
(5F17C -> 418DD0, token A8) and first waits for the PREVIOUS frame's one (3D2B90 -> GfxWaitFenceValue, Sleep(0) poll). So
main in frame N+1 cannot pass that task before the GPU has executed frame N up to its effect point, and the GPU gets frame N's
last lists only after the replay thread finishes N. In stock the drain hid exactly this wait (sync-point GPU wait 0.00 ms/f);
with FO main arrives 2.5 ms earlier and waits 2.3 ms. The sum of the waits equals the old drain: **FO's ceiling on pc2 is the
single replay thread feeding the GPU, chained to main by a one-frame GPU->CPU dependency. Moving work around main cannot beat it.**
Floors: main work 20.0 ms, replay 21.6 ms, GPU 19.5 ms -> the best a perfectly overlapped pipeline could reach is ~21.6 ms
(46 fps, +19 %), and only if both chains below are cut.

Screenshot pair (FO 1 A1 vs FO 0 B2): PSNR 24.9 dB, 4.6 % of pixels differ by > 32, all on animated content (bamboo, effect
aura, particles); two FO 1 shots 15 min apart differ the same (24.8 dB); static sky / ground ~0; no blocks, tearing or wrong
buffer. No FO artefact visible, but a live animated scene cannot prove bit-equivalence.

| option | expected pc2 gain | effort | verdict |
|---|---|---|---|
| FO as built (fo8), any FO_Pct | +0.4 % measured | done | **NO-GO** as a feature: keep default off; the code stays as infrastructure (it is stable: gate 17/17, 47 k frames) |
| defer B6 to the barrier / move K (FO_Pct) | ~0-1 %: main then waits at the effect sync point instead (same GPU(N) event) | 2-4 days RE (page-op protocol) | **NO-GO** alone |
| B6 overlap + relax the effect sync to frame N-1 (two-frame GPU readback) | up to ~+15-19 %, capped by the replay thread (21.6 ms) | RE of the effect readback (consumer, buffer count) + B6 protocol, 1-2 weeks; same-look proof needed (effects would see GPU results one frame later) | **NO-GO for now**: risky for looks, and the gain is CPU-only |
| faster single-thread replay (cut D3D12 API / state / barrier / descriptor cost of the ~21 ms replay; helps stock too, since drain = replay tail) | ~1 ms frame per 1 ms of replay tail removed (stock and FO alike) | profile the replay thread first (which handlers / API calls), then targeted fixes like PCM / GB / KL; days each | **GO (small, measured steps)**: the only lever that also works without FO |
| parallel replay = new back end (N dispatcher threads recording command lists from stream segments, per-thread SynCtx, cross-list resource state, descriptor rings) | the +20-25 % of section 5.3 (frame -> max(main 20, GPU 19.5)) | weeks to months, very high risk (185 handlers, LTCG state, vkd3d on the Ally) | **NO-GO** |

Context for all of it: the project goal is the Ally, which is GPU-bound at 99 %, so any of these CPU-side gains is
fps-neutral there. FO's original +15-25 % estimate assumed the drain was pure waste; it is the replay thread's real work, and
a GPU->CPU dependency pins main to it.

## 8. Files

- `mod/Nioh3EngineFix/src/frameoverlap.{h,cpp}`, `tests/frameoverlap_harness.cpp`, `tests/run_frameoverlap_harness.py` (FO).
- `mod/Nioh3EngineFix/src/bkstats.cpp` (fov line), `tests/bkstats_harness.cpp`, `src/dllmain.cpp` (version 1.7.58-fov1), ini comment.
- Emitter map used for section 2 (tokemit.py, all tokens): 0x8F <- 6B64D8 FF2759 18D66E0 191E88E 32F58C8 341F704; 0x90 <- 253F8C;
  0x9E <- 298990 2998B0 299AB8 454F7C 922DB0 38C730C; 0xAA <- 18C313C; 0xA9 <- 4F8F84; 0xA8 <- 418DD0 FF29F6; 0x96 <- 2F121D8 2F14F34;
  0x98 <- 7C50B4 2F06AB4; 0x82/0x83 <- 4BBEC 189A490 189AAA0 1A1779C 2F06CD8 2F07880 (+ 7D9C24 writes them directly).
