# Cross-system GPU comparison: PC (RDNA4) / pc2 (RDNA2) / Ally (RDNA3, RADV) — 2026-10-09

User idea (#676): compare the per-pass GPU workload across the three machines, find passes whose relative cost differs far
more than raw GPU speed explains, and decompile around them. Tools: `tools/xsys_compare.py` (analysis),
`tools/xsys_capture.ps1` (matched capture, Windows), `tools/tour/tourlist_xsys.txt`, `configs/xsys-1080-high-fsr65.xml`.
Raw outputs: `captures/xsys/` (local; zips on the bridge #144 PC, #145 pc2), tables `captures/xsys/xsys1_*.txt`.

## 0. Verdict

- **One big anomaly, on pc2 only.** With identical settings, spots and shaders, pc2 (RX 6650 XT, Windows) spends
  **~7-8 ms of its 31.6 ms frame (~24 %) in passes that are 9-100x slower than on the RX 9070**, while everything else is
  2.5-3.8x slower (= raw speed). The affected passes are the meshlet front-end kernels (MIC, MshLod #1/#2, some MPC) and
  low-shading draws that read per-frame CPU-written data (eye/STD/PB2 small props, shadow casters, UI quads, particles).
  An eye-shader draw at 286 us, an 8-byte-per-thread copy kernel at ~1 ms: that is not shading or wave/occupancy cost.
- **Leading cause (H1, not yet proven): GPU reads from system memory over PCIe on pc2.** pc2 runs with **1.19 GB of
  non-local (system-memory) GPU allocations from the first minute, with local VRAM only half used (3.8/7.4 GB)**; the PC at
  the same settings has 0.19 GB. The game queries only D3D12_OPTIONS..OPTIONS7 / feature levels / shader model / format
  support (decompiled `GfxInitCheckFeatureSupport*` 2F04FB0 / 2F052A0 / 2F056B0 / 2F05730): no ARCHITECTURE/UMA or
  OPTIONS16 (GPU-upload heap) query, so its heap choice is the same on both PCs. The difference therefore comes from where
  Windows/the driver places CPU-visible (UPLOAD-type) heaps: in VRAM through the resizable BAR on the PC (SAM), in system RAM
  on pc2 if SAM/ReBAR is off. The 6650 XT is PCIe 4.0 **x8**: every GPU read of those buffers is a latency-bound PCIe read.
  This explains exactly the pattern above (kernels and draws that read per-frame CPU data explode; full-screen passes over
  VRAM render targets scale normally) and why the Ally (UMA, no PCIe) and the 9070 (SAM) do not show it.
- **RDNA4 (PC): no anomaly.** No pass is relatively more expensive than on RDNA2 beyond the noise floor (largest: PB2UMAH water
  0.065 ms at 0.5 draws/frame).
- **Ally (RADV): no matched data yet** (XSYS1 tour could not arm, §5). Against its older USER-preset data (FSR3 accumulate as
  the speed reference, x2.42), the only flagged pass is a bandwidth pass (PB2DUS, §3.3), explained by LPDDR5 without Infinity
  Cache. No vkd3d/RADV "explosion" is visible in the existing data; FSR3 runs at exactly raw speed.
- Fixes: nothing ships from this yet. H1 has a zero-code confirmation test and a mod-side fix design (§4). If H1 holds, it is
  worth **~3-7 ms on pc2 (10-20 % fps)**, 0 on the PC and the Ally.

## 1. Data and method

| | PC | pc2 | Ally |
|---|---|---|---|
| GPU | RX 9070 (RDNA4, gfx1201), Win D3D12 | RX 6650 XT (RDNA2, gfx1032, 8 GB, PCIe 4.0 x8), Win D3D12 | Z1 Extreme 780M (RDNA3 gfx1103), Proton/vkd3d-proton/RADV, 25 W |
| matched XSYS1 | **yes** (06:50, #144) | **yes** (06:16, #145) | **no** (tour never armed, #745) |
| older data | RGP r5 trace (1 frame, shrine, 1080p render, FSR4) | GP2 #125/#126 (MinRun 4), #140 (MinRun 1) | GP2 S1 USER preset (LIGHTEST+VOL STD, REN 50, shrine), 175-L2 MAX |

**XSYS1 spec** (identical on all three): `configs/xsys-1080-high-fsr65.xml` (1920x1080, custom HIGH, REN_RAT 65 = 1248x702,
in-game FSR 3.1, FG off, VSync off), tour 7356 / 52 / 7292 / 5631 **spawn point only** (the warp points sit outside the level),
2 passes x 30 s dwell, GP2 MinRun 1, Top 150 (ASI clamps to 100), SampleEvery 16, AB 0, `[VRR] MaxFps 240`, PresentMon,
shaders: ghdm replaced (shipped default), every other item original (both PCs verified by the wrapper).
- The PC's swapchain is 2560x1440 (borderless takes the desktop size), but the render resolution is the same: the fixed-size
  ESM pass (2048^2) is x3.8 faster on the PC and the render-res passes x3.4-4.6 (SDFOCC, WDO#14, GBufferEdit copy), so the
  pixel work matches; only the final output copy / UI run at 1440p on the PC.

**Normalisation (`xsys_compare.py`).** Rows are keyed by the shader DXBC digests (identical binaries on every system; the RGP
loader maps PSO -> digests from the trace). Per shader: ms per draw/dispatch (removes draw-count differences), divided by the
system pair's **speed factor** (median of the shared shaders, or `--ref <digest>`; FSR3 ACCUMULATE_SHARPEN e7386840 is the best
fixed-work reference when both run FSR3 at the same output). anomaly = 1 means "scales like the GPU". Families: per frame.
**Noise floor:** same device, same point, pass 1 vs pass 2: per-shader p90 x1.35 (pc2) / x1.20 (PC) at MinRun 1. Comparing a
MinRun 4 run with a MinRun 1 run of the same pc2 tour gives single rows up to x3 (#126 vs #140): **never mix MinRun values**,
and treat unmatched-spot single rows below ~x3 as unproven. The strong flag threshold is max(2, p90 noise).

## 2. Anomaly table (matched XSYS1, PC vs pc2; speed factor PC = x0.40 per draw, ESM fixed-work x0.26)

| pass (digest) | pc2 ms/frame (draws) | PC ms/frame (draws) | raw ratio | anomaly vs speed | reading |
|---|---|---|---|---|---|
| **meshlet front-end family** | **6.73 (21.3 %)** | **0.58 (6.8 %)** | 11.6x | **4.6x** | RDNA2-only |
| MIC instance culling e31c389f | 1.256 (3.7) | 0.029 (3.5) | 43x | 11x | |
| MshLod#2 per-cluster LOD 63c07c21 (levels 0-3) | 1.500 (4.4) | 0.015 (0.8) | ~100x/frame | ~40x/frame | |
| MshLod#1 root-list init e6a62bda | 1.661 (1.3) | < top-100 cut; **RGP r5: 7.3 us** for 3437 groups | > 100x | > 40x | an 8-byte copy kernel |
| MPC 18b6fee7 / a09d4223 / cc40b6f0 | 0.49 / 0.18 / 0.12 | 0.058 / 0.015 / 0.013 | 8-12x | 2.3-3.4x | |
| MPC#27 1b4b5b75 | 0.76 | 0.34 | 2.2x | 0.9x | normal |
| PB2EYEC eyes 9bf370df/a4ab1343 | 0.451 (1.9) | 0.018 (2.0) | 25x | 6.6x | 2 draws, 286 us each on pc2 |
| STD small props 96690c3b/d1f13d22 | 0.427 (128, **2 intervals**) | 0.024 (127) | 18x | 4.8x | |
| UI quads AMBSL 0ad688da/ff5fcc12 | 0.223 (51) | 0.017 (51) | 13x | 3.4x | dynamic UI vertices |
| particles G1E f645c461/d4ed3fe4, f32bf54a | 0.15 / 0.11 | 0.011 / 0.017 | 13x / 6x | 2.6-3.4x | dynamic particle vertices |
| shadow casters SM-Sfb e5190e82/c4656a8d | 0.909 (88) | 0.079 (72) | 11.5x | 2.6x | |
| PB2 small props 2c6f629c/d2fe68c9, DEPTH-Sfb 2ace2070 | 0.557 / 0.213 | 0.060 / 0.031 | 9x / 7x | 2.5x / 1.9x | |
| VCLOUD b76874bb **async** | 2.91 | 0.49 | 5.9x | 2.3x | async wall time (contention) |
| VFSCAT 641524ee **async** | 1.10 | 0.19 | 5.8x | 2.3x | async wall time |
| everything else (G-buffer materials, deferred, GI, FSR3, post, ESM, copies) | | | 2.2-4.6x | 0.5-1.9x | = raw speed |

Excess on pc2 (time above "scales like the GPU"): meshlet family ~5.3 ms + small-draw/UI/particle/shadow rows ~2.4 ms =
**~7.7 ms of 31.6 ms**; async contention another ~2.4 ms of async wall (overlapped, frame impact unknown).

## 3. Causes and decompile findings

### 3.1 pc2: meshlet front-end + per-frame-data draws (H1 = system-memory reads over PCIe)
Evidence for H1:
- Memory placement (MemStats, same config): pc2 `VRAM local 3801/7379 MB, non-local 1186 MB` one minute after launch,
  `5822/6901, non-local 1187` in the field; PC `8139/15417, non-local 189`. The ~1 GB is not budget overflow (local is half
  used when it appears) but a fixed placement decision.
- The exploding rows are exactly the consumers of CPU-written per-frame data: instance/LOD root lists (MIC, MshLod#1/#2),
  per-draw data of many small draws (STD 128 draws billed in 2 intervals, eyes, small PB2 props), UI and particle vertices.
  Full-screen passes that read only VRAM render targets scale like the GPU (anomaly 0.5-1.9).
- MshLod#1 is a trivial copy (meshlet-frontend.md §0: one thread per root, 2 loads, 2 stores). RGP on the RX 9070 measures it at
  7.3 us in its own barrier-isolated segment with no idle before it. ~1 ms on pc2 means latency-bound reads, not work.
- Neither the Ally (UMA: system memory is its VRAM, no PCIe) nor the 9070 (SAM) show it.
- The game does not query GPU-upload heap support or UMA (decompiled feature checks above), so the placement difference is
  the driver's/OS's (ReBAR/SAM state), not a game code path.
Not yet ruled out (each predicts a different fix):
- **H2 layout transitions billed to the next draw** (DCC/HTILE decompress on RDNA2 where RDNA3/4 keep compression for shader
  reads). GP2 intervals are within one command list and bill an in-list barrier to the following PSO, so a decompress lands on
  "eyes" or "MIC". H2 does not explain the UI/particle rows or the buffer-only MshLod kernels; H1 explains all of them.
- **H3 in-list waits** (a kernel waiting on a fence/copy-queue result). The meshlet agent's Phase M probe (per-dispatch vs
  barrier split) separates H3 from H1/H2 for the front-end.

### 3.2 Async volumetrics on pc2 (x2.3 relative wall time)
VCLOUD / VFSCAT wall times are 2.3x more inflated on pc2 than on the PC. This is the contention question of
render-architecture.md F3 / Q1, owned by the scheduling/sync agent; not duplicated here. Note for them: if H1 holds, part of the
contention may be the same PCIe traffic (the async kernels run beside the front-end).

### 3.3 Ally: PB2DUS 6f7e3409 (unmatched data, x2.84 vs the FSR3 reference)
DXIL (`0x618b07e7.file` record 1, dumped with dxc): 6 `sampleLevel` from 5 G-buffer SRVs (one a 2D array) -> 3 MRTs, no loop, no
ALU to speak of. A pure bandwidth pass at render resolution. The 780M has no Infinity Cache (2 MB L2) and shares LPDDR5 with the
CPU; the 6650 XT keeps a 702p G-buffer in its 32 MB IC. Expected 2-3x relative cost, so explained. Same-look fix: fuse into its
neighbour (engine-level), ~0.2 ms Ally, not worth it now. VFSCAT on the Ally (x1.6 per froxel vs pc2) is the known
bandwidth/occupancy case (ally-gpu-budget.md §6 F). FSR3 on the Ally costs exactly the raw-speed ratio (x2.42 accumulate).

### 3.4 Things that are NOT anomalous (checked)
Wave32/64, LDS and occupancy differences would show as 1.5-3x on ALU-heavy passes (RefProbe GI 24.7k-instruction CS, deferred
lighting, FSR3, VFSCAT): all sit at 0.6-1.9 across RDNA2 and RDNA4. Tessellated terrain (PB2LS) 0.57. No pass shows a
driver-compiler outlier between the two Windows drivers.

## 4. Fix candidates (estimated ms per system)

| # | fix | pc2 | PC 9070 | Ally | status |
|---|---|---|---|---|---|
| 1a | **Confirm H1 (zero code):** pc2 reports its SAM/ReBAR state (Adrenalin > Performance > Tuning, or GPU-Z "Resizable BAR"); XSYS1 rerun with `Profiler.GQ=1` added. Optional, user's call (system setting, diagnostic only): one XSYS1 with SAM on. | - | - | - | queued (§6) |
| 1b | **Mod: keep hot per-frame GPU-read buffers in VRAM.** Hook the GpuMem create paths (`GpuMemCreateCommittedResource*` 62C4DC.., `UploadHeapSuballoc_CreatePlaced` 62D784) to tag the buffers the front-end / per-draw data read; create a DEFAULT-heap twin and copy UPLOAD -> DEFAULT once per frame on the DIRECT queue before the first reader (the CPU keeps writing the UPLOAD one). Only active when the adapter is discrete and the allocation is non-local (no-op on the Ally and with SAM). | **3-7 ms** (if H1) | 0 | 0 | design; needs H1 proof + a resource census probe (which heap each hot buffer lives in) |
| 1c | ID3D12Device1::SetResidencyPriority(MAXIMUM) on the same buffers | 0 if they are UPLOAD heaps (priority does not move a heap type), small if they are demoted DEFAULT buffers | 0 | 0 | only if the census shows DEFAULT buffers in non-local |
| 2 | async volumetrics: per-vendor sync/async (AS row) | 0-1.5 (unknown sign) | ~0 | ? | scheduling agent, Q1 |
| 3 | meshlet front-end restructure (meshlet-frontend.md) | 2-3.5 (their estimate) | 0.3-0.5 | 1-3 | other agent; **if H1 holds, most of pc2's 6.7 ms is memory placement, not structure: re-rank after 1a** |
| 4 | Ally PB2DUS fusion | 0 | 0 | ~0.2 | not worth it |

Honest limit: 1b is a medium-large mod feature (resource tagging + per-frame copies + hazard tracking). It is only worth
building after 1a shows the hot buffers are non-local. If pc2 turns out to run with SAM on, H1 is wrong and H2/H3 (§3.1) are next.

## 5. The Ally's XSYS1 (failed) and the fix
The tour never arms after a title **Load Game** on the Ally: `FtState()` / `WarpBState()` read `1148547920` / `1148229280`
(floats, `0x4475E550`) from the save load onward, so `StableField()` never holds (#745, #748; earlier runs only armed when the
5 s window happened before the flip). The two globals (`g_FastTravelSeq` 4757470, `g_WarpSeqB` 47098A0) point at reused
memory after that load. **Fix committed (a4908ba, `tour.cpp`):** a sequencer read outside 0..63 is not a state, so it is treated
as idle and logged once; the player / 58A load byte / mission queue / pause gates still block real loads and warps. Compiles
(cl /Zs); **needs the next ASI build**, then the Ally reruns XSYS1 exactly as specified (#711). No-build fallback if the build is
far off: the matched config at the Ally's Load Game spot, Tour 0, 4 x 30 s GP2 windows standing still; `xsys_compare.py`
then uses the unlabeled windows (`--min-draws`) and only per-shader anomalies (not spot-dependent families) count.

## 6. Next tests
1. **pc2** (after its autosave-notice block, #756): SAM/ReBAR state (read-only check); XSYS1 rerun with `Profiler.GQ=1` added to
   `-ExtraKeys`; optional RGP capture at 7292 spawn (barrier types: H2).
2. **Ally**: XSYS1 on the first ASI build containing a4908ba.
3. **PC**: none needed; rerun XSYS1 only together with the others if the ASI changes GP2.
4. Compare: `py -I tools\xsys_compare.py --in pc2=<pc2 run01.log> --in pc=<pc run01.log> --in ally=<ally log> --pm pc2=... --pm pc=...`
   (add `--ref e7386840` when all run FSR3 at the same output, `--min-ms 0.01` to see the PC's small rows).
