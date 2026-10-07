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

## 2026-10-07 04:20 - draw-prep parallelisation study + work-elimination caves (nothing applied)
- Study: `notes\parallel-drawprep.md`. Verdict: moving draw recording onto the KIDS job workers is NO-GO as a live patch (single KtglContext / ordered token stream / state-shadow cache, per-draw side effects outside the stream, single dispatcher). Helper-thread prefetch NO-GO (in-line prefetch caves were neutral in r9; adds CPU on the Ally).
- New script `patcher\drawprep_elim.fsx` (memory-only, not run against the game yet): PP = page-pool usage sweep 2523FC every 4th frame (call site 252677 -> cave), FPC = per-frame cache for material FindParam (vtable slot 38FB9D8 -> cave). `apply` installs both with switches OFF; `on`/`off` to A/B. Offline: ALL PASSED (`re\scripts_draw\drawprep_elim_offline.txt`).
- Symbols: `re\symbols\incoming\parallel-drawprep.tsv` (24 rows, not merged).
- Undo: `dotnet fsi patcher\drawprep_elim.fsx off PP,FPC` (switch) / `revert PP,FPC` (original bytes + vtable pointer), or restart the game.

## 2026-10-07 04:xx - GPU savings round 2 (staged only, nothing installed)
- New: `gpu-shaders\ESM_fast.hlsl` + `build_esm.py` -> staged `gpu-shaders\replacement\0x1ca8bb39.file` (ESM shadow-map blur, records #2..#11: R+1 gathers instead of 2R+1 samples; interface parts byte-identical; offline emulation `emulate_esm.py` = 0 diff). Original backed up to `gpu-shaders\backup\0x1ca8bb39.file` (sha256 22103ae7...6dd54). Installer `gpu-shaders\apply_esm.ps1` (`-Status` / `-Revert`). **Game folder untouched** (status: ORIGINAL).
- Analysis + Ally frame model + ranking: `notes\gpu-savings-round2.md`.
- Undo (only if later installed): `powershell -File gpu-shaders\apply_esm.ps1 -Revert` (game closed) or Steam verify.

## 2026-10-07 04:30 - ASI 1.1.0 (v5): W6 present wait, experimental CPU-power patches, MaxFrameLatency, FPS counter fix, Proton FG guard (memory only; nothing run in-game)
- W6 (ini W6, default 0; Ally zip W6 = 1): 7D9EA2 `xor ecx,ecx; call [Sleep]` inside 7D9E90 `while ([ctx+7728h]) Sleep(0)` -> call caveW6 -> W6Wait: pause-spin W6_SpinUs (50) then WaitOnAddress(flag, 1 ms) rounds; waker 1A822D9 `mov byte [rbx+7728h],0` -> caveW6Wake (store, mfence, WakeByAddressAll when a waiter is registered). Live menu switch (off = one Sleep(0) per round, as stock). Stats: waits/s, avg wait us, slept/spun, wakes.
- Experimental ports, each its own ini key (default 0), v2.02 only, sites + original bytes + new `deps` verified before writing: B (gallop, 3BC17, live switch added), PP (252677), FPC (vtable slot 38FB9D8, [App]Game TID found by thread name once frames advance), AABB (6EA50), M1 (49131), M2 (5 rows), J1 (8 rows), X1 (435BB7), E1 (7E82B4 + 160B83), S1 (418F68). Live switches: W6 B PP FPC AABB M1 X1 E1; M2 J1 S1 next launch. Tuning: PP_Period, E1_N, W6_SpinUs, W6_TimeoutMs. 30 s `stats/s exp:` log line + menu `Exp/s` line. Nothing skipped. MASM caves compared instruction by instruction with the scripts' Iced caves; all five scripts' offline modes re-run: ALL PASSED.
- patches.json: new kinds `jmp` / `vptr`, fields callee / iat_disp_at / iat_name / vtarget, `deps` (21). tests\test_patterns.py: ALL 55 SITES OK, deps 21/21.
- [Latency] MaxFrameLatency (default 0; Ally zip 1): IDXGISwapChain2::SetMaximumFrameLatency on waitable swapchains, game value logged, re-checked every 2 s; live menu item game/1/2/3.
- OSD: presents counted per swapchain on every presenting thread (FSR FG behind sl.interposer: game chain + FSR presenter chain); FPS = rendered/presented, FG FPS = busiest chain; G+0x610 identified as the game-update TICK counter (add at 233B73, end of 2338B0) -> shown only as "(game N Hz)" when it differs.
- Proton FG guard: marker Nioh3EngineFix.fgmarker when the mod switched FG on and config.xml had OFF at start; OFF restored at exit (game's ExitProcess/TerminateProcess import hooks; DLL detach last resort) and at the next start before the game reads config.xml.
- Ally zip rebuilt: Desktop\Nioh3EngineFix-Ally.zip (WorkerCap = 10, W6 = 1, MaxFrameLatency = 1).
- Undo: ini keys to 0 / MaxFrameLatency = 0, or the previous .asi. config.xml backup: config.xml.Nioh3EngineFix.bak.

## 2026-10-07 04:40 - input-latency study + live IK/TS script (nothing applied; game not run)
- Study: `notes\input-latency.md`. It covers the frame budget from controller to photons, and the ranked same-look
  reductions. Findings:
  - The pad poll is kicked at the END of the logic frame (5AB6CF), before the QPC limiter, and consumed at the next
    frame's start (5ACAAA). So input is stale by the limiter slack, ~5-9 ms at a 60 cap.
  - The game never calls SetMaximumFrameLatency, so the waitable latency is 1. Under vkd3d-proton the waitable is
    released after vkWaitForPresentKHR, which likely serialises render and GPU on the Ally (MaxFrameLatency 1 vs 2
    A/B).
  - VSync ON (composed) costs the PC ~1 frame of present->display vs VSync OFF + VRR (PresentMon: 25.8 vs 6-10 ms).
- New script `patcher\latency_live.fsx` (memory-only, PC). IK moves the input kick to logic-frame start; TS adds
  timestamp probes K/C/L/W/B/P. `apply` installs everything with both switches OFF (stock); toggle with `on`/`off`.
  Offline check: ALL PASSED (6 sites vs dump, call targets, no branch into the sites, no overlap with patches.json).
- Helpers: `re\scripts_latency\` (vscan, peek, derive_lat_patterns, branch_into_sites). Symbols:
  `re\symbols\incoming\input-latency.tsv` (already merged by the symbol agent in cae0a2e).
- Undo (only if applied later): `set FT_ALLOW_WRITE=1 & dotnet fsi patcher\latency_live.fsx revert`, or
  `off IK` / `off TS`, or restart the game.

## 2026-10-07 05:xx - GPU: MAX-settings map + GI HIGHEST patch spec (static only, nothing installed or patched)
- Study: `notes\gpu-maxsettings.md`. Every quality setting mapped to its engine consumer, per-level values and passes (HIGH vs MAX). Finding: GI HIGHEST sets all four probe-GI gather scales to 1.0 (HIGH 0.6/0.5/0.35/0.6; table read every frame by GiScaleLookup 0x187A50) = +1.8 ms PC / ~+11-14 ms Ally; the other MAX steps are small (SHADOW HIGHEST = one flag, AO HIGHEST masked here, SSR x2 steps, MB 10 samples, LANDSCAPE ULTRA LOD 0.8/4000). Ally at MAX native 1080p modelled at ~55-80 ms GPU: 60 fps there is not reachable with same-look changes.
- New: `gpu-shaders\maxsettings_spec.py` (offline validator: v2.02 site bytes, tables re-derived from code operands, r5 dispatch-grid cross-check, preset proofs, unique AOB; ALL PASSED) -> `gpu-shaders\maxsettings_patches.json` (GI1 caveGiScale spec for the ASI agent; ASI src not touched).
- New: `gpu-shaders\ally_shaders.sh` - Linux status/apply/revert for the staged GHDM/ESM/WDO#28 swaps (sha256-checked, exact revert, refuses while Nioh3.exe runs; tested on a scratch copy only).
- Symbols: `re\symbols\incoming\gpu-maxsettings.tsv` (13 rows, not merged).
- Undo: nothing was applied. If ally_shaders.sh was used: `./ally_shaders.sh revert all` or Steam verify.

## 2026-10-07 04:50 - ASI 1.2.0 (v6): input latency IK + TS, MaxFrameLatency default back to the game's (memory only; nothing run in-game)
- IK (ini IK, default 0; Ally zip IK = 1): port of patcher\latency_live.fsx. 5AB6CF `call InputWorkerKick` -> caveIkKick (skipped while the per-frame latch is set), 5ACAAA `call InputWorkerWaitPadStage` -> caveIkPadWait (latch; IK: kick + KtglEventWaitFor(padStageEvent, IK_PadWaitCapMs = 2)). Switching off never skips a poll; the switch-on frame polls twice (documented). Live menu switch. Stats: kicks at frame end/start, input age K->C, wait timeouts.
- TS (ini TS, default 0; Ally zip TS = 1): probes K/C/L(5AD2BB)/W(4A52D4)/B(4A535E)/P(3D6552) into a 4096-entry ring; 30 s log line with K->C, C->L, L->W, W->B, B->P, C->C, P->P, K->P medians and input->display B(n+1)-K(n); menu row "TS input -> display" shows the median (every 2 s). TSb uses the R1-independent pattern; the pattern test checks it still matches with R1j written.
- patches.json: 6 sites + 5 deps (86C018 bytes, QpcNow, unused results/flags). Test: ALL 61 SITES OK, deps 26/26, coexistence OK. No branch lands inside the 6 sites (re\scripts_latency\branch_into_sites.fsx: 0 hits).
- MaxFrameLatency: the game never sets it (value 1); under vkd3d lowering applies only after a swapchain rebuild/restart - menu text and docs say so; Ally zip back to MaxFrameLatency = 0.
- Docs follow the scope rule (game-side only): README-ALLY A/B recipes run in the stock environment (no env vars, power profiles or in-game changes); latency A/B order: baseline, MaxFrameLatency 2, IK on.
- Ally zip rebuilt: WorkerCap = 10, W6 = 1 (confirmed on the Ally), IK = 1, TS = 1, MaxFrameLatency = 0.
- Undo: IK = 0 / TS = 0 (or the menu switches), or the previous .asi.