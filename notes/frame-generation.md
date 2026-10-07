# Frame generation (FG): setting, gates, live apply path (static RE, v2.02)

Status: 2026-10-07. Source: `D:\nioh3-re\decomp` + `patcher\ft_x.fsx` on `re\Nioh3.dump.exe` (RVAs, image base
0x7FF74F410000). Every address below that the mod relies on is checked by `mod\Nioh3EngineFix\tests\test_patterns.py`.
Symbols: `re\symbols\incoming\fg-toggle.tsv`. **Not live-tested**: no game run on the PC for this note.

## 1. The setting and its encoding

| Slot | RVA | Values (enum index) | Notes |
|---|---|---|---|
| FRAME_GENERATION (idx 54) | `0x45BB008` | 0 OFF, 1 DLSS, 2 FSR, 3 XeSS | config.xml strings are exactly `OFF DLSS FSR XeSS` (mixed-case XeSS); descriptor `0x45C8570 + 54*0x30` |
| RESOLUTION_SCALING (idx 50) | `0x45BAFF8` | 0 OFF, 1 DLSS, 2 FSR, 3 XESS | upscaler; table default 5 is out of range -> first run picks by vendor |
| DLSS_FRAME_GEN_MULI (idx 55) | `0x45BB00C` | 2X 3X 4X | DLSS only |
| VSYNC / NVIDIA_REFLEX / INTEL_XELL | `0x45BAF3C` / `0x45BB010` / `0x45BB014` | | written as side effects of the FG setter |

config.xml (`%LOCALAPPDATA%\KoeiTecmo\NIOH3\config.xml`; the game's own full path string is at `0x4B736F0`) is plain
XML, one element per key: `<FRAME_GENERATION>FSR</FRAME_GENERATION>`. The game writes it without BOM, CRLF, 4-space
indent; the loader is XPath `//KEY/text()` with a case-sensitive compare, so an unknown value falls back to OFF.
The Ally's file (2026-10-07) had `FRAME_GENERATION=OFF`, `RESOLUTION_SCALING=FSR`: **FG never ran there because it
was off in the settings**, not because something blocked it.

## 2. Gates (what can keep FG off)

1. **Availability gate** `IsFrameGenTypeAvailable` `0x1FE3FA4` (used by the setter and at load):
   DLSS -> Streamline caps `SlGetFeatureSupport()` (`0x6B8E88`, magic static at `0x46D7690`) `+0xF0` (DLSS-G);
   XeSS -> `g_XessFgAvailable` `0x45C3EAA`; **FSR -> always true** (no vendor, D3D12-feature or Wine check).
   If the type is unavailable the setter writes 0, so the slot reads OFF.
2. **Load-time validation** `ValidateUpscalerFrameGen` `0x1FDEC4C`: DLSS FG without caps+F0 or XeSS FG without
   `0x45C3EAA` -> FRAME_GENERATION = 0. DLSS FG forces Reflex on and VSYNC off. FSR passes untouched.
   Upscaler: DLSS needs caps+0x20, XeSS needs `g_XessAvailable` `0x45C3E40`, else re-picked by `0x1FE07D8`.
3. **No upscaler dependency for FSR FG.** `ApplyGraphicsSettings` `0x39A39C` (runs **every frame**: `0x2338B0 ->
   0x39B570 -> 0x39A39C`) sets up FSR FG (RenderConfig S+0x2054=1, S+0x2058=3, S+0x20E0 HUD-less, S+0x2064 UI mask,
   `g_FfxFenceWaitMode` `0x4BD2580` = 1) purely on FRAME_GENERATION==2. The PC ran FSR FG with RESOLUTION_SCALING=XESS
   (OptiScaler) - confirmed by the measured FG runs in the README.
4. **Per-frame pause flag** S+0x2055 (stored at `0x39A945`): set while a menu is open (`0x5A37D4` / `0xD63C0`, the
   check Nioh3Fix's "Menu Framegen" hook at `0x39A86C` overrides), the window is not visible (IsWindowVisible
   `0x39A884`), in title / movie / character-creation / logo modes (type checks via `0x85960` / `0x1C307FC`), an
   event-scene + EVENT_QUALITY condition (`0x39A890`), and `0x9A7834` / `0x561C80` / `0x39B04C`==0x19. So "FG on but no
   generated frames" in menus and cutscenes is expected engine behaviour (medium confidence on the exact meaning).
5. **Dynamic resolution** `0x5AF7F8` halves its fps target when VSYNC==1 and FG is FSR or XeSS.
6. **Swapchain creation** (below): the FG backend is chosen when the display reset rebuilds the swapchain. If that
   fails (e.g. under Proton/vkd3d-proton) the setting stays FSR but no FSR presenter thread exists. Community reports:
   on Steam Deck / Linux, enabling FG crashes the game and it won't start again until config.xml is deleted
   (`notes\community-2026-10-06.md` section 3: RPGSite, Proton #9473, vkd3d-proton #2845, still open).

No GPU-vendor, Proton/Wine or D3D12 feature-level check was found on the FSR path.

## 3. Live apply path (what the game's options menu does)

1. Options menu item id **0x126** -> `SetOptionValue(id, v)` `0x12A2368` (named CommitConfigValues before; it is the
   per-item setter: 16 callers, a switch on the id) -> `FrameGenSettingWrite(v)` `0x1FDDFCC` (call at `0x12A24C4`) and
   `g_ConfigDirty` `0x45C6F80` = 1 (`0x12A46AD`).
2. `FrameGenSettingWrite`: gate (1.) then slot = v; DLSS: Reflex=1, XeLL=0, VSYNC=0; XeSS: XeLL=1, Reflex=0; OFF/FSR:
   Reflex=XeLL=0.
3. Every frame, the display-settings watcher `0x1350210` (from `0x12399D0`) compares slots to cached copies
   (window object +0x1B0..0x1C8; FG cache +0x1C4) and posts window messages: 0x401 SCR_REZ, 0x402 MONITOR_ID,
   0x403 DISP_MODE, 0x404 HDR, **0x405 FRAME_GENERATION**, 0x406 (a byte at `[0x4BD08D8]+0x405`). VSYNC and
   SHADOW_QUAL changes are applied inside the watcher directly.
4. `WndProc` `0x878054`, message 0x405: window+0x13C = 5 (countdown), +0x138 = 1 (reset pending), +0x139 = 1.
5. The watcher counts the 5 down (one per call) and, unless `[0x475A420]+0x248/+0x249` is set (busy), runs the display
   reset `0x226D140`: waits until `0x3B2970` is idle (Sleep(1) loop), builds the swapchain description (VSYNC, 3 buffers,
   refresh-rate change flag, FG backend pointer by type: DLSS `0x198D150`, FSR `0x34692B4`, XeSS `0x3420C98`, OFF none)
   and calls `0x2EEA690` (swapchain/device recreate, 4.5 KB) -> then `0x226D364`.
6. `g_ConfigDirty` is consumed by `SaveConfigIfDirty` `0x283892C` (SaveConfig `0x1FE56A8`, then clears it), called from
   four option-screen functions `0x2826F60 / 0x282729C / 0x2827560 / 0x28276E4`. SaveConfig also runs at boot
   (`0xE6C53A`), after a windowed-mode move (WndProc) and from the first-run auto preset. No save on exit was found.

Conclusion: FG is **designed to change live**. The render config follows the slot every frame and the swapchain is
rebuilt by the engine's own deferred reset. The same path also runs for display mode, monitor and HDR changes. So a
mod can toggle FG exactly like the options menu: call `FrameGenSettingWrite(v)` and set `g_ConfigDirty`. Under
Proton, the rebuild with the FSR backend is the step reported to crash.

## 4. What Nioh3EngineFix does with this (1.0.0+, 2026-10-07)

* Overlay popup + menu: FG setting (OFF/DLSS/FSR/XeSS), upscaler setting, whether FG runs (presented/real > 1.4; FSR
  presenter thread seen presenting in the last 2 s), and a reason line when the setting is on but FG does not run.
  The log prints `OSD: game settings: FRAME_GENERATION ..., RESOLUTION_SCALING ...` at the first window and on change.
* Menu item "Frame generation": calls the game's `FrameGenSettingWrite` (its 93 bytes verified at start) + sets
  `g_ConfigDirty`, like the options menu. One change per 2 s. `[Menu] FrameGenToggle`: 0 status only, 1 = switch on
  Windows (status only under Proton), 2 = switch under Proton too, but FG-on is never marked dirty nor written to
  config.xml there, so a crash is undone by restarting the game.
* "Save to ini" also writes FRAME_GENERATION into config.xml when it was changed from the mod menu: only that value,
  after verifying the game's descriptor strings, with a copy to `config.xml.Nioh3EngineFix.bak` first, via a temp
  file + MoveFileEx. The game's own SaveConfigIfDirty would write the same value when its options screen closes.
