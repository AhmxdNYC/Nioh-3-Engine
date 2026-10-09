# Root cause: VRAM budget / residency / streaming on 8 GB cards (and the pc2 non-local GB) - 2026-10-09

Question: on pc2 (RX 6650 XT 8 GB, 1080p HIGH, FSR 65 %) is the game near or over its VRAM budget, churning residency
(MakeResident/Evict, texture and meshlet pages over PCIe every frame, a dropped streaming mip level) and is that what makes
mid-range 8 GB cards slow or stuttery? Second question (coordinator, after XSYS1): pc2 keeps **1.19 GB in non-local (system)
memory** vs 0.19 GB on the PC; are GPU reads of that memory over PCIe the cause of the 9-100x slower meshlet front-end and
CPU-fed draws (notes/cross-system-compare.md)?

Method: static RE of the v2.02 dump (capstone over `re\Nioh3.dump.exe`; Ghidra was not used because the PC game was running),
the existing MemStats lines of every capture (PC, pc2, Ally), and a new diagnostic in the ASI (**VS probe, `[Profiler] MemStats = 2`,
1.7.48-1.7.50**, default off) run on the PC: 5 tour captures with the XSYS1 config (`configs/xsys-1080-high-fsr65.xml`, tour
`tourlist_xsys.txt`), 3 of them GP2 MinRun 1, plus a forced-system-memory A/B. pc2 and the Ally runs are queued (#787/#796, #788).

## 0. Verdict

1. **VRAM budget / residency / texture streaming: NOT a root cause on pc2. ~0 ms.**
   - pc2 is never over budget: local VRAM 5.5-6.4 GB of a 6.9-7.4 GB budget (84-92 %) in every pc2 capture (phase A, #140,
     XSYS1 #145, session #441), flat after the load, no growth.
   - Heavy and light windows use the same VRAM: spawn points (25-34 ms GPU) 5960 MB, warp points (8-14 ms GPU) 5923 MB (pc2
     phase A and #140, 13 vs 21 points). VRAM pressure cannot produce the 30 vs 8-10 ms spread; scene density does
     (render-architecture.md: the meshlet front-end scales ~11x).
   - The engine does not drive residency per frame: MakeResident 30-37 objects and Evict 62-65 objects **per 8-minute run** on
     the PC (boot/level-load only), 0 per second in the dwells. VidMm only demotes when a process is over budget; pc2 is not.
   - The texture page pool is fixed-size, not budget-driven (PC: 37,465 x 64 KB pages = 2,342 MB at HIGH), occupancy 3-43 % in
     the dwells, last-frame page misses 0-15, queue-full 0. The engine's only pressure reaction (a mip bias that grows while
     occupancy > 85 %) never engages on the PC; pc2's pool numbers come with the queued run.
2. **pc2's 1.19 GB non-local = the engine's UPLOAD/READBACK heaps, not demoted textures.** Census (PC, same allocations on
   every PC): one **700 MB UPLOAD ring heap** + one **100 MB READBACK heap** created at boot (`GpuMemCreateHeapAndDesc 1A6103E
   <- 2F03387 <- 2F0269E`), 10 x 4 MB device-init upload heaps (`2F036DE <- 2EE9855/2EE9896 <- 2EE3537`), plus transient
   committed upload buffers (dedicated texture staging `2EF2CEE <- 9C3023 <- 62D7EC`, up to 78 MB each). 840 MB of heaps +
   0.1-0.35 GB committed staging = pc2's 1.04-1.21 GB non-local. On the PC the same heaps are counted as LOCAL (non-local stays
   0.16-0.19 GB even after they are created): with ReBAR/SAM the driver backs CPU-visible heaps with VRAM.
3. **H1 (GPU reads of that memory over PCIe explain the pc2 anomaly): OPEN, now the lead** (SCHED1 #842 falsified the async-VCLOUD
   explanation). What the GPU reads from these heaps each frame is small in bytes - see section 4 - so a bandwidth effect is
   ruled out (< 0.05 ms at x8); only a latency effect on dependent reads can produce ms. The decisive data is the bind census
   (1.7.50, section 5) on pc2 and the PC write-back A/B (section 6).

## 1. Data (MemStats lines, all captures)

| | PC (RX 9070 16 GB, ReBAR) | pc2 (RX 6650 XT 8 GB, PCIe 4.0 x8) | Ally (Z1E, UMA, Proton) |
|---|---|---|---|
| adapter (DXGI) | dedicated 16,189 / shared 15,940 MB | dedicated 8,147 / shared 16,346 MB | dedicated 10,685 / shared 5,342 MB |
| local budget | 15,417 MB | 7,379 at boot, 6,898-6,954 in game | 10,685 MB |
| local used, gameplay | 8.0-8.6 GB at XSYS1 HIGH; 8.2-10.0 GB at MAX | 5.5-6.4 GB (84-92 % of budget) | 5.5-6.9 GB |
| non-local used | 0.16-0.19 GB | **1.04-1.21 GB** (from the first sample, local only 2.6-3.8 GB then) | 0.92-1.0 GB |
| over budget? | never | never | n/a (one pool; swap bursts at loads, stutter.md) |
| heavy vs light window VRAM | - | 5,960 vs 5,923 MB | - |

## 2. How the engine sizes and budgets memory (RE, v2.02)

- **DXGI budget is read in exactly three places, none of them a pool size:**
  1. `GfxInitQueryAdapterMemory 2EEBCCC` (device init, from 2EE32CB): QueryVideoMemoryInfo(LOCAL) then
     `SetVideoMemoryReservation(0, LOCAL, min(request, AvailableForReservation))`.
  2. `GfxSettingsTooHeavy 1FE3D38 = 1FE3C48 || 1FE3CD4` (the boot "settings may be unstable, reset?" dialog). 1FE3CD4 reads
     the budget through `22CECFC` (NvAPI on 0x10DE, DXGI on 0x1002): >= 10,000 MB never; <= 6,000 MB fires above 2048x1152;
     <= 7,000 MB above 3840x1080; <= 8,500 MB above 5120x1440. At 1920x1080 it can never fire (pc2's dialog #790 comes from
     1FE3C48, the settings-above-recommended score; posted #795).
  3. The options-menu VRAM meter (`283D7B7`): usage/budget >= 0.9 -> warning text.
- **Texture page pool** (KTGLGraph cache, object `[45BA588]`, pool +61A0h): size = (P+40ECh - P+40F0h) x P+40CCh pages of
  64 KB, copied from the device create parameters (`5D5964`: +C68h.. -> +40CCh..), not from the budget. Request budget
  `+7560h` (PC 28,956 pages = 1,810 MB), per-frame cap `+75A8h` = 128 (set at 28A930D), <= 16 tiles per texture per frame.
- **Pressure reaction** (`PagePoolOccupancyCheck 126C64`, decompiled): occupancy = (used +6CD0h - reserved) / size; a
  120-sample time window of "> 85 %"; while the window says over, every 0.3 x window it raises a streaming bias (t+3C0h) by
  0.1, cap 10 - t+3BCh; it steps back by 0.1 when no longer over, re-armed by a camera move > 100 units / 30 degrees. This
  is the only budget-like feedback; it is local to the page pool and never sees DXGI.
- **Residency calls:** `GpuMemMakeResident 2F118CC` / `GpuMemEvict 2F11614` are thin wrappers (callers in the page cache
  491D70 and the texture block allocator 127AEC/128294); `GpuMemCreateHeapWithPriority 2F1165C` sets SetResidencyPriority.
  Measured counts: section 0.1.
- **Upload path:** every CPU->GPU upload goes through `UploadHeapSuballoc 62D688` (placed resource in the 700 MB ring,
  [dev+4678h]); a full ring or a too-big request falls back to a committed buffer (`TextureStagingAllocDedicated 9C2EE0`).
  `MeshletUploadBuffers 4DB6C` / `MeshletUploadCopy 4DD4C` memcpy the meshlet scene data into the ring each frame
  (double-buffered by the parity at +16A0B8).

## 3. VS probe (ASI 1.7.48-1.7.50, `[Profiler] MemStats = 2`, default 0)

- `stats vs:` every second: VRAM local/non-local used/budget and MB/s, the page pool (size, used, reserved, occupancy, "OVER85"),
  request pool, last-frame touches/hits/misses/queue-full, and D3D12 MakeResident/Evict/CreateHeap/CreateCommittedResource
  counts and MB per memory pool (VRAM vs system), plus totals since the hook.
- `stats vs callers` every 30 s: system-memory heaps (H), committed (C) and placed-in-system-heap (P) allocations by the
  first three return addresses inside Nioh3.exe (lower case = READBACK).
- `stats vs binds` every 30 s (1.7.50, needs GPUProf = 2): root CBV/SRV/UAV (graphics and compute), IASetIndexBuffer and
  IASetVertexBuffers whose GPU address lies in a system-memory heap or buffer, per kind and per engine marker.
- Hooks: device vtable slots 27/28/29/30/34/35 (installed from a D3D12CreateDevice IAT wrapper set during the decrypt wait, so
  boot heaps are seen) and gpuprof's command-list slots 37-44. `VSForceSysUpload = 1/2` (PC A/B only): UPLOAD -> CUSTOM L0
  write-combine / write-back.
- Tools: `tools/vs_capture.ps1` (matched tour capture, `-GP2`, `-AddKeys`, `-Tag`), `re/scripts_gpu/vs_digest.py`
  (per tour point: VRAM, churn, pool occupancy, misses, residency, PresentMon GPUBusy).

## 4. What lives in system memory and how much the GPU reads per frame (PC census, 8 min, XSYS1 spots)

Cumulative placed bytes in the system heaps (ring suballocations, so MB/s, not residency):

| allocator (rva0 <- rva1 <- rva2) | what | 8 min | per frame at ~150 fps |
|---|---|---|---|
| 62C9CF <- 62C6F1 <- 62C642 / 62DA5B | streaming staging (texture/meshlet page loads), up to 111 MB | 3.6 GB | copy source, read once by a copy |
| 62D78B <- 6305F5 / 630CAC / 62EED4 / 62FEBE | GpuMemUploadTexture* staging | 1.5 GB | copy source |
| 62D78B <- 4FF07 (GpuBufferMap) | dynamic buffers mapped by the CPU each frame | 0.8 GB | ~11 KB |
| 62D78B <- 62D000 (GpuIndexBufferMap) | UI index/vertex buffers | 0.21 GB | ~3 KB |
| 62D78B <- 4E2C0 <- 4D943 | meshlet scene upload | 0.39 GB | ~5 KB |
| 62D78B <- 4E2C0 <- 62E8D2 (READBACK) | meshlet streaming feedback | 2.2 GB written by the GPU | ~30 KB |

The per-frame GPU-read volume of CPU-written data is tens of KB, i.e. **< 0.01 ms of PCIe 4.0 x8 bandwidth** even if every
byte were re-read 10x. Texture/meshlet staging is read once by a copy (that is the normal path on every PC without ReBAR).
So if H1 holds, it holds through latency: kernels/draws that do dependent reads from upload memory (root CBV/SRV indirection,
per-instance data looked up per thread) at ~1-2 us per PCIe round trip.

## 5. PC A/B: forcing the upload heaps into system memory (VSForceSysUpload = 1)

All UPLOAD heaps (incl. the 700 MB ring) and committed upload buffers created as CUSTOM {WRITE_COMBINE, L0} from the boot hook
(forced 61+ allocations): GP2 per family and per shader identical to the base run (every anomaly 0.96-1.07, same-device
noise p90 x1.23), VRAM local/non-local unchanged (8.2-8.6 GB / 0.19 GB). The driver keeps write-combined CPU-visible heaps in
BAR-mapped VRAM even when L0 is requested, so this arm does **not** emulate pc2: inconclusive. The write-back arm (= 2) is next
(section 6).

## 6. Pending (queued)

- PC: bind census run (1.7.50, `vs_capture.ps1 -GP2 -Tag binds`) and the write-back arm (`-AddKeys Profiler.VSForceSysUpload=2`).
- pc2 (#787/#796): VS1 base on 1.7.50 (pool size at 8 GB, census, binds, ReBAR line) with GP2 MinRun 1.
- Ally (#788): MemStats = 2 in the XSYS1 rerun (UMA case).

## 7. Fix direction (only if H1 is proven)

- Keep the GPU-read dynamic buffers in VRAM on cards without ReBAR: for the allocators the bind census flags (expected:
  GpuBufferMap 4FF07 users and the meshlet upload 4E2C0), allocate a DEFAULT-heap twin and record one CopyBufferRegion
  UPLOAD -> DEFAULT per frame before the first reader (the bytes are tens of KB, so the copy is ~free), then redirect the
  root-descriptor / VB / IB / CBV addresses to the twin. Active only on a discrete adapter whose UPLOAD heaps are non-local
  (DXGI non-local usage > 0.5 GB or OPTIONS16 GPUUploadHeapSupported = 0). No-op on the PC (ReBAR) and the Ally (UMA).
- Expected gain: up to the measured excess of the flagged passes on pc2 (cross-system-compare.md: ~7.7 ms of 31.6 ms if all of
  it is PCIe latency; realistically 3-7 ms). Zero gain if the bind census shows the anomalous markers bind no system memory.
- Not a fix: SetResidencyPriority (does not move a heap type), budget tweaks, smaller texture pool (changes the look, and
  VRAM is not short).
- User-side note (user-settings.md, no system changes by us): Resizable BAR / SAM on in the BIOS/driver puts these heaps in
  VRAM on AMD; that is the zero-code equivalent of the fix where the board supports it.
