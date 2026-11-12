# Upscaler swap: where Nioh 3 calls FSR 3 / XeSS / DLSS, and what a better upscaler can buy (2026-11-12, offline)

This was static work only. Sources: `re/Nioh3.dump.exe` (pefile import/export read), `D:\nioh3-re\decomp` (0x18d8898,
0x18d66e0, 0x341f68c, 0x3420230, 0x2998ce4), the game folder, `optiscaler/apply.ps1`, and the notes cited inline.
The game was not run. Rows marked **(U)** are unverified.

## 1. What already exists (old news, do not redo)
- **The three upscalers are linked differently** (exe import table):
  - **FSR 3.1 is statically linked.** It is a custom FFX fork. The exe exports `ffxConfigure`/`ffxCreateContext`/
    `ffxDestroyContext`/`ffxDispatch`/`ffxQuery` as ord 108-112. There is no game FSR DLL to upgrade.
  - **XeSS is a real DLL import.** `libxess.dll` provides `xessD3D12CreateContext/Init/Execute` and `xessSetVelocityScale`.
    `libxess_fg.dll` and `libxell.dll` are imported the same way.
  - **DLSS goes through Streamline.** `sl.interposer.dll` is imported (`slEvaluateFeature`, `slSetTag`...) and loads
    `sl.dlss.dll` -> `nvngx_dlss.dll`.
- **The `amd_fidelityfx_*.dll` files in the game folder are not the game's.** They come from OptiScaler (Jun 29, the
  FSR 4.1.1 SDK DLLs) and are installed by `optiscaler/apply.ps1`. OptiScaler itself is `dxgi.dll` 0.9.4 (Jul 18). The
  playsession tools stash and restore this whole set (`tools/playsession/session_lib.ps1`).
- **Main PC: OptiScaler XeSS -> FSR4 is installed and was profiled.** The user setting is RESOLUTION_SCALING = XeSS and
  OptiScaler takes the XeSS inputs. FSR4 cost **0.81 ms at 1440p** on the RX 9070 (`gpu-profile.md`, WMMA passes
  bc8a53cc/2156f246). The research notes (`research.md` §4) say to use XeSS inputs: OptiScaler cannot hook the FSR 3.1
  inputs because of the custom fork, and DLSS inputs crash.
- **OptiScaler.ini today:** `Dx12Upscaler=fsr31`, `EnableXeSSInputs=true`, `Fsr4Update=false`.
  - `Fsr4Update` was set to false on 2026-10-24 to isolate the DXGI popup (`pc-crash-tracker.md`).
  - `dxgi-device-removed-rootcause.md` later rated OptiScaler as only ~5 % likely to be the cause.
  - **(U)** Which model runs now (FSR4 or FSR 3.1 from the 4.1.1 DLL) was not checked after that change. The 0.81 ms
    FSR4 profile is from 10-06.
- **Shipped EngineFix keys** (origin/main; game-dir ini lines 563-586):
  - `[ExtraPerf] XSHARP` (#67772, live) writes FSR3 RCAS sharpness to S+0x1F58.
  - `XRES` 1-4 (#67206, restart needed) scales render size by x0.9/0.8/0.7/0.6. It hooks `RenderDimsGet 0x5AC428` at 4
    call sites, including `XessConfigApply 0x2998CE4`. Preset 4 "Extreme" = XRES 3. The game-dir ini on this PC currently
    has `XRES=3`.
- **Audits already done:**
  - `fsr3-audit.md`: no FSR pass can be removed bit-identically, there is no double sharpen, and FG work is skipped
    when FG is OFF.
  - `gpu-resolution-audit.md`: at 65 % most of the frame does not scale with render size.
  - `engine-rewrite-options.md` row 9, "better upscaler at lower res", was judged settings advice for the PC only.
- **REN_RAT only takes values in steps of 5** (`ally-gpu-budget.md` §5): "67" is silently ignored. Use 65 or 70, or XRES.

## 2. Hook points (RVA into Nioh3.exe v2.02, image base 0x140000000)
| what | RVA | notes |
|---|---|---|
| `Fsr3UpscalerExecute` | 0x18d8898 | Callers: PostEffectApply 0x6da118 and the FSR-FG backend 0x34692bc. The desc is built at 0x18d8c82..0x18d8ff2 and `ffxDispatch(&g_Fsr3UpscaleCtx, desc)` is called just after |
| `Fsr3EnsureContext` | 0x346948c | Creates ctx `g 0x45c4300` (desc 0x45c4188, flags 0x45c4198, type 0x10000, DX12 backend 0x1000001) |
| `Fsr3DestroyContext` | 0x9a63a4 | wrapper; the export `ffxDestroyContext` is at 0x3715894 |
| ffx exports | Configure 0x18dfd50, Create 0x18dfd74, Dispatch 0x18dfdf4, Query 0x18dfe18 | These are exported, so an ASI can `GetProcAddress` them and detour them with no pattern scan |
| `FsrConfigApply` | 0x2998b4c | hard-wires enableSharpening=1, sharpness = RS+0x1f58 |
| FSR textures | opaque 0x45c4310, reactive 0x45c4318, output 0x45c4320, upscaled depth 0x45c4330 | from fsr3-audit |
| `XessExecute` | 0x18d66e0 | Called from PostEffectApply (call at 0x6daeb8) and `Wrap_XessExecute` 0x341fe60. Calls `xessSetVelocityScale(ctx, -w, -h)`, then `xessD3D12Execute`, then the engine RCAS 0x52e47c when sharpness > 0 |
| XeSS create / init | 0x341f68c (`xessD3D12CreateContext` -> ctx `g 0x45c3da8`), 0x3420230 (`xessD3D12Init`, params `g 0x45c3d60`) | also creates XeSS_ColorOpaqueOnly / ReactiveMask textures when S+0x20bf != 0 |
| XeSS exec params | `g 0x45c3db0..0x45c3dfc` **(U field map)** | color db8, velocity dc0, depth dc8, exposure dd0, responsive mask dd8 (=0), output de0, jitter de8/dec, exposureScale df0 = 1.0, reset df4, input w/h df8/dfc |
| `XessConfigApply` | 0x2998ce4 | Upscaler input size comes from RenderDimsGet 0x5AC428 (XRS site 2). **The main PC Preset 4 boot crash was at +0x2998daa, inside this function** (#218) |
| AMD driver-override probe | strings `amdxc64.dll` / `AmdExtD3DCreateInterface` at RVA 0x4110798, next to "AMD FSR Presenter Thread" | **(U xref)** Looks like the ffx-api driver-extension query compiled into the static fork. That would explain why gamegpu/TPU report that the Adrenalin FSR4 override works (`research.md` #5), even though `engine-rewrite-options.md` guessed it "probably cannot apply" |

**FSR3 upscale dispatch inputs** (type 0x10001; decomp 0x18d8898 L349-443 plus fsr3-audit §1):
- **Resources:** color = scene color, depth, MV, exposure (a game texture, null when AUTO_EXPOSURE is set), reactive
  (the generated mask only when RS+0x1ff8 == 2, otherwise null), T&C = null, an **extra fork-only resource = depth**,
  output = 0x45c4320.
- **Jitter** = `[R+0x1ef318/0x1ef31c]` (XeSS reads +0x1ef340/344).
- **MV scale** = (-renderW, -renderH): MVs are negated UV-space. Render size = locals 4224/4220. upscaleSize 0 = max.
- **Other params:** preExposure 1.0, frameTimeDelta = `param3+0xc` x 1000, reset = `param3+0x11`. Camera near/far/FOV
  come from locals 3c70-3c78 **(U)**. There is a 9-dword tuning tail after the flags.
- **Create flags:** DEPTH_INVERTED only (plus AUTO_EXPOSURE). No HDR, DRS or display-res-MV flags. maxRender = maxUpscale = target.

## 3. Per device: what can be plugged in
| device | today | plug-in options | verdict |
|---|---|---|---|
| **pc** RX 9070 (RDNA4), Win | game XeSS -> OptiScaler -> FSR4 | (a) keep OptiScaler, confirm FSR4 really runs with `Fsr4Update=false`; (b) no OptiScaler: game FSR + Adrenalin "AMD FSR Upscaling" override (reported to work; static-fork probe above) | **Already done.** The only open item is the Fsr4Update check, and dropping REN_RAT 75 -> 70/65 with FSR4 |
| **pc2** RX 6650 XT (RDNA2), Win | game FSR 3.1 at REN_RAT 65 | (a) XeSS: swap `libxess.dll` (real import), DP4a path; (b) OptiScaler FSR4 **INT8** (`Fsr4ForceEnableInt8`, unofficial on RDNA2) | Neither is proven to beat FSR3. XeSS DP4a was slower on the Ally (+0.8 ms); INT8 FSR4 cost on RDNA2 is **unmeasured** |
| **Ally** Z1 Extreme 780M (RDNA3), Win + Proton | game FSR 3.1 at REN_RAT 50, 1080p | (a) XeSS: measured 33.3 vs 34.2 fps (**-1 fps**); (b) OptiScaler FSR4 INT8 on RDNA3, needs `WINEDLLOVERRIDES="dxgi=n,b;dinput8=n,b"` under Proton **(U)**; `PROTON_FSR4_UPGRADE` cannot reach a static fork **(U)** | FSR3 is the cheapest upscaler here. FSR4 INT8 is the only quality upgrade and is expected to cost more than FSR3; one A/B is needed |
DLSS is not usable on any of these devices (all AMD).

## 4. Estimated fps at lower internal res (every number is sourced; E = estimate)
- **pc:**
  - XRES 0.9 = 116 vs 109 fps (+6 %), and XRES 0.7 = 118: the PC is **CPU-capped at ~118** (`.handoff/next.md` 2026-11-05 2:30).
  - Ultra 35 % gave GPU -0.83 ms and **0 fps** (`ultra35-halfrate-pc-127.md`).
  - FSR4 at 67 % vs 75 % is E -0.7 to -1 ms GPU (`engine-rewrite-options.md`).
  - **Realistic gain: <= +6 %, and only in GPU-bound scenes.**
- **pc2:**
  - XRES 0.9 = 50 -> 53 fps (+7 %), look ~same (#239, next.md 2026-11-05 3:32).
  - Extreme (XRES 0.7 + cuts) gave +29.5 % / +28.4 %, vs +3.9 % without XRES. The x0.7 render size is most of that gain (next.md 2026-11-04 11:12).
  - FSR3 costs 1.26 ms/frame (`engine-rewrite-options.md`). 65 -> 60 % is E 1.2-1.5 ms (`gpu-resolution-audit.md`).
  - For FSR4 INT8 at 55-60 % to break even, it would have to cost less than FSR3 + ~1.3 ms. **Unknown, so measure before building.**
- **Ally:**
  - REN_RAT 50 = 34.2 fps, REN_RAT 100 = 18.0 fps. FSR3 ms fits `2.56 + 4.43*r^2` (`ally-gpu-budget.md` §5).
  - Extreme (XRES 0.7 + cuts) = 42.1 -> 58.5 (+39 %) and 45.1 -> 60.7 (+35 %) (`done.md` #224), with a softer image and a brown sky.
  - XeSS swap = -1 fps.
- **Look caveat:** GI, SSR, AO and the fog composite are sized from render resolution (`engine-rewrite-options.md` §5). A
  better upscaler restores edges, not those buffers. **"Same or better look at lower res" is plausible only for FSR4 on
  the pc.** On pc2 and the Ally it is a look trade-off.

## 5. Build plan (each step is gated on the one before)
1. **pc, config only, no build.** Read the OptiScaler overlay or log to see which upscaler runs with `Fsr4Update=false`.
   - If it is FSR 3.1, set `Fsr4Update=true` again. Watch for the DXGI popup: `pc-fgon-device-removed.md` arm B is the rollback.
   - Then run an ABBA of REN_RAT 75 vs 70 with FSR4 plus shotdiff. Expect <= +6 %, because the PC is CPU-capped.
   - Risk: the DXGI popup history.
2. **ffx counter ASI (diagnostic, fsr3-audit §4).** Detour the exported `ffxCreateContext`/`ffxDispatch` and log desc
   types, render/output size, jitter and flags per minute.
   - Zero look change. This also proves the field map of the fork's 0x10001 desc on real frames.
   - Do the same for XeSS: hook the `xessD3D12Execute` IAT and dump the param block. That turns the **(U)** rows in §2
     into verified ones.
3. **pc2 + Ally cost probe, no code.** Install OptiScaler with XeSS inputs, `Fsr4ForceEnableInt8=true` and in-game XeSS.
   - Run ABBA: FSR3 @ 65 (pc2) / 50 (Ally) vs FSR4-INT8 @ the same %, then FSR4-INT8 @ one step lower. Use PresentMon,
     GP2 upscaler rows and shotdiff.
   - On the Ally, add `dxgi=n,b` to the existing launch override.
   - Risks: vkd3d-proton may lack fast int8 dot / WMMA (so a large cost), and OptiScaler on Proton is untested here.
   - **Stop if the INT8 cost exceeds the saved render ms (~1.3 ms on pc2, ~1-2 ms on the Ally).**
4. **Only if step 3 wins, and the user does not want OptiScaler:** build an EngineFix "FSR3 -> FFX 4 bridge".
   - Detour `ffxDispatch` type 0x10001 and translate the fork desc to public `ffxDispatchDescUpscale`.
   - Drop the extra depth resource and the tuning tail. Pass the MV scale (-w,-h), jitter, reset, frameTime and
     DEPTH_INVERTED through.
   - Dispatch into a separately created FFX-SDK 2.x context loaded from `amd_fidelityfx_upscaler_dx12.dll`.
   - Mirror create/destroy on `Fsr3EnsureContext`/`Fsr3DestroyContext` (size changes, FG mode).
   - Risks:
     - The fork's private resource 0x33 feeds `DebugFSR3CopyUpscaledDepth` -> 0x45c4330, which post reads. That copy
       must keep working, or the upscaled depth must be produced in some other way.
     - FSR-FG shares this path (0x34692bc).
     - The fork's tuning tail is unknown.
     - Command-list state after a foreign dispatch: restore root signature and descriptor heaps, as gpu-barriers notes.
     - Licensing: FFX SDK DLLs are MIT/AMD-redistributable, but FSR4 runs officially only on RDNA4. OptiScaler is GPL-3.
       **Do not bundle either. Load them from a user-installed folder, as apply.ps1 does.**
   - Effort: 3-5 agent-days.
5. **XRES on the XeSS path (pc).** Fix the Preset 4 boot crash at +0x2998daa (XRS site 2 in `XessConfigApply`) before
   any "FSR4 + XRES" preset ships on the pc. Main-PC users run XeSS -> OptiScaler, so this is the path they hit.

**Bottom line:** the "better upscaler" is already in place on the only device that can use it (pc, FSR4 via OptiScaler).
The PC is CPU-capped, so lower internal res buys <= +6 % there. On pc2 and the Ally, the measured fps lever is render size
(XRES / REN_RAT), not the upscaler. FSR4 INT8 needs a cost A/B before any code is written.
