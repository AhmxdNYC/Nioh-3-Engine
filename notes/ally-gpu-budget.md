# Ally GPU budget: measured numbers only (2026-10-08)

Offline analysis of logs already on disk. The game was not run, nothing in the game folder was touched, and no Ghidra was used.
Tool: `re\scripts_gpu\ally_budget.py` (`windows` / `gp1` / `gp2 --min-draws N`). It reuses the `gp2_digest.py` labels.

Tags: **M** = measured on the Ally. **M\*** = measured with GP2 per-PSO stamps (single-dispatch passes are not inflated, see §1).
**E** = estimate (the basis is given). Nothing here is PC-scaled unless it is tagged E.

## 1. Sources, and why the old Ally tables were wrong

| config | what it is | data | frame |
|---|---|---|---|
| **USER** | the user's preset: LIGHTEST_PERFORMANCE, REN_RAT 50 (960x540 -> FSR 3.1 -> 1080p), FSR_SHARPNESS 1.0, FG off for profiling | GP1 engine markers (no PSO stamps), ASI 1.7.23 / 1.7.25: `captures\ally\1723\*` (6 steady windows at 30-34 fps), `captures\ally\1725\*` (4 launches) | **30.5-33 ms GPU span, ~32 fps** (M; MangoHud v4 = 30 fps / 33 ms) |
| **MAX** | MAX preset at REN_RAT 100 (FSR 3.1 native AA) | GP2 per-PSO + GP1 markers, ASI 1.7.5: `nioh3_ally_modlog_175-L2_2112.log` (all replacements OFF = stock shaders; 6 gameplay windows), L1/L3 as controls | ~61 ms unstamped (16.0-16.6 fps, v71 MAX log); 64.3 ms GP2-stamped span |

- **Menu dilution.** `gp2_digest.py` (and so `gp2_L1_full.*` / `gp2_L2_full.*`) averages *every* stamped window. 26 of
  L2's 38 ON windows are the title/menu: 16.6 ms span and ~56 draws/frame, with FSR3 running every frame and nothing else
  running. Another 6 are loading/transition windows. So:
  - FSR3 dominated those tables.
  - Every gameplay-only PSO is divided by 38 instead of 6, which shrinks it ~5-6x.
  - The "mean span 30.1 ms" is a mix of 16.6 ms menus and 64 ms gameplay. It only looks like a real frame by coincidence.
  - Any Ally ms quoted from `gp2_L*_full` is a menu-weighted mean. That covers `gpu-post-runtime.md` §7,
    `gi-highest-audit.md` §1, `gpu-materials-trim.md`, `render-nodes.md` and `settings-cost.md` "Ally M".
- **Wrong config label.** `gi-highest-audit.md` calls the 175 logs "~STANDARD/540p". They are REN_RAT 100.
  - Every render-res FSR pass in L2 gameplay is 3.7-4.0x the USER run: prepare inputs 1.03 vs 0.28 ms, reactivity
    0.85 vs 0.22, luma instability 0.71 vs 0.19, generate reactive 0.36 vs 0.09.
  - The output-res passes are equal: RCAS 0.32 vs 0.32, upscaled-depth copy 0.218 vs 0.220, VideoOut 0.38 vs 0.39.
- **Stamp overhead.** The output-res equality above also shows that GP2 stamps do not inflate single-dispatch passes.
  - At MAX the whole stamped span is ~5 % over the unstamped frame (64.3 vs ~61 ms).
  - Many-draw buckets (G-buffer, particles) carry most of that 5 %.
  - The STATUS "stamps cost ~14 ms" is the menu A/B (30.4 vs 16.7 ms), not gameplay.
- **Reproducibility.** The untouched single-dispatch PSOs agree within ±0.03 ms across L1, L2 and L3: FSR accumulate
  2.698 / 2.714 / 2.665. So the MAX table is solid at ≥ 0.42 ms per row.
- **Cutoffs.** GP2 1.7.5 logged only the top 40 PSOs per window, which puts the gameplay cutoff at 0.40-0.46 ms. The rows
  below cover 45 of 64 ms. The other ~19 ms are ~650 PSOs under 0.42 ms each (ESM, MshLod, most MPC, HBAO, bloom ...).
  GP1 marker sums cover only ~10.5 of the 32.5 ms USER frame.

## 2. USER preset (REN_RAT 50): engine markers, gameplay, unstamped (M)
Mean of 6 steady windows (1723 a/b/c, 31.9 fps, span 32.5 ms). Ranges are over all 7 launches (1723 + 1725).
Markers nest (`incl`), so do not sum them. Only ~10.5 ms of the frame is under a marker; **~22 ms is unattributed at this preset.**

| # | marker | ms/frame | range | per frame | note |
|---|---|---|---|---|---|
| 1 | **FSR3 (whole upscaler)** | **3.67** | 3.51-3.91 | 1 | 11 % of the frame; breakdown in §5 |
| 2 | MeshletPreculling | 3.03 | 2.21-3.06 | 3.8-4.7 | GPU culling chain (MPC). A marker interval, so it may include the tail of the preceding work |
| 3 | VirtualShadowmap (Clipmaps 0-4) | 1.63 | 1.10-2.02 | 0.3 | VSM page rendering; Clipmap 4 0.58, Clipmap 3 0.42, Clipmap 2 0.34 |
| 4 | Batch Tiles 8 | 1.49 | 0.96-1.81 | 1.2-2.0 | shadow-map tile batches (SM / SMTREE2 draws) |
| 5 | MeshletResolveLOD | 0.72 | 0.52-0.72 | 1.3 | includes sub-markers 0 / 1 / 2 |
| 6 | MeshletCheckRequestLoad | 0.63 | 0.35-0.68 | 1 | meshlet streaming requests |
| 7 | VideoOut | 0.39 | 0.36-0.42 | 1 | output-res final pass |
| 8 | DebugFSR3CopyUpscaledDepth | 0.22 | 0.21-0.22 | 1 | FSR-only helper (`fsr3-audit.md` #7) |
| 9 | DrawSky (VCLOUD) | 0.17 | 0.16-0.25 | 1 | |
| 10 | GenerateSceneHiZ / MeshletInstanceCulling / GPUCulling Setup / MeshletRequestLoad | 0.12 / 0.10 / 0.08 / 0.07 | | | |

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

## 4. Section 3 re-checked against measured cost

| item | claim in remaining-work | where the claim came from | Ally-measured now | verdict |
|---|---|---|---|---|
| 3.1 gi #121 | staged | RGA model | replacement **+1.51 ms slower** (gameplay means: L1 4.65 vs L2 3.14) | DROP (confirmed) |
| 3.1 VFSCAT fog | staged | RGA model | **+0.54 slower** (4.13 vs 3.59, async) | DROP |
| 3.1 toonedge2 | staged | RGA model | ≥ +0.13 slower (0.55 vs < 0.42) | DROP |
| 3.1 WDO#28 | staged | model | original 1.08 ms. The L1 log still shows the original 32e8deff (1.07), so the replacement hardly ran | **unresolved**, not a win |
| 3.1 gi #414 | staged | model | original 2.36 ms (MAX). Never installed on the Ally | unresolved |
| 3.1 ESM_fast | "0-0.9 ms Ally" | **PC 296 µs x 7.5 (E)** | no ESM PSO reaches the 0.42 ms gameplay cutoff at MAX | **PC-scaled.** Win ≤ ~0.2-0.4 (E); resolve with GP2 Top=100 |
| 3.1 MPC#23 / mshlod | "0.11-0.24 ms Ally" | **PC x (E)** | culling marker 1.7 (MAX) - 3.0 ms (USER). MPC 1b4b5b75 0.73 is unchanged L1 vs L2 (-0.03), so #23 is a sub-cutoff PSO | **PC-scaled**; unresolved |
| 3.1 post-v1 | staged | - | targets rare (3.3) | closed by 3.3 |
| 3.2 GI A0/A1 merge | "0.7-1.9 ms Ally" | **PC r5 x 6-8 (E)**. The audit's "Ally GP2" column is the menu-diluted table with the wrong config | Measured PSO totals at MAX: #121 3.14 (3.9 disp/fr), #414 2.36 (2.9 disp/fr). The dispatch counts match the r5 structure | **Re-anchored: ~0.25-0.5 ms at MAX** (r5 group-1 empty share 3-7 % of #121, 6-12 % of #414, applied to the measured totals; E). **~0.02-0.03 ms at USER** (GI STANDARD x 540p = 1/16 of the gather pixels; E). Priority down; still needs A0 first |
| 3.4 VCLOUD hoist / SFB dedupe | - | sized from the diluted table (0.386 / 0.645) | VCLOUD CS 1.50 ms async + 0.29 PS at MAX; DrawSky 0.17 ms at USER. PB2 opaque 2.91 ms at MAX | both fail the register rule (`gpu-materials-trim.md`); the targets are 4x bigger than thought, but VCLOUD is async (frame gain < its ms) |
| 3.5 WDO#14 page-flag dedupe | "0.2-0.4 ms Ally" | shader "0.7-1.1 ms Ally" was PC x (E); the dedupe share is a model | **shader 1.08 ms at MAX (M\*)**, consistent. At USER (render-res, 1/4 the pixels) ~0.27 ms (E) | value stands at MAX (E 0.2-0.4); **~0.05-0.1 ms at USER (E)** |
| 3.6 TileStats census | no ms claim | - | the block it targets is measured: VirtualShadowmap + Batch Tiles 8 = 2.6-3.8 ms at USER, 4.5 ms at MAX | keep; it is the only lever on a measured multi-ms block |
| 3.7 ESM 2048->1024 | "1.5 ms Ally" | **PC 0.2 ms x 7.5 (E)** | no ESM PSO ≥ 0.42 ms in gameplay at MAX | **PC-scaled, likely over**; it changes the look anyway. Needs GP2 Top=100 before anyone offers it |

## 5. FSR3: what it costs on the Ally, and what could cut it

**The 7 ms is MAX/native-AA only.** At the user's REN_RAT 50 the upscaler costs **3.5-3.9 ms (M, 7 launches)**.
- Whole FSR3 marker in gameplay (M):
  - MAX (REN_RAT 100): **6.99 ms**.
  - USER (REN_RAT 50): **3.67 ms**.
  - In menus at REN_RAT 50 it is 6.0-6.2 ms. There ACCUMULATE alone is 4.4 ms vs 2.1 ms in gameplay, while the other passes
    are equal. A likely cause is the lower memory clock in menus (MangoHud 686 vs 930 MHz). Ignore menu numbers.
- Per pass in gameplay (M):

| FSR3 pass | res | USER (REN_RAT 50) | MAX (REN_RAT 100) |
|---|---|---|---|
| ACCUMULATE_SHARPEN | output | **2.11** (2.05-2.41) | 2.68 |
| RCAS (FSR_SHARPNESS 1.0) | output | 0.32 | 0.32 |
| PREPARE_INPUTS | render | 0.28 | 1.03 |
| PREPARE_REACTIVITY | render | 0.22 | 0.85 |
| LUMA_INSTABILITY | render | 0.19 | 0.71 |
| SHADING_CHANGE_PYRAMID + SHADING_CHANGE | render | 0.12 | 0.42 |
| GENERATE_REACTIVE | render | 0.09 | 0.36 |
| LUMA_PYRAMID (2 dispatches) | render | 0.08 | 0.25 |
| marker self | - | 0.04 | 0.14 |
| FSR-only helpers: CopyUpscaledDepth / SaveOpaqueColor | output / render | 0.22 / 0.02 | 0.22 / 0.18 |

- **Cost per mode.** Through the two measured points, FSR3 ≈ **2.56 + 4.43·r² ms** with r = REN_RAT/100.
  - Points between them are E: r 0.58 ≈ 4.0 ms, r 0.67 ≈ 4.5 ms, r 0.77 ≈ 5.2 ms.
  - The **output-res part (~2.6 ms: accumulate, RCAS, depth copy) does not shrink with a lower REN_RAT**. Going 50 -> 40
    would save only ~0.4 ms in FSR.
  - XeSS (DP4a on the 780M), RESOLUTION_SCALING OFF, FSR_SHARPNESS 0 and FSR FG: **no Ally data**. Protocol in §7.
- **Game-side levers that keep the user's choice** (recommendations only, none started):

| lever | output | expected win (USER) | note |
|---|---|---|---|
| R1 rebuild ACCUMULATE_SHARPEN from the same FidelityFX 3.1 source as the SDK's native-fp16 (ALLOW_FP16, `-enable-16bit-types`) permutation; psoswap by digest | **not bit-identical**. The shipped blob is min-precision (SFI0 0x10), which RADV runs as FP32 (`fsr3-audit.md` §5). On Windows/AMD the same blob likely already runs at fp16 | **0.2-0.5 ms (E**: 10-25 % of 2.1 ms) | best value, but it needs the user's OK on "within FSR's own fp16 tolerance" plus a shotdiff check. Prescreen on RADV (3.4) |
| R2 same permutation recompiled with an explicit `[WaveSize]` (SM 6.6; FORCE_WAVE64 or a wave32 build) | identical math (provable with the sweep CS interpreter) | **0 ± 0.2 ms (E, sign unknown)**. RADV already picks a CS wave size itself | cheap to try once 3.4 exists; drop unless the Ally shows ≥ 0.1 ms |
| R3 fuse RCAS into its consumer (FFX dispatched with sharpening off + RCAS math in the consumer PS, emulating the intermediate's storage rounding) | can be bit-exact | **0.1-0.2 ms (E)**: one 1080p write + read saved | engine surgery (ffxDispatch hook + consumer PS swap); low priority |
| changing REN_RAT, mode or sharpness | changes the user's choice / look | - | out of scope; the cost curve above is for the user to decide |
| removing GENERATE_REACTIVE / SaveOpaqueColor / CopyUpscaledDepth | changes the image (`fsr3-audit.md` #2, #7) | - | rejected |

**Recommendation:** R1 is the only lever worth > 0.2 ms. Ask the user. Build it only after the RADV prescreen tool (3.4) exists.

## 6. Top 3 real Ally GPU targets (measured ms, expected same-look win)

1. **FSR3 ACCUMULATE_SHARPEN: 2.1 ms at USER, 2.7 ms at MAX (M).** This is the largest single dispatch measured at the
   user's preset. Win: R1 0.2-0.5 ms (E, near-look, user decision); R2 0 ± 0.2 (identical math).
2. **GPU culling chain** (MeshletPreculling + ResolveLOD + CheckRequestLoad + HiZ + InstanceCulling): **3.5-4.5 ms
   marker time at USER, ~3.3 ms at MAX (M).**
   - It is latency-bound: ~4 chains of small dependent dispatches with barriers between them.
   - Win: 0.1-0.5 ms (E) from the staged MPC#23 / mshlod plus dropping redundant full drains in the chain
     (`vkd3d-audit.md`).
   - First get the per-PSO split at USER (§7). The 1.7.5 cutoff hides it.
3. **Shadows: VSM clipmaps + Batch Tiles 8 = 2.6-3.8 ms at USER (M).** At MAX, 4.5 ms plus the WDO full-screen
   resolve family at 7.0 ms (WDO#14 1.08).
   - Win: TileStats census (3.6) for exact empty-tile / batch skips (unknown until the census runs).
   - WDO#14 dedupe (3.5): 0.2-0.4 ms at MAX, ~0.05-0.1 ms at USER (E).

At MAX only, **GI RefProbe (7.9 ms M\*)** would be #1 by size, but its exact lever (3.2) is worth only ~0.25-0.5 ms (E).

**Honest scale:**
- All three together are worth ~0.4-1.4 ms at USER (E), +1-5 % fps.
- 60 fps at the USER preset needs the GPU frame to drop from ~31 to 16.7 ms. No same-look shader work gets near that.
  At this preset FG is the lever, and it is the user's.
- The largest unknown is the **~22 ms of the USER frame that has no per-PSO data.**

## 7. Ally protocol (one session; fills the gaps above)
ASI ≥ 1.7.8. Same save and spot as `final-benchmark-protocol.md`. Stand still, 30 s settle, then 2 x 30 s windows.
Edit config.xml between launches (one launch per arm).
1. **Per-PSO at USER:** `[Profiler] GPUProf = 2`, `GPUProf2_Top = 100`, `GPUProf2_SampleEvery = 8`, `GPUProf2_AB = 1`,
   `GPUProf_Top = 60`.
   - Digest: `py -I re\scripts_gpu\ally_budget.py gp2 LOG --min-draws 1000 --top 100` and `... gp1 LOG`.
   - Gives the 22 ms unattributed, ESM / MPC / mshlod / WDO#14 at 540p, and the real 3.1 / 3.7 values.
2. **Upscaler arms, REN_RAT 50**, GPUProf = 1 (markers only, unstamped):
   - U1 FSR, sharpness 1.0 (control);
   - U2 FSR, sharpness 0.0 (does RCAS disappear?);
   - U3 XESS (no FFX markers, so read the frame span plus a GP2 window);
   - U4 OFF (what the engine runs instead);
   - U5 FSR at REN_RAT 67 and 100 (checks the 2.56 + 4.43·r² fit);
   - U6 FSR + FSR FG (FFX_FRAMEINTERPOLATION_* cost).
3. **fps per arm:** GPUProf = 0, MangoHud 60 s. Stamps change fps, so never mix those runs into the fps numbers.
4. **Report** per arm: upscaler ms (marker incl or summed PSOs), GPU span, fps. Then update §2, §5 and remaining-work §3.

Logs `captures\ally\1723\` and `1725\` are local (untracked, owned by the load-test agent).
