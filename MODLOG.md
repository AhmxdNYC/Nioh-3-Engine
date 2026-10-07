# MODLOG — every change made to the game, and how to undo it

| When | Change | Undo |
|---|---|---|
| 2026-10-06 13:34 | Backup of `%LOCALAPPDATA%\KoeiTecmo\NIOH3` (config.xml + saves) + Nioh3.exe sha256 → `backups\20261006-1334-pristine\` | — |
| 2026-10-06 ~14:10 | OptiScaler 0.9.4 installed into game folder: `dxgi.dll` (OptiScaler, signed), `OptiScaler.ini`, `amd_fidelityfx_{dx12,upscaler_dx12,framegeneration_dx12}.dll` (AMD-signed). Game's own libxess/libxell untouched (identical versions). ini: Dx12Upscaler=fsr31 + Fsr4Update=true (→ FSR4), XeSS inputs only, OptiScaler FG off. | `powershell -File optiscaler\apply.ps1 -Revert` |

Pending (not yet applied): config.xml → `configs\quality-60.xml` (Acer) or `configs\perf-120.xml` (AOC); in-game upscaler must be set to XeSS for OptiScaler to take over.
| 2026-10-06 14:15 | Main monitor DISPLAY1 refresh 60 ? 240 Hz (Windows display mode, user-approved: 'use my main'). | `dotnet fsi tools\modes.fsx set \\.\DISPLAY1 60` |
| 2026-10-06 14:16 | config.xml ? `configs\main-240.xml` (quality-60 base, REN_FPS 120, FG off, XeSS?FSR4 @75%). | copy `backups\20261006-1334-pristine\AppData-NIOH3\config.xml` back |
| 2026-10-06 14:40 | LIVE MEMORY PATCH (not on disk; gone on game restart): CPU fixes W1 (rep stosd zero-fill, 61BAC) + W2a-d (movq store, 1371D0 quad writer) � removes read-modify-write on write-combined GPU upload memory. | `dotnet fsi patcher\cpu_patches.fsx revert` or restart game |
| 2026-10-06 14:42 | LIVE MEMORY PATCH: variable-dt frame timing (P1/P2/P3x7/P4 + code cave) � game speed 0.90x -> 1.00x at off-target fps, 120 tier only. | `FT_ALLOW_WRITE=1 dotnet fsi patcher\ft_vrr_patch.fsx revert` (or `off`) |
| 2026-10-06 19:35 | GAME FILE: `shader_dx12\0x0104840c.file` replaced with the fast GHDM depth-reduction shader (A/B: +3% fps no FG, ~+11% with FG; same image). Applied by the panel (option "Fast shadow shader", on). Original in `gpu-shaders\backup\`. | `powershell -File gpu-shaders\apply_ghdm.ps1 -Revert` (game closed), untick in the panel, or Steam verify |
| 2026-10-06 19:20 | Tested and REVERTED (archive is back to shipped bytes): physics MaxThreadCount 2 -> 4 via append-and-repoint in `package\system.rdb` + `package\0x52482d03.fdata`. Optional panel option "Physics on 4 threads" (off). | `powershell -File data\physics_threads.ps1 -Revert` |
| 2026-10-06 18:00 | Installed MSI Afterburner 4.6.6 + RivaTuner 7.3.7 (MSI-signed, user request) for an FPS overlay. OSD only; no limiter, no overclock. | Uninstall from Windows Settings > Apps |
| 2026-10-06 18:20 | New live patches available: F1 (FG presenter wait, `patcher\fg_cave.fsx`), P12 (worker-pool penalty lift at boot, `patcher\pool_patch.fsx`, off by default). Both memory-only. Panel `panel\Nioh3 Panel.cmd` applies the chosen live fixes on every launch. | restart the game / untick in the panel |
| 2026-10-06 21:36 | CRASH observed (1 in ~60 launches today): sweep launch 'SSS ON -> OFF', fixes applied by the panel during loading. Fatal error 0xC0000005 at Nioh3+2526D1 (FUN_2526C8 writes cmd token 0x95 to ctx+0x3C40 command stream; caller 252510 <- 7D9B6C render-frame begin). No patch touches this code. To check: SSS off stock vs patched, and panel apply timing (loading vs in-game). | - |

## 2026-10-07 03:20 - ASI v3 (Ally OSD fix)
- OSD: when IDXGISwapChain::GetDevice(ID3D12CommandQueue) fails (Streamline sl.interposer swapchain under Proton, E_NOINTERFACE), draw on the game's DIRECT queue captured from an ID3D12CommandQueue::ExecuteCommandLists vtable hook (slot 10); device checked against GetBuffer(0)->GetDevice; retries up to 600 frames.
- Menu: MessageBeep on open/close.
- Ally zip rebuilt (WorkerCap = 10) and sent to the Ally agents on bridge channel nioh3 (file #5).

## 2026-10-07 03:50 - ASI: frame-generation status + game-path FG switch (memory only; config.xml only on an explicit Save)
- RE (static, notes\frame-generation.md): FRAME_GENERATION 0x45BB008 = 0 OFF / 1 DLSS / 2 FSR / 3 XeSS; RESOLUTION_SCALING 0x45BAFF8 = OFF/DLSS/FSR/XESS. FSR FG has no vendor/Wine/upscaler gate (IsFrameGenTypeAvailable 0x1FE3FA4). The game applies FG live: options item 0x126 -> SetOptionValue 0x12A2368 -> FrameGenSettingWrite 0x1FDDFCC + g_ConfigDirty 0x45C6F80; the per-frame watcher 0x1350210 posts 0x405 and rebuilds the swapchain (0x226D140 -> 0x2EEA690). The Ally's FG never ran because its config.xml had FRAME_GENERATION=OFF.
- Overlay popup: 'Frame generation: off in game settings' / '<type> active (real -> presented fps)' / '<type> on in settings but NOT running' + reason line; upscaler shown; log line 'OSD: game settings: FRAME_GENERATION ..., RESOLUTION_SCALING ...' at start and on change.
- Menu: GAME SETTING section - 'Frame generation' (calls the game's FrameGenSettingWrite, bytes verified; one change per 2 s), read-only 'FG running' and 'Upscaler'. New ini key [Menu] FrameGenToggle = 1 (0 status only, 1 switch on Windows / status only under Proton, 2 switch under Proton too, FG-on never persisted there).
- 'Save to ini' + FG changed from the mod menu: rewrites ONLY <FRAME_GENERATION> in %LOCALAPPDATA%\KoeiTecmo\NIOH3\config.xml (path read from the game, 0x4B736F0; game descriptor strings verified), never FG-on under Proton.
- tests\test_patterns.py: new 'fg:' checks (setter/gate bytes, 9 references, descriptors 50/54, config path). Untested in-game.
- Undo: config.xml -> copy config.xml.Nioh3EngineFix.bak back (game closed), or set Frame Generation in the game's options. The mod change itself: set [Menu] FrameGenToggle = 0, or replace the .asi with the previous build.
- Ally zip rebuilt (WorkerCap = 10 kept; FrameGenToggle = 1 added).
