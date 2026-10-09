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
2. **The pc2 slowdown is GPU reads of the engine's UPLOAD heaps over PCIe (H1): proven by census + a causal A/B.**
   - pc2 has **Resizable BAR / SAM off** (BAR0 = 258 MB, pc2 #867). The engine's 700 MB UPLOAD ring heap + 100 MB READBACK +
     4 x 4 MB (+ transient staging) = pc2's 0.92-1.16 GB non-local. On the PC (ReBAR) the same heaps are counted as local.
   - **Every CBV the engine creates points into that ring** (PC: 107.7 M of 107.7 M CBV descriptors in 4.5 min, ~2,400 per
     frame, 98 % from the per-draw recorder site `118F89 <- 118E1E <- 11863D`, avg 2.9 KB; the rest from `495B2E`). So on
     pc2 every draw and every dispatch fetches its constants from system memory. Also 23 % (pc2) / 26-35 % (PC) of VB binds
     (particles G1E f32bf54a / f645c461 / df1296ed, UI a2bd0e86 / ExeUI, dbb52433, e9af3848) and 4-10 % of IB binds. Root
     CBVs < 1 per frame; compute root descriptors 0; SRVs/UAVs on system memory: 4.6 % / 0.15 % of those created.
   - **Causal A/B on the RX 9070:** `VSForceSysUpload = 2` creates the UPLOAD heaps as write-back system memory, which gives
     the PC pc2's layout (non-local 0.89-1.04 GB). GP2 frame span **8.90 -> 12.76 ms (+3.86 ms, +43 %)**, same spots, same
     work (2,355 vs 2,375 draws+dispatches). Stamped draw/dispatch time +0.83 ms (tiny kernels and small draws the most:
     MIC e31c389f x2.1, MshLod 419354de x2.4 / 63c07c21 x1.8, STD 96690c3b x2-5, UI AMBSL x1.8, post x2), un-stamped time
     between them (barriers, waits, launch) +3.0 ms. That is the pc2 anomaly pattern. (Write-combined L0, `= 1`, does not
     move the heaps on a ReBAR PC: unchanged, section 5.)
   - pc2 is PCIe 4.0 x8 (the RX 9070 is 5.0 x16, same latency class): pc2's share of the ~7.7 ms excess that this explains
     is at least the PC's +3.9 ms and plausibly most of it; the pc2 CM A/B gives the number.
3. **Fix built: CM (CbvMirror), 1.7.52, default off.** First PC measurement with the pc2 layout forced: span 12.76 ->
   **10.68 ms (-2.1 ms, 54 % of the penalty back)** with the copies on the DIRECT queue; the COPY-queue version (shipped) is
   being measured (section 6). Expected on pc2: 2-4 ms of 31.6 ms (6-13 % fps), 0 on the PC (ReBAR) and the Ally (UMA).
   The VB/IB share (particles/UI) is not covered yet (section 7).

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
| = 2 + CbvMirror = 2, own COPY queue (1.7.52) | | pending | | | section 6 |

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
- Development notes: the first ring-retire rule (only "a later copy completed") deadlocked on a loading screen (no traffic ->
  ring full forever, run wbcmq): fixed with the 250 ms idle rule. At 150 fps the PC needs `CbvMirrorMB = 256`; pc2 at
  35 fps moves ~4x fewer bytes per second.

## 7. Next

1. PC: the COPY-queue CM arm (`-AddKeys "Profiler.VSForceSysUpload=2;GpuOpt.CbvMirror=2;GpuOpt.CbvMirrorMB=256"`).
2. pc2: `tools\testmatrix\pc2_pc_gpu_session.ps1 -Phases B -Items cm` (ABBA, GPUBusy + shots + shotdiff) on 1.7.52; and VS1
   with CM on to see the per-PSO rows drop.
3. If CM holds: a VB/IB mirror for the dynamic vertex streams (needs the drawn vertex range per bind, not the view size; the
   1.7.51 histogram shows most sys VB views are 64 KB-1 MB) - the particle/UI share of the excess.
4. User-side alternative (no code; user-settings.md): enabling Resizable BAR / SAM (BIOS: Above 4G decoding + Re-Size BAR,
   CSM off; AMD Adrenalin SAM) puts these heaps in VRAM on AMD cards; on pc2 that is the user's call (machine change).
