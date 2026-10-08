# FSR3 integration audit: is any FSR work output-neutral?

2026-10-07. Static only (decomp + Ghidra asm of `re\Nioh3.dump.exe`, Ally GP2 `captures\ally\gp2_L2_full.md`). Game not run.
Setting under study: Ally MAX, 1080p, RESOLUTION_SCALING=FSR at 100 % ("native AA"), FSR_SHARPNESS 1.0, FRAME_GENERATION=OFF.

**Answer: no.** Every FSR pass that runs on the Ally feeds the final image. Nothing can be removed bit-identically.
FG work is already skipped when FG is OFF, the context is not re-created per frame, and there is no double sharpening.
The only lead is precision (see §5). It changes the image, so it is research only and nothing ships from it.

## 1. Integration (statically linked AMD ffx-api; `ffxDispatch` is exe export ord 111)
| RVA | what |
|---|---|
| `0x18d8898` Fsr3UpscalerExecute | per-frame FSR3 path (callers PostEffectApply `0x6da118`, FSR FG backend `0x34692bc`) |
| `0x346948c` Fsr3EnsureContext | creates the upscale ctx `g 0x45c4300` (desc `0x45c4188`, type 0x10000, DX12 backend 0x1000001) and 5 textures |
| `0x9a63a4` Fsr3DestroyContext | destroys upscale + FG ctx, frees the textures |
| `0x34692f0` Fsr3SaveOpaqueColor | mid-frame CopyRect of scene color -> `0x45c4310` (GP2 "FSR3:SaveOpaqueColor", 0.057 ms) |
| `0x18dfdf4/0x18dfd50/0x18dfd74/0x18dfe18` | ffxDispatch / ffxConfigure / ffxCreateContext / ffxQuery |

**Create flags** (`0x45c4198`): `DEPTH_INVERTED (8)`, plus `AUTO_EXPOSURE (0x20)` only when RS+0x2000 is set or param+8 is 0.
No HDR, DRS, jitter-cancel or display-res-MV flags. maxRender = maxUpscale = target size.

**Upscale dispatch** (type 0x10001, built at `0x18d8c82..0x18d8ff2`). It uses the public ffx-api `ffxDispatchDescUpscale`
layout, plus one extra resource before `output` and a 9-dword tuning tail after `flags`. The game uses a custom FFX fork,
which matches the `FSR3_Stencil` / `FSR3_*` resource names in the exe.
- color = scene color, depth, MV, exposure = game texture (null when auto-exposure is on), **reactive = generated mask
  `0x45c4318` when RS+0x1ff8 == 2, else null**, T&C = null, extra = depth (COMPUTE_READ), output = `0x45c4320`.
- enableSharpening = RS+0x1f54, sharpness = RS+0x1f58, preExposure 1.0, upscaleSize 0 (= max), viewSpaceToMeters 0,
  flags bit0 (debug view) = RS+0x2063. RS = RenderSettings; PostFrameState copy at +0x4d0.

## 2. Candidates checked
| # | candidate | evidence | verdict |
|---|---|---|---|
| 1 | FG work with FG OFF | The FG ctx `0x47092b0` is created only if the FFX FI swapchain ctx `0x47092b8` exists (`0x346948c`). FG prepare (0x20004), UI-resource configure (0x30002) and the interpolation queries are gated on both (`0x18d9018`). The Ally GP2 shows no `FFX_FRAMEINTERPOLATION_*` / `FFX_FI_*` pass. HUD-less / UI-mask flags are set only in the FSR-FG branch (gpu-copies-postres.md) | **not run**, nothing to save |
| 2 | GENERATE_REACTIVE (0.158) + SaveOpaqueColor (0.057) | The generated mask is the **only** reactive input (T&C null; the game passes no mask of its own). It is regenerated every frame from jittered color, so it cannot be cached | **used**, and removing it changes the image (reject) |
| 3 | RCAS after ACCUMULATE_SHARPEN | In FSR 3.1, `_SHARPEN` only redirects the accumulate output into the RCAS input (blob 0x3a0c940 vs ACCUMULATE 0x3a150b0 differ by 164 B). RCAS (0.184) is the single sharpen. The engine RCAS `0x52e47c` runs only on the XeSS path, and fbef920b is absent from the Ally GP2 | **no double sharpening** |
| 4 | Flags adding passes | No flag adds a pass. LUMA_PYRAMID (0.185) always runs in 3.1 because later passes need its luma and farthest-depth mips, and AUTO_EXPOSURE only picks where exposure comes from. DEBUG_VIEW is off (absent from GP2) | nothing to remove |
| 5 | Context re-created per frame | `0x346948c` re-creates only when target sizes, RS+0x1ffe, the auto-exposure choice, FG mode RS+0x2058, the FI-swapchain ptr, the HDR vfunc +0xa0 or param+9 change. These are all stable in play | not per frame. A counter can confirm it (§4) |
| 6 | Wrong resolution | All FSR textures are render- or output-size, and both are 1080p here. `FSR3VelocityViewportFix` (MV CopyRect) runs only when the render ratio != 1.0, so it is skipped at 100 % (absent from GP2) | OK |
| 7 | `DebugFSR3CopyUpscaledDepth` (5fccb795 `oDepth=t0` draw; GP2 0.145 ms is the total for that PS, and the FSR draw is part of it) | Copies FSR internal resource 0x33 (ffxQuery 0x10010001) into `0x45c4328`. The result is `0x45c4330`, the post-chain depth that PostEffectApply reads (`0x6db979`, `0x6df8c1`) | **used**. Replacing it with raw depth is not provably identical, because it is the fork's internal resource (low value, ≤0.05-0.1 ms) |

## 3. Patch handles (none applied)
- `0x18d8a42` `cmp [rbp+0x2868],2 / jnz`: reactive-mode gate (generate + use). Patching it changes the look. Do not ship.
- `0x18d8d8a`: reactive slot select (mode 2 ? `0x45c4318` : null).
- `0x18d971b` area: upscaled-depth copy. Skipping it leaves `0x45c4330` stale. Do not ship.

## 4. Ally check (optional, diagnostic only, no look change)
An ASI counter on the exe exports `ffxCreateContext` / `ffxDispatch` (desc type at +0) would log per minute:
creates, 0x10001, 0x10006 and 0x20004. Expected at MAX with FG OFF: creates = 0 after load, 0x10001 = 0x10006 = 1 per frame,
0x20004 = 0. A non-zero create rate would be a real bug worth a fix. Fold it into the next batched Ally session; no A/B is needed.

## 5. Research lead (changes the look, so it is not for shipping)
All 28 embedded FSR3 CS blobs are DXIL 6.5 with one permutation per pass. ACCUMULATE_SHARPEN / ACCUMULATE have SFI0 = 0x10
(**MinimumPrecision**, i.e. `min16float`, not native 16-bit), and no wave size is set. Under vkd3d-proton/RADV, min-precision
most likely runs as FP32. That may explain why ACCUMULATE_SHARPEN costs 3.69 ms. A native-fp16 or wave-tuned rebuild would
be faster, but it rounds differently, so it fails the same-look rule. Revisit only if the user accepts "within FSR's own
fp16 tolerance".
Tool: `sfi0.py`-style DXBC part read (offsets from `gpu-shaders\post\out\inventory.csv`).
