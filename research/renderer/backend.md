# renderer/backend: the D3D12 back end (token stream, replay thread, rings, fences, submission) and a new-backend assessment

2026-10-10, package 01 of research/renderer/00-map.md. Nioh3.exe Steam v2.02, RVAs (Ghidra base 0x7FF74F410000). Tags: **S** static
(decompile / disassembly, cited), **M** measured (pc2 captures, cited), **E** estimate. Inputs: cpu-bound-midrange.md, d3d12-submission.md,
d3d12-sites.md, gpu-tiles.md, gpu-scheduling-sync.md, pc-submit-overhead.md and fresh decompiles of every function named below
(`<re-scratch>\decomp` + `decomp_ro.cmd`). The game was not run on this PC. Symbols: block `# renderer/backend` in re/symbols/symbols.tsv.
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
3. **The three "blocked" rows are not locks, barriers or a full queue: they are the dispatcher's own idle wait (M, pc2 CPU5).**
   SP Focus 2 names the system export: all three are `ntdll!NtWaitForSingleObject` (<- 1139D4 7.9 %, <- 7F6904 7.6 %, <- 86F760 5.8 %
   of ticks = 21.3 %). An SRW wait would be NtWaitForAlertByThreadId, and the engine's own timers say ECL ring full = **0 waits**,
   KtglCache lock contended = **0.0 %** of samples (held 0.2 %). The only INFINITE NtWaitForSingleObject on that thread is the
   dispatcher's semaphore wait in its loop (1A7C3E0); the SP stack scan attributes it to stale return slots of the deepest recent calls
   (barrier / tok 0x95 / ECL push), as it already did for 1180A4. BKS measures the same thing directly: **dispatcher starved 20.5 %
   of samples** (ring empty, nothing in flight) - the same 21 %. So the replay thread is idle ~5 ms per frame, mostly while the main
   thread is still recording (starved in 25.6 % of the recording-phase samples), and backlogged at the end (main in drain 20.4 %,
   engine drain timer 5.09 ms/frame).
   The SRW lock itself (S): KtglCache+CF8h, shared once per frame by token 0x95 (7F64DC; across an upload-ring allocation that can
   submit + spin on a fence, two CopyBufferRegion batches and per-object updates), exclusive in cache insert/evict 250ADC and the
   tile-mapping commit 2EEF440. Correct but not a bottleneck on pc2.
4. **So the wall is the frame structure, not blocking (M + S):** the dispatcher has ~15 ms of real work per frame, sits idle ~5 ms
   while the main thread prepares, then the main thread waits ~5 ms for it to finish. KL (hand-off every 16 instead of 64 draws)
   gave **+1.2 % fps (SE 0.04, 4/4 pairs positive)** and drain 5.09 -> 4.61 ms; starvation did not change (25.0 %), i.e. the idle time
   comes from stretches where the main thread produces no tokens, which only overlap of frames can fill.
5. **FCAP1 confirms the queue model and adds one fact (M, section 4.1):** 18-22 DIRECT ExecuteCommandLists per frame, all from the ECL
   thread, and **all of a frame's DIRECT and COMPUTE lists are submitted in two bursts in the last ~11 % of the CPU frame**: closed
   lists are held in the list ring until a submit point.
6. **Rewrite verdict (E, section 6):** a new back end that consumes the same token stream is feasible and is the right level; the
   win comes from **pipelining** (letting frame N+1's recording overlap frame N's replay), not from parallel recording alone.
   The design-time estimate for pc2 was up to ~+25-30 % (E); parallel recording adds little on pc2 because the main thread
   (~19.7 ms) becomes the bound. Effort 4-8 weeks for an A/B-able ASI mode; the risk is lifetime bugs in token payloads.
   **The pipelining prototype (frame overlap, renderer/frame-overlap.md section 7.3) measured +0.4 % on pc2**: the drain is
   the replay thread's real work, chained to the main thread by a GPU->CPU sync point, so the estimate does not hold.
7. **First experiment built and measured (section 7):** BKS census (no hooks: the engine's own wait timers + 1 kHz queue sampler) and
   **KL**, a live data knob for the hand-off granularity (CmdFlushLimit / n): pc2 CPU5 +1.2 % at KL_Div 4 (stock limit 64 -> 16).

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

### 4.1 Check against FCAP1 (pc2, 5 captures x 2 frames, XSYS config; M)

The FC logger recorded no command-list calls in this run (its list hooks were not active), but every queue operation is there
with thread id and caller, which is what this model needs:
- **DIRECT: 18-22 ExecuteCommandLists per frame, each from EclSubmitAndSignal (return 4F9114) followed by its Signal (4F9127), all on
  one thread (the ECL thread)**, plus 2 per frame from another module (FFX / OSD). COMPUTE: 4-6 per frame through ECL token 6 (4F925F),
  each preceded by an ECL token 8 Wait (return 4F908B in EclReplayTokens). DIRECT queue Waits: 3 via ECL token 2 -> EclWait 4F92A4 and
  3 via GfxWait 8F4548. The 8F4548 waits are on COPY-queue fences (streaming uploads); the async-compute joins are the
  4F9323 waits (Capture-check at the end). COPY: texture uploads (GfxSubmitSegmentVirtCalls 4DA0C, up to 18 per frame in streaming
  bursts, other threads), 892D88 (3 per frame), CmdListRingCloseAndSubmit 5C268; UpdateTileMappings only through
  CacheTileBatchFlushDirect 290F94 on the cache COPY queue (0-63 per frame), none through ECL token 4 in these captures.
- **Submission is batched at the end of the CPU frame.** In every captured frame the DIRECT lists reach the queue in two bursts:
  ~15 lists within ~0.6 ms at ~89 % of the frame (21.2-21.8 ms of 23.8) and ~6 lists at 99 % (23.6-23.8 ms), right before Present;
  between ~0.6 ms and ~21 ms **nothing** is submitted to DIRECT or COMPUTE. Cause (S): a closed list is pushed to the ECL ring only
  at a submit point (CmdListRingSubmitUpTo from a submit item, CmdListRingSubmitIfBehind, or when its ring slot must be recycled:
  CmdListRingFlushAndRecycle 4920E0 pushes only up to the slot it reuses), so mid-frame cuts close lists but do not submit them.
  The same pattern is in the RX 9070 RGP trace ("all GFX lists within a 2.5-3.2 ms burst at the end of the CPU frame",
  d3d12-submission.md 5). Consequences: the GPU starts frame N only after the dispatcher has replayed almost all of it (latency, and
  GPU idle when the CPU frame is long), and the ECL thread sees ~20 items arrive at once (its ring still never fills: CPU5, section 5). Early submission (push each list when it is closed) is a cheap complement to option 1.

## 5. What blocks the replay thread: reading the three SP rows correctly (S + M CPU5)

The SP sampler attributes a sample whose RIP is outside the exe to the **first return address into the exe found on the copied
stack** (sp.cpp). Stale return slots below the live frames win when the system frames do not overwrite them. CPU5 (pc2, SP Focus 2 +
BKS, diag window) settles all three rows:

| SP row (CPU3) | export (SPW, CPU5) | engine timer / BKS sample (CPU5) | verdict |
|---|---|---|---|
| `[ntdll] <- 1139D4` 7.5 % | NtWaitForSingleObject 7.9 % | - | stale slot: 1139D4 only calls ResourceBarrier; a driver wait would sit under amdxc64 frames |
| `[ntdll] <- 7F6904` 6.8 % | NtWaitForSingleObject 7.6 % | cache SRW +CF8h waiters **0.0 %**, held 0.2 % | stale slot (7F6904 never calls ntdll; an SRW wait is NtWaitForAlertByThreadId) |
| `[ntdll] <- 86F760` 5.5 % | NtWaitForSingleObject 5.8 % | ECL stall **0.00 ms/frame, 0 waits**; ECL high water 4 of 128 | stale slot; the ECL ring is never full |
| sum 19.8 % | 21.3 % | **dispatcher starved 20.5 %** of samples | = the dispatcher's own semaphore wait in 1A7C3E0 (idle, no work) |

Other CPU5 numbers (KL_Div 0, 10 s, 38.6 fps): dispatcher ring cap 256, never full; CmdFlushLimit 64 (36 kicks, 1660 draws, 10 MB of
stream per frame, buffer 48 MB); list ring 128 lists; drain 5.09 ms/frame (engine timer, max 10 ms); main in drain 20.4 % of samples;
in flight 12 segments on average. The dispatcher's real work is therefore ~15 ms/frame (~59 % run), idle ~5 ms, and the frame is
main-thread preparation (~20 ms, during which the dispatcher idles a quarter of the time) + the dispatcher's backlogged tail (~5 ms).

## 6. Rewrite feasibility: a new back end on the same token stream

**What the engine guarantees today:** one replay thread, tokens in order, resource states tracked by replaying in submission order,
all per-frame CPU data alive until the drain, 3 engine frame slots for GPU lifetime.

**Options (E; pc2 CPU5: main ~20.5 ms busy + 5.1 ms drain, dispatcher ~15 ms work + ~5 ms idle, no lock / queue blocking):**

| # | change | what it needs | pc2 frame (fps) | effort | risk |
|---|---|---|---|---|---|
| 0 | KL: smaller hand-off granularity (built) | data write | **M: +1.2 % (KL_Div 4: limit 64 -> 16), drain -0.5 ms** | done | none |
| 1 | **Pipelined frame end** (keep the replay thread): double-buffer the stream (+3C30h base), move the post-drain main-thread work (query resolves, PRESENT barrier, close / submit, present hand-off) into tokens the dispatcher executes, let RenderFrameBegin start N+1 while the dispatcher finishes N | lifetime audit of every pointer payload (185 tokens) against what the main thread frees / rewrites at frame begin; the frame-slot allocators (3 slots) already keep GPU-lifetime data; tok 0x95 / PagePoolFrameUpdate ordering; FFX / Streamline markers per frame | 26 -> ~20-21 ms (38 -> ~48 fps, +25 %, E): frame = max(main 19.7, dispatcher ~20). Prototype measured +0.4 % (frame-overlap.md 7.3) | 3-5 weeks | medium-high: a missed lifetime = rare corruption / device removal |
| 2 | Parallel replay (new back end): N replay threads on consecutive segments, each with its own SynCtx copy, list and descriptor / upload ring; lists submitted in order by the existing ECL thread | per-segment state reconstruction (SynBeginList already re-binds everything at list start), **barrier tracking moved to submit time** (record "needed state" per resource per list, resolve sequentially at submit with a small fix-up list), thread-safe PSO cache (has a CS), per-thread rings | with the drain kept: tail ~6 -> ~2 ms (**~+15-20 %**); with 1: no extra gain on pc2 (main becomes the bound), +gain only where the main thread is faster than replay (PC at high fps) | 4-8 weeks (00-map.md 5a) | high: 185 handlers' side effects on SynCtx state, driver-specific behaviour, vkd3d |
| 3 | Cheaper replay (no new thread): PCM-style memos, CBV / descriptor path, engine-side barrier batching, early submission of closed lists | small caves | CPU5 shows **no blocking to remove**; only replay work in the backlogged tail reaches the frame (PCM: ~20 % of the saving) -> **+1-3 %** unless combined with 1 | 1-2 weeks | low-medium |
| 4 | Fully new renderer / frame graph | see 00-map.md 5 b/c | same CPU ceiling as 1+2 | months-years | very high |

**Recommendation (after CPU5):** prototype **option 1** behind a default-off key, because it attacks the cause (no
overlap) with the smallest code surface. (It was prototyped as frame overlap, off by default: +0.4 % on pc2, see
renderer/frame-overlap.md section 7.3, which explains why the drain could not be hidden.) Option 1 is also the prerequisite that makes option 2 pay on any CPU. Option 2 alone is the
expensive way to get a smaller share of the same prize on pc2. The token-lifetime audit for option 1 is the main work item:
- tokens whose payload is inline (constants 0x66, state values) are safe by construction once the stream is double-buffered;
- tokens carrying pointers into per-frame-slot memory are safe (3 slots, GPU-fenced);
- tokens carrying pointers to engine objects that the main thread mutates during frame N+1 (pass records, view / RT descriptors,
  immediate-mode vertex staging +2458h, the KtglCache lists) need a copy or a slot; FCAP1's caller RVAs give the per-token counts to
  prioritise the audit.

Gain on other machines (E): the PC (RX 9070) is co-critical at high fps (pc-submit-overhead.md 3): option 1 helps there too; the Ally is
GPU-bound (~0).

## 7. First experiment: BKS census + KL kick limit (built, measured)

Branch `renderer-backend` (commit 8e30fc2; ASI 1.7.58-bk1, built together with the SP Focus 2 profiler build):
- **`[Profiler] Backend = 1`** (default 0), `BackendSampleUs = 1000`, `BackendSec`: no hooks, no code bytes. Reads the engine's own
  timers listed in section 1 per frame / per report, and samples every 1 ms: main in drain spin (g_DispatcherDrainBusy 45CE8A0) vs
  recording, **dispatcher starved while the main thread records** (ring empty, nothing in flight), in-flight segments, dispatcher / ECL
  ring full, KtglCache SRW +CF8h held / exclusive / waiters. Layout self-check of three byte runs (86F775, 418F46, 418F90) or it stays off.
  Two `BKS:` lines per report.
- **`[Tuning] KL_Div = 0`** (live: LiveCtl `Tuning.KL_Div = n`): CmdFlushLimit = max(16, engine value / n), restored on 0, re-asserted
  if the engine rewrites the field. Same tokens, lists and order; only the hand-off granularity changes (every reader compares at use time).
- Offline proof: `tests/run_bkstats_harness.py` (fake v2.02 image + fake engine thread: all report values reproduced, KL write / clamp /
  re-assert / restore / refuse) ALL PASSED; `tests/test_patterns.py` ALL 245 SITES OK.
- **pc2 CPU5**: Continue spot (CPU-bound), the SP Focus 2 profiler keys + Backend = 1,
  cpu_live_sched `Tuning.KL_Div=0/4` ABBA x4 (M):
  - **KL_Div 4: +0.48 fps = +1.2 % of 38.72, SE 0.04, every pair positive** (windows A 38.71 / 38.68 / 38.71 / 38.79, B 39.09 / 39.21 /
    39.27 / 39.24). `LIVE tuning: KL_Div = 4 (CmdFlushLimit 16, stock 64)`. Kicks 36 -> 85 per frame, drain 5.09 -> 4.61 ms/frame,
    starved-while-recording 25.6 -> 25.0 %. Passes the >= 1 % / SE < 0.3 rule: **KL is a keep candidate** (pure data, live, same
    output); next: KL_Div 2 / 8 and the Ally / PC no-harm check before any profile ships it.
  - Section 5's table comes from this run. BKS bug seen there (underflowed recycle / idle-spin / tok95 timers when the engine resets
    them) fixed in 384844e (reset-safe deltas, harness case added). The `submits/f` field reads 0: CmdListRing+11Ch is not a
    cumulative submit counter (it is reset per frame); treat it as unknown.

Not chosen as the first experiment: a deeper dispatcher ring (no evidence the main thread ever finds it full; BKS measures it), a
deeper ECL ring (it already grows with the list ring), splitting the KtglCache lock (needs the lock census first; option 3).

## 8. KTGL token table (3C02220, 185 entries)

185 of 185 handlers named (S: decompile / disassembly; payload layouts in the symbol descriptions). Handler signature `void h(SynCtx* c, u8** cursor)`.

| op | handler | name | D3D12 command-list methods reached (vcalls) | what it does |
|---|---|---|---|---|
| 00 | 7D9A60 | SynTokBeginRenderPass00 | - | tok 0x00: hdr = RT count (b0), flag (b1), byte2/hi16 args; reads 2 dwords + qword DSV + n qword RTVs and hands them to SynBeginRenderPass (push pas... |
| 01 | 8F0B44 | SynTokFencePoint01 | - | tok 0x01: adv4, reads one u32 and tail-jumps to SynTokFencePoint (list boundary + state reset) |
| 02 | 458E34 | SynCmdReplaySetRenderTargets | ClearRenderTargetView, ResourceBarrier | 463 B command-stream decoder (no direct callers, handler table): header u32 = RTV count (low byte) / flag (byte 1) / ...; reads up to 8 RTV handles... |
| 03 | 45A020 | SynTokCopyTextureRegion03 | CopyTextureRegion, ResourceBarrier | tok 0x03: src -> COPY_SOURCE, dst -> COPY_DEST, CopyTextureRegion, src restored (depth bits stripped); no stamp |
| 04 | 866638 | SynTokClearRtvDsv04 | ClearRenderTargetView, ResourceBarrier | tok 0x04: hdr byte2 = slot, byte3 = flag, 2 dwords; calls SynClearRtvDsvTracked (ClearRenderTargetView/ClearDepthStencilView with RENDER_TARGET/DEP... |
| 05 | 2F0D814 | SynTokClearRtvDsv05Entry | ClearRenderTargetView, ResourceBarrier | tok 0x05: hdr byte2/byte3, 4 dwords color/depth-stencil values + 1 dword, calls SynTokClearRtvDsv05 (2EFBDA4); consumes 24 B |
| 06 | 2F0E420 | SynTokResetDefaultRenderState06 | - | tok 0x06: adv4, tail-jumps SynSetDefaultRenderState (resets the cached fixed-function state to defaults) |
| 07 | 45A42C | SynTokReadDescriptorList384 | DrawIndexedInstanced, IASetIndexBuffer, IASetVertexBuffers, OMSetRenderTargets, RSSetScissorRects, RSSetViewports, ResourceBarrier | decodes token header, memsets 0x180 bytes and reads up to N entries from token stream |
| 08 | 45D020 | SynTokFlushDrawsIfStateChanged | DrawIndexedInstanced, IASetIndexBuffer, IASetVertexBuffers, OMSetRenderTargets, RSSetScissorRects, RSSetViewports, ResourceBarrier | token handler: reads packed token, flushes pending draws (+0x2458) if per-slot state (+0x170) differs |
| 09 | 699B18 | SynTokSetPerRtValue09 | DrawIndexedInstanced, IASetIndexBuffer, IASetVertexBuffers, OMSetRenderTargets, RSSetScissorRects, RSSetViewports, ResourceBarrier | KTGL tok 0x09: per-render-target u32 at c+0xA4[8] (slot 0xFF = all 8), mirrored to c+0x79C for enabled RTs (c+0x794), dirty c+0xB24 /= 0x20; flushe... |
| 0A | 111B3C | SynCmdReplaySetStateVariant16E | DrawIndexedInstanced, IASetIndexBuffer, IASetVertexBuffers, OMSetRenderTargets, RSSetScissorRects, RSSetViewports, ResourceBarrier | 152 B command-stream decoder (no direct callers); reads u32, flag = hi16 != 0; if it differs from ctx+16Eh: SynFlushImmediateDraws when +2458h pend... |
| 0B | 111CBC | SynTokSetAlphaTest0B | DrawIndexedInstanced, IASetIndexBuffer, IASetVertexBuffers, OMSetRenderTargets, RSSetScissorRects, RSSetViewports, ResourceBarrier | tok 0x0B: hdr byte2 = alpha func (ctx+0xE0), byte3 = alpha ref (ctx+0xE4); flushes immediate draws on change and calls SynUpdateAlphaTestConstant |
| 0C | 45B208 | SynTokSetFlag16F | DrawIndexedInstanced, IASetIndexBuffer, IASetVertexBuffers, OMSetRenderTargets, RSSetScissorRects, RSSetViewports, ResourceBarrier | token handler: when bit16 of the token changes the flag +16Fh, flushes immediate draws, updates +780h bit0 and marks state dirty (+B24h/20h)... |
| 0D | 45E52C | SynTokSetStateE8_0D | DrawIndexedInstanced, IASetIndexBuffer, IASetVertexBuffers, OMSetRenderTargets, RSSetScissorRects, RSSetViewports, ResourceBarrier | tok 0x0D: hdr hi16 -> ctx+0xE8; low 3 bits packed into ctx+0x780 bits 2-4, dirty OR 0x20 (likely a 3-bit raster/blend mode) |
| 0E | 45E5E0 | SynTokFlushDrawsIfStateChangedB | DrawIndexedInstanced, IASetIndexBuffer, IASetVertexBuffers, OMSetRenderTargets, RSSetScissorRects, RSSetViewports, ResourceBarrier | token handler: flushes draws if flag byte (+0x188) differs from token bit, clears bit1 of +0x780 |
| 0F | 797384 | SynTokFlushDrawsIfStateChangedC | DrawIndexedInstanced, IASetIndexBuffer, IASetVertexBuffers, OMSetRenderTargets, RSSetScissorRects, RSSetViewports, ResourceBarrier | token handler: flushes draws if flag byte (+0x16d) differs from token bit |
| 10 | 696DA8 | SynOMSetStencilRef | DrawIndexedInstanced, IASetIndexBuffer, IASetVertexBuffers, OMSetRenderTargets, OMSetStencilRef, RSSetScissorRects, RSSetViewports, ResourceBarrier | SynRecord subsystem; D3D12: CL.OMSetStencilRef (sites 696E68) |
| 11 | 79E6B8 | SynCmdSetStencilOps | DrawIndexedInstanced, IASetIndexBuffer, IASetVertexBuffers, OMSetRenderTargets, RSSetScissorRects, RSSetViewports, ResourceBarrier | 375 B command-stream handler: reads packed u32 (fail b0, depth-fail b1, pass hi16), if changed vs cache +0xD0/+0xD4/+0xD8 flushes SynFlushImmediate... |
| 12 | 79E5E8 | SynTokSetStateDC_12 | - | tok 0x12: hdr hi16 -> ctx+0xDC; updates ctx+0x785 byte (gated by 0x780 mode bits) and dirty flags |
| 13 | 2F0E1D8 | SynTokSetRasterMode9C_13 | - | tok 0x13: hdr hi16 -> SynSetRasterModeAt9C (ctx+0x9C validated against a mode table; bit 16 of ctx+0x784 when mode==2) |
| 14 | 45B154 | SynTokSetStateBits98_14 | DrawIndexedInstanced, IASetIndexBuffer, IASetVertexBuffers, OMSetRenderTargets, RSSetScissorRects, RSSetViewports, ResourceBarrier | tok 0x14: hdr hi16 -> ctx+0x98, low 2 bits into ctx+0x784 bits 17-18, dirty OR 0x20 |
| 15 | 861294 | SynTokSetFlag189_15 | DrawIndexedInstanced, IASetIndexBuffer, IASetVertexBuffers, OMSetRenderTargets, RSSetScissorRects, RSSetViewports, ResourceBarrier | tok 0x15: boolean = (hdr hi16 != 0) stored at ctx+0x189, dirty OR 8; flushes immediate draws first |
| 16 | 458CC0 | SynTokReadDescriptorList256 | DrawIndexedInstanced, IASetIndexBuffer, IASetVertexBuffers, OMSetRenderTargets, RSSetScissorRects, RSSetViewports, ResourceBarrier | decodes token header, memsets 0x100 bytes and reads up to N descriptor entries from token stream |
| 17 | 45BB70 | SynTokSetPolygonOffsetEnable17 | - | tok 0x17: boolean from hdr hi16, tail-jumps SynSetPolygonOffsetEnable (ctx+0x18B) |
| 18 | 45BB8C | SynTokSetPolygonOffset18 | - | tok 0x18: reads 2 floats (slope scale, constant bias) and tail-jumps SynSetPolygonOffset (ctx+0x198/0x19C) |
| 19 | 7865F4 | SynTokSetRtColorWriteMask19 | DrawIndexedInstanced, IASetIndexBuffer, IASetVertexBuffers, OMSetRenderTargets, RSSetScissorRects, RSSetViewports, ResourceBarrier | tok 0x19: hdr byte2 = RT index, byte3 bits 0-2 = RGB write mask, hi nibble flag = alpha; stores ctx+0x178/0x180[idx] and packs 4 bits per RT into c... |
| 1A | 2F0DCEC | SynTokSetStateFlag16C_1A | - | tok 0x1A: boolean from hdr hi16 -> SynSetStateFlag16C (ctx+0x16C, ctx+0x784 bit 19) |
| 1B | A19564 | SynTokSetClipPlaneMask1B | - | tok 0x1B: adv4, one u32 mask -> SynSetClipPlaneMask (ctx+0x1A0) |
| 1C | 9B48F0 | SynTokSetClipPlane1C | - | tok 0x1C: hdr hi16 = plane index, 4 dwords plane equation (20 B) -> SynSetClipPlaneVec4 |
| 1D | 2F0DD08 | SynTokSetStateFlag18A_1D | - | tok 0x1D: boolean from hdr hi16 -> SynSetStateFlag18A (ctx+0x18A, ctx+0x780 bit 30) |
| 1E | 2F0E48C | SynTokSetStateAt190_1E | - | tok 0x1E: adv4, one u32 -> SynSetStateAt190 (ctx+0x190; -1/0xFFFF sentinel handling, ctx+0x780 bit 31) |
| 1F | 2F0E444 | SynTokSetPointModeAt230_1F | - | tok 0x1F: hdr hi16 -> SynSetPointModeAt230 (ctx+0x230, pushed to stage-3 constant slot) |
| 20 | 2F0E42C | SynTokSetPointSizeAt234_20 | - | tok 0x20: reads float at +4 -> SynSetPointSizeAt234 (ctx+0x234, pushed to stage-3 constant slot) |
| 21 | 45D148 | SynTokSetRenderStateMode | DrawIndexedInstanced, IASetIndexBuffer, IASetVertexBuffers, OMSetRenderTargets, RSSetScissorRects, RSSetViewports, ResourceBarrier | token handler: mode = token>>16 plus object; when changed flushes immediate draws, stores +194h/+60h and binds the matching render-state object (Sy... |
| 22 | 2F0E164 | SynTokSetShaderProgramAt50_22 | - | tok 0x22: adv4, qword pointer -> SynSetShaderProgramAt50 (ctx+0x50) |
| 23 | 1164C4 | SynTokSetPipelineLayout | DrawIndexedInstanced, IASetIndexBuffer, IASetVertexBuffers, OMSetRenderTargets, RSSetScissorRects, RSSetViewports, ResourceBarrier | 323 B dispatcher token handler: pops a layout/program object L; if != c+48h: SynFlushImmediateDraws when c+2458h, sets dirty c+B24h/=30h, stores L ... |
| 24 | 45C7C4 | SynTokSetStageProgramAt68_24 | - | tok 0x24: adv4, qword pointer -> SynSetStageProgramAt68 (stage slot 1) |
| 25 | 45C7A0 | SynTokSetStageProgramAt70_25 | - | tok 0x25: adv4, qword pointer -> SynSetStageProgramAt70 (stage slot 2) |
| 26 | 45BC2C | SynTokSetStageProgramAt78_26 | - | tok 0x26: adv4, qword pointer -> SynSetStageProgramAt78 (stage slot 3) |
| 27 | 2F0E210 | SynTokSetStageProgramAt88_27 | - | tok 0x27: adv4, qword pointer -> SynSetStageProgramAt88 (stage slot 5) |
| 28 | 2F0E274 | SynTokSetStageProgramAt90_28 | - | tok 0x28: adv4, qword pointer -> SynSetStageProgramAt90 (stage slot 6) |
| 29 | 114750 | SynCmdSetRootSignature | - | 294 B; decodes pointer from cmd stream, if changed marks dirty flags (+0xb24 /0x20 etc.) comparing per-table counts (0x44..0x50) |
| 2A | 1134E4 | SynTokSetProgramAt80 | - | Syn command-token handler (table 3C02370, next to SynCmdSetRootSignature): pops a program/layout ptr P; if != ctx+80h: dirty B24h/=40h, stores P at... |
| 2B | 115988 | SynTokSetCbDataSlot | - | token handler: reads {size<<18, slot<<28 / offset} and copies the inline blob into the per-slot constant shadow c+BB0h[slot]+offset, sets dirty bit... |
| 2C | 1146DC | SynTokUploadConstants2C_Stage0 | - | KTGL tok 0x2C (handler table 0x3C02220 slot 0x2C): decodes header and calls SynUploadConstantsToRing(stage 0) (decompile-verified) |
| 2D | 907384 | SynTokCbShadowWrite_Stage1_2D | - | tok 0x2D: stub, stage=1, tail-jumps SynCbShadowWriteStage |
| 2E | 2F0E264 | SynTokUploadConstants_Stage1_2E | - | tok 0x2E: stub, stage=1, tail-jumps SynUploadConstantsStageWorker |
| 2F | 9055A0 | SynTokCbShadowWrite_Stage2_2F | - | tok 0x2F: stub, stage=2, tail-jumps SynCbShadowWriteStage |
| 30 | 900824 | SynTokUploadConstants_Stage2_30 | - | tok 0x30: stub, stage=2, tail-jumps SynUploadConstantsStageWorker |
| 31 | 9061A4 | SynTokCbShadowWrite_Stage3_31 | - | tok 0x31: stub, stage=3, tail-jumps SynCbShadowWriteStage |
| 32 | 2F0E254 | SynTokUploadConstants_Stage3_32 | - | tok 0x32: stub, stage=3, tail-jumps SynUploadConstantsStageWorker |
| 33 | 2F0E234 | SynTokCbShadowWrite_Stage5_33 | - | tok 0x33: stub, stage=5, tail-jumps SynCbShadowWriteStage |
| 34 | 2F0E244 | SynTokUploadConstants_Stage5_34 | - | tok 0x34: stub, stage=5, tail-jumps SynUploadConstantsStageWorker |
| 35 | 2F0E298 | SynTokCbShadowWrite_Stage6_35 | - | tok 0x35: stub, stage=6, tail-jumps SynCbShadowWriteStage |
| 36 | 2F0E2A8 | SynTokUploadConstants_Stage6_36 | - | tok 0x36: stub, stage=6, tail-jumps SynUploadConstantsStageWorker |
| 37 | 115EA4 | SynTokSetCbData_Tok37 | - | KtglTokenHandlerTable[0x37]: decodes header (size = w0>>18 dwords), slot = w1>>28, offset = w1&0xFFFF; Memcpy inline blob into SynCtx+0xD70[slot] s... |
| 38 | 11579C | SynTokUploadConstants38_Stage4 | - | KTGL tok 0x38 (handler table 0x3C02220 slot 0x38): decodes count/stride header and calls SynUploadConstantsToRing(stage 4) (decompile-verified)... |
| 39 | 11676C | SynTokSetCbData_Tok39 | - | KtglTokenHandlerTable[0x39]: same as tok 0x37 but into per-slot shadow SynCtx+0xEC0[slot] with dirty bit in +0xB8C (another stage's CbvDirtyMask wo... |
| 3A | 861CF8 | SynCmdReplaySetConstantsStage7 | - | 117 B command-stream decoder (no direct callers); reads header (elemSize = hdr>>16, payload stride hdr>>18), slot, start (lo16) / count (hi16), adv... |
| 3B | 45B034 | SynTokSetPtrAt38Dirty10 | - | Syn command-token handler (table 3C023F8, next to SynTokSetVertexBufferTracked): pops a u64; if != ctx+38h stores it and sets dirty B24h/=10h; dire... |
| 3C | 116260 | SynTokSetVertexBufferTracked | IASetVertexBuffers, ResourceBarrier | tok 0x3C: VB slot bind: X=(cur&0xAC3)/vb+48 via SynTransitionResource (stamp vb+28), then IASetVertexBuffers(slot,1) (vtbl+0x160) |
| 3D | 114A20 | SynTokSetIndexBuffer | IASetIndexBuffer, ResourceBarrier | dispatcher token 0x3D: reads the IB object from the stream, skips if unchanged (c+30h), else IASetIndexBuffer view |
| 3E | 8DAEEC | SynTokBindTexturesStage0 | - | Syn command-token handler (table 3C02410): tail-calls 115B08(stage 0, ctx, ip), which decodes the slot list and calls SynBindTextureSlot; leaf (no ... |
| 3F | 2F0E2C8 | SynTokBindTextureSlotState_Stage1_3F | - | tok 0x3F: stub, stage=1, tail-jumps SynBindTextureSlotStateBlockStage |
| 40 | 9142FC | SynTokBindTextureSlotState_Stage2_40 | - | tok 0x40: stub, stage=2, tail-jumps SynBindTextureSlotStateBlockStage |
| 41 | 2F0E200 | SynTokBindTextureSlotState_Stage3_41 | - | tok 0x41: stub, stage=3, tail-jumps SynBindTextureSlotStateBlockStage |
| 42 | 2F0E1B8 | SynTokBindTextureSlotState_Stage5_42 | - | tok 0x42: stub, stage=5, tail-jumps SynBindTextureSlotStateBlockStage |
| 43 | 2F0E308 | SynTokBindTextureSlotState_Stage6_43 | - | tok 0x43: stub, stage=6, tail-jumps SynBindTextureSlotStateBlockStage |
| 44 | 115A70 | SynTokReadStateBlock128Dispatch | DrawIndexedInstanced, IASetIndexBuffer, IASetVertexBuffers, OMSetRenderTargets, RSSetScissorRects, RSSetViewports, ResourceBarrier | decodes 128-byte state block from token stream into SynBindTextureSlot 115B9C |
| 45 | 84196C | SynTokBindTextureSlotState_Stage7_45 | DrawIndexedInstanced, IASetIndexBuffer, IASetVertexBuffers, OMSetRenderTargets, RSSetScissorRects, RSSetViewports, ResourceBarrier | tok 0x45: stage 7 inline variant: hdr hi16 = slot, memset 128 B state block, SynTokReadStateBlock128 then SynBindTextureSlot(stage 7) |
| 46 | 747190 | SynTokClearTextureSlot_Stage0_46 | DrawIndexedInstanced, IASetIndexBuffer, IASetVertexBuffers, OMSetRenderTargets, RSSetScissorRects, RSSetViewports, ResourceBarrier | tok 0x46: hdr hi16 = slot; SynBindTextureSlot(stage 0, slot, null, null) |
| 47 | 2F0D7F4 | SynTokClearTextureSlot_Stage1_47 | - | tok 0x47: stub, stage=1, tail-jumps SynClearTextureSlotStage |
| 48 | 747180 | SynTokClearTextureSlot_Stage2_48 | - | tok 0x48: stub, stage=2, tail-jumps SynClearTextureSlotStage |
| 49 | 2F0D7E4 | SynTokClearTextureSlot_Stage3_49 | - | tok 0x49: stub, stage=3, tail-jumps SynClearTextureSlotStage |
| 4A | 2F0D7D4 | SynTokClearTextureSlot_Stage5_4A | - | tok 0x4A: stub, stage=5, tail-jumps SynClearTextureSlotStage |
| 4B | 2F0D804 | SynTokClearTextureSlot_Stage6_4B | - | tok 0x4B: stub, stage=6, tail-jumps SynClearTextureSlotStage |
| 4C | 7471E8 | SynTokClearTextureSlot_Stage4_4C | DrawIndexedInstanced, IASetIndexBuffer, IASetVertexBuffers, OMSetRenderTargets, RSSetScissorRects, RSSetViewports, ResourceBarrier | tok 0x4C: hdr hi16 = slot; SynBindTextureSlot(stage 4, slot, null, null) |
| 4D | 7471BC | SynCmdClearTextureSlotStage7 | DrawIndexedInstanced, IASetIndexBuffer, IASetVertexBuffers, OMSetRenderTargets, RSSetScissorRects, RSSetViewports, ResourceBarrier | 43 B command-stream handler: reads u32 from the cursor, advances 4 B, SynBindTextureSlot(c, stage 7, slot = v >> 16, null, 0) i.e. unbinds that tex... |
| 4E | 113BA4 | SynTokBindBufferSrv4E | DrawIndexedInstanced, IASetIndexBuffer, IASetVertexBuffers, OMSetRenderTargets, RSSetScissorRects, RSSetViewports, ResourceBarrier | tok 0x4E: buffer SRV bind, cache c+0x1118, X=(cur&0xA83)/NPSR, stamp from the stream |
| 4F | 989F2C | SynTokBindBufferSrv_Stage1_4F | - | tok 0x4F: stub, stage=1, tail-jumps SynBindBufferSrvStage |
| 50 | 90FA3C | SynTokBindBufferSrv_Stage2_50 | - | tok 0x50: stub, stage=2, tail-jumps SynBindBufferSrvStage |
| 51 | 9E5858 | SynTokBindBufferSrv_Stage3_51 | - | tok 0x51: stub, stage=3, tail-jumps SynBindBufferSrvStage |
| 52 | 2F0E188 | SynTokBindBufferSrv_Stage5_52 | - | tok 0x52: stub, stage=5, tail-jumps SynBindBufferSrvStage |
| 53 | 2F0E2D8 | SynTokBindBufferSrv_Stage6_53 | - | tok 0x53: stub, stage=6, tail-jumps SynBindBufferSrvStage |
| 54 | 115084 | SynTokBindBufferSrv54 | DrawIndexedInstanced, IASetIndexBuffer, IASetVertexBuffers, OMSetRenderTargets, RSSetScissorRects, RSSetViewports, ResourceBarrier | tok 0x54: buffer SRV bind, cache c+0x1718, stamped (twin of 8507B0) |
| 55 | 815DB0 | SynCmdBindResourceSlot | DrawIndexedInstanced, IASetIndexBuffer, IASetVertexBuffers, OMSetRenderTargets, RSSetScissorRects, RSSetViewports, ResourceBarrier | 262 B; decodes cmd (slot=hi16), flushes immediate draws, SynCopyDescriptorSimple1 + SynTransitionResource; skips if slot unchanged |
| 56 | 115398 | SynTokBindTextureSrv56 | DrawIndexedInstanced, IASetIndexBuffer, IASetVertexBuffers, OMSetRenderTargets, RSSetScissorRects, RSSetViewports, ResourceBarrier | tok 0x56: texture SRV bind, cache c+0x1118, no stamp |
| 57 | 2F0E2B8 | SynTokBindTextureSrv_Stage1_57 | - | tok 0x57: stub, stage=1, tail-jumps SynBindTextureSrvStage |
| 58 | 2F0E1C8 | SynTokBindTextureSrv_Stage2_58 | - | tok 0x58: stub, stage=2, tail-jumps SynBindTextureSrvStage |
| 59 | 2F0E1F0 | SynTokBindTextureSrv_Stage3_59 | - | tok 0x59: stub, stage=3, tail-jumps SynBindTextureSrvStage |
| 5A | 2F0E1A8 | SynTokBindTextureSrv_Stage5_5A | - | tok 0x5A: stub, stage=5, tail-jumps SynBindTextureSrvStage |
| 5B | 2F0E2F8 | SynTokBindTextureSrv_Stage6_5B | - | tok 0x5B: stub, stage=6, tail-jumps SynBindTextureSrvStage |
| 5C | 8507B0 | SynTokBindTextureSrv5C | DrawIndexedInstanced, IASetIndexBuffer, IASetVertexBuffers, OMSetRenderTargets, RSSetScissorRects, RSSetViewports, ResourceBarrier | tok 0x5C: texture SRV bind, cache c+0x1718, no stamp |
| 5D | 823534 | SynTokBindTextureSrv5D | DrawIndexedInstanced, IASetIndexBuffer, IASetVertexBuffers, OMSetRenderTargets, RSSetScissorRects, RSSetViewports, ResourceBarrier | tok 0x5D: texture SRV bind, cache c+0x1B98, no stamp |
| 5E | 90445C | SynTokBindUav2Stamped_Stage0_5E | - | tok 0x5E: stub, stage=0, tail-jumps SynTokBindUav2Stamped |
| 5F | 113A60 | SynTokBindUavStamped | DrawIndexedInstanced, IASetIndexBuffer, IASetVertexBuffers, OMSetRenderTargets, RSSetScissorRects, RSSetViewports, ResourceBarrier | tok 0x5F: UAV bind, cache c+0x1DB0 (same descriptor: bind and transition skipped; never reset per list), SynTransitionResource(UAV) with stamp from... |
| 60 | 698DC0 | SynTokSetCbData_Stage0_60 | - | tok 0x60: stub, stage=0, tail-jumps SynSetCbDataStage |
| 61 | 698DD0 | SynTokSetCbData_Stage1_61 | - | tok 0x61: stub, stage=1, tail-jumps SynSetCbDataStage |
| 62 | 698DE0 | SynTokSetCbData_Stage2_62 | - | tok 0x62: stub, stage=2, tail-jumps SynSetCbDataStage |
| 63 | 698DF0 | SynTokSetCbData_Stage3_63 | - | tok 0x63: stub, stage=3, tail-jumps SynSetCbDataStage |
| 64 | 2F0E198 | SynTokSetCbData_Stage5_64 | - | tok 0x64: stub, stage=5, tail-jumps SynSetCbDataStage |
| 65 | 2F0E2E8 | SynTokSetCbData_Stage6_65 | - | tok 0x65: stub, stage=6, tail-jumps SynSetCbDataStage |
| 66 | 698EAC | SynTokSetCbData | - | tok 0x66: copies the inline constant blob into the per-slot shadow c+0xD70[slot] (2nd copy) and sets dirty bit c+0xB80 |
| 67 | 698E00 | SynTokSetCbData_Stage7_67 | - | tok 0x67: stage 7 inline variant of SynSetCbDataStage (hdr hi16 = slot, u32 size, inline data copied into shadow at ctx+0xEC0 + slot*8, dirty bit i... |
| 68 | 2F0E5AC | SynTokCopyBufferRegion68 | CopyBufferRegion, ResourceBarrier, SOSetTargets | tok 0x68: hdr b2 (7 bits), u32, 3 qwords; calls SynCopyBufferRegionTracked (CopyBufferRegion + SOSetTargets) |
| 69 | 6BE12C | SynTokDraw | DrawInstanced, OMSetRenderTargets, RSSetScissorRects, RSSetViewports, ResourceBarrier | tok 0x69: state flush then DrawInstanced |
| 6A | 1167D4 | SynTokDrawIndexed | DrawIndexedInstanced, OMSetRenderTargets, RSSetScissorRects, RSSetViewports, ResourceBarrier | tok 0x6A: SynFlushGfxStateForDraw then DrawIndexedInstanced(count, 1, start, 0, 0); ++c+0x548 |
| 6B | 87A4F4 | SynTokDrawIndexedBoundIB6B | - | tok 0x6B: hdr hi16 + one u32 (instance count); calls SynDrawIndexedFromBoundIB (DrawIndexedInstanced with the bound index buffer's count) |
| 6C | 2F0DC54 | SynTokDrawIndexedInstanced6C | DrawIndexedInstanced, OMSetRenderTargets, RSSetScissorRects, RSSetViewports, ResourceBarrier | tok 0x6C: hdr hi16 + 5 dwords; calls SynDrawIndexedInstanced_2EFD898 (DrawIndexedInstanced, vtbl+0x68) |
| 6D | 9F3484 | SynTokDrawInstanced6D | DrawInstanced, OMSetRenderTargets, RSSetScissorRects, RSSetViewports, ResourceBarrier | tok 0x6D: hdr hi16 + 4 dwords; calls SynDrawInstanced_9F34E0 (DrawInstanced, vtbl+0x60) |
| 6E | 6ECF48 | SynTokExecuteIndirect6E | ExecuteIndirect, OMSetRenderTargets, RSSetScissorRects, RSSetViewports, ResourceBarrier | tok 0x6E: SynFlushGfxStateForDraw, arg buffer -> obj+30 (stamped), ExecuteIndirect |
| 6F | 6EB908 | SynTokExecuteIndirect | ExecuteIndirect, OMSetRenderTargets, RSSetScissorRects, RSSetViewports, ResourceBarrier | tok 0x6F: state flush, transition the argument buffer (115E24), ExecuteIndirect |
| 70 | 45B670 | SynTokBeginBlitCopyPass70 | - | tok 0x70: adv4, tail-jumps SynBeginBlitCopyPass |
| 71 | 45B67C | SynTokFlushTrackedCopyPrelude71 | - | tok 0x71: adv4, tail-jumps SynFlushTrackedCopyPrelude |
| 72 | 8741EC | FlushPendingDrawsIfAny | - | if immediate-draw count (+0x2458) nonzero tail-jumps to flush routine; function start 0x8741ec... |
| 73 | 115F0C | SynTokImmediateQuad73 | DrawIndexedInstanced, IASetIndexBuffer, IASetVertexBuffers, OMSetRenderTargets, RSSetScissorRects, RSSetViewports, ResourceBarrier | tok 0x73: reads 10 dwords (rect corners / UVs / colour) and queues an immediate quad via SynEnsureImmediateDrawSpace |
| 74 | 2F0DA40 | SynTokImmediateQuad74 | DrawIndexedInstanced, IASetIndexBuffer, IASetVertexBuffers | tok 0x74: adv4, two 20-B vertex blocks (Copy20ByteFromPtrPtr) + 2 dwords, queues an immediate quad (SynImmediateQuadThunk -> SynEnsureImmediateDraw... |
| 75 | 7A6AB0 | SynTokImmediateQuad75 | DrawIndexedInstanced, IASetIndexBuffer, IASetVertexBuffers, OMSetRenderTargets, RSSetScissorRects, RSSetViewports, ResourceBarrier | tok 0x75: adv4, two 20-B vertex blocks + 5 dwords, queues an immediate quad via SynEnsureImmediateDrawSpace |
| 76 | 2F0DBB8 | SynTokImmediateTriangle76 | DrawIndexedInstanced, IASetIndexBuffer, IASetVertexBuffers | tok 0x76: adv4, two 24-B vertex blocks + 2 dwords; SynImmediateTriangleUniform -> SynImmediateTriangleAppend (3 verts into immediate VB/IB) |
| 77 | 2F0DB08 | SynTokImmediateTriangleVc77 | DrawIndexedInstanced, IASetIndexBuffer, IASetVertexBuffers, OMSetRenderTargets, RSSetScissorRects, RSSetViewports, ResourceBarrier | tok 0x77: adv4, two 24-B blocks + 12-B per-vertex data + dword; SynImmediateTriangleAppend |
| 78 | 2F0D9EC | SynTokImmediateLine78 | DrawIndexedInstanced, IASetIndexBuffer, IASetVertexBuffers, OMSetRenderTargets, RSSetScissorRects, RSSetViewports, ResourceBarrier | tok 0x78: 7 dwords (x0,y0,x1,y1 floats + 2 args); SynImmediateLineAppend (2 verts at +0.5 offset) |
| 79 | 2F0DAD0 | SynTokDrawVbIb79Entry | - | tok 0x79: adv4, 4 dwords + qword; tail-jumps SynTokDrawVbIb79 (2EFD430) |
| 7A | 45C7E8 | SynTokCopyTrackedResource | CopyTextureRegion, DrawIndexedInstanced, IASetIndexBuffer, IASetVertexBuffers, OMSetRenderTargets, RSSetScissorRects, RSSetViewports, ResourceBarrier | reads two handles from token stream and calls SynGenericCopyTracked, cookie-protected |
| 7B | 45DD48 | SynTokDepthCopy7B | CopyTextureRegion, DrawIndexedInstanced, IASetIndexBuffer, IASetVertexBuffers, OMSetRenderTargets, RSSetScissorRects, RSSetViewports, ResourceBarrier | tok 0x7B: adv4, two 16-B boxes + qword; calls SynDepthCopyTracked (CopyTextureRegion with depth transitions) |
| 7C | 8BB170 | SynTokComputeBegin | - | KTGL tok 0x7C: compute section begins on the graphics list (c+0x541 = 1, dirty 0x40) |
| 7D | 8988A8 | SynTokComputeEnd | ResourceBarrier | KTGL tok 0x7D: UAV barrier (1139D4), dirty 0x20, c+0x541 = 0 |
| 7E | 58BDC4 | SynTokDispatch | Dispatch, SetComputeRootDescriptorTable, SetPipelineState | tok 0x7E: SynFlushComputeState then Dispatch |
| 7F | 2F0D970 | SynTokDispatchMesh7F | +0, OMSetRenderTargets, RSSetScissorRects, RSSetViewports, ResourceBarrier | tok 0x7F: adv4, 4 dwords (x,y,z + flags); calls SynDispatchMesh |
| 80 | 2F0D9B4 | SynTokExecuteIndirect80Entry | - | tok 0x80: adv4, 2 dwords + qword; tail-jumps SynTokExecuteIndirect80 (2EFD138) |
| 81 | 58BC74 | SynTokExecuteIndirectDispatch | ExecuteIndirect, ResourceBarrier, SetComputeRootDescriptorTable, SetPipelineState | tok 0x81: SynFlushComputeState, arg buffer -> obj+30 (stamp via 114C04), ExecuteIndirect |
| 82 | 9AFDF0 | SynTokPagePoolBegin82 | - | tok 0x82: adv4; sets ctx+0x543=1 and ctx+0x544=0 (opens a tile page-pool update batch) |
| 83 | 9A62FC | SynTokPagePoolEnd83 | - | tok 0x83: adv4; if ctx+0x544 then SynPagePoolEndBatch (PagePool+61A0 flush); clears ctx+0x543 |
| 84 | 2F0DCBC | SynTokSetStateFlag482_84 | - | tok 0x84: boolean from hdr hi16 -> SynSetStateFlag482 (ctx+0x482, ctx+0x784 bit 20) |
| 85 | 2F0DCD4 | SynTokSetStateFlag483_85 | - | tok 0x85: boolean from hdr hi16 -> SynSetStateFlag483 (ctx+0x483, ctx+0x784 bit 31) |
| 86 | 7F6118 | SynTokReadStateBlock128DispatchB | DrawIndexedInstanced, IASetIndexBuffer, IASetVertexBuffers, OMSetRenderTargets, RSSetScissorRects, RSSetViewports, ResourceBarrier | twin of 115a70: decodes 128-byte state block, calls SynBindTextureSlot 115B9C with mode 1 |
| 87 | 698A44 | SynTokBindTexture87 | DrawIndexedInstanced, IASetIndexBuffer, IASetVertexBuffers, OMSetRenderTargets, RSSetScissorRects, RSSetViewports, ResourceBarrier | tok 0x87: texture bind, X=(cur&0xAC3)/stage bit (R1 superset source), no stamp |
| 88 | 698B90 | SynTokBindSrvDescriptor | DrawIndexedInstanced, IASetIndexBuffer, IASetVertexBuffers, OMSetRenderTargets, RSSetScissorRects, RSSetViewports, ResourceBarrier | reads packed token (slot/stage), flushes immediate draws and copies descriptors (SynCopyDescriptorSimple1) |
| 89 | 2F0DCA4 | SynTokGpuDrivenBatch89Entry | - | tok 0x89: hdr hi16 -> tail-jumps SynTokGpuDrivenBatch89 (2EFD8DC) |
| 8A | 113A0C | SynTokUavBarrier | ResourceBarrier | tok 0x8A: ResourceBarrier(1, UAV, resource from token; NULL = global); emitted by GpuParticleSimRecord 694B38 once after each non-empty GPU-particl... |
| 8B | 7A5D2C | SynTokBufferCopyTransitions | ResourceBarrier | KtglTokenHandlerTable[0x8B]: decodes 3 u32 + 2 u64 from the token stream and calls SynBufferCopyTransitions |
| 8C | 2F0D858 | SynCopyBufferRegion | CopyBufferRegion | SynRecord subsystem; D3D12: CL.CopyBufferRegion (sites 2F0D8D2) |
| 8D | 2F0D8EC | SynTokCopyResource8D | - | tok 0x8D: adv4, two qwords (dst, src); tail-jumps SynCopyResource -> CopyResource (cmd list vtbl+0x88) |
| 8E | A96D90 | SynTokSetMarkerIface | - | token 0x8E: [4BD09C0] = qword from the stream |
| 8F | 6B8884 | SynTokCallbackWithCmdList8F | - | tok 0x8F: adv4, qword fn + qword arg; calls fn({ID3D12GraphicsCommandList* (ctx+0x580), ctx}, arg) on the replay thread |
| 90 | 3D4FC4 | CmdSubStreamRun | - | token handler: reads {object, begin, end} from the stream and runs the u16 opcodes of [begin,end) through the handler table 3C02800 on object+68h (... |
| 91 | 9E2A74 | SynTokCopyTextureRegion91Entry | CopyTextureRegion, ResourceBarrier | tok 0x91: adv4, qword + 32-B struct + qword + 20 B; calls SynCopyTextureRegion91 (CopyTextureRegion with src/dst transitions) |
| 92 | 7F9A6C | SynTokCopyTextureRegion92 | ResourceBarrier | tok 0x92: adv4, 2 qwords + ~14 dwords (subresources, xyz, box); calls SynCopyTextureRegionTracked (ResourceBarrier x2, CopyTextureRegion, restore) |
| 93 | 2F0DFEC | SynTokResolveSubresourceRegion93Entry | +0, ResourceBarrier | tok 0x93: hdr hi16 flag (optional 16-B rect), 2 qwords + ~10 dwords; calls SynResolveSubresourceRegionTracked (ResolveSubresourceRegion) |
| 94 | 7B1750 | SynTokClearUavUint | ClearUAVUint, ResourceBarrier | reads clear value/handles from token stream and calls SynClearUavUintTracked with __chkstk locals |
| 95 | 7F64B4 | SynTokPagePoolUpdate95 | - | tok 0x95: adv4; calls SynSharedLockedUpdate_7F64DC(PagePool=device+61A0, ctx, false) |
| 96 | 2F0D750 | SynTokPagePoolTouch96 | - | tok 0x96: adv4, 2 dwords; SynPagePoolTouch (lazy-opens cmd list ring, then PagePool cache copy + dirty-bit mark) |
| 97 | 2F0D774 | SynTokCacheTileUnmap97 | DrawIndexedInstanced, IASetIndexBuffer, IASetVertexBuffers, OMSetRenderTargets, RSSetScissorRects, RSSetViewports, ResourceBarrier | tok 0x97: adv4, 3 dwords + 16 B; calls CacheUnmapOnCtx (tile cache unmap batch) |
| 98 | 891F24 | SynTokPagePoolPushRecord98 | - | tok 0x98: hdr b2/b3 flags, 3 dwords; SynPagePoolPushRecord appends {u64,u32} to PagePool record list under SRW lock |
| 99 | 981220 | SynTokFlushPendingTransitions99 | - | tok 0x99: adv4; tail-jumps SynFlushPendingTransitions (batched resource state init via obj+0x6870 / ctx+0x6D0) |
| 9A | 2F0D684 | SynTokCacheTileMap9A | - | tok 0x9A: adv4, qword, 16-B coord, 2 dwords, 16-B coord, 20-B region; SynCacheTileMapOnCtx -> tile cache batch map |
| 9B | 7F52AC | SynTokTransitionResource | ResourceBarrier | tok 0x9B: explicit state-tracked transition from the stream (resource, state slot, new state, stamp, subresource); skips equal states |
| 9C | 7C67E0 | SynResourceBarrier_7C67E0 | - | SynRecord subsystem; D3D12: CL.ResourceBarrier (sites 7C68CC) |
| 9D | 84EFF4 | SynTokResourceBarrierBlob9D | - | tok 0x9D: adv4, count + count*16 B barrier blob; replays it with SynResourceBarrier_85033C on ctx+0x580 |
| 9E | 8F2C90 | SynTokWriteDwordToPtr9E | - | tok 0x9E: adv4, qword ptr + dword; stores the dword to CPU address (replay-side readback/fence flag) |
| 9F | 8962D8 | SynTokDiscardResource9F | - | tok 0x9F: adv4, qword resource; DiscardResource(resource, NULL) (cmd list vtbl+0x198) |
| A0 | A96D70 | SynTokSetMarkerReplay | - | token 0xA0: c+542h = byte from the stream (the engine's own replay switch) |
| A1 | 2F0E318 | SynTokSetMarkerAnsiA1 | - | tok 0xA1: hdr hi16 = byte len, u32 colour, ANSI string; strncpy_s to 1 KB buffer, SynSetMarkerAnsi (SetMarker vtbl+0x1C0) |
| A2 | 2F0E39C | SynTokSetMarkerWideA2 | - | tok 0xA2: hdr hi16 = wchar count, u32 colour, wide string; wcsncpy_s, SynSetMarkerWide (SetMarker vtbl+0x1C0) |
| A3 | 2F0E4A4 | SynTokSetMarkerAnsiA3 | - | tok 0xA3: same layout as A1 (ANSI string), second copy -> SynSetMarkerAnsiB (SetMarker vtbl+0x1C0) |
| A4 | 2F0E528 | SynTokSetMarkerWideA4 | - | tok 0xA4: same layout as A2 (wide string), second copy -> SynSetMarkerWideB (SetMarker vtbl+0x1C0) |
| A5 | 114024 | SynTokBeginEvent | BeginEvent, DrawIndexedInstanced, IASetIndexBuffer, IASetVertexBuffers, OMSetRenderTargets, RSSetScissorRects, RSSetViewports, ResourceBarrier | token 0xA5: unpack colour + name, call SynBeginEvent 113E64 |
| A6 | 113FC8 | SynTokEndEvent | DrawIndexedInstanced, EndEvent, IASetIndexBuffer, IASetVertexBuffers, OMSetRenderTargets, RSSetScissorRects, RSSetViewports, ResourceBarrier | token 0xA6: EndEvent (vtbl+1D0h) when byte c+542h |
| A7 | 8CEBC0 | SynTokWriteMarkerBBBB | DrawIndexedInstanced, IASetIndexBuffer, IASetVertexBuffers, OMSetRenderTargets, RSSetScissorRects, RSSetViewports, ResourceBarrier | token handler: flushes immediate draws and overwrites the next stream dword with 0xBBBBBBBB (decompile-read) |
| A8 | 495FC4 | SynTokCloseCmdListAndHash | DrawIndexedInstanced, IASetIndexBuffer, IASetVertexBuffers, OMSetRenderTargets, RSSetScissorRects, RSSetViewports, ResourceBarrier | reads handle from tokens, SynCloseCmdList then hash-bucket stores the list |
| A9 | 905480 | SynTokCloseListAndPushEclA9 | - | tok 0xA9: adv4, qword; tail-jumps SynCloseListAndPushEclTok2 (close cmd list, push ECL token 2 with the u64) |
| AA | 2F0E650 | SynTokCloseListIfFenceBehindAA | - | tok 0xAA: adv4, pointer to u64; if *ptr != 0 calls SynSubmitIfBehindFence (closes list/executes if ctx+0x2510 < value) |
| AB | 8C16AC | SynTokReadThreeQwordsDispatch | - | reads 3 qwords from command stream and calls 0x8a00b8 with two state sub-objects at +0x5a0/+0x588 |
| AC | 2F0D634 | SynTokBeginQueryAC | - | tok 0xAC: adv4, 2 dwords (type,index) + qword heap; BeginQuery (cmd list vtbl+0x1A0) |
| AD | 2F0DD80 | SynTokEndQueryAD | - | tok 0xAD: adv4, 2 dwords (type,index) + qword heap; EndQuery (cmd list vtbl+0x1A8) |
| AE | 2F0DF30 | SynResolveQueryData | - | SynRecord subsystem; D3D12: CL.ResolveQueryData (sites 2F0DFCD) |
| AF | 7543BC | SynTokEndQueryTimestampAF | - | tok 0xAF: adv4, dword index + qword heap; EndQuery(heap, type=2 TIMESTAMP, index) (vtbl+0x1A8) |
| B0 | 2F0D678 | SynTokBeginPipelineStatsB0 | - | tok 0xB0: adv4; tail-jumps SynBeginPipelineStatsQuery (BeginQuery type 3 on ctx+0x24D8 ring) |
| B1 | 2F0DDC4 | SynTokEndPipelineStatsB1 | - | tok 0xB1: adv4; tail-jumps SynEndPipelineStatsQuery (EndQuery type 3) |
| B2 | 2F0D5FC | SynTokQueryScopeBeginDefaultB2 | - | tok 0xB2: adv4; tail-jumps SynQueryScopeBeginDefault (query object at ctx+0x2500) |
| B3 | 2F0DD54 | SynTokQueryScopeEndDefaultB3 | - | tok 0xB3: adv4; tail-jumps SynQueryScopeEndDefault (query object at ctx+0x2500) |
| B4 | 2F0D5D8 | SynTokQueryScopeBeginObjB4 | - | tok 0xB4: adv4, qword query object; SynQueryScopeSetCurrentAndBegin (AddRef, store ctx+0x2508, reset+begin) |
| B5 | 2F0DD30 | SynTokQueryScopeEndObjB5 | - | tok 0xB5: adv4, qword query object; SynQueryScopeEndAndClearCurrent |
| B6 | 2F0D608 | SynTokQueryObjBeginSlotB6 | - | tok 0xB6: adv4, qword query object + u32 index; SynQueryObjBeginSlot (BeginQuery on obj via vtbl+0x1A0 of ctx+0x590 list) |
| B7 | 2F0DD60 | SynTokQueryObjEndSlotB7 | - | tok 0xB7: adv4, qword query object; tail-jumps SynQueryObjEndSlot (EndQuery vtbl+0x1A8) |
| B8 | 2F0E45C | SynTokSetPredicationB8 | - | tok 0xB8: adv4, qword buffer + dword offset index; tail-jumps SynTokSetPredication (2F00468) |

## 9. Files

- `mod/Nioh3EngineFix/src/bkstats.{h,cpp}`, `tests/bkstats_harness.cpp`, `tests/run_bkstats_harness.py` (branch renderer-backend).
- Scripts used (scratch, not committed): capstone disassembly / IAT scans for the SRW holders (srwscan), token emitters (tokemit),
  token-handler classification (tokclass: payload advance, CL methods from `<re-scratch>\d3d12\vcalls.tsv`, callees, tail targets).
- Existing: `re\scripts_d3d12\toktab.py` (table dump), `vcalls.fsx`, `d3d12_sites.tsv`.

## Capture-check (package 12, 2026-10-10; table in 12-capture-check.md section 2)

Re-captured with list calls (pc2, Ally and PC runs) on three GPUs:
- **Confirmed:** two DIRECT bursts (pc2 89 % / 99 %, Ally 88 % / 99 %, PC 82-84 % / 99 %), 18-27 DIRECT ECL per frame each followed by its Signal, one list per ECL, COMPUTE through 4F925F / 4F908B, the lists are cut at the marker boundaries listed in 12 section 2 (B4), the G-buffer phase 1 is a single 537-draw list. Burst 1 holds every list up to the `App_CopyMRLRScene` list **plus all COMPUTE lists**; burst 2 starts at `EffectMergeShrinkBuffer` (FSR3, post, UI, VideoOut).
- **GfxWait 8F4548 / return 8F4583 wait for streaming uploads, not async joins.** Their fences are signalled by the COPY queues (Signal rows at 892E19 and 5C315): DIRECT waits for streaming uploads. The async-compute joins are the 4F9323 waits (fences signalled at 4F908B). 3 of each per frame.
- **Refined:** the 2 foreign ECL per frame (ra ffffffff) sit at the start of the next frame's window (0.4 ms), i.e. they are the previous present's FFX / present-path lists, not mid-frame work. UpdateTileMappings: 0 on DIRECT everywhere; the COPY queue 290B91 waits on the DIRECT Signal 4F9127.
- On the PC the `ecl` rows carry ra ffffffff (the OSD hook sits under another layer on that machine); the Signal / Wait rows keep the engine RVAs. Do not read ECL callers from PC captures.
- Caller RVA table per D3D12 method: 12-capture-check.md section 7 (for 01-backend to map to tokens).
