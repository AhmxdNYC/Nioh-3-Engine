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
4. **Verdict: GO for design + gate, not yet for the prototype.** Estimate revised: **5-7 agent-weeks** (was 3-5), risk
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

## 5. Stage 2 design (summary; detail grows with the prototype)

- **Key** `[GpuOpt] FrameOverlap = 0` (default), boot-only. Hard fallback to stock (full drain) on: render-size change (B2),
  device reset / teardown (2EEC4C8), stage load / pause screens (loading flag), FG on/off switch, any hook self-check failure, and
  a watchdog (replay of N not done after 250 ms -> drain).
- **Stream:** two halves of the existing buffer; at E1 main pushes the final kick plus a **TAIL item** (a 0x8F token whose fn is
  the ASI's tail routine), waits until the replay of N-1 (incl. its tail) is done, swaps halves, returns. The handle scheme encodes
  the half in the generation nibble.
- **Tail on the replay thread (in order):** E2' (release N's sub-stream half), E3, E4, E5 (submit; on the replay thread
  GfxSubmitFrameCmdList's ECL idle spin is the same), E6, E7, B4/B6 (section 4a), E8 (Present via the engine's present thread to keep
  the replay thread short), E9-tail (allocator reset, list-ring recycle, record rebuild, readback) with the **replay slot**.
- **Main side:** E9-main (record slot ++, wait slot fence N-2), B3 waits for present N-1, B5 predicted index, B7 with 3 sets.
- **Slot split:** the ASI keeps `recordSlot` (main) and `replaySlot` (tail); the 6 dispatcher reads of +4730h (4933F4, 48F178,
  492220, 490F3C, 9AFED0, E3) are redirected to `replaySlot` by caves; main-side readers keep +4730h = recordSlot.
- **0x8F args:** per-frame 2-deep copies of the three upscaler globals (sizes from the emitters), the token arg rewritten at
  record time by a cave after the store.
- **Sync points:** sync-table ring >= 3 (raise +3C18h if smaller at init), meshlet sets 2 -> 3.
- **Latency (E):** frame N's pipeline is unchanged (record -> replay tail -> GPU -> present); main simply starts N+1 up to ~5 ms
  earlier. While CPU-bound, input-to-photon stays about equal per frame and the higher rate cuts the average sampling wait (pc2:
  half of 26 -> 20 ms frames = ~-3 ms). When GPU-bound, one more CPU frame can be queued behind the GPU (+1 frame of latency, up
  to ~+10-17 ms at 60-100 fps) unless MaxLatency / the waitable object limits it; the A/B must report PresentMon
  AllInputToPhotonLatency for both arms and FrameOverlap should auto-disable when GPU-bound (slot-fence wait > 1 ms per frame).

## 6. Gate built: BKS "fov:" line (1.7.58-fov1, branch frame-overlap)

`[Profiler] Backend = 1` now logs a third line per window:
`BKS: fov: handle wait X ms/f (n/f, max) | sync-point GPU wait X ms/f (n/f, in N % of frames, max) | stat3CB8 X ms/f | sync tables
T frame slots S sub-streams U present thread P`, from the engine's own per-frame snapshot (+3D40h..+3D80h, copies of the live
stats +3CB8h / +3CD0h (7D99FC) / +3CE8h (3D2B90)). No hooks. Harness: `tests/run_bkstats_harness.py` ALL PASSED (fov values
reproduced); `tests/test_patterns.py` ALL 245 SITES OK.

**Gate rule:** with the CPU-bound pc2 spot, if sync-point GPU wait < 0.5 ms/frame and sync tables >= 3, the overlap ceiling stands
(Stage 3 go). If the sync-point wait is several ms (main already waiting for the GPU every frame), the prize shrinks by that amount
and B7 (3 scene-buffer sets) becomes step 1.

## 7. Files

- `mod/Nioh3EngineFix/src/bkstats.cpp` (fov line), `tests/bkstats_harness.cpp`, `src/dllmain.cpp` (version 1.7.58-fov1), ini comment.
- Emitter map used for section 2 (tokemit.py, all tokens): 0x8F <- 6B64D8 FF2759 18D66E0 191E88E 32F58C8 341F704; 0x90 <- 253F8C;
  0x9E <- 298990 2998B0 299AB8 454F7C 922DB0 38C730C; 0xAA <- 18C313C; 0xA9 <- 4F8F84; 0xA8 <- 418DD0 FF29F6; 0x96 <- 2F121D8 2F14F34;
  0x98 <- 7C50B4 2F06AB4; 0x82/0x83 <- 4BBEC 189A490 189AAA0 1A1779C 2F06CD8 2F07880 (+ 7D9C24 writes them directly).
