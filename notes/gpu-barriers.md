# Nioh 3: what D3D12 barriers really cost, which ones can go, and the barrier-batching hook (gpuopt)

Status 2026-10-07. Static analysis + the r5 RGP trace (`captures/rgp/Nioh3_2026-10-06_19-39-58.rgp`, RX 9070, Windows,
1440p out / 1080p render, FSR4, 10.7 ms GPU frame) + an offline D3D12 test harness on this PC. The game was not launched
and nothing in the game folder changed. New code is default OFF and not integrated (the ASI's `src` belongs to another
agent; integration steps in §7).

## TL;DR

| | finding | number (PC r5, one frame) |
|---|---|---|
| 1 | Barriers per frame on the GFX queue | **1360** (1220 API, 140 driver-internal) in **478 chains** (no draw/dispatch/clear/copy between) |
| 2 | Flags | 485 EOP waits, 619 CS partial flushes, 568 PS partial flushes, 303 L2 flushes, 225 L2 invalidates, 255 CB/DB flushes; 48 without any wait |
| 3 | Time inside barriers | 6.84 ms summed, but **5.51 ms is the first barrier of a chain waiting for real work to finish** (dependency drain, not waste). The GPU is completely idle for only **0.56 ms** of it |
| 4 | GPU idle over the frame | 1.86 ms: 0.66 ms while a barrier chain is open, **1.07 ms at command-buffer boundaries** (submission gaps: CPU-bound, d3d12-submission.md §5), 0.13 ms other |
| 5 | **Saving from batching each chain into one ResourceBarrier(n)** | 786 of 1220 API calls disappear (1220 → 434, −64 %); GPU idle saved **0.19 ms (conservative) – 0.26 ms (upper)**, up to ~0.45 ms if the extra barriers that overlap async-compute waves also count. **≈ 2–4 % of the PC frame** |
| 6 | Redundant barriers | none provable from the GPU trace (no resource identity in RGP); the engine already skips same-state transitions at every site. The hook drops two provably neutral cases (duplicate UAV barriers, opt-in undo pairs) and can **trace** resource-level data to find more |
| 7 | "75 particle dispatches, each with a UAV barrier" | **refuted**: seg 78/79 (74 + 75 `Dispatch(1,1,1)`) have **no barrier between the dispatches**; they issue in ~5 µs and run concurrently. The 100 µs "segment" is an **87.5 µs CP-DMA wait** (a buffer copy without an event marker) after them. Merging the dispatches would save ≤ 10 µs → not built |
| 8 | RefProbe GI light-group pair | 3 API barriers between #121 group 1 and group 2 (1 between the #414 pair); the cost is group 1's 35 µs drain, not the barrier. Removing it needs proof that group 2 does not read group 1's output → trace mode first, not built |
| 9 | **Ally (vkd3d-proton → RADV)** | RADV already merges consecutive barriers: `vkCmdPipelineBarrier2` only ORs flush/wait bits into `cmd_buffer->state.flush_bits`, emitted once at the next draw/dispatch (`radv_emit_cache_flush`). So the PC's per-barrier GPU cost **does not exist on the Ally**. Expected Ally GPU saving from batching **≈ 0–0.1 ms**, plus ~790 fewer vkd3d ResourceBarrier translations per frame on the dispatcher thread (CPU → watts). **This replaces the "1–2 ms Ally" estimate** in gpu-savings-round2.md #3 / gpu-maxsettings.md #10 (medium confidence: from RADV's design, not from an Ally trace) |

**Built:** `mod/Nioh3EngineFix/src/gpuopt.{h,cpp}` + `gpuopt_thunks.asm` — a D3D12 command-list vtable hook that defers
the dispatcher thread's barriers and emits each chain as one `ResourceBarrier(n)`, with live switch, counters and a
resource-level trace mode. Verified against the real D3D12 runtime (D3D12Core 10.0.26100.9278, RX 9070, driver
8.18.10.0504) in a scratch harness: 20/20 checks (order, batching, drops, argument preservation through the thunks,
cross-thread flush, COMPUTE lists, live off, trace, real submits with no device removal). Default OFF.

**Expected:** PC GPU **−0.2–0.3 ms** per frame when GPU-bound (+2–3 % fps), dispatcher CPU −64 % ResourceBarrier calls.
Ally **≈ 0–0.1 ms GPU**, a small CPU/power gain; measure before expecting more.

---

## 1. Method

`re/scripts_d3d12/barrier_cost.py` (new) extends `barriers.py` with the SQTT **wave timeline** (3.1 M waves, all SEs, all
queues): every barrier interval is split into time with shader waves still in flight (= the barrier is waiting for real
work: a dependency drain) and time with **no wave anywhere on the GPU** (= pure sync/flush overhead). Each chain (barriers
with no draw/dispatch/clear/copy between them, i.e. what one batched call would be) is split into the first barrier and
the time the 2nd…kth add after the previous one ended.

Merge estimate per chain: **upper** = idle time added by API barriers 2..k; **conservative** = upper minus the largest
increment that brought new flush/wait flags (a merged barrier still has to do the union of the flushes once). Driver-
internal barriers (e.g. before clears) stay separate. Outputs: `D:\nioh3-re\d3d12\bcost\*.bcost_chains.csv`,
`*.bcost_idle.csv`, `summary_r5.txt`.

```
frame span 10.716 ms; GPU busy (any wave in flight) 8.854 ms; idle 1.861 ms
  idle while a GFX barrier chain is open: 0.660 ms; at CB boundaries (CB_END->CB_START): 1.068 ms; other: 0.133 ms
GFX barriers: 1360 (1220 API, 140 driver-internal) in 478 chains
  flags: EOP waits 485, CS partial flush 619, PS partial flush 568, L2 flush 303, L2 inv 225, CB/DB flush 255, no wait 48
  sum of barrier durations 6.838 ms, of which GPU fully idle 0.562 ms
  first barrier of each chain: 5.774 ms = drain under running work 5.510 ms + idle 0.264 ms
  2nd..kth barriers add 1.064 ms (idle 0.298 ms); idle from chain end to next event 0.029 ms
  merge (batch API barriers per chain): 786 API barriers removed; saving upper 0.255 ms, conservative 0.186 ms
  CP gaps between consecutive barriers of a chain: 0.104 ms (GPU idle 0.099 ms); >5 us: 1 gap 0.088 ms, followed by a CP-DMA wait
  chains by API-barrier count: 0:44 1:148 2:115 3:68 4:30 5:32 6:7 7:9 8:7 9+:18
```

| next work after the chain | chains | API barriers | merge saving upper / cons. (µs) |
|---|---|---|---|
| Dispatch | 153 | 450 | 104.6 / 76.1 |
| api15 (dispatch-like, has group dims: indirect/EI dispatch) | 14 | 75 | 34.8 / 24.5 |
| api31 (ExecuteIndirect draw, meshlets) | 13 | 63 | 25.1 / 21.8 |
| DrawInstanced | 124 | 288 | 21.2 / 16.3 |
| DrawIndexedInstanced | 65 | 138 | 19.8 / 15.4 |
| ClearRenderTargetView | 31 | 62 | 14.4 / 9.9 |
| api32 (EI variant) | 18 | 79 | 13.5 / 7.9 |
| CB_END (barriers right before Close) | 16 | 33 | 11.6 / 10.9 |
| CopyTextureRegion | 8 | 21 | 7.9 / 2.3 |
| ClearDepthStencilView | 4 | 10 | 1.6 / 1.2 |

Per extra barrier the CP pays ~0.3–0.5 µs plus its own cache action: a typical case is four `eop + wait_ts + L2 flush/inv`
barriers in a row (4 render targets of one pass transitioned one call at a time, e.g. 1621.5–1624.4 µs): ~2.6 µs, of which
~2 µs disappear when merged.

**Correction to earlier notes.** gpu-profile.md's "1.16 ms of barrier tails" and the round-2 "barrier/flush overhead ~14 %
of the frame" mix three things: real dependency drains (most of it), CB-boundary gaps (submission timing, 1.07 ms) and
per-barrier overhead. Only the last is removable by batching: 0.19–0.26 ms of idle GPU (≤ ~0.45 ms incl. overlap).

## 2. The 100 µs "particle" segment and the GI pair

**Particles (seg 78/79, ~1514–1625 µs).** Seg 78 = 74 × `Dispatch(1,1,1)` PSO 49aea368 (10.6 µs), seg 79 = 75 ×
`Dispatch(1,1,1)` PSO 0aac43e6. All 75 dispatch events of seg 79 are issued in 4.7 µs with **no barrier marker between
them**; then two cs_pf barriers (done at 1534.0 µs, waves finished), then **87.5 µs with no marker and no wave**, ended by
a barrier whose flags include **`cpdma`** (wait for CP DMA). The GPU was executing a CP-DMA copy (a `CopyBufferRegion`
the driver routes through the CP's DMA engine; DMA copies get no event marker). So:
- invisible-gpu.md §4b / round-2 #4 ("75 dispatches, each closed by a UAV barrier, ~100 µs") is wrong for this trace;
  the particle work itself is ~5–15 µs. Batching those dispatches would save ≤ 10 µs (PC) → **not built**.
- The 87.5 µs CP-DMA copy is a separate lead (PC only: RADV copies buffers > 4 KB with a compute shader, not CP DMA).
  The trace mode below records `CopyBufferRegion` arguments, which identifies it.

**RefProbe GI light groups (segs 228–231).** #121 group 1 (35 µs) → 3 API barriers (cs_pf, ps_pf, vs_pf; tail 0.69 µs) →
#121 group 2 (302 µs); #414 group 1 (57 µs) → 1 barrier → group 2 (260 µs). The removable part is group 1's drain (its
last waves finishing while group 2 could already start): PC ≤ ~10–20 µs, Ally maybe 0.05–0.15 ms. Dropping these needs
proof that group 2 does not read what group 1 writes (both write `uOutTex1`, disjoint pixels by stencil, but the 3 barriers
may be transitions of other resources). RGP has no resource identity → **run the trace mode** (§6) before designing a drop.

## 3. Where the barriers come from (engine code, v2.02 RVAs)

Every barrier the engine records is a single-element `ResourceBarrier(1, &b)` (`call qword ptr [rax+0D0h]`,
`FF 90 D0 00 00 00`, bytes verified in the dump at all 13 sites). All transitions use `Flags = 0` and almost always
`Subresource = 0xFFFFFFFF`; no split barriers, no aliasing barriers.

| site | function | token / caller | what | same-state skip |
|---|---|---|---|---|
| 115E97 | SynTransitionResource 115E24 | 45 callers: texture bind 115B9C (SRV → PIXEL/NON_PIXEL_SR at bind time), compute binds, EI argument buffers (6EB908), copies, clears, GfxDevicePresent (back buffer → PRESENT, main thread) | generic tracked transition; resets the tracked state to COMMON when the per-list stamp `c+0x6D0` changed | yes |
| 118443 | SynFlushGfxStateForDraw 1180D0 | before every draw | one call **per render target** (RT state 4) | yes |
| 118519, 118597 | 1180D0 | before every draw | depth buffer (DEPTH_WRITE, or DEPTH_READ\|SRV) | yes |
| 1139FE | SynUavBarrier 1139D4 | 83BBD8, 8988A8 | UAV barrier | – |
| 113A52 | SynTokUavBarrier 113A0C | tok 0x8A (table entry 3C02670) | UAV barrier, resource from the stream | – |
| 1146D0 | SynSetIndexBufferTracked 114614 | tok 0x7A/0x7B | index buffer transition, then IASetIndexBuffer | yes |
| 114AF4 | SynTokSetIndexBuffer 114A20 | tok 0x3D | same | yes |
| 7F5365 | SynTokTransitionResource 7F52AC | tok 0x9B | explicit transition from the stream | yes |
| 7F9D27 / 7F9D64 / 7F9D9F / 7F9DD6 | SynCopyTextureRegionTracked 7F9BF8 | tok 0x92 | src → COPY_SOURCE, dst → COPY_DEST (2 calls), CopyTextureRegion, both restored (2 calls) | yes |

Per draw the dispatcher records: [bind-token transitions (115E24)] → 1180D0: SynFlushPsoAndBindings 118604 (topology,
PSO, root tables) → RT transitions (118443) → DSV transition (118519) → OMSetRenderTargets → viewports/scissors → draw.
Between the barriers of one chain there are **only state setters** (and device-level CopyDescriptorsSimple), which is why
one deferral rule — "emit before the next command that executes work" — merges a whole chain.

**Redundancy, statically:** every site compares the tracked state first, so there are no same-state transitions. The
only patterns that look improvable without semantics knowledge are (a) the single-element calls (→ batching), (b)
7F9BF8's restore transitions right after the copy (→ a split-barrier candidate later), (c) the per-list reset to COMMON
(115E24/114614/7F52AC with stamp `+0x6D0`): the first use of a resource in a list re-transitions from COMMON. Whether that
ever produces an unnecessary barrier depends on promotion/decay rules per resource — trace mode will show it.

## 4. Design options considered

| option | verdict |
|---|---|
| **Byte-patch the 13 call sites** (6-byte `call [rax+0D0h]` → `call [rip+x]` to a deferring function) | engine-precise, but the flush points are ~50 call sites plus third-party recorders (FSR/XeSS via OptiScaler, Streamline) on the same list; with Streamline's proxy list the engine's list pointer differs from the one the runtime sees. Not chosen |
| **Hook ID3D12GraphicsCommandList's vtable in the D3D12 runtime** (D3D12Core / vkd3d-proton), defer `ResourceBarrier` per list, emit before any non-setter method | **chosen**: every recorder ends up in that vtable (engine directly, Streamline proxy forwards, upscalers record through it), so no path can slip a command between a deferred barrier and its emit |
| Merge the 74/75 single-group particle dispatches | ≤ 10 µs PC; needs per-emitter cbuffer changes (shader + engine) → no |
| Drop UAV barriers between "independent" dispatches | not provable from RGP; needs per-dispatch read/write sets → trace first, then decide per site |
| Split barriers (BEGIN at producer end, END at first use) for EOP-heavy RT → SRV transitions | real PC gains possible where independent work sits between, but needs the next-use point per resource → future, from trace data |

## 5. gpuopt: what is built and why it is safe

Files: `mod/Nioh3EngineFix/src/gpuopt.h` (API + integration comment), `gpuopt.cpp`, `gpuopt_thunks.asm`.

**Install** (`InstallNow`, once the engine renders): create a throw-away device through the *system* d3d12.dll (as the OSD
does), one DIRECT and one COMPUTE list, read their vtables (they differ on D3D12Core: +0x30E830 / +0x30ED90), size them
by QueryInterface up to ID3D12GraphicsCommandList10 (86 slots), release the dummies, then patch: all flush and bind slots
first, **ResourceBarrier last**, so a barrier can never be deferred before every flush point exists. Slots never unpatched;
switching off = pass-through.

**Slot classes** (`kSlotInfo`):
- *flush* (emit pending first): QueryInterface (unwrap by Streamline/upscalers), Release, Close, Reset, ClearState, every
  Draw/Dispatch/Copy/Resolve/Clear/Discard, ExecuteBundle, ExecuteIndirect, queries, SetPredication, markers/events,
  SOSetTargets, **OMSetRenderTargets** and **SetSamplePositions** (conservative: DSV layout / MSAA depth decompression may
  depend on them), all List1–List10 work methods, Barrier (enhanced), unknown slots.
- *bind* (crossed, unless `MergeAcrossBinds = 0`): topology, viewports, scissors, blend factor, stencil ref, SetPipelineState(1),
  descriptor heaps, root signatures and all root arguments, IA buffers, depth bounds, view-instance mask, shading rate,
  depth bias, strip cut, SetProgram.
- *skip* (not hooked): AddRef, Get/SetPrivateData(Interface), SetName, GetDevice, GetType.

**Thunks.** One per slot and vtable set (2 × 96). Fast path (no stack change): `trace off && (slot is bind || rcx not in
the 4 pending-list slots)` → `jmp [orig]`. Slow path calls `gpuopt_slowN` (full unwind info; saves rcx/rdx/r8/r9 and
xmm0–3, 16-byte aligned), which calls `GpuOptSlow(self, set*256+slot, a1, a2, a3)` and returns; the thunk then tail-jumps
to the original with the caller's exact registers and stack. (The harness caught one alignment bug — movdqa on a
misaligned frame — fixed and re-verified.)

**Deferral rules** (`HkResourceBarrier<set>`):
1. Defer only when batching is on, the calling thread's name contains `DeferThreads` (default `SynergisticDispatcher`), and
   the return address is inside Nioh3.exe or sl.interposer.dll (`AnyCaller = 1` lifts that). Everything else first emits
   whatever is pending on that list, then passes through. Calls the D3D12 runtime makes on itself are therefore never
   deferred, and the main thread's frame-end barrier on the dispatcher's list (GfxDevicePresent) flushes the batch first.
2. One pending batch per deferring thread (max 4), each with an SRW lock; `gpuopt_pend[i]` mirrors the list for the thunks.
3. Order kept. A second barrier on a resource already in the batch emits the batch first (per-resource sequences never
   rely on in-call ordering inside a driver). Aliasing barriers are never deferred.
4. Drops (counted): duplicate UAV barrier on R (or after a NULL UAV barrier) with no command in between — the first already
   orders everything the second would (`DropDuplicateUav = 1`). Undo pair A:S1→S2, A:S2→S1 (flags 0, same subresource) for
   S1 ∈ {RENDER_TARGET, DEPTH_WRITE, read-only states} — identity with no command between; never for UNORDERED_ACCESS /
   COPY_DEST / COMMON (`DropUndoPairs = 0` by default; always counted).
5. Batch limit `MaxBatch` (64).

**Why the result is identical.** D3D12 specifies `ResourceBarrier(n, b)` as n sequential calls. A barrier orders and
transitions only the work recorded before it against the work recorded after it; gpuopt never moves a barrier across a
command that executes, observes or hands off GPU work (all such methods are flush slots, and unknown slots flush). State
setters do not access resources. Hence the dependency graph the GPU sees is unchanged; the driver just gets one call
with the union of the waits instead of k calls.

**Fail-safes.** Install under SEH (any fault → Failed, game untouched). Re-entrancy guard (our own emits, detours inside
them). Watchdog every 2 s: if another component replaces one of our slots, batching is switched off and logged.
`Barriers = 2` installs the hooks with batching off (counters only) for a live A/B.

**Counters** (`GetStats` / `StatusLine`): barriers and calls seen, deferred, passed through, batches emitted and their
size, dropped duplicate UAVs, undo pairs seen/dropped, conflict and size splits, foreign flushes (another thread emitted),
max batch, emits per triggering method.

**Trace mode** (`TraceRecords = N`, `TraceDelaySec = 30`): after the delay, records every hooked call on every list (thread,
list, method, first 3 args, barrier resource/states/flags, engine caller RVA from the stack) into memory, then writes
`Nioh3EngineFix.gpuopt-trace.csv` next to the ini. `re/scripts_d3d12/gpuopt_trace.py` reports barriers per call site,
emitted batch sizes, undo pairs, UAV barriers between same-PSO dispatches and resources bounced across one piece of work.
This is the resource-level evidence RGP cannot give (for §2's GI pair and any future drop). Tracing slows the dispatcher
while it runs (roughly 15–25 frames for 300k records at ~12–20k list calls per frame).

### Offline verification (scratch harness, not committed; game not involved)

Real D3D12Core runtime + RX 9070 driver, spies installed in the vtable *before* gpuopt record what reaches the runtime:

```
[PASS] batch across a state setter, emitted before work      Topo RB(3: A B C) Copy
[PASS] duplicate UAV barriers dropped (C twice, C after NULL) RB(2: UC UNULL) Copy
[PASS] same resource again splits the batch                   RB(2) RB(1) Copy
[PASS] undo pair kept by default / dropped with DropUndoPairs=1 (read-only S1) / never for UNORDERED_ACCESS
[PASS] CopyTextureRegion: 3 stack args intact after the slow path
[PASS] ClearDepthStencilView: xmm3 float + stack args intact after the slow path
[PASS] MergeAcrossBinds=0: setters flush, floats intact; bind slot fast path
[PASS] Close emits the batch first
[PASS] main thread call flushes the dispatcher's batch first; main thread not deferred
[PASS] submit + fence wait (device removed reason 00000000)  x2
[PASS] batching off: unchanged pass-through
[PASS] compute list (vtable set 1): 2 deferred, 1 emit of 2 at Close; compute submit OK
[PASS] trace CSV written and parsed by gpuopt_trace.py
```

Build: `cl /O2 /MT /GS /W4 /permissive- /std:c++17` + `ml64` clean (no warnings); a scratch link of
dllmain/osd/menu/caves + gpuopt objects links (no symbol clashes). Not verified: the game itself, vkd3d-proton,
Streamline's proxy path, OptiScaler's FSR4 evaluation on the same list (all covered by design, see §8 risks).

## 6. Platform reality: PC vs Ally

- **PC (Windows, AMD PAL driver).** Each `ResourceBarrier` call becomes its own barrier packet sequence (RGP shows a
  BARRIER_START/END pair per call, each with its own wait/flush flags). Batching removes ~786 of them: 0.19–0.26 ms of idle
  GPU per frame (≤ ~0.45 ms), plus driver CPU time on the dispatcher thread.
- **Ally (Bazzite, vkd3d-proton → RADV, RDNA3 780M).** vkd3d-proton turns each `ResourceBarrier` call into one
  `vkCmdPipelineBarrier2`. RADV does not emit a wait per call: it accumulates the source/destination flush and
  partial-flush bits in `cmd_buffer->state.flush_bits` and emits them once in `radv_emit_cache_flush()` before the next
  draw/dispatch/copy (only image layout transitions that need a decompress pass emit earlier; on RDNA3 DCC/HTILE are
  shader-readable, so those are rare). Back-to-back barrier calls therefore already collapse into one GPU sync. Batching
  on the Ally saves **CPU** (≈790 fewer vkd3d ResourceBarrier translations + `vkCmdPipelineBarrier2` per frame on the
  dispatcher thread) and at most ~0.1 ms GPU. The Ally's barrier-related GPU time is the **dependency drains** (the 5.5 ms
  class on PC), which only split barriers / async overlap / dropping proven-redundant barriers can reduce — that needs the
  trace data first.
- Confidence: high for the PC numbers (measured), medium for the Ally claim (RADV source behaviour; no RDNA3 trace yet).

## 7. Integration (for the ASI owner; nothing in the existing src was edited)

1. `build.bat`: `ml64 /nologo /c /W3 /Fobuild\gpuopt_thunks.obj src\gpuopt_thunks.asm` and
   `cl … /Fobuild\gpuopt.obj src\gpuopt.cpp` (same flags as osd.cpp); add both objects to `link`. CMakeLists: add
   `src/gpuopt.cpp src/gpuopt_thunks.asm`.
2. dllmain (next to `osd::Start`): `gpuopt::StartInfo gi; gi.log = Log; gi.iniPath = iniPath; gi.wine = wineVersion;
   gpuopt::Start(gi);` — reads `[GpuOpt]`, returns at once; with `Barriers = 0` and `TraceRecords = 0` nothing happens.
3. osd `InstallThread`, right after `InstallHooks()` succeeded: `gpuopt::InstallNow();` (otherwise `Start` installs by
   itself `InstallDelaySec` after d3d12/dxgi are loaded).
4. osd Present hook, once per presented frame: `gpuopt::OnFrame();` (per-frame numbers in the status line).
5. Menu: item "GPU barrier batching" (on/off → `gpuopt::SetBatching`, shown only when `GetState()` is Active/Paused),
   status row `gpuopt::StatusLine(buf, n)`; optional rows "Merge across binds" (`SetMergeAcrossBinds`) and "Drop undo pairs"
   (`SetDropUndoPairs`). Save to ini: `[GpuOpt] Barriers = 1` (on) / `2` (hooked, off).
6. Stats thread every 30 s: `Log("stats/s gpuopt: %s", gpuopt::StatusLine(buf, sizeof buf))`.

ini block to add (all default OFF):
```
[GpuOpt]
; D3D12 barrier batching on the game's command lists (notes\gpu-barriers.md). 0 off, 1 on, 2 hooks + counters only
Barriers = 0
MergeAcrossBinds = 1      ; 1: state setters do not split a batch; 0: only literally adjacent ResourceBarrier calls merge
DropDuplicateUav = 1      ; drop a UAV barrier already covered by one in the same batch (result-neutral)
DropUndoPairs = 0         ; drop A:S1->S2,A:S2->S1 pairs for RT/depth-write/read-only S1 (result-neutral; counted anyway)
MaxBatch = 64
DeferThreads = SynergisticDispatcher
AnyCaller = 0
TraceRecords = 0          ; e.g. 300000: write Nioh3EngineFix.gpuopt-trace.csv once (resource-level barrier log)
TraceDelaySec = 30
InstallDelaySec = 20      ; only used when osd does not call gpuopt::InstallNow()
```
Log prefix `GPUOPT:`.

## 8. Risks

| risk | mitigation |
|---|---|
| a recorder writes into the list without going through the hooked vtable (cached unwrapped pointer of a different class) | the hooked class *is* the runtime's list; proxies forward into it; QueryInterface flushes |
| another hooker restores a slot after us | watchdog → batching off + log; slots we patched are re-checked every 2 s |
| another hooker (OptiScaler, overlays) detours the same functions | chain-compatible: our "original" is whatever was in the slot; inline detours sit behind it |
| driver bug with large mixed batches | conflict splitting keeps one entry per resource; `MaxBatch`; live off switch |
| the dispatcher thread is renamed / unnamed under Wine | nothing is deferred (log: no "first deferred barrier" line); `DeferThreads = *` with `AnyCaller = 0` still limits deferral to engine/Streamline callers |
| a list destroyed while holding a batch | Release is a flush slot |
| vkd3d-proton vtable layout | sized by QueryInterface; only known slots patched |

## 9. A/B plan

**Every run:** same save, same spot (the r5 shrine spot on PC; Futamata on the Ally), 60 s warm-up, then log.

PC (GPU-bound settings, 1440p, FG off):
1. `Barriers = 2` (hooks, batching off) vs `Barriers = 1`, toggled live in the menu, ABAB 60 s each, PresentMon GPU busy.
   The log's `stats/s gpuopt` line must show ~1200 barriers/frame → ~430 calls.
2. RGP capture with batching on: `barrier_cost.py` should show ≈ 434 API chains with 1 barrier each, `2nd..kth add` near
   0 for API barriers, GPU idle while chains are open down by ≈ 0.2 ms. Compare `segments.csv` totals.
3. Visual: no change expected; any corruption → off switch, report the trace.
4. Once: `TraceRecords = 300000` run → `py -I re\scripts_d3d12\gpuopt_trace.py <csv>` → decide on GI-pair / UAV drops.

Ally (Bazzite; game-side only, stock environment, no env vars):
1. Same live A/B (`Barriers = 2` ↔ 1 via the menu), MangoHud CSV, 120 s ABAB, fixed in-game settings. Compare fps, GPU
   clock/power and **[App]Game / dispatcher CPU**. Expect ≈ no GPU change; a CPU/power change is the possible win.
2. If an RDNA3 RGP trace is available (RADV's own SQTT trigger), run `barrier_cost.py` on it: chains should already show
   one effective flush per chain (delayed barrier-end markers), which confirms §6.

## 10. Files

| file | what |
|---|---|
| `mod/Nioh3EngineFix/src/gpuopt.h`, `gpuopt.cpp`, `gpuopt_thunks.asm` | the hook (new, default OFF, not integrated) |
| `re/scripts_d3d12/barrier_cost.py` | RGP barrier cost with wave occupancy, merge estimate, idle attribution, `--window t0 t1` marker dump |
| `re/scripts_d3d12/gpuopt_trace.py` | analyser for the trace CSV |
| `D:\nioh3-re\d3d12\bcost\` | r5 outputs (`summary_r5.txt`, chains/idle CSVs) |
| `re/symbols/incoming/gpu-barriers.tsv` | 18 rows: the 13 barrier call sites, 4 barrier-emitting handlers, token entry 0x8A |
