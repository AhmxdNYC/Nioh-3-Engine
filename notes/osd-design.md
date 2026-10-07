# In-game overlay (OSD): FPS counter + startup popup (static design + implementation, 2026-10-07)

Requested by a ROG Ally (Bazzite / gamescope) user who can't read the log in Game Mode. Wanted:
1. An always-on counter, top-left: `Real FPS: 40 | FG FPS: 80` (or `FG: off`), with optional frame time and VRR speed factor (`1.00x`).
2. A 5-10 s startup panel: mod loaded + version, fixes applied/skipped, frame generation actually active (and which), VRR + cap, a warning when "FPS dynamic adjustment" is ON, and Proton detection with the L1/F1 state.

MangoHud and the Steam overlay only see presented frames, and gamescope composites only the game's swapchain. So the text has to be drawn **into the game's own frame by the mod**.

**Decision (user direction, 2026-10-07): approach C, our own D3D12 overlay.** It is implemented in
`mod\Nioh3EngineFix\src\osd.cpp`, `osd.h`, `osd.hlsl`, `osd_shaders.h` and `osd_font.h`. It compiles clean at /W4 with `build.bat`.
dllmain.cpp is **not** modified; the lines to add are in §5. Nothing has been run in-game yet, because Smart App Control blocks unsigned DLLs on the dev PC.
The GPU half was verified offscreen with the exact shader bytes, font and pipeline (§6).

---------------------------------------------------------------------------------------------------
## 1. Approaches considered

### A. Built-in KTGL debug FPS display: exists, but dead in retail
- `0x2EE8498` (named `DebugFpsOverlayFormat`, aka KtglDebugPerfOverlay) is a full KTGL perf overlay. It prints
  `-- KTGL For DX12(x64)[2.17.0] CS[sRGB|Linear] --`, `PU CLCK`, `V  WAIT` and `FPS %2.2f(%4.2f)`, and at level 2 also the platform info and PSO counts.
  The text goes into a **debug text console** at `GfxDevice+0x76F0`, and the level is `GfxDevice+0x76F8`.
  - The function saves and restores the render state around the overlay and writes KTGL tokens into `GfxDevice+0x3C40`.
  - It is called each frame from `RenderFrameDebugOverlay 0x4A361C` ← `RenderWaitForFrame 0x4A512F`, and only when `+0x76F0 != 0`.
- The console API is `DebugTextPuts 0x1D46F8`, `DebugTextPrintf 0x2EF596C` (vsnprintf into 0x400), `DebugTextPutsLine 0x2EF59DC` and `DebugTextPutChar 0x1D4934`.
  The colour is at console `+0x2C` (ARGB).
- **The only writer of `+0x76F0/+0x76F8` with a non-zero value is the leaf setter `0x2EEBEE4`.**
  It has no references at all (no rel32 call or jump, no absolute pointer in the dump), so the console object is never created in retail.
  The other writers are the ctor at `2EE0080` and the per-frame reset of row/col in RenderWaitForFrame.
- Switching it on would mean building a `DebugTextConsole` ourselves (font page table `+0x38/+0x40`, device `+0x48`) and calling the setter.
  We don't know the font page layout, and the text would sit in the pre-frame-generation image (it gets interpolated).
  It also shows the KTGL numbers, not ours. **Rejected.** The symbols are in `re\symbols\incoming\osd.tsv`.

### B. The game's UI text renderer: possible in principle, not worth it
- The UI text path (`UiElementWalk 1CE818` → `GlyphRasterize 61EA0` / `GlyphCacheAllocSlot 62000`, `UiQuadBuild 1CD690`, `RendererVtbl+0x158` GetTextRenderer) is **element-driven**.
  Glyphs are produced while walking UI element trees, with font pages, palette keys and a per-frame shelf atlas that is wiped and can overflow.
  There is no "draw string at x,y" entry point. We would have to fabricate UI elements on the main thread, inside the UI pass, with unknown types.
- It would also render **before** frame generation, so FG FPS would sit inside interpolated frames, and it would compete for the 1024-slot atlas (overflow drops the rest of the frame's text).
  **Rejected.**

### C. Own D3D12 overlay at the bottom-most Present: chosen
Self-contained, engine-independent for the drawing, and correct with frame generation: it draws on every presented frame, real and generated.
It works the same on Windows and on Proton (DXVK dxgi + vkd3d-proton).

---------------------------------------------------------------------------------------------------
## 2. Design (as implemented in osd.cpp)

### 2.1 Hook
1. **When:** a background thread (`InstallThread`) waits until the engine renders. On the known build that means `G=[base+0x4759910]` exists and `G+0x610` has advanced by 30 or more frames, which implies the device and swapchain exist. Timeout 300 s.
   On an unknown build it waits for d3d12/dxgi to load and then another 10 s.
2. **Getting the vtable:** load **`%SystemDir%\d3d12.dll` and `%SystemDir%\dxgi.dll` by full path**.
   - Windows: the real DXGI, even if a wrapper `dxgi.dll` (OptiScaler, ReShade) sits next to the exe.
   - Proton: DXVK's dxgi and vkd3d-proton.
   Then create a dummy flip-model swapchain (64×64, 2 buffers) on a hidden window: `D3D12CreateDevice(default adapter)` returns the per-adapter singleton, so this is the game's device. Read its vtable, release everything, destroy the window.
3. **Patch the shared vtable:** slots 8 `Present`, 22 `Present1` and 38 `SetColorSpace1`, using `VirtualProtect` and an atomic pointer swap, with the original published first.
   All swapchains of that class share the vtable, so this catches the **bottom-most** Present:
   - **FG off:** the engine's present thread (`PresentThreadLoop 1A82298` → `swapchain->Present`, through Streamline on Windows).
   - **FSR FG on:** the FidelityFX FrameInterpolationSwapChain's real swapchain, presented from the **"AMD FSR Presenter Thread"** (`FsrFiPresenterLoop 37778B4`; F1 wait at `3777A40`), once per real **and** generated frame.
     The FFX proxy's own Present (code in Nioh3.exe) is not hooked, which is intended.
   - Steam overlay and RTSS inline-hook the function bodies. We call the original pointer, so they stay in the chain and draw after us.
4. **Re-entrancy:** a `thread_local` depth counter means only the outermost call draws and counts (DXVK's Present calls Present1 virtually). `DXGI_PRESENT_TEST` is ignored.
   `TryAcquireSRWLockExclusive`: if another thread is drawing, the frame is presented without the overlay. The hook never blocks.

### 2.2 Drawing (per hooked Present, before calling the original)
- **Queue:** `IDXGISwapChain::GetDevice(ID3D12CommandQueue)` returns the queue the swapchain was created with. With FG on, that is FFX's present queue.
  It must be `DIRECT`, otherwise the overlay is off for that swapchain. The device comes from `queue->GetDevice`. The queue is re-checked every frame, so a new swapchain at a reused address triggers a rebuild.
- **Our objects** (per swapchain):
  - fence + event
  - 4 command allocators (ring) + 1 command list
  - an RTV heap of 4
  - one persistently mapped upload buffer: the font constant buffer (6 KB) + 4 × 48 KB instance regions
  - a root signature v1.0: root CBV b0 (font) and 4 root constants b1 (1/W, 1/H, scale, brightness)
  - a PSO per back-buffer format: RGBA8, BGRA8, RGB10A2, FP16, and the sRGB variants
- **Per frame:**
  1. `GetCurrentBackBufferIndex` → `GetBuffer`.
  2. Build the quads (CPU, at most 2048).
  3. Wait for the ring slot's fence. If it is not done within 50 ms, skip the overlay this frame.
  4. Barrier `PRESENT→RENDER_TARGET`, create the RTV, one `DrawInstanced(4, n)` triangle strip, barrier back to `PRESENT`.
  5. `ExecuteCommandLists` on the swapchain's queue, then `Signal`, then release the back-buffer reference.
- **No back-buffer reference is held between frames**, so the game's `ResizeBuffers` can never fail because of us, and no ResizeBuffers hook is needed.
  Our commands run on the present queue ahead of the Present, so any flush the game or FFX does before a resize covers them.
- **The swapchain is never AddRef'd**: holding it would keep the HWND bound and break re-creation when FG is toggled.
  When another swapchain presents and ours has been quiet for 1 s or more, the context is torn down (wait on our fence, 500 ms) and rebuilt. Swapchains that can't be used (D3D11, non-direct queue) go on a small blacklist.
- **Shaders:** `osd.hlsl`, precompiled to DXBC SM 5.0 by `tools\gen_osd_shaders.bat` (fxc) into `osd_shaders.h`. There is no runtime d3dcompiler dependency, which matters on Wine.
  Each instance is `{float4 rect, uint glyph, uint color}`. Glyph `0xFFFF` is a solid panel quad.
  - The PS reads 4-bit glyph coverage from the constant buffer and adds a 1-texel black outline (max of 8 neighbours), so text stays readable on bright and dark scenes.
  - Blending is `SRC_ALPHA / INV_SRC_ALPHA`; destination alpha is kept.
- **Font:** `osd_font.h`, generated by `tools\gen_font.py` (GDI rasteriser) from **Inconsolata** at weight 400, -15 px: 8×16 cells, ASCII 32..126.
  Inconsolata is under the SIL OFL 1.1; attribution is in the header. The bold weight turned `m` into a blob at 8 px, so 400 was chosen.
- **HDR:**
  - FP16 back buffers (scRGB, 1.0 = 80 nits): ×2.5.
  - R10G10B10A2 with `SetColorSpace1(G2084_P2020)` (PQ): ×0.55, about 150-200 nits instead of 10,000.
  - `[Overlay] Brightness` multiplies on top.
- **Scale:** `[Overlay] Scale` × auto factor (×1 up to 1440p, ×2 at 4K). Integer scales look crisp; non-integer ones give uneven texels.

### 2.3 Fail-safe
- Every D3D/DXGI call is checked. A failure logs `OSD: <call> failed (hr …)` and disables the overlay for that swapchain; the hook keeps passing through.
- `OnPresent` runs under SEH (`__try`). An exception disables the overlay for the session and is logged.
- More than 16 context builds disables the overlay.
- If the hook can't be installed (dummy device or swapchain creation, VirtualProtect), the log says so and the patches are unaffected.

---------------------------------------------------------------------------------------------------
## 3. Data sources (what each number is)

| shown | source | notes |
|---|---|---|
| **Real FPS** | Δ`G+0x610` (engine frame counter, +1 per simulated and rendered frame) over 0.5 s windows, `G=[base+0x4759910]` | the engine's own frame rate; independent of FG and of the VRR patch |
| **FG FPS** | number of hooked (bottom-most) Present calls on the active swapchain per second | includes generated frames; shown only while FG is detected |
| **FG active?** | `ratio = presented / real`; on when > 1.4, off when < 1.2 (hysteresis), real > 5 fps | runtime evidence, not the setting |
| **FG type** | the presenting thread's name contains "FSR"/"FrameInterpolation" (`GetThreadDescription`) → FSR; else `FRAME_GENERATION [base+0x45BB008]` (1 DLSS, 2 FSR, 3 XeSS); else `libxess_fg.dll` / `sl.dlss_g.dll` loaded; else "external" | popup: "FSR selected but NOT running" when the setting is on but the ratio is about 1 |
| **frame time** | `1000 / realFps` (window average) | |
| **speed factor** | `realFps × mean(vrr_scale / G[0x614])` sampled per present | game seconds per wall second. VRR: `G[614] = round(1/interval)` and `vrr_scale = fps·interval`, so the result is about 1.00. Stock: `real/tier`, e.g. 0.77 at 46 fps in the 60 tier. `vrr_scale` is 1.0 when VRR isn't installed |
| VRR | the `vrrDone` flag from dllmain; cap from `REN_FPS [base+0x45BAF54]` → {30,60,120}; limiter = `vrr_capCur` | |
| Dynamic FPS warning | `DYNAMIC_FPS [base+0x45BAF58] == 1` | |
| Proton, L1/F1 | `wine_get_version()` and the L1/F1 group state from dllmain | |

The engine globals are read only on the **known build** (TimeDateStamp check), through SEH-guarded reads.
On an unknown build the counter shows only `FPS: N` (presented frames) and the popup says that the engine readouts are off.

The F1 cave site (`3777A40`) was not used for counting: F1 is skipped under Wine by default, and the DXGI hook counts the same presents on every platform.

---------------------------------------------------------------------------------------------------
## 4. What the user sees

```
 Real FPS: 40 | FG FPS: 80 | 25.0 ms | 1.00x                    <- always (ini switches)
 Nioh3EngineFix v1.0.0 loaded - 10/10 fixes active              <- green; yellow + "(skipped: ...)" if any skipped
 Frame generation: FSR active (40 -> 80 fps)                    <- or "off" / "FSR selected but NOT running" / "measuring..."
 VRR timing: on - 60 fps cap (limiter 60), game speed 1.00x
 WARNING: 'FPS dynamic adjustment' is ON - set it OFF           <- only when ON (orange)
 Proton/Wine 9.0 detected - L1 off (Wine), F1 off (Wine)        <- or "Windows - L1 on, F1 on"
```
- The popup clock starts at the first hooked frame. The hook is installed only after 30 or more engine frames, so it never appears during the pre-render black screen.
- The popup appears after `StartupPopupDelay` (3 s), stays `StartupPopupSeconds` (8 s), fades in over 0.3 s and out over the last 1.5 s.
- It sits directly under the counter (above it for Bottom* positions).
- Each block has a translucent black panel. Colours: white text, cyan FG, green OK, yellow attention, orange warning.

---------------------------------------------------------------------------------------------------
## 5. Integration (exact edits; dllmain.cpp itself was left untouched)

**build.bat** (already done): compiles `src\osd.cpp` and links `build\osd.obj`. d3d12 and dxgi are loaded at run time, so the import table stays KERNEL32 + USER32.
Without the dllmain lines, `/OPT:REF` drops the OSD code and the ASI behaves exactly as before.

**dllmain.cpp** (a verified diff is in `D:\nioh3-re\osd\dllmain_integration.diff`; a compile+link check of the patched copy produced `D:\nioh3-re\osd\Nioh3EngineFix_osdtest.asi`):
```cpp
// 1. after #include "patches_gen.h"
#include "osd.h"

// 2. in Worker(), right after `int applied = 0, skipped = 0;`
    std::string skippedNames;  // OSD popup
    osd::State l1State = osd::State::Off, f1State = osd::State::Off;

// 3. in the `run` lambda
        if (!ok || !ApplyBatch(name, v)) { ++skipped; skippedNames += name; skippedNames += ' '; Log("%s: SKIPPED", name); return false; }

// 4. L1
    if (cfg.l1 && wine && !cfg.l1OnWine) { Log("L1: skipped under Wine/Proton (set [Patches] L1OnWine=1 to try it)"); l1State = osd::State::WineSkipped; }
        ...
        l1State = run("L1", v, ok) ? osd::State::On : osd::State::Failed;       // was: run("L1", v, ok);

// 5. F1
    if (cfg.fg && wine && !cfg.fgOnWine) { Log("F1: skipped under Wine/Proton (set [Patches] FGOnWine=1 to try it)"); f1State = osd::State::WineSkipped; }
        ...
        f1State = run("F1", v, ok) ? osd::State::On : osd::State::Failed;       // was: run("F1", v, ok);

// 6. just before   Log("done: %d group(s) applied, ...
    {  // in-game overlay: FPS counter + startup popup (osd.cpp)
        std::wstring iniPath = SelfPathWithExt(L".ini");
        osd::StartupInfo si;
        si.version = MOD_VERSION;
        si.log = &Log;
        si.iniPath = iniPath.c_str();
        si.base = g_base;
        si.knownBuild = known;
        si.applied = applied;
        si.skipped = skipped;
        si.skippedNames = skippedNames.c_str();
        si.wine = wine;
        si.vrr = vrrDone;
        si.l1 = l1State;
        si.f1 = f1State;
        osd::Start(si);  // copies everything; returns at once
    }
```
The early returns (Enabled=0, unknown build refused, second instance) also skip the overlay. That is intentional: no patches means no popup.

**Nioh3EngineFix.ini** (add; the defaults in code are the same, so missing keys behave like this):
```ini
[General]
; ... existing keys ...
; 1 = show a status popup for a few seconds once the game renders (mod loaded, fixes active, frame generation, VRR,
;     warnings). Drawn into the game's own picture, so it shows in Steam Deck / ROG Ally Game Mode (gamescope).
StartupPopup = 1
; StartupPopupSeconds = 8
; StartupPopupDelay = 3

[Overlay]
; FPS counter drawn into the game's picture: "Real FPS" = frames the game simulates/renders,
; "FG FPS" = frames shown incl. frame-generated ones ("FG: off" when frame generation isn't running).
FpsCounter = 1
; also show the real frame time (ms) and the game-speed factor (1.00x = real time; below 1 = the game runs slow)
FrameTime = 1
SpeedFactor = 1
; TopLeft, TopRight, BottomLeft, BottomRight
Position = TopLeft
; text size: 1.0 = 8x16-pixel letters (doubled automatically at 4K). Whole numbers (1, 2) look sharpest.
Scale = 1.0
; text brightness multiplier (HDR is compensated automatically)
Brightness = 1.0
```
Setting both FpsCounter=0 and StartupPopup=0 installs no hook at all.

---------------------------------------------------------------------------------------------------
## 6. Verification done
- `build.bat`: clean at /W4 /permissive- (C++17). The integrated copy also compiles and links clean.
  `dumpbin /imports` shows KERNEL32 and USER32 only, and the integrated build contains 41 `OSD:` log strings plus the DXBC.
- **GPU test** (`D:\nioh3-re\osd\gputest\osd_gputest.fsx`, Vortice.Direct3D12): it parses `osd_shaders.h` and `osd_font.h`, builds the same root signature (v1.0), input layout, blend state and PSO, and draws the counter and popup with the same instance layout.
  - Tested formats: RGBA8, BGRA8 at scale 2, RGB10A2 and FP16. Each read back with `DeviceRemovedReason = 0`.
  - Images are in `D:\nioh3-re\osd\gputest\*.png` (`rgba8_s1.png` is bold, superseded; `w400.png` and `s2.png` are current). The outline keeps the text readable on near-white and near-black backgrounds.
- **Not tested:** the hook and present path in the real game. Smart App Control blocks unsigned native binaries on the dev PC. **The first real run is on the Ally.**

## 7. Risks
| risk | likelihood | mitigation |
|---|---|---|
| Dummy swapchain on a hidden window disturbs gamescope or Wine | low (common practice, e.g. kiero-style hooks on Deck) | created only after the engine renders; never shown; destroyed immediately; failure is logged and the overlay skipped |
| `GetDevice(ID3D12CommandQueue)` unsupported on Windows DXGI | low-medium (works on DXVK by construction; widely used on Windows) | logged (`IDXGISwapChain::GetDevice(ID3D12CommandQueue) … failed`); overlay off, patches unaffected. Fallback if needed: hook `ID3D12CommandQueue::ExecuteCommandLists` (slot 10) and pick the queue that executes on the presenting thread |
| Our extra ExecuteCommandLists on FFX's present queue affects FG pacing | low (tiny list, about 0.05 ms GPU) | FpsCounter=0 + StartupPopup=0 removes the hook entirely |
| Back buffer not in PRESENT state at Present (non-conformant wrapper) | very low (required by D3D12) | debug layer would flag it; visually harmless on drivers |
| Another wrapper also patches the vtable slot | low | we chain to whatever pointer was there |
| Text inside screenshots/recordings | by design | ini switches |
| `G+0x610` is not 1:1 with rendered frames in some state (loading, cutscene ctx[1E6]) | low | only affects the Real FPS readout in those moments |

## 8. What to look for in Nioh3EngineFix.log on the Ally
```
OSD: FpsCounter=1 FrameTime=1 SpeedFactor=1 StartupPopup=1 Position=0 Scale=1.00
OSD: engine is rendering (... ms after start) - installing the Present hook
OSD: system d3d12 C:\windows\system32\d3d12.dll+0, dxgi C:\windows\system32\dxgi.dll+0
OSD: dummy swapchain ..., vtable ...dxgi.dll+XXXX
OSD: hooked Present: vtable slot ..., original ...dxgi.dll+XXXX        (also Present1, SetColorSpace1)
OSD: swapchain ...: 1280x800 fmt 28, 3 buffers, ... queue ... device ...
OSD: drawing context ready (build #1)
OSD: pipeline created for back buffer format 28
OSD: Present called from thread NNNN "AMD FSR Presenter Thread"      (FG on)  /  "" (FG off: engine present thread)
OSD: first overlay frame drawn (N quads)
OSD: stats: real 40.1 fps, presented 80.2 fps (x2.00, FG active), speed 1.00x, vrr tier 60 cap 60 armed 40, draws ..., skipped 0
```
Any `failed (hr …)` line names the failing call. The overlay then stays off for that swapchain and everything else keeps working.

## 9. Files
| file | what |
|---|---|
| `mod\Nioh3EngineFix\src\osd.h` / `osd.cpp` | overlay (hook, context, layout, stats, ini) |
| `mod\Nioh3EngineFix\src\osd.hlsl` → `osd_shaders.h` | shaders (DXBC SM 5.0, generated by `tools\gen_osd_shaders.bat`) |
| `mod\Nioh3EngineFix\src\osd_font.h` | 8×16 4-bit Inconsolata bitmap font (generated by `tools\gen_font.py`; OFL 1.1) |
| `mod\Nioh3EngineFix\build.bat` | + osd.cpp compile/link |
| `re\symbols\incoming\osd.tsv` | KTGL debug overlay / debug text console symbols (§1 A) |
| `D:\nioh3-re\osd\` | Ghidra query output (q1.log), displacement scan, integration diff/test build, GPU test script + PNGs, font source/preview |
| `mod\Nioh3EngineFix\src\menu.h` / `menu.cpp` | in-game mod menu (§10) |

---------------------------------------------------------------------------------------------------
## 10. Mod menu (added 2026-10-07): `src\menu.cpp` / `menu.h`

An in-game menu drawn through the same Present hook. It switches the runtime fixes, the overlay settings and the
next-launch options, shows live stats, and writes everything back to the ini. Builds clean at /W4.
`py -I tests\test_patterns.py` passes, including the new menu checks. It has not been run in the game yet; the first test is on the Ally.

### 10.1 Controls
- **Keyboard:** `[Menu] Key` (default **F10**) opens and closes the menu. Up/Down/Left/Right, Enter, Esc/Backspace.
- **Controller:** **hold `[Menu] HoldButton` (default Start) for `HoldSeconds` (default 5 s)**. A countdown note shows top-right after 1 s.
  A short Start press reaches the game untouched (its pause menu opens; the user accepts that).
  Inside the menu: D-pad/left stick, A, B; the hold gesture again closes. `HoldSeconds = 0` opens on a plain press of the combo (for `L3+R3` etc.).
- The menu opens at any time (gameplay, pause menu, title, loading). The only gate is that the overlay presented
  within the last 1 s, so it can actually be drawn.

### 10.2 Input plumbing ("use the game's own reads")
| path | how | blocking while open |
|---|---|---|
| controller | the game's IAT slot for `XInputGetState` (XINPUT1_4 ordinal 2, `0x38E2C50`; found by walking the import table, RVA as the known-build fallback) is swapped for `HkXInputGetState`, which calls the **previous** pointer (the Steam overlay's inline hook inside xinput stays in the chain) | the game gets a zeroed `XINPUT_GAMEPAD` (packet number +0x40000000 so the change is visible to packet-caching code) while open, and afterwards per slot until every button is released (no stray B/Start in the game) |
| keyboard | slot 9 (`GetDeviceState`) of the shared DirectInput device vtables, both A and W, from a throw-away keyboard device of `%System%\dinput8.dll`. Only 256-byte reads (the keyboard) are looked at; a thread-local depth guard handles Wine's A→W forwarding | the menu key is always zeroed; arrows/Enter/NumpadEnter/Esc/Backspace while open and until released |
| window messages | W3's `hookPeek` (PM_REMOVE only) turns `WM_(SYS)KEYDOWN/UP` of the menu key into `WM_NULL` (no F10 system-menu mode). The nav keys are dropped the same way while open | needs W3 installed |
| fallback | if a hook is missing, or the game hasn't fed it for 0.5 s: own XInput (`xinput1_4` → `1_3` → `9_1_0`, loaded dynamically; empty slots probed every 2 s only) and `GetAsyncKeyState` (menu key only while closed; foreground check), polled from the Present hook at ≤ 60 Hz | none (logged: "the game will also see ...") |

Edges are gathered in the input thread (`InterlockedOr`) and consumed on the Present thread, which does every state
change and every runtime switch write. Up/down/left/right auto-repeat: 400 ms, then every 90 ms.

### 10.3 Items
- **Live:** VRR (`vrr_enable`), W3 (`g_w3Enable`), W3 wait mode (`g_w3UseSleep`, Proton only), R1 (near `r1Timeout`, 0..16 ms, 0 = stock poll), L1 (`g_l1Enable`), F1 (`g_fgEnable`), WK (`wk_enable`).
  A fix that isn't installed shows `not installed` / `off (Proton)` with the reason in the description line.
- **Overlay:** FpsCounter, FrameTime, SpeedFactor, Position, Scale. These are osd's `g_cfg`, exposed as `osd::Overlay()`.
- **Next launch** (marked `* restart` when changed): WorkerCap (off / 6..31), P12, and under Proton L1OnWine / FGOnWine.
- **Save to ini:** `WritePrivateProfileStringW` on a background thread. It writes only the keys of installed fixes
  (an L1 skipped on Proton keeps its ini value). The result shows in the footer and in the log.
- **Stats line:** W3 waits/s, L1/F1 timer sleeps/s and WK kept/skipped per second, from the existing counters over 1 s windows.

### 10.4 Drawing
`menu::Draw` appends its quads after the counter/popup quads. The pixel shader derives font texels from one scale
constant, so `DrawFrame` now issues **two** `DrawInstanced` calls (counter scale, then menu scale, using
`StartInstanceLocation`). There is no shader change. `kMaxInst` was raised from 2048 to 4096.
Menu scale: auto `round(H/540 * 2)/2` (800p 1.5, 1080p 2, 1440p 2.5, 4K 4), reduced in 0.5 steps until the panel
fits in 94 % of the height and 96 % of the width. The panel is centred, `0xD8` black with gold rules and a blue
selection bar. The popup is hidden while the menu is open.

### 10.5 Pause reminder: engine stop flags (static RE, v2.02)
- `G+0x588..0x58D` is a block of 6 **latched stop flags**. `0x89B6EC` (called by FrameDtSetup with `G+0x588`)
  latches each one from a request counter at `G+0x590+4*i`: it sets on 0→non-0 and clears on non-0→0.
- `SetStopRequest(idx, on, n)` is at `0x3B28FC` → `0xA461E4` (counter += n or -= n, floor 0).
  The generic UI-menu open at `0x1ECDC1C` stores its stop type in `menu+0x3C8` and raises `SetStopRequest(type, 1)`; the close is at `0x1EB8E78`.
  The script command `Pause::SetFlag` (`0x23A763C`) uses index 2 (`0x58A`). `0x10FD6E0` toggles index 3 with direct `inc/dec [G+0x59C]`, and `0x10B2530` decrements index 4 (`G+0x5A0`).
  The WinMain exit path increments index 0.
- `GetStopState(G+0x588)` (`0x123A930`, wrapper `0x46518` with 90 callers) returns 4 for `58C`, 3 for `588/589/58D`, 1 for `58A`, 2 for `58B`, else 0.
- **FrameDtSetup zeroes the game time scale (`G+0x624`) only for `0x58B` and `0x58C`** (`0x5AD6B2..0x5AD6D5`).
  Those two are the direct "game time stopped" flags.
- String `MENU_ID_PAUSE_MENU` (index 97 of the MENU_ID name block at `0x3AB0xxx`) has no code references. Which stop
  type the pause menu passes was **not** pinned down statically.

So the reminder uses the direct flags: it shows when a flag in `[Menu] PauseHintFlags` (default `58B 58C`) has been
set for ≥ 250 ms, the frame counter `G+0x610` advanced within 300 ms (still rendering, not a frozen load), and none of
the "hard" stops `588/589/58D` is set (unless listed). It disappears at once when the flag clears. No time-scale or
delta heuristic is used: `G+0x624` is derived from these same flags.
**Every change of the 6-byte block is logged** (up to 200 lines) as `MENU: engine stop flags (G+588..58D) now: 58B` and so on.
The first Ally log therefore shows which byte the pause menu sets. If it isn't 58B/58C, set `PauseHintFlags` to that byte.
`test_patterns.py` verifies the FrameDtSetup / latch / GetStopState bytes and the XInput IAT slot.

### 10.6 Fail-safe
- Each hook body runs under SEH. An exception sets `g_disabled`, which makes every hook pass through, closes the menu and logs `MENU: exception ...`.
  The overlay and the fixes are unaffected.
- The input hooks stop blanking on their own if the overlay hasn't presented for 1.5 s, and the menu closes and logs it. A dead overlay can never lock the player out.
- If the Present hook can't be installed: `MENU: disabled: ...`, and no input hooks are installed.
- `[Menu] Enabled = 0` turns everything off. The Present hook is then installed only for the counter or popup.

### 10.7 What to look for in Nioh3EngineFix.log on the Ally
```
MENU: enabled - key F10, controller hold Start 5.0 s, scale auto, pause hint on (flags mask 18)
MENU: hooked the game's XInputGetState: IAT slot Nioh3.exe+38E2C50, chained to xinput1_4.dll+XXXX
MENU: hooked DirectInput GetDeviceState (W): vtable dinput8.dll+..., original dinput8.dll+...   (and/or (A))
MENU: own XInput for fallback polling: xinput1_4.dll+...
MENU: ready - open with F10 / Hold Start 5s
MENU: engine stop flags (G+588..58D) now: 58B          <- open the game's pause menu: note which byte appears
MENU: opened (controller gesture) - game pad and navigation-key input is held neutral while open
MENU: VRR timing -> OFF
MENU: save: writing 16 keys / save: 16 keys written to the ini
MENU: closed (B / Esc)
```
Problem lines: `NOT hooked ... ALSO reach the game` (fallback active), `exception ... mod menu disabled`, or `closed: the overlay stopped presenting`.

### 10.8 Build notes
`build.bat` compiles `src\menu.cpp`. `CMakeLists.txt` and the CI workflow now compile **osd.cpp and menu.cpp**:
both were missing osd.cpp, which dllmain has called since the OSD integration, so their link would have failed.
Imports are still only KERNEL32 + USER32; dinput8 and xinput are loaded at run time.
