# Root cause: VRAM budget / residency / streaming on 8 GB cards, and the pc2 system-memory reads - 2026-10-09

Question: on pc2 (RX 6650 XT 8 GB, PCIe 4.0 x8, 1080p HIGH, FSR 65 %) is the game near or over its VRAM budget, churning
residency (MakeResident/Evict, texture and meshlet pages over PCIe, a dropped mip level), and is that what makes mid-range
8 GB cards slow or stuttery? Second question (coordinator, after XSYS1): pc2 keeps 1.19 GB in non-local (system) memory vs
0.19 GB on the PC; do GPU reads of that memory explain the 9-100x slower meshlet front-end and CPU-fed draws
(notes/cross-system-compare.md, ~7.7 ms of pc2's 31.6 ms frame)?

Method: static RE of the v2.02 dump (capstone over `re\Nioh3.dump.exe`; Ghidra was not used while the PC game ran), the
MemStats lines of every capture (PC, pc2, Ally), and a new ASI diagnostic, the **VS probe** (`[Profiler] MemStats = 2`,
ASI 1.7.48-1.7.51, default off): 11 PC tour captures with the XSYS1 config (`configs/xsys-1080-high-fsr65.xml`,
`tourlist_xsys.txt`, GP2 MinRun 1 where marked) and one pc2 capture (#158, ASI 1.7.50). Then a fix, **CM** (`[GpuOpt]
CbvMirror`, 1.7.52, default off), measured on the PC with pc2's memory layout forced.

## 0. Verdict

1. **VRAM budget / residency / texture streaming: NOT a cause. ~0 ms.**
   - pc2 is never over budget: local 5.5-6.4 GB of 6.9-7.4 GB in every capture (VS1 #158: max 6,093 MB of 6,993-7,379).
   - Heavy and light windows use the same VRAM (pc2 spawn 5,960 MB vs warp 5,923 MB, 13 vs 21 points) while their GPU time
     differs 3x: VRAM pressure cannot explain the 30 vs 8-10 ms spread.
   - No residency churn: MakeResident 30-37 and Evict 50-65 objects per 8-minute run, PC and pc2 alike (boot and loads only).
   - The texture page pool is sized from the adapter, not from the live budget: **PC 2,342 MB, pc2 1,389 MB** (64 KB pages).
     pc2 occupancy 33-55 % at the dense spawns, page misses 1-7 per frame, queue-full 0. The engine's only pressure
     reaction (a mip bias that grows while occupancy > 85 %, `PagePoolOccupancyCheck 126C64`) never engages.
2. **System-memory reads explain PART of the pc2 anomaly, not all of it.**
   - pc2 has **Resizable BAR / SAM off** (BAR0 = 258 MB, #867). The engine's 700 MB UPLOAD ring heap + 100 MB READBACK +
     4 x 4 MB (+ transient staging) = pc2's 0.92-1.16 GB non-local. On the PC (ReBAR) the same heaps count as local.
   - **Every CBV the engine creates points into that ring** (PC: 107.7 M of 107.7 M CBV descriptors in 4.5 min, ~2,400 per
     frame, 98 % from the per-draw recorder `118F89 <- 118E1E <- 11863D`, avg 2.9 KB). So on pc2 every draw and dispatch
     fetches its constants from system memory. Also 23 % (pc2) / 26-35 % (PC) of VB binds (particles G1E, UI) and 4-10 % of
     IB binds. Compute root descriptors 0; SRVs/UAVs created on system memory 4.6 % / 0.15 %.
   - **PC causal A/B** (`VSForceSysUpload = 2`, the PC gets pc2's layout): GP2 span 8.90 -> 12.76 ms (+3.86 ms, +43 %).
   - **pc2 per-shader evidence (#1030 arm C vs A: CbvMirror on, DIRECT copies only, same spot 7356; one window each, so single
     rows below ~x2 are not proven)** - moving the per-draw CBVs to VRAM made these anomaly rows faster:
     PB2 small props 2c6f629c 0.651 -> 0.185 ms (x3.5), UI quads AMBSL 0.267 -> 0.110 (x2.4), shadow casters SM-Sfb 1.521 ->
     0.936, MshLod#2 63c07c21 1.633 -> 0.730, eyes PB2EYEC 0.409 -> 0.321; families G-buffer 8.00 -> 5.64, shadows 2.13 -> 1.46,
     meshlet 6.06 -> 5.27. **About 4 ms of real pass time is CBV-over-PCIe** (~half of the ~7.7 ms excess).
   - **Not changed by CBVs in VRAM:** MIC e31c389f 1.631 -> 1.756, MshLod#1 e6a62bda (the 8-byte copy kernel) 1.414 -> 1.309,
     MPC 18b6fee7 / 1b4b5b75 unchanged. The worst meshlet-front-end rows (~3 ms) have another cause (section 0.6).
3. **CM (CbvMirror) is a dead end as built: two designs, both slower on pc2. STOPPED.**
   - 1.7.52 (own COPY queue + a fence wait before every game submission): GP2 span 32.2-33.2 -> 47.7 ms (+15 ms, #988).
   - 1.7.54 inline (prologue list on the game's own queue): +36 ms with async-compute re-copies, **+10.7 ms without** (#1030
     arm C: 32.08 -> 42.79). Arm C's ledger: passes **-4 ms**, but our copies show as a copy bucket of 1.36 -> **17.48 ms** and
     async VCLOUD 2.58 -> 15.17 ms wall: `CopyBufferRegion` from system memory on RDNA2 (CP DMA on the graphics queue)
     moved ~7-8 MB per frame at well under 1 GB/s, i.e. far below the PCIe 4.0 x8 link. Picture fine in every arm.
   - Why the PC said otherwise: the RX 9070 runs the same copies over PCIe 5.0 x16 with a faster DMA path; its emulation
     arm got -2.1 ms. The PC is not a stand-in for RDNA2 copy throughput.
   - Not built (would need a new design, only if ReBAR-ON shows a big ceiling): a compute-shader copy (many waves, full link
     bandwidth) or copying only the bytes the shaders read (the 2.9 KB average view is mostly padding/palette space).
4. **The user-side equivalent is ReBAR / SAM** (user-settings.md). The ReBAR-ON run (section 0.7) is the ceiling test for
   everything system-memory related, CBVs and VB/IB together, with zero copy cost.
5. **Ranked explanations for the pc2 anomaly (excess ~7.7 ms of 31.6 ms), with one cheap discriminating test each:**

   | rank | explanation | evidence for / against | share | cheap test |
   |---|---|---|---|---|
   | 1 | **CBV fetches over PCIe** (no ReBAR) | for: arm C rows above (G-buffer small draws, shadows, MshLod#2, UI); PC emulation +3.9 ms | ~4 ms (measured, arm C pass rows) | done; ReBAR-ON confirms without copy cost |
   | 2 | **Barrier / cache-flush cost billed to the next tiny kernel** (GP2 MinRun 1 stamps the interval that starts after a barrier; ~250 barriers/frame in the meshlet chain; RDNA2 flushes/invalidates L2 on UAV->SRV barriers) | for: MIC / MshLod#1 / MPC unchanged by CBVs; an 8-byte copy kernel costing ~1 ms is not compute; the PC's emulation showed most growth in UN-stamped time | ~2-3 ms | `[GpuOpt] Barriers = 1` (barrier batching, built) A/B on pc2 at spawn 7356 (queued #769) - if MIC/MshLod#1 rows drop, it is barriers; or one RGP capture on pc2 (barrier durations) |
   | 3 | **Atomics / writes into system-memory buffers** (meshlet streaming feedback lives in the 100 MB READBACK heap; GPU atomics over PCIe are ~us each) | for: 0.15 % of UAVs and the readback heap are in system memory and stay there even with ReBAR; MIC/MshLod#1 read/write request lists; against: UAV count is small | 0-2 ms | VS probe: log the callers of the 33 system-memory UAVs (callers table, kind U - currently cut at 24 rows) and match them to MIC/MCLRC; ReBAR-ON does NOT move READBACK heaps, so if MIC stays slow with ReBAR ON this rises |
   | 4 | **RDNA2 occupancy / wave launch cost for 1x1x1 and few-group dispatches** (architecture, not memory) | for: MIC is x56 vs the PC, beyond any memory effect seen; against: the Ally (RDNA3) matched data is still missing | 0-2 ms | Ally XSYS1 matched run (MIC/MshLod#1 rows vs speed factor): if the Ally shows the same per-dispatch blow-up, it is not PCIe |
   | 5 | **VB/IB dynamic streams over PCIe** (particles G1E, UI) | for: 23 % of VB binds in system memory; arm C left them there and the UI rows still sped up only via CBVs | 0.2-0.5 ms | ReBAR-ON (moves them) vs arm C (does not) on the particle rows f645c461 / f32bf54a |
   | 6 | GP2 attribution itself (timestamps inflating tiny intervals) | against: PresentMon GPUBusy agrees with the span (27-28 vs 28.6 ms base) | small | GP2 MinRun 4 vs 1 on the same pc2 spot (MinRun 4 merges the tiny intervals) |

6. Bottom line: VRAM is not the problem. About half of pc2's excess is constant-buffer reads over PCIe because ReBAR is off;
   the mod cannot move them cheaply (copy cost on RDNA2 exceeds the gain), so the practical fix is ReBAR/SAM on the user's
   PC. The other half (meshlet front-end kernels) is not CBV-bound; barriers (test 2) are the lead.
7. **ReBAR-ON ceiling arm (pc2, needs the user in the BIOS; spec):**
   - Same as #1030: ASI 1.7.54 (file #169), CM OFF (`GpuOpt.CbvMirror = 0`), pc2-bench config / XSYS1 spot 7356 spawn,
     30 s dwell, GP2 MinRun 1, `Profiler.MemStats = 2` (VS probe), ini rebuilt per launch, PresentMon.
   - Launches: A A with ReBAR OFF (today's state) -> user enables "Above 4G Decoding" + "Re-Size BAR Support" (CSM off) and SAM
     in Adrenalin -> R R with ReBAR ON -> (optional) back to OFF for one A.
   - Check in the log: `VS: GPUUploadHeapSupported` (may still say n/a), `stats vs:` non-local must drop from ~1.15 GB to
     ~0.2 GB (the UPLOAD heaps move to VRAM; READBACK stays), `stats vs binds` unchanged (same engine), BAR0 ~8 GB (#867 method).
   - Compare per shader (xsys_compare A vs R) and GPUBusy: the R - A delta is the full ceiling of the system-memory branch;
     rows that stay slow with ReBAR ON (expected MIC / MshLod#1 / MPC) belong to explanations 2-4.

## 1. Data (MemStats lines, all captures)

| | PC (RX 9070 16 GB, ReBAR) | pc2 (RX 6650 XT 8 GB, PCIe 4.0 x8, no ReBAR) | Ally (Z1E, UMA, Proton) |
|---|---|---|---|
| adapter (DXGI) | dedicated 16,189 / shared 15,940 MB | dedicated 8,147 / shared 16,346 MB | dedicated 10,685 / shared 5,342 MB |
| local budget | 15,417 MB | 7,379 at boot, 6,898-7,073 in game | 10,685 MB |
| local used, gameplay | 8.0-8.6 GB at XSYS1 HIGH; 8.2-10.0 GB at MAX | 5.5-6.4 GB (84-92 % of budget) | 5.5-6.9 GB |
| non-local used | 0.16-0.19 GB | **0.92-1.21 GB** (from the first sample) | 0.92-1.0 GB |
| texture page pool | 2,342 MB (37,465 pages) | 1,389 MB | (queued #788) |
| pool occupancy at dense spawns | 18-43 % | 33-55 % | - |
| residency calls / 8 min | MakeResident 30-37, Evict 62-65 objects | 32 / 50 | - |

## 2. How the engine sizes and budgets memory (RE, v2.02)

- **DXGI budget is read in exactly three places, none of them a pool size:** (1) `GfxInitQueryAdapterMemory 2EEBCCC` at
  device init: `SetVideoMemoryReservation(min(request, AvailableForReservation))`; (2) `GfxSettingsTooHeavy 1FE3D38 =
  1FE3C48 || 1FE3CD4`, the boot "settings may be unstable, reset?" dialog: 1FE3CD4 reads the budget via `22CECFC` (NvAPI on
  0x10DE, DXGI on 0x1002) and fires only above 2048x1152 (<= 6,000 MB), 3840x1080 (<= 7,000 MB) or 5120x1440 (<= 8,500 MB), so
  never at 1080p (pc2's dialog #790 is 1FE3C48, the settings-above-recommended score; posted #795); (3) the options-menu
  VRAM meter (`283D7B7`, usage/budget >= 0.9 -> warning).
- **Texture page pool** (KTGLGraph cache, `[45BA588]+61A0h`): size = (P+40ECh - P+40F0h) x P+40CCh pages of 64 KB, copied from
  the device create parameters (`5D5964`: +C68h.. -> +40CCh..) - adapter-dependent (PC 2,342 / pc2 1,389 MB), fixed for the
  session. Request pool `+7560h`, per-frame cap `+75A8h` = 128, <= 16 tiles per texture per frame.
- **Pressure reaction** (`PagePoolOccupancyCheck 126C64`, decompiled): occupancy = (used +6CD0h - reserved) / size into a
  120-sample time window of "> 85 %"; while the window says over, every 0.3 x window a streaming bias (t+3C0h) rises by 0.1
  (cap 10 - t+3BCh), and steps back when no longer over. Never sees DXGI. Not engaged on any device measured.
- **Residency calls:** `GpuMemMakeResident 2F118CC` / `GpuMemEvict 2F11614` wrappers (page cache 491D70, texture block
  allocator 127AEC/128294); `GpuMemCreateHeapWithPriority 2F1165C` sets SetResidencyPriority. Counts: section 1.
- **Upload path:** one 700 MB UPLOAD heap (`GpuMemCreateHeapAndDesc 1A6103E <- 2F03387 <- 2F0269E`, at boot) is the ring for
  every CPU->GPU write: `UploadHeapSuballoc 62D688` placed resources (texture staging `GpuMemUploadTexture*`, `GpuBufferMap
  4FD84`, UI `GpuIndexBufferMap 62CE7C`, meshlet upload 4E250) and the per-draw constants. A full ring falls back to committed
  staging (`TextureStagingAllocDedicated 9C2EE0`). A 100 MB READBACK heap from the same path takes the meshlet streaming
  feedback (`4E2C0 <- 62E8D2`, ~30 KB/frame). The game queries no OPTIONS16 / GPU-upload-heap support.

## 3. VS probe (ASI 1.7.48-1.7.51, `[Profiler] MemStats = 2`, default 0)

- `stats vs:` every second: VRAM local/non-local used/budget and MB/s, page pool (size, used, reserved, occupancy, OVER85),
  request pool, last-frame touches/hits/misses/queue-full, D3D12 MakeResident/Evict/CreateHeap/CreateCommittedResource per
  second and MB per memory pool, totals since the hook.
- `stats vs callers` (30 s): system-memory heaps (H), committed (C), placed-in-system-heap (P) and CBV/SRV/UAV descriptors on
  system memory (V/S/U; CBVs sampled 1 in 64) by the first three return addresses inside Nioh3.exe.
- `stats vs binds` (30 s, needs GPUProf = 2): root CBV/SRV/UAV, IASetIndexBuffer/IASetVertexBuffers into system memory, per
  kind and engine marker, plus descriptor totals; `stats vs pso`: the same per bound PSO (sampled frames, VS/PS digests that
  join the GP2 rows) and the VB view-size histogram.
- Hooks: D3D12CreateDevice through the game's IAT (set from the decrypt wait, so the boot heaps are seen), device vtable
  slots 17-19/27-30/34/35, gpuprof command-list slots 37-44. `[Profiler] VSForceSysUpload` 1/2 = PC A/B only.
- Tools: `tools/vs_capture.ps1` (matched tour capture, `-GP2`, `-AddKeys`, `-Tag`), `re/scripts_gpu/vs_digest.py`.

## 4. What the GPU reads from system memory on a PC without ReBAR (census, PC = engine-identical, pc2 confirms)

| what | how bound | per frame | share in system memory (no ReBAR) |
|---|---|---|---|
| per-draw constants (all draws and dispatches) | CBV descriptors (tables), recorder 118F89 | ~2,400 CBVs, avg 2.9 KB | **100 %** |
| other constant buffers | CBV descriptors, 495B2E / 117F20 | ~250 | 100 % |
| particle / UI / dynamic vertex streams | IASetVertexBuffers | ~200 of ~880 binds (pc2) | 23-35 % of VB binds |
| dynamic index buffers (UI) | IASetIndexBuffer | ~44 of ~1,040 (pc2) | 4-10 % |
| root CBVs | SetGraphicsRootConstantBufferView | < 1 | 100 % |
| compute root CBV/SRV/UAV | - | 0 | - |
| buffer SRVs / UAVs | descriptors | rare | 4.6 % / 0.15 % of created |
| texture / meshlet staging | CopyTextureRegion / CopyBufferRegion source | MB during streaming | read once by a copy: normal |

## 5. PC A/B data (XSYS1 spots 7356 / 52 / 7292 / 5631, spawn, 2 x 30 s, GP2 MinRun 1)

| arm | non-local | GP2 span (ms) | stamped | un-stamped | note |
|---|---|---|---|---|---|
| base (x3 runs) | 0.19 GB | 8.90 (8.89-9.15) | 6.14 | 2.75 | same-device p90 noise x1.23 per shader |
| VSForceSysUpload = 1 (write-combined L0) | 0.19 GB | 8.9 | - | - | identical to base: the driver keeps the heaps in BAR VRAM |
| **VSForceSysUpload = 2 (write-back L0) = pc2 layout** | **0.89-1.04 GB** | **12.76** | 6.97 | 5.79 | **+3.86 ms** |
| = 2 + CbvMirror = 2, copies on DIRECT (first build) | ~1 GB | **10.68** | 6.78 | 3.90 | **-2.08 ms**; 296k CBVs/s redirected, 0.66-0.82 GB/s copied |
| = 2 + CbvMirror = 2, own COPY queue (1.7.52) | | not measured | | | first try deadlocked its ring on a loading screen (fixed); PC busy since |

Write-back system memory is snooped, so the PC arm is a slightly pessimistic stand-in for pc2's write-combined memory; pc2's
x8 link is the other way round. The pc2 A/B is the number that counts.

## 6. CM - CbvMirror (ASI 1.7.52, `[GpuOpt] CbvMirror`, default 0) - src/upmirror.cpp

- The CBV hook (device slot 17) redirects each per-draw CBV (return address 118F89) whose range lies in the UPLOAD ring: the
  bytes get a 256-aligned slot in a DEFAULT-heap mirror ring (VRAM, `CbvMirrorMB` = 128), the descriptor is created on the
  mirror address, the (source, mirror, size) copy is queued. Before every game ExecuteCommandLists on a DIRECT / COMPUTE queue
  (osd.cpp hook) one command list on **our own COPY queue** copies the queued ranges from an alias buffer placed over the whole
  UPLOAD heap (merged, sorted), signals a fence, and the game queue `Wait`s on it. Mirror space is retired once a copy flushed
  >= 80 ms later completed (or 250 ms after its own copy when nothing newer came). COMMON-state buffer: implicit promotion
  to COPY_DEST / constant-buffer read, decay at each ExecuteCommandLists. Ring full -> that CBV stays on system memory.
- Modes: 0 off; **1 auto** = on only when the UPLOAD/READBACK heaps (>= 256 MB) show up as DXGI non-local usage (>= 75 %),
  the device is not UMA and the process is not under Wine/vkd3d (so: off on the RX 9070, off on the Ally, on on pc2);
  2 force (test, any PC). Logs: `CM:` lines, `stats/s CM` every 30 s (redirected/s, ring full, flushes, copies, MB/s, waits).
- Assumption A1: a recorder CBV's bytes are final when its descriptor is created (draw prep writes constants on the main
  thread before the dispatcher records). The picture check (shotdiff in the GPU-session flow, item `cm` / `cmforce`) is the
  proof; nothing visual was checked yet (the PC's user was active, no screenshot taken).
- **1.7.54 inline mode (`CbvMirrorQueue = 0`, default):** no own queue. At a game ExecuteCommandLists on a DIRECT / COMPUTE
  queue, one prologue list of that queue's type (copies from the alias, then one barrier COPY_DEST -> VERTEX_AND_CONSTANT_
  BUFFER) goes in front of the game's lists in the SAME call; a per-queue fence is signalled after them (retire). A shared log
  of redirected ranges with a cursor per queue: DIRECT copies everything since its last submission; async COMPUTE queues copy
  the same ranges again into the same slots (identical bytes, buffers are simultaneous-access), skipping retired slots, unless
  `CbvMirrorComputeCopy = 0`. `CbvMirrorQueue = 1` keeps the 1.7.52 path for comparison. CM also stands down if the game
  creates a GPU_UPLOAD heap or OPTIONS16 reports GPU upload heaps (ReBAR exposed to D3D12).
- Development notes: the first ring-retire rule (only "a later copy completed") deadlocked on a loading screen (no traffic ->
  ring full forever, run wbcmq): fixed with the 250 ms idle rule. At 150 fps the PC needs `CbvMirrorMB = 256`; pc2 at
  35 fps moves ~4x fewer bytes per second.

## 7. Next

1. DONE: pc2 1.7.54 inline A/B (#1030) = +10.7 / +36 ms -> CM stopped (section 0.3). Next: the ReBAR-ON arm (0.7) and the tests in 0.5.
2. PC: the same with pc2's layout forced (`-AddKeys "Profiler.VSForceSysUpload=2;GpuOpt.CbvMirror=2;GpuOpt.CbvMirrorMB=256"`),
   queued behind the other PC sessions.
3. If CM holds: a VB/IB mirror for the dynamic vertex streams (needs the drawn vertex range per bind, not the view size; the
   1.7.51 histogram shows most sys VB views are 64 KB-1 MB) - the particle/UI share of the excess.
4. User-side alternative (no code; user-settings.md): enabling Resizable BAR / SAM (BIOS: Above 4G decoding + Re-Size BAR,
   CSM off; AMD Adrenalin SAM) puts these heaps in VRAM on AMD cards; on pc2 that is the user's call (machine change).

## PC no-ReBAR simulation A/B of CbvMirror, 2026-10-09 (pc, tools\testmatrix\cm_sim_pc.ps1 + cm_sim_report.py)
ASI 1.7.54 (arm 1 = an earlier 1.7.5x build from 15:58, another agent rebuilt bin at 16:06; arms 2-6 = the same hash 068BFBCDD0A4, so A = arm 6 is the clean baseline). run_tour vpad, tourlist_xsys (4 spawns), 2 passes x 30 s each, xsys-1080-high-fsr65 config, MemStats 2, GP2 1-in-8 MinRun 1, MaxFps 240. A = VSForceSysUpload 2 + CbvMirror 0, B = VSForceSysUpload 2 + CbvMirror 2, C = normal (ReBAR). Order A B C C B A. Windows = after Tour ARMED + 25 s (includes the warps/loads between spawns, so p99 is contaminated; p50 and GP2 span are in-field).

| # | arm | exit | frames | GPUBusy p50 | GPUBusy p99 | frame p50 | frame p99 | GP2 span | meshlet rows ms |
|---|---|---|---|---|---|---|---|---|---|
| 1 | A | 0 | 20021 | 7.20 | 18.22 | 8.88 | 19.44 | 10.10 | 0.517 |
| 2 | B | 0 | 37801 | 8.22 | 15.43 | 9.94 | 17.54 | 11.29 | 0.815 |
| 3 | C | 0 | 40085 | 6.93 | 15.17 | 8.88 | 17.15 | 9.35 | 0.434 |
| 4 | C | 0 | 40357 | 6.96 | 14.88 | 8.82 | 16.17 | 9.34 | 0.432 |
| 5 | B | 0 | 34672 | 8.38 | 17.25 | 10.47 | 17.94 | 11.59 | 0.743 |
| 6 | A | 0 | 40253 | 7.44 | 15.69 | 9.04 | 17.21 | 9.86 | 0.501 |

A (n=2): GPUBusy p50 7.32 (+-0.12) p99 16.96 | frame p50 8.96 p99 18.33 | GP2 span 9.98 (+-0.12) | meshlet rows 0.509
B (n=2): GPUBusy p50 8.30 (+-0.08) p99 16.34 | frame p50 10.21 p99 17.74 | GP2 span 11.44 (+-0.15) | meshlet rows 0.779
C (n=2): GPUBusy p50 6.94 (+-0.01) p99 15.02 | frame p50 8.85 p99 16.66 | GP2 span 9.34 (+-0.01) | meshlet rows 0.433
B - A: GPUBusy p50 +0.98 ms, frame p99 -0.58 ms, GP2 span +1.46 ms, meshlet rows +0.270 ms
C - A: GPUBusy p50 -0.38 ms, frame p99 -1.66 ms, GP2 span -0.63 ms, meshlet rows -0.076 ms

Reading: the forced layout is active (nonlocal 894-953 MB in A/B vs 162-187 MB in C) but costs only +0.6 ms GP2 span here (A 9.86 vs C 9.34), not the +3.9 ms of the earlier 4-spot run. CbvMirror=2 (copy queue) did NOT win: B is +0.9..1.0 ms GPUBusy p50 and +1.5 ms GP2 span WORSE than A (meshlet rows +0.27 ms). No shotdiff: tour mode takes no screenshots (pc2's cm A/B found shots within noise). Verdict on the PC sim: no gain from CbvMirror; the copy-queue version adds cost.

