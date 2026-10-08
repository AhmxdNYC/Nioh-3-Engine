# GPU shader sweep at MAX settings: what is left after GHDM / ESM / WDO#28 (everything except GI and fog)

Status 2026-10-07. Static work plus offline proofs only. The game was not launched, nothing was installed into the
game folder, and no system setting was touched. Scope: rewrite the game's own `shader_dx12` files so they produce the
same output faster. GI (RefProbe) and volumetric fog (VFSCAT) belong to the GI agent and were not touched.

Inputs: RGP r5 (`captures/rgp/Nioh3_2026-10-06_19-39-58`, RX 9070, HIGH, 1920×1080 render, 10.7 ms GPU) with
`re/scripts_gpu/out/rgp_top300` (all hot PSOs named); the 14:16 trace for per-wave lifetimes; `notes/gpu-maxsettings.md`
for what MAX changes; the shader files read-only from the Steam v2.02 install.

## 0. Bottom line (honest)

- **Two more same-look replacements are built, staged and proven bit-exact offline:**
  - **MPC#23** (meshlet-culling prefix sums, `0x29ee0902.file`): one load round trip + an LDS scan instead of up to 64
    serialised load round trips per thread in a single-group dispatch that runs 4× per frame.
  - **TOONEDGE2#2** (full-screen outline distance pass, `0x9c481f79.file` + its twin `0x47bc5cb4.file`): an 18×18 LDS
    tile instead of 19 image loads per pixel. Measured in the interpreter: **17.5 → 3.5 image loads per pixel.**
- **Expected gain is small: ~0.25–0.4 ms on the Ally at MAX (≈ +0.4–0.7 % fps, 16.3 → ~16.4 fps).** That is below
  MangoHud's run-to-run noise; an Ally RGP capture is the way to see it (§6).
- **No GHDM-class win (+8.7 %) is left among the file-swappable, non-GI/fog shaders.** Reasons, measured or read from
  the ISA (§4):
  - The remaining big costs are the **G-buffer material family** (hundreds of bindless permutations; the PAL compiler's
    descriptor waterfalls do *not* wait inside the loop, so they are not serialised), **particles** (per-effect material
    graphs), and **true pointer-chasing culling kernels** with atomics.
  - The full-screen passes that look "serial" in the ISA run 1.6k concurrent waves on the PC and are throughput-bound
    (SSS: 6 `exp` + 16 divisions per tap; PB2MRLR: an early-exit ray march where batching adds fetches).
  - (2026-10-08 correction: the Ally figure was a PC×6 estimate. Ally GP2 1.7.5 measured the game post chain at
    0.15-0.17 ms/frame and the exe full-screen PS at 1.4 ms; the 7 ms "exe" block is FSR3. See gpu-post-runtime.md §7.)
  - **~1.4 ms of PC GPU time (≈ 8–10 ms on the Ally) is in shaders embedded in Nioh3.exe**: the whole post chain
    (motion blur with 10 samples at MAX, DOF, bloom, tonemap/compose, the TAA-style filter, CustomDepthUpscale and three
    full-screen copies). A file swap cannot reach them; a `CreatePipelineState` bytecode-swap hook in the ASI could (§5).
- **The next real candidate is WDO#14** (sun-shadow resolve, ≈1 ms on the Ally): de-duplicating the per-tap VSM page-flag
  loads is output-identical by construction and cuts its texture-unit work by an estimated 30–45 % (§5). It needs a
  pixel-shader port of the DXIL→HLSL decompiler and the interpreter; not built.

## 1. Method

- **Rank:** `gpu-shaders/sweep/rank_passes.py` sums RGP time per PSO (r5), names it (file/group#index) and adds gfx1201 ISA
  statistics: instructions, fetches, full waits (`s_wait_*cnt 0`), highest VGPR, data loops vs waterfall loops
  (`sweep/out/rank_r5.txt`). The 14:16 trace adds summed wave lifetimes per PSO (occupancy-time, a better throughput proxy).
- **Ally scaling** (same as `gpu-savings-round2.md` / `gpu-maxsettings.md`): render-resolution and fixed-size passes
  ×6–9 of PC µs (GHDM's measured 0.57 ms PC → 5.3 ms Ally saving = ×9.3); latency-bound single-group chains ×2–4
  (`shadows-culling.md`). The Ally renders natively at 1080p, the same pixel count as the r5 render. Output-resolution
  passes ×0.56 (they are exe-embedded anyway).
- **Read** the DXIL of every candidate (`dxc -dumpbin`), decide whether an identical-output rewrite exists that removes
  memory work or serial latency, and only then rewrite.
- **Prove** (`sweep/verify_sweep.py`): the shipped record and the replacement both run in a DXIL reference interpreter
  on synthetic scenes and every UAV is compared bit for bit (§3). Plus interface identity, untouched records
  byte-identical, mutation tests, and RGA ISA for gfx1103 (780M) and gfx1201 (RX 9070).

## 2. Ranked passes at MAX (PC r5 at HIGH; MAX deltas from `gpu-maxsettings.md`)

| # | Pass (shader) | PC µs (r5) | MAX change | Ally ms (est.) | File-swappable? | Verdict |
|---|---|---|---|---|---|---|
| 1 | RefProbe GI/reflection gathers #121/#414 (+ water views) | 707 | ×3.5 at GI HIGHEST (≈2,511) | 15–20 | yes | **GI agent** |
| 2 | G-buffer materials PB2-*/PB2TREE2-*/PB2LS (meshlet + direct, incl. geometry) | ~1,630 | LANDSCAPE ULTRA: terrain LOD ×1.6, distance ×2 | 10–14 | per permutation | no generic rewrite: bindless, hundreds of permutations; waterfalls do not wait inside the loop (§4) |
| 3 | Exe-embedded post / copies (449cf0ae 247, 7c5acaaf 224, d787dbd0 172, 17149dca 131, 2493f9d9 93, d4974586 93, df4adc57 86, 5fccb795 82, 0cbad2cb 66, …) | ~1,400 | MB 10 samples at HIGHEST | 8–10 | **no** (Nioh3.exe) | needs an ASI PSO bytecode hook (§5) |
| 4 | G1E particles (#1486, #1319, #1248, #1561, …) | 394 | – | 2.5–3.5 | per effect | material graphs, 3.2k-instruction PS; no same-look rewrite |
| 5 | Culling chain: MPC (44 dispatches), MshLod (27), MCLRC, MIC, GFM | ~700 | MODEL_DRAW MANY | 1.5–3 (latency) | yes | **MPC#23 rewritten**; the rest are pointer chases with atomics (MPC#27/#20, MshLod#2) or need a new dispatch sequence (`shadows-culling.md` B1) |
| 6 | WDO-PtSrbVsm#28 sun-shadow-mask blur | 367 | – | 2.2–3.3 | yes | built earlier (`apply_wdo28.ps1`) |
| 7 | VFSCAT fog scatter (async) | 303 | – | 1.8–2.7 | yes | **fog agent** |
| 8 | ESM blur | 297 | – | 1.8–2.7 | yes | built earlier (`apply_esm.ps1`) |
| 9 | Water: PB2UMAH 186, CTS 80, UMTV 71, WET 48, SRIVER2 30 | ~415 | – | scene-dependent | yes | water scenes only; no clear win |
| 10 | PB2DS deferred local lights (cube/spot shadow PCF, clustered) | ~280 | – | 1.7–2.5 | per permutation | 45 compare samples per pixel, real work; no bindless; no win |
| 11 | SM-* / SMTREE2 shadow casters | ~140 | SHADOW HIGHEST: one per-object flag | 1–1.8 | – | geometry (dynamic casters, `shadows-culling.md`) |
| 12 | **WDO-PtSrbSsmVsm#14** sun-shadow resolve (CSM + VSM clipmap) | 120 | – | 0.7–1.1 | yes | **next candidate** (§5), not built |
| 13 | PB2MRLR (ambient-specular local occlusion march) + PBRLR (SSR mip chain) | 117 + 75 | SSR HIGHEST: ray steps ×2 | 1.2–1.7 | yes | early-exit march, fetch-bound: batching adds fetches; no |
| 14 | PFB ocean profile (async) | 95 | – | overlapped | yes | ALU only, 16 groups |
| 15 | GHDM-FAST + GHDM#1 (mips) | 82 + 39 | – | 0.5–1.1 | yes | built (#0); #1 = 21 tiny dispatches of 1.4–2 µs (launch overhead) |
| 16 | VCLOUD | 53 | – | 0.3–0.5 | yes | volumetrics (GI/fog agent) |
| 17 | MPC#23 (4 × Dispatch 1×1×1) | 51.8 | – | 0.1–0.2 (latency) | yes | **rewritten (§3.1)** |
| 18 | ARPSSSShader-Hf#16 subsurface scattering | 46 | – | 0.3–0.4 | yes | ALU-bound: per tap 3 samples, 6 `exp`, 16 divisions; no identical-output saving |
| 19 | TOONEDGE2#2 outline distance (Dispatch 120×68) | 45 | – | 0.27–0.4 | yes | **rewritten (§3.2)** |
| 20 | HBAO | ~0 (masked at the r5 spot) | AO HIGHEST: directions ×2 | ~0 here | yes | only where its stencil group covers pixels; re-check with an Ally capture |

Wave-time view (14:16 trace, summed wave lifetimes): the pre-fix GHDM 10.8 %, WDO#28 7.1 %, one G-buffer material PSO
(PB2-BlrGbiMm00MshltSfbVl#1) 6.8 %, FSR4 4.2 %, ESM 3.8 %, exe d4974586 3.7 %, exe 449cf0ae 3.2 %, WDO#14 2.2 %,
TOONEDGE2 0.8 %. Shaders whose waterfall loops wait for their own fetch: 2.7 % of the frame, almost all RefProbe#414
(`sweep/out/waterfall_share_1416.txt`).

## 3. Built: two replacements (staged in `gpu-shaders/replacement/`, not installed)

| | MPC#23 | TOONEDGE2#2 |
|---|---|---|
| Files | `0x29ee0902.file` | `0x9c481f79.file`, `0x47bc5cb4.file` (same record in both; which one the game loads is not known, so both are swapped) |
| Original record digest → new | `34078a28…` → `a840b8dd12f59a5812f1ec8a5a584112` | `318fa766…` → `8986d40f4c6b26c6185083b0737d3976` |
| Source | `sweep/MPC23_fast.hlsl` | `sweep/TOONEDGE2_fast.hlsl` |
| Interface | parts list (incl. ILDN via `-Zs`), SFI0, ISG1, OSG1, PSV0, cbuffer layout, resource names/types/registers, numthreads: identical; cs_6_0, validator 1.5, no wave ops | same |
| Other records | 38 byte-identical; CXDK interface tail kept | 3 byte-identical per file; tail kept |
| Proof | 60 scenes, 0 differences | 96 scenes, 0 differences |
| Memory work | 2 loads/thread + LDS scan (16 waves, 1 round trip) instead of up to 64 serialised round trips | 3.5 instead of 17.5 image loads per pixel (typical mask) |
| gfx1103 (780M) RGA | VGPR 12 → 13, LDS 0 → 8 KB, no scratch | VGPR 48 → 48 (same occupancy), LDS 0 → 6.5 KB, no scratch |
| gfx1201 (9070) RGA | VGPR 7 → 11, LDS 8 KB | VGPR 44 → 48, LDS 6.3 KB |
| Expected PC | 4 × 11–16 µs → ~4 × 2–3 µs (−35…−45 µs) | 45 → ~15–25 µs |
| Expected Ally | −0.08…−0.15 ms (latency-bound) | −0.15…−0.25 ms |

### 3.1 MPC#23 — meshlet draw-argument prefix sums
- **Original:** thread `i = DTid.x·64 + DTid.y` (numthreads 16×64, `Dispatch(1,1,1)`) sums
  `uArguments[nMshltPrm2.x + 4j]` and `uVSDrawMshltMtrOffsets[3j+2]` over `j ∈ [start, i)` with `start = x·64` (or
  `x·64 − 64` for `y = 0`, i.e. the previous block's total), then writes `uVSDrawMshltMtrOffsets[3i]` and
  `uVSDrawArguments[9i+9]`. The loop issues two loads and waits for them every iteration: up to 64 serialised round trips
  in a 16-wave dispatch, 11–16 µs per call on the PC, 4 calls per frame (one per culling view).
- **Replacement:** every element the original reads (`j ≤ cnt−2`) is loaded once, a Hillis–Steele inclusive scan over
  each 64-element segment runs in LDS (6 steps), and thread `i` reads `scan[li−1]`, which is exactly `sum[x·64, i)` or,
  for `y = 0`, the previous segment's total. u32 addition is associative mod 2³², so the result is bit-identical in any
  order. Threads whose range is not inside the group's LDS copy (only `Gid.y > 0` or the first thread of a group with
  `Gid.x > 0`: never for the game's 1×1×1 dispatch) run the original loop unchanged.
- **Assumption:** `uArguments` is not the same resource as `uVSDrawArguments`/`uVSDrawMshltMtrOffsets` (if it were, the
  original would race with itself).
- **Coordination:** `shadows-culling.md` B1 plans a single-pass scan of the whole MPC #28–#26 chain (a dispatch-sequence
  change). This replacement is a drop-in for record #23 only. If another replacement of `0x29ee0902.file` is ever
  built, both must be spliced into one file (`build_sweep.py` splices per record; extend `ITEMS`).

### 3.2 TOONEDGE2#2 — outline distance pass
- **Original:** per pixel: centre `sInputTex` (unclamped); if any channel ≠ 0 → `0x7F7FFFFD`. Else the 8 neighbours of
  `sInputTex` at clamped coordinates, the stencil bit (`sStencil.y & 1` → `0x7F7FFFFD`), the clamped centre depth and the
  8 neighbour depths. Thickness `fThick + 0.5` is reduced by a quarter for every neighbour that is non-zero and not in
  front (`d ≥ dC`, ordered). The result is ±0.5 / ±0.7071070 (`0x3F3504F7`) ± max(acc, 0), chosen by the ×255-quantised
  R/G of the 4-neighbours and the diagonals. 19 image loads per pixel on the common path.
- **Replacement:** the 16×16 group loads an 18×18 tile of `sInputTex` with the original's clamp (`IMax` then `IMin`
  against `nRes − 1`), and the depth tile only if some pixel of the group reaches the depth test (LDS flag via
  `InterlockedOr`). The arithmetic is the original's op for op; the two odd constants are the original's bit patterns.
  Threads outside `[0,nResX) × [0,nResY)` (the 8 padding rows at 1080p) run the original code with direct loads, so
  out-of-bounds behaviour is unchanged.
- The first build differed in exactly those two constants (`FLT_MAX` vs `0x7F7FFFFD`, `0.70710677` vs `0x3F3504F7`);
  the interpreter caught both.

### 3.3 Proof (`py -I gpu-shaders\sweep\verify_sweep.py`, output in `sweep/out/verify.txt`)
- **Interpreter:** `sweep/sim.py` extends a snapshot of the GI agent's DXIL interpreter (`sweep/vendor_gi/`, copied
  unmodified: `dxil_interp.py` sha256 `4bde0796…1885`, `llir.py` `0848a998…5e97`) with barriers (blocks split at
  `dx.op.barrier`, threads run phase by phase, divergent barriers reported), an LDS race detector (a slot written by one
  thread and touched by another within a phase is an error), `atomicrmw` on groupshared memory, and scalar groupshared
  globals. Float semantics: IEEE float32 per IR op; out-of-bounds loads return 0 and stores are dropped (D3D12 robust
  access).
- **MPC#23 scenes (60):** counts 0, 1, 2, 3, 63–66, 127–129, 500, 1000, 1022–1025 with `Dispatch(1,1,1)`; 1023–2049
  with 2 groups in x; 63–1100 with 2 groups in y (the fallback path); a z-dispatch; random 32-bit data (wrap-around),
  random cbuffer offsets. Compared: final contents of all three UAVs and the multiset of writes.
- **TOONEDGE2#2 scenes (96):** textures larger, equal and smaller than `nResX/nResY`; `nRes` 0, negative, 1×1;
  partial groups; a group in the interior of a 70×60 image; inputs with k/255 ± ½/255 and ± 1e-7 quantisation edges,
  NaN, ±Inf, denormals, −0, negative values, z/w-only non-zero texels, large all-zero regions; depth ties, NaN, ±Inf;
  random stencil bits; `fThick` ∈ {0, 1e-3, 1, 2.5, 7.25, −0.5, −3}.
- **Mutation check:** a one-element change in the MPC scan, an unclamped tile load, or `>` instead of `≥` in the depth
  test is caught in every scene; swapping the order of the diagonal offsets or always loading the depth tile is
  (correctly) not detected, because both are equivalent.
- **Not done:** GPU execution. The interpreter proves the DXIL programs compute the same function; driver codegen
  (PAL on Windows, ACO under vkd3d-proton) is the same for both programs' op patterns, and the image ops are plain
  `Load`s (no filtering, no derivatives).

## 4. Investigated and not rewritten (why)

- **G-buffer materials (PB2 family).** Fully bindless (`tBindlessTexture2D` / `sBindlessSamplerState` with
  `NonUniformResourceIndex`). The PAL ISA wraps each sample in a `v_readfirstlane` waterfall but does not wait inside it,
  so the samples are not serialised. Their long wave lifetimes (21.5 µs average for PB2-BlrGbiMm00MshltSfbVl#1) are real
  work. Hundreds of permutations; no generic same-output rewrite.
- **SSS (ARPSSSShader-Hf#16).** Up to 32 unrolled taps (cbuffer `samplenum`), each 3 samples plus 6 `exp`, 16 divisions and a
  square root (Burley profile). Batching the uniform-loop taps would not reduce work; the pass is ALU/transcendental
  bound, and every rewrite that saves ALU changes rounding.
- **PB2MRLR#108/#44 (ambient-specular local occlusion march, `vAmbSpcLclOcc`).** One depth `SampleLevel` per step with
  an early exit on the first hit. Batching 4 steps keeps the output identical but issues up to 3 extra fetches per pixel:
  worse on a fetch-bound Ally.
- **MPC#27 / MPC#20 / MshLod#2.** Dependent pointer chases (instance → G1M offsets → meshlet → material) with an atomic
  slot allocation; the output order depends on the atomics. No identical-output restructuring. MshLod#3, MPC#22/#28–#31
  and GHDM#1 are 1.3–6 µs single-group dispatches dominated by launch and barrier overhead (a CPU-side change).
- **PB2DS deferred lights.** No dynamic resource indexing. The `readfirstlane` loops in the PAL ISA belong to the
  clustered-light scalarisation, and the cost is 45 compare samples per lit pixel.
- **G1E particles.** Per-effect material graphs (atlas lookups per layer, discard), several hundred permutations.
- **CTS/WET/UMTV (water).** Only in water scenes; no serialised structure worth rewriting.
- **TOONEDGE2 via gathers.** Not possible: the pass binds no sampler, and adding one changes the root signature. Hence the
  LDS tile.
- **ESM, WDO#28, GHDM#0.** Already built; unchanged at MAX.

## 5. Next candidates (designs, not built)

1. **WDO#14 VSM page-flag de-duplication** (`0x1667d3a0.file` #14, 3,914 DXIL lines, 120 µs PC, 0.7–1.1 ms Ally).
   **BUILT 2026-10-08** (DXIL-level rewrite, bit-exact, staged off as `wdo14`): `gpu-wdo14-flag-dedupe.md`.
   - **Today, per pixel:** a clip-level search loop (one `tVirtualShadowMapFlags` load per level, serial). Then for every
     PCF tap: a page-flag `buffer_load_format_x` in parallel with the `SampleCmpLevelZero`, wait, branch on the flag.
     A resident page then repeats flag + sample for the cross-fade level. That is 2–3 dependent round trips and two
     texture-unit ops per tap.
   - **The change:** taps of one kernel almost always fall in the same page, so a per-thread cache
     `(idx == lastIdx) ? lastVal : load(idx)` is output-identical by construction (read-only SRV). It removes ~8 of 9
     flag loads per kernel and their waits.
   - **Estimate:** −30…−45 % of the pass's texture-unit work, ≈ −0.2…−0.4 ms on the Ally.
   - **Needs:** a pixel-shader port of the DXIL→HLSL decompiler (`gpu-shaders/gi/ll2hlsl.py` is compute-only) and
     `loadInput`/`storeOutput` in `sim.py` (the shader uses no implicit-derivative samples, so pixels interpret
     independently). Plus scenes that drive the VSM path (cbuffer with valid clipmap flags and page tables).
2. **Exe-embedded post chain** (~1.4 ms PC, ≈8–10 ms Ally - estimate; measured Ally post chain ≈ 0.17 ms, closed in
   gpu-post-runtime.md §7).
   - **Shaders:** the MAX motion blur (10 samples), DOF, bloom (17149dca ×4 downsample, 18a4c2bb upsample), compose
     (6a305be4), the 449cf0ae temporal filter, CustomDepthUpscale (0cbad2cb), copies d787dbd0 / 7c5acaaf / 5fccb795, and
     d4974586 (3.7 % of the frame's wave time, compute, no fetches).
   - **Delivery:** a `CreateGraphicsPipelineState` / `CreateComputePipelineState` hook in the ASI that swaps bytecode by
     DXBC digest (ASI src owner). Then the same offline-proof pipeline applies. Digests are in `gpu-copies-postres.md` §1
     and `invisible-gpu.md` §4a.
3. **MPC chain #28→#26 single-pass scan** (`shadows-culling.md` B1). Needs a dispatch/barrier recording change in
   MeshletPreculling 5301FC, together with a shader rewrite.

## 6. Test steps

**Ally (Bazzite, vkd3d-proton; MAX settings, native 1080p, fixed TDP, same save and spot):**
```
# copy gpu-shaders/ally_shaders.sh and replacement/{0x29ee0902,0x9c481f79,0x47bc5cb4}.file (same layout) to the Ally
./ally_shaders.sh status                 # mpc23 / toonedge2 rows: ORIGINAL
./ally_shaders.sh apply toonedge2        # refuses while Nioh3.exe runs; checks every file before touching any
./ally_shaders.sh apply mpc23
./ally_shaders.sh revert toonedge2       # exact restore from ~/.local/share/nioh3-shader-backup (sha256-checked)
```
1. Play a warm-up minute (vkd3d compiles the new pipelines once).
2. Run ABAB, 120 s each, with MangoHud. Expect ~+0.4–0.7 % fps, which is inside the noise, so judge with the RADV RGP
   trigger: `MESA_VK_TRACE=rgp MESA_VK_TRACE_TRIGGER=/tmp/rgp_trigger %command%`, then `touch /tmp/rgp_trigger`.
   - TOONEDGE2 is the 120×68-group dispatch (~45 µs × Ally factor before).
   - MPC#23 is the four 1×1×1 dispatches right after each MIC + MPC#0/#20 group.
3. Visual check:
   - TOONEDGE2: outlines (e.g. highlighted characters / interactables) should look the same, including at screen edges.
   - MPC#23: wrong output would show up as missing or flickering meshlet geometry. Either way: revert.

**PC:** `powershell -NoProfile -ExecutionPolicy Bypass -File gpu-shaders\apply_sweep.ps1 -Item all` (game closed;
`-Status`, `-Revert`). The RGP `pso_map.csv` should show digests `a840b8dd…` (MPC#23) and `8986d40f…` (TOONEDGE2#2).
Tested here only against a scratch copy of the game folder: apply, re-apply, revert, revert again, byte-exact restore,
and refusal on a modified file (nothing touched).

## 7. Files

| File | What |
|---|---|
| `gpu-shaders/sweep/MPC23_fast.hlsl`, `TOONEDGE2_fast.hlsl` | replacement sources (interface identical) |
| `gpu-shaders/sweep/build_sweep.py` | compile + interface check + per-record splice + repack → `gpu-shaders/replacement/` (reads the game read-only; originals → `gpu-shaders/backup/`) |
| `gpu-shaders/sweep/verify_sweep.py` | bit-exact interpreter proof (`mpc23`, `toonedge2`) and load counts (`stats`) |
| `gpu-shaders/sweep/sim.py` + `vendor_gi/` | interpreter with barriers, LDS race detection, groupshared atomics |
| `gpu-shaders/sweep/s2g.py` | generic `_S2G7000` record reader / splicer (bare DXBC and CXDK records) |
| `gpu-shaders/sweep/rank_passes.py`, `waterfall_share.py`, `out/*.txt` | ranking and ISA scans, with their outputs |
| `gpu-shaders/apply_sweep.ps1` | Windows install / exact revert / status (sha256, refuses while the game runs) |
| `gpu-shaders/ally_shaders.sh` | Linux installer; new rows `mpc23`, `toonedge2`; now checks every selected file before acting |

| File | Original sha256 (v2.02) | Replacement sha256 |
|---|---|---|
| `0x29ee0902.file` (mpc23) | `42f045c370e99b80626c64e9d8084a4a855bb3baad3b04b8f634114d34b97b16` | `26417c0ff41b128443ce364a69e68328588abd5f044129fe1a8c2e8dea80cec7` |
| `0x9c481f79.file` (toonedge2) | `7a8171617b2530648e12ce9dd52c091faf31cd4f1026d3b40a7fdde5788ac1ae` | `d672a5d530f9d1cb49f37ee3de6f626d95e7273eb897e17de3683a8dfb2c2e6d` |
| `0x47bc5cb4.file` (toonedge2) | `07542a8322a617b7783b0c7b79580689339429c1d1e416bf70c8d0f3b0aee99a` | `9a37b6e45360bb8664746687757c68ebfd33cdeb711a6459bbbe29cee111ef80` |
