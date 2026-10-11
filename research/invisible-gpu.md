# Nioh 3: GPU work with no (or barely) visible effect

Status 2026-10-06. Static analysis only: the trace `captures/rgp/Nioh3_2026-10-06_19-39-58` (r5, all fixes on, 1440p, 75 % render
scale, FSR4 through OptiScaler, ~10.7 ms GPU frame), `re/Nioh3.dump.exe` in Ghidra, the render graphs, and the shader files.
The game was not touched, nothing in the game folder was modified, and no patch was applied.
Helper scripts are in `re/scripts_gpu/` (list at the end).

## Bottom line

The premise for items 1 and 2 does not hold:
- **The reflection-probe passes are not probe re-captures.** They are the per-pixel GI and reflection gather for the
  current camera, so they have to run every frame.
- **The occluder capacity does not limit GPU culling.**

The real waste is in the shadow post-processing. Shadows cost about **0.87 ms** per frame, but only **0.23 ms** of
that is drawing shadow maps. The rest is filtering:

| Pass | Cost |
|---|---|
| WDO#14 (sun-shadow resolve) | 120 µs |
| WDO#28 (blur) | 367 µs |
| ESM | 297 µs |
| GHDM (replaced by our optional shader file, not distributed; small fps effect, see docs/BENCHMARKS.md) | 82 µs |

One of these, the WDO#28 blur, has the same latency-bound "wait on every fetch" bug that GHDM had. A replacement with
the same output is written, compiled and staged (not installed).

### Ranking (saving × safety)

| # | Candidate | Est. saving / frame | Visual risk | Delivery | Confidence |
|---|---|---|---|---|---|
| 1 | **WDO#28 sun-shadow-mask blur rewrite**: same math, 8 fetches in flight instead of 1 | **0.15–0.25 ms** (of 367 µs) | none (same result up to float reassociation) | shader file swap, game closed (boot) | high that it is latency-bound; speed-up not yet measured |
| 2 | **ESM chain 2048² → 1024²**. The directional ESM feeds only volumetric fog + LPV/indirect | 0.15–0.22 ms (of ~297 µs) | low–medium (softer fog light shafts) | render-graph data edit (boot) | medium: the mapping from quality to array index is inferred, not proven |
| 3 | **Full-screen copies (~0.55 ms in total)**: 4-target G-buffer copy 224 µs, 3-layer output-resolution copy 169 µs, 2 depth copies 81 µs, depth reprojection 66 µs | 0 to ~0.4 ms, only for copies found to be unused | none if unused | needs one resource-level capture first | low: purpose not yet proven |
| 4 | **GPU particle update**: 75 per-emitter dispatches of a single 64-thread group inside one of 10 sim stages, with one global UAV barrier per non-empty stage (particles.md section 6) | 0.05–0.09 ms | none | CPU code change (merge dispatches) | medium; site not located |
| 5 | TOONEDGE2 outline pass runs full-screen even with nothing to outline | ≤ 45 µs | none | CPU skip; site not located | low |
| 6 | RefProbe: 2 stencil-split dispatches per pass could be 1 | 15–30 µs | none | CPU code | not worth it |
| — | RefProbe "update every N frames" | — | — | — | **not possible: screen-space gather, see 1** |
| — | OccluderUnitArrayCapacity 16 → more | ~0 | — | — | **no effect, see 2** |
| — | VSM tile lifespan | ≤ 0.1 ms | — | — | **already cached, see 3** |

---

## 1. Reflection probes ("RefProbe-CllPtSfbSplsSslsSsmVsm", shader_dx12\0xd070067d.file)

### What the two compute shaders really are
DXIL extracted with `re/scripts_gpu/extract_dxbc.py` and disassembled with dxc. The output is in `re/scripts_gpu/out/RefProbe_121.ll` and `RefProbe_414.ll`.

**#121** (digest dcbbd6eb…, PSO d32d17ac1b4fa108)
- `[numthreads(8,8,1)]`.
- Inputs: the main view's G-buffer (`sGBuf0`, `sDepth`, `sStencil`), the camera matrices `mV2W`/`mP2W` and `gv_viewport`.
- It traces into the OBORO octahedral probe field: `sOctDepthSource`, `oct_probe_info`, `oct_ref_table`.
- Output: one `textureStore` of `uOutTex1[tid.xy, slice 0]`.
- `shCoeff` and `sky_occ` are bound but never written in this variant.
- Every pixel first passes:
  - a bounds test: `tid < gv_info_out.xy` and inside `gv_viewport`;
  - a stencil test: `(stencil & gv_mask_param.y) == gv_mask_param.x`.
- So it is the **per-pixel diffuse-GI gather**.

**#414** (digest 89cf7520…, PSO 5a14b619e553032c)
- The same kind of shader for **specular/reflection**.
- It also reads `sGBuf1`, `sOctRadianceSource`, `sOctNormalSource`, `sOctRadianceMask` and `sRLRResult` (the SSR result), plus `fRoughLmt`.
- It writes `uOutTex1` and `sky_occ`.

Neither shader reads `gv_time_param` (cbuffer reg 31 / reg 33), so there is no checkerboard or temporal interleave.

Every dispatch grid has the screen's 16:9 aspect ratio, which a probe-update dispatch would not:

| Dispatch | Grid | Size |
|---|---|---|
| #121, main view | 144×81 ×8×8 | 1152×648 = 0.6 × 1920×1080 |
| #414, main view | 120×68 | 960×544 = half resolution |
| water view (#121, #414, #121) | 84×48, 144×81, 84×48 | |

**Conclusion: these are screen-space lighting passes that depend on the camera. They are not probe captures, so "update every N frames" or "update only on change" does not apply.**

### Trace timing (r5)

| Seg | t (µs) | Dispatch | µs | Note |
|---|---|---|---|---|
| 228 | 5584 | #121 144×81 | 35.5 | stencil light-group 1 (probably characters) |
| 229 | 5620 | #121 144×81 | 302.1 | light-group 2 |
| 230 | 5922 | #414 120×68 | 57.3 | group 1 |
| 231 | 5979 | #414 120×68 | 259.8 | group 2 |
| 283 / 284 / 304 | 7195–7678 | #121 84×48, #414 144×81, #121 84×48 | 23.9 / 23.1 / 5.6 | 2nd view, right before the river/water draws (`PB2UMAH`, `SRIVER2`) |

Total 0.71 ms.

Why each pass is dispatched twice:
- The render graph sets `ParallelStencilLightGroupMask [1, 2]`.
- Each dispatch covers the whole grid and early-outs on the stencil test.
- Merging them would only save the empty early-out waves, about 15–30 µs.

### Where it comes from (render graph + CPU)
Render graph `system 0x102dd706.kidsrender` (the root graphs 0x05c37956 / 0xce279af4 / 0xd813e276 are the same):

| Node | Contents |
|---|---|
| 150 `ReflectionProbePass` | RenderingType 17, LightGroupID 8, ReflectionProbeShader `0xd1eb2b6a`, SetupShader `0x55cd9dfc`, System `0xa33f150e`, target `0x58d0a6f2` (screen-size RT array, 5 slices = the `GBufferRR` layer the deferred shading reads) |
| 151 `ReflectionProbeDenoiser` | temporal: Prev depth/normal + velocity |
| 153 `ReflectionProbePass` | RenderingType 18, inside `BeginCheckSkip '!'` |
| 154–156 `RaytracingReflection` | not active, no RT in the trace |
| 159 `OBOROExport` | |
| 243, 246, 292, 338, 348 | other views |

CPU side (RVAs, image base 0x7FF74F410000):

| What | Where |
|---|---|
| Type registration | `0x2448c5c` (type hash 0x88416cdb, handler table RVA 0x3b392b8) |
| Node factory | `0x25b2ac0` (resolves 20 RT/shader hashes) |
| Node constructor | `0x368a33c` (object 0x138 bytes, vtable RVA `0x410a428`) |
| Per-frame "resources ready?" | `0x368a9e4`: AND of all the node's resources; on first use it lazily creates the probe system through `0x33a0f2c`. That function is init only, not a throttle |
| Neighbouring vtables | `0x410a460` / `0x410a498` (methods `0x368ae9c`, `0x368aedc`, `0x368b0ac`) belong to the UltraMarineEffectGBufferEdit / UltraMarine water nodes (perf-coverage.md), not to the reflection-probe node |

- The dispatch-recording function itself was **not** traced to its Dispatch call. That is not needed, because the work is the screen gather.
- **Probe radiance updates are already event-driven.**
  - Script API: `ReflectionProbe::SetForceRadianceUpdate/IsUpdating/SetRestart/RestartAllImmediatly`, `Graphics::IsOboroProbeUpdate/OboroForceRadianceUpdate`, `Hell::StartRadianceUpdate/SkipRadianceUpdate`.
  - No probe-relight shader group appears in the top 300 segments; the 521 PSO rows were checked.
  - Env parameters `ReflectionProbeRadianceUpdateType` (hash F58CE595) and `ReflectionProbeApplyInterpolateSpan` (433C605E) are 0 in all 62 presets of env 0x396b9210.
  - Their schema sites are 0x260743b / 0x263f1c3 / 0x11cb19d / 0x121bf9e (editor schemas). The runtime reader was not located, because it accesses the value by field offset.

### What could actually reduce it
- The only real levers change what you see:
  - the GI / SSR quality (the 0.6 / 0.5 gather scale);
  - REN_RAT.
- A latency rewrite like GHDM is not obviously possible. Both shaders are huge (12.4k / 24.7k instructions, 118–120 VGPRs), with real per-pixel work. **Verdict: necessary work.**

---

## 2. Occlusion culling (`OcclusionCullingManager.OccluderUnitArrayCapacity = 16`)

**Data.** system `0x908ab6e2.kidsobjdb` holds two objects:

| Object | TargetOccluderId | OccluderUnitArrayCapacity |
|---|---|---|
| `fd5ca92e` | 0 | 16 |
| `1ca04a44` | 1 | 16 |

- The capacity property is hash `FC231E0A`, type u32 (05000001); its schema site is at `0x266e00e`.
- The main view (system `0x62550860`) points at both: `OcclusionCullingManagerNameHash = 0xfd5ca92e` and `…NameHash6 = 0x1ca04a44`.

**What the 16 counts.**
- Each `MotorStageSetting` has one `OccluderUnitObjectNameHash`, so there is one `OccluderUnit` per stage DB.
- An `OccluderUnit` (type 0xDDD0E775) holds `NameHashArray`, a list of occluder **models**. A model becomes an occluder through `Model.OccluderShapeType / OccluderRadius / OccluderAttachmentNameHash`.
- So the capacity is the number of stage occluder units registered at the same time, **not the number of occluders**.
- Across the 152 OccluderUnits in the dumped stage DBs:
  - **139 are empty**;
  - 13 list 2–14 occluders;
  - e.g. stage 0x8166c9be lists 14, which are octree placeables of type 16.
- This is inferred from the data layout. The runtime consumer was not reached:
  - type hash 0x36CE880E is only returned by the stub at 0xa874f0;
  - the OccluderUnit handler is touched in `0x1fc34cc`, which sets handler+0x18 = 0x32.

**GPU culling is a separate system.**
- Meshlet instance culling (`MshLod#0–6`, 225 µs) and Hi-Z (`GFM-Hiz`, `MeshletGenerateHiZMap` nodes) do the visibility work.
- `[App]CustomOccluderRayCastThread` is a CPU worker pool for the custom-occluder ray casts.

**Verdict.**
- Raising the capacity is expected to change nothing, because fewer than 16 units are ever non-empty.
- It is an allocation-size field in a boot-loaded system DB, so it would have to be changed in the archive or a loose file, not live.
- **Not recommended.**

---

## 3. Shadows

**Shadow-map rendering is already cheap.**
- All draws with SM-, SMTREE2- or DEPTH- vertex shaders add up to **232 µs** per frame, including the local-light cube maps (`SM-CM0Sfb`).
- For a 4096² VSM plus cascades, that only adds up if pages are cached. A full re-render would cost far more.

**The lifespan knob.**
- `VirtualShadowMapTileLifeSpan` is CommonViewSetting property #149 (hash C097D185, u32, schema at `0x26600ad`).
- No shipped DB sets it (see game-data.md §4.0), so the value comes from a code default.
- At best a lifespan change could trim a fraction of the 0.23 ms, at the cost of VRAM and visible pops. **Not worth pursuing.**

**Where the shadow time really goes (r5):**

| Pass | µs | What it is |
|---|---|---|
| `WDO-PtSrbSsmVsm#14` (0x1667d3a0.file), PSO 4001317a | 120 | Sun-shadow resolve at render resolution: CSM `sShadow`, `sSubShadow` cascades, `sVirtualShadow` + `tVirtualShadowMapFlags`, writes a mask. 3852 instructions, 85 samples. Necessary |
| **`WDO-PtSrbVsm#28` (0x1fd07cc0.file), PSO 3e7955a8** | **367** | **Depth-aware 1-D vertical Gaussian blur of that mask (alpha only). Latency-bound, see below** |
| `ESM#9` + `ESM#4` (0x1ca8bb39.file) | 57 + 239 | 7-tap separable blur of the directional exponential shadow map (H then V). Already well written: 7 samples in parallel, the driver reduced it to 1 channel |
| GHDM-FAST (our replacement) | 82 | Confirmed active in r5 (was 655 µs) |

### 3a. WDO#28 rewrite (candidate #1)
Original semantics, from DXIL and ISA (`re/scripts_gpu/out/WDO_28.ll`, `captures/.../isa/3e7955a89ee71e82.txt`):
```
cbuffer: vP2T, vResolutionParam(.w = v step), vBlurParam(.x radius R, .y k, .z depth threshold), vDepthParam(lin z, .z far), vViewportParam
uv = tc*vP2T.xy + vP2T.zw; c0 = sColor[uv,0].w; z0 = 1/(depth*dp.x+dp.y)
if (dp.z <= z0 || !inViewport(uv)) out = c0
else for t in [-R..-1] then [1..R]: if inViewport && depth valid: w = exp2(-t*t*k) * (|z-z0| < thr); acc += w*c; wsum += w
out = float4(0,0,0, acc/wsum)
```
- Each tap costs `image_sample depth → s_wait_samplecnt 0 → compare → image_sample colour → s_wait_samplecnt 0`.
- That is 2 serialized round trips per tap, the same pattern as GHDM.

The replacement is `re/scripts_gpu/WDO28_fast.hlsl`:
- Same registers (t1/s1 depth, t3/s3 colour array, $Globals 80 B), the same input/output signature and interpolation modes (verified against the original metadata), ps_6_5, validator 1.5.
- It processes 4 taps per iteration and fetches depth and colour **unconditionally**, masking each contribution with the original predicates, so a NaN at a rejected tap is still excluded.
- The summation order is the same.
- `exp2` matches DXIL op 21. A first draft used `exp`, which would have been wrong.
- RDNA4 ISA (`rga -s dx12 -c gfx1201`, `re/scripts_gpu/out/rga/`): **8 `image_sample_lz` in flight per wait**, 24 VGPRs. The original issues 2 per tap with a full wait after each.
- Expected: 367 → roughly 120–200 µs, depending on R at runtime (not in the trace).

Staging:
- `py -I re/scripts_gpu/build_wdo28.py` builds `re/scripts_gpu/out/wdo28/0x1fd07cc0.file`. It reads the game file read-only.
- Record #28 has the layout `CXDK2000 | u32 dxbcSize 6175 | u32 tailSize 196 | DXBC | tail`, and its table size is 6388.
- The build rewrites the DXBC (7183 B), keeps the 196-byte engine tail, fixes the table, total and blob size, and checks that the other 137 records are byte-identical.
- New container digest: 9945c0635ca06e3267abf943ae64a1cc.

Risk and how to verify:
- **Unverified:** whether the 196-byte reflection tail carries anything derived from the DXBC. GHDM was a compute group without a tail.
- Install the same way as `apply_ghdm.ps1`, with the game closed and a backup.
- If the game rejects it, the shadow mask will be missing or flat → revert.
- Then capture RGP and check that seg 191's digest becomes 9945c063… and its time.
- Compare screenshots of soft shadow edges, including against the sky.
- Can only be applied at boot, because PSOs are built at startup.

### 3b. ESM resolution (candidate #2)
How the ESM is selected and used:
- The directional ESM render target is picked from a 5-entry array that sits in the render graph on `DeferredIndirectPass` (node 149, which carries `ESMShaderObjectHash 0x1d2050e8`) and `VolumetricFogScatter` (node 162):

  | Entry | Render target | Size |
  |---|---|---|
  | 0 | `0xe2bd5591` | 256² |
  | 1 | `0x2e9e3a6e` | 512² |
  | 2 | `0xad650905` | 1024² |
  | 3 | `0x303038e6` | 2048² |
  | 4 | `0x07672c20` | 4096² |

  Sizes come from the RT table in system 0x93fb3280.
- In `VolumetricFogScatter` the fog's `ShadowmapTextureObjectNameHash` is the same list with a plain 4096² map (`0x8e951a98`) in slot 4.
- So the ESM feeds **volumetric fog in-scattering and LPV/indirect**. Both are low-frequency, and the fog grid is 160×90×64.
- SHADOW_QUAL has 5 usable values (OFF is rejected), which suggests index = quality − 1. HIGH would then mean **2048²**, and the ESM cost is consistent with that.

Patch idea (data, boot-time):
- In every live `.kidsrender`, replace u32 `0x303038e6` with `0xad650905` in the ESM / fog arrays.
- In system `0x102dd706.kidsrender` the byte offsets are `0x2103, 0xf1ee, 0x104ca, 0x11041, 0x11055`. The other 11 graphs have the same 4–5 hits; all offsets are listed in `re/scripts_gpu/out/esm_2048_offsets.txt`.
- Delivery: loose-file override, or fdata injection with a backup.
- Expected effect: ESM blur cost ÷4, saving about 0.15–0.2 ms.

Risks:
- Fog light shafts and indirect shadowing get softer.
- The index mapping is an inference and must be confirmed with an RGP capture: the ESM#4 and ESM#9 segments should shrink.
- If it is wrong, nothing changes.

---

## 4. Other work worth a look

### 4a. Full-screen copies (≈0.55 ms; purpose not proven)
These shaders are FXC SM5 shaders embedded in the exe, not in shader_dx12. `re/scripts_gpu/exe_dxbc.py` extracts them and `fxcdis.py` disassembles them.

All their PSOs are created in one post-effect library initialiser, `FUN 0x32fae8c`, which stores each PSO in a slot of the library object R15:

| Shader (digest) | µs | What it does | Created at | Slot |
|---|---|---|---|---|
| 7c5acaaf… (PSO 54e610d3) | 224 | **4-target copy**: t0, t1 (2D) and t2 slices 0 and 1 → MRT0–3, 9 instructions, at 4.22 ms right after the G-buffer | `0x32fcc7c` | `+0x308` |
| d787dbd0… (PSO 5e9935b5) | 169 + 3 | 1-sample copy at output resolution, ~3 layers, at 10.24 ms between post and UI | `0x32fcacd` | `+0x2d0` |
| 5fccb795… (PSO 80896c31) | 26 + 56 | depth copy (`oDepth = t0`) at 6.79 ms and 7.27 ms (before the water draws) | `0x32fc8ab` | `+0x220` |
| 0cbad2cb… (PSO 07072497) | 66 | reprojects previous depth with velocity into `oDepth`, at output size, at 9.04 ms just before FSR4 | `0x32fcfcc` | `+0x380` |
| 449cf0ae… (PSO 10c323d1) | 247 | TAA-style temporal filter (5-tap variance clip, history reprojected by velocity, stencil 0x50 anti-ghost), at 8.06 ms *before* transparents | `0x32fcf8f` | `+0x378` |

- Likely consumers:
  - The UltraMarine water edits (`WET#0` 48 µs, `CTS#0` caustics 80 µs). These read `GALBEDO/GREFLECTANCE/DIRECT_OCC` while writing `CALBEDO/CREFLECTANCE`.
  - Refraction depth.
  - UI / HDR output.
- There is a river at the test spot (`SRIVER2`, `PB2UMAH` 186 µs covers ~1/3 of the screen), so the water passes are legitimate there.
- **What to do next:** a single resource-level capture (PIX or RenderDoc on another PC, or decoding RGP's API resource chunk) that shows which RTs each copy writes and whether anything reads them before they are overwritten.
- The 3-layer output-resolution copy and the depth reprojection right before an external upscaler are the most suspicious. They could be frame-generation / XeSS-input preparation that runs even with FG off. Check that by toggling FRAME_GENERATION and RESOLUTION_SCALING and diffing `segments.csv`.

### 4b. GPU particle update (100 µs, ~idle GPU)
- `exe` shader 5e771e53… is FXC, created at `0x2ec7b16` in `FUN 0x2ec7ad4`.
  - cbuffer: `nMaxParticleNum, dt, nParticleManagerAddress…`;
  - UAV `ParticleBuffer`.
- In seg 79 it runs as 75 per-emitter dispatches of `1x1x1` (64 threads), so the GPU runs one wave at a time for ~100 µs.
- Batching all emitters into one dispatch would recover most of it. There are no per-emitter barriers, just one UAV barrier per non-empty stage (particles.md section 6, gpu-barriers.md section 2).
- That is a CPU command-list change, and its safety depends on whether emitters read each other's particles (`MainParticleBuffer`). Medium risk; the recording site was not located.

### 4c. Smaller items
- `TOONEDGE2#2` (0x9c481f79.file):
  - 45 µs, a 120×68 compute pass that reads `sInputTex/sDepth/sStencil` and outlines stencil-8 pixels;
  - it runs full-screen every frame;
  - only skippable if nothing on screen uses outlines.
- `PFB#0` (ocean wave profile, 95 µs) and `VFSCAT` (fog, 293 µs) run on the async queue and are fully overlapped, so there is nothing to gain.
- Clears: 38 × `ClearRenderTargetView` = 104 µs in total, all small; nothing obviously duplicated.

---

## Tools written (re/scripts_gpu/)
| File | Purpose |
|---|---|
| `extract_dxbc.py` | extract DXBC #N of a shader_dx12 group, in the same index order as `pso_map.csv` |
| `shinfo.ps1` | extract + `dxc -dumpbin` + print signature/cbuffer/bindings (`-File 0x….file -Index "4,9"`) |
| `timeline.py` | RGP event timeline with shader-group names (`timeline.py <trace-prefix> t0 t1 [min_us]`) |
| `group_rollup.py` | GPU time per shader group (combines several pso_map.csv files) |
| `exe_dxbc.py`, `fxcdis.py` | extract exe-embedded FXC shaders by digest; disassemble them via the system D3DCompiler_47 |
| `g1s_find.py`, `rdb_digest_scan.py` | scan package G1S/binary entries for a DXBC digest (negative for the post-effect shaders: they live in the exe) |
| `findimm.py`, `findptr.py`, `qdump.py` | u32-immediate search, pointer search, qword/vtable dump on the decrypted dump |
| `rtinfo.py` | look up render-target sizes in the RT table |
| `WDO28_fast.hlsl`, `build_wdo28.py` | candidate #1: source + staging build (output `out/wdo28/`, **not installed**) |
| `out/rgp_top300/` | `rgp_report.py --top 300 --dump-shaders --no-waves` (521 PSO rows named; use its `pso_map.csv` for names) |

Caveat on the trace's "approx_waves":
- Some full-screen passes show 2× (WDO#28) or 0.5× (the 449cf0ae filter) the render-resolution wave count. Use segment µs, not waves, when sizing passes.
- Waves = pixels/64 holds for most passes, e.g. WDO#14 is exactly 1920×1080/64.
