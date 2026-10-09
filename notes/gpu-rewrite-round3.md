# GPU rewrite round 3: mid-range RDNA2 (pc2, RX 6650 XT, 1080p HIGH, FSR 65) - 2026-10-09

Goal: GHDM-style same-picture shader rewrites for the passes that cost the most on a mid-range GPU (pc2 ~37 fps,
GPU-bound, mean GP2 span 23.2 ms). Static/offline work on the PC; the game was not run here (pc2 holds the lock).
Scope after the coordinator's split: **meshlet family** (MshLod / MIC / MPC + the meshlet vertex shaders) and **SM
shadows** here; volumetrics (VCLOUD, VFSCAT) moved to another agent; copy/resolve/clear to the resolution-audit agent.

## 1. Ranking on RDNA2 (pc2 GP2, file #125 / #126, stock shaders, 23 gameplay windows)

| family (pc2) | ms/frame | % of 23.2 ms | owner / status |
|---|---|---|---|
| copy / resolve / clear (+ absorbed draws) | 1.73 | 7.5 | resolution-audit agent |
| VCLOUD async CS (DrawSky) + VFSCAT async CS | 1.60 + 1.04 | 11.4 | volumetrics agent (async: frame cost < wall time) |
| **meshlet culling: MshLod (MeshletResolveLOD) + MIC + MPC** | ~3.0 (GP1 markers: ResolveLOD incl 3.24, Preculling 1.68, InstanceCulling 0.95 in the GP1 windows) | ~13 | **this note, §3** |
| FSR3 (all passes) | ~1.4 | 6 | FFX dll; R1/R2/R3 in ally-gpu-budget.md §5 |
| **meshlet VS PSOs (PB2 / PB2TREE2 / SM / SMTREE2 rows with a meshlet VS), incl. their PS** | **0.98** (PB2 0.74, TREE2 0.19, shadow 0.06) | 4.2 | **MSV built, §2** |
| SM-Sfb shadows (IA-skinned VS e5190e82, 55 draws) + Cascade markers | 0.66 (+ cascades ~0.25 each) | 2.8 | §4: no shader lever (skinning palette loads are per-vertex) |
| RefProbe #121 | 0.58 | 2.5 | gi item (DROP on RADV; PC neutral) |
| GHDM | 0.14 (#38) | 0.6 | installed item; small on RDNA2 at HIGH |

**GP2 attribution caveat (important for the meshlet rows).** GP2 runs with `GPUProf2_MinRun = 4`: an interval is cut
only after 4 draws/dispatches, so a bucket is named after its FIRST PSO and absorbs the next ones. The "MshLod
c4a71e16 0.81 + 0.45 ms" rows are **not** that kernel: c4a71e16 is a 4-store argument initialiser (129-line DXIL, no
loads). Its buckets carry the following MshLod#2 ExecuteIndirect dispatches (63c07c21: the per-cluster LOD kernel,
which itself shows 0.001 ms / 12 intervals). Likewise MPC a09d4223 has **0 VMEM loads** (RGA) and MPC 304bac05 5 - their
0.5 ms buckets are the chained MPC#27/#20 pointer chases. To rank kernels inside the meshlet family a run with
`GPUProf2_MinRun = 1` is needed: `pc_gpu_session.ps1` now has `-GP2MinRun` (default 4, unchanged behaviour).

## 2. Candidate 1 - MSV: meshlet-VS slot-chain scalarization (BUILT, proven offline, queued on pc2)

**What the shipped meshlet VS does** (all 1616 meshlet VS records share `slSetUpMeshletVertex`): draws are non-indexed,
`vid / 384` is the meshlet slot (128 triangle slots x 3 corners; `vid >> 9` on the direct path). From the slot the VS
walks a chain of ~7 **dependent** buffer loads that depend only on the slot: draw-mesh offsets t25 -> payload t24 ->
G1M offsets t17 (11 dwords) -> bindless G1M root t1[] (4 levels: header, vertex/index info offsets, palette index) ->
vertex info t21 / index info t20 / instance offsets t19 -> prim->texture t27, then 3 per-vertex loads (triangle
indices, vertex index, vertex data) and 3 matrix-palette rows (slot-uniform again). Every lane of a wave loads the
same addresses (384 consecutive vertices share a slot), but the compiler cannot know it: the whole chain is VMEM
with VGPR addresses, one round trip per level, plus a readfirstlane waterfall around every bindless handle. The
asset audit (asset-audit.md) adds: meshlet fill is 56 %, only 63.6 % of VS waves contain a triangle - the empty
slots run the full header chain and then exit, so for a third of the waves the chain IS the whole cost.

**Rewrite (DXIL text level, `gpu-shaders/meshvs/scalarize.py`, no HLSL decompile, no float op touched):**
```
P:     ... slot phi K ...                          br wf_h
wf_h:  k = WaveReadLaneFirst(K); <P's rest, K->k>  (single-entry region: the chain blocks, K->k,
       bindless createHandle NonUniform flag cleared - k is wave-uniform)
wf_m:  the region's join phis (+ the 3 hoisted matrix-palette loads)
       br (K == k) ? M : wf_h                      (lanes leave in the iteration whose k is their own slot)
M:     x = phi [wf_x, wf_m]  (LCSSA)  ... rest unchanged
```
Asserted per record: slot phi found from `udiv vid, 384`; the region (P .. its immediate post-dominator) uses no
per-lane value but K; no output/UAV/discard inside the region; every region exit goes to M. Each lane evaluates the
chain on exactly its own slot -> output identical by construction; the loop runs once per distinct slot in the wave
(1, rarely 2). The only new feature is WaveOps (SFI0 bit 0x4000; DXIL ShaderFlags bit 19) - SM 6.0 wave intrinsics,
supported by every D3D12 SM6 driver and vkd3d-proton.

**Driver ISA (RGA offline DX12 driver; no RDNA2 target in RGA 2026-05, gfx1102 = RX 7600 used as the mid-range proxy):**

| shader | VMEM loads | SMEM loads | full vmcnt waits | VGPR | SGPR |
|---|---|---|---|---|---|
| PB2 1559714c (pc2 #14/#66/#74) shipped -> MSV (no hoist) -> MSV+hoist | 35 -> 10 -> **7** | 24 -> 60 -> 63 | 11 -> 8 -> 7 | 44 -> 44 -> 48 | 46 -> 86 -> 94 |
| PB2TREE2 f311c8e2 (pc2 #53) shipped -> MSV | 71 -> **46** | 76 -> 115 | 35 -> 32 | 92 -> 92 | 62 -> 94 |
| SM shadow 3fe655e9 shipped -> MSV | 51 -> **25** | 38 -> 75 | 27 -> 22 | 104 -> 104 | 46 -> 86 |
(gfx1103 and gfx1201 give the same counts.) The chain becomes `s_buffer_load` / `s_load` (scalar cache, no VGPRs, no
64-lane address cycles); the dependent VMEM depth per wave drops from ~10 levels to ~4 (typed t25 load + index +
vertex index + vertex data). Occupancy unchanged (VGPR equal or +4 within the same allocation step).

**Proof (`gpu-shaders/meshvs/verify_msv.py`, DXIL reference interpreter = materials/psim + sweep/vendor_gi):** shipped
record vs rewrite on random scenes (all SRV buffers from a per-scene mix of small ints / floats / random u32; direct and
meshlet path both forced), every vertex's outputs bit-compared. WaveReadLaneFirst is **scripted** per vertex: honest
(1 iteration), decoy (first returns a foreign slot: the chain runs on foreign data and the lane must not leave; 2
iterations) and decoy2 (3 iterations); the iteration count is checked too. NaN payloads are compared as one class
(the Python float model is not payload-exact; inputs to the NaN ops were checked identical). Vertices on which the
SHIPPED shader itself never terminates on random data (data-dependent loops) are skipped in both (limit 10 %).
Mutation check: replacing the exit test by `K == K` is caught. Coverage of the rewritten region (honest mode, 8 scene
x path combinations): 1559714c 4/4 blocks, f311c8e2 4/4, SM 3fe655e9 5/5, PB2 2bee37ce 3/4 (all blocks of the module:
17/19, 73/101, 37/76, 34/81 - the untouched per-vertex tail has data-dependent paths random scenes rarely reach). Per record also: assembled module == rewritten text,
container ISG1/OSG1/PSV0 byte-identical, SFI0 differs only by WaveOps, dx.op multiset = shipped + 1 waveReadLaneFirst.

**Build / sets** (`gpu-shaders/meshvs/build_msv.py --set hot|all`):
- `hot`: the 3 meshlet VS digests in the pc2 table (1559714c, 70d0ee9f, f311c8e2) -> 7 files.
- `all`: every meshlet VS (PB2, PB2TREE2, SM, SMTREE2, DEPTH): **1616 unique records, 7817 records, 187 files**;
  all 1616 match the pattern (`meshvs/scan.py --all-mshlt`). Build result: see §6.
- Output `gpu-shaders/replacement/msv_<set>/` (gitignored), manifest `gpu-shaders/meshvs/msv_manifest_<set>.tsv`,
  transport pack `gpu-shaders/meshvs/out/msv_<set>_records.zip` (proven DXBC by original digest + manifest; not in git:
  derived game data). On another PC: `py -I gpu-shaders\meshvs\splice_msv.py <pack> [--flat]` rebuilds every file from
  the game's originals and only writes it if its sha256 equals the manifest (tested: byte-identical).
- Install: `gpu-shaders\apply_msv.ps1 -Set hot|all [-Revert|-Status]` (sha-checked, backups in
  `gpu-shaders\backup\msv\`, refuses while Nioh3 runs; tested on a scratch copy: install, re-install no-op, revert,
  re-revert no-op). Harness/Ally: item `msv` rows in `gpu-shaders/ally_shaders.sh` (flat layout, `splice_msv.py --flat`).
- **Conflict:** `0x52c39db0.file` (trvs) and `0xfd4f418f.file` (pbvs) are also MSV files. trvs/pbvs are load-dedupe
  items with identical RGA ISA (= no gain); msv supersedes them, they cannot be installed together.

**Expected gain (E).** pc2: meshlet-VS PSO rows 0.98 ms/frame incl. PS; VS share 55-75 % (Ally finding) -> ~0.55-0.75
ms of VS; latency chain cut ~40-60 % -> **~0.15-0.4 ms (0.7-1.7 %) on pc2**, more in dense scenes (window 1 of the tour:
1559714c alone 0.40 ms). Ally USER: PB2 meshlet VS rows 1.57 + 1.10 ms at ~75 % VS -> **0.4-1.0 ms (1.5-3.5 %)**.
Risk: the gain needs the VS to be latency-bound (likely: 10-level dependent chain, the 63.6 % wave-fill finding); if
the GPU is fetch/ROP bound in those draws the gain is ~0. Picture: identical by construction (shotdiff confirms).

## 3. Meshlet culling family (MshLod / MIC / MPC) - analysis, not built

- **MshLod#2 (63c07c21, the per-cluster LOD kernel behind the "c4a71e16" buckets; 906 ISA instr, 37 VMEM, 30 full
  waits, 48 VGPR on gfx1103).** One thread per LOD cluster payload `(G1M idx = p0>>6, instance = p0<<14|p1>>18,
  cluster = p1 & 0x3ffff)`; the chain is payload (UAV) -> instance offsets t18 / resource offsets t19 / G1M offsets t16
  (the same 3-6 dwords re-loaded in 6 places) -> bindless G1M root (cluster record, LOD error, bounds) -> page flags
  t21 -> cache codes (UAV) -> atomics into next-level payloads. Same structure as the VS chain, but the key (G1M idx)
  is only wave-uniform when a wave's clusters come from one mesh, and the kernel has UAV writes/atomics, so the MSV
  loop form (re-running a region) is not usable. **Design for round 4:** a guarded fast path - `if
  (WaveActiveAllEqual(g1m))` load the t16/t19 rows once with a uniform index (SMEM) and use them for every re-load,
  else the shipped loads - output identical by construction (equal index into read-only SRVs), atomics untouched and in
  the same order. Its value depends on how often a wave shares a mesh (asset reuse makes that likely at levels >= 1,
  where children of one parent are contiguous). Needs the MinRun=1 numbers first.
- **MIC#0 (e31c389f, 2830 instr, 77 VMEM, 32 samples, 72 VGPR)** - one thread per instance: frustum planes, a 4x4
  Hi-Z footprint (4-iteration loop of 4 `SampleLevel`, unrolled by the driver: fine), then a **summed-area-table lookup
  in `tValidRectCountCumulativeMap`** (VSM valid rects; DXIL blocks 386-450, repeated at ~1569 and a third copy):
  3 header loads at constant indices 0/2/1 and 4 rect-corner loads, **each behind its own branch**, so the ISA issues
  them one at a time with `s_waitcnt vmcnt(0)` after each (gfx1103 ISA lines 499-656: 7 serial round trips). The 4
  corner indices do not depend on each other. Same-output rewrite (round-4 candidate, not built): load the 3 header
  dwords together, then compute the 4 corner indices and issue the 4 loads unconditionally (typed-buffer SRV reads
  out of range return 0 in D3D12 / Vulkan robust access - no fault), keep the original `cond ? v : 0` selects: 7 -> 2
  round trips per wave. MIC runs few waves (one thread per instance; 56 groups in r5), so it is latency-bound and the
  serial lookup is a direct share of its time. GP2 window 1 on pc2 shows MIC at ~0.66 ms per dispatch in a dense
  scene. Expected ~10-20 % of MIC = 0.05-0.1 ms on pc2 average, more in dense scenes. Needs a CS proof scene with a
  valid SAT (sweep/sim.py supports CS). **ISA experiment** (first of the 3 copies only, loads moved to the dominating
  blocks via `sfb_rewrite.Fn`, `meshvs/work/mic/`, not staged, not proven): gfx1103 full waits 33 -> 30, **VGPR 72 -> 64
  (14 -> 16 waves/SIMD)**, branches 101 -> 98. All three copies: ~9 fewer serial round trips on the executed path.
- **MPC#27/#20** pointer chases with atomic slot allocation (gpu-culling-rewrite.md §5): unchanged verdict.
- The RX 9070 spends 0.225 ms on the whole MshLod chain (r5); pc2 spends ~3 ms (GP1 markers, 13 windows). That ratio
  (~14x vs ~2.5x for ordinary passes) says latency/occupancy, not ALU: exactly what scalarization targets, and what an
  async-compute move (shadows-culling.md B1b, engine change) would hide.

## 4. SM shadows

- `SM-Sfb` e5190e82 (0.66 ms, 55 draws) and SM-CSfb / DEPTH-Sfb are **input-assembler skinned** VSs (POSITION,
  BLENDWEIGHT, BLENDINDICES): the 48-97 buffer loads are bone-palette rows indexed per vertex - not uniform, nothing
  to scalarize. Their per-draw constants are already cbuffer (SMEM).
- The meshlet shadow VSs (SM-*Mshltv*, SMTREE2) are in the MSV `all` set (51 -> 25 VMEM on 3fe655e9). On pc2 they are
  only 0.06 ms in the top-100 rows: the VSM page cache keeps static meshlet geometry out of most frames
  (shadows-culling.md).

## 5. Asset reuse (user hint)

- **(a) LOD per instance / per prototype.** MshLod selects LOD per cluster per instance from the instance's distance
  (`vEye`, `fScrDist`, `vThPOE[8]` in its cbuffer). Caching per prototype + distance band would change which LOD an
  instance gets unless two instances are at the same distance: **not same-picture** (LOD pops at band edges). What
  reuse does give for free: waves whose clusters come from the same mesh -> the §3 uniform fast path.
- **(b) Instancing.** The meshlet world is already GPU-driven: one ExecuteIndirect pair per material PSO for all
  instances (36 EI pairs + few draws carry 57k waves in r5 seg 147), so repeated meshes are not separate draws. The
  separate draws are (1) IA-skinned characters/dynamic casters (SM-Sfb 55 draws: different skeletons, cannot merge),
  (2) ~117 draws/frame of PB2 2c6f629c / STD 96690c3b and the 262 tiny alpha-tested quads of shadows-culling.md B4
  (2-tri cards, different models/materials, no SV_InstanceID). Instancing them is an engine change whose gain is CPU
  (~0.3-0.45 ms main thread), GPU ~0 (0.4 us per draw). Negative for GPU.
- **(c) Redundant per-draw state.** Per-draw cbuffers (284 B on the tiny draws) and descriptor tables are CPU/upload
  cost; the GPU side shows no redundant-state signature in GP2 (no bucket dominated by state changes). The barrier
  work (R2a/R1, -7 % on PC) was the measurable state-side lever and was rejected. Nothing new found.
- **(d) Static shadow caching.** Already done by the engine: the sun is a virtual shadow map with a page cache - only
  invalidated pages re-render, static meshlet geometry is culled against `tValidRectCountCumulativeMap`
  (shadows-culling.md). The per-frame shadow draws are dynamic casters (skinned). A second cache: ~0 gain. Negative.

## 6. Status / files

| item | status | files |
|---|---|---|
| MSV hot (3 VS, 7 files) | built + proven (3/3) + staged | `gpu-shaders/meshvs/*`, `apply_msv.ps1`, `msv_manifest_hot.tsv` |
| MSV all (1616 VS, 187 files) | **built + proven 1616/1616 + staged**; pack (24.6 MB) re-splices byte-identical; this PC's game = ORIGINAL x187 | `msv_manifest_all.tsv`, `out/msv_build_all.csv`, pack via bridge, `ally_shaders.sh` item `msv` (187 rows, excluded from `all`) |
| MIC SAT if-conversion | design + ISA experiment (§3), not proven | `meshvs/work/mic/` (local) |
| MshLod#2 uniform fast path | design (§3), not built | - |
| harness | `pc_gpu_session.ps1 -GP2MinRun`; `pc_gpu_report.py` TARGETS `msv` | |

## 7. Volumetrics (handed off)

No new volumetrics work in this round. Pointers for the volumetrics agent: VFSCAT - `fogpar` (bit-exact early-exit
VSM level search) is built and installed with GHDM on pc2's mod run; the full rewrite lost on RADV (VGPR 112-116 vs 96).
VCLOUD - invariant hoist rejected (+VGPR -> 3 waves), skip-identical/occlusion trim not same-picture, only a
`[WaveSize(32)]` metadata build left (ally-gpu-budget.md §6 V/F). On pc2 both are async; their frame cost is below the
1.60 / 1.04 ms wall time shown by GP2.
