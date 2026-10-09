# pc2 (RX 6650 XT, RDNA2, no ReBAR): why the meshlet front-end kernels are 40-100x slower per call - 2026-10-09

Question: on pc2 the meshlet front-end kernels run 40-100x slower per call than on the PC (RX 9070), while raw GPU speed
explains 2.5-3.8x (MshLod#2 1.50 vs 0.015 ms/frame; MshLod#1, an 8-byte-per-thread copy, ~1 ms/dispatch vs 7.3 us in RGP r5).
Leading hypothesis handed in: barriers (UAV barrier / transition = full drain + cache flush on RDNA2, billed by GP2 to the next
stamped dispatch). Inputs: pc2 GP2 MinRun 1 logs #140, XSYS1 #145, CM arms #170 (1.7.54, spawn 7356), SCHED1 #155, VS1 #158;
the PC VS-probe tour runs `captures/vs/pc-20261009-*` (base, forcesys 1, wb = VSForceSysUpload 2, wbcm = wb + CbvMirror 2);
static: capstone over `re/Nioh3.dump.exe` (no Ghidra: the PC game was running). Tags: **M** measured, **S** static, **E** estimate.

## 0. Verdict

1. **Barriers are not the cause. M + S.** On pc2 a dependent one-wave meshlet stage *including its UAV barriers* costs ~7.6 us
   (MPC #0/#26/#28-#31: ~0.16 ms over ~21 stage-intervals per frame, meshlet-frontend.md §0); LOD levels 4..13 cost ~5.5 us per
   stage. That is ~2x the RX 9070 harness (3.7 us/stage) = raw-speed scale. Barriers cost microseconds on RDNA2 here, not the
   ~1 ms the 40-100x rows show. The blow-up scales with the **work in the kernel** (LOD level 0 >> level 1 >> level 2, MIC with
   the instance count), not with the number of barriers. Barrier batching/narrowing ceiling in the front-end: **<= ~0.3 ms
   (~1 %) on pc2 - below the 5 % bar.** No barrier fix built.
2. **The barrier chain is already narrow (S, section 2):** every 0x8A token names one MeshletSys buffer (`[MeshletSys+16A1xxh]+18h`),
   no NULL-resource UAV barriers, no aliasing barriers, no transitions inside the recorders (EI argument transitions happen at
   bind time in 115E24 with the same-state filter). The only redundancy is back-to-back UAV barriers (same or different
   buffer, no command between): gpuopt's batching + DropDuplicateUav (`[GpuOpt] Barriers = 1`, queued #769) already removes
   those at runtime. Enhanced barriers would not change the RDNA2 cost (the drain is real dependency).
3. **The data points at memory placement, and specifically at the compute constants, which no test so far has moved. M.**
   - PC, same barriers, only the UPLOAD heaps moved to system memory (`wb`, VSForceSysUpload = 2): MIC 8.3 -> 17.5 us/dispatch,
     MshLod#1 ~7 -> 35.5 us/dispatch (it enters the top-100 at all), MshLod#2 L0 20.7 -> 27.2. Same with `wbcm` (per-draw CBVs
     mirrored back to VRAM): MIC 17.4, MshLod#1 35.2. So these kernels read something in the UPLOAD heap that is not a per-draw
     CBV. (VSForceSysUpload = 1 = write-combined L0 keeps the heaps in BAR VRAM: unchanged, as expected.)
   - **CbvMirror (#988, #1030) only ever redirected the per-draw graphics site 118F89.** The VS-probe callers table (PC wbcm)
     shows the compute dispatch constants come from two other sites that stayed in system memory in every CM arm:
     `V 495B2E <- 49545C <- 58BEE3` (CmdListUploadComputeCbvTable 4959EC <- SynFlushComputeState 58BE90; 10.8 M CBVs per run, the
     bulk) and `V 117F20 <- 119CB4 <- 119BDC` (SynFlushComputeDescriptors 117DE8 <- SynFlushComputeStateEx 119C08). So "the meshlet
     kernels did not change when CBVs moved to VRAM" was never a test of their constants: the root-cause-vram-streaming.md §0.2
     "not CBV-bound" conclusion for MIC / MshLod#1 / MPC is unproven.
   - Consistent with it: async VCLOUD / VFSCAT (compute, same CBV path) are 2.3x relatively slower on pc2 and VCLOUD's wall went
     2.6 -> 15.2 ms when CM's CP-DMA copies loaded PCIe (#1030 C): compute kernels on pc2 are PCIe-read-sensitive. The MshLod#1 vs
     VCLOUD correlation (r = 0.88, gpu-scheduling-sync.md §1.2) fits two kernels sharing one slow PCIe read path better than CU
     contention (SCHED1 #842: VCLOUD on DIRECT was +0.32 ms, so the overlap itself is not the loss).
   - Not yet excluded: buffer SRVs the kernels read from the UPLOAD heap (S descriptors 4.6 % of created SRVs; the VS-probe
     callers table lists only `915B0B <- 4EC3B` with 343 creations). The meshlet-system UPLOAD placements in the pc2 callers table
     (`62D78B <- 4FF07 <- 525B9B`, 59 x <= 3.3 MB, READBACK twins) are asset-load staging (525B00: create, map, stream read, unmap),
     not per-frame hot reads.
4. **Size of the prize (E):** MIC + MshLod#1 + MshLod#2 + MPC#20 on pc2 dense = ~4-5 ms of ~31 ms (#140/#145/#170 rows: MIC 1.0-1.7,
   MshLod#1 1.4-1.7, MshLod#2 L0-2 1.0-1.5, MPC#20 0.4-0.5) against ~0.15 ms for the same rows on the PC. If the compute constants
   are the cause, CMC recovers most of it: **~3-4 ms = 10-13 % fps on no-ReBAR RDNA2**, and the PC / Ally gain nothing (CM auto
   stays off with ReBAR / UMA / Wine). If not, the ceiling of everything barrier-related is ~1 %, and only ReBAR (user setting)
   or a buffer-level mirror of the kernels' SRVs remain.

## 1. Per-dispatch rows (dense windows, >= 1500 draws; `rows.py` below)

| run | span ms | MIC#0 us/disp | MshLod#1 us/disp | MshLod#2 L0 | MPC#20 | VCLOUD (async) |
|---|---|---|---|---|---|---|
| PC base (ReBAR) | 8.89 | 8.3 | ~7 (RGP r5; below top-100) | 20.7 | 23.2 | 494 |
| PC forcesys (= 1, WC, stays in BAR) | 8.54-9.15 | 8.3-8.4 | - | 20.7 | 23 | 485-510 |
| PC wb (= 2, sysmem layout) | 12.76 | 17.5 | 35.5 | 27.2 | 26.0 | 582 |
| PC wbcm (wb + per-draw CM) | 10.68 | 17.4 | 35.2 | 25.6 | 25.2 | 482 |
| pc2 #140 (13 windows) | 31.41 | 300 | 1009 | 633 | 194 | 2772 |
| pc2 XSYS1 #145 | 31.60 | 344 | 1117 | 771 | 199 | 2910 |
| pc2 #170 A (1 window) | 32.56 | 713 | 941 | 1238 | 165 | 2594 |
| pc2 #170 C (per-draw CM) | 42.79 | 702 | 873 | 651 | 137 | 15174 |

GP2 semantics (gpuprof.cpp Gp2Event): the stamp sits right before the first Dispatch/Draw of a new PSO, so an interval =
that dispatch + the barriers recorded after it + any unstamped commands up to the next PSO's first dispatch. Barriers recorded
before a dispatch belong to the previous interval. Cross-queue waits cannot hide in an interval (lists split at every wait).

## 2. Barrier inventory of the meshlet recorders (S; static linear order, branches not resolved)

Token stream: 0x7E Dispatch, 0x81 DispatchIndirect, 0x8A UAV barrier (ID3D12Resource* = `[buf+18h]`), executed by
SynTokUavBarrier 113A0C as `ResourceBarrier(1, UAV{res})`. Script: `barriers.py` below.

| recorder | dispatches | UAV barrier tokens | buffers barriered (MeshletSys offsets) |
|---|---|---|---|
| MeshletResolveLOD 531948 | 9 D + 1 DI | 15 | +16A1A0 (payload ping-pong) x9, +16A160 (codes) x3, +16A1A8 / +16A1B0 (level args / counters) x3 |
| MeshletPreculling 5301FC | 5 D + 3 DI | 40 | +16A178 / +16A188 (draw args / offsets, alternating x8 in the #28-#31 tail), +16A170 x7, +16A1C0 x3, +16A198 x2, +16A140 x2, +16A168 x2, +16A1A0 x2, +16A180, +16A190 |
| MeshletInstanceCulling 53FD3C | 2 D | 1 | +16A160 |

- Types: UAV only; no NULL resource (no "flush everything" barrier); transitions (UAV <-> INDIRECT_ARGUMENT for EI argument
  buffers, SRV at bind) happen in the dispatcher's tracked path (115E24, 6EB908) with the same-state filter (gpu-barriers.md §3).
- Redundant candidates: consecutive tokens on the same buffer with no dispatch between (+16A1A0 x4, +16A140 x2, +16A168 x2,
  +16A170 x2 in the static order; some sit on alternative branches) and the per-stage pairs on different buffers. Both are what
  `[GpuOpt] Barriers = 1` (batch + DropDuplicateUav) removes at runtime; #769 measures it on pc2.
- Overly broad: none. Narrowing is not possible (already per-resource); splitting the per-stage barrier sets would not help a
  dependent chain.

## 3. Built: CMC = compute-constant mirror (ASI 1.7.55, main 09a0da1, bridge file #172)

`[GpuOpt] CbvMirrorSites` (upmirror.cpp), bitmask, ignored with CbvMirrorAllSites = 1: **1 = per-draw 118F89 (default = the
1.7.52-1.7.54 behaviour)**, 2 = only the compute dispatch constants (return addresses 495B2E and 117F20), 3 = both. Needs
`CbvMirror = 1` (auto: only when the UPLOAD heaps are in system memory, never UMA/Wine) or 2 (force, test). Everything else is
the 1.7.54 inline CM path (prologue copy list in the same ExecuteCommandLists, COPY_DEST -> constant-buffer barrier, compute
queues re-copy with `CbvMirrorComputeCopy = 1`, which the async clouds/fog need).
- Correctness (S): both sites memcpy the constants (B94250) into a fresh ring slot immediately before
  `CreateConstantBufferView` (device vtbl+88h) - the "bytes final at create" assumption A1 of CM holds as it does at 118F89.
  A different exe never matches the return addresses (no-op). Default 1 leaves every existing setup unchanged.
- Volume: ~250 CBVs/frame, avg < 1 KB -> ~0.2 MB/frame, vs ~7-8 MB/frame in the per-draw arm that cost +10.7 ms (#1030 C).
- Build: build.bat clean (only the pre-existing C4456 in dllmain), `tests/test_patterns.py`: ALL 243 SITES OK.
- Not run in the game yet: the PC was busy (game running, not ours) when built.

## 4. Tests queued (pc2)

1. **#692 Phase M** (MeshletProbe, ASI 1.7.47, queued by the coordinator): per-dispatch vs per-barrier vs gap split, CS invocations
   per dispatch (threads -> bytes -> effective GB/s for MshLod#1). Expected: barrier column small (us per call), dispatch column
   carrying the ~1 ms. Raw run01.log + mlp_events.csv asked for (#1044).
2. **#769** barrier batching on pc2 (GPU-bound): expected <= ~0.3 ms.
3. **CMC #1051**: A D A D A at spawn 7356, A = CM off, D = `CbvMirror=1; CbvMirrorSites=2; CbvMirrorComputeCopy=1`, GP2 MinRun 1 +
   MemStats 2 + shotdiff. Decision: MIC + MshLod#1 down >= ~1 ms and GPUBusy down >= ~3 % -> the anomaly is compute constants
   over PCIe; ship `CbvMirrorSites = 2` under auto (no-ReBAR systems only). Flat -> not constants; then the kernels' own buffer
   SRVs (VS-probe S/U callers per PSO) or ReBAR (user setting) are what is left.
4. PC (when free): `VSForceSysUpload=2` vs `+ CbvMirror=2; CbvMirrorSites=2` - MIC should return to ~8 us and MshLod#1 to ~7 us if
   the constants are the whole story (cheap mechanism check; the PC's PCIe 5.0 x16 makes it small in ms).

## 5. Reproduce

- Rows: `py -I rows.py <run01.log ...>` (scratch; parses `GP2: frame` + rows for e6a62bda / e31c389f / 63c07c21 / 18b6fee7 /
  1b4b5b75 / b76874bb / 2c6f629c in windows with >= 1500 draws+dispatches). Inputs: `captures/vs/pc-20261009-1248-base`, `-1420-wb`,
  `-1429-wbcm`, `-1305-forcesys3`; pc2 bridge files #140 / #145 / #170.
- Callers: `grep "stats vs callers" captures/vs/pc-20261009-1429-wbcm/run01.log | tail -1 | tr ';' '\n' | grep "^ [VSU] "`.
- Barrier tokens: capstone over the dump, functions 531948 / 5301FC / 53FD3C, `mov dword ptr [rax], 8Ah` (or via a register)
  after `mov rX, [rbp + 16A1xxh]`.
- Compute CBV sites: `call B94250` (memcpy into the ring) then `call [rax+88h]` (CreateConstantBufferView) at 495B0x..495B28 and
  117EE6..117F1A.
