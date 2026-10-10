# renderer/frame-overlap: overlapping frame N's D3D12 replay with frame N+1's recording (Stage 1 lifetime audit, Stage 2 design, gate)

2026-10-10. Nioh3.exe Steam v2.02, RVAs (Ghidra base 0x7FF74F410000). Builds on backend.md (token stream, replay thread, the
per-frame drain; read that first). Tags: **S** static (decompile / disassembly, cited RVA), **M** measured, **E** estimate.
Branch `frame-overlap` (worktree D:\nioh3-wt\overlap, from `renderer-backend` 384844e). Scripts (scratch, not committed):
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
4. **Verdict: GO for design + gate (gate cleared 2026-10-10, pc2 FOV1: 0.00 ms/f main-thread GPU waits -> Stage 3 GO).** Estimate revised: **5-7 agent-weeks** (was 3-5), risk
   medium-high, concentrated in the PagePool / KtglCache frame protocol and FG / Streamline present handling. Gain on pc2 if the
   gate is clean: **+15-25 % (38 -> ~44-48 fps), central ~+18 %** (the moved tail adds ~0.5-1.5 ms to the replay thread);
   Ally ~0 (GPU-bound); PC RX 9070 some, at high fps only.
5. Correction to gpu-tiles.md: CacheFlushOnGfx 2EF0938 is **not** run twice per frame. Its only per-frame caller,
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
| E9 | slot = (slot+1) % 3, **wait slot fence** (GPU done with N-2), sync-table frame++ and reset (8BEDC8, +3C1Ch), RenderFrameRecordRebuild(SynCtx) 9AFED0, timestamp readback copy, GetTimestampFrequency, **Reset allocator +4290h[slot]**, CmdListRing recycle 4920E0 on SynCtx | GfxFrameSlotAdvance 8BEC48 | slot +4730h, SynCtx | **split**: record slot ++ and the N-2 fence wait stay on main; allocator reset, list-ring recycle, record rebuild, readback move to the tail with the replay slot |
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

Inputs since Stage 1: pc2 FOV1 (bridge #1255): main-thread handle wait **0.00 ms/f**, sync-point GPU wait **0.00 ms/f**, present
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
| **pc2** (5600X + 6650 XT, ReBAR, Windows) | CPU: main 20.5 ms + drain 6 ms = 26 ms; GPU 17.6 ms | the drain: main records N+1 while the replay thread finishes N | 26 -> ~20.5-21.5 ms: **+20-25 %** (38 -> 46-48 fps) | better: replay-side spikes (PSO creation on the dispatcher, ring-full submit spins, tile cache work in tok 0x95) no longer stall main up to K; p99 improves at least as much as the mean | per frame pipeline unchanged; main starts N+1 up to ~6 ms earlier, frames 5-6 ms shorter: input-to-photon about equal (+-3 ms) while CPU-bound |
| **Ally** (25 W, Proton / vkd3d-proton) | CPU at low TDP: both threads slower, and the frame tail is heavier under vkd3d (ExecuteCommandLists = vkQueueSubmit through the ECL thread, the ECL idle spin in GfxSubmitFrameCmdList, Present = vkQueuePresentKHR) | the same drain **plus** the tail (submit spin + present hand-off) leaves the main thread | **+10-25 %** at 25 W when CPU-bound (the larger the tail, the larger the gain); ~0 at settings where the GPU binds | the vkd3d submit / present hitches move off the main thread: 1 % lows should gain more than the mean | as pc2; when GPU-bound keep MaxLatency so the extra CPU lead does not queue a frame |
| **PC** (RX 9070, Windows) | GPU-bound at the user's settings (~100 fps); CPU-bound at high fps / low res / FG | drain share at high fps (replay thread ~ main) | **0 when GPU-bound**, +10-20 % where CPU-bound | CPU-bound spikes as pc2 | GPU-bound: auto-off (slot-fence wait > 1 ms/f on main -> K = 0) so no added queueing |

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
3. **Ally gain test** (not only no-harm): 25 W, user config, same fastab items; report avg, 1 % low, latency.
4. PC no-harm at the user's settings (GPU-bound) + one CPU-bound config (FG off, low res) for the gain.

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

Runs: pc2 FO1 queued (bridge #1260): boot FrameOverlap = 2, live `Tuning.FO = 0/1` and `Tuning.FO_Pct = 0/25` ABBA x4 at the
CPU-bound Continue spot, screenshots per arm, 10-min FO = 1 watch.

## 8. Files

- `mod/Nioh3EngineFix/src/frameoverlap.{h,cpp}`, `tests/frameoverlap_harness.cpp`, `tests/run_frameoverlap_harness.py` (FO).
- `mod/Nioh3EngineFix/src/bkstats.cpp` (fov line), `tests/bkstats_harness.cpp`, `src/dllmain.cpp` (version 1.7.58-fov1), ini comment.
- Emitter map used for section 2 (tokemit.py, all tokens): 0x8F <- 6B64D8 FF2759 18D66E0 191E88E 32F58C8 341F704; 0x90 <- 253F8C;
  0x9E <- 298990 2998B0 299AB8 454F7C 922DB0 38C730C; 0xAA <- 18C313C; 0xA9 <- 4F8F84; 0xA8 <- 418DD0 FF29F6; 0x96 <- 2F121D8 2F14F34;
  0x98 <- 7C50B4 2F06AB4; 0x82/0x83 <- 4BBEC 189A490 189AAA0 1A1779C 2F06CD8 2F07880 (+ 7D9C24 writes them directly).
