# GPU scheduling and synchronization: what the structure costs on pc2 (RX 6650 XT, 1080p HIGH) - 2026-10-09

Question (user): small shader wins don't add up. VCLOUD got 40 % cheaper with zero frame change on pc2, because it runs on the
async queue. So: how does Nioh 3 schedule and synchronize GPU work, and what does that structure cost on mid-range RDNA2?
Inputs: pc2 #125/#126 (Phase A, MinRun 4), #140 (MinRun 1, 34 windows), pc2 VCLOUD A/B #604/#622, pc2 stock/mod PresentMon sessions
(`captures/pc2/raw/`), Ally GQ log (#119), r5 RGP (RX 9070) numbers from gpu-barriers.md / d3d12-submission.md / gpu-scheduling.md,
Ghidra on the v2.02 dump. Tags: **M** measured, **S** static, **E** estimate. Phase U (GQ on pc2) and Phase M (#692) not in yet.

## 0. Verdict

**The scheduling is mostly fine. One structural suspect stands out, and it is not where the earlier notes looked.**

1. **Async VCLOUD overlaps the meshlet front-end, and the two contend.** VCLOUD is kicked at graph node 4. MshLod#1 (an 8-byte copy
   kernel) runs at node 14, while VCLOUD is still in flight. In the pc2 MinRun-1 log (#140), MshLod#1 costs 26-172 us/dispatch in
   light windows and 400-2500 us in dense ones. Across the 13 dense windows its us/dispatch tracks VCLOUD's async wall time
   (**r = 0.88**), not draw count (**r = 0.00**). The regression slope is 0.56 ms per ms of VCLOUD wall, which extrapolates to about 0 at
   VCLOUD's uncontended 0.46 ms. MIC#0 behaves the same way (r = 0.89 vs MshLod#1). **M**
   So most of MshLod#1's 1.5 ms/frame, and probably part of MIC#0's ~1 ms, is time spent running beside VCLOUD, not real work.
   The VCLOUD A/B (-1.2 ms of async work, frame neutral) does not contradict this: the shader rewrite made VCLOUD cheaper per wave,
   but the heavy overlap with the front-end is still there.
   **Test queued (SCHED1, bridge #762; zero new code):** VCLOUD on the graphics queue via the AS row.
2. **No evidence of large cross-queue waits on pc2. M + S**
   - The VCLOUD join (DrawSky, node ~267) sits ~20 ms after its kick.
   - The fog join (StaticNature SSFog, node ~230+) sits after P11-P13 (~3-4 ms of DIRECT work), against fog's 1.0-1.3 ms wall.
   - Whole-GPU idle inside the frame is <= 0.27 ms (PresentMon GPUWait).
   - In dense frames, summed GPUBusy is 100.6-100.9 % of wall time in both the stock and the mod session: the GPU is saturated.
3. **Barriers.** The count is high: 1360 per frame, all single-element calls, no split barriers. But the cost is mostly real
   dependency drains (r5: 5.5 of 6.8 ms of barrier time is waiting on real work). Batching is worth 0.2-0.3 ms at best.
   RDNA2-specific decompression costs (DCC fast-clear eliminate) cannot be ruled out without RGP. **M (r5) / E (pc2)**
4. **Submission is fine.** On the Ally, per-submit gaps are 0.067 ms/frame and list heads 0.013 ms (**M**). On the PC, earlier and
   more frequent submits model at <= 0.1 ms (**E**). ECL merging is not worth its semantic risk.

Realistic recoverable on pc2 dense (~30 ms): **0.5-1.5 ms from the VCLOUD/front-end overlap if SCHED1 confirms it**, plus <= ~0.3 ms
from barriers. Nothing else in the scheduling structure is worth more than ~0.1 ms. The other structural cost is the meshlet
front-end's own serialization (~1-2 ms per notes/meshlet-frontend.md §6), which is engine-level work.

## 1. Async compute: overlap or contention

### 1.1 Queues and joins (S; v2.02 RVAs)
Slots 3 / 4 / 6 are the three shipped async compute queues (gpu-async-wins.md §2). Each kick splits the DIRECT list (GFX Signal ->
compute Wait). Each join is a consumer `3C5D0C` peek -> `GfxWaitAsyncFence 4F8F38`, which issues a queue Wait plus a list split,
unless the fence is already complete on the CPU side.

| slot | work | producer (kick) | consumer (join), found this session |
|---|---|---|---|
| 3 VCLOUD | `b76874bb`, 1 dispatch, 220 waves | NatureSkyDrawClouds 5F2A88 (5F328D), graph node 4 | DrawSky (NatureSkyRender 293230 -> 5F2A88), node ~267 |
| 4 VFSCAT fog | `641524ee`/`5473bfb7` froxels | VolumetricFogScatterNodeExecute 14871C, node 162 | **only one site: `4F9AD8` (peek slot 4 at 4F9BFC, wait 4F9C5E), called from StaticNatureRenderView 108590** (the SSFog pass, P13); gated by RS+968h bit 0x20000 |
| 6 ocean (PFB/IGP/BWVM) | NUpdate | 1A25524 | UltraMarineRecord cold chunk 1A23646 (peek slot 6 at 1A23779, wait 1A2378B), plus 851244 (slot RSI+6) |
| 0 effects | off on AMD | 3D0C67 | EffectViewPrepare / RefProbe 5443DA / 3D38B9 |
| 5, 11, 12 | off | - | 3664ECE (5), MaterialTechniqueBind 3C7FB7/3C7FE4 and ViewVsmListA 1F4CF3/1F4D14 (11/12) |

### 1.2 What pc2's data says (M)
- **VCLOUD wall time vs its work:** 0.46 ms in light windows, 2.2-4.6 ms in dense ones, for the same 220-wave dispatch. Async waves
  get the leftovers of a saturated GPU. With ~20 ms between kick and join, that stretch costs nothing directly.
- **The VCLOUD A/B (#604/#622):** VCLOUD 3.07 -> 1.85 ms (-40 %). GP2 span 33.15 -> 32.3 ms (-0.8, inside launch noise).
  PresentMon GPUBusy 24.6 vs 24.3-25.0 (neutral). Cheaper async work does not shorten the frame.
- **What overlaps VCLOUD: MshLod#1 and MIC#0**, both early in the frame. Per-window table:
  `py -I <scratch>/mshlod1.py captures/pc2/gp2_minrun1_140/run01.log`. The script is reproduced in §6.

  | dense window (draws) | VCLOUD wall | MshLod#1 us/dispatch | MIC#0 ms |
  |---|---|---|---|
  | 1649 | 2.49 | 996 | 0.44 |
  | 1887 | 1.79 | 401 | 0.28 |
  | 1918 | **4.61** | **2498** | 2.40 |
  | 1920 | 2.22 | 668 | 0.11 |
  | 1996 | 2.50 | 911 | 1.81 |
  | 2536 | **4.35** | **1723** | 1.79 |
  | 2842 | 3.38 | 743 | 1.25 |
  | light windows (<= 1437) | 0.5-1.3 | 26-172 | 0.03-0.05 |

  Dense windows: r(MshLod#1 us/dispatch, VCLOUD) = 0.88, r(., fog) = 0.82, r(., draws) = 0.00, r(., span) = 0.58. This is
  correlation, not proof: a third factor that stretches both (memory pressure; pc2 VRAM is at 6.4 / 6.95 GB budget, 1.2 GB non-local)
  would look the same. SCHED1 separates the two explanations.

### 1.3 The MshLod#1 billing question (asked by the meshlet agent)
What GP2 can and cannot put into a dispatch's interval:
- GP2 writes a timestamp before each stamped draw / dispatch / copy / clear. The interval billed to a PSO runs from its stamp to the
  next stamp on the same list. It includes the dispatch, the barriers recorded after it, and any unstamped commands in between.
  Barriers recorded before the dispatch belong to the previous interval.
- **A cross-queue wait cannot hide inside an interval.** Queue Waits are queue operations between lists. The engine splits the list
  at every kick and every join, so a wait shows up as a gap between lists, which GQ measures, never as GP2 interval time.
  The frame-begin UpdateTileMappings / Signals are also queue operations before the first lists.
- That leaves three explanations:
  - (a) **Concurrent execution with async work.** Same-time CU, LDS, VGPR, L2 and DRAM sharing stretches the DIRECT dispatch.
    The data above favours this one.
  - (b) A real large root count (thread count). The Phase M stats frames will give the invocations.
  - (c) Slow memory: TLB misses or demoted resources.
- **Prediction:** with VCLOUD on DIRECT (vcsync), MshLod#1 falls toward its light-window 30-170 us/dispatch, and a new DIRECT
  VCLOUD row of ~0.5 ms appears. If MshLod#1 stays ~1 ms with vcsync, the cause is (b) or (c), and #692's invocation counts decide
  between them.

### 1.4 The Ally (M + S)
- The Ally's top GQ gap edge `[GFM_TextureClear -> App_CopyMRLROcnScene]` (0.59 ms/frame, once per frame, mid-frame) is **not**
  the ocean wait. The slot-6 test was neutral, and the producer made 0 calls at Futamata.
- The only other join in that region is the fog join in StaticNature (slot 4). It is the next candidate.
- Zero-code test: `[Patches] AS = 2`, `[Tuning] AS_Slots = -4`, live ABBA of the AS row at Futamata. Not queued by me: the Ally
  queue is busy, and SCHED1's fogsync arm answers the pc2 half first.

## 2. Barriers and transitions

**Counts.** Per frame (r5, RX 9070; same engine, same API stream; M): 1360 barriers on GFX (1220 API, 140 driver) in 478 chains.
- Flags: 485 EOP waits (full drains), 619 CS and 568 PS partial flushes, 303 L2 flushes, 255 CB/DB flushes.
- Layout work: 14 HiZ-range expands, **0 DCC decompress / fast-clear eliminate** (RDNA4 reads DCC everywhere).

**API usage (S, gpu-barriers.md §3).** Every call is `ResourceBarrier(1, ...)`:
- `Flags = 0`, so **no split barriers**.
- Almost always `Subresource = ALL`.
- No aliasing barriers.
- Same-state transitions are filtered at all 13 sites.
- First use of a resource in each list starts from COMMON (stamp reset; correct D3D12, since 1 list = 1 ECL).

**Classes and what they cost on pc2:**

| class | per frame | cost on RDNA2 | lever |
|---|---|---|---|
| dependency drains (first barrier of a chain waits for the producer) | 5.5 ms of 6.8 ms barrier time on r5 | real: the next pass needs the data | only reordering / async (engine) |
| extra barriers in chains (k-1 of each chain) | 786 removable calls | 0.19-0.26 ms GPU on r5 (E: similar on pc2, the CP cost is per packet) | `[GpuOpt] Barriers = 1`: -7 % on the PC when CPU-bound (dispatcher CPU). **Never tested GPU-bound**; pc2 has ~19 ms of CPU slack per frame, so it could net +0.2-0.3 ms there. Low priority |
| UAV barriers in the meshlet chains (~250) | ~100 dependent stages | CS partial flush + launch latency, ~2.5-3x the RX 9070 per stage | meshlet-frontend.md (engine-level / MPCS) |
| COMMON -> X on buffers at list starts | ~40-110 | at list heads the queue is near-empty anyway; Ally heads 0.013 ms/frame | none (R2a is RADV-only) |
| RT -> SRV of fast-cleared targets (possible DCC FCE on gfx10.3 for non-0/1 clear colors) | unknown (~38 RT clears/frame) | **unknown: needs RGP on pc2** (user OK pending, #556) | only if RGP shows FCE / DCC decompress |

## 3. Queue and submission structure

| item | value | source |
|---|---|---|
| DIRECT lists / ExecuteCommandLists | 21-23 per frame on the PC (r5 ~28 incl. FSR / overlay), 15-18 on the Ally; one list per ECL. Cut at fence points (slot kicks/joins, query waits, UI offscreen), never at K1 | M, S |
| frame-begin queue ops | CacheFlushOnGfx 2EF0938 x2: UpdateTileMappings (token 4) + 2 GFX Signals every frame | S |
| cross-queue syncs | per active async slot: kick (GFX Signal -> CMP Wait) + join (CMP Signal -> GFX Wait, skipped if already complete) = 2 list splits each; ~4 per frame on pc2 (slots 3, 4; 6 only with water) | S |
| copy queues | streaming DMA + tile-mapping COPY queue, CPU-polled; no GFX Wait on the cache COPY fence found | S |
| per-submit gap class | Ally 0.067 ms/frame; PC ~8 us per list boundary (~0.17 ms) | M |
| CPU-late / gated gaps | Ally total gaps 1.0 ms/frame (0.59 at the one edge in §1.4); r5 PC "gated" device-wide stalls ~1.35 ms/frame (tile-mapping hypothesis, gpu-tiles.md); pc2 pending Phase U | M |

**Between-frame idle (pc2).** gpu-unattributed.md's 0.85 ms/frame "FT - GPUTime" is not reliable. Summed per-frame GPUBusy is >= 100 %
of wall time in dense frames: async and copy work of adjacent frames overlaps the frame boundary in PresentMon's attribution.
- The stock session drains nothing either. GPULatency is 49 ms there because the stock game queues deeper (display latency 79 ms vs
  the mod's 41 ms).
- `[Latency] MaxFrameLatency` cannot help here: the OSD sees the presented swapchain as non-waitable (Streamline, flags 2). The log says so.
- No frame-boundary lever found. **M**

## 4. Ranked structural changes (pc2 dense ~30 ms)

| # | change | pc2 ms (E) | difficulty | risk | hardware | status |
|---|---|---|---|---|---|---|
| 1 | **VCLOUD off the async queue while the meshlet front-end runs**: (a) `AS_Slots = -3` (VCLOUD on DIRECT, engine path); (b) later: kick VCLOUD after node 14-35 (engine move) to keep the overlap with cheaper passes | (a) -0.5..+1.5 net: saves the MshLod#1/MIC#0 inflation (~1-2 ms), pays VCLOUD's ~0.5 ms on DIRECT; (b) up to the full inflation | (a) zero code; (b) medium-high (reorder the NatureSky update node or delay the producer section) | none: same shaders, engine sync path | per vendor (RDNA2 Windows first; RDNA4 / NVIDIA / RADV separately) | **(a) queued: SCHED1, bridge #762** |
| 2 | fog (slot 4) on DIRECT, or a later join | pc2 ~0 (no wait expected); Ally maybe +0.3-0.6 (the §1.4 edge) | zero code | none | per vendor | pc2 = SCHED1 fogsync arm; Ally = proposed test (§1.4) |
| 3 | Barrier batching on GPU-bound pc2 | 0-0.3 | built (`[GpuOpt] Barriers = 1`, live row) | medium (hook) | Windows; ~0 on RADV | not queued (marginal; revisit after SCHED1) |
| 4 | Remove RDNA2 DCC decompress / FCE transitions (clear colors, transition points) | unknown, 0-0.5 | medium | low | RDNA1/2 only | needs an RGP capture on pc2 first |
| 5 | ECL merge / earlier submit / Signal elision | <= 0.1 | medium | semantic (per-ECL decay) | all | rejected (gpu-async-wins #6/#7, gq-jt-probes §5) |
| 6 | MaxFrameLatency / frame pacing | ~0 | - | latency | - | not applicable on pc2 (§3) |

Honest bottom line: outside item 1 the queue/sync structure is efficient on pc2. Item 1 is a hypothesis with strong correlational
support and a zero-code test. If SCHED1 shows MshLod#1 unchanged with vcsync, the overlap is innocent, and the 1.5 ms is the meshlet
front-end's own business (root count or memory), to be settled by #692.

## 5. Built / changed this session

- `tools/testmatrix/pc_gpu_session.ps1` and `pc2_pc_gpu_session.ps1`: Phase B ASI items `vcsync` (`[Patches] AS = 1`,
  `[Tuning] AS_Slots = -3`) and `fogsync` (`-4`). Base arms force `AS = 0`.
- `pc_gpu_report.py`: their log proofs (`AS: switch ON, ... kept synchronous while ON 3|4 |`) and target rows (VCLOUD
  b76874bb + vc7 hashes, MshLod#1 e6a62bda, MIC#0 e31c389f; fog 641524ee / 5473bfb7).
- Checks: `pc_gpu_report.py selftest` passes, the PowerShell parse is clean, and a PC `-DryRun -Phases B -Items vcsync,fogsync`
  ends DryRun OK with both items proven. Commit 0c2aa72.
- **Not built: GQ_Cause** (a design, ready if Phase U leaves the cause of the gaps open). It would stamp the CPU QPC at each DIRECT
  ExecuteCommandLists and map it to the GPU clock with `ID3D12CommandQueue::GetClockCalibration` (vkd3d-proton implements it). Each GQ
  gap would then split into CPU-late (the next list was submitted after the queue ran dry) and GPU-gated (already queued). Gated
  gaps would be tagged with the queue ops issued between the two submissions (Wait / UpdateTileMappings / Signal) through tilestats'
  existing queue hooks. That would separate cross-queue waits, paging gates and late submits per edge on pc2, the Ally and the PC
  (r5's "gated" ~1.35 ms). Deferred because Phase U's GQ plus SCHED1 may already answer it.

## 6. Reproduce
- MshLod#1 vs VCLOUD: parse `GP2: frame ... async-compute X ms ... N draws` and rows `cs=e6a62bda` / `b76874bb` / `641524ee|5473bfb7` /
  `e31c389f` per window of `captures/pc2/gp2_minrun1_140/run01.log` (MinRun 1 only: in #126, MinRun 4 merges MshLod#1 into other buckets).
- PresentMon saturation: sum(GPUBusy) / sum(FrameTime) over frames with GPUBusy > 24 ms in `captures/pc2/raw/2026100{9-0020,9-0036}-session/presentmon.csv`.
- Consumers: `re\decomp.cmd c:0x4F8F38 c:0x3C5D0C` plus `asm:` at each peek site (R8D = slot).
