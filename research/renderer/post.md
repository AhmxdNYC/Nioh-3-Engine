# renderer/post: upscaler, post chain, frame generation, present and pacing (Nioh3.exe Steam v2.02)

2026-10-10. Static summary of existing notes plus a pass over the pre-decompiled C and symbols.tsv. Game not run. Tags: **M** measured / byte-verified,
**E** estimate, **?** not proven. RVAs, image base 0x7FF74F410000. Evidence lives in the cited notes.
`PostEffectApply` 0x6da118 (28 KB) times out in the pre-decompile, so its internal order is taken from shader inventories and GP2/RGP tables, not from its C.

## 1. Where post sits in the frame (render graph, `data/dumps/kgk_0x102dd706.kidsrender.txt`)
Scene HDR is built by deferred + forward nodes, then toon edge (nodes 370/376) and silhouette/highlight (383-387) draw into it, then:

| node | type | role |
|---|---|---|
| 399-405 | SSFadeSaturation 401, OldEffect 403-405 | last scene-space fades/effects at render res |
| 406 | CopyColorRenderTarget | scene colour copy |
| 407-408 | EndScene (WriteBackMask 0x40000001), **HdrOutput** (ColorRT 0x881e16f9, ColorRT16F 0xcc1ba7ab) | resolves the HDR scene target (exec 0x6d8928) |
| **409** | **PostEffect::Apply** (exec 0x945350 -> **PostEffectApply 0x6da118**) | upscaler + whole post chain |
| 410-413 | BeginScene, OldEffect, **PostEffect::Fade** | back-buffer-sized scene, fades |
| 414-419 | Views, CopyColor 417, **StaticScreenLayout 418** (UI quads), EndScene | **UI composite** (output res, AMBSL/ExeUI draws) |
| 420-428 | HdrOutput 421/428, CopyColor 422/427, 2nd UI layer 424 | "VideoOut" gamma/HDR copy to the back buffer |

Apply node inputs: SceneTexture `064b746b`, Depth/Stencil `e13ddb4f`, Normal `7e94c315`, Velocity `62590470`, LightScene `2a691f4a`,
PrevDepth `2fbbc100`, LUT interpolation `90d32cff`; PostEffect3 technique `77084092`. Work targets: TAA3 internal/16F, DOF3 internal/16F + 4 scatter-accumulate weight maps +
LUT weight masks, MB2 internal/16F, FullSizeTempColor, OpaqueOnlyColor (+16F), **ReactiveMask**, KTSS (the engine's own TAA/upscaler: colour/depth/luminance/dilated work targets),
TAA3 Color/Depth Work1/2 (+ MultchView1 copies). SpecifyUsingPE3TechniqueFlags = 6.

## 2. Post chain in order
Order inside Apply is fixed by code: **the upscaler always runs first** (`PostEffectApply_CallXessExecute` 0x6daeb8); the rest runs after it (gpu-copies-postres §2).
The list below is the static reading from shader inventories. GPU captures show a different order after the upscaler (motion
blur before bloom, exposure after the first compose, two compose passes); the measured order is in the Capture-check section at
the end.

1. **Upscale / TAA** (by RESOLUTION_SCALING). Three back ends behind one "XeSS" slot: DLSS (Streamline, `DlssExecute` 0x6b64d8), XeSS (`XessExecute` 0x18d66e0)
   and FSR3 (`Fsr3UpscalerExecute` 0x18d8898). On the test PC, OptiScaler replaces the XeSS entry and runs FSR4 (amd_fidelityfx_upscaler_dx12.dll). The engine's own KTSS/TAA3 is the non-upscaler path.
   - Before it: **CustomDepthUpscale** 0x2953fc (PSO 07072497, PS 0cbad2cb, 66 us): temporal depth upscale to output res (writes XeSS_OutputDepth). Patch handle: 0x18d6887 jz->jmp = plain `DepthCopyCommand` 0x6b6344.
   - **FSR3 integration** (fsr3-audit.md): statically linked ffx-api (`ffxCreateContext/Configure/Dispatch/Query` 0x18dfd74/50/f4/e18), a custom fork (public desc + 1 extra depth resource + 9-dword tail).
     Context made by `Fsr3EnsureContext` 0x346948c (upscale ctx 0x45c4300; flags `DEPTH_INVERTED`, plus `AUTO_EXPOSURE` when RS+0x2000; no HDR/DRS/jitter-cancel flags). It is re-created only when sizes,
     RS+0x1ffe, exposure choice, FG mode RS+0x2058, FI-swapchain ptr, HDR or p+9 change. Per frame:
     (a) `Fsr3SaveOpaqueColor` 0x34692f0 mid-frame copy of opaque colour -> 0x45c4310 (0.057 ms);
     (b) **GENERATE_REACTIVE** (type 0x10006, only when RS+0x1ff8 == 2) builds the reactive mask from opaque-vs-final colour -> 0x45c4318, 0.158 ms. It is the **only** reactivity input
     (the Transparency&Composition mask is always null; the game passes no mask of its own);
     (c) **upscale dispatch** type 0x10001 at 0x18d8ff2: colour = scene, depth, **motion vectors = Velocity 62590470 (render res)**,
     exposure = game texture (null with auto-exposure), reactive, output 0x45c4320; sharpening RS+0x1f54/0x1f58 (FSR_SHARPNESS), preExposure 1.0, flags bit0 = debug view;
     (d) upscaled-depth copy to 0x45c4328 (ffxQuery 0x10010001, internal resource 0x33) -> 0x45c4330 = the depth the rest of post reads;
     (e) `FSR3VelocityViewportFix` MV copy only when the render ratio != 1. Jitter comes from FFX and is applied to the projection by the engine (? exact site not traced).
     Embedded FSR3 CS (28 DXIL 6.5, MinimumPrecision): prepare inputs 0.77, prepare reactivity 0.88, luma instability 0.71, luma pyramid 0.18-0.39, accumulate+sharpen 0.8 (RX 9070 1080p) to 3.7 ms (Ally), RCAS 0.19 (fused in 3.1).
   - After it: optional **engine RCAS** `RcasRecord` 0x52e47c (PSO d3fd8774, 34 us), only on the XeSS/FSR4 path when XESS_SHARPNESS > 0 (gate 0x18d6bfd).
2. **Exposure histogram / eye adaptation** (CS 92075474, a1712355, 6ee82656; histogram 06054478 + 91f9db43).
3. **Bloom**: bright pass 7139b08f / prefilter 90e56d9e, 4-tap downsample 17149dca / c12e69fd (x4), 9-tap blur 18a4c2bb, upsample ab11dcd7 (x5), b5b4638c. All DXBC SM5 PS, VS c5909527.
4. **Motion blur** (MB2): velocity decode 615daafd -> tile max f11afe97 -> neighbour max 76a3c427 -> gather 4e13e236 (VS d8fc9faf). Sample cap from MOTION_BLUR (`ApplyMotionBlurQuality` 0x39b4a4: S+0x188c enable, S+0x1898 samples 2/4/6/8/10).
5. **DOF** (DOF3): CoC 51dd7477/b5dfef9e/745a18ff/baff8a76/e7318142, bokeh sprite CS 3dd31ae3/e3cb5cbe + sprite VS/PS 01da6d84/0ddd389c/aaee9c94, gather+composite cf792d56 (VS 523efaab), hex DOF.
   Functor names in the string table 0xae19f4..0xae1cb4: Default, DOFMerge, MGF, Once, SwitchTex, RenderStar, MRT, RenderLensFlare, RenderAnamorphicStar, RenderHexDOF.
6. **Lens flare / star** (796e2cdd, b94af32d), heat haze (9ea0865d, f3596c47), radial blur 278e3bad (gameplay effect only).
7. **Tonemap / compose**: 6a305be4 (light composite, no CA, VS d21b4b07) / 660f2f60, or the CA variants **04c1052d / e6efc8ff** (chromatic aberration loop 3-16 taps, dramatic LUTs, vignette, noise, mach band); HDR LUT CS 0c73c30e. Output in `CMP2OutputInternal`.
   There is no separate tonemap pass seen: tone mapping, LUT grading, CA, vignette and noise are folded into the compose PS (? inferred from the shader inventory). `SetupPostFxChain` 0x39b32c sets the CA / noise / vignette / DOF / flare / bloom / mach band slots.
   An earlier **CombineHDR** node (exec 0x1057ac, node 219) merges forward HDR layers before the upscaler.
8. **Toon edge** is not in Apply. `EdgePass` nodes 370/376 (TOONEDGE2: DrawEdge shader a127c98a, DrawEdge2 acad776d, CompositeEdgeRT 7bb9d36d) run full-screen every frame at render res, before the upscaler (0.17 ms);
   stencil bit 8 = outline pixel. Then Silhouette 383 / Highlight 385-387 (0.08 ms). Edges therefore get upscaled and anti-aliased by FSR together with the scene.
9. **UI composite**: after Apply, at output res, in the back-buffer-sized target (nodes 414-428). The UI does not go through the upscaler. With FG on, `SaveColorBeforeUiRendering` 0x3453364 copies HUD-less colour
   (CRenderer::m_pHudLessColor) and `CGraphicsDevice::m_pUIMask` is rendered; the FFX UI resource is registered (config type 0x30002) so FG can composite UI over interpolated frames.

## 3. Render-target sizes and formats, and who sizes them
- **Render size**: `SetRenderRatioIndex` 0x1fde3e8 -> `g_RenderResolution` = table 0x39783d0[0x39787b0[SCR_REZ*16+idx]] (DLSS clamps idx to 2 or 5). Dynamic res is a no-op unless enabled; VSync ON halves the DRS target at 0x5AF7F8.
- **Render-res targets** (x0.42 pixels at 65 % scale at 1080p): G-buffer AB/NS/RS/RR (4 B/px each), velocity, D32S8, scene `064b746b`, HdrOutput colour, toon edge. **Fixed-size**: shadow pools, ESM 2048^2, froxels 160x90x64.
- **Output-size targets**: FSR output 0x45c4320, upscaled depth 0x45c4328/330, TAA3/CMP2/KTSS work, DOF/MB internals, back buffer. Scene colour RGBA16F 1920x1080 = 16.6 MB.
- **POSTEFFECT_RESOLUTION** (setting 36, slot 0x45BAFC0; STANDARD=0 -> flag 1): `XessConfigApply` 0x2998ce4 (S+0x20bd), `FsrConfigApply` 0x2998b4c (S+0x1fff), `DlssConfigApply` 0x39af18 (S+0x2091).
  In Apply at **0x6dbb6d**: if the upscaler is active and the flag is set, the post base size = upscaler *input* size (`g_XessInputSize` 0x45C3DA0) instead of the output size. It sizes the 8 post targets named in
  `PostTargetNameTable` 0x3c03d60 (CMP2TargetFXAA, CMP2OutputInternal, TAA3Work1/2, TAA3DepthWork1/2, TAA3OutputColorInternal, TAA3OutputDepthMapInternal). Each target has an absolute / ratio / inverse-ratio mode in the
  PostEffect3 technique data (array lVar23 + idx*0x24, mode bytes +8/+9, values +0xc/+0x10; the data was not located). Reallocated when the size changes. FSR3's 5 textures are render- or output-size only.
- **Formats**: the object-db formats of the post RTs were not found (the render-graph dump holds only name hashes). Known: scene HDR RGBA16F, G-buffer RTVs B8G8R8A8 / R16G16B16A16_UNORM / R11G11B10_FLOAT, depth D32_S8,
  FG UAV texture = display size in the back-buffer format. HDR output is FP16 scRGB or R10G10B10A2 + PQ (`SetColorSpace1`).
- **CPU setup**: `ApplyGraphicsSettings` 0x39a39c fills RenderConfig (RESOLUTION_SCALING, FRAME_GENERATION, POSTEFFECT_RESOLUTION, MOTION_BLUR ...) and calls `SetupPostFxChain`. The per-frame copy of the settings block
  (`RenderParamsAssign` 0x247950, "S"+0x4d0) is what Apply and the upscaler read (S+0x20d8 CustomDepthUpscale, S+0x20e0 HUD-less flag, S+0x1ff8 reactive mode, S+0x2058 FG mode).

## 4. Frame generation
- Setting FRAME_GENERATION `0x45BB008`: 0 OFF, 1 DLSS, 2 FSR, 3 XeSS (`FrameGenSettingWrite` 0x1fddfcc, `IsFrameGenTypeAvailable` 0x1fe3fa4: FSR always "available"; DLSS multiplier `SetDlssFrameGenMultiplier` 0x1fddf00).
  A change is seen by `DisplaySettingsWatcher` 0x1350210, which rebuilds the swapchain (`DisplayResetRebuildSwapchain` 0x226d140 picks the FG back end: DLSS 0x198D150, FSR 0x34692B4, XeSS 0x3420C98).
- **FSR FG** is the statically linked FFX frame-interpolation **swapchain proxy**: `FfxFiSwapChainInit` 0x37761e4 (2 command queues, 5 fences, IDXGISwapChain4), an "AMD FSR Interpolation Thread" and the
  **"AMD FSR Presenter Thread"** (`FsrFiPresenterLoop` 0x37778b4; `FfxFiPresentFrame` 0x3777098 / 0x37773e4: interpolated present, then real present; `FfxFiWaitFence` 0x370f41c paces via `g_FfxFenceWaitMode` 0x4BD2580).
  The presenter forwards the engine's (SyncInterval, tearing) as `(vsync, tearing && !vsync && !exclusive ? ALLOW_TEARING : 0)`. FI dispatch 0x3788ba4 ("Clear Reconstructed Depth Interpolated Frame" ...);
  FG prepare 0x20004 in `Fsr3UpscalerExecute` runs only when the FG ctx 0x47092b0 and FI swapchain ctx 0x47092b8 both exist. Inputs: upscaled colour, depth, MV, HUD-less colour, UI mask.
- FFX KTGL backend: interface table filled at 0x3719384, `fpExecuteGpuJobs` 0x371811c, create-resource 0x3715ccc (stores null and returns FFX_OK on failure), copy-job executor 0x3718db4
  (**unchecked null texture -> crash**, fg-proton-fix.md).
- With FG off none of this runs (no HUD-less/UI-mask copies, no FI passes). On the Ally: ~7 ms GPU per real frame plus one held real frame; FG pays only above ~50 fps base.
- The DLSS-FG and XeSS-FG back ends sit behind the same slot (Streamline; not traced here).

## 5. Present, swap chain, pacing, latency
- **Swap chain creation**: `DxgiCreateSwapChain` 0x2ee5a38, legacy `IDXGIFactory::CreateSwapChain` through Streamline (sl.interposer) -> OptiScaler dxgi proxy -> real dxgi. BufferCount 3, flags **0x842** =
  ALLOW_MODE_SWITCH | FRAME_LATENCY_WAITABLE_OBJECT | ALLOW_TEARING (the desc builder at 0x226d140 writes 0x42 when the DISP_MODE slot 0x45BAF5C == 1 and 1 otherwise; ? mapping to the final flags not traced). FLIP_DISCARD;
  QI to IDXGISwapChain3 (GfxDevice+0x4278); `MakeWindowAssociation(hwnd, 3)`. (Re)built by `RecreateSwapchainDevice` 0x2eea690 (ResizeBuffers keeps BufferCount, GfxDevice+0x4240).
  `GetFrameLatencyWaitableObject` is fetched every frame (0x4a5338) and never closed (one handle leak per call, memory-leaks.md).
- **SyncInterval**: `GfxDevicePresent` 0x3d62e4: `SI = (params bit0) ? 0 : [GfxDevice+0x424C]`. +0x424C is written only by the ctor default (1, at 0x2edfbbd), `DisplaySettingsWatcher` (VSync slot 0x45BAF3C; it can drop a change while a guard is set)
  and a setter 0x2eebb50 (no static caller). SI is 0 or 1, never 2. Hand-off: if `[d+0x3F80]==0` present inline, else `SetEvent(d+0x7720)` to the present thread `PresentThreadLoop` 0x1a82298
  (`Present(+0x7738, +0x773C)`); the main thread waits for the previous present with `SpinWaitFlagSleep0` 0x7d9e90 (Sleep(0) loop). Present chain on the test PC: SL -> RTSS OSD -> Steam overlay -> dxgi (the FSR FI proxy when FG is on).
  Known failure (M): on some launches SI stays 1, which gives Composed Flip (+7 ms, 8-20 % frames dropped). The ASI `VrrPresent` rewrites to (0, ALLOW_TEARING) in the dxgi hook; the one-PC A/B was DROP/inconclusive (vrr-present.md).
- **Frames in flight**: `GfxFrameSlotAdvance` 0x8bec48 after each Present: slot = (slot+1) % 3, then `GfxWaitFenceValue` 0x7d9e1c on that slot's fence = wait for frame N-2, so up to 2 GPU frames are queued.
  `SetMaximumFrameLatency` is **never called**: the waitable defaults to 1, and at frame begin (`RenderFrameBegin` 0x4a529c) the game thread polls `WaitForSingleObject(h, 0)` in a pause loop (0x4a5348; patch R1 makes it a 2 ms wait).
  That poll is the stricter limit and takes ~32 % of main-thread samples at times.
- **Latency markers**: `LatencyMarkerBeginFrame` 0x6b8d34 (Reflex/AntiLag/XeLL abstraction from 0x6b88dc), `xellSleep` at 0x4a538c, `SlPclPresentStart` 0x6b9014. Reflex needs NVIDIA_REFLEX on and Streamline.
- **Frame limiter**: `FrameEnd` 0x5acf80, QPC busy-wait 0x5ad229..0x5ad2bb (`QpcNow` 0x205530): spin until QPF/fps, running a scheduler job or `Sleep(0)` (`JobWaitStep` 0x916ebc) each pass. fps = G+0x614 = the **target tier**
  (30/60/120 from REN_FPS or the `DynFpsGovernorTick` 0x5af4e4 controller, which looks at CPU time only), **never the measured rate**: each frame advances `1/fps` of game time (`FrameDtSetup` 0x5ad5f8),
  so below the target the game runs in slow motion. The measured interval at [r15+0x24] is for stats only. The limiter fps is written by `FrameProfile_SetLimiterFps` 0x5af400.
  The pad sample is taken at the end of the logic frame, before the limiter (`UpdateDispatchB_CallInputKick` 0x5ab6cf), so the limiter wait adds input latency.
- Latency (PC, 60 cap, M): MsUntilDisplayed 25.8 ms with SI 1 composed flip, 6-10 ms with SI 0 independent flip (input-latency.md §3).

## 6. Keep vs replace in a new renderer
**Keep (the look depends on it):** the upscaler as an external module (FSR via ffx-api, reactive mask generated from an opaque-colour copy, render-res velocity, inverted depth, auto-exposure) with its sharpen plus engine-RCAS rule;
an output-res upscaled depth (CustomDepthUpscale) for DOF/MB; the **chain order** as captured (upscale -> MB -> bloom -> compose -> exposure -> CA compose -> heat haze -> eye-adaptation resolve; see Capture-check); toon edge **before** the upscaler; UI at output res after post;
the HUD-less + UI-mask copies for FG; fixed-step game time of 1/fps (or replace it deliberately, see win 3).
**Replace:** the runtime-compiled DXBC SM5 post shaders (no source on disk; the 5 bit-exact batched rewrites in gpu-post-runtime §2 are references); the one-flag POSTEFFECT_RESOLUTION (make each target's scale explicit);
the FFX KTGL backend and FI proxy swapchain (use the stock ffx-api D3D12 backend / FG swapchain, null-check every job); the busy-wait limiter; the present-thread plumbing; the waitable with no `SetMaximumFrameLatency`.
**Easy wins seen:**
1. Present with SyncInterval 0 + ALLOW_TEARING whenever VRR is on, driven by the display mode and not by a cached slot that can be lost (the `DisplaySettingsWatcher` guard).
2. Call `SetMaximumFrameLatency(2)` explicitly, wait on the waitable with a timeout instead of the pause loop, and close the handle.
3. Hybrid sleep-then-spin limiter to the deadline, and measured dt (or an accumulator) so a missed target is not slow motion.
4. Skip TOONEDGE2 when no stencil-8 pixel exists (0.05-0.2 ms); per-target post scale (bloom/histogram at render res, compose at output res: E 0.3-0.45 ms); CustomDepthUpscale -> plain depth copy where DOF/MB tolerate it (<= 0.06 ms).
5. FSR3 CS are MinimumPrecision DXIL: native fp16 + wave size for RDNA2/Ally (accumulate+sharpen is 3.7 ms there; E large, changes rounding).
6. Null-check every FG job and fall back when a UAV texture cannot be created, so a failed format cannot hit the copy executor 0x3718db4.

## Capture-check (package 12, 2026-10-10; table in 12-capture-check.md section 6)

Measured order on pc2, the Ally and the PC (shader digests in this note match the PS md5 prefixes in the PSO sets, so the ids are right; the order is not):
- **Measured order** (differs from the static list in section 2). After the FSR3 RCAS: upscaled-depth copy (5fccb795) -> **motion blur** (615daafd decode, 76a3c427 + a955f6c3 pyramid 960x540..240x135, f11afe97 / ee6f48b2 tile max at 34x19, 4e13e236 gather) -> **bloom** (7139b08f bright at 960x540, b5b4638c at 480x270, 17149dca down, 18a4c2bb 9-tap blur back up) -> **compose 6a305be4** -> exposure CS (d7040592, 6ee82656, a1712355) -> **second compose, CA variant** (04c1052d or e6efc8ff) -> heat haze (f3596c47, 9ea0865d) -> eye-adaptation resolve (d787dbd0 at 2x1 + CS 41393a1d) -> UI (`OffScreenRendering`) -> `VideoOut`. Exposure is not before bloom; MB is not after bloom.
- **Compose is two passes in one frame**, not "6a305be4 or the CA variants". The CA digest follows the scene (tour points 1 / 4: 04c1052d; 2 / 3: e6efc8ff); 660f2f60 never appears.
- **Bloom depth** is 6 down + 7 blur on Windows (RDNA2 and RDNA4), 4 + 5 on the Ally under Proton; `ab11dcd7` never appears.
- **Confirmed:** the upscaler runs first (FSR3 GENERATE_REACTIVE first, then PREPARE_INPUTS .. RCAS), post runs at output res (bloom starts at 960x540), UI after Apply at 1920x1080, `VideoOut` last, FSR3 output is one 1920x1080 RGBA16 surface.
- **Unverifiable / absent:** DOF, lens flare, radial blur (no CoC / flare digests drawn at the 4 tour points); TOONEDGE2 shaders appear in no capture.
- New format facts: post work targets at output res are R11G11B10F (3 RT + 1 UAV), the UI uses a 3840x3240 BGRA8 offscreen target; see 12-capture-check section 6.1.
