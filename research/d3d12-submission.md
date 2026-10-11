# Nioh 3: how KTGL submits to D3D12, and where the submission wastes CPU or GPU time

Static analysis + existing traces only (2026-10-06): the dump in Ghidra / Iced, the r5 CPU profile
(`captures\cpuprof-r5.txt`, plus r6/r7/ingame for cross-checks) and the r5 RGP trace
(`captures\rgp\Nioh3_2026-10-06_19-39-58.rgp`, now also decoded for **barrier flags** and **CPU submit timestamps**).
The game was not touched and nothing was applied. Scripts are in `re\scripts_d3d12\`, large outputs in `<re-scratch>\d3d12\`.
Symbols: `re\symbols\incoming\d3d12-submission.tsv`.

## TL;DR

The submission path is reasonably engineered: state, SRVs, samplers and resource states are all shadow-filtered before
they reach D3D12, and the dispatcher thread is ~45 % idle. The waste that exists is mostly **synchronisation**, not
redundant API calls:

| rank | id | what | evidence | est. saving | delivery | risk |
|---|---|---|---|---|---|---|
| 1 | **P1** | The main thread waits at every frame begin until the **Present thread has returned from Present()** (7D9B6C → 7D9E90). About half of Present's CPU time is the **RTSS (Afterburner) OSD hook**, plus Steam overlay and Streamline | 7D9E90 = 6.1 % of main (r5), 1–3.8 % in other runs. Present thread: SL 8.1 % → RTSSHooks64 7.5 % → Steam 3.75 % → dxgi 3.6 % (r5); r7-B1 5.4 / 5.4 / 2.2 / 1.7 | RTSS OSD off: **~0.2–0.3 ms per frame off the main thread's critical path, ~+2–3 % fps when CPU-bound (estimate, not measured)** | setting on the test PC (no code) | none |
| 2 | **B1** | **Barriers are issued one per ResourceBarrier call** (115E24, 1180D0, 113A0C all pass NumBarriers=1), interleaved with binds. On the GPU, 1228 of 1360 barriers sit in back-to-back chains; each extra barrier pays its own sync | RGP r5: 478 chains, 882 "extra" barriers; chains with <10 µs tail add 467 µs. Long tails (>10 µs, 677 µs) are real drains, not waste | **GPU ~0.15–0.4 ms/frame** (only matters when GPU-bound); dispatcher CPU ~2–3 % of its time | D3D12 command-list proxy (batch until next non-barrier call). Not a byte patch | medium |
| 3 | **P2** | Engine-level version of P1: begin the next frame without waiting for Present to return | 7D9B6C waits only to read GetCurrentBackBufferIndex and run 252510 | 1–6 % of main thread | cave in 7D9B6C/7D9C78, ordering of Present vs the next frame's ExecuteCommandLists changes | high: not built |
| 4 | **K1** | Draws per stream hand-off (`KtglContext::CmdFlushLimit` +0x3C70, copied at init from device-param +0x804) controls how much work the dispatcher still has when the main thread reaches the end-of-frame drain spin (418F40) | 418F40 = 0.7–2.1 % of main (all runs); value not readable statically | ≤ 0.7–2.1 % of main | one live read, then a live data write for A/B | low |
| 5 | **C1** | Constant data is copied **three times** (source → command stream on main → per-slot shadow on dispatcher (698EAC) → upload ring (118E28)) | main: B94230/B94250 from 242EB8/242FC4/1D7340 ≈ 2.5–3.5 % of main; dispatcher ≈ 1.5 % | ≤ 1.5–2.5 % of main | redesign of KTGL constants | high: not built |
| 6 | S1 | The end-of-frame drain spin 418F40 has no PAUSE | pure spin, 92 % busy | ~0 (SMT courtesy only) | **17-byte in-place patch, verified offline** (`drainspin_pause.fsx`) | none |
| – | – | Descriptors (CopyDescriptorsSimple / tables / root tables) | dirty-mask filtered per stage; same-SRV/same-sampler binds skipped | none worth doing | – | – |
| – | – | "Too few frames in flight" | No: the GPU idles because the main thread is the bottleneck, not because of a pipeline-depth limit (§5) | – | – | – |

Estimates (none of these was measured): **P1 (free) ≈ +2–3 % when CPU-bound**, B1 ≈ 2–4 % GPU time when GPU-bound (needs a
proxy DLL), the rest is small or invasive.

---

## 1. Threads and D3D12 call sites

D3D12/DXGI come in through Streamline (`SL.INTERPOSER.DLL`: D3D12CreateDevice at 2EE300C, CreateDXGIFactory*), so every
D3D12 call is a vtable call. `re\scripts_d3d12\vcalls.fsx` walks the token handler table and classifies
`call [reg+disp]` by the `this` load (`[ctx+580h]` = ID3D12GraphicsCommandList).

| thread | entry | what it does with D3D12 | CPU (r5) |
|---|---|---|---|
| **[App]Game** (main) | 1A74130 | records KTGL tokens into `KtglContext+0x3C40` (no per-draw D3D12 calls). Hands stream segments to the dispatcher ring (`KtglCmdStreamFlush` 254110 → ring at device+0x3BE0, ReleaseSemaphore) every `+0x3C70` draws. At frame end (GfxDevicePresent 3D62E4): final kick + **busy-wait until the dispatcher has drained** (418F40), then records the back-buffer → PRESENT barrier and ResolveQueryData (list +0x590) itself, closes/submits (252758), hands Present to the Present thread | 91.5 %; D3D12Core 0.6 %, amdxc64 1.0 % |
| **SynergisticDispatcherTask** | 2EEDFA0 → 1A7C3E0 | replays the token stream through the 185-entry handler table **3C02220** into one open ID3D12GraphicsCommandList (`SynCtx+0x580`, SynCtx = device+0x10). All per-draw D3D12 calls happen here (table below). Closes lists (ClearState(NULL) + Close, 490D5C) at submit points and hands them to the ECL ring (4930C8 → 86F760) | 60.5 %, of which 44 % blocked in its semaphore wait; amdxc64 26.5 %, D3D12Core 6.1 %, Nioh3 21.7 % |
| **ExecuteCommandListsThread** | 2F144B4 → 1A75970 | replays a second, tiny token table **3BD4DF0** (9 entries): tok 6 = `queue->ExecuteCommandLists(1, &list)` (4F9214), tok 7 = `Signal` (4F91C0), tok 8 = `Wait` (4F93C8) | 2.2–4.2 %, mostly kernel; never caught by the sampler |
| **Present thread** (unnamed) | 2EEA344 → 1A82298 | waits on `GfxDevice+0x7720`, calls `swapchain->Present(+0x7738, +0x773C)` (vtbl+0x40), clears `+0x7728` | 6 % CPU; 93.5 % of its samples idle |

`MOTOR_RUN_COMMAND_LIST_SUBMIT` / `MOTOR_RUN_SYNERGISTIC` are option-name strings in the table at 0x3977AE0 (used by
1A6BD18), not thread names; they very likely switch the ECL / dispatcher threads on (`GfxDevice+0x3F80` is the same kind
of switch for the Present thread). Not verified.

### Command-list calls by handler (dispatcher), with the profile's "attributed to first Nioh3 frame" share of that thread

| site | method | handler / token | share |
|---|---|---|---|
| 1167D4 → 116832 | DrawIndexedInstanced | tok 0x6A | 2.8 % |
| 6BE12C / 9F34E0 | DrawInstanced | tok 0x69 / 0x6D | 0.6 % |
| 6EB908, 6ECF48, 58BC74 | ExecuteIndirect | tok 0x6F, 0x6E, 0x81 | 3.2 % incl. |
| 58BDC4 → 58BE18 | Dispatch | tok 0x7E | 1.4 % |
| **1180D0** (called before every draw) | OMSetRenderTargets, RSSetViewports, RSSetScissorRects, **ResourceBarrier ×3 sites (1 barrier each)** | gated by `SynCtx+0xB24 & 0xF` | 2.4 % |
| **118604** (functor 3C332A8, called from 1180D0) | IASetPrimitiveTopology, **SetPipelineState**, plus CBV/SRV/sampler table flush (118DA8/118C10/1188AC → SetGraphicsRootDescriptorTable) | per draw, dirty-gated | 5.1 % (PSO set dominates: D3D12Core+23BF40 is 67 % called from 118737) |
| 58BE90 | SetPipelineState (compute) + SetComputeRootDescriptorTable (4959EC/495414/495764) | tok 0x7E/0x81 | 1.8 % |
| **115E24** | ResourceBarrier (1 transition, skipped when the tracked state is equal) | 45 callers | **4.4 %** |
| 113A0C / 1139D4 | ResourceBarrier (1 UAV barrier) | tok 0x8A / 0x7D | 1.2 % |
| 115DE8 | **device->CopyDescriptorsSimple(1, …)** (staging SRV / sampler) | from 115B9C (tex bind) etc. | 2.2 % |
| 118E28 | CreateConstantBufferView (device +0x88) per dirty CBV, CopyDescriptorsSimple(N) of the whole table, SetGraphicsRootDescriptorTable | per draw when CBVs dirty | 1.2 % |
| 7A728C | IASetVertexBuffers + IASetIndexBuffer + DrawIndexedInstanced for immediate-mode geometry | called by 88 handlers when `+0x2458 != 0` | – |
| 45C880/45A020/… | CopyTextureRegion, CopyBufferRegion, ClearRenderTargetView (4591DC 1.4 %), ClearUAVUint | various | – |
| 119820 | not D3D12: PSO cache B-tree lookup with 128-byte memcmp keys | – | 2.2 % |
| 341F65C | xessD3D12Execute (OptiScaler/FSR4) | – | 1.2 % |

Driver hot spots: amdxc64+196B710 (3.2 % self, under D3D12Core+18A80 = CopyDescriptorsSimple), D3D12Core+23BF40
(SetPipelineState), D3D12Core+23B280/229FDC/23E128 (5.7–7.8 % incl).

Per frame (RGP queue events): **~28 GFX ExecuteCommandLists, each with one list**, plus 5–8 compute submits (queues
CMP1–3 = streaming, CMP35–37 = async compute).

## 2. Barriers (RGP r5, `barriers.py`)

| | |
|---|---|
| GFX barriers per frame | **1360** (140 driver-internal, e.g. before clears) in **478 chains** (barriers with no draw/dispatch between them) |
| chain length histogram | 1:132, 2:162, 3:76, 4:32, 5:32, 6:9, 7:8, 8:7, ≥9:20 (up to 23) |
| barriers in multi-barrier chains | 1228; "extra" barriers (k−1 per chain) = **882** |
| flags over all barriers | inv_tcp/inv_sqK 990, wait_ts 662, cs_partial_flush 608, ps_partial_flush 568, **EOP wait 471**, CB+DB flush/inval 255, L2 flush 303 |
| layout transitions | only 14 HiZ-range expands; **no DCC decompress / fast-clear eliminate**, so no hidden decompression cost |
| async queue Q1 | 58 barriers in 13 chains, all cs_partial_flush only |

Chain "tail" = first BARRIER_END → last BARRIER_END inside a chain. Chains with < 10 µs tail: 326 chains, **467 µs** in
total. Long chains show the per-extra-barrier price directly: 9–23 barriers in 2–10 µs, i.e. **~0.3–0.5 µs each**. The
long tails (≥ 10 µs, 677 µs, e.g. 120 µs at 4467.8) are a cheap first barrier followed by a driver-internal barrier that
drains a 2048² full-screen draw: real work, not waste. So the earlier "1.16 ms of barrier tails" (gpu-profile.md) is
about 40 % overhead and 60 % real drains.

**Where the chains come from (CPU).** Every barrier is its own `ResourceBarrier(1, …)` call:
- 115B9C (texture bind) transitions the texture to PIXEL/NON_PIXEL_SHADER_RESOURCE **at bind time** via 115E24.
- 1180D0 then transitions each render target / the depth buffer, one call per RT.
- tok 0x8A / 0x7D are single UAV barriers (for GPU particles: one per non-empty sim stage, not per emitter; particles.md section 6).
- Same-state transitions are already filtered (115E24 compares `*state`; 1180D0 compares too). I found **no redundant or
  immediately-undone transitions** in the code paths, and RGP carries no resource identity to look for them on the GPU.

**B1 – batching.** Merge consecutive barriers into one `ResourceBarrier(n, …)`. That removes 882 of the 1360 syncs.
Estimate: 0.15–0.4 ms of GPU time per frame (467 µs is the upper bound; SQTT markers inflate per-barrier cost a little).
Only worth it when GPU-bound. The clean way is a proxy `ID3D12GraphicsCommandList` vtable installed by a mod DLL:
`ResourceBarrier` appends to a per-list array, and every other method (draw/dispatch/copy/clear/OMSet…/Close/
ExecuteIndirect/ResolveQueryData/BeginEvent…) flushes the array first. Order is preserved, so semantics are identical.
Risk medium (must cover every method, including ones the driver/OptiScaler/XeSS call on the same list). **Not built.**
Byte patching inside 115E24 alone is not enough, because the flush points are scattered over ~50 call sites.

UAV barriers between independent dispatches: 75 single-group per-emitter particle dispatches, with one tok 0x8A UAV barrier
per non-empty sim stage (seg 79, ~100 µs; particles.md section 6). Merging them depends on emitter independence; see
invisible-gpu.md §4b. Not changed.

## 3. Descriptors

- Model: per-stage **staging (CPU) tables** at `SynCtx+0x940` (stride `+0x938`): 14 CBVs + 16-ish SRVs per stage, a
  separate sampler staging heap at `+0x970/+0x968`.
- Bind (115B9C): skipped when the same SRV *and* sampler are already in the slot; otherwise 1 × CopyDescriptorsSimple into
  staging and a dirty bit (`+0x10F0` SRV tables, `+0x1E38` samplers).
- Before a draw (118604): for each dirty stage, allocate N descriptors in the shader-visible ring (1192B0), copy the
  **whole stage table** with one `CopyDescriptorsSimple(N)` and `SetGraphicsRootDescriptorTable`. Dirty CBVs:
  memcpy the constants into the upload ring, `CreateConstantBufferView` into staging, then the same table copy.
- Cost: CopyDescriptorsSimple (all callers) ≈ 3–4 % of the dispatcher's samples (≈ 0.02 core); root-table sets are not
  visible in the top lists. No redundant root-signature or table sets found (everything is dirty-gated).
- **Verdict: no candidate.** A root-CBV redesign would drop CreateConstantBufferView per dirty CBV, but that changes root
  signatures (shader side). The dispatcher is not the bottleneck anyway (§5).
- Minor: every list gets `ClearState(NULL)` before `Close` (490D5C). ~28 per frame; negligible.

## 4. Copies into the command stream (main → stream → D3D12)

- **Token 0x66 (set constant-buffer slot data)**: 242EB8/242FC4 (main) write `0x66 | slot<<16`, the size, then the whole
  blob copied from the CPU constant buffer (B94250 / B94230 `rep movsb`). The dispatcher handler **698EAC copies it again**
  into a per-slot shadow (`SynCtx+0xD70[slot]`), and 118E28 copies it a **third time** into the upload ring before
  creating the CBV. Main-thread cost of these copies ≈ 1.5 % (242EB8 36 % + 242FC4 23 % of B94230's 2.5 %); dispatcher
  ≈ 1–1.5 %.
- 1D7340 (shader-constant slots → tokens 0x2B/0x2C/0x37/0x38 + inline memcpy) ≈ 1.1 % of main.
- 4DD4C → mapped upload heap is a single direct copy (not part of the stream).
- **Could the dispatcher read the source directly?** Only where the source is immutable until the dispatcher has consumed
  it. The main thread does wait for a full drain at the end of each frame (418F40), so frame-lifetime data would be safe,
  but the constant shadows are rewritten between draws inside the frame, so a pointer would see later values. It needs
  per-caller lifetime analysis (or copy-on-write in KTGL). **Assessment: invasive, ≤ 1.5–2.5 % of the main thread.
  Not recommended.**

## 5. Pipeline depth, the waits, and is the submit thread a bottleneck?

### Main-thread waits on the back end
| site | waits for | share of main thread |
|---|---|---|
| **7D9E90** via 7D9B6C (frame begin) | the Present thread to *return* from Present() of the previous frame (`GfxDevice+0x7728`, Sleep(0) loop) | **6.1 % (r5)**, 0.9–3.8 % (r6/r7/ingame) |
| **418F40** via 3D62E4 → 418B94 → 418C70 (frame end) | the dispatcher ring to be empty and idle (`write == read && inflight == 0`), pure busy spin | 0.7–2.1 % |

The Present thread buys almost no overlap: the main thread hands Present over at the end of frame N and waits for it at
the start of frame N+1. The wait is there because 7D9B6C reads `GetCurrentBackBufferIndex` (swapchain vtbl+0x120) and
passes the back-buffer RT into the new command stream (7D9C78).

Present's own cost (r5 present thread, inclusive % of its samples): sl.interposer 8.1 → **RTSSHooks64 7.5** →
gameoverlayrenderer64 3.75 → dxgi 3.6. So RTSS's own part (OSD drawn through d3d11on12) is ~3.7 %, the actual
DXGI/driver present ~3.6 %, Steam ~0.2 %. r7-B1 shows the same split (5.4 / 5.4 / 2.2 / 1.7).

### The dispatcher
- 44–50 % blocked in its semaphore wait (INFINITE, not a poll), so it has headroom overall. It is on the critical path only
  in the end-of-frame drain (418F40 share above). Making it faster (B1, fewer driver calls) shortens that spin, nothing
  more.
- How much it has left at the end depends on how often the main thread kicks: **every `+0x3C70` draws** (K1; compare in
  1D3118 and ~25 other draw paths). The value comes from device-param `+0x804` at init (5D7023) and is not a constant in
  code. Read it live first; if it is large, lowering it moves dispatcher work earlier and shrinks the 418F40 spin, at
  the price of one ReleaseSemaphore (~1–2 µs) per kick.

### GPU side (RGP r5 queue events, `queue_events.py`, CPU QPC mapped to GPU time via the ClockCalibration chunk)
- Every GFX list of frame N is submitted within a **2.5–3.2 ms burst at the end of the CPU frame** (e.g. frame 23125:
  16.84–19.80 ms), 7–8 ms after the previous Present. So the GPU cannot start frame N before the main thread has nearly
  finished recording it.
- GPU idle at frame start: **837 µs** (23125), 0 (23126), 1957 (23127), 862 (23128), 355 (23129). In 23125/23129 the
  first list was submitted after the GPU ran dry: the main thread was late (CPU-bound).
- But in 23127 the first list started **1.4 ms after its submit** on an idle GPU, in 23128 the GPU sat idle 0.86 ms with
  work already queued, and in 23129 the third list waited **2.25 ms** after its submit. These are **GPU-side waits with
  work queued** (a queue Wait or the swapchain's back-buffer wait under VSync ON + VRR; frame-timing.md §4b). The
  trace has no Wait events, so the cause is unresolved. A cheap check: the same RGP capture with in-game VSync OFF (VRR
  on).
- Frames in flight: the CPU records frame N+1 while the GPU runs frame N, and no CPU thread showed a GPU-fence wait in
  the profiles while CPU-bound (4920E0's allocator-recycle callback may wait on a fence when lists run out; not examined). **So no, the pipeline is not
  limited by too few frames in flight.** The idle bubbles are the CPU-bound main thread, plus the unexplained GPU waits
  above.

## 6. Candidates in detail

### P1 – RTSS OSD costs main-thread time (a setting, not code)
Turn off the RTSS/Afterburner on-screen display while playing (keep using PresentMon for measurement). Expect the
7D9E90 share to drop by roughly the RTSS part of Present, ~0.2–0.3 ms per frame, so **+2–3 % fps when CPU-bound**. The
Steam overlay hook is ~0.2 % only. Verify: PresentMon A/B at the shrine spot, RTSS OSD on vs off (RTSS still running vs
fully closed as a third leg). The +2–3 % is an estimate from the profile, not a measured result.

### P2 – do not wait for Present at frame begin (not built)
Idea: in 7D9B6C, skip the 7D9E90 wait, compute the next back-buffer index as `(prev+1) % BufferCount` (flip model), and
wait for `+0x7728` only right before the first use of the back buffer / before 3D62E4. Unknowns: whether frame N+1's
early ExecuteCommandLists may reach the queue before Present(N) (latency only, if nothing touches the back buffer), what
252510 (page-pool bookkeeping, called after the wait) assumes, and XeLL/Streamline marker ordering. High risk; document
only.

### S1 – PAUSE in the drain spin (safe, tiny)
`418F68` (17 bytes):
```
orig  88 05 32 59 1B 04  0F B6 05 2B 59 1B 04  84 C0  75 E7
      mov [45CE8A0],al ; movzx eax,byte [45CE8A0] ; test al,al ; jne 418F60
new   88 05 32 59 1B 04  84 C0  74 07  F3 90  EB EA  0F 1F 00
      mov [45CE8A0],al ; test al,al ; je 418F79 ; pause ; jmp 418F60 ; nop3
```
- `g_DispatcherDrainBusy` (45CE8A0) is still written every iteration; it is referenced only here.
- eax upper bits differ after the loop, but 492954 (the next call) overwrites rax first; flags are dead; nothing branches
  into 418F6E..418F78.
- Verified offline by `re\scripts_d3d12\drainspin_pause.fsx` (bytes, lengths, disassembly).
- Benefit: only less SMT pressure on a sibling thread during the spin. Do not expect fps.

### K1 – kick threshold (needs one live read)
`KtglContext::CmdFlushLimit` (+0x3C70; the same object as GfxDevice). Read it live; if it is in the hundreds or more,
A/B a quarter of it (live data write; the code only compares `>=`). Watch the 418F40 share and the dispatcher's wake rate.

## 7. Names and figures from other notes
- **W6 / 7D9E90 (SpinWaitFlagSleep0) does not wait for the submit thread.** It waits for the *Present* thread to return
  from Present() (flag `GfxDevice+0x7728`, cleared at 1A822D9). 7D9B6C keeps the symbols.tsv name WaitSubmitThread
  (alias RenderFrameBeginWaitPresent), but it waits for the Present thread. The main thread's wait for the submit
  (dispatcher) thread is WaitDispatcherDrainSpin 418F40, a pure busy spin.
- `KtglContext` (stream at +0x3C40) and `GfxDevice` (swapchain at +0x4278) are the **same object**: 418B94/3D62E4 use
  both offsets on one pointer. The dispatcher context (`SynCtx`) is embedded at +0x10.
- The "1.16 ms of back-to-back barrier tails" figure (gpu-profile.md): ~0.47 ms is per-barrier overhead, the rest is real drain (§2).

## 8. Files
- `re\scripts_d3d12\toktab.py` – dump a token handler table (3C02220 KTGL, 3BD4DF0 ECL).
- `re\scripts_d3d12\vcalls.fsx` – virtual calls reachable from handlers, ID3D12GraphicsCommandList method names.
- `re\scripts_d3d12\tokemit.fsx` – find code that writes a given token id.
- `re\scripts_d3d12\barriers.py` – RGP barrier/layout markers → chains, flags, tails (`*.barriers.csv`, `*.chains.csv`).
- `re\scripts_d3d12\queue_events.py` – RGP QueueEvent/ClockCalibration → CPU submit vs GPU start per list.
- `re\scripts_d3d12\drainspin_pause.fsx` – offline check of S1.
- `<re-scratch>\d3d12\` – decompiles (q1–q4), disassembly, vcall tables, barrier/chain CSVs, queue-event dump.
