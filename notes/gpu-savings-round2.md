# GPU savings, round 2: what can still be cut without changing the image (ROG Ally first)

Status 2026-10-07. Static analysis + offline builds only. The game was not launched and nothing in the game folder was
changed. Data: RGP r5 (`captures/rgp/Nioh3_2026-10-06_19-39-58`, 1440p output, 1080p render, FSR4, RX 9070 at forced
peak clocks, 10.7 ms GPU frame), `re/scripts_gpu/out/rgp_top300/` (all hot PSOs named), the Ally field data in
`captures/ally/`, and the shader files in `shader_dx12\`.

## Bottom line (honest)

- **Goal:** the Ally at 60 fps (16.7 ms) from ~33–40 fps (25–30 ms GPU, 91–95 % busy, power-limited at 25 W).
  That needs about **−40 %** of GPU time per frame.
- **Shader swaps that keep the image identical get about 0.5–1.3 ms on the Ally (2–5 %).**
  Two are built: the WDO#28 blur (earlier) and the ESM blur (new, below). That is +1–2 fps, not +20.
- **Everything "pure" found, if all of it lands, is about 5–8 ms → roughly 40–48 fps.** That adds three groups:
  - CPU-side command-list fixes in the ASI;
  - giving the GPU the watts the CPU wastes today;
  - two near-invisible data edits.
- **60 fps on the Ally is not reachable with same-look GPU optimisations alone.** Two reasons:
  1. On the Ally the frame is dominated by **fixed-size work**: shadow maps, the ESM, fog froxels, GPU culling,
     barrier/flush overhead. Those passes do not shrink with the render scale, and the only big levers on them are
     quality settings.
  2. The Ally's **main thread is ~96 % busy at 33 fps** (`notes/ally-report-2026-10-06.md`). Before any GPU work is
     counted toward 60, run test 0 below to prove the CPU can even deliver 60.

## 0. First measurements to take (they decide everything below)

1. **CPU ceiling test (5 min).**
   - Ally, same spot: set the render scale to its minimum and shadows/volumetrics to LOW. Log with MangoHud for 60 s.
   - If fps stays below ~60 with the GPU under ~80 %, the Ally is CPU/power-limited at 60. In that case no GPU work
     gets there, and the CPU wake storm (ally report §4.2) becomes the main lever.
2. **A real Ally GPU profile.**
   - RADV can write RGP traces itself:
     `MESA_VK_TRACE=rgp MESA_VK_TRACE_TRIGGER=/tmp/rgp_trigger %command%`, then `touch /tmp/rgp_trigger` in game.
     The `.rgp` lands in `/tmp`.
   - `patcher/gpu/rgp_report.py` is written for RDNA4 SQTT. The RGP GUI reads RDNA3 traces directly, so per-pass times
     can be read there even if our parser needs work.
   - This replaces the extrapolated model in §1 with facts.
3. **The Ally's `config.xml`** (shadow, volumetric and AO/GI/SSR quality, render scale, upscaler).
   - The fixed-size passes depend on SHADOW_QUAL and the volumetric setting.
   - The ESM map size is 256²…4096² by quality index (`notes/invisible-gpu.md` §3b).

## 1. Where the Ally's frame most likely goes (model, to be replaced by step 0.2)

**Scaling assumptions** (RX 9070 r5 → 780M):
- Throughput ratio ≈ 7.5×: CU count × clock 56×2.9 GHz vs 12×~1.8 GHz, and bandwidth 640 vs ~85 GB/s.
- Render-resolution passes scale with pixels: 50 % per axis of 1080p = 0.25× the PC's 1080p render.
- Output-resolution passes: 1080p/1440p = 0.56×.
- Fixed-size passes: 1×.
- Barrier/flush/small-dispatch overhead: ~2× (latency, not throughput).
- **Same quality settings as the PC assumed.**

| Family (PC r5 passes) | PC µs | Ally scale | Ally est. ms | Share |
|---|---|---|---|---|
| **Fixed-size**: ESM 297, shadow-map draws ~232, GHDM 82, VFSCAT fog 293 (async), MPC/MshLod/MCLRC culling 336, particle update 100, PFB 95 (async) | ~1435 | ×7.5 | **~10.8** | ~34 % |
| Geometry (vertex/primitive half of the G-buffer + terrain) | ~950 | ×~5 | ~4.7 | ~15 % |
| Render-res pixel work (G-buffer pixels ~950, RefProbe 702, WDO 487, G1E particles 376, deferred lights 277, 449cf0ae 247, G-buffer copy 224, water 186+71+80+48, …) | ~4040 | ×0.25×7.5 | ~7.6 | ~24 % |
| Output-res post + UI (bloom chain, histogram, 169 µs copy, depth upscale, RCAS, UI) | ~700 | ×0.56×7.5 | ~3.0 | ~9 % |
| Upscaler on the Ally (FSR 3.1 / XeSS-DP4a at 1080p; FSR4 not available on RDNA3) | – | – | ~1–2 | ~5 % |
| Tiny segments (<15 µs, 1.36 ms), barrier tails, idle bubbles | ~2350 | ×~2 | ~4–5 | ~14 % |
| **Total** | 10.7 ms | | **~31 ms** | vs measured 25–30 ms GPU-busy: the model is in range |

What this means:
- At a 35–50 % render scale the classic "render-res" passes are only about a quarter of the Ally frame.
- Lowering the render scale further buys little. Per-pixel shader polish on render-res passes is worth ~4× less on
  the Ally than on the PC.
- Fixed-size work and per-pass overhead are worth **the same or more**.
- That is why the ESM blur (fixed 2048²) is the most Ally-relevant shader swap available, and why the shadow/fog
  quality settings are the Ally's real lever.

## 2. Ranked candidates (Ally impact first)

"Ally ms" = estimated saving per frame at the PC's quality settings. "PC ms" uses the r5 RGP segment times.

| # | Candidate | Pass / shader | PC cost → est. saving | Ally est. saving | Look | Status |
|---|---|---|---|---|---|---|
| 1 | **ESM blur: R+1 gathers instead of 2R+1 samples** | `0x1ca8bb39.file` group `ESM`, records #2–#11. Hot: #9 H 7-tap PSO 8501ad7a (57 µs), #4 V 7-tap PSO c570890a (239 µs). Directional ESM 2048², fixed size | 296 µs → **0–0.13 ms** | **0–0.9 ms** (pass ≈ 2.1 ms est.) | identical (§3) | **BUILT, untested**: `gpu-shaders/apply_esm.ps1` |
| 2 | **WDO#28 sun-shadow-mask blur, latency fix** | `0x1fd07cc0.file` record #28, PSO 3e7955a8; render res, 2× waves | 367 µs → **0.15–0.25 ms** | **0.25–0.4 ms** | identical (float reassociation) | **BUILT earlier**: `apply_wdo28.ps1`. `captures/bench-r9-wdo-{orig,fast}{1,2}.csv` show it booted. Pair 1: GPU-busy 8.31 → 6.94 ms, but pair 2 ran at a 60 cap and scene noise is large, so the A/B is still open |
| 3 | Barrier/flush tails + 375 tiny segments | 1369 barriers; 1.16 ms of back-to-back drain tails on PC | 0.5–1.0 ms | 1–2 ms | none | needs a D3D12 command-list hook in the ASI (**not** a file swap; other agent's `src`). Under vkd3d-proton, barriers become Vulkan pipeline barriers, so measure on the Ally first (§0.2) |
| 4 | Particle update: 75 × `Dispatch(1,1,1)`, each with a UAV barrier | exe shader 5e771e53, `FUN 0x2ec7ad4` | 0.05–0.09 ms | ~0.15–0.2 ms | none | CPU change (batch dispatches / drop barriers) in the ASI; recording site not located yet |
| 5 | Output-res 1-tap copy at 32 bit/channel | PSO 5e9935b5 / exe PS d787dbd0, slot +0x2d0, 10.24 ms | 169 µs → 0–169 | 0–0.7 ms | none *if* redundant | unproven: needs a resource-level capture (`notes/gpu-copies-postres.md` §3). Exe-embedded FXC shader, so only a PSO bytecode-swap hook can touch it |
| 6 | Give the GPU the CPU's watts | KIDS job-system wake storm, ~3–5 cores active at 25 W (ally report §4.2). The GPU is clocked at 1.6–1.9 GHz of 2.7 | – | **~2–4 ms** (every ~1 W moved to the GPU ≈ +4–5 % clock) | none | CPU agent's area, but on this device it is the biggest same-look GPU-time lever. WorkerCap=10 is in; its power effect has not been measured |
| 7 | ESM 2048² → 1024² (data) | `.kidsrender` u32 `0x303038e6 → 0xad650905` (offsets in `re/scripts_gpu/out/esm_2048_offsets.txt`) | ~0.2 ms | ~1.5 ms | near-invisible: fog light shafts and indirect shadowing slightly softer | not built: archive edit + visual sign-off needed |
| 8 | Volumetric fog froxel scatter | VFSCAT#235, PSO 5123196a, 160×90×64 grid, async | 293 µs (overlapped on PC) | ~2 ms of work; overlap does not save watts on a power-bound APU | – | no same-look rewrite found (3.7k instructions, 104 transcendentals). A setting, not a shader swap |
| – | RefProbe GI/reflection gather, deferred lights, TOONEDGE, CustomDepthUpscale, G-buffer edit copy | – | – | – | – | re-checked: necessary work, or ≤45 µs (`notes/invisible-gpu.md`, `notes/gpu-copies-postres.md`). The "serial" loops in the deferred-light shaders (772c50c1…) are descriptor waterfalls for non-uniform shadow-map indices, not GHDM-style fetch chains |

**Exe-embedded shaders.** All output-resolution post shaders, and the copy, TAA-filter and depth-upscale shaders,
live inside `Nioh3.exe` (FXC SM5, post library `FUN 0x32fae8c`), not in `shader_dx12\`.
- They cannot be replaced with a file swap, and the exe must not be modified.
- Any work on them needs an `ID3D12Device::CreateGraphicsPipelineState` hook that swaps bytecode by DXBC digest. That
  is ASI territory.
- The digests are listed in `notes/gpu-copies-postres.md` §1.

## 3. Built: ESM_fast (candidate 1)

**What the originals do.** The ESM group holds:
- #0: a full-screen-triangle VS;
- #1: ESM generation (9 gathers + exp2);
- #2–#6: vertical blurs, R = 1…5 (3/5/7/9/11 taps);
- #7–#11: horizontal blurs, same radii.

Each blur pixel does `d = 1/dim; s(k) = t0.Sample(s0, tc ± k·d)` and
`out = ((s(+1)+s(−1))·w1 + s(0)·w0) + Σk≥2 (s(+k)+s(−k))·wk`.
- The target is R32F: the driver keeps one channel (`dmask 0x1`).
- On the PC (gfx1201) the 7 samples are issued back to back, so it is not latency-bound like GHDM. It is bound by
  fetch/texture-address rate and bandwidth.
- The V pass (239 µs) costs 4× the identical H pass (57 µs). The cause is not visible in the trace: the vertical
  access pattern or a decompress in the 37 µs drain tail.

**Replacement** (`gpu-shaders/ESM_fast.hlsl`, built by `gpu-shaders/build_esm.py`):
- **When the destination pixel maps 1:1 onto source texel centres** (always true for the same-size ping-pong):
  - every original tap is exactly one texel;
  - a `Gather` placed on the corner right/below our texel returns a 2×2 block whose u0 column (V) or v0 row (H) is
    ours, so each gather yields 2 taps;
  - result: **R+1 gathers instead of 2R+1 samples** (7 → 4 for the hot R=3);
  - the weights are bit-identical, and the summation order is the original's.
- **Otherwise** (offset > 1/1024 texel, different source/destination size, mirrored), the pixel runs the original
  2R+1-tap code. It uses `SampleGrad` with the same derivatives, i.e. the same LOD.
- **Interface identical, checked by the build:**
  - the SFI0, ISG1, OSG1 and PSV0 parts are byte-identical;
  - no wave ops; reflection stripped like the shipped records;
  - same t0/s0 bindings, ps_6_5, validator 1.5;
  - the 96-byte engine tail is the same for all ten PS records (interface metadata: signature names + sampler name),
    so it is kept unchanged.
- Records #0 and #1 stay byte-identical.

**Driver ISA** (`rga -s dx12` with an R32_FLOAT target, scratch files):

| Target | Original V 7-tap | Fast |
|---|---|---|
| gfx1103 (780M) | 7 `image_sample` | **4 `image_gather4_lz`, all in flight**; Green/Blue/Alpha gathers removed like the original's unused channels; 24–28 VGPRs; ~40 VALU on the fast path (fallback branch skipped) |
| gfx1201 | same as gfx1103 | same as gfx1103 |

Expected effect on the 780M:
- Texture-address cycles per wave64 drop from 112 to 64 (−43 %), while VALU stays below the texture time.
- **If the pass is address-rate bound, ~−40 %; if it is bandwidth bound, ~0.**
- On the PC the 239 µs V pass is far above both its texture-address rate (~45 µs) and its bandwidth (~50 µs) bounds.
  It is limited by something the trace does not show (cache behaviour of the vertical walk). Gathers fetch the same
  cache lines, so a PC gain is not guaranteed.
- Treat this as an **experiment worth 0–0.9 ms on the Ally**.

**Offline validation** (`py -I gpu-shaders/emulate_esm.py`: float32 CPU model of both shaders, D3D gather order,
clamp addressing, tc ±1 ulp off the centre, every R and both directions):
- With a round-to-nearest sub-texel filter: **0 difference in 46,080 pixels**.
- With a truncating filter: up to 2.8 % on random noise. That comes from the original tap mixing ≤1/256 of a
  neighbour when interpolation puts tc 1 ulp below the centre (the gather never does).
  - On a real ESM that is a sub-texel shift of fog-shadow edges: invisible.
- A 2:1 source/destination mapping always takes the fallback, as intended (11,520/11,520).
- Not done: a GPU execution test (no D3D12 host here).

**Files:**
- `gpu-shaders/backup/0x1ca8bb39.file`: original, sha256 `22103ae7…6dd54`, 12,719 B.
- `gpu-shaders/replacement/0x1ca8bb39.file`: sha256 `3c34641f…bc73a`, 21,064 B.
- New container digests (for `pso_map.csv` / RGP checks): #4 V7 `016b302cc61049bbc7aad04479fd3a83`, #9 H7
  `91d3ed4405cfc60998a41a695d94f04b`, and the rest as printed by `build_esm.py`.

## 4. Test steps

### PC A/B (each item separately, game closed between runs)
1. Check `apply_esm.ps1 -Status` and `apply_wdo28.ps1 -Status`: both should read `ORIGINAL`.
2. Baseline: r5 spot, PresentMon for 60 s (GPU busy), RGP capture, screenshots. For the ESM item, take the
   screenshots in a scene with sun shafts through fog.
3. `powershell -NoProfile -ExecutionPolicy Bypass -File gpu-shaders\apply_esm.ps1`. Same launch, spot and run. The
   first boot compiles the new PSOs once.
4. `rgp_report.py <trace> --dump-shaders`:
   - the ESM segments should show digests `91d3ed44…` (H) and `016b302c…` (V);
   - compare them with 57 / 239 µs.
5. Run it ABAB (2× each), because scene variance on this spot is ~±0.1 ms GPU-busy.
6. Visual check:
   - Fog light shafts, shadowed fog volumes and indirect shadowing should match the baseline.
   - Missing or flat fog shadowing means revert: `apply_esm.ps1 -Revert`, or Steam "verify integrity".
7. Repeat the same steps for WDO#28 (`apply_wdo28.ps1`). Its RGP check is seg ~191 digest `9945c063…`, 367 µs before.

### Ally A/B (Bazzite; the PowerShell scripts do not run there)
- Copy by hand, keeping a backup:
  ```
  G="<SteamLibrary>/steamapps/common/Nioh3/shader_dx12"
  cp -n "$G/0x1ca8bb39.file" ~/nioh3-backup-0x1ca8bb39.file    # sha256 22103ae7d838771f4aa8e9506855c2dc04820feece73fe7dbcacddecfe56dd54
  cp replacement/0x1ca8bb39.file "$G/"                          # sha256 3c34641f1fb8d53bcac7111b66b2212f7a9ff1e7378d736626bebdf7f42bc73a
  # WDO#28: original 197f0c43…28ab3, replacement a38f7de7…536f9
  # revert: cp ~/nioh3-backup-0x1ca8bb39.file "$G/0x1ca8bb39.file"   (or Steam verify)
  ```
- **Fixed TDP 25 W** (PL1/PL2/PL3 as in the report). Same save, same spot, Proton version and launch options.
- Before each logged run, play one warm-up minute so vkd3d-proton compiles the new pipelines.
- Run ABAB, 120 s each, with MangoHud CSV logging. Compare:
  - average fps and frame time, 1 % lows;
  - **GPU clock and GPU power**: a pure GPU-work saving shows up as the same watts at higher fps, or the same fps
    at lower watts;
  - fps per GPU watt.
- Best run alongside the RADV RGP trigger (§0.2), so the ESM/WDO pass times are visible directly.

## 5. Not done / open
- Nothing installed. No game launch. No GPU execution of the new shaders.
- No new engine functions were identified this round, so there is no `re/symbols/incoming/gpu-savings.tsv`.
- Hook-level items (3, 4, 5) need the ASI. They are not built, because `mod/Nioh3EngineFix/src` belongs to another
  agent.
