# renderer/backend: the D3D12 back end (token stream, replay thread, rings, fences, submission) and a new-backend assessment

2026-10-10, package 01 of notes/renderer/00-map.md. Nioh3.exe Steam v2.02, RVAs (Ghidra base 0x7FF74F410000). Tags: **S** static
(decompile / disassembly, cited), **M** measured (pc2 captures, cited), **E** estimate. Inputs: cpu-bound-midrange.md, d3d12-submission.md,
d3d12-sites.md, gpu-tiles.md, gpu-scheduling-sync.md, pc-submit-overhead.md and fresh decompiles of every function named below
(`D:\nioh3-re\decomp` + `decomp_ro.cmd`). The game was not run on this PC. Symbols: block `# renderer/backend` in re/symbols/symbols.tsv.
Experiment: ASI branch `renderer-backend` (1.7.58-bk1), section 7.

## 0. Answer

1. **Yes, it is a deferred command buffer.** `[App]Game` (and only it) writes KTGL tokens into one linear per-frame stream inside the
   device object (GfxDevice+3C40h). Every `CmdFlushLimit` (+3C70h) draws it hands the new stretch of the stream to
   **SynergisticDispatcherTask** through a 32-byte-item ring (+3BE0h). The dispatcher replays each stretch through a **185-entry
   handler table** (3C02220) into one open `ID3D12GraphicsCommandList` (SynCtx+580h). Closed lists go to a second ring read by
   **ExecuteCommandListsThread**, which replays a 9-entry "ECL token" table (3BD4DF0): ExecuteCommandLists + Signal, Wait,
   UpdateTileMappings. A fourth thread presents. Section 1-3.
2. **The structural cause of the CPU wall is the per-frame drain (S).** The stream is not a ring: at frame end the main thread kicks
   the last stretch, **busy-waits until the dispatcher has replayed everything** (418F40, ~6 ms on pc2), then records into the
   dispatcher's own command list itself (query resolves, back-buffer PRESENT barrier), closes it, waits for the ECL thread to
   submit everything, and only then **resets the stream to its base** and starts the next frame. So main-thread recording of frame
   N+1 and dispatcher replay of frame N never overlap. That is why main-thread savings vanish into the drain (W1B +0.1 %) and only
   the part of the dispatcher's work that falls in its backlogged tail reaches the frame (PCM: 0.7 ms saved, 0.13 ms gained).
3. **The "shared lock" (S):** SRWLOCK at **KtglCache+CF8h** (GfxDevice+61A0h+CF8h; the texture-streaming tile cache). The dispatcher
   takes it **shared once per frame** in token 0x95 (emitted by PagePoolFrameUpdate at frame begin): 7F64DC holds it across an
   upload-ring allocation that can submit the list and spin on a GPU fence (945610), two CopyBufferRegion batches and per-object
   virtual updates. **Exclusive** holders are cache insert/evict: 250ADC (around the +6D0h list edit) and 2EEF440 (tile-mapping job,
   under +CF0h too). The main thread takes it shared in CacheFlushOnGfx 2EF0938, streaming code in 828898 / 8291AC / 891F80.
   An SRW lock blocks new shared acquirers once an exclusive waiter queues, so one streaming thread waiting for exclusive access
   stalls the dispatcher behind a shared holder. The note's "[ntdll] <- 7F6904" is the SP stack scan finding a stale return slot
   of 7F6904's previous call (same as the known 1180A4 case); the real wait site is AcquireSRWLockShared at 7F650E.
4. **The "full submit queue" (S):** the ECL ring's capacity equals the command-list ring size (2F1416C / 2F142BC), and the list ring
   grows instead of waiting when its oldest list is still on the GPU (4931B0). The ECL thread is 98 % waiting with 74 context
   switches per second on pc2 (M, CPU1 TSTATE), i.e. it is not busy. Two readings fit, and BKS (section 7) separates them with the
   engine's own timer (CmdListRing+B0h): (a) the ECL thread is blocked **inside** ExecuteCommandLists / UpdateTileMappings (a kernel
   wait counts as "wait"), or (b) "[ntdll] <- 86F760" is not a full ring at all but the **ReleaseSemaphore** at 86F7BA (its return
   address 86F7C0 is in 86F760; a real full-ring wait would show its own frame 86F828 / cold chunk 1921A30).
5. **FCAP1 confirms the queue model and adds one fact (M, section 4.1):** 18-22 DIRECT ExecuteCommandLists per frame, all from the ECL
   thread, and **all of a frame's DIRECT and COMPUTE lists are submitted in two bursts in the last ~11 % of the CPU frame**: closed
   lists are held in the list ring until a submit point.
6. **Rewrite verdict (E, section 6):** a new back end that consumes the same token stream is feasible and is the right level; the
   win comes from **pipelining** (letting frame N+1's recording overlap frame N's replay), not from parallel recording alone.
   On pc2 that is worth up to ~+25-30 % (38 -> ~48-50 fps); parallel recording adds little on pc2 because the main thread
   (~19.7 ms) becomes the bound. Effort 4-8 agent-weeks for an A/B-able ASI mode; the risk is lifetime bugs in token payloads.
7. **First experiment built and queued (section 7):** BKS census (no hooks: the engine's own wait timers + 1 kHz queue sampler) and
   **KL**, a live data knob for the hand-off granularity (CmdFlushLimit / n). pc2 CPU5 is queued behind FCAP1 (bridge #1193, ASI #188).

## 1. Threads and the queues between them (S)

```
 [App]Game (main, prio 15)                     SynergisticDispatcherTask (prio 15)          ExecuteCommandListsThread (15)   present thread (15)
 ---------------------------                   ---------------------------------           ------------------------------    -------------------
 RenderFrameBegin 4A529C                        loop 1A7C3E0:                               loop 1A75970:                     loop 1A82298:
  WaitSubmitThread 7D9B6C: Sleep(0) until        WaitForSingleObject(ring sem, INF)          Wait(ECL sem, INF)                Wait(+7720h event)
   present thread idle; PagePoolFrameUpdate      pop item (32 B)                             replay ECL tokens 4F9054:         Present(+7730h)
   -> tok 0x95; GetCurrentBackBufferIndex        replay tokens via 3C02220 into SynCtx+580h   tok0 ECL(1 list)+Signal           clear PresentPending +7728h
  CacheFlushOnGfx x2 (ECL tok 4/7, locks)        if item.submit: close list 490D5C,           tok4 UpdateTileMappings
 graph walk: record tokens into +3C40h            CmdListRingSubmitUpTo 4930C8 ->              tok6/7/8 ECL / Signal / Wait
  every +3C70h draws: KtglCmdStreamFlush 254110   EclRingPush 86F760 ----ECL ring---->       EclItemDone: ReleaseSemaphore(free)
   ring full -> Sleep(0) spin (timer +3C88h)    InFlight-- (+3C10h)
   ReleaseSemaphore(+3BF0h) --dispatcher ring-->  inside replay: upload / descriptor ring
 GfxDevicePresent 3D62E4:                          exhausted -> submit list + Sleep(0)
  CmdStreamEndFrame 418B94: final kick,            until fence (945610), tok 0x95 SRW
   WaitDispatcherDrainSpin 418F40 (+3CA0h)         shared (7F650E), PSO miss -> create
  ResolveQueryData / PRESENT barrier on            the PSO synchronously (119670)
   SynCtx's list (+590h) - main thread!
  GfxSubmitFrameCmdList 252758: close, submit,
   RingWaitIdleSpin 252730 until ECL drained
  SetEvent(+7720h) -> present thread ------------------------------------------------------------------------------------>
  GfxFrameSlotAdvance 8BEC48: slot=(slot+1)%3, wait slot fence (GPU done with frame N-2)
```

| queue | object | item | capacity | full / empty behaviour | stats the engine keeps (S) |
|---|---|---|---|---|---|
| command stream | GfxDevice +3C30h base, +3C38h end, +3C40h write, +3C48h last kick, +3C50h high water | tokens (section 2) | `param+190h` bytes (+3C28h), reserve +3C2Ch | **linear, reset to base at every frame end** (418C70); near the end KtglCmdStreamFlush takes the wrap path (418E74: tok 0xA7 marker + 0xAAAAAAAA, generation +3C60h += 1) | bytes/frame +3C84h, high water +3D88h |
| dispatcher ring | GfxDevice +3BE0h: +3BF0h semaphore, +3BF8h items, +3C00h cap, +3C04h write, +3C08h read, +3C10h in-flight | 32 B {SynCtx*, begin, end, u32 submit} | `param+160h` (+3C00h) | producer: **Sleep(0) spin** while `write == read + cap` (254110 / 25426C); consumer: semaphore | ring-full spin {tot,max,cnt} +3C88h, drain spin +3CA0h, kicks +3C7Ch, draws +3C78h; per-frame snapshot +3D00h (418B94) |
| command-list ring | SynCtx+5A0h (= GfxDevice+5B0h): +10h entries[0x18 B: list, fence, ECL-token pos], +18h count, +1Ch grow step, +20h current, +24h last closed, +28h first pending, +30h fence counter, +38h submitted, +40h..+68h ECL token writer | closed lists | grows by +1Ch when the slot to reuse is still on the GPU (4931B0: drains the ECL ring, re-creates it with the new size) | recycle: fence wait (vfn+10h of SynCtx+588h, timed +F8h) | submits +11Ch, ECL bytes/frame +114h / max +118h |
| ECL ring | CmdListRing+70h: +80h wake semaphore, +88h items, +90h free-slot semaphore, +98h cap, +9Ch write, +A0h read, +A4h in-flight, +A8h high water | 32 B {queue wrapper*, begin, end, u8 flag} | **== list count** (2F13FBC / 2F142BC) | producer: `WaitForSingleObject(free, 0)`, else INFINITE (86F828, timed **+B0h**); consumer releases one free slot per item | stall +B0h, idle spin at frame submit +C8h (252730) |
| present | GfxDevice +7720h event, +7728h pending, +7730h swapchain | one Present | 1 | main spins Sleep(0) at frame begin until +7728h clears (7D9E90) | Present duration +7708h |

All engine stats are `{u64 total, u64 max, u32 count}` accumulated by 492954 around the wait (QPC ticks when EngineClock+12h, else clock()).
They are free to read; section 7 uses them.

## 2. The command format (S)

**Stream:** a sequence of tokens, each starting with a u32 header `opcode | arg << 16` (opcode = index into 3C02220), followed by an
opcode-specific payload, all 4-byte aligned. A handler is `void h(SynCtx* c, u8** cursor)`; it advances `*cursor` past its own payload
(the replay loop is `while (cur < end) table[*(u16*)cur](c, &cur)`). Examples: **0x66** `{0x66 | slot<<16, u32 size, blob}` (constant
data copied inline: the stream carries the constants, 698EAC); **0x6A** DrawIndexedInstanced; **0x95** header only; **0xA7** stream
wrap marker `{0xA7, 0xAAAAAAAA}`. Pointer payloads (resources, views, PSO keys, immediate vertex data) point at engine objects;
their lifetime is what section 6 must audit.

**Who writes tokens:** ~45 record paths on the main thread (DrawSubmit 1D2F60, MeshletDrawRecord 1D2AAC, UiViewRender 253C68,
PostEffectApply 6DA118, ... the callers of KtglCmdStreamFlush 254110). Each draw path does `++draws(+3C78h); if (++sinceFlush(+3C74h)
>= limit(+3C70h)) KtglCmdStreamFlush(ctx, 0)`. A flush with `submit = 1` (frame end, and some explicit callers) makes the dispatcher
close and submit its list after that stretch.

**Where lists are cut (S):** (1) a submit item (frame end, explicit); (2) **CmdListRingSubmitIfBehind 945664** when an upload or
descriptor ring is exhausted (the dispatcher needs GPU progress: submit, then FenceValueReachedOrPoll + Sleep(0) spin in 945610);
(3) token 0xA9 SynCloseListAndPushEclTok2 and the async-slot kick/join paths (gpu-scheduling-sync.md 1.1: GFX Signal -> compute Wait
and back; each splits the DIRECT list); (4) 2EFBA1C / 2EFB97C (close once, then cache work). RGP r5: ~28 GFX ExecuteCommandLists of
one list each per frame (d3d12-submission.md 1).

**ECL tokens (3BD4DF0)**, replayed by the ECL thread on the queue wrapper `w` stored in the item (`u32 opcode`, then u64 fields):

| op | handler | effect |
|---|---|---|
| 0 | 4F90A0 | `w->vfn+30h(list, value)` = EclSubmitAndSignal 4F90DC for the GFX wrapper: `GfxQueue(+4270h)->ExecuteCommandLists(1, &list)` + `Signal(SynCtx fence +6C8h, value)` |
| 1 | 2F155F8 | `w->vfn+38h(a, b)` |
| 2 | 4F9264 | `w->vfn+40h(&u64, flag)` (cross-queue wait / fence op of the wrapper) |
| 3 | 4F9138 | `w->vfn+48h(a, b, c)` |
| 4 | 2F15528 | `w->vfn+50h`: **UpdateTileMappings**(resource, nRegions, coords, sizes, heap, nRanges, flags, starts, counts) (GfxExecUpdateTileMappings) |
| 5 | 2F15400 | `w->vfn+58h`: CopyTileMappings |
| 6 | 4F9214 | `queue->ExecuteCommandLists(1, &list)` |
| 7 | 4F91C0 | `queue->Signal(fence, value)` |
| 8 | 4F93C8 | `queue->Wait(fence, value)` |

**KTGL token table (3C02220):** the full 185-row table (opcode, handler, name, what it does) is in section 8.

## 3. D3D12 objects (S)

| object | where | notes |
|---|---|---|
| device | GfxDevice+4268h (CreateD3D12Device 2EE300C, through Streamline's interposer) | SynCtx+8E0h / +8E8h hold it for CBV creation / descriptor copies |
| queues | DIRECT +4270h (GfxCreateDirectQueue 2EE9DE4); COPY x3: GfxCreateCopyQueue8 2EE9EE8 (8 lists), cache tile queue 2F110A8 (falls back to DIRECT), cache upload queue 2F13350; COMPUTE: AsyncComputeCtxCreateQueue 2EF8448 per async slot (own lists, root signature, CBV/SRV/UAV + sampler heaps); FFX FI presenter queues 37761E4 | shipped async slots on pc: 3 VCLOUD, 4 VFSCAT, 6 ocean (gpu-scheduling-sync.md 1.1) |
| allocators / lists | GFX: per-list allocators in the CmdListRing (SynCtx+588h pool: vfn 0 create, +8 IsComplete(fence), +10h wait, +18h reset, +20h/+28h around Close); 16 per-frame-slot DIRECT allocators GfxDevice+4290h (2EEA690) for the frame-slot work | ring grows on demand (4931B0), so the GFX path almost never waits for an allocator |
| list begin | SynBeginList 49349C: SetDescriptorHeaps(2: CBV/SRV/UAV ring heap c+AA8h[c+AB0h], sampler heap c+988h), SetGraphicsRootSignature(c+6D8h), SetComputeRootSignature(c+6E0h), 6 root tables (c+B08h..B1Ch), re-bind VB/IB with transitions (494E00), `c+B24h = 7Fh` (all state dirty) | every list re-establishes all state |
| list close | CmdListRingClose 490D5C: `ClearState(NULL)` + Close, ++fence, ECL token 0 {list, value} | ~28 per frame |
| fences | SynCtx+6C8h (GFX list fence; completed value cached at SynCtx+2510h, FenceValueReachedOrPoll 4920A0 / IsFenceComplete 1180A4), per-frame-slot values GfxDevice+4320h[3], cache COPY queue fence (b+50h), async slot fences | GfxFrameSlotAdvance 8BEC48 after Present: **3 engine frames in flight** (BufferCount +4240h = 3) |
| descriptor heaps | GfxInitCreateDescriptorHeapWrap 2EF3944; CPU staging tables per shader stage (c+940h, stride c+938h; samplers c+970h/968h); shader-visible **linear ring** (ShaderVisibleDescRingAlloc 1192B0, fence-checked) | per dirty stage: allocate N, one CopyDescriptorsSimple(N) of the whole stage table, SetGraphicsRootDescriptorTable; SRV/sampler binds copied into staging one at a time (115DE8); sampler tables content-cached (1188AC) |
| constants | token 0x66 blob -> per-slot shadow c+D70h (698EAC) -> UploadRingAllocate 1190C0 (block chain retired by fence) -> CreateConstantBufferView into staging -> table copy (SynUploadCbvTable 118E28) | three copies (d3d12-submission.md 4); one CBV creation per dirty CBV per draw |
| root signatures | 2 on the GFX context (graphics c+6D8h, compute c+6E0h) + 4 command signatures (EI draw c+6E8h, draw-indexed c+6F0h, ...) + query heap, all in SynCtxCreateSignaturesAndQueries 2EFEA14; async contexts have their own | table-only layout (CBV / SRV / sampler tables per stage) |
| PSO binding | PsoGetOrCreateGfx 119670 (128-byte key hash, CS, B-tree c+708h; **a miss creates the PSO synchronously on the dispatcher under the CS**: CreateGfxPso 9290D8), SetPipelineState only when c+B24h & 20h; compute SynFlushComputeStateEx 119C08 / CreateComputePso 58C2B0 | PCM memo (psomemo.cpp) sits in front of the B-tree |
| barriers | SynTransitionResource 115E24: the current state lives **in the resource object** plus a per-list stamp (SynCtx+6D0h); skip if equal, else `ResourceBarrier(1, TRANSITION)`; reset to COMMON when the stamp shows a new list (buffer decay at ECL boundaries). Emitted at bind time (texture bind 115B9C -> SRV states), before draws for RT/DSV (1180D0), UAV barriers from tokens 0x8A / 0x7D (113A0C / 1139D4) | **one barrier per call, never batched**; exact because recording order == submission order on one queue |
| resources / upload | GpuMem* creators (committed / placed / reserved / heaps: 62C4DC, 62C8E0, 62A164, 2F1165C, ...) on streaming threads; UploadHeapSuballoc 62D688 (UPLOAD heap; in VRAM with ReBAR); textures stream as **reserved resources + UpdateTileMappings** (KtglCache, gpu-tiles.md), tile data through the cache COPY queues | the KtglCache lock (section 0.3) couples streaming to the dispatcher |

## 4. Per frame: who runs, who waits (S + M pc2, ReBAR on, Continue spot)

| step | thread | waits on | pc2 cost (M) |
|---|---|---|---|
| frame begin | main | present thread idle (7D9E90 Sleep(0)) | ~0 (W->B 0) |
| tok 0x95 replay | dispatcher | KtglCache SRW +CF8h shared vs streaming writers | "[ntdll] <- 7F6904" 6.1-6.8 % of the dispatcher ≈ 1.6-1.75 ms |
| recording | main | dispatcher ring full (Sleep(0) spin) | unknown (BKS) |
| replay | dispatcher | driver inside ResourceBarrier (UAV, 1139D4) | 7.5 % ≈ 1.9 ms |
| replay | dispatcher | ECL ring full or the ReleaseSemaphore wake (86F760) | 5.5 % ≈ 1.4 ms |
| replay | dispatcher | upload / descriptor ring exhausted -> submit + fence spin (945610) | unknown (not timed by the engine) |
| replay | dispatcher | nothing to do (semaphore) | 2.7 % SP, wait 27-29 % TSTATE (all waits) |
| frame end | main | **dispatcher drained** (418F40) | **23.7 % of main = 6.1 ms** |
| frame end | main | ECL thread drained (252730) | small (BKS) |
| after Present | main | GPU finished frame N-2 (slot fence) | ~0 while CPU-bound |

Per frame on the GPU side: ~28 GFX ExecuteCommandLists (one list each, RX 9070 r5), 2 GFX Signals from CacheFlushOnGfx, per active async
slot a kick and a join (2 list splits each), the cache's own COPY-queue batches.

### 4.1 Check against FCAP1 (pc2, bridge #189, 5 captures x 2 frames, XSYS config; M)

The FC logger recorded no command-list calls in this run (its list hooks were not active; pc2 #1202), but every queue operation is there
with thread id and caller, which is what this model needs:
- **DIRECT: 18-22 ExecuteCommandLists per frame, each from EclSubmitAndSignal (return 4F9114) followed by its Signal (4F9127), all on
  one thread (the ECL thread)**, plus 2 per frame from another module (FFX / OSD). COMPUTE: 4-6 per frame through ECL token 6 (4F925F),
  each preceded by an ECL token 8 Wait (return 4F908B in EclReplayTokens). DIRECT queue Waits: 3 via ECL token 2 -> EclWait 4F92A4 and
  3 via GfxWait 8F4548 (the async joins). COPY: texture uploads (GfxSubmitSegmentVirtCalls 4DA0C, up to 18 per frame in streaming
  bursts, other threads), 892D88 (3 per frame), CmdListRingCloseAndSubmit 5C268; UpdateTileMappings only through
  CacheTileBatchFlushDirect 290F94 on the cache COPY queue (0-63 per frame), none through ECL token 4 in these captures.
- **Submission is batched at the end of the CPU frame.** In every captured frame the DIRECT lists reach the queue in two bursts:
  ~15 lists within ~0.6 ms at ~89 % of the frame (21.2-21.8 ms of 23.8) and ~6 lists at 99 % (23.6-23.8 ms), right before Present;
  between ~0.6 ms and ~21 ms **nothing** is submitted to DIRECT or COMPUTE. Cause (S): a closed list is pushed to the ECL ring only
  at a submit point (CmdListRingSubmitUpTo from a submit item, CmdListRingSubmitIfBehind, or when its ring slot must be recycled:
  CmdListRingFlushAndRecycle 4920E0 pushes only up to the slot it reuses), so mid-frame cuts close lists but do not submit them.
  The same pattern is in the RX 9070 RGP trace ("all GFX lists within a 2.5-3.2 ms burst at the end of the CPU frame",
  d3d12-submission.md 5). Consequences: the GPU starts frame N only after the dispatcher has replayed almost all of it (latency, and
  GPU idle when the CPU frame is long), and the ECL thread sees ~20 items arrive at once (the ECL-ring-full / ReleaseSemaphore
  rows of section 5 fall in these bursts). Early submission (push each list when it is closed) is a cheap complement to option 1.

## 5. What blocks the replay thread: reading the three SP rows correctly

The SP sampler attributes a sample whose RIP is outside the exe to the **first return address into the exe found on the copied
stack** (sp.cpp). Stale return slots below the live frames therefore win when the system frames do not overwrite them:
- **`[ntdll] <- 7F6904` (6.8 %)**: 7F6904 cannot call into ntdll by itself (it calls 7F6A64, 7F6964, QpcNow, 492954). Its caller
  7F64DC calls AcquireSRWLockShared at 7F650E *before* calling 7F6904, so the stale slot is from the previous frame's 7F6904 call at the
  same stack depth. Conclusion: tok 0x95 waits for the cache lock (S, probable). Fix shape: keep the shared section short (snapshot the
  +6D0h lists under the lock, do the upload / copies / possible fence wait after releasing it), or make 250ADC / 2EEF440 not need
  exclusive access for their list edit.
- **`[ntdll] <- 1139D4` (7.5 %)**: 1139D4 is a 0x48-byte leaf that only calls ResourceBarrier; the driver (amdxc64) blocks inside it
  (allocation of command-buffer space, a driver lock, or residency). Not fixable from the engine except by fewer / batched barriers.
- **`[ntdll] <- 86F760` (5.5 %)**: see section 0.4. BKS's ECL-stall timer (CmdListRing+B0h) and SP Focus 2's per-export census
  (SPW: NtReleaseSemaphore vs NtWaitForSingleObject) decide which it is.

## 6. Rewrite feasibility: a new back end on the same token stream

**What the engine guarantees today:** one replay thread, tokens in order, resource states tracked by replaying in submission order,
all per-frame CPU data alive until the drain, 3 engine frame slots for GPU lifetime.

**Options (E; pc2 numbers from cpu-bound-midrange.md: main 19.7 ms busy + 6.1 ms drain, dispatcher ~20 ms incl. ~5-6 ms blocked):**

| # | change | what it needs | pc2 frame (fps) | effort | risk |
|---|---|---|---|---|---|
| 0 | KL: smaller hand-off granularity (built) | data write | 0 to -1 ms if the dispatcher starves mid-frame (BKS shows it) | done | none |
| 1 | **Pipelined frame end** (keep the replay thread): double-buffer the stream (+3C30h base), move the post-drain main-thread work (query resolves, PRESENT barrier, close / submit, present hand-off) into tokens the dispatcher executes, let RenderFrameBegin start N+1 while the dispatcher finishes N | lifetime audit of every pointer payload (185 tokens) against what the main thread frees / rewrites at frame begin; the frame-slot allocators (3 slots) already keep GPU-lifetime data; tok 0x95 / PagePoolFrameUpdate ordering; FFX / Streamline markers per frame | 26 -> ~20-21 ms (**38 -> ~48 fps, +25 %**): frame = max(main 19.7, dispatcher ~20) | 3-5 agent-weeks | medium-high: a missed lifetime = rare corruption / device removal |
| 2 | Parallel replay (new back end): N replay threads on consecutive segments, each with its own SynCtx copy, list and descriptor / upload ring; lists submitted in order by the existing ECL thread | per-segment state reconstruction (SynBeginList already re-binds everything at list start), **barrier tracking moved to submit time** (record "needed state" per resource per list, resolve sequentially at submit with a small fix-up list), thread-safe PSO cache (has a CS), per-thread rings | with the drain kept: tail ~6 -> ~2 ms (**~+15-20 %**); with 1: no extra gain on pc2 (main becomes the bound), +gain only where the main thread is faster than replay (PC at high fps) | 4-8 agent-weeks (00-map.md 5a) | high: 185 handlers' side effects on SynCtx state, driver-specific behaviour, vkd3d |
| 3 | Dispatcher waits (no new thread): batch barriers inside the engine (no COM proxy: append in SynTransitionResource, flush at the next draw / dispatch / copy / close), shorten the KtglCache shared section, ECL wake batching | small caves | 1-3 ms of dispatcher time; only the backlogged-tail share reaches the frame (PCM: ~20 %) -> **+1-3 %** unless combined with 1 | 1-2 agent-weeks | low-medium (proxy batching cost +0.37 ms on the PC: pc-submit-overhead.md 3) |
| 4 | Fully new renderer / frame graph | see 00-map.md 5 b/c | same CPU ceiling as 1+2 | months-years | very high |

**Recommendation:** measure first (CPU5), then prototype **option 1** behind a default-off key, because it attacks the cause (no
overlap) with the smallest code surface; it is also the prerequisite that makes option 2 pay on any CPU. Option 2 alone is the
expensive way to get a smaller share of the same prize on pc2. The token-lifetime audit for option 1 is the main work item:
- tokens whose payload is inline (constants 0x66, state values) are safe by construction once the stream is double-buffered;
- tokens carrying pointers into per-frame-slot memory are safe (3 slots, GPU-fenced);
- tokens carrying pointers to engine objects that the main thread mutates during frame N+1 (pass records, view / RT descriptors,
  immediate-mode vertex staging +2458h, the KtglCache lists) need a copy or a slot; FCAP1's caller RVAs give the per-token counts to
  prioritise the audit.

Gain on other machines (E): the PC (RX 9070) is co-critical at high fps (pc-submit-overhead.md 3): option 1 helps there too; the Ally is
GPU-bound (~0).

## 7. First experiment: BKS census + KL kick limit (built, queued)

Branch `renderer-backend` (commit 8e30fc2; ASI 1.7.58-bk1, bridge #188 merged with CPU4's SP Focus 2 build):
- **`[Profiler] Backend = 1`** (default 0), `BackendSampleUs = 1000`, `BackendSec`: no hooks, no code bytes. Reads the engine's own
  timers listed in section 1 per frame / per report, and samples every 1 ms: main in drain spin (g_DispatcherDrainBusy 45CE8A0) vs
  recording, **dispatcher starved while the main thread records** (ring empty, nothing in flight), in-flight segments, dispatcher / ECL
  ring full, KtglCache SRW +CF8h held / exclusive / waiters. Layout self-check of three byte runs (86F775, 418F46, 418F90) or it stays off.
  Two `BKS:` lines per report.
- **`[Tuning] KL_Div = 0`** (live: LiveCtl `Tuning.KL_Div = n`): CmdFlushLimit = max(16, engine value / n), restored on 0, re-asserted
  if the engine rewrites the field. Same tokens, lists and order; only the hand-off granularity changes (every reader compares at use time).
- Offline proof: `tests/run_bkstats_harness.py` (fake v2.02 image + fake engine thread: all report values reproduced, KL write / clamp /
  re-assert / restore / refuse) ALL PASSED; `tests/test_patterns.py` ALL 245 SITES OK.
- **pc2 CPU5** (bridge #1193): Continue spot (CPU-bound), CPU4's SP Focus 2 keys + Backend = 1, cpu_live_sched `Tuning.KL_Div=0/4`
  ABBA x4. Decision rules: KL keeps if B - A >= +1 % with SE < 0.3; "starved-while-recording" >= 15 % says the hand-off granularity
  matters; ECL stall ms/frame vs SPW's NtReleaseSemaphore share decides section 0.4; drain ms/frame from the engine timer replaces the SP
  share as the option-1 prize.

Not chosen as the first experiment: a deeper dispatcher ring (no evidence the main thread ever finds it full; BKS measures it), a
deeper ECL ring (it already grows with the list ring), splitting the KtglCache lock (needs the lock census first; option 3).

## 8. KTGL token table (3C02220, 185 entries)

64 of 185 handlers named (S: decompile / disassembly; payload layouts in the symbol descriptions). Handler signature `void h(SynCtx* c, u8** cursor)`.

| op | handler | name | D3D12 command-list methods reached (vcalls) | what it does |
|---|---|---|---|---|
| 00 | 7D9A60 | ? | - |  |
| 01 | 8F0B44 | ? | - |  |
| 02 | 458E34 | SynCmdReplaySetRenderTargets | ClearRenderTargetView, ResourceBarrier | 463 B command-stream decoder (no direct callers, handler table): header u32 = RTV count (low byte) / flag (byte 1) / ...; reads up to 8 RTV handles... |
| 03 | 45A020 | SynTokCopyTextureRegion03 | CopyTextureRegion, ResourceBarrier | tok 0x03: src -> COPY_SOURCE, dst -> COPY_DEST, CopyTextureRegion, src restored (depth bits stripped); no stamp |
| 04 | 866638 | ? | ClearRenderTargetView, ResourceBarrier |  |
| 05 | 2F0D814 | ? | ClearRenderTargetView, ResourceBarrier |  |
| 06 | 2F0E420 | ? | - |  |
| 07 | 45A42C | SynTokReadDescriptorList384 | DrawIndexedInstanced, IASetIndexBuffer, IASetVertexBuffers, OMSetRenderTargets, RSSetScissorRects, RSSetViewports, ResourceBarrier | decodes token header, memsets 0x180 bytes and reads up to N entries from token stream (was SynTokReadStateBlock384) |
| 08 | 45D020 | SynTokFlushDrawsIfStateChanged | DrawIndexedInstanced, IASetIndexBuffer, IASetVertexBuffers, OMSetRenderTargets, RSSetScissorRects, RSSetViewports, ResourceBarrier | token handler: reads packed token, flushes pending draws (+0x2458) if per-slot state (+0x170) differs (was SynTokFlushThenBindDraw) |
| 09 | 699B18 | SynTokSetPerRtValue09 | DrawIndexedInstanced, IASetIndexBuffer, IASetVertexBuffers, OMSetRenderTargets, RSSetScissorRects, RSSetViewports, ResourceBarrier | KTGL tok 0x09: per-render-target u32 at c+0xA4[8] (slot 0xFF = all 8), mirrored to c+0x79C for enabled RTs (c+0x794), dirty c+0xB24 /= 0x20; flushe... |
| 0A | 111B3C | SynCmdReplaySetStateVariant16E | DrawIndexedInstanced, IASetIndexBuffer, IASetVertexBuffers, OMSetRenderTargets, RSSetScissorRects, RSSetViewports, ResourceBarrier | 152 B command-stream decoder (no direct callers); reads u32, flag = hi16 != 0; if it differs from ctx+16Eh: SynFlushImmediateDraws when +2458h pend... |
| 0B | 111CBC | ? | DrawIndexedInstanced, IASetIndexBuffer, IASetVertexBuffers, OMSetRenderTargets, RSSetScissorRects, RSSetViewports, ResourceBarrier |  |
| 0C | 45B208 | SynTokSetFlag16F | DrawIndexedInstanced, IASetIndexBuffer, IASetVertexBuffers, OMSetRenderTargets, RSSetScissorRects, RSSetViewports, ResourceBarrier | token handler: when bit16 of the token changes the flag +16Fh, flushes immediate draws, updates +780h bit0 and marks state dirty (+B24h/20h) (cpu10... |
| 0D | 45E52C | ? | DrawIndexedInstanced, IASetIndexBuffer, IASetVertexBuffers, OMSetRenderTargets, RSSetScissorRects, RSSetViewports, ResourceBarrier |  |
| 0E | 45E5E0 | SynTokFlushDrawsIfStateChangedB | DrawIndexedInstanced, IASetIndexBuffer, IASetVertexBuffers, OMSetRenderTargets, RSSetScissorRects, RSSetViewports, ResourceBarrier | token handler: flushes draws if flag byte (+0x188) differs from token bit, clears bit1 of +0x780 (was SynTokFlushThenSetState) |
| 0F | 797384 | SynTokFlushDrawsIfStateChangedC | DrawIndexedInstanced, IASetIndexBuffer, IASetVertexBuffers, OMSetRenderTargets, RSSetScissorRects, RSSetViewports, ResourceBarrier | token handler: flushes draws if flag byte (+0x16d) differs from token bit (was SynTokFlushThenSetStateB) |
| 10 | 696DA8 | SynOMSetStencilRef | DrawIndexedInstanced, IASetIndexBuffer, IASetVertexBuffers, OMSetRenderTargets, OMSetStencilRef, RSSetScissorRects, RSSetViewports, ResourceBarrier | SynRecord subsystem; D3D12: CL.OMSetStencilRef (sites 696E68) |
| 11 | 79E6B8 | SynCmdSetStencilOps | DrawIndexedInstanced, IASetIndexBuffer, IASetVertexBuffers, OMSetRenderTargets, RSSetScissorRects, RSSetViewports, ResourceBarrier | 375 B command-stream handler: reads packed u32 (fail b0, depth-fail b1, pass hi16), if changed vs cache +0xD0/+0xD4/+0xD8 flushes SynFlushImmediate... |
| 12 | 79E5E8 | ? | - |  |
| 13 | 2F0E1D8 | ? | - |  |
| 14 | 45B154 | ? | DrawIndexedInstanced, IASetIndexBuffer, IASetVertexBuffers, OMSetRenderTargets, RSSetScissorRects, RSSetViewports, ResourceBarrier |  |
| 15 | 861294 | ? | DrawIndexedInstanced, IASetIndexBuffer, IASetVertexBuffers, OMSetRenderTargets, RSSetScissorRects, RSSetViewports, ResourceBarrier |  |
| 16 | 458CC0 | SynTokReadDescriptorList256 | DrawIndexedInstanced, IASetIndexBuffer, IASetVertexBuffers, OMSetRenderTargets, RSSetScissorRects, RSSetViewports, ResourceBarrier | decodes token header, memsets 0x100 bytes and reads up to N descriptor entries from token stream (was SynTokReadStateBlock256) |
| 17 | 45BB70 | ? | - |  |
| 18 | 45BB8C | ? | - |  |
| 19 | 7865F4 | ? | DrawIndexedInstanced, IASetIndexBuffer, IASetVertexBuffers, OMSetRenderTargets, RSSetScissorRects, RSSetViewports, ResourceBarrier |  |
| 1A | 2F0DCEC | ? | - |  |
| 1B | A19564 | ? | - |  |
| 1C | 9B48F0 | ? | - |  |
| 1D | 2F0DD08 | ? | - |  |
| 1E | 2F0E48C | ? | - |  |
| 1F | 2F0E444 | ? | - |  |
| 20 | 2F0E42C | ? | - |  |
| 21 | 45D148 | SynTokSetRenderStateMode | DrawIndexedInstanced, IASetIndexBuffer, IASetVertexBuffers, OMSetRenderTargets, RSSetScissorRects, RSSetViewports, ResourceBarrier | token handler: mode = token>>16 plus object; when changed flushes immediate draws, stores +194h/+60h and binds the matching render-state object (Sy... |
| 22 | 2F0E164 | ? | - |  |
| 23 | 1164C4 | SynTokSetPipelineLayout | DrawIndexedInstanced, IASetIndexBuffer, IASetVertexBuffers, OMSetRenderTargets, RSSetScissorRects, RSSetViewports, ResourceBarrier | 323 B dispatcher token handler: pops a layout/program object L; if != c+48h: SynFlushImmediateDraws when c+2458h, sets dirty c+B24h/=30h, stores L ... |
| 24 | 45C7C4 | ? | - |  |
| 25 | 45C7A0 | ? | - |  |
| 26 | 45BC2C | ? | - |  |
| 27 | 2F0E210 | ? | - |  |
| 28 | 2F0E274 | ? | - |  |
| 29 | 114750 | SynCmdSetRootSignature | - | 294 B; decodes pointer from cmd stream, if changed marks dirty flags (+0xb24 /0x20 etc.) comparing per-table counts (0x44..0x50) |
| 2A | 1134E4 | SynTokSetProgramAt80 | - | Syn command-token handler (table 3C02370, next to SynCmdSetRootSignature): pops a program/layout ptr P; if != ctx+80h: dirty B24h/=40h, stores P at... |
| 2B | 115988 | SynTokSetCbDataSlot | - | token handler: reads {size<<18, slot<<28 / offset} and copies the inline blob into the per-slot constant shadow c+BB0h[slot]+offset, sets dirty bit... |
| 2C | 1146DC | SynTokUploadConstants2C_Stage0 | - | KTGL tok 0x2C (handler table 0x3C02220 slot 0x2C): decodes header and calls SynUploadConstantsToRing(stage 0) (decompile-verified, cpu100-u1) (was ... |
| 2D | 907384 | ? | - |  |
| 2E | 2F0E264 | ? | - |  |
| 2F | 9055A0 | ? | - |  |
| 30 | 900824 | ? | - |  |
| 31 | 9061A4 | ? | - |  |
| 32 | 2F0E254 | ? | - |  |
| 33 | 2F0E234 | ? | - |  |
| 34 | 2F0E244 | ? | - |  |
| 35 | 2F0E298 | ? | - |  |
| 36 | 2F0E2A8 | ? | - |  |
| 37 | 115EA4 | SynTokSetCbData_Tok37 | - | KtglTokenHandlerTable[0x37]: decodes header (size = w0>>18 dwords), slot = w1>>28, offset = w1&0xFFFF; Memcpy inline blob into SynCtx+0xD70[slot] s... |
| 38 | 11579C | SynTokUploadConstants38_Stage4 | - | KTGL tok 0x38 (handler table 0x3C02220 slot 0x38): decodes count/stride header and calls SynUploadConstantsToRing(stage 4) (decompile-verified, cpu... |
| 39 | 11676C | SynTokSetCbData_Tok39 | - | KtglTokenHandlerTable[0x39]: same as tok 0x37 but into per-slot shadow SynCtx+0xEC0[slot] with dirty bit in +0xB8C (another stage's CbvDirtyMask wo... |
| 3A | 861CF8 | SynCmdReplaySetConstantsStage7 | - | 117 B command-stream decoder (no direct callers); reads header (elemSize = hdr>>16, payload stride hdr>>18), slot, start (lo16) / count (hi16), adv... |
| 3B | 45B034 | SynTokSetPtrAt38Dirty10 | - | Syn command-token handler (table 3C023F8, next to SynTokSetVertexBufferTracked): pops a u64; if != ctx+38h stores it and sets dirty B24h/=10h; dire... |
| 3C | 116260 | SynTokSetVertexBufferTracked | IASetVertexBuffers, ResourceBarrier | tok 0x3C: VB slot bind: X=(cur&0xAC3)/vb+48 via SynTransitionResource (stamp vb+28), then IASetVertexBuffers(slot,1) (vtbl+0x160); was SynTokResour... |
| 3D | 114A20 | SynTokSetIndexBuffer | IASetIndexBuffer, ResourceBarrier | dispatcher token 0x3D: reads the IB object from the stream, skips if unchanged (c+30h), else IASetIndexBuffer view; was RenderSubmitHot_114A20 |
| 3E | 8DAEEC | SynTokBindTexturesStage0 | - | Syn command-token handler (table 3C02410): tail-calls 115B08(stage 0, ctx, ip), which decodes the slot list and calls SynBindTextureSlot; leaf (no ... |
| 3F | 2F0E2C8 | ? | - |  |
| 40 | 9142FC | ? | - |  |
| 41 | 2F0E200 | ? | - |  |
| 42 | 2F0E1B8 | ? | - |  |
| 43 | 2F0E308 | ? | - |  |
| 44 | 115A70 | SynTokReadStateBlock128Dispatch | DrawIndexedInstanced, IASetIndexBuffer, IASetVertexBuffers, OMSetRenderTargets, RSSetScissorRects, RSSetViewports, ResourceBarrier | decodes 128-byte state block from token stream into RenderSubmitHot_115B9C (was SynTokBindTextureSlotRead) |
| 45 | 84196C | ? | DrawIndexedInstanced, IASetIndexBuffer, IASetVertexBuffers, OMSetRenderTargets, RSSetScissorRects, RSSetViewports, ResourceBarrier |  |
| 46 | 747190 | ? | DrawIndexedInstanced, IASetIndexBuffer, IASetVertexBuffers, OMSetRenderTargets, RSSetScissorRects, RSSetViewports, ResourceBarrier |  |
| 47 | 2F0D7F4 | ? | - |  |
| 48 | 747180 | ? | - |  |
| 49 | 2F0D7E4 | ? | - |  |
| 4A | 2F0D7D4 | ? | - |  |
| 4B | 2F0D804 | ? | - |  |
| 4C | 7471E8 | ? | DrawIndexedInstanced, IASetIndexBuffer, IASetVertexBuffers, OMSetRenderTargets, RSSetScissorRects, RSSetViewports, ResourceBarrier |  |
| 4D | 7471BC | SynCmdClearTextureSlotStage7 | DrawIndexedInstanced, IASetIndexBuffer, IASetVertexBuffers, OMSetRenderTargets, RSSetScissorRects, RSSetViewports, ResourceBarrier | 43 B command-stream handler: reads u32 from the cursor, advances 4 B, SynBindTextureSlot(c, stage 7, slot = v >> 16, null, 0) i.e. unbinds that tex... |
| 4E | 113BA4 | SynTokBindBufferSrv4E | DrawIndexedInstanced, IASetIndexBuffer, IASetVertexBuffers, OMSetRenderTargets, RSSetScissorRects, RSSetViewports, ResourceBarrier | tok 0x4E: buffer SRV bind, cache c+0x1118, X=(cur&0xA83)/NPSR, stamp from the stream |
| 4F | 989F2C | ? | - |  |
| 50 | 90FA3C | ? | - |  |
| 51 | 9E5858 | ? | - |  |
| 52 | 2F0E188 | ? | - |  |
| 53 | 2F0E2D8 | ? | - |  |
| 54 | 115084 | SynTokBindBufferSrv54 | DrawIndexedInstanced, IASetIndexBuffer, IASetVertexBuffers, OMSetRenderTargets, RSSetScissorRects, RSSetViewports, ResourceBarrier | tok 0x54: buffer SRV bind, cache c+0x1718, stamped (twin of 8507B0) |
| 55 | 815DB0 | SynCmdBindResourceSlot | DrawIndexedInstanced, IASetIndexBuffer, IASetVertexBuffers, OMSetRenderTargets, RSSetScissorRects, RSSetViewports, ResourceBarrier | 262 B; decodes cmd (slot=hi16), flushes immediate draws, SynCopyDescriptorSimple1 + SynTransitionResource; skips if slot unchanged; was RenderSubmi... |
| 56 | 115398 | SynTokBindTextureSrv56 | DrawIndexedInstanced, IASetIndexBuffer, IASetVertexBuffers, OMSetRenderTargets, RSSetScissorRects, RSSetViewports, ResourceBarrier | tok 0x56: texture SRV bind, cache c+0x1118, no stamp |
| 57 | 2F0E2B8 | ? | - |  |
| 58 | 2F0E1C8 | ? | - |  |
| 59 | 2F0E1F0 | ? | - |  |
| 5A | 2F0E1A8 | ? | - |  |
| 5B | 2F0E2F8 | ? | - |  |
| 5C | 8507B0 | SynTokBindTextureSrv5C | DrawIndexedInstanced, IASetIndexBuffer, IASetVertexBuffers, OMSetRenderTargets, RSSetScissorRects, RSSetViewports, ResourceBarrier | tok 0x5C: texture SRV bind, cache c+0x1718, no stamp |
| 5D | 823534 | SynTokBindTextureSrv5D | DrawIndexedInstanced, IASetIndexBuffer, IASetVertexBuffers, OMSetRenderTargets, RSSetScissorRects, RSSetViewports, ResourceBarrier | tok 0x5D: texture SRV bind, cache c+0x1B98, no stamp |
| 5E | 90445C | ? | - |  |
| 5F | 113A60 | SynTokBindUavStamped | DrawIndexedInstanced, IASetIndexBuffer, IASetVertexBuffers, OMSetRenderTargets, RSSetScissorRects, RSSetViewports, ResourceBarrier | tok 0x5F: UAV bind, cache c+0x1DB0 (same descriptor: bind and transition skipped; never reset per list), SynTransitionResource(UAV) with stamp from... |
| 60 | 698DC0 | ? | - |  |
| 61 | 698DD0 | ? | - |  |
| 62 | 698DE0 | ? | - |  |
| 63 | 698DF0 | ? | - |  |
| 64 | 2F0E198 | ? | - |  |
| 65 | 2F0E2E8 | ? | - |  |
| 66 | 698EAC | SynTokSetCbData | - | tok 0x66: copies the inline constant blob into the per-slot shadow c+0xD70[slot] (2nd copy) and sets dirty bit c+0xB80 |
| 67 | 698E00 | ? | - |  |
| 68 | 2F0E5AC | ? | CopyBufferRegion, ResourceBarrier, SOSetTargets |  |
| 69 | 6BE12C | SynTokDraw | DrawInstanced, OMSetRenderTargets, RSSetScissorRects, RSSetViewports, ResourceBarrier | tok 0x69: state flush then DrawInstanced |
| 6A | 1167D4 | SynTokDrawIndexed | DrawIndexedInstanced, OMSetRenderTargets, RSSetScissorRects, RSSetViewports, ResourceBarrier | tok 0x6A: SynFlushGfxStateForDraw then DrawIndexedInstanced(count, 1, start, 0, 0); ++c+0x548 |
| 6B | 87A4F4 | ? | - |  |
| 6C | 2F0DC54 | ? | DrawIndexedInstanced, OMSetRenderTargets, RSSetScissorRects, RSSetViewports, ResourceBarrier |  |
| 6D | 9F3484 | ? | DrawInstanced, OMSetRenderTargets, RSSetScissorRects, RSSetViewports, ResourceBarrier |  |
| 6E | 6ECF48 | SynTokExecuteIndirect6E | ExecuteIndirect, OMSetRenderTargets, RSSetScissorRects, RSSetViewports, ResourceBarrier | tok 0x6E: SynFlushGfxStateForDraw, arg buffer -> obj+30 (stamped), ExecuteIndirect |
| 6F | 6EB908 | SynTokExecuteIndirect | ExecuteIndirect, OMSetRenderTargets, RSSetScissorRects, RSSetViewports, ResourceBarrier | tok 0x6F: state flush, transition the argument buffer (115E24), ExecuteIndirect |
| 70 | 45B670 | ? | - |  |
| 71 | 45B67C | ? | - |  |
| 72 | 8741EC | FlushPendingDrawsIfAny | - | if immediate-draw count (+0x2458) nonzero tail-jumps to flush routine (was SynFlushImmediateDrawsIfPending); function start 0x8741ec (row moved fro... |
| 73 | 115F0C | ? | DrawIndexedInstanced, IASetIndexBuffer, IASetVertexBuffers, OMSetRenderTargets, RSSetScissorRects, RSSetViewports, ResourceBarrier |  |
| 74 | 2F0DA40 | ? | DrawIndexedInstanced, IASetIndexBuffer, IASetVertexBuffers |  |
| 75 | 7A6AB0 | ? | DrawIndexedInstanced, IASetIndexBuffer, IASetVertexBuffers, OMSetRenderTargets, RSSetScissorRects, RSSetViewports, ResourceBarrier |  |
| 76 | 2F0DBB8 | ? | DrawIndexedInstanced, IASetIndexBuffer, IASetVertexBuffers |  |
| 77 | 2F0DB08 | ? | DrawIndexedInstanced, IASetIndexBuffer, IASetVertexBuffers, OMSetRenderTargets, RSSetScissorRects, RSSetViewports, ResourceBarrier |  |
| 78 | 2F0D9EC | ? | DrawIndexedInstanced, IASetIndexBuffer, IASetVertexBuffers, OMSetRenderTargets, RSSetScissorRects, RSSetViewports, ResourceBarrier |  |
| 79 | 2F0DAD0 | ? | - |  |
| 7A | 45C7E8 | SynTokCopyTrackedResource | CopyTextureRegion, DrawIndexedInstanced, IASetIndexBuffer, IASetVertexBuffers, OMSetRenderTargets, RSSetScissorRects, RSSetViewports, ResourceBarrier | reads two handles from token stream and calls SynGenericCopyTracked, cookie-protected (was SynTokGenericCopyTrackedRead) |
| 7B | 45DD48 | ? | CopyTextureRegion, DrawIndexedInstanced, IASetIndexBuffer, IASetVertexBuffers, OMSetRenderTargets, RSSetScissorRects, RSSetViewports, ResourceBarrier |  |
| 7C | 8BB170 | SynTokComputeBegin | - | KTGL tok 0x7C: compute section begins on the graphics list (c+0x541 = 1, dirty 0x40) |
| 7D | 8988A8 | SynTokComputeEnd | ResourceBarrier | KTGL tok 0x7D: UAV barrier (1139D4), dirty 0x20, c+0x541 = 0 |
| 7E | 58BDC4 | SynTokDispatch | Dispatch, SetComputeRootDescriptorTable, SetPipelineState | tok 0x7E: SynFlushComputeState then Dispatch |
| 7F | 2F0D970 | ? | +0, OMSetRenderTargets, RSSetScissorRects, RSSetViewports, ResourceBarrier |  |
| 80 | 2F0D9B4 | ? | - |  |
| 81 | 58BC74 | SynTokExecuteIndirectDispatch | ExecuteIndirect, ResourceBarrier, SetComputeRootDescriptorTable, SetPipelineState | tok 0x81: SynFlushComputeState, arg buffer -> obj+30 (stamp via 114C04), ExecuteIndirect |
| 82 | 9AFDF0 | ? | - |  |
| 83 | 9A62FC | ? | - |  |
| 84 | 2F0DCBC | ? | - |  |
| 85 | 2F0DCD4 | ? | - |  |
| 86 | 7F6118 | SynTokReadStateBlock128DispatchB | DrawIndexedInstanced, IASetIndexBuffer, IASetVertexBuffers, OMSetRenderTargets, RSSetScissorRects, RSSetViewports, ResourceBarrier | twin of 115a70: decodes 128-byte state block, calls RenderSubmitHot_115B9C with mode 1 (was SynTokBindTextureSlotReadB) |
| 87 | 698A44 | SynTokBindTexture87 | DrawIndexedInstanced, IASetIndexBuffer, IASetVertexBuffers, OMSetRenderTargets, RSSetScissorRects, RSSetViewports, ResourceBarrier | tok 0x87: texture bind, X=(cur&0xAC3)/stage bit (R1 superset source), no stamp |
| 88 | 698B90 | SynTokBindSrvDescriptor | DrawIndexedInstanced, IASetIndexBuffer, IASetVertexBuffers, OMSetRenderTargets, RSSetScissorRects, RSSetViewports, ResourceBarrier | reads packed token (slot/stage), flushes immediate draws and copies descriptors (SynCopyDescriptorSimple1) (was SynBindUavSlotIfChanged) |
| 89 | 2F0DCA4 | ? | - |  |
| 8A | 113A0C | SynTokUavBarrier | ResourceBarrier | tok 0x8A: ResourceBarrier(1, UAV, resource from token; NULL = global); emitted by GpuParticleSimRecord 694B38 once after each non-empty GPU-particl... |
| 8B | 7A5D2C | SynTokBufferCopyTransitions | ResourceBarrier | KtglTokenHandlerTable[0x8B]: decodes 3 u32 + 2 u64 from the token stream and calls SynBufferCopyTransitions (was Unknown_Gfx_7a5d2c) |
| 8C | 2F0D858 | SynCopyBufferRegion | CopyBufferRegion | SynRecord subsystem; D3D12: CL.CopyBufferRegion (sites 2F0D8D2) |
| 8D | 2F0D8EC | ? | - |  |
| 8E | A96D90 | SynTokSetMarkerIface | - | token 0x8E: [4BD09C0] = qword from the stream |
| 8F | 6B8884 | ? | - |  |
| 90 | 3D4FC4 | CmdSubStreamRun | - | token handler: reads {object, begin, end} from the stream and runs the u16 opcodes of [begin,end) through the handler table 3C02800 on object+68h (... |
| 91 | 9E2A74 | ? | CopyTextureRegion, ResourceBarrier |  |
| 92 | 7F9A6C | ? | ResourceBarrier |  |
| 93 | 2F0DFEC | ? | +0, ResourceBarrier |  |
| 94 | 7B1750 | SynTokClearUavUint | ClearUAVUint, ResourceBarrier | reads clear value/handles from token stream and calls SynClearUavUintTracked with __chkstk locals (was SynTokClearUavUintRead) |
| 95 | 7F64B4 | ? | - |  |
| 96 | 2F0D750 | ? | - |  |
| 97 | 2F0D774 | ? | DrawIndexedInstanced, IASetIndexBuffer, IASetVertexBuffers, OMSetRenderTargets, RSSetScissorRects, RSSetViewports, ResourceBarrier |  |
| 98 | 891F24 | ? | - |  |
| 99 | 981220 | ? | - |  |
| 9A | 2F0D684 | ? | - |  |
| 9B | 7F52AC | SynTokTransitionResource | ResourceBarrier | tok 0x9B: explicit state-tracked transition from the stream (resource, state slot, new state, stamp, subresource); skips equal states |
| 9C | 7C67E0 | SynResourceBarrier_7C67E0 | - | SynRecord subsystem; D3D12: CL.ResourceBarrier (sites 7C68CC) |
| 9D | 84EFF4 | ? | - |  |
| 9E | 8F2C90 | ? | - |  |
| 9F | 8962D8 | ? | - |  |
| A0 | A96D70 | SynTokSetMarkerReplay | - | token 0xA0: c+542h = byte from the stream (the engine's own replay switch) |
| A1 | 2F0E318 | ? | - |  |
| A2 | 2F0E39C | ? | - |  |
| A3 | 2F0E4A4 | ? | - |  |
| A4 | 2F0E528 | ? | - |  |
| A5 | 114024 | SynTokBeginEvent | BeginEvent, DrawIndexedInstanced, IASetIndexBuffer, IASetVertexBuffers, OMSetRenderTargets, RSSetScissorRects, RSSetViewports, ResourceBarrier | token 0xA5: unpack colour + name, call SynBeginEvent 113E64 |
| A6 | 113FC8 | SynTokEndEvent | DrawIndexedInstanced, EndEvent, IASetIndexBuffer, IASetVertexBuffers, OMSetRenderTargets, RSSetScissorRects, RSSetViewports, ResourceBarrier | token 0xA6: EndEvent (vtbl+1D0h) when byte c+542h |
| A7 | 8CEBC0 | SynTokWriteMarkerBBBB | DrawIndexedInstanced, IASetIndexBuffer, IASetVertexBuffers, OMSetRenderTargets, RSSetScissorRects, RSSetViewports, ResourceBarrier | token handler: flushes immediate draws and overwrites the next stream dword with 0xBBBBBBBB (cpu100-u0, decompile-read) (was Unknown_Draw_8cebc0) |
| A8 | 495FC4 | SynTokCloseCmdListAndHash | DrawIndexedInstanced, IASetIndexBuffer, IASetVertexBuffers, OMSetRenderTargets, RSSetScissorRects, RSSetViewports, ResourceBarrier | reads handle from tokens, SynCloseCmdList then hash-bucket stores the list (was SynTokCloseCmdListAndStore) |
| A9 | 905480 | ? | - |  |
| AA | 2F0E650 | ? | - |  |
| AB | 8C16AC | SynTokReadThreeQwordsDispatch | - | reads 3 qwords from command stream and calls 0x8a00b8 with two state sub-objects at +0x5a0/+0x588 (was SynTokReadThreeQwordArgs) |
| AC | 2F0D634 | ? | - |  |
| AD | 2F0DD80 | ? | - |  |
| AE | 2F0DF30 | SynResolveQueryData | - | SynRecord subsystem; D3D12: CL.ResolveQueryData (sites 2F0DFCD) |
| AF | 7543BC | ? | - |  |
| B0 | 2F0D678 | ? | - |  |
| B1 | 2F0DDC4 | ? | - |  |
| B2 | 2F0D5FC | ? | - |  |
| B3 | 2F0DD54 | ? | - |  |
| B4 | 2F0D5D8 | ? | - |  |
| B5 | 2F0DD30 | ? | - |  |
| B6 | 2F0D608 | ? | - |  |
| B7 | 2F0DD60 | ? | - |  |
| B8 | 2F0E45C | ? | - |  |

## 9. Files

- `mod/Nioh3EngineFix/src/bkstats.{h,cpp}`, `tests/bkstats_harness.cpp`, `tests/run_bkstats_harness.py` (branch renderer-backend).
- Scripts used (scratch, not committed): capstone disassembly / IAT scans for the SRW holders (srwscan), token emitters (tokemit),
  token-handler classification (tokclass: payload advance, CL methods from `D:\nioh3-re\d3d12\vcalls.tsv`, callees, tail targets).
- Existing: `re\scripts_d3d12\toktab.py` (table dump), `vcalls.fsx`, `d3d12_sites.tsv`.
