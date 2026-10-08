# PC submit-path D3D12 call overhead: volume, redundancy, ASI state filters, and why Barriers regresses on PC

Static, 2026-10-08. Inputs: PC RGP r5 frame (`re\scripts_gpu\out\rgp_top300\...events.csv` at HEAD), PC 1.7.16 gpuopt
counters (`captures\pc\loadstats\*.log`), PC live A/B PresentMon CSVs (`captures\tm\pc_*.csv`), the r5 CPU profile
(`captures\cpuprof-r5.txt`, dispatcher thread), decomp (118604, 1180D0, 118C10, 1188AC, 58BE90, 116260, 1164C4, 45A654) and
dump scans. The game was not run. Nothing in `mod\...\src` changed. New: `re\scripts_d3d12\submit_redundancy.py`
(`--selftest` OK).

## TL;DR

**Two premises of the brief are wrong:**
- **B is not barrier batching.** B is the main-thread *draw-list galloping search* (`notes\mainthread-drawprep.md` §B, cave at 3BC17).
  Barrier batching is `[GpuOpt] Barriers`, and that is the item that measured **-7.1 %**. So B's +15.6 % says nothing about
  D3D12 call overhead.
- **The PC does not draw ~45k times a frame.** The r5 frame has 1354 draws, 1081 dispatches and about 140 ExecuteIndirect
  calls (416 api30/31 events). Ally GP2 shows 2.2-2.9k draws + dispatches. All D3D12 calls together are an estimated
  **12-25k per frame**.

**B is suspicious.** A pure CPU patch with an exact result cannot lower GPU work per frame. With B on, PresentMon
`MsGPUBusy` drops **5.01 -> 4.22 ms (-16 %)** in the stand windows and 5.05 -> 4.26 in the move window. That
means less GPU work per frame: either B skips draws (the "type-3 list is sorted" assumption is false live, so this is
not the same look), or that window caught a scene transient. The next two windows (PP 4.59, GF 4.92) recover
step by step. **Do not KEEP B until a B on/off screenshot compare and a draws/frame count agree (section 7).**

**Why Barriers regresses on PC.** R2a/R1 never run on Windows: they are gated on `!g_wine.empty()` at gpuopt.cpp:603,
and the live switch at :971 has "no effect off Wine". So the -7.1 % row is batching alone. The same-launch numbers,
Barriers 1 vs 2 (both hooked):

| | frame ms | CPUBusy | GPUBusy | GPULatency |
|---|---|---|---|---|
| Barriers = 2 (BASE-live) | 5.52 | 5.39 | 5.01 | 1.32 |
| Barriers = 1 | 5.89 | 5.76 | 4.98 | 2.82 |

- The GPU gain is about zero (-0.03 ms; the r5 estimate was 0.19-0.26 ms, made in a GPU-bound 1440p scene).
- The CPU frame is +0.37 ms longer.
- The GPU starts each frame's work +1.5 ms later.

The cost is CPU and submission-side, on the dispatcher. At about 180 fps the dispatcher sits on the critical path, so
its time turns into frame time (section 3). Fix now: **`[GpuOpt] Barriers = 0` on PC.** The full-mod ini
`tools\testmatrix\fullmod_ini_1716.ini` ships `Barriers = 1`. Also keep R2a/R2b/R1 at 0 on PC: any of them installs
the hooks without doing anything there.

**Ranked candidates** (all exact or config, default off, live switch, in the style of B):

| # | id | what | how | exact | est. PC gain | Ally |
|---|---|---|---|---|---|---|
| 1 | **PC-B0** | Barriers = 0 on PC | ini | yes (no hooks) | recovers the measured **-7 %** vs Barriers = 1 (in-launch) | separate decision (~0 there) |
| 2 | **SF1 + PM** | Skip `SetPipelineState(p)` when p is already bound (ASI asm thunk on slot 25). Plus an engine cave that memoizes the PSO key in front of the 119670 / 58BE90 cache lookups: an 8-entry MRU of 128-B keys skips the B-tree + memcmp + CritSec; when the key equals the bound key, the call is skipped too | ASI thunk + cave | yes | dispatcher -0.1-0.3 ms -> **PC +0-3 %** when the dispatcher is co-critical (>= ~170 fps) | ~0 fps (dispatcher 81 % parked) |
| 3 | **SF3** | Skip identical `OMSetRenderTargets` / `RSSetViewports` / `RSSetScissorRects` re-sets. 45A654 marks all three dirty (`b24 |= 0xD`) on every RT-set token without comparing, including each copy/clear restore | ASI (C++ slow path is fine, low volume) | yes, with RTV/DSV descriptor-generation + barrier invalidation | 0.02-0.1 ms -> +0-1 % | also fewer vkd3d render-pass restarts (OMSet always ends the pass) |
| 4 | SF2 | Skip same-value root signature / root descriptor table / root CBV-SRV-UAV re-sets | ASI asm thunks | yes (ResourceBinding.md 658-662) | small. SRV/CBV tables get a fresh ring range per dirty stage, so their handles never repeat; only the content-cached sampler tables and compute tables can repeat | ~0 |
| 5 | SF4 | Topology | - | - | ~0: the engine already filters it (`+0x854`) | - |
| 6 | E-SRV | Content-addressed SRV/CBV stage tables, the way samplers already are (1188AC), to cut `CopyDescriptorsSimple(N)` (amdxc64+196B710 = 3.2 % self of the dispatcher) | engine cave | yes if ring lifetime is respected | unknown; dirty implies the content changed, so it only pays for A/B/A reuse | high risk; only if a trace shows reuse |

Items 2-4 need one PC trace run before any build (section 7). It counts every redundancy directly; no new build is needed.

## 1. Per-frame call volume on the submit path (PC)

All per-draw D3D12 calls are made by the **SynergisticDispatcherTask** thread. It replays the KTGL token stream (185-entry
table 3C02220) into one open list. The main thread records tokens and only calls D3D12 at frame end (`notes\d3d12-submission.md` §1).

| call | /frame | source | engine filter before the call |
|---|---|---|---|
| ResourceBarrier | **1752 calls / 1773 structs** (in-game 30 s window); 1220 API barriers in r5 | gpuopt 1.7.16 counter; RGP | same-state skip (115E24, 1180D0) |
| DrawIndexedInstanced | 1175 | RGP | - (always 1 instance, 1167D4) |
| DrawInstanced | 179 | RGP | - |
| Dispatch | 1081 (1068 GFX + 13 async) | RGP | - |
| ExecuteIndirect | ~140 calls (api30 138 / api31 278 events) | RGP | - |
| Clear RTV / UAV / DSV, CopyTextureRegion | 38 / 32 / 5 / 8 | RGP | - |
| SetPipelineState | **>= 1327** (GFX-queue PSO changes = lower bound); est. 1.4-2.2k | RGP + static | set when dirty bit 0x20 (39 setter sites) / 0x40 (7). The PSO pointer is **not** compared |
| Set*RootDescriptorTable | est. 2-6k (per dirty CBV / SRV / sampler stage table) | static (118DA8, 118C10, 1188AC) | per-stage dirty masks `+0x10F0`, `+0x1E38`, `+0xB70` |
| device CopyDescriptorsSimple | est. 3-8k (1 per SRV bind change + 1 per dirty table) | static (115DE8, 118C10) | same-SRV+sampler skip at bind |
| device CreateConstantBufferView | est. 1-3k (per dirty CBV) | 118E28 | dirty CBV only |
| IASetVertexBuffers / IASetIndexBuffer | est. 0.3-1.5k / 0.3-1k (1 view per call, per changed slot) | 116260, 114614 | per-slot compare (buffer, offset, stride) |
| IASetPrimitiveTopology | est. 0.1-0.5k | 118604 | compared to `+0x854` |
| OMSetRenderTargets + RSSetViewports + RSSetScissorRects | est. 100-300 each | 1180D0 bits 1/4/8 | **none**: 45A654 sets `0xD` on every RT-set token |
| SetGraphicsRootSignature, SetDescriptorHeaps | ~28-36 (per list begin) + compute | 49349C | once per list |
| ECL / Close / Reset / ClearState | ~28 GFX + 5-8 compute | RGP queue events | - |

Dispatcher budget (r5, ~113 fps = 8.85 ms per frame): about 4.9 ms busy per frame.
- **D3D12 runtime + driver: about 2.9 ms per frame** (D3D12Core 6.1 % + amdxc64 26.5 % of its samples).
- Nioh3 code: about 1.9 ms per frame.
- ResourceBarrier inclusive (D3D12Core+23B280) = 7.8 % of samples, about 0.69 ms, which is roughly 0.4 µs per call.
- PSO path:
  - 118604 + 58BE90 attributed: 6.9 %, mostly D3D12Core+23BF40 SetPipelineState.
  - PSO cache lookup (119820 B-tree): 2.2 %.
  - memcmp: 0.6 %.
  - RtlEnterCriticalSection (58BF5B): 0.4 %.
  - Together that is about 10 % of samples, ~0.9 ms per frame.
- CopyDescriptorsSimple: about 3.4 %.

## 2. What the engine already filters, and what is left

Already filtered: resource states, same SRV+sampler binds, VB/IB per slot, topology, sampler tables (content-hashed via
FUN_8a40d8 and reused), and dirty-gated PSO / tables / RT / viewport / scissor. Root signature and descriptor heaps are set once per list.

Left over, and filterable at the API without changing what the GPU does:
- **PSO:** `0x20`/`0x40` are set by *restore* paths without a compare. Examples: 2EFE9FC (compute shader restore after an internal op) and
  2EFD8DC (tok 89 GPU-driven batch), plus 1164C4 setting `0x30` on any VS change. The lookup and `SetPipelineState` then run even when
  the key equals the bound PSO. RGP: 754 of 1175 indexed draws and 427 of 1068 dispatches use the same PSO as the previous event,
  so that is the ceiling for redundant sets. The real number is unknown (trace).
- **RT/VP/scissor:** 45A654 (6 callers, including the CopyTextureRegion / generic-copy restores 45A020 / 45A190) re-dirties all three.
- **Sampler-table root sets:** a content-cache hit can hand back the handle that is already bound, and `SetGraphicsRootDescriptorTable` is still called.
- **SRV/CBV tables:** a new ring range on every dirty, so the API sees different handles. Only an engine-side content cache helps (E-SRV).

## 3. Why Barriers (batching) costs ~7 % on PC

1. **No GPU upside at this spot.** GPUBusy 4.98 vs 5.01 ms. The PC A/B spot is CPU-bound: CPUWait is 0.13 ms in every window.
2. **The dispatcher is co-critical at ~180 fps.** It has about 4.9 ms of work per frame (r5), against a 5.5 ms frame.
   - The main thread busy-waits for the dispatcher to drain at frame end (418F40).
   - The GPU gets its first list only after the dispatcher closes it.
   - So extra dispatcher time shows up as frame time and as later GPU starts. That matches **GPULatency +1.5 ms** with Barriers = 1.
   - At r5's 113 fps it was 44 % idle and this did not matter. That is why the batching design (written for the GPU-bound
     1440p case) predicted a gain.
3. **Where the extra dispatcher time comes from (ranked, unproven):**
   - (a) **The driver/runtime path for n > 1.** D3D12Core+23B280 -> amdxc64 cost looks per-struct, not per-call. Merging 1752
     calls into ~690 saves only the fixed part (<= ~0.2 ms) and may take a slower batched path.
   - (b) **gpuopt's own per-barrier work.** SRW lock, 4-6 `InterlockedAdd64` on shared counter lines, the batch scan, and
     ~500-700 slow-path flushes per frame: an estimated 0.1-0.2 ms.
   - (c) **Deferral moves barrier work past binds** to the draw, which can hurt the driver's state caching (MergeAcrossBinds = 1).

   None alone clearly gives +0.37 ms. Decisive test (section 7): SP on the dispatcher with Barriers 2 vs 1, then MaxBatch 4 and
   MergeAcrossBinds 0.
4. **Lesson for SF1-SF3:** no locks and no atomics on per-call paths. The filter must cost fewer cycles than the
   D3D12Core + amdxc64 call it removes (~0.2-0.5 µs).

## 4. SF filters: design (for the ASI owner; default off)

`[GpuOpt] StateFilter = 0|1|2` (2 = count "would skip" only), with sub-switches `SF_PSO`, `SF_OM`, `SF_Root`. All are
live-switchable. They reuse gpuopt's vtable patch (D3D12Core vtable sets 0/1); filtered slots get dedicated asm thunks.

- **Shadow:** a direct-mapped table of 64 x 64-B entries keyed by list pointer `{tag=list, pso, rsG, rsC, om_valid, ...}`.
  A tag miss means: replace the entry, pass the call through, and record the value. A collision is therefore only a missed skip,
  never a wrong one.
  - Root tables use a small per-entry array for indices 0-15 (graphics and compute).
  - Counters are plain per-entry increments, summed by the 30 s stats line.
- **Fast path (SF1 example):**
  ```
  cmp byte [sf_pso],0 / je orig
  mov rax,[rsp] / cmp rax,exeLo / jb orig / cmp rax,exeHi / jae store_only   ; only Nioh3.exe calls are skipped
  entry = sf_tab + ((rcx>>4)&63)*64 / cmp [entry],rcx / jne miss
  cmp [entry+8],rdx / jne store / inc qword [entry+56] / ret                  ; skipped (return type void)
  ```
  Foreign callers (FFX upscaler, sl.interposer, OptiScaler) are never skipped, but they **update** the shadow, so it stays
  truthful.
- **Invalidate the entry on:**
  - Reset, ClearState, Close.
  - ExecuteBundle: the bundle's PSO and root arguments come back out.
  - SetPipelineState1 / SetProgram (PSO).
  - QueryInterface. Upscalers unwrap through QI before recording; this is the same hand-over point gpuopt already relies on,
    and it covers any module that records through a captured original pointer.
  - Root tables only: a root signature *change* or SetDescriptorHeaps.
  - ExecuteIndirect: only if the signature is not one of the engine's two plain ones (`SynCtx+0x6E8/+0x6F0`, NULL root
    signature). Otherwise clear the root caches.
- **Why the result is identical:**
  - Binding the bound PSO again is a no-op; a PSO set does not touch the root signature (ResourceBinding.md 622).
  - A redundant root signature set keeps the bindings (ResourceBinding.md 658-662).
  - A root argument set to its current value carries no new data.
  - A list is recorded by one thread at a time (D3D12 rule), so there are no races.
- **SF3 (OM/RS), C++ slow path, ~300-900 calls per frame:**
  - Compare NumRT, the RTV handle values, the single-range flag, and the **DSV handle**. The DSV is the 5th argument, on the
    stack: the slow path must receive the caller's rsp and read `[rsp+28h]`.
  - Skip only if all of these hold: no ResourceBarrier on this list since that OMSet (the barrier hook clears `om_valid`;
    this covers drivers that derive depth layouts at bind time), and the global RTV/DSV descriptor generation is unchanged.
    The generation is bumped by a device-vtable hook on CreateRenderTargetView / CreateDepthStencilView and on CopyDescriptors*
    into RTV/DSV heaps.
  - Viewports and scissors are compared by content (num x 24 / 16 B).
- **PM (PSO memo cave), engine side:**
  - Before the call to 119670 (graphics) and inside 58BE90's `0x40` block (before EnterCriticalSection), keep an 8-entry MRU of
    {128-B key, PSO}.
  - A hit skips the lookup. If the hit PSO equals the PSO last set on the current list (`SynCtx+0x580`, cleared at SynBeginList
    49349C), it also skips `SetPipelineState` and just clears the dirty bit.
  - Invalidate the MRU when the PSO cache's insert/remove path runs (generation counter). This needs the cache's eviction code
    located first (open item).
- **Verify:** trace counts move into "skipped"; screenshot compare at a paused spot; 2 min play. The Ally runs the same
  binary (vkd3d), so a separate Ally A/B is needed for SF3's render-pass effect.

## 5. Estimated PC gain, honestly

| | dispatcher ms/frame saved | fps (only while the dispatcher is co-critical) |
|---|---|---|
| PC-B0 | removes the measured +0.37 ms | +7 % vs Barriers = 1 (in-launch) |
| SF1 + PM | lookup 0.28 ms x MRU hit rate (est. 50-80 %) + redundant sets x ~0.3-0.5 µs | +0-3 % |
| SF3 | (100-300 x 3 calls) x share redundant x ~0.2 µs | +0-1 % |
| SF2 | small | ~0 |

Gains turn into fps one-for-one at most, and only where the frame is about as short as the dispatcher's work, which
the PC reaches at ~170+ fps. That threshold gets lower as main-thread patches (A, B if it holds, GF) land. At r5's 113 fps it
would be ~0.

## 6. Trace extension (small, for the ASI owner)

`TraceRec` keeps pointers in a1..a3. For slots 21/22 (viewports/scissors), 43/44 (IB/VB views), 46 (OMSetRenderTargets +
the DSV stack argument) and 35/36 (Root32BitConstants), store an FNV-1a of the pointed-to content in a new `h` column. Then
`submit_redundancy.py` can turn its "weak" rows into exact counts. Read the content only when the trace is on.

## 7. Next PC turn (one launch, ~15 min; no new build for steps 1-3)

1. **Counts:** `[GpuOpt] Barriers = 2, TraceRecords = 2000000, TraceDelaySec = 60` at the A/B spot. Then
   `py -I re\scripts_d3d12\submit_redundancy.py <trace.csv>` (calls per frame by method, exact PSO / RootSig / Table / RootVA /
   Topology redundancy, weak OM/RS/VB hints, dead PSO sets) and `vkd3d_cost.py` on the same CSV. Build SF1 if PSO redundancy
   is >= ~200/frame; build PM if the SP shows the lookup at >= 2 % of the dispatcher. Build SF3 after the `h` column, if
   >= ~100/frame.
2. **B check:** take the same trace (or GPUProf draw counts) with B = 0 and with B = 1. If draws/frame differ, B is not exact:
   drop it or cap the gallop. Also take a screenshot pair at the paused spot.
3. **Barriers cause:** run SP with the dispatcher thread split, Barriers 2 vs 1 live. Compare gpuopt self time, D3D12Core+23B280,
   amdxc64 under it, the dispatcher's busy %, and PresentMon GPULatency. Next launch: MaxBatch 4 and MergeAcrossBinds 0.
4. Ship **Barriers = 0** in the PC ini now.

## 8. Files

- `re\scripts_d3d12\submit_redundancy.py` (new): trace -> per-frame call volume + exact redundancy, `--selftest`.
- This note. No symbol changes. Sites used: 118604 SynFlushPsoAndBindings, 1180D0 SynFlushGfxStateForDraw, 45A654 (RT-set,
  unconditional `|= 0xD`), 2EFE9FC / 2EFD8DC (unconditional compute PSO dirty), 1134E4 (compute shader set, compared).
