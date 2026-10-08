# Ally GPU budget: measured numbers (2026-10-08, updated with the 1.7.31 GP2 session)

Offline analysis of Ally logs. The game was not run on the PC and nothing in the game folder was touched.
Tool: `re\scripts_gpu\ally_budget.py` (`windows` / `gp1` / `gp2 --min-draws N`). It reuses the `gp2_digest.py` labels.

Tags: **M** = measured on the Ally. **M\*** = measured with GP2 per-PSO stamps (single-dispatch passes are not inflated, see §1).
**E** = estimate (the basis is given). Nothing here is PC-scaled unless it is tagged E.

## 1. Sources

| config | what it is | data | frame |
|---|---|---|---|
| **USER** | the user's config: PRESET LIGHTEST_PERFORMANCE **but VOLUMETRIC STANDARD** (custom), REN_RAT 50 (960x540 -> FSR 3.1 -> 1080p), FSR_SHARPNESS 1.0, FG off | **S1 = GP2 per-PSO, Top 100, 1 frame in 8, ASI 1.7.31** (bridge #293, `captures\ally\gpu_budget_1531\nioh3_ally_modlog_gpub-S1_1500.log`, digest `S1_gp2_top100.txt`); U1-U6 upscaler arms (GP1 markers) in the same folder. Older GP1-only runs: `captures\ally\1723\*`, `1725\*` | **28.7 ms GPU span, 34.9 fps (M\*)**; GP2 covers 27.3 ms (**95 %**), async 3.46 ms; GPUProf=0 control 34.3-34.4 fps |
| **MAX** | MAX preset at REN_RAT 100 (FSR 3.1 native AA) | GP2 per-PSO + GP1 markers, ASI 1.7.5: `nioh3_ally_modlog_175-L2_2112.log` (stock shaders; 6 gameplay windows) | ~61 ms unstamped (16.0-16.6 fps); 64.3 ms GP2-stamped span |

- **Menu dilution (old tables).** `gp2_digest.py` (`gp2_L*_full.*`) averages every stamped window; 26 of L2's 38 are the
  title menu. Any Ally ms quoted from `gp2_L*_full` is a menu-weighted mean. `ally_budget.py gp2 --min-draws 2000` keeps
  gameplay only (S1: windows 6-9, 2470-2610 draws+dispatches/frame).
- **Stamp overhead.** GP2 1.7.31 measures it live: +0.08 ms/frame at USER. Single-dispatch passes are not inflated
  (RCAS 0.320 GP2 vs 0.317 GP1 in the same launch).
- **The "(copy / resolve / clear)" row is partly draws.** Since 1.7.8 an interval is only cut after `GPUProf2_MinRun`
  (4) draws and a copy counts as a full run, so the bucket also bills up to 3 preceding draws (`gpuprof.cpp` l.369-379).
  Its "24.3 per frame" are those draws. Real copy/clear time needs one run with `GPUProf2_MinRun = 1` (§7).
- **Stale labels.** `gp2_hash_index.json` still calls VS be0fa132 "OUR REPLACEMENT (trvs/pbvs)"; it is the stock VS
  (trvs is not installed on the Ally, pbvs records did not run). Rebuild with `gp2_digest.py --rebuild` before quoting.
- GP2 logged the top 100 rows per window (109 distinct in gameplay, 308 buckets in total); rows below ~0.03 ms are cut.

## 2. USER preset: per-PSO, gameplay (M\*, S1, 4 windows, span 29.2 ms stamped)

| # | PSO / pass | ms | per frame | note |
|---|---|---|---|---|
| 1 | FSR3 ACCUMULATE_SHARPEN e7386840 | **1.97** | 1 | output res; §5 |
| 2 | VFSCAT volumetric fog CS 641524ee (**async**) | **1.78** | 2 | VOLUMETRIC STANDARD, 120x67x48 froxels; §6 F1 |
| 3 | PB2 G-buffer VS 1559714c / PS 7dcfe3de | **1.57** | 31 | ~75 % VS (meshlet VS); §6 M1 |
| 4 | (copy / resolve / clear) + up to 3 draws each | 1.54 | ~37 runs | §1, §6 C |
| 5 | VCLOUD sky CS b76874bb (**async**, under DrawSky) | **1.52** | 1 | async wall time; §6 V |
| 6 | RefProbe #121 GI gather CS 4b4c3cbe | 1.26 | 8.1 | stock |
| 7 | PB2 G-buffer VS 70d0ee9f / PS 05ad1715 | 1.10 | 23 | 55-70 % VS |
| 8 | MPC 304bac05 (MeshletPreculling) | 1.02 | 9.9 | culling |
| 9 | PB2 VS be0fa132 / PS a11ff9e5 | 0.77 | 14 | stock VS (label stale) |
| 10 | PB2LS terrain 55954792 / 7d506745 | 0.77 | 4 | |
| 11 | post VS d21b4b07 / bloom e6efc8ff | 0.75 | 4 | |
| 12 | SM shadow-map draws e5190e82 / c4656a8d | 0.67 | 53 | |
| 13 | MCLRC 864b4941 (MeshletCheckRequestLoad) | 0.65 | 3 | culling |
| 14 | G1E particles f645c461 / d4ed3fe4 | 0.62 | 127 | |
| 15 | WDO_SST + PB2DUS 6f7e3409 | 0.61 | 4 | deferred |
| 16 | PB2DECAL c013d811 | 0.54 | 6.7 | |
| 17 | PB2TREE2 9429db01 / 92139110 | 0.49 | 41 | |
| 18 | WDO 0b17198d / PB2MRLR b68c49cd / PB2DS f61e54aa | 0.46 / 0.38 / 0.33 | 2-6 | shadow resolve / deferred |
| 19 | GVD c6cdf35e / ESM 47ef1c9b / MPC a09d4223 / unnamed CS dfe80724 | 0.36 / 0.35 / 0.35 / 0.34 | | |
| 20 | motion blur f11afe97 / RCAS db8d59bf / SRIVER2 / ExeUI_AlphaFadeQuad / FSR PREPARE_INPUTS | 0.33 / 0.32 / 0.30 / 0.29 / 0.29 | | |

**Family totals (all 109 logged rows, M\*; async rows overlap, so they sum past the span):**

| family | ms | | family | ms |
|---|---|---|---|---|
| G-buffer materials (PB2*) | **7.54** | | copy/resolve/clear (incl. absorbed draws) | 1.79 |
| FSR3 (all passes + depth copy + save color) | **3.59** | | GI RefProbe / SDFOCC / HBAO | 1.77 |
| culling (MPC, MshLod, MCLRC, MIC, GVD, HiZ) | **2.88** | | sky VCLOUD (async CS 1.52 + PS 0.14) | 1.66 |
| deferred / shadow resolve (WDO_SST) | 2.04 | | shadow maps (SM, ESM, tiles, cascades) | 1.38 |
| fog (VFSCAT async 1.78 + SSFOG 0.22) | 2.00 | | particles (G1E, AMBSL, GFM) | 1.34 |
| post (bloom, MB, compose, VideoOut, toon edge) | 1.94 | | UI/exe blits 0.39, water 0.29, unnamed CS 0.62, SSR 0.08 | |

GP1 markers in the same launch (5 windows, span 28.65): FSR3 incl 3.65, MeshletPreculling 1.37, VirtualShadowmap 0.66,
MeshletCheckRequestLoad 0.58, Batch Tiles 8 0.58, MeshletResolveLOD 0.40, VideoOut 0.38, DrawSky (graphics part) 0.25.
**The "22 ms unattributed" of the old §2 is now attributed:** the frame is G-buffer-heavy (7.5 ms), not upscaler-heavy.

## 3. MAX (REN_RAT 100): per-PSO, gameplay, stock shaders (M\*, 175-L2, 6 windows)

| # | PSO / pass | ms | draws/fr | family |
|---|---|---|---|---|
| 1 | VFSCAT volumetric fog CS 641524ee (**async**) | 3.59 | 1 | fog |
| 2 | RefProbe #121 diffuse GI gather CS 4b4c3cbe | 3.14 | 3.9 | GI |
| 3 | PB2 opaque G-buffer db2a9be2 | 2.91 | 14 | G-buffer |
| 4 | FSR3 ACCUMULATE_SHARPEN e7386840 | 2.70 | 1 | FSR3 |
| 5 | RefProbe #414 reflection gather CS 6ae59bd0 | 2.36 | 2.9 | GI |
| 6 | copy / resolve / clear | 1.58 | - | copies |
| 7 | WDO_SST + PB2MRLR (ambient-specular occlusion) 2ec48e3f | 1.51 | 3.9 | deferred / shadow resolve |
| 8 | VCLOUD sky CS b76874bb (**async**) | 1.50 | 1 | sky |
| 9 | PB2LS terrain (tessellated) | 1.34 | 1 | G-buffer |
| 10 | WDO_SST + PB2DS deferred lights f61e54aa | 1.28 | 3.9 | deferred |
| 11 | PB2DECAL c013d811 | 1.20 | 3.8 | G-buffer |
| 12 | RefProbe #9 bilateral GI composite 7c978f3f (VS SDFOCC) | 1.20 | 2 | GI |
| 13 | WDO#14 sun-shadow resolve 8a013bc7 (0x1667d3a0) | 1.08 | 1 | shadow resolve |
| 14 | WDO#28 32e8deff / WDO 0b17198d | 1.08 / 1.07 | 3 / 3 | shadow resolve |
| 15 | G1E particles | 1.06 | 102 | particles |
| 16 | ExeCopy_GBufferEdit4RT 7c5acaaf | 1.05 | 2 | copies |
| 17 | FSR3 PREPARE_INPUTS / PREPARE_REACTIVITY / LUMA_INSTABILITY | 1.05 / 0.87 / 0.71 | 1 | FSR3 |
| 18 | PBRLR (SSR) | 1.00 | 1 | SSR |
| 19 | WDO_SST + PB2DUS 6f7e3409 | 0.98 | 4.9 | deferred |
| 20 | PB2UMAH water / CTS caustics / WET / UMTV | 0.96 / 0.56 / 0.52 / 0.47 | 1 | water |
| 21 | ExeUI_TintedQuad 030dc802 / ExeCopy_TintBlit 29ecb6e4 / ExeUI_AlphaFadeQuad 9a76cbef | 0.81 / 0.60 / 0.47 | 3 / 5.5 / 1.9 | copies / UI |
| 22 | MPC 1b4b5b75 (MeshletPreculling) | 0.73 | 4.1 | culling |
| 23 | PB2 (our trvs/pbvs VS) a11ff9e5 | 0.71 | 14 | G-buffer |
| 24 | SSFOG / RefProbe #15 resample 7d51c698 | 0.61 / 0.58 | 2 / 3 | fog / GI |
| 25 | post compose 6a305be4 | 0.46 | 1 | post |
| 26 | PB2TREE2 foliage | 0.44 (4/6 windows) | 14 | G-buffer |

**Family totals (rows ≥ 0.42 ms only, M\*):**
- GI RefProbe ~7.9 ms (#121 + #414 + #9 + #15 + small).
- G-buffer / materials ~7.1.
- **FSR3 7.0** (GP1 marker incl 6.99, +0.40 for the FSR-only depth copy and SaveOpaqueColor).
- Deferred / shadow resolve (WDO_SST full-screen PS) 7.0.
- Copies + exe blits 4.5 (copy/resolve/clear 1.58, GBufferEdit4RT 1.05, TintedQuad 0.81, TintBlit 0.60, AlphaFadeQuad 0.47).
- VSM (GP1 markers) VirtualShadowmap 2.46 + Batch Tiles 8 2.06.
- Fog 4.2 (3.59 of it async).
- Water 2.5. Sky 1.8 (async). Culling marker 1.71. Particles 1.2. SSR 1.0. Post compose 0.46.

**Corrections to numbers in other notes (all came from the menu-diluted table):**
- Game post chain: "0.17 ms" was menu-diluted. In gameplay, compose 6a305be4 is 0.46 ms every frame; DOF, MB and CA are
  rare and under the cutoff. **The 3.3 verdict stands**: post-v1 does not touch compose, and its own targets are rare.
- Exe full-screen PS: "1.44 ms" -> ≥ 3.2 ms (still straight-line blits).
- RefProbe #121 / #414 / #9: "0.56 / 0.43 / 0.28" -> 3.14 / 2.36 / 1.20.
- PB2 opaque: "0.645" -> 2.91.
- VCLOUD CS: "0.386" -> 1.50.

## 4. remaining-work §3 re-checked against the USER measurement

| item | USER measured now (M\*) | verdict |
|---|---|---|
| 3.1 gi #121 | stock 1.26 ms (8.1 disp) | replacement was +1.5 ms slower at MAX: DROP stands |
| 3.1 VFSCAT fog | stock 1.78 ms async | full rewrite DROP stands (cause in §6 F1); **`fogpar` built instead** |
| 3.1 WDO#28 / WDO#14 | WDO rows 0.46 + 0.38 + 0.33 ms; WDO#14 8a013bc7 not in the top 100 | WDO#14 at USER < ~0.1 ms: its win is ~0.01-0.03 ms (E). A/B only at MAX |
| 3.1 ESM_fast | ESM 47ef1c9b **0.35 ms** | real target exists at USER; ESM_fast win ≤ ~0.1 (E) |
| 3.1 MPC#23 / mshlod | MPC 304bac05 1.02 + a09d4223 0.35 + 4225b9ff 0.06; MshLod 0.10 + 0.05 | MPC is now a measured 1.4 ms family: the staged mpc item is worth an Ally A/B |
| 3.2 GI A0/A1 merge | #121 1.26 ms | ~0.05-0.1 ms at USER (E: group-1 share 3-7 % of 1.26) |
| 3.4 VCLOUD hoist / SFB dedupe | VCLOUD 1.52 async; PB2 opaque rows 1.57 + 1.10 + 0.77 | **VCLOUD hoist: REJECT** (≤ 0.05 ms, +VGPR -> 3 waves). **SFB dedupe: REJECT stands**; the PB2 cost is mostly the meshlet **VS** (§6 M1) |
| 3.6 TileStats census | VirtualShadowmap 0.66 + Batch Tiles 8 0.58 (GP1) | smaller at this spot than 1723 (2.6-3.8); keep, lower priority |
| 3.8 FSR3 upscaler | §5 | all arms measured now |

## 5. FSR3 and the upscaler arms (M, bridge #293)

| arm | fps | GP1 span | upscaler | finding |
|---|---|---|---|---|
| U1 FSR, sharpness 1.0 (control) | 34.2 (GPUProf=0: 34.3-34.4) | 28.76 | FSR3 3.65 (ACCUM 2.08, RCAS 0.32) | |
| U2 FSR, sharpness 0.0 | 34.1 | - | FSR3 3.65, **RCAS still 0.31** | RCAS runs at every sharpness (below) |
| U3 XeSS | 33.3 | 29.56 | XeSS 4.00 (Execute 3.72) | **~1 fps slower than FSR** on the 780M (DP4a path) |
| U4 OFF | = U1 | | FSR3 markers present | the game rejected OFF and kept FSR (below) |
| U5a REN_RAT 67 | = U1 (34.3) | | FSR3 3.63 | **67 is not a legal value** (below) |
| U5b REN_RAT 100 | 18.0 (p99 59 ms) | 54.56 | FSR3 6.99 (ACCUM 2.60) | the fit **2.56 + 4.43·r²** predicts 3.67 / 6.99: confirmed |
| U6 FSR + FG on (live switch) | **49.0-49.3 displayed** (from 34.2) | | FSR3 2.02 per displayed frame; OF ~1.27 + interp ~1.35 ms | FG is the only lever that reaches ~50 |

**REN_RAT 67 is not a clamp and not a preset bug.** `REN_RAT` is an enum key, not an int: `ReadConfigKey` (0x1fe4244)
strcmp's the text against the option list `OptsRatioPercent` 0x3a9b830 = {25,30,...,95,100} (steps of 5). "67" matches
no entry, the function returns 0 and the slot keeps its previous value (50). U5b proves LIGHTEST does not clamp: 100
applied. **Use 65 or 70.** (The same rule applies to the sharpness keys: {0.0,0.1,...,1.0}.)

**RCAS at sharpness 0 is not a no-op, so skipping it would change the image (rejected).**
- `FsrConfigApply` 0x2998b4c hard-wires `enableSharpening` (RS+0x1f54) = 1 and passes the FSR_SHARPNESS value as the
  float `sharpness` (RS+0x1f58).
- FidelityFX FSR 3.1 maps `sharpness` 0..1 to RCAS stops 2..0 (`-2·s + 2`; `FsrRcasCon` scales the lobe by
  2^-stops). At 0.0 RCAS still sharpens at 1/4 strength. An exact identity exists only with sharpening disabled, and
  that is a different look (the non-SHARPEN ACCUMULATE writes the output directly).
- The same-look lever stays R3 (fuse RCAS into its consumer, 0.1-0.2 ms E, engine surgery, low priority).
- The remap is FFX 3.1 source behaviour; the exe's statically linked FFX code was not disassembled. U2 measured that
  RCAS still runs, which is consistent with it.

**XeSS / OFF:** the game rejected `RESOLUTION_SCALING OFF` and kept FSR (OSD `FSR (2)`), so U4 = U1. XeSS was accepted.

FSR levers (unchanged): R1 fp16 rebuild of ACCUMULATE_SHARPEN 0.2-0.5 ms (E, near-look, needs the user's OK); R2
wave-size rebuild 0 ± 0.2 (E); R3 RCAS fusion 0.1-0.2 (E).

## 6. Top non-FSR items: why they cost what they cost, and same-look fixes ranked

Sources: `scratch\sky`, `scratch\copies`, `scratch\mat` in the allygpu worktree (untracked RGA / Ghidra / IR output).

**V. DrawSky VCLOUD CS b76874bb (1.52 ms async).** It is not ray-marched at full or output resolution:
- The dispatch is ceil(w/8) x ceil(h/8) of the VCBayer target (`0x1900E8` -> `0x190350`; sync path `0x2F9BEEC`).
  `0x93fb3280.kidsobjdb` sizes that target at render/8 (code -3); the reprojection target is render/2.
  So USER = 15x9 groups (135 waves), MAX = 30x17 (510 waves; the PC RGP shows the same 30x17 at 1080p render).
- It is already temporally amortized: 1 pixel per 4x4 block per frame (16-frame Bayer, `g_iBayerPosNum`) plus
  reprojection. The cheapest CloudRes is forced at every VOLUMETRIC level.
- 32-256 march steps by view angle, early exit at transmittance < 1/255, two layers, no depth input.
  RGA gfx1103: 108 VGPR, 4 waves.
- **Why 1.5 ms at both resolutions:** it is async-queue wall time while it overlaps fog and graphics, not ALU cost. The
  stock shader moved 0.51 -> 1.26 ms when only the fog shader changed (`ally-replacement-ab.md`). Per-window range is
  0.9-1.6 ms. The MAX number came from another build, session and spot. Real frame cost at USER ≤ ~0.3 ms (E).
- No same-look fix worth building. Skip on identical inputs is useless (the Bayer index and cloud offsets change every
  frame). Trimming occluded pixels is not identical (no depth input, history differs). The invariant hoist is ≤ 0.05 ms
  for -1 wave. Only a wave32 metadata build is left (0-0.1 ms E).

**F. VFSCAT fog CS 641524ee (1.78 ms async).** USER runs VOLUMETRIC **STANDARD** (120x67x48, ~6,480 waves, ~270 per
SIMD), not LIGHTEST (that would be fog off). The bit-exact rewrite lost (+0.33..0.54 ms at MAX) because:
1. The shader is throughput-bound. 5 resident waves already hide memory latency, so the single-wave x2.7 model did
   not apply.
2. Occupancy: what RADV compiles (pins folded) is 112-116 VGPR in RGA -> 4 waves, against 96 / 5 for the original.
3. +13-18 % VALU.
4. Speculative spot-light loads and past-end prefetches waste LPDDR5 bandwidth that VCLOUD and graphics share.

**C. copy/resolve/clear 1.54 ms.** It is mostly absorbed draws (§1). The real copies/clears are each needed:
- VideoOut gamma copy `0x10f17e0`: 0.197 ms copy + draw = 0.395 ms. It is skippable only when gamma x == 1.0, which
  at BRIGHTNESS 50 happens in cutscenes only (gameplay x = 1.108), so the gameplay gain is 0.
- MRLR scene copy 0.05 (SSR input). FSR3 SaveOpaqueColor 0.05.
- ~32 ClearUAVUint at frame start for texture streaming (`0x1284e8`): batchable, not skippable.
- Full-RT clears: fast clears under vkd3d, ~0 ms. No skippable call is worth > 0.05 ms at USER.

**M. PB2 material 1559714c/7dcfe3de (1.57 ms, 31 draws).** ~75 % of it is the meshlet VS, 55-70 % for 70d0ee9f/05ad1715.
- LOAD-DEDUPE (trvs/pbvs style) saves 0 here.
- The only same-look lever is scalarizing the ~30 per-meshlet uniform loads (`WaveReadLaneFirst`): 0.3-1.0 ms (E),
  medium risk. It needs psim wave-op support and an exception to the register rule.

| rank | fix | identical output? | expected ms USER | risk | status |
|---|---|---|---|---|---|
| 1 | **F1 `fogpar`**: VFSCAT with ONLY the merged early-exit VSM level search (no prefetch / tail reorder) | **bit-exact (proven, below)** | 0.1-0.25 async, **0.05-0.2 frame (E)** | low-med (RADV VGPRs unverifiable offline; RGA 96 / 5 waves = shipped) | **BUILT + STAGED**, Ally A/B §7 |
| 2 | M1 PB2 meshlet-VS load scalarization | identical (needs proof tooling) | 0.3-1.0 (E) | medium | not built: psim needs wave ops |
| 3 | F2 / V1 `[WaveSize(32)]` metadata-only builds of VFSCAT / VCLOUD | identical by construction | 0 ± 0.2 / 0-0.1 (E, sign unknown) | low-med | not built: cheap follow-up if F1 shows RADV listens |
| 4 | VideoOut gamma-copy skip at x == 1.0 | bit-exact | 0 in gameplay (0.395 in cutscenes) | low | not built (no gameplay value) |
| - | RCAS skip at sharpness 0 | **no** (§5) | 0.31 | - | REJECT |
| - | VCLOUD hoist / skip-identical / occlusion trim | hoist yes, others no | ≤ 0.05 / 0 / - | high / - / - | REJECT |

**F1 proof (`gpu-shaders\fog\build_fogpar.py`, `fog\vf235_par_src.hlsl` = `mkfog.py --no-cl --no-tail`, pins
`fog\vf235_pins.txt`):**
- `gi\verify.py --kind vfscat` vs the shipped DXIL:
  - 768 coverage seeds: 196,608 writes bit-exact, 0 ulp, 137/137 blocks;
  - hold-out seeds 768-2303: 393,216 writes bit-exact, 0 ulp, 137/137 blocks;
  - 256 realistic seeds: 131,072 writes bit-exact (130/137 blocks; rare paths not taken);
  - 128 heavy-light realistic seeds: 65,536 writes bit-exact. Two builds give the same sha256.
- Container: SFI0 / ISG1 / OSG1 / PSV0 / bindings identical; 362 other records of the file byte-identical.
- The packed record dumps to exactly the proven `gi\out\vf235_par.ll`.
- RGA gfx1103, shipped -> fogpar: pinned 96 VGPR / 5 waves (VALU 2097); unpinned (the RADV view) 96 / 5, VALU
  1854 -> 1896 (+2 %), VMEM loads 101 -> 79, full waits 56 -> 47. The full rewrite unpinned was 112-116 VGPR.
- Files:
  - pack `gpu-shaders\replacement\0xa9910db8.file`, sha256 `81007b3f...767334e` (original `4784aabe...bf4f993`);
  - record digest 641524ee -> `5473bfb797f181002f6cd88547d207e4` (GP2 shows this hash when it runs);
  - ASI ShaderSwap file `gpu-shaders\fog\out\psoswap\641524ee74b197c7d8f1535066bb2c1a.dxil` (sha256 `c259803c...d56e7a`).
- Install:
  - Ally: `./ally_shaders.sh apply fogpar` / `revert fogpar`. The item replaces the dropped `fog` row and upgrades or
    reverts its old file.
  - PC: `apply_fog.ps1` (matrix `S_fog`).

## 7. Ally A/B protocol (fogpar; one session, game-lock as usual)

Setup: the USER config exactly as S1 (LIGHTEST + VOLUMETRIC STANDARD, REN_RAT 50, FSR 1.0, FG OFF). Same save, same
shrine spot, stand still, same camera pitch: VCLOUD and fog cost depend on the view. ASI 1.7.31 or newer, no other
shader items installed (`./ally_shaders.sh status all` = ORIGINAL everywhere).
1. **Confirm it runs** (1 launch, B): `apply fogpar`, `[Profiler] GPUProf = 2`, `GPUProf2_Top = 100`,
   `GPUProf2_MinRun = 1`.
   - Expected: the GP2 rows show cs=**5473bfb7** (async) instead of 641524ee, at ≤ 1.78 ms.
   - If 641524ee still shows, the file was not picked up: stop.
   - `MinRun = 1` also gives the real copy/resolve/clear time (§1).
   - In the same launch, at the same spot, read REN_RAT 50 vs 100 (V0, one GP2 window each, edit config.xml between
     launches). That settles VCLOUD's resolution scaling.
2. **fps ABBA**, GPUProf = 0, 4 launches: A = `revert fogpar`, B = `apply fogpar`, order A B B A.
   - Each launch: 30 s settle, 2 x 30 s MangoHud windows. Metric: mean fps and mean GPU frame time per arm.
   - KEEP if B - A ≥ +0.1 fps (≈ -0.08 ms) in both pairs and no pair is negative. Expected +0.05..+0.25 fps
     (0.05-0.2 ms of 28.7).
   - A ±0.1 fps difference is noise: the U1/U1f controls agree within 0.1-0.2 fps.
3. **Screenshot diff** (same-look check; fog is volumetric, so use a foggy view):
   - Take one F12/Steam screenshot per launch at the identical pose: A1, B1, A2 (= second stock launch).
   - Run `py -I tools\testmatrix\shotdiff.py A1.png B1.png --noise A2.png --out fogdiff.png`.
   - PASS = A-vs-B not worse than A-vs-A2 (FSR/TAA jitter makes launches differ). The shader is bit-exact on the
     interpreter, so any systematic diff means a driver/compile issue: revert.
4. **Report** to the bridge: per-arm fps + GPU ms, the GP2 row for 5473bfb7, the shotdiff verdict, and V0's VCLOUD
   rows. Then `revert fogpar` unless KEEP.
- The ASI-only alternative (no game file touched): copy `641524ee74b197c7d8f1535066bb2c1a.dxil` into
  `Nioh3EngineFix_shaders\` and set `[ShaderSwap] Enabled = 1`. The arms then differ by one ini line. Use it only if the
  file swap fails: psoswap has not run a CS on the Ally yet.
