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
