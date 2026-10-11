# Nioh 3: late-frame copies/filters and POSTEFFECT_RESOLUTION (static RE)

Status 2026-10-06. Static analysis only: trace r5 `captures/rgp/Nioh3_2026-10-06_19-39-58`, `re/Nioh3.dump.exe` in Ghidra,
render graph `data/dumps/kgk_0x102dd706.kidsrender.txt`, game DLLs (read only). Nothing was patched, the game was not touched.
RVAs below; image base 0x7FF74F410000. Settings struct "S" = the render-settings block written by the upscaler config
callbacks; the per-frame copy of it used by the upscaler/post code sits at S+0x4d0 (e.g. S+0x20d8 is read as +0x25a8).

## Bottom line

- **No frame-generation work runs with FRAME_GENERATION=OFF.** All FG inputs are gated on the setting and are off:
  - `fn 0x39a39c` reads FRAME_GENERATION (0x45BB008). With OFF, `and dword [rbx+0x20e0],0` (0x39a7c8) clears the
    HUD-less flag, and the UI-mask flags S+0x2054/S+0x2064 are only set in the FSR-FG branch.
  - `fn 0x253c68` (UI view render) only calls `SaveColorBeforeUiRendering` (0x3453364, CRenderer::m_pHudLessColor) when
    copy+0x25b0 (=S+0x20e0) != 0, and `CGraphicsDevice::m_pUIMask` (0x3453134) when +0x2534 && +0x2524. Both are off.
  - XeSS_ColorOpaqueOnly / XeSS_ReactiveMask are only created when +0x258f (S+0x20bf) != 0. It is 0 (0x2998ce4 writes
    u16 1 to S+0x20be, so 0x20bf = 0).
  - `DLSSVelocityViewportFix` (a velocity copy) only runs when ViewportScaleForUpscale != 1.0 (dynamic resolution off, so not).
- **None of the five suspect passes is OptiScaler's own work.** The four exe-embedded pixel shaders are engine passes. Only
  bc8a53cc… is FSR4 (its DXBC 0d915718… is inside `amd_fidelityfx_upscaler_dx12.dll`).
- So there is no "dead with FG off" pass to cut. What remains is necessary work, or a quality trade-off.

## 1. The five passes

| PSO / PS digest | µs | t (ms) | What it is | Issued by | Read by | Verdict |
|---|---|---|---|---|---|---|
| 07072497 / **0cbad2cb** | 66 | 9.04 | **CustomDepthUpscale**: temporal depth upscale to output res (cur depth at uv·uvInfo, prev upscaled depth reprojected by velocity, clamped to the 4-neighbour min/max) | XeSS execute `0x18d66e0` ("UpscaleDepth" marker 0x18d685d) → `0x2953fc` (call 0x18d68d8), shader = post library slot +0x380. DLSS path does the same (call 0x6b67aa) | Result = `XeSS_OutputDepth` (global 0x45C3E68 / 0x45C3E70). The post chain binds it as its depth input (`0x6da118`, `local_53a8 = _3e68`), so DOF / motion blur at output res use it | **Necessary** while DOF/MB run at output res. A plain-copy path exists (branch below) but is unverified |
| 5e9935b5 / **d787dbd0** | 169 | 10.24 | 1-tap rect copy (`sample t0`), 32-bit/channel export, between the exposure histogram and the first UI draw | generic copy command (op 0x7a, `fn 0x6b79fc`, 41 callers), shader = slot +0x2d0 | the UI / final output | **Not the HUD-less copy** (that one is FG-gated, see above). Exact node not proven; most likely the final copy of the post output (HdrOutput/CopyColorRenderTarget). See §3 |
| 10c323d1 / **449cf0ae** | 247 | 8.06 | temporal filter (5-tap mean/variance clip, history reprojected by velocity, stencil 0x50 edge mode) | `RealtimeLocalReflectionPassExecute` `0x295c44` (symbols.tsv: the Execute of the RealtimeLocalReflectionPass render node, vtable 0x410A0E0; PrepareThinSurfaceBSDF's own Execute is 0x453ACC, see research/render-nodes.md), shader slot +0x378, weight = copy+0x25dc. It is followed by the GenerateFilteredMipMap chain (7d27/9794 draws at 8.31 ms) | translucent / refraction passes (`TranslucentSpecular`, `FullSizeIntermediate`) per the graph reading that first tied it to PrepareThinSurfaceBSDF; with the SSR attribution the consumer needs re-checking. The river (SRIVER2/PB2UMAH) is on screen | **Necessary** (temporal filter feeding reflection / refraction). Only scene-dependent savings |
| 54e610d3 / **7c5acaaf** | 224 | 4.22 | 4-target copy: t0 → RGBA16F, t1 → RGBA16 UNORM, t2[0], t2[1] → RGBA16F | very likely the **GBufferEdit** G-buffer copy (AB/NS/RS/EM → GBufferEdit*Texture; signature and position match: after the G-buffer, before lighting). Node-to-code mapping not proven | GBufferEdit decals / snow / ripples, and **RealtimeLocalReflectionPass (SSR) reads GBufferEditNS/RS** (graph node 290) | **Keep.** No decal draws follow it in this frame, but SSR consumes it |
| bc8a53cc | 214 | 9.66 | FSR4 ML core pass (wave32 WMMA) | OptiScaler → amd_fidelityfx_upscaler_dx12.dll | output color | **Necessary** (that is the upscaler) |

Extra pass found next to FSR4: **d3fd8774 (34.5 µs, 160×90 groups) = engine RCAS** (exe shader fbef920b, FidelityFX
`Const0..3/Sample` cbuffer). It runs after XeSS/FSR4 only when the XeSS sharpness is above 0
(`0x18d6bfd cmp byte [rsp+0x50]` → call `0x52e47c` at 0x18d6c26). It is visible: it is the XESS_SHARPNESS=0.3
sharpening. OptiScaler `[CAS]`/`[Sharpness]` are `auto`, so check that sharpening is not applied twice.

### Patch handles (none recommended without a capture)
- **CustomDepthUpscale → plain copy**: `0x18d6887  74 56` (jz) → `EB 56` forces the non-temporal `FUN_6b6344` path
  (op 0x7b depth copy). Or clear S+0x20d8: in `0x2998ce4` at 0x2998d40 `41 C6 82 D8 20 00 00 01`, last byte → 00
  (DLSS config `0x39af18` at 0x39af6f has the same store). Saves at most ~0.04–0.06 ms.
  **Risk: unknown/likely visible.** The op-0x7b copy writes a render-size rect into the output-size depth. It is unclear
  whether it stretches; if not, DOF/MB at output res get wrong depth. Live-patchable (code byte, takes effect next frame).
- Nothing to patch for FG: already off.

## 2. POSTEFFECT_RESOLUTION (setting 36, live slot 0x45BAFC0)

- **What the setting writes.** STANDARD=0 and HIGH=1. The value goes into one flag per upscaler:
  - XeSS: S+0x20bd (`0x2998ce4`: 0x2998d0a/0x2998d13);
  - FSR: S+0x1fff (`0x2998b4c`);
  - DLSS: S+0x2091 (`0x39af18`).
  The flag is set to 1 for STANDARD. In the copy these are +0x258d / +0x24cf / +0x2561.
- **What it changes.** In the post-apply function `0x6da118`:
  - **0x6dbb6d**: `cmp [rbp-0x7b],0` checks that XeSS is active, then `cmp byte [rbp+0x2e1d],0` checks the flag. If both are set, the post base size is replaced by the XeSS *input* size `[0x45C3DA0]` (1920×1080); otherwise it stays the upscaled output size (2560×1440).
  - This base is used to size the 8 post targets: `CMP2TargetFXAA`, `CMP2OutputInternal`, `TAA3Work1/2`, `TAA3DepthWork1/2`, `TAA3OutputColorInternal`, `TAA3OutputDepthMapInternal` (name table 0x3c03d60; each target has an absolute / ratio / inverse-ratio mode). The targets are reallocated when the size changes.
- **What it does not change.**
  - The upscaler always runs first: XeSS execute is at 0x6daeb8, before the post loop. So the post never moves before the upscaler.
  - STANDARD does not mean "post before upscale". It means the post chain after the upscaler works on render-size targets:
    the 1440p XeSS/FSR4 output is processed at 1080p, then brought back to 1440p.
- **Passes involved at HIGH (output res, after FSR4):**
  - bloom prefilter 90e56d9e (41 µs);
  - downsample c12e69fd ×4 (~131 µs);
  - upsample ab11dcd7 ×5 (~40 µs);
  - composite 660f2f60 (47 µs);
  - exposure histogram 06054478 + 91f9db43 (~42 µs);
  - very likely the 169 µs copy and the 66 µs CustomDepthUpscale consumer side.

  Scaling ~535 µs by 0.56 gives only ~0.23 ms, so the measured 0.41 ms (noise about ±0.1 ms; base1 vs base2 differ by 0.12) suggests at least one output-res pass (most likely the 169 µs copy) drops out entirely at STANDARD.
- **Visual risk of STANDARD.** If `CMP2OutputInternal` (the compose output) is render-size, the final image is 1080p re-upscaled: a real loss of FSR4 sharpness, not an invisible change. The sweep's SSIM column (0.40 for every run) is unusable, so this is not yet measured.
- **No cheaper hybrid exists as a switch.** One flag moves all 8 targets. A hybrid (bloom/histogram at render res, compose at output res) would need edits to the per-target size modes in the PostEffect3 technique data (array `lVar23 + idx*0x24`: mode bytes +8/+9, values +0xc/+0x10). That data was not located; follow-up.

## 3. How to verify (one capture settles §1 row 2 and §2)
1. Same spot as r5, set `POSTEFFECT_RESOLUTION=STANDARD`, take an RGP capture, run `rgp_report.py`, and diff `segments.csv` by PSO:
   - Does 5e9935b5 (169 µs) vanish or shrink? Then it is the post-output copy.
   - Do the 90e56/c12e/ab11/660f/0605 passes drop to 32400-wave sizes?
   - Is there a new stretch pass?
2. Take static screenshots (no HUD, no motion, photo mode) at HIGH vs STANDARD and diff them on foliage and text-like detail. If they are identical, STANDARD is free +0.4 ms. If softer, keep HIGH.
3. Optional: a PIX/RenderDoc capture on another PC to name the 169 µs copy's RTs.

## Tools (re/scripts_gpu2/)
| File | Purpose |
|---|---|
| `dll_digest_scan.py` | look for DXBC digests in the game DLLs + dump (found FSR4 shaders in amd_fidelityfx_upscaler_dx12.dll) |
| `isa1.py` | rga-disassemble single PSO ELFs from the r5 dumps |
| `finddisp.py` | x64 heuristic: instructions using `[reg+disp32]` with a given displacement |
| `findbytes.py`, `findcall.py`, `findrip.py` | byte pattern / E8-E9 call target / RIP-relative reference scans (numpy) |
| `libslot.py` | find `lib=[x+0x2b0]` then `[lib+slot]` (post-effect shader library slot users) |
