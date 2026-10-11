## 2026-10-11 Codex - FO98 native removal/retirement trace

Documentation/static provenance only: three swap-removal branches and locked retirement enqueue/fallback traced;4identity checks PASS. Lifetime/capacity/admission remain unproven; FO remains NO-GO. No installed game, defaults or performance change. See notes/handoff-codex-fo98-retirement.md.

## 2026-10-11 Codex - FO98 consumer registration trace

Documentation/static provenance only: native token98 registration and downstream object virtual calls traced;5identity checks PASS. FO remains NO-GO pending lifetime/admission/caller-mode proof. No runtime or performance claim, deployment or defaults change. See notes/handoff-codex-fo98-consumer.md.

# MODLOG — every change made to the game, and how to undo it

| When | Change | Undo |
|---|---|---|
| 2026-10-11 | Static FO emitter ownership trace: both sequences verified; mode1 shared/mode2 exclusive/other no-lock selectors resolved through PE imports. Caller modes/consumer/retirement remain unproven; no repair or performance claim. | Documentation only; installed1.7.61/FO0 unchanged. |
| 2026-10-11 | Experimental24494d5 adds actual-code epoch/lifecycle negative controls: 12 new witnesses, zero failures/compiler warnings; Restore/FlushAll pending-record gaps reproduced. No production repair, game test or FPS claim; FO held. | No installed game change; revert isolated audit commit if needed. |
| 2026-10-11 | FO lifecycle ordering and explicit admission/epoch/publication/closure/lifetime contract recorded; no repair, build, new runtime test or performance claim. Epoch audit witnesses next; FO remains held. | Documentation only; installed main1.7.61/FO0 unchanged. |
| 2026-10-11 | Read-only training inventory/prototype plan; 88 add-on sites, 121 catalogue rows, explicit spawn/reset safety gaps. Documentation only; renderer lifecycle work next. | No game change to undo; revert documentation commit if needed. |
| 2026-10-11 | Isolated experimental FO startup availability repaired3b89c04; 18 cases, pre-fix control, 255patterns and FO/stub harness PASS. No installed ASI/game changes. Handoff/ledger updated; FO release still held. | Keep main1.7.61/FO0; revert experimental commit if needed. |
| 2026-10-11 | Corrected Codex continuation stall caused by the lead's unrequested daily budget; caps now optional/null, actual usage still tracked, serial ownership/game guards retained. Controller13checksPASS; main ASI/game unchanged. | Pause Codex /stop and let active work finish; set numeric optional caps in runtime config if the user requests them. |
| 2026-10-11 | User-authorized Codex continuation/phone tooling in universal-optimizer94e62bd; ten controller checks, repo suites and live phone route PASS. Added task ledger/progress report and attributed incomplete API safety review. No ASI or game-setting change; Claude auto-wake remains disabled, no credential retained. | Phone Codex /stop; let active worker finish; disable CodexNiohContinuation and use monitor.stop. See universal tools/codex-autowake/README.md. |
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

## 2026-10-07 06:xx - GPU barriers: measured cost + D3D12 barrier-batching hook gpuopt (new files only, default OFF, nothing applied)
- Study: `notes\gpu-barriers.md`. RGP r5 with wave occupancy (`re\scripts_d3d12\barrier_cost.py`): 1360 GFX barriers/frame (1220 API) in 478 chains; 5.5 ms of barrier time is real dependency drain, only 0.56 ms fully idle; batching each chain into one ResourceBarrier(n) removes 786 calls and 0.19-0.26 ms idle GPU on PC (<= ~0.45 ms). The "75 particle dispatches each with a UAV barrier" is refuted (no barriers between them; the 100 us segment is an 87.5 us CP-DMA copy wait). Ally (vkd3d-proton -> RADV): RADV already coalesces consecutive barriers (lazy flush bits), so expect ~0-0.1 ms GPU there, plus dispatcher CPU; replaces the earlier 1-2 ms Ally estimate.
- New (not integrated; the ASI src owner integrates): `mod\Nioh3EngineFix\src\gpuopt.h`, `gpuopt.cpp`, `gpuopt_thunks.asm` - vtable hook of the D3D12 runtime's command-list class; defers the dispatcher thread's ResourceBarrier calls (engine / Streamline callers only) and emits them as one call before the next work/observing method; drops duplicate UAV barriers (and optional undo pairs); live switch, counters, resource-level trace mode (`Nioh3EngineFix.gpuopt-trace.csv`, analyser `re\scripts_d3d12\gpuopt_trace.py`). ini `[GpuOpt] Barriers = 0` (default) does nothing at all.
- Verified offline in a scratch D3D12 harness on this PC (D3D12Core 10.0.26100.9278, RX 9070): 20/20 checks incl. real submits; compile /W4 clean; scratch link with the mod's objects OK. Not run in the game.
- Symbols: `re\symbols\incoming\gpu-barriers.tsv` (18 rows: 13 barrier call sites, 4 handlers, token entry 0x8A; not merged).
- Undo: nothing applied. Once integrated: `[GpuOpt] Barriers = 0`, or the menu switch (live).

## 2026-10-07 05:25 - ASI 1.3.0 (v7): worker-wake counters, EffectDisp skip, GPU barrier batching integrated, IK wait fix (memory only; nothing run in-game)
- WakeStats (ini WakeStats, default 0; Ally zip 1): counters at the 6 SetEvent wake sites (40B0A 40D2F 48C6B 48D06 48DEC 48E3C), worker sleeps per pool at 1A6BC35, wake checks at 40A4C split by 40800's caller (3FE14 / 403A0 / 207200 / other, via the WK cave; WK site installed with its gate off when WK = 0). 30 s "stats wakes/s" line incl. wasted ~= sleeps - checks from 3FE14.
- ED (ini ED, 0 off / 1 on / 2 installed+off; Ally zip 2): EffectDisp kick (3D6F46) skips workers whose list ([obj+1A0h] vector) is empty; the join (3D6F8B) skips exactly those. Layout verified in the dump (vtable 402D768 slot 2 = 3D83BC, obj stride 1A8h, start +8, done +0A8h, push_back 73A784, empty -> epilogue 3D8421).
- WK2 not shipped: no queued-job count exists (lock-free stacks, only per-priority bitmaps), and the idle count can't separate woken-not-running from about-to-sleep workers (inc pool+30h at 1A6BBFC before bit0 at 1A6BC15). Reasons in README.
- IK: IK_PadWaitCapMs default 0 = no wait at the frame start (v6 timed out every frame at 2 ms); Ally zip IK = 0.
- gpuopt (another agent's src\gpuopt*.cpp/.h/.asm) integrated per notes\gpu-barriers.md 7: build.bat/CMake, gpuopt::Start from dllmain, InstallNow from the OSD install thread (SEH-guarded), OnFrame from the Present hook, menu item + GPU status row, 30 s "stats/s gpuopt" line. [GpuOpt] Barriers = 0 (main ini) / 2 (Ally zip).
- Not in this build: the in-process sampling profiler (SP).
- patches.json: 9 sites + 10 deps; test ALL 70 SITES OK, deps 36/36; no branch into the new sites.
- Ally zip rebuilt: WorkerCap 10, W6 1, IK 0, TS 1, WakeStats 1, ED 2, MaxFrameLatency 0, Barriers 2.
- Undo: WakeStats = 0 / ED = 0 / Barriers = 0, or the previous .asi.
## 2026-10-07 05:45 - ASI 1.3.1: SP sampling profiler
- New src\sp.cpp/.h: in-process sampler ([Profiler] SP, default 0; Ally zip 1). Suspends [App]Game every 1 ms + one engine worker round-robin, GetThreadContext + stack copy via ReadProcessMemory while suspended (no heap/loader work while suspended), then attributes RIP to the Nioh3.exe .pdata function, or for system code to the first game return address on the stack (call-site validated). Top-N per thread group logged every 30 s.
- Verified on the PC with a harness: a Sleep(0)-spinning thread named [App]Game -> '97.8 % [ntdll.dll] <- fn <its RVA>'; a busy worker -> 100 % its function.

## 2026-10-07 05:45 - ASI 1.4.0 (v8): GF GPU-fence waits (memory only; nothing run in-game)
- GF (ini GF, main default 0; Ally zip GF = 1; live menu switch): the Sleep(0) in FenceWait 7D9E1C's event == NULL loop (7D9E66) and in the worker fence poll 117D6C's p2 == NULL loop (117D9F) -> GfWait: per-thread auto-reset event (TLS) registered with ID3D12Fence::SetEventOnCompletion (vtbl+48h, HRESULT checked), then WaitForSingleObject(event, GF_TimeoutMs = 100). The engine's loop re-checks the fence after every return. Failure -> stock Sleep(0) for that round (counted). Off = stock.
- Stats: 30 s "stats/s GF:" line (waits/s, avg wait us, fallbacks); menu Exp/s line.
- patches.json: 2 sites + 5 deps (FenceWait registers, the loop re-checks, IsFenceComplete 1180A4, the 117D6C loop). Test: ALL 72 SITES OK, deps 41/41; no branch into the sites.
- Built on 1.3.1 (SP profiler, other agent). Ally zip rebuilt: SP = 1 kept, GF = 1 added; all other values unchanged.
- Undo: GF = 0 or the menu switch, or the previous .asi.

## 2026-10-07 13:40 - memory-leak study + MemStats module (new files only; nothing applied, game not run)
- Study: `notes\memory-leaks.md`. Ally MangoHud (4 sessions) and mod logs (70 min v7.1): no steady-state growth (v5 plateau GPU 0.0 MB/min, RAM+swap ~+1 MB/min; fps flat 16.35 +0.1/h over 70 min; thread census constant). Load-time capacity pressure on the 16 GB Ally (GPU memory 4.8 -> 6.8 GB in ~90 s, 0.75-0.85 GB swap-out bursts, 260-300 ms frames), not a leak. Repeated loads not yet measured (test plan B).
- Static: engine CPU memory = one 6 GiB MEM_COMMIT arena (294D214) split into 52 fixed pools (294C2E0) -> no unbounded commit growth; swapchain rebuild 2EEA690 balanced by its teardown 2EEC4C8.
- LEAK FOUND (Windows/PC only): 4A5338 calls GetFrameLatencyWaitableObject every frame and never CloseHandles it; DXGI duplicates on each call (scratch harness: +1000 handles / 1000 calls) -> 1 handle per frame (216k/h at 60 fps). Not active on the Ally (swapchain flags 2). Fix WL designed (call -> cave closing the previous frame's handle), not built.
- New (not integrated; the ASI src owner integrates): `mod\Nioh3EngineFix\src\memstats.h`, `memstats.cpp` - [Profiler] MemStats (default 0): 30 s "stats mem:" line (private, working set, commit, handles, threads, VRAM local/non-local used/budget via system dxgi CreateDXGIFactory1 + device LUID, system RAM, deltas). Compiles /W4-clean with build.bat flags; only KERNEL32/USER32 imports; verified in a scratch harness on this PC.
- Symbols: `re\symbols\incoming\memory.tsv` (11 rows; not merged).
- Undo: nothing applied.

## 2026-10-07 - ASI 1.5.0 (v9): WL waitable-handle leak fix, MemStats integrated (memory only; nothing run in-game)
- WL (ini WL, default 1; live menu switch): 4A5338 `call [rax+108h]` (GetFrameLatencyWaitableObject, every frame, never closed) -> caveWL: same call, then CloseHandle on the previous frame's handle (its wait loop is over: rbp's only use 4A534C, overwritten at 4A5461 - verified in the dump, deps in patches.json). Same value kept. No overlap with R1. No-op under Proton (getter returns NULL). 30 s "stats/s WL" line.
- MemStats (another agent's src\memstats.cpp): memstats::Start next to sp::Start, LogLine in the 30 s block, build.bat/CMake (CMake also gets src/sp.cpp, missing before). [Profiler] MemStats = 0 (main) / 1 (Ally zip).
- patches.json: 1 site + 4 deps. Test: ALL 73 SITES OK.
- Ally zip rebuilt: GF 1, SP 1, MemStats 1, WL 1; other values unchanged.
- Undo: WL = 0 / MemStats = 0, or the previous .asi.

## 2026-10-07 13:30 - CPU hotspots on the ROG Ally from the SP profiles (static only; nothing applied, ASI src not touched)
- Study: `notes\cpu-hotspots-ally.md`. Input: the v71 Ally log (146 SP reports, 139 of them steady MAX gameplay: 16.2 fps
  rendered, 44.6 game ticks/s). Ranked sinks: (1) [App]Game FenceWait Sleep(0) spin, 66 % of its wall time -> GF (1.4.0,
  ~0.6 core); (2) logic over-ticking, 2.8 logic ticks per shown frame -> TG tick governor (caveLim clamp + governor in the
  Present hook; zero-code A/B: in-game FPS cap 30), ~1.0-1.4 cores; (3) worker wake storms (wake-one 48C6B up to 150k/s on
  pool 1) -> WG pending-wake gate on the existing WS caves; (4) W3 on Proton -> W3_TimeoutMs = 3; small: IATS, DS, RL.
- Finding: the "DirectInput poll under 0x4E390" is a stale-slot attribution. 4E390 = ID3D12Resource::GetDesc().Width < n;
  the DINPUT8.dll rows are Ultimate ASI Loader's kernel32 IAT shim in front of FenceWait's Sleep(0) (r = 0.956 with the
  fence-wait rows); they vanish with GF. The game's own DirectInput reads cost ~0.
- Sweep: all 400 Sleep/SleepEx sites classified (`re\out\sleepclass.txt`); every site with any SP sample in 70 min listed
  with thread, wait target, blocking primitive and fix. Outside GF/W6/W3/L1 the spin/poll waste is < 0.5 % of one core.
- New: `re\scripts_cpu\sp_aggregate.py` (SP log aggregator), `re\scripts_cpu\cpu_hotspots_check.py` (offline proof of the
  TG/WG/DS/RL/IATS sites, bytes and register contexts: ALL PASSED), `re\scripts_cpu\sleep_classify.py`.
- Symbols: `re\symbols\incoming\cpu-hotspots.tsv` (20 rows, dry-run merge 19 added / 1 updated; not merged).
- Note: TG and the cap-30 A/B need L1 under Proton (`L1OnWine = 1`; it was on in v71, the ally-package ini says 0).
- Undo: nothing was applied.

## 2026-10-07 - worker/job CPU: collision + stage prefetch caves and locked-load rows (offline only; nothing applied, game not run)
- Study: `notes\worker-jobs-opt.md`. Ally SP aggregation: CollisionUpdate = 45 % and the StageUpdate placeable loop = 25.5 % of all KIDSTS job samples; object update = 32 % of KIDSTaskScheduler job samples. All hot worker RIPs are first-touch misses or `lock cmpxchg` loads; no redundant arithmetic found.
- New live scripts (memory only, default off, dry/apply/on/off/status/revert, FT_ALLOW_WRITE + safeWrite):
  - `patcher\collprefetch_live.fsx` CP: jmp at D8180 (CollisionObjectUpdate loop head) -> prefetch cave (colliders 3 strides ahead, their arrays 2 ahead, first 4 parts 1 ahead). Real D80B0 + callees run from the mapped dump on 300 synthetic worlds: 0 mismatches (175k change records + every world byte); -16 to -27 % loop time on this PC.
  - `patcher\stageprefetch_live.fsx` SPF: jmp at DB8BF (placeable-map loop of DA830) -> prefetch cave (placeable 4 ahead, its +B0h object 2 ahead). Transparency test 30,000 runs: 0 differences.
  - `patcher\lockload_live.fsx` LK: 18 `lock cmpxchg [m],zero` loads -> plain mov (+test) in 1A188C, 425B0, 4A2D0, 130014, 40800, 411C0 (beyond J1/M2). Dataflow/flag/StoreLoad proofs + native row test: 0 differences.
- ASI-ready MASM + patches.json rows/patterns in the notes §7 (src not edited). Helpers/outputs: `re\scripts_wjobs\`. Symbols: `re\symbols\incoming\worker-jobs.tsv` (38 rows, not merged).
- Expected: PC ~0.15-0.25 core, Ally ~0.12-0.2 core of worker CPU; no fps change expected.
- Undo (only if applied later): `set FT_ALLOW_WRITE=1 & dotnet fsi patcher\<script> revert` (or `off` for CP/SPF), or restart the game.

## 2026-10-07 - shadow caching + visibility/culling audit (static only; nothing applied, game not run)
- Study: `notes\shadows-culling.md`. Verdict: the engine already caches shadows (VSM page cache with valid-rect meshlet culling and a 16-tiles/frame budget, two-phase shadow occlusion with GenerateShadowMapHiZ, static/dynamic cascade list split, optional baked ShadowTree) and culls GPU-driven, two-phase, for every meshlet view. Per-frame shadow draws in r5 are ~200 direct draws of dynamic casters (characters) + 10 tree meshlet draws (~0.23 ms PC GPU, <=3-4 % main thread): a further "static cascade cache" would gain ~0.
- Largest structural items found (designs only, none a safe byte patch): GPU culling chains = 0.92 ms PC GFX time incl. Hi-Z (8.6 % of the frame; latency-bound, est. 1.5-3 ms on the Ally; true prefix-scan dependencies, DXIL checked); 262 tiny non-instanced alpha-tested direct draws (~3-5 % main thread); phase-2 re-record of all 129 meshlet groups (~2-3 % main thread); no occlusion culling for direct draws. Hypothesis H1 (dynamic list drawn in both shadow occlusion phases) needs one marker capture before the 1-arg patch A2.
- New live tool `patcher\gpu_markers.fsx` (memory only, default off): switches on the engine's own debug/PIX markers (record byte g+2A62h, replay byte g+552h = SynCtx+542h) so an RGP/PIX capture shows Cascade/VSM/Tile/culling/render-node names. Offline: ALL PASSED (`re\scripts_shadow\gpu_markers_offline.txt`). Diagnostic only (CPU cost per marker; turn off after the capture).
- Helpers: `re\scripts_shadow\` (rgp_groups.py, rgp_window.py, rgp_families.py, Ghidra outputs q1/q2, extracted DXIL). Symbols: `re\symbols\incoming\shadows-culling.tsv` (44 rows, dry-run merge 44 added; not merged).
- Undo (only if used later): `set FT_ALLOW_WRITE=1 & dotnet fsi patcher\gpu_markers.fsx off` (or `revert`), or restart the game.

## 2026-10-07 14:xx - GPU shader sweep at MAX: MPC#23 + TOONEDGE2#2 replacements staged (nothing installed, game not run)
- Study: `notes\gpu-shader-sweep.md`. All non-GI/fog passes ranked at MAX (r5 RGP + 14:16 wave lifetimes + gfx1201 ISA scans). No GHDM-class win is left among file-swappable shaders; ~1.4 ms PC of the post chain lives in Nioh3.exe (needs an ASI PSO bytecode hook). Next candidate designed, not built: WDO#14 VSM page-flag de-duplication.
- New replacements (staged in `gpu-shaders\replacement\`, gitignored): `0x29ee0902.file` MPC#23 (meshlet prefix sums: 1 load round trip + LDS scan instead of up to 64 serial round trips), `0x9c481f79.file` + `0x47bc5cb4.file` TOONEDGE2#2 (outline pass: 18x18 LDS tile, 17.5 -> 3.5 image loads per pixel). Interface identical (parts/SFI0/ISG1/OSG1/PSV0/bindings), other records byte-identical; bit-exact against the shipped DXIL in a reference interpreter (60 + 96 scenes, mutation-checked). Expected ~0.25-0.4 ms on the Ally at MAX (+0.4-0.7 % fps).
- New: `gpu-shaders\sweep\` (sources, build_sweep.py, verify_sweep.py, sim.py with barriers/LDS race check on a vendored snapshot of the GI agent's interpreter, s2g.py, rank/ISA scripts), `gpu-shaders\apply_sweep.ps1`; `gpu-shaders\ally_shaders.sh` gains rows mpc23 / toonedge2 and now checks every selected file before touching any. Both installers tested on a scratch copy only.
- Undo: nothing was applied. If installed later: `apply_sweep.ps1 -Item all -Revert` (PC) or `./ally_shaders.sh revert mpc23` / `revert toonedge2` (Ally), or Steam verify.

## 2026-10-07 14:10 - GPU scheduling: async compute + submission timing (static/trace only; nothing applied, game not run)
- Study: `notes\gpu-scheduling.md`. Queues: 1 DIRECT (2EE9DE4), COPY (2EE9EE8/2F110A8/2F13350), one COMPUTE queue per enabled KTGL async slot (2EF8448). Async today (RGP r5): VFSCAT fog 293 us, ocean PFB/IGW 182 us, VCLOUD <1 us = 0.49 ms, all overlapped; 2.72 ms of compute still on GFX.
- Found the engine's async-compute slot system: 13-slot table .data 44C04D0 (shipped on: 0,3,4,6), renderer init 2269EC4 switches slot 0 (effects, KIDS_ASync_LLCulling_Effect) OFF on every non-NVIDIA GPU (226A490); PC dump live copy 45C0460 = slots 3,4,6. Movable compute with idle CUs: effect 1x1x1 chains on GFX (0.32 ms at ~4 % occupancy) and RefProbe GI (0.65 ms at HIGH, ~2.5 ms at GI HIGHEST, 10-36 %). Estimate PC -0.5..0.7 ms (5-6 %) at HIGH, Ally 0-4 % (power-limited).
- Submission: K1 (+3C70) never creates ExecuteCommandLists (all 31 K1 sites submit=0); lists are cut at fence points / 4 explicit sites; 21-23 GFX lists per frame. GPU idle per frame (r5): 0.32 ms CPU-late vs 1.35 ms with work queued (device-wide stalls: GFX + compute + DMA start together; lead: tile-mapping/paging). "More frequent submission" model: <= 0.1-0.13 ms/frame -> not built.
- New live scripts (default off, offline checks ALL PASSED, memory only): `patcher\k1_live.fsx` (offline | status [sec] | apply <N> | revert; reads the engine's own per-frame stream stats at +3D00: draws, kicks, drain spin, ring-full wait); `patcher\async_slots.fsx` EXPERIMENT (offline | status | arm <slots> boot race before renderer init | revert; `arm 0` = 226A497 74->EB, slot 0 on AMD as on NVIDIA; risk unknown - KT disabled it on purpose).
- Tools: `re\scripts_gpu\sched_overlap.py`, `re\scripts_d3d12\submit_model.py`, `re\scripts_d3d12\slotargs.fsx`. Symbols: `re\symbols\incoming\merged\20261007-140743_gpu-scheduling.tsv` (29 rows, merged into symbols.tsv).
- Undo: nothing applied. If used: `k1_live.fsx revert` or restart; async_slots takes effect only at renderer init -> restart the game without arming.

## 2026-10-07 - ASI 1.6.0 (v10): WG wake gate, TG tick governor (opt-in experiment), DS, LK, CP, SPF (memory only; nothing run in-game)
- All new keys default 0 in the main ini. Ally zip: WG = 2, CP = 2, SPF = 2 (installed, switched off for a live A/B); TG = 0, DS = 0, LK = 0. Every existing value in both inis kept.
- WG (notes\cpu-hotspots-ally.md 5): WS2 48C6B / WS3 48D06 caves call WgWake, the worker-sleep cave 1A6BC35 became a wrapper (WgSleep). Verified against the code: the note's single per-pool counter is not exact (a worker can return from its wait on a stale signal with bit0 still set), so WG tracks a per-worker waiting/pending state; pending per pool = workers woken by a counted wake and still inside their wait. Skip = target keeps bit0, stays asleep. No-stall proof in README (the pending worker clears its unit with a locked op, then runs JobWorkerRunUntilIdle 3FE14 over every queue of the pool; store-buffer ordering vs the submitter's push + CAS). New tests\run_wg_harness.py: real WgWake/WgSleep source in a v2.02-layout pool model, 4 runs, ~10 M jobs, 0 stalls, pending back to 0; the negative control stalls. Live switch; 30 s "stats/s WG" line; WG_MaxPending / WG_WindowUs.
- TG (opt-in EXPERIMENT; changes the logic step to ~1/30 s like the game's 30 FPS mode, taps must be held ~33 ms): caveLim (P4) clamps the limiter target to tg_cap; governor on the Present-hook thread after each overlay stats window (cap = ceil(rendered x 1.25) + 2, floor TG_Floor 30 / VRR clamp min + 1, raise at once, lower after 2 s); watchdog drops it on switch-off, L1/VRR off or a 250 ms present stall. Refuses (logged, menu "REFUSED: L1 off") without L1 active; under Proton that needs L1OnWine = 1. Live switch whenever VRR is installed; "stats/s TG" line (ticks/s vs rendered/s, cap, L1 sleeps).
- DS: 1A74BDA call [QueryPerformanceCounter] in the main loop's 1 ms retry spin -> DsQpc (own high-resolution timer, one thread, exit test unchanged). Needs L1 installed. "stats/s DS" line.
- LK / CP / SPF from notes\worker-jobs-opt.md 7 (scripts offline-verified there): LK 18 byte rows (next launch; the four 1301xx rows got patterns with their rel32 bytes because 130014 has a byte-identical twin at 12F243), CP cave D8180, SPF cave DB8BF (live switches). Deps: CP/SPF loop hashes.
- Skipped: RL (never sampled in gameplay; delays load requests up to 3 ms), IATS (rewrites a third-party loader's IAT shims; not provable from the game side). Reasons in README.
- patches.json: 21 sites (DS, CP, SPF, 18 LK) + 15 deps (8 WG under group WS, 5 DS, CP, SPF). Test: ALL 94 SITES OK, deps 60/60.
- Undo: the keys back to 0 (or the menu switches), or the previous .asi.
## 2026-10-07 - ASI 1.7.0: Proton FG-off crash fix + GPUProf (measurement, default off)
- osd.cpp: HkExecuteCommandLists captures a queue only when the caller's return address is inside Nioh3.exe or sl.interposer.dll (FSR frame-interpolation internal queues are never used as the overlay queue); DestroyCtx waits up to 1.5 s on the old ctx fence before releasing.
- menu.cpp: under Proton, FG ON -> OFF no longer calls the game setter (the live swapchain rebuild crashed the game ~2 s later, v9 log); it writes FRAME_GENERATION OFF via the safe config.xml writer (+ marker) and shows "FG off at next launch". OFF -> ON stays live.
- Also kept from the interrupted work: gpuprof.cpp GP pass profiler ([Profiler] GPUProf, default 0), 103 sites OK. LK/CP/SPF/WG/TG/DS were already in 1.6.0 (defaults 0; Ally zip WG=2, CP=2, SPF=2, TG=0).
- Untested on hardware (game not run): FG-off path, caller-filtered queue capture.

## 2026-10-07 - psoswap (PSO bytecode swap, default OFF, not integrated) + off-disk post shaders located (nothing applied, game not run)
- Study: `notes\gpu-post-exe.md`. Of the "~1.4 ms exe post chain", only the TAA filter 449cf0ae, the copies and CustomDepthUpscale are in Nioh3.exe. Bloom (17149dca/18a4c2bb), compose 6a305be4, df4adc57 and MB/DOF are in no file of the game: the bytecode is built at run time. 2493f9d9/d4974586 belong to FSR4 (amd_fidelityfx_upscaler_dx12.dll).
- No rewrite shipped: the exe-resident post shaders are pixel shaders (no LDS) or bandwidth-bound copies, so there is no output-identical win (reasons in the note). MB/DOF need a DumpMode run first.
- New: `mod\Nioh3EngineFix\src\psoswap.{h,cpp}` ([ShaderSwap] Enabled/Folder/DumpMode/LogAll). It hooks the device's PSO creation (graphics, compute, stream, pipeline library) through the D3D12CreateDevice IAT slot. Files are keyed by container or DXIL HASH digest; a runtime refusal falls back to the original desc.
- psoswap integration steps for the ASI src owner: psoswap.h header and note §3. Test: `tests\run_psoswap_harness.py` (real D3D12 device on this PC): ALL PSOSWAP CHECKS OK.
- Tools: `gpu-shaders\post\inventory.py`, `disasm_all.py`, `offdisk_scan.py` (also decodes the 24 IDRK flag-0x4000 files that dxil_index.csv skips). The extracted blobs and disassembly are gitignored.
- Undo: nothing installed. Once integrated: `[ShaderSwap] Enabled = 0` (default) or delete the replacement folder.
| 2026-10-07 | Nioh3EngineFix ASI 1.7.1 (103 sites unchanged): psoswap integrated (dllmain early Start, osd install thread InstallNow, build.bat/CMake). `[ShaderSwap]` main ini Enabled=0 (inert). Ally package ini: Enabled=1, DumpMode=2, LogAll=1 - observe + dump only, no replacement files; dumps in `Nioh3EngineFix_shaders\dump`, log lists each shader hash once. Package zip `Desktop\Nioh3EngineFix-Ally.zip`. Tests: run_psoswap_harness ALL PSOSWAP CHECKS OK, test_patterns ALL 103 SITES OK. | `[ShaderSwap] Enabled = 0`, delete `Nioh3EngineFix_shaders`, or restore the 1.7.0 .asi |
| 2026-10-07 | Stutter study (no game change): `notes\stutter.md`, census `re\scripts_stutter\ally_spikes.py` + `pc_spikes.py` + helpers, `patcher\stutter_probe_live.fsx` (SPR, measurement only, not applied), `patcher\reswait_live.fsx` unchanged. Ally: 0 spikes in steady play, all hitches in the load window. | Delete the new files; nothing installed. |
| 2026-10-07 | GPU tile-mapping study (no game change): `notes\gpu-tiles.md` (census: 2 UpdateTileMappings sites, both in the KTGLGraph texture-streaming cache - job path on its own COPY queue, frame-begin path recorded at the head of the GFX stream twice per frame; VSM uses no D3D12 tiling), symbols `re\symbols\incoming\gpu-tiles.tsv`, scripts `re\scripts_d3d12\tiles_*.fsx`. NEW unwired ASI module `mod\Nioh3EngineFix\src\tilestats.cpp/.h` ([Profiler] TileStats=0 census line; [GpuOpt] TileBatch=0 same-result merge of consecutive UpdateTileMappings), harness `tests\run_tilestats_harness.py` ALL OK (bit-exact read-back for TileBatch 0/1/2 on a real reserved texture), integration steps `mod\Nioh3EngineFix\tilestats_INTEGRATION.md`. | nothing installed; once integrated: TileStats=0 / TileBatch=0 (defaults) |
| 2026-10-07 | Nioh3EngineFix ASI 1.7.2 (106 sites, +3): tilestats integrated (build.bat/CMake, dllmain Start + 30 s LogLine, osd OnEcl/InstallOnQueueVtable/OnFrame; `[Profiler] TileStats`, `[GpuOpt] TileBatch`); RWb (22-byte patch 1A3146A, wake behind the Sleep(0) wrap test in ResStateWaitSwap; ini `RWb`, default 0, next launch); SPR (measurement-only timing caves on CreateGfxPso 119670->9290D8 and upload-heap fallback 62D688->62D7E7, `stats spr:` every 30 s). Main ini: TileStats=0, TileBatch=0, RWb=0, SPR=0. Ally zip ini: TileStats=2, TileBatch=2 (count only), RWb=0, SPR=1. Tests: test_patterns ALL 106 SITES OK, tilestats + psoswap harnesses OK. Zip `Desktop\Nioh3EngineFix-Ally.zip` rebuilt. | Ini: `TileStats=0`, `TileBatch=0`, `RWb=0`, `SPR=0`; or restore the 1.7.1 .asi |
| 2026-10-07 | GPU culling-chain rewrite (staged, NOT installed; game not run): `notes\gpu-culling-rewrite.md`. ONE combined 0x29ee0902.file (item `mpc`, supersedes the sweep's mpc23-only build): MPC#23 computes the whole #23->#31 prefix-sum chain for cnt <= 1024 (LDS scan, 1 load round trip), #28-#31 return early then (else shipped code); 0x8d9282e6.file (item `mshlod`): MshLod#3 pad loop spread over lanes. Proof: `gpu-shaders\culling\verify_culling.py` 95 chain scenes + selftest, `mshlod` 2926 scenes + selftest, all 0 failures (DXIL interpreter, shipped vs replacement, write sets). sha256 0x29ee0902 42f045c3->3947f3d7, 0x8d9282e6 ec0f5283->214ac371. Est. PC -0.055 ms, Ally -0.11..0.24 ms; optional ASI skip of #28-#31/#26 (RVAs in the note §3, symbols `re\symbols\incoming\gpu-culling.tsv`) another ~0.05 / 0.1-0.2 ms. Undo: `apply_sweep.ps1 -Item mpc|mshlod -Revert` / `ally_shaders.sh revert mpc|mshlod`. |
| 2026-10-07 | GI gather rewrite (staged, NOT installed; game not run): `notes\gpu-gi-rewrite.md`. RefProbe#121 (diffuse probe GI) in `shader_dx12\0xd070067d.file`: probe-corner/search loops unrolled + if-converted (all corner reads issued together), 103 reassociation pins; bit-exact vs shipped DXIL on 768+256 interpreter seeds (100 % blocks). Model: Ally ×1.4–1.5 on #121 (~2 ms at GI HIGHEST), PC neutral (Ally-only). #414 / VFSCAT not shipped (reasons in note). Replacement sha256 c952f39a…fb97, original d3cbb484…c372. | `ally_shaders.sh revert gi` / `apply_gi.ps1 -Revert`, or Steam Verify integrity |
| 2026-10-07 19:00 | Nioh3EngineFix 1.7.3 (mod only, nothing applied to the game): fixed 1.7.2 SPR=1 hang (no frame presented). Root cause: dllmain.cpp `kLastTarget` still pointed at `caveSPF` so the trampolines for `caveSprG`/`caveSprU` were never written; the SPg/SPu call sites jumped into zeroed near-page memory. SPR default stays 0 in both inis; test_patterns.py now checks kLastTarget == last Target enum entry and that no branch lands inside the SPR overwritten bytes. MOD_VERSION bumped (1.7.2 build logged 1.7.1). Package `mod\Nioh3EngineFix-1.7.3-ally.zip` (not sent). | restore `mod\Nioh3EngineFix` from commit e5b01ef / delete the zip |
| 2026-10-07 | Nioh3EngineFix ASI 1.7.4 (106 sites; nothing applied to the game, game not run): `[Profiler] GPUProf = 2` (GP2) per-PSO GPU attribution. Motivation: the engine markers cover only ~25 % of the Ally GPU time (14.5 of 59 ms). gpuprof.cpp hooks SetPipelineState/Draw*/Dispatch/ExecuteIndirect/DispatchMesh/Copy*/Resolve*/Clear* through gpuopt, writes a timestamp on bucket change (first draw after a PSO change, copy/clear, marker begin/end, list first call, before Close) and bills the interval to the bound PSO (+ enclosing marker). psoswap.cpp gained PSO-pointer -> shader-set tracking (hooks installed by GPUProf>=2 even with [ShaderSwap] Enabled=0; lock-free PsoSetId/DescribeSet). Same 8-slot ring (read 6 frames late, no stall), own cap GPUProf2_MaxStamps=8192, heap 8x(2xMaxMarkers+MaxStamps+16384). Every 30 s: "GP2: frame X ms GPU span (wall); covered Y ms", top GPUProf2_Top=40 buckets (vs=/ps=/cs= hashes, ms/frame, draws+dispatches/frame, [marker]), and "GP2: overhead" (hook CPU sampled with QPC; windows 2 and 4 run with PSO stamps off for an ON/OFF wall-frame A/B, GPUProf2_AB=1). Caveat documented: timestamps between draws measure GPU-pipelined intervals, so per-PSO numbers are approximate (barrier waits go to the preceding PSO). Tests: pattern ALL 106 SITES OK; run_psoswap_harness ALL PSOSWAP CHECKS OK (+GP2 map checks); run_tilestats_harness ALL OK; run_gp_harness now runs mode 1 and 2 on this PC GPU (heavy vs light compute PSO, hashes, dispatch counts, covered <= span): ALL PASSED. Main ini GPUProf=0; Ally zip ini GPUProf=2 (+ README-ALLY note, asi + README-MOD.md refreshed in mod\ally-package, zip on Desktop, not committed, not sent). Real overhead on the Ally unknown until the first log; PC harness: ~260 ns/hook call (incl. QPC sampling). Undo: GPUProf=0. |
| 2026-10-07 | Volumetric fog rewrite (staged, NOT installed; game not run): `notes\gpu-fog-rewrite.md`. VFSCAT#235 in `shader_dx12\0xa9910db8.file`: one merged early-exit VSM clip-level search for the 3 sun samples (was every level x 3, 2 round trips each), cascade/VSM samples issued together, clustered light list software-pipelined (next index ahead, spot rows at loop top), LPV/reprojection loads overlapped with the noise. 346 `max(x,x)` pins (exact incl. NaN; NIR folds them, so RADV/ACO sees the shipped DAG/FMA fusion). Bit-exact vs shipped DXIL: 768 coverage seeds (137/137 blocks) + 256 realistic + 128 heavy-light + hold-out 1536 seeds, 0 ulp. RGA gfx1103 96 VGPR / 5 waves (= shipped), VMEM loads 101->81. Model (Ally): x2.7 in-order / x1.35 dataflow, ~0.5-1.3 ms at VOLUMETRIC HIGH; PC ~0 (async). sha256 4784aabe...f993 -> ae04ee6a...0e6d. GI tooling extended (mkfast src/pin_lo/pin_div, perfmodel --inorder, interpreter cube arrays; rp121 build unchanged). | `ally_shaders.sh revert fog` / `apply_fog.ps1 -Revert`, or Steam Verify integrity |
| 2026-10-07 | Runtime post shaders (staged, NOT installed; game not run): `notes\gpu-post-runtime.md`. Ally dump 172 inventoried (1812 blobs: 1653 disk, 104 exe, 55 runtime-only; `gpu-shaders\post\out
untime_inventory.csv`); MB / DOF / bloom / composite are runtime DXBC (fxc), df4adc57 absent; the 172 mod log was not on this PC. 5 PS rewritten with batched fetches (same ops, same order): MB tile max f11afe97, MB gather 4e13e236, DOF gather cf792d56, composite+CA 04c1052d / e6efc8ff. Proof: new SM5 interpreter `dxbc_interp.py`, 400 scenes each, 0 mismatching pixels, 100 % instruction coverage, mutation-checked; D3D12 PSO check ALL PSOS OK. Package `gpu-shaders\post\out\<digest>.dxbc` + SHA256SUMS -> `Nioh3EngineFix_shaders\` with [ShaderSwap] Enabled=1, DumpMode=0. Expected 0.05-0.2 ms (model; tile max is the sure part), A/B per file with GPUProf=2. | `[ShaderSwap] Enabled = 0` or delete the files |
| 2026-10-07 | Nioh3EngineFix ASI 1.7.5 (107 sites; nothing applied to the game, game not run, not sent to the Ally): (1) **CS (culling skip) NOT shipped**: guard design was ready (sha256 3947f3d7... of the installed 0x29ee0902.file) but the static proof failed. Proven: `[rsp+0A8h]` is written once (530304) from the same `cnt` that sizes #23's dispatch and that 530FE4/530EEE reload; NOT proven: that the shaders' cbuffer nVSDrawPrm.y equals it (the record function only feeds cnt to the buffer-view setter 52F8EC; the cbuffer fill is in the compute-context commit, vtbl+C8h, not located). Details in notes\gpu-culling-rewrite.md section 3; needs the commit/cbuffer path or a runtime compare. (2) **AS0** (`[Patches] AS0`, default 0 everywhere incl. the Ally zip): `226A497 74 -> EB` keeps async slot 0 (KIDS_ASync_LLCulling_Effect) on for non-NVIDIA GPUs; written in the decrypt-wait loop, refused if the live slot table (.bss 0x45C0460, entry 1 id) is already filled (renderer init ran), log `AS0: slot 0 forced on`; new `stats/s ASYNC: N async compute queue(s), slots ...` every 30 s. Risk: KT disabled it on purpose; faster/slower/one-frame effect lag/corruption/GPU hang possible. Site AS0 in patches.json (pattern-tested). Tests: pattern ALL 107 SITES OK; psoswap, tilestats, gp, wg harnesses all OK. Ally zip (Desktop, uncommitted) refreshed. Undo: AS0=0. |
| 2026-10-07 | Level-load memory study (no game change, game not run): `notes\load-memory.md`, symbols `re\symbols\incoming\load-memory.tsv`, offline check `re\scripts_stutter\loadmem_check.py` (ALL LOADMEM CHECKS OK). Found the arena sub-heap (HeapFree 130D0C, HeapReset 9C0A94, chunk layout; nothing ever decommits). Design LM (default OFF, not integrated): ASI sweeper that MEM_RESETs payload pages of free chunks unchanged for >= 1 s (headers excluded); model proof of identical allocator behaviour; est. removes 40-100 % of the +0.75-1.1 GB load swap burst (class A). Class C untouched. | nothing installed; once integrated `[Memory] LM = 0` |
| 2026-10-07 | Nioh3EngineFix ASI 1.7.6 (107 sites unchanged; nothing applied to the game, game not run, not sent to the Ally): LM load-memory sweeper integrated (`src\loadmem.cpp/.h`, notes\load-memory.md; build.bat/CMake, dllmain Start + 30 s LogLine). `[Memory] LM` (default 0 in BOTH inis; the Ally A/Bs across loads by setting LM=1), LM_Force, LM_PeriodMs 1000, LM_MinKB 256, LM_BudgetUs 1000, LM_SliceMB 128. Sweeper thread: finds engine heaps by scanning the static arena (`[IMG+4B50348]`, 4 MiB) for the heap vtables 408F940/408FB78 with layout sanity checks, takes the game's own _Mtx_lock (B614A8, heap+60h; lock layout re-verified against the dump: HeapFree does lea rcx,[heap+60h]; call B614A8; ends jmp B61524; code bytes checked at start), one lock per bin walk, hold capped by LM_BudgetUs, MEM_RESET sliced; resets only the page interior of free chunks >= 256 KB with identical (addr,size) in the previous sweep (never the header), forgets anything not seen free in a sweep, never resets twice. Start-up self-test (64 MB scratch must drop the working set by >= 50 %, else LM off + logged; LM_Force=1 skips). Logs `LM: ...` and `stats lm:` every 30 s. Tests: new `tests\run_loadmem_harness.py` (synthetic heap in real memory, ALL LOADMEM CHECKS OK), test_patterns ALL 107 SITES OK, psoswap/tilestats/gp/wg harnesses OK, `re\scripts_stutter\loadmem_check.py` OK. README + README-ALLY updated (test = MangoHud + MemStats over 3 loads, LM 0 vs 1). Ally zip on Desktop refreshed (uncommitted). | `[Memory] LM = 0` or restore the 1.7.5 .asi |
| 2026-10-07 | GP2 log digest (tooling only, game not run): `re\scripts_gpu\gp2_digest.py` ranks GPUProf=2 `GP2:` rows by ms/frame and % of span, resolves hashes to shader_dx12 record / exe-embedded / runtime-compiled, flags existing replacements (GI/fog/sweep/post); index cached at D:
ioh3-re\gp2_hash_index.json; selftest passes; usage in notes\gpu-maxsettings.md. |
| 2026-10-07 | GI reflection rewrite (staged, NOT installed; game not run): `notes\gpu-gi-rewrite.md` addendum. RefProbe#414 (reflection probe trace) added to the GI replacement `shader_dx12\0xd070067d.file` next to the unchanged #121: 11 of 12 constant-trip probe loops collapsed with control-equivalent predicates (`ll2hlsl --cdpred`) and `-HV 2018` (stops dxc sinking speculated reads back into branches), Chebyshev branch of the 8-probe gather if-converted, 140 `max(x,x)` pins. Bit-exact vs shipped DXIL, 0 ulp: new geometry-consistent scenes (`gi\geoscene.py`) 768 + hold-out 1536 + 256 realistic seeds, old random scenes 768 + 1536 + 256; blocks 268/270, 270/270 with a both-programs guard mutation. RGA gfx1103 120 VGPR / 4 waves (shipped 124 / 4), branches 363->161; model Ally x2.2-2.6 on #414 (~1.5-2.5 ms at GI HIGHEST if it holds), PC x1.45-1.8. sha256 d3cbb484...c372 -> 59897999...de4f (supersedes c952f39a...fb97, #121 only); `apply_gi.ps1` / `ally_shaders.sh` updated. | `ally_shaders.sh revert gi` / `apply_gi.ps1 -Revert`, or Steam Verify integrity |
| 2026-10-07 | Nioh3EngineFix ASI 1.7.7 (107 sites unchanged; nothing applied to the game, game not run, not sent to the Ally): menu UX. New default controller gesture = TRIPLE-TAP Start (`[Menu] Gesture = TripleTap`, `TapWindowMs = 900`, `TapGapMs = 300`, `SwallowTaps = 1`; `Gesture = Hold` + HoldSeconds = old behaviour). In the XInputGetState hook (src\menu.cpp PadFeed) Start presses are withheld from the game while a tap sequence is pending; a lone press is replayed to the game as a ~120 ms tap after TapGapMs, a still-held press reaches the game after TapGapMs, 3 presses open/close the menu and are never shown to the game (pad latched neutral until released). SwallowTaps = 0 passes every press immediately. Pause-menu hint (existing 58B/58C flags) is drawn every frame while paused (not a timed popup): `F10 / Press Start 3x: Mod menu` (hold text when Gesture=Hold). F10 and menu navigation unchanged. Both inis (keys added only), README, README-ALLY updated; ally-package refreshed, zip on Desktop (uncommitted). Tests: pattern ALL 107 SITES OK; psoswap/tilestats/gp/wg/loadmem harnesses OK (input path itself not exercised - needs an in-game check: single Start tap still opens the pause menu ~0.3 s late, triple-tap opens the mod menu without the pause menu flickering). | `Gesture = Hold`, `SwallowTaps = 0`, or restore the 1.7.6 .asi |
| 2026-10-07 | Ally 1.7.5 GP2 replacement A/B (tooling + analysis only, game not run): `notes\ally-replacement-ab.md`. GP2 keys = DXBC container digest; `gp2_digest.py` index now covers new replacement containers, all shader_dx12 records, dump contents (--rebuild); new `re\scripts_gpu\gp2_ab.py`. Result: gi #121 +0.89 ms, fog VFSCAT +0.33, toonedge2 >=+0.14 slower on RADV -> drop; gi #414 not on the Ally; others below top-40. |
| 2026-10-07 | Nioh3EngineFix ASI 1.7.8 (107 sites unchanged; nothing applied to the game, game not run, not sent to the Ally): cheap GP2 (`GPUProf = 2`). Problem: ~1400 timestamps/frame made GP2 frames 30 ms vs 16.7 ms (hook CPU only 2.3 ms/frame, so the cost is GPU-side: each EndQuery on RADV drains the pipeline; the OFF baseline was also a lobby window). New `[Profiler] GPUProf2_SampleEvery = 8` (stamp 1 frame in N; other frames clear the SetPSO/Draw/Dispatch/Copy/Clear hook slots, take no queries, pay nothing) and `GPUProf2_MinRun = 4` (stamps stay at PSO-run boundaries, an interval is cut only after >= K draws; short runs merge into the longest neighbour, copies count K). Log format compatible (frame line gets a suffix; rows per sampled frame; gp2_digest selftest OK). The `GP2: overhead` line now compares sampled vs unsampled frames live in the same scene and reports the amortised cost. Both inis (keys added only), README/README-MOD updated, ally-package refreshed, zip on Desktop (uncommitted). Tests: pattern ALL 107 SITES OK; gp harness now has mode 3 (sampled + coarse), psoswap/tilestats/wg/loadmem OK. Not measured on the Ally yet. | `GPUProf2_SampleEvery = 1`, `GPUProf2_MinRun = 1` (old behaviour), `GPUProf = 0`, or restore the 1.7.7 .asi |
| 2026-10-07 | vkd3d-proton audit (analysis + tooling only; game not run, nothing in src or the game changed): `notes\vkd3d-audit.md`. CPU translation is not the Ally limit (dispatcher 81 % parked, vkd3d+winevulkan 9.3 % of its samples). GPU-side vkd3d costs of the engine pattern: COMMON-sourced transitions = ALL_COMMANDS full drains (R2, per-list stamp reset in SynTransitionResource 115E24), read->read superset transitions from the texture bind 115B9C/115D94 that vkd3d does not elide (R1, each a barrier + render-pass end), render-pass ends on every barrier/OMSetRenderTargets (gpuopt Barriers=1 removes ~60 %), 21-28 ECL submissions. Not costs: ExecuteIndirect (plain DRAW/DRAW_INDEXED signatures at 2EFF231, no DGC), queries (none on the dispatcher), CBV/descriptor work (CPU only). Designs R1/R2/ECL-merge with RVAs, none implemented (not spec-exact / needs proof). New `re\scripts_d3d12\vkd3d_cost.py` (gpuopt trace -> per-frame vkd3d cost classes, `--selftest` OK); symbols `re\symbols\incoming\vkd3d-audit.tsv`. Next: one Ally run with `[GpuOpt] Barriers=2, TraceRecords=300000`. | delete the new files |
| 2026-10-07 | Perf-coverage symbol pass (RE only; nothing applied to the game, game not run): new `re\scripts_re\perf_coverage.py` (rerunnable coverage of Ally SP + PC cpuprof time by named functions, leaf/.pdata folding) and 189 function rows (`incoming\perf-coverage.tsv`, merged by the main session 22:41). The 37 renames of `Sched0Hot_/MainHot_/RenderSubmitHot_` placeholders are pending in `incoming\perf-coverage-2.tsv` and must be applied in place. Self-time coverage 56.5 % -> 96.6 % (82.0 % real names + 14.5 % `Unknown_<area>`); 77.5 % until the renames land. Wall time 91.1 % -> 99.3 %. `notes\perf-coverage.md`. | revert the merged rows (see `incoming\merged61007-224150_perf-coverage.tsv`); delete `perf-coverage-2.tsv` |
| 2026-10-07 | Material / long-tail shader trim (staged, NOT installed; game not run): `notes\gpu-materials-trim.md`. Strictly-redundant-only audit of the Ally top PB2 / PB2TREE2 / PB2UMAH / PB2MRLR / PB2DS / VCLOUD / SDFOCC shaders: no static redundancy in any PS/CS (only cbuffer re-reads). SFB-DEDUPE (killed agent, finished: bit-exact) REJECTED - RGA +11 % instr, +19 full waits, +4 VGPR on trees; staged SFB files deleted. VCLOUD/SDFOCC loop-invariant hoists rejected (+81 / +8 live registers). Staged LOAD-DEDUPE (repeat of a read-only load right after storeOutput removed): `trvs` = `shader_dx12\0x52c39db0.file` (tree VS #0-#3, -3 loads each), `pbvs` = `0xfd4f418f.file` (14 PB2 meshlet VS, -5 loads each); proven (interpreter + structure), RGA ISA identical (expect ~0 unless RADV does not merge them). Interpreter fixes for bindless material shaders (psim/verify_sfb), rga_ps root signature + `--gpso`. | Ally: `./ally_shaders.sh revert trvs` / `revert pbvs`; Windows: `gpu-shaders\apply_materials.ps1 -Item trvs -Revert` (or pbvs); Steam verify also restores. |

## 2026-10-07 - ASI 1.7.9 (unsent): gpuopt barrier trace caller2
- [GpuOpt] trace CSV gets a caller2_rva column (second Nioh3.exe return address via RtlCaptureStackBackTrace, 16 frames; only while TraceRecords > 0), splitting the SynTransitionResource 115E9D sites across the ~45 real callers. re\scripts_d3d12\vkd3d_cost.py groups its call-site tables by it (selftest OK; old CSVs still load). Trace already carried before/after states, COMMON/read->read (derived), OMSetRenderTargets rows. README recipe 'Ally barrier trace'. Zip on Desktop (not committed, not sent).
- Undo: replace the .asi with 1.7.8; TraceRecords = 0 is the default.

## 2026-10-07 - audio thread identified + ObjUpdate placeholder renames (analysis only, nothing installed)
- Thread 1A786A8 = in-house KIDS audio mix thread on XAudio2 (87 % blocked in a 100 ms event wait, no spin; ~0.126 core of real DSP: convolution reverb 5 %, voice decode 4.4 %). Candidate silent-input convolution skip designed, not applied (needs a probe). See `notes\perf-coverage.md` Addendum.
- `re\symbols\incoming\audio-objupdate.tsv` (+8 rows) and `audio-objupdate-renames.tsv` (18 renames, apply in place). `re\scripts_re\perf_coverage.py` reads both: named coverage 96.5 -> 97.1 %.
- Undo: delete the two TSVs and the Addendum; revert perf_coverage.py.

## 2026-10-07 - FSR3 waste audit (analysis only, nothing installed, game not run)
- `notes\fsr3-audit.md`: at Ally MAX (FSR 100 %, sharpness 1.0, FG OFF), no FSR3 work can be removed with an identical image. FG prepare/UI/FI is already skipped (gated on the FG ctx + FI swapchain ctx; no FI passes in GP2). GENERATE_REACTIVE is the only reactive input. RCAS is the single sharpen (no double sharpening). No create flag adds a pass. The context is re-created only on size/mode changes. The velocity-viewport fix is skipped at 100 %. The upscaled-depth copy is consumed by the post chain.
- Research only (changes the look): the embedded FSR3 blobs are min16float (SFI0 MinimumPrecision), likely run as FP32 under vkd3d/RADV.
- Symbols: `re\symbols\incoming\fsr3.tsv` (31 rows: Fsr3UpscalerExecute 0x18d8898, Fsr3EnsureContext 0x346948c, ffx-api exports, FSR3 globals, RenderSettings FSR fields).
- Undo: delete the note and the TSV.

## 2026-10-07 - asset audit (analysis only, nothing installed, game not run)
- `notes\asset-audit.md`: Futamata + SEN_02 base/foliage sample (493 G1M models, 66 G1CO, 417 G1T). Assets are clean: 0.5 % dup verts, ACMR already at Forsyth level, all BCn with full mips, 0 dup textures, collision = 1 % of render tris. Only real inefficiency: meshlets 56 % full (foliage 46 %) because of a 128-vertex cap; the PB2 VS draws 384 lanes per meshlet. A lossless group-preserving repack saves ~22-27 % of meshlets = ~0.04 ms/frame on the Ally; not worth a cluster-DAG rebuild. 262 tiny draws = one-quad decal ModelData (engine-side batching only).
- Scripts: `re\scripts_assets\*.py` (G1M/G1MG/G1T/G1CO parsers + stage resolvers). Game copies on `D:\nioh3-re\assets\`.
- Undo: delete the note and the scripts.

## 2026-10-07 - barrier callers + R1/R2 patch designs (analysis only, nothing installed, game not run)
- `notes\vkd3d-audit.md` Addendum B: all 45 SynTransitionResource callers (74 sites) classified. SynCtx+0x6D0 is a per-list id (set at every list close by 490E50), so stamped resources start each list from COMMON. 6 COMMON stores: 115E61, 11469A, 114ABE, 7F5333, and new list-begin VB/IB rebind 494E00 (494FE8/495001; barrier sites 495036/495074, missing from gpu-barriers.md). Stamped = buffers only (decay emulation; COMMON is their true D3D12 state); textures never stamped. Bind caches (+1118/+1718/+1B98/+1D30/+1DB0) persist across lists, so `P -> X` is NOT safe.
- Designs (gpuopt hook, default off): R2a = COMMON -> (P|NPSR|PSR) for buffers without ALLOW_UNORDERED_ACCESS (exact under D3D12 semantics); R2b (UAV-capable, experimental; may save nothing on RADV); R1 superset drop (RADV-identical, not spec-exact) + engine cave at 115D94 with bytes. Build thresholds and the trace join in B6; ask: add GetDesc Dimension/Flags to the trace.
- New: `re\scripts_d3d12\barrier_callers.fsx`, `barrier_sites.py`, `barrier_sites.tsv`; symbols `re\symbols\incoming\barrier-callers.tsv` (60) + `barrier-callers-renames.tsv` (3, apply in place).
- Undo: delete Addendum B and the new files.

## 2026-10-07 - perf-rename-2: profiled CPU self time 87.5 % -> 99.5 % named (RE only; nothing installed, game not run)
- `re\symbols\incoming\perf-rename-2.tsv` (new rows) and `perf-rename-2-renames.tsv` (renames of `Unknown_*` / `*Hot_*` placeholders; apply in place, merge keeps old names). Role names from the decompile with evidence per row; not merged.
- `re\scripts_re\perf_coverage.py` reads both: `--self` named coverage combined 87.5 -> 99.5 %, Ally 93.0 -> 99.5 %, PC 85.7 -> 99.5 %, `Unknown_` 9.2 -> 0.0 %; all 99 functions with >= 0.3 % PC inclusive share are named.
- `notes\perf-coverage.md` "Pass 2": subsystem roles (resource readiness polling, draw-state commit, UI nodes, animation/secondary motion, cloth/soft body, audio mixer, jobs, script VM), one O(n^2) map walk found (1196B70, too small to patch), next step for GPU-pass CPU paths.
- Undo: delete the two TSVs and the "Pass 2" section; revert the INCOMING line in perf_coverage.py.
- 2026-10-07 (later): perf-rename-2 was merged by the main session at 23:32. Follow-up `re\symbols\incoming\perf-rename-3.tsv` (92 rows: CPU chains of the costliest Ally GPU passes - VolumetricFogScatter, ReflectionProbePass, SkyAmbient, NatureSky/VCLOUD, StaticNature/SSFog, UltraMarine, VSM / local-light shadows, culling; 9 node vtables) and `perf-rename-3-renames.tsv` (5 corrections: 410A460 / 410A498 and 368AE9C / 368AEDC / 368B0AC are UltraMarine node objects, not ReflectionProbe commands; apply in place). Notes: `notes\perf-coverage.md` "GPU-pass CPU paths". `perf_coverage.py` now reads the -3 files. Undo: delete the two TSVs and that section.

## 2026-10-07 23:47 - PC: ASI installed in the game folder (Smart App Control now off)
- Copied dinput8.dll (Ultimate ASI Loader), Nioh3EngineFix.asi (current bin build) and the main (PC-default) Nioh3EngineFix.ini next to Nioh3.exe; OptiScaler's dxgi.dll stays. Undo: delete those three files. Not launched yet (Steam account shared with the Ally).

## 2026-10-08 - ASI 1.7.10 (unsent): barrier trace resource desc + R2a / R2b / R1 rewrites (default OFF)
- Trace: every barrier row gets `res_dim` / `res_flags` (GetDesc cached per resource pointer); the CSV header now names `caller2_rva` too (1.7.9 wrote it without). `re\scripts_d3d12\vkd3d_cost.py` splits COMMON -> X into R2a (non-UAV buffers) / R2b (UAV buffers) / textures and counts R1 supersets by resource class and caller (`--selftest` OK).
- gpuopt hook (Proton + dispatcher thread only, live menu switches, `[GpuOpt] R2a / R2b / R1` all 0, both inis + ally-package ini): R2a = COMMON before-state -> (last state | NPSR | PSR), a write last state gives a 2-struct pair kept in one batch; R2b (EXPERIMENT, warning in the ini) = UAV buffers; R1 (EXPERIMENT) = drop read->read supersets within VCB/IB/NPSR/PSR. Per resource the last StateAfter, recording thread and list generation are tracked; texture / no last state / other thread / same list / partial subresource / flags passes through unchanged and is counted (kept_*). Counters in the `stats/s gpuopt` line.
- Tests: `tests\r2_harness.cpp` + `run_r2_harness.py` (real device, identical work with every switch combination: buffers + 64x64 render target bit-identical, counters, trace columns): ALL PASSED. test_patterns ALL 107 SITES OK; gp / psoswap / loadmem / tilestats / wg harnesses pass.
- README: Ally recipe (one launch Barriers = 2 TraceRecords = 300000 for counts, then A/B R2a and R1 live with GPU W/fps + screenshots). Package zip on the Desktop, NOT sent to the Ally.
- Undo: set R2a / R2b / R1 = 0 (the default), or revert to the 1.7.9 .asi.

## 2026-10-08 - work deciders: quality settings -> pass parameters, LOD, streaming, culling (RE only; nothing installed, game not run)
- `notes\work-deciders.md`: per-setting table of what each applier writes into RenderConfig, with env defaults. Main findings: RenderConfig field names come from the env schema (`re\scripts_re\renderconfig_fields.py` -> `re\out\renderconfig_fields.tsv`, 1,567 fields); VOLUMETRIC scales VolumetricFogResolutionX/Z (+0x13DC/+0x13E0); VFSCAT cb+14D0/14D4 = render width/height (not the grid); VCLOUD ray samples 32/256 come from env data only (no setting); MODEL_QUAL consumer found (character LOD distances x0.5/0.75/1); MODEL_DRAW = max drawn characters 30/45/60; SHADOW_QUAL column +0x38 is read by CharacterFarCullFlag (20/30/70 m) and +0x14 is the VSM tiles-per-frame budget (2/8).
- Verdict: no same-look lever in these tables (all authored quality steps).
- Symbols: `re\symbols\incoming\work-deciders.tsv` (47 rows) + `work-deciders-renames.tsv` (12, apply in place; not merged).
- Undo: delete the note, the two TSVs, the script and its output.

## 2026-10-08 - GPU passes mapped made exact + grey buckets of the Ally GPU chart identified (RE only; nothing installed, game not run)
- `perf_coverage.py` gpu-notes check: ignores non-functions (gpu-profile.md `| 0xa9af..` command-buffer ids, invisible-gpu.md ESM `byte offsets`, `+0x..` D3D12Core offsets). Was 55/63; of the 8 misses 4 were non-functions (0xa9afc, 0x11010, 0x30e830, 0x104c0), 4 real (0x25f15b0, 0x2629194, 0x1197e20, 0x11e8870 = editor env-param schema field-index switches). Now 55/59 measured; 59/59 once `incoming/gpu-passes-renames.tsv` is merged (not merged here).
- `gp2_digest.py`: new curated `re\scripts_gpu\exe_passes.tsv` (16 exe-embedded / runtime shaders: copies, sprite/UI quads, tessellation CS, TAA filter, EffectMerge) applied in `resolve()`; hash-only fallbacks ("(not in inventory)") no longer count as names. `progress_treemap.py`: new group "sprite quads / UI & effect merge", ExeCopy_/ExeTess_/ExePost_ name rules.
- Grey buckets of `docs\progress\gpu.svg` (~1.3 ms, 14 rows, mostly shared VS a2bd0e86 + exe PS pairs) are all mapped. Symbols: `re\symbols\incoming\gpu-passes.tsv` (8 rdata shader labels) + `gpu-passes-renames.tsv` (4).
- Undo: revert the three scripts, delete exe_passes.tsv and the two TSVs.

## 2026-10-08 - docs: subsystem write-ups (goal part 4)
- Added docs\subsystems\ (13 pages + README index with CPU share, threads, coverage, naming issues) for every area >= 0.3 % of profiled CPU self time; consolidated from notes\*.md and symbols.tsv. Docs only, no game/mod/symbol change. Undo: delete docs\subsystems and the README link.

## 2026-10-08 - decompile DB cleanup + treemap area rules (RE only; nothing installed, game not run)
- Settled B94230 = `MemcpyErms` (16 B rep movsb stub, reached from Memcpy B94250 for large copies); named the six "?" rows and the eight Unknown_ blocked-wait callers (`re\symbols\incoming\cleanup.tsv`, `cleanup-renames.tsv`, applied + merged); removed a stray line from symbols.tsv.
- `progress_treemap.py`: prefix rules + `area_overrides.tsv` + stricter neighbour rule, new Math/CRT and Object/scene areas; charts regenerated. Notes: notes\cleanup-decompile-db.md.
- Undo: revert the script, delete area_overrides.tsv and the two TSVs.

## 2026-10-08 - steady-state streaming + animation deep-dive (RE only; nothing installed, game not run)
- notes\streaming-anim.md: texture tile request/keep-alive/budget path, PagePool frame update + occupancy controller, resource-readiness re-walk, I/O + inflate, animation pipeline and LOD. Findings: MODEL_QUAL is a no-op in v2.02 (like MOTION_QUALITY); skinning is GPU; ResourceIsReady re-walks dependency trees even for ready resources; PagePoolCountUsed feeds only a >85 % occupancy window. Ranked candidates R1/PP/PT1/RR1/MQ, no patches; none moves Ally fps.
- Symbols: re\symbols\incoming\streaming-anim.tsv (22 new) + streaming-anim-renames.tsv (9 in-place, incl. 219E80/6D360 -> NodeGraph*), not merged.
- Undo: delete the note and the two TSVs.

## 2026-10-08 - particle / effect pipeline deep-dive (RE only; nothing installed, game not run)
- notes\particles.md: spawn -> CPU sim -> per-view GPU recording (sim + draw) -> per-particle frustum collect -> EffectMerge, with RVAs. Findings: on AMD the GPU-effect recording (365AA98 -> 3D275C -> 694B38) runs synchronously on [App]Game at 6-7 % (async slot 0 is NVIDIA-only; AS0 also moves this CPU work); 365AA98 was misnamed UiRootUpdate (ui-mainthread.md's "UI 6-8 %" is mostly effects); VISUAL_EFFECT has one functional reader (EffectCreate); GPU sim = 10 stages with one global UAV barrier each, recorded once per frame; no alpha-0 or occlusion culling of particles; Ally measured G1E draws 0.19-0.23 ms, EffectMerge 0.009 ms (the ~3 ms estimate was wrong). Ranked candidates, no patches.
- Symbols: re\symbols\incoming\particles.tsv (40 new, merged) + particles-renames.tsv (4 in place: 365AA98 EffectViewRecordSync, 3D859C EffectGpuObjLatchSubsteps, SynTokUavBarrier and GpuParticleInit descriptions).
- Undo: delete the note; revert symbols.tsv/csv/x64dbg/ida to the previous commit.

### 2026-10-08 notes consistency pass
- Inline "(corrected 2026-10-08, see <note>)" marks added: ui-mainthread.md, mainthread-logic.md (x3), invisible-gpu.md (x3), perf-coverage.md (x4), docs/subsystems/ui.md, gfx-submission.md. Content: 365AA98 = EffectViewRecordSync (UI 6-8 % mostly effects); 3D859C = effect substep latch; particle barriers are per stage not per emitter; 0x410A460/498 are UltraMarine water node vtables; RS+960/964 = render width/height, not froxel grid.
- Not touched: work-deciders.md, streaming-anim.md (MODEL_QUAL reconcile in progress elsewhere).
- Undo: git revert this commit.

## 2026-10-08 - work deciders, second pass (RE only; nothing installed, game not run)
- `notes\work-deciders.md` addendum A1-A5. Fog froxel grid = X x floor(X / view aspect) x Z, created by 0x1478F0 -> 0x38C0C4C (3 x 3D textures) from the per-view RenderParams (= 0x2F4-byte header + env RenderConfig); Ally STANDARD 120 x 67 x 48. GRASS level path traced to the placement unit (+0xFC) and rebuild queue; the arithmetic consumer is still open. Kids property hash found (sum c*31^(i+1); `re\scripts_re\kidshash.py`); the 11 shadow/meshlet param hashes are render-node property overrides with code defaults (4096 / 16 / 0, meshlet 2.0), names not in any static source. Exact live-read recipe for the meshlet LOD table (g_pLodScaleCfg+0xA08). MODEL_QUAL re-verified as a no-op: corrected §1/§2 (streaming-anim.md was right); grass rebuild RVAs fixed (0xA483A4/0xA34750).
- Symbols: `re\symbols\incoming\work-deciders-2.tsv` (24 rows) + `work-deciders-2-renames.tsv` (7, apply in place; not merged).
- Undo: revert the note, delete the two TSVs and kidshash.py.

## 2026-10-08 - level loads: phase timings, load pipeline RE, LoadStats measurement (nothing installed, game not run)
- `notes\loading.md`: Ally load windows (stop flag 58A) 84-132 s; the title->level route takes 118.3-118.7 s in 4 loads with different mod configs/settings/frame rates, so that window is timer/sequence-bound, not throughput-bound. Phases: A CPU 85-95 % / GPU 800 MHz (+ swap burst on some loads), B streaming at 15-40 MB/s, C 190-300 ms GPU-bound frames while LoadIsLoadEnd 3B3424 waits for characters (timeout 120 dt60 = frames). Pipeline: two request managers 1FEE28/1FF320 with an in-flight cap of 10 (1 if GameMain+4Dh) and Sleep(1) polls; zlib 1.1.4 per 16 KiB chunk, parallel CInflateTask jobs; overlapped ReadFile; streamed textures/meshlets are uncompressed. Ranked same-result fixes (RWb/GF/WG during loads, LM, cap 10->16, completion event, faster bit-exact inflate, PSO pre-creation) gated on LoadStats.
- New ASI files (not wired in): `mod\Nioh3EngineFix\src\loadstats.{h,cpp}` ([Profiler] LoadStats = 0), `tests\loadstats_harness.cpp`, `tests\run_loadstats_harness.py` (ALL OK), `loadstats_INTEGRATION.md`. Scripts: `re\scripts_load\load_timeline.py`, `load_sp.py`, `ls_sites.py` (LOADSTATS SITES OK).
- Symbols: `re\symbols\incoming\loading.tsv` (30) + `loading-renames.tsv` (5) applied and merged (merge_incoming also merged the pending `d3d12-sites.tsv`, 192 rows, from another agent).
- Undo: revert the note/scripts/new files; symbols: `git checkout` the symbols.* files from before this entry.

## 2026-10-08 - D3D12/DXGI call-site census, goal part 3 (RE only; nothing installed, game not run)
- `notes\d3d12-sites.md`: 604 vtable call sites in 272 functions (barriers excluded, listed for completeness): Device 195, CL 135, Resource 81, SwapChain 46, Queue 44, Fence 42; 261 per-frame/per-draw, 144 streaming, 103 init. Subsystems: GpuMemory 192, SynRecord 117, FSR3 frame-interpolation swapchain 105, DeviceInit 71, CmdListRing 38. Gaps listed (token-stream `this`, small resource wrapper fields).
- Scripts: `re\scripts_d3d12\d3d12_vsites.fsx` (whole-binary vcall + slice dump), `d3d12_iids.py` + `iids.tsv`, `d3d12_sites.py` (IID/holder/param typing + reviewed sites), `d3d12_census.py` -> census `re\scripts_d3d12\d3d12_sites.tsv`.
- Symbols: `re\symbols\incoming\d3d12-sites.tsv` (192 new names + 80 notes) and `d3d12-sites-renames.tsv` (0 rows); not merged.
- Undo: revert the commit (new files only).

## 2026-10-08 - ASI 1.7.11: LoadStats integrated (built + tested, NOT sent to the Ally)
- `src\loadstats.{h,cpp}` wired in per `loadstats_INTEGRATION.md`: build.bat + CMakeLists, dllmain (after loadmem::Start), `[Profiler] LoadStats` in the ini (main 0, Ally zip 1). MOD_VERSION 1.7.11. Measurement only; 25 call sites hooked only when LoadStats > 0.
- Checks: test_patterns ALL 107 SITES OK; loadstats, gp, psoswap, loadmem, tilestats, wg, r2 harnesses all OK; ls_sites LOADSTATS SITES OK; build.bat clean link.
- README-ALLY: 1.7.11 load test (title->level x2 + shrine travel x2 with LoadStats=1 baseline, then RWb=1+WG=1, then LM=1 as a separate pair). Docs: README.md section + changelog line. Zip on the Desktop (not committed): Nioh3EngineFix-1.7.11-ally.zip.
- Undo: revert this commit.

## 2026-10-08 - streaming-anim CPU candidates MQ / PT1 built + proved offline, RR1 skipped (nothing applied, game not run)
- `patcher\mq_live.fsx` (MQ): bit-exact SSE MatrixFromScaleQuatTrans 88780 (entry jmp, stock fallback on out/scale overlap). Offline vs the original bytes: 2,755,130 inputs incl. NaN/sNaN payloads, +-inf, denormals, -0, 5 MXCSR modes: 0 bitwise diffs, 0 MXCSR-flag diffs, 3,001 aliasing cases OK; body 2.86 -> 1.35 ns/call. Expected ~0.8-0.9 % of each KIDSTaskScheduler thread on the PC; 0 fps on the Ally.
- `patcher\pt1_live.fsx` (PT1): pool+D08h shared lock held once per texture in 1290B8 phase 2 (5 sites: 114CA8/114CEE IAT calls -> owner+return-address-gated caps, 129651 claim, 12982E release before vfunc+28h, 1298B3 release). Offline: site/branch/closure checks, lock census, 19 native checks on a real SRWLOCK + 2-thread stress (no deadlock, no overlap with exclusive sections). Per touch 6.0 -> 3.3 ns idle, 26.4 -> 6.7 ns contended; <= ~0.3 % of the main thread.
- RR1 skipped: ResourceIsReady drives loading and its inputs change on loader threads without a choke point; a memo cannot be proven same-result (notes\streaming-anim.md §4.3).
- Outputs: `re\scripts_cpu\mq_offline.txt`, `re\scripts_cpu\pt1_offline.txt`; notes\streaming-anim.md §4 (results, proofs, ASI-ready caves). Both scripts default off (memory only, `apply` needs FT_ALLOW_WRITE=1).
- Undo: nothing applied; if applied live: `FT_ALLOW_WRITE=1 dotnet fsi patcher\mq_live.fsx revert` / `pt1_live.fsx revert` (or restart the game).

## 2026-10-08 - FINAL PHASE tooling: test matrix + benchmark protocol (new files only; game not launched, nothing applied)
- New: `tools\testmatrix\matrix.json` (all ASI flags with live/next-launch mode, devices, wine-only/handheld-only, risk; 10 shader items incl. dropped gi/fog/toonedge2/trvs/pbvs; 5 post-v1 .dxbc via [ShaderSwap]; measurement runs; BEST combos), `run_pc.ps1` (backup/restore of ini + shaders, game lock, PresentMon stand 2x20 s + move 20 s, resume, -DryRun, verdict vs 3x BASE noise), `tm.py` (stats/report/best), `ally_plan.md` (Ally step list + [RESULT] digest), `notes\final-benchmark-protocol.md`.
- Tested: `run_pc.ps1 -DryRun` (60 entries, ~6 h; -LiveMenu would save ~1 h but needs menu_map.json, not recorded), ini editor and `tm.py report/best` on synthetic data. Real runs not done. MQ/PT1/RR1 are listed as disabled (not in the ASI yet). tools\ is git-ignored: files added with `git add -f`.
- Undo: delete `tools\testmatrix`. If a real run ever dies mid-way: `tools\testmatrix\state\Nioh3EngineFix.ini.orig` is the original game ini (run_pc.ps1 restores it on the next start), shaders: `apply_*.ps1 -Revert`.

## 2026-10-08 - autoname (symbols only; nothing installed)
- `re\scripts_re\autoname.py` -> `re\symbols\incoming\autoname.tsv` (4,738 new auto-named functions, RTTI/strings/thunks; see `notes\autoname.md`, merge pending). `progress_treemap.py` load_syms now folds rows to .pdata function starts (1393 -> 1399).
- Undo: delete the new files / `git checkout` progress_treemap.py.

## 2026-10-08 - testmatrix: menu_map.json generated from menu.cpp; run_pc.ps1 -LiveMenu verifies each toggle by log
- `tools\testmatrix\menu_map.py` parses menu.cpp (enum Id, kItems labels, Visible/ReadOnly, kExpIniKey) and emits per live matrix item the `ui.fsx keyseq` tokens: F10, Up/Down (wrap-around, skips invisible + read-only rows) from the parked "Close menu" row, Enter = toggle, back to Close, Enter. The cursor is NOT reset on reopen (g_sel persists), hence the park-on-Close invariant plus an `init` step (F10, Up, Enter) after the game starts. Row visibility is computed from the batch ini (alloff + live items at live_installed_value; GPU rows need Barriers!=0, R2a/R2b/R1 need Wine). Generated `menu_map.json` = PC defaults (17 items); `py -I menu_map.py --selftest` simulates the C++ menu state machine from the same parsed tables (shuffled/reversed toggles, both PC and Wine) - OK.
- `tools\ui.fsx keyseq` (new): sequential presses with waits; arrows sent with the EXTENDED flag (DirectInput DIK_UP/DOWN) - the old `key` command would have hit the numpad scancodes.
- `run_pc.ps1 -LiveMenu`: Invoke-MenuToggle checks the mod log for `MENU: <label> -> ON|OFF` (expected state) after every toggle; mismatch aborts that item and the rest of the batch (cursor unknown). Items without a map entry relaunch individually; -DryRun lists the map. Old runner used `U key F10` (not a hex VK) - fixed. Nothing run in-game.

## 2026-10-08 - collision bookkeeping pass: CB (DL + CS) built + proved offline (nothing applied, game not run)
- `patcher\collbook_live.fsx` (CB, default OFF; `apply` installs with both switches off): DL = skip the dead first CollisionPartMapFind (D83F4 `call` -> cave) on the global-owner path; CS = conditional stores at D867B (owner+110h) / D86A4 (+126h). Offline: site/branch/dead-register/flag/purity checks + CP real-code harness, 300 worlds, 0 mismatches (CB on/off, DL only, CB+CP, 5 concurrent jobs). Gain small (DL ~0.014 PC core); harness timing within noise.
- `patcher\collprefetch_live.fsx`: main now runs only when it is the invoked script (so CB can `#load` its harness); behaviour unchanged.
- notes\collision-opt.md (verdict: no algorithmic waste; candidates GP/OP/SI/OM), symbols `re\symbols\incoming\collision-opt.tsv` + `-renames.tsv` (not merged), output `re\scripts_wjobs\cb_offline_out.txt`.
- Undo: nothing applied; if applied live: `FT_ALLOW_WRITE=1 dotnet fsi --optimize- patcher\collbook_live.fsx revert` (or restart the game).

## 2026-10-08 - gamelogic: MissionThread / GameUpdateTick map (RE notes + symbols only; nothing applied)
- notes\gamelogic.md:
  - [App]MissionThread (~14 % of a core on the PC and the Ally) is a fork/join helper of GameUpdateTick (MissionThreadKick 233810 / MissionThreadJoin 233888) and is off the tick's critical path.
  - Its time is mostly cache-miss-bound walks of the task and object lists (E266B0, 2866F0, 20B900), the script VM (3.2 %) and spawn/despawn passes.
  - Same-behaviour candidates GL1-GL6, ranked. Only GL1 (prefetch in the three walks, ~2.5-3 % of a core, ~0.05-0.1 W on the Ally) is clean. TG / sub-rating / script-slice skipping are documented as out of scope (they change behaviour).
- re\symbols\incoming\gamelogic.tsv (41 rows) + gamelogic-renames.tsv (6 description fixes); merge left to the main session.
- Undo: delete the three files (no game, ASI or data change).

## 2026-10-08 - ASI 1.7.12: MQ, PT1, DL ported (built + tested, default 0, NOT sent to the Ally)
- New patches (all `[Patches]` keys default 0 in both inis, live menu switches 0/1/2, v2.02 only): **MQ** = bit-exact SSE MatrixFromScaleQuatTrans (88780; cave transcribed from `patcher\mq_live.fsx`, instruction stream diffed against the iced cave: identical apart from data refs), **PT1** = pool SRWLOCK held once per texture (5 sites 114CA8/114CEE/129651/12982E/1298B3; safe-revert = live switch only, bytes never restored), **DL** = skip the dead first CollisionPartMapFind (D83F4) as a second site of the **CP** group (`DL=1/2` installs the CP site group; CS not ported).
- `patches.json`: 8 sites + 4 deps (MQ body, PT1 phase-2 and 114C38 ranges, D8740 purity); `tests\test_patterns.py` ends ALL 114 SITES OK; all 7 harnesses pass. New targets after caveSprU (kLastTarget = cavePt1D).
- Stats: `stats cpu:` every 30 s (PT1 claims / lock pairs saved always; MQ calls and DL skips need new key `CpuStats = 1`). Menu rows MQ / PT1 / DL in 1.6.0 - OPT-IN; `tools\testmatrix`: MQ, PT1, DL enabled in matrix.json (live), menu_map.json regenerated (menu_map.py assert now kXDL).
- Docs: README section "1.7.12", settings line; ally-package .asi + README-MOD.md refreshed (ini: only keys added). MOD_VERSION 1.7.12. Zip on the Desktop (not committed): Nioh3EngineFix-1.7.12-ally.zip.
- Not run in the game (Ally playing / game not launched): first live check is a PC session with MQ=1 PT1=1 DL=1 CpuStats=1 and the `stats cpu:` line; a crash or visual change = set the keys back to 0.
- Undo: set MQ / PT1 / DL to 0 (they are 0 by default); revert this commit.

## 2026-10-08 01:10 - Code map: purpose regions for the full-game chart (analysis only, no game change)
- New `re\scripts_re\regions.py` writes `regions.tsv`, which splits .text into 3,023 contiguous regions with `start end label confidence evidence funcs code_bytes`. Each label is in "tech / plain" form, 41 labels in total.
- Evidence used: strings, imports, symbols.tsv and auto-names, the RTRig table, and our own rip-relative scan. It is spread over calls, vtables and shared module-local globals, then smoothed over address neighbours.
- Unknown purpose: 0.7 % of code bytes. Hold-out check: medium-confidence labels are 91 % right.
- `progress_treemap.py` now draws the full-game chart by purpose region and falls back to the old areas if regions.tsv is missing. The perf and GPU charts are unchanged. It also gained short block labels.
- New doc `docs\code-map.md` has the table, the method and the findings, and is linked from the README. Main finding: 22 % of the code (11.5 MB) is generated RTRig rig programs, reached through the `UnknownRTRig` table.
- Undo: revert this commit, or delete regions.tsv to get the old chart.

## 2026-10-08 live read-only probes (PC, game running)
patcher\live_reads_ro.fsx (VM_READ only). A4 meshlet table, GL1 list counts (proximity 4380), R1 page-pool counters (0 miss/0 qfull). See captures\pc\live_reads_2026-10-08.md. No game state changed.

## 2026-10-08 - ASI 1.7.13: pause hint from the game's menu signal, overlay offsets, per-thread FG state, plain-language menu, Esc x3
- Pause/options hint: now keyed to the game's own "a menu is open" getters (`0x5A37D4` screen object vfuncs 0x100/0x48, `0xD63C0` vfunc 8 of `[0x4758BA0]`: the two checks ApplyGraphicsSettings ORs into S+0x2055). Called on the Present thread inside SEH, bytes verified at start (pattern test: `menu: game 'menu open' getters ... OK`). `[Menu] PauseHintSource = Menu|StopFlags|Both` (default Both). Hint text `Mod menu: F10 / Esc x3 / Start x3`, also in the startup popup. The stop-flag path (G+58B/58C) stays as fallback; the old flags never changed in the options menu, hence no hint on the PC. Log: `MENU: game menu signal now OPEN/closed`. NOT verified in game (game running on the PC): if the signal is also true in gameplay, set `PauseHintSource = StopFlags`.
- Overlay: `[Overlay] OffsetX/OffsetY` (px at 1080p, scaled by height). Default TopLeft sits 160 px down (below the game's health/ki bars, ~15 % of 1080); menu rows Move FPS counter / Nudge counter right / down; Save to ini writes the offsets.
- FG state: presents counted PER THREAD (game thread = real fps, `AMD FSR Presenter Thread` = displayed fps; FG running = presenter > 1.15x real) because on the PC the presenter presents into the same swapchain (old per-chain count: x1.00 / 1 chain). Menu row `Frame gen: ON - running (R -> D fps)` green / `ON - paused (game menu open|loading|menu/loading/cutscene)` orange / `OFF` grey; counter gets an `FG` tag; log `FG state: ... [reason]` on every transition; stats line shows `threads game N/s presenter M/s`.
- Menu rewrite: plain names with the technical id small and dim, sections (Display & overlay / Frame generation / Performance fixes (safe) / Experimental / Diagnostics / Next launch / Save & exit), two-line explanation of the selected row (what, picture/timing, now/next launch, measured effect), scrolling when it does not fit. Row `Turn frame gen ON/OFF` (A toggles, Left/Right picks the type; Proton ON->OFF still next launch). Row `Game's own performance display`: nothing to toggle, the KTGL fps overlay is compiled out of retail (osd-design.md 1A; no config key exists).
- Escape x3 opens the menu (`[Menu] KeyTripleTap = Escape|None`), same tap logic as Start (TapWindowMs/TapGapMs/SwallowTaps), DirectInput-level swallow + replay of a lone press (game's Esc menu opens ~0.3 s later; None = old timing). Not run in game.
- Tests: `tests\test_patterns.py` ALL 114 SITES OK; all 7 harnesses pass; `tools\testmatrix\menu_map.py` follows kOrder/new kItems, selftest OK, menu_map.json regenerated (the `matrix.json` working-tree change is not mine and not committed).
- ini: keys added only (OffsetX/OffsetY commented, PauseHintSource, KeyTripleTap) in both inis. ally-package .asi + README-MOD.md + README-ALLY.txt refreshed (folder is git-ignored). Zip on the Desktop (not committed): Nioh3EngineFix-1.7.13-ally.zip.
- Undo: `KeyTripleTap = None`, `PauseHintSource = StopFlags`, OffsetX/OffsetY = 0; revert this commit.

## 2026-10-08 - ASI 1.7.14: GL1, LC, LW (built + pattern-tested, all default 0, NOT run in the game)
- New `[Patches]` keys in `Nioh3EngineFix.ini` (v2.02 only, installed at launch, no menu rows): **GL1** = prefetch of the task 3 entries ahead in the two mission task-list walks (E266D1 `GL1a` 10 B -> cave, E26690 `GL1b` 5 B -> cave; `prefetcht0` +40h/+80h/+C0h, bounded by `last`; r10 scratch, unused by the functions), **LC** = resource-load in-flight cap 10 -> 16 (`LCa` 1FEE98: `and r15,-9` -> `-15` and `add r15,0Ah` -> `10h`, keeps cap 1 for the boot flag; `LCb` 1FF3BA `mov eax,0Ah` -> `10h`), **LW** = manager `Sleep(1)` -> `WaitForSingleObject(event,1)` at 1FEE68 / 1FF18C / 1FF371 (`LWa`-`LWc`, `FF 15` -> `E8` + nop) and a signal in `ResourceStepJob_Run` (`LWs` 1343D35, 8 B -> cave: `or [rbx+24h],1`, `SetEvent` only while `lw_waiting` > 0). Faster zlib inflate not built (no cheap bit-exact drop-in).
- `patches.json`: 8 sites + 4 deps; `tests\test_patterns.py` ends ALL 122 SITES OK (new block: branch-into-range scan, loop exits, cap arithmetic, ecx=1 at the Sleep sites, job prologue); the 7 harnesses pass; build clean (one old C4456 warning). LoadStats prints the cap actually in force (`StartInfo::loaderCap`); with LW on its 3 manager-Sleep hooks are skipped (bytes differ).
- Docs: README "1.7.14", ini comments. MOD_VERSION 1.7.14. Binary `mod\Nioh3EngineFix\bin\Nioh3EngineFix.asi`. No zip made.
- First live check (PC or Ally): LoadStats baseline, then each flag alone on the same load route; GL1 vs MissionThread CPU. A crash or a stuck load = set the key back to 0.
- Undo: keys at 0 (default); revert this commit.

## 1.7.15 (2026-10-08)
- Mod-menu reminder top-right is now always on screen by default ([Menu] PauseHint = 2; 1 = only in game menus, 0 = off) and has its own menu row "Mod menu reminder (top-right)" next to "Show FPS counter". Undo: PauseHint = 1.

## 1.7.16 (2026-10-08)
- Esc x3 now toggles the mod menu both ways: the press that closes the menu mutes Escape for TapWindowMs, so the 2nd/3rd taps no longer leak to the game (they used to open the pause menu). Controller Start x3 already toggled. Undo: revert menu.cpp g_escMuteUntil.

## 2026-10-08 - Shadow redundancy audit (static only; nothing installed or patched)
- `notes/shadows-redundancy.md`: one provable same-look saving, S1 (VSM "Batch Tiles" draws past the per-frame tile budget are never sampled: all 356 VSM shader variants gate taps on tile-valid bit 0; Ally est. 0.1-0.4 ms/frame mean, 3-5.7 ms in camera-move bursts). ASI design `VBC` (4 call redirects, default 0) - not built. Write-offs S2-S7 (cascade phase-2 duplicate = false).
- Symbols: `re/symbols/incoming/shadow-audit-renames.tsv`. Scripts: `re/scripts_shadow/{gp_marker_digest,vsm_batch_waste,vsm_consumers}.py`.
- Undo: nothing to undo.

## 2026-10-08 - S1 / VBC re-check: not exact, not built (static only; nothing installed or patched)
- `notes/shadows-redundancy.md` §1b. The skipped "Batch Tiles" texels are never sampled, but the redundant work has cross-frame side effects:
  - the SMTREE2/SM-*Sfb PSOs write `uGeneral_FeedbackMap` (texture-streaming feedback);
  - the batch culling writes `uCacheCodes` / `uExistsNotLoadedModel` (meshlet streaming).
- With VBC, a waiting tile could later be marked valid with data that streamed in later than stock. That is not provably the same image.
- No VBC flag or code was added; the ASI stays 1.7.16.
- Undo: nothing to undo.

## 2026-10-08 - Load wall-time bottleneck (static + PC LoadStats; nothing installed or patched)
- `notes/loading-bottleneck.md`: PC title->Continue 58A window 25.8 s; the resource managers drain at +9.9..+13.0 s and LC/LW/GL1 change nothing, so the last ~15 s wait on an unmeasured LoadIsLoadEnd gate (G1-G8) or sequencer state; PSO, zlib, disk, Sleep polls, ready-wait and the 2.10 s + 0.59 s UI fades written off.
- 58A owners decoded (FastTravelSeqTick 3B2A04, WarpSeqBTick 5643A0, Pause::SetFlag); probe LG specified for the next PC/Ally load test.
- Symbols: `re/symbols/incoming/load-renames.tsv`. Script: `re/scripts_load/ls_digest.py`. Data: `captures/pc/loadstats/`.
- Undo: nothing to undo.

## 2026-10-08 - ASI 1.7.17: probe LG (LoadGates) + LoadStats = 3 (measurement only, both default 0)
- `[Profiler] LoadGates = 1` (needs LoadStats >= 1): pass-through hooks on the 9 gate calls of LoadIsLoadEnd 3B3424 (3B3443 3B3460 3B347B 3B3490 3B34A4 3B34D0 3B34F0 3B3506 3B3559; E8 sites verified by patches.json group LG, ALL 131 SITES OK). Per load: `LoadGates #n` evaluations, per gate calls / fails / first-failing share / held seconds / first+last fail / flip-to-pass (relative to the 58A set) and `holder: Gx`. 10 ms polls of g_FastTravelSeq [4757470] and g_WarpSeqB [47098A0] log `LoadSeq FT 5->6 +12.3 s`, the 58A owner at the set, [[45B9E40]+58h] and the blocking-widget flag.
- `LoadStats = 3`: the 1 s tick also runs outside the 58A window (while frames advance) with 58A / FT / B state / first failing gate.
- Game behaviour unchanged (same args in, same rax out). Test script: `tools\testmatrix\lg_ini.ps1` -> `fullmod_ini_1717_lg.ini`, `loadstats_ab.ps1 -Ini fullmod_ini_1717_lg.ini BASE_a BASE_b`. Not run in game (Ally holds it).
- Undo: `LoadGates = 0` / `LoadStats = 0`, or the 1.7.16 .asi.

## 2026-10-08 - ASI 1.7.18: BossLog (read-only boss fight logger, default off)
- `[Debug] BossLog = 1` (src\bosslog.cpp): entry hooks (relocated-prologue thunks, bytes verified, patches.json group BL, check sites) on Mission::BossBattleMode 23A2DCC / End 23A2F70, Character::Create impl 1180740, MissionFlagSet 70275C, UI::ShowBossGauge 23B8678, SetMissionClearFlag impl 1C1DB88. Logs fight #, timestamps, placement ids (ring of last 16 dumped at fight start), flag id+value in-fight and 20 s after, HP-bar actor id, mission id at clear. Rematch step 1 of notes\boss-fights.md section 9.
- Game behaviour unchanged (logger runs with arg registers saved, then the original instructions). Not run in game.
- Undo: `BossLog = 0`, or the 1.7.17 .asi.

## 1.7.19 (2026-10-08)
- FIX: 1.7.17 LG probe crashed the game during loads (Ally, 0xC0000005 at Nioh3.exe+3B3A56). LoadIsLoadEnd keeps r11 live across the LG6/LG7 calls (LTCG register allocation) and reuses it as rcx at LG8; the C++ gate hooks clobbered it. Gate call-site thunks now save/restore r10/r11 (NewSaveThunk). BossLog entry thunks also preserve r10/r11. Only affected LoadGates=1 / BossLog=1 (both default off).

## 1.7.20 (2026-10-08)
- ADD: BossLog ([Debug] BossLog=1, default off, read-only) now also logs boss rewards, tagged with the fight number, while a fight is active and for 30 s after BossBattleModeEnd: Character::AddDropTable/AddDropFixItem/AddDropKeyItemDirectGet/ForceDrop*/EnemyForceKillWithItemDrop, Item::AddItem/AddFixItem/AddMagicItem/AddItemNoNotice/AddEquipment(+BySlot/Preset), Player::AddGuardianSpirit(+NoNotice/WithTimer)/AddSkill/AddSkillDojo/AddMoney, Mission::SetReward/GetGameClearRewardNormal/Hard and GameClearRewardGrant. Script args are read from the VM value stack (SEH-guarded, no game calls); item names are not resolved (ids only). 24 new check sites BL7-BL30 (patches.json), same r10/r11-preserving entry thunk. Undo: BossLog=0 (nothing installed) or revert to 1.7.19.

## 2026-10-08 - Battle Scrolls research (static; nothing installed or patched)
- `notes/battle-scrolls.md` + `notes/battle-scrolls.csv`: Battle Scroll = shrine item 3 (visible on mission flag 0xAB) = mission replay over the mission table (LFM archive_03 entry 398, 130 x 0x198-B records, `MissionRecordGet` 0x377B38) + Picture Scroll/Emaki half (FeatureUnlocked bit 8 + flag 0x217). 62 listable missions with bosses; 3 duels excluded by flag 0x40.
- Symbols: `re/symbols/incoming/battlescroll-renames.tsv`. Scripts: `re/scripts_boss/bs_*.py`. LFM archives 02/06/15 extracted to `data/extract/lfm` (git-ignored).
- Undo: nothing to undo.

## 2026-10-08 - Eternal Rift / Mugen research (static)
- notes/eternal-rift.md: Mugen::* natives (0x23E4228, 0x23E41CC) are constant-return stubs, no script calls them; Rift = era-1 rows of the mission table (3 teachers x 3 tiers + 3 tutorials), no spawner or floor table. Names: re/symbols/incoming/mugen-renames.tsv. Nothing to undo.

## 2026-10-08 - Boss per mission, all missions (static; nothing installed or patched)
- Mission record file ids resolve through the shipped id table `archive_08\AE\50B70535...` (`g_LfmFileIdMap` 0x475A1A0, built by 0x29481E8). Enemy table = rec +0x54, mission id = rec u16 +0x158, script = rec +0x50.
- `notes/boss-map-all.csv` (130 rows: 86 bosses, 24 no flag-4 record, 20 unshipped), `notes/boss-duel-flow.md` §7, generator `re/scripts_re/boss_map_all.py`, names `re/symbols/incoming/placement-renames.tsv`.
- Undo: nothing to undo.

## 2026-10-08 - PC submit-path D3D12 overhead (static; nothing installed or patched)
- `notes/pc-submit-overhead.md`: per-frame call volume (PC: 1752 barrier calls, 1354 draws, 1081 dispatches, ~140 EI, >=1327 PSO sets; ~12-25k D3D12 calls), what KTGL already filters, ranked ASI state filters SF1-SF3 + PSO-key memo (exact, default off), why Barriers=1 costs 7 % on PC (dispatcher co-critical at ~180 fps, no GPU gain; R2a/R1 are Wine-only no-ops), B GPUBusy anomaly (-16 %: verify draws/frame before KEEP).
- New `re/scripts_d3d12/submit_redundancy.py` (gpuopt trace -> calls/frame + exact redundancy, --selftest).
- Undo: nothing to undo.

## 2026-10-08 - Ally load probe 1.7.19 digest (analysis only; nothing installed or patched)
- `notes/loading-ally-fixed.md` §7: the work ends at +93 s, with no timeout (manager A latency / Wine-serialized reads); 7.5 cores are an idle spin baseline; G9 hold = tip-page probe artifact; fixes GF+W6+RWb A/B, RC, AQ, TS, event wake; RP probe spec.
- New `re/scripts_load/ally_load_sp_phases.py`; names in `re/symbols/incoming/ally-load-renames.tsv`. Logs in `D:\nioh3-re\ally-logs\`.
- Undo: nothing to undo.

## 1.7.21 (2026-10-08)
- ADD: `[Debug] RematchLog = 1` (default 0, read-only, also installs the BossLog hooks): Phase 0 of notes/boss-rematch-design.md in src/bosslog.cpp. 29 new entry hooks (patches.json group RM, check sites, ALL 190 SITES OK): MissionLoadStep tick (Emaki table dump, state/queue/online change log, scroll item watch, mission IN/EXIT lifecycle), EmakiMissionRequest, MissionSetupEmaki, MissionRequestById/Enqueue, MissionStartFromSave, replay menu setup/open/list builder, BattleScrollTop_Select, item-use handlers, MissionRecordGet and the 3 clear/unlock getters (window-limited), unique-defeat serializer, ClearMapSetEntry, TrophyUnlock, mission result/retry/game-over natives, BuildSaveFilePath. Thunks now also pass the entry sp (caller RVA, stack args); thunk page 8 KB. Only game calls: the 3 pure getters inside the replay window.
- Correction found while decompiling: MissionRequestEnqueue rcx is the first 8 StartParams bytes by value, rdx the 0x58 request.
- Built, not run in game. Run plan: notes/boss-rematch-design.md "How to run Phase 0". Undo: `RematchLog = 0` or the 1.7.20 .asi.

## 1.7.22 (2026-10-08)
- ADD: `[General] AutoContinueLoad = 1` (default ON, user request) + mod menu row "Skip 'press button' after loading" (Gameplay; live, writes the ini key at once). The post-load tip page ("[DECIDE] Continue/Retry", UI slot 2Bh) closes by itself on the first frame the game would accept the press: its confirm check TipPageConfirmCheck 1CECAE4 is only reached after the page's load-done byte +71Bh is set by the mission sequencer (state 0Bh 223BE08, after LoadIsLoadEnd), and the mod makes that check's ConfirmPressed call (1CECAED -> 283950, patches.json AC1 + 2 deps, ALL 191 SITES OK) answer 1 through an al/flags-only thunk. Other gates (GameMain+3Fh, queued system message) still apply; nothing else calls the check. New src/autocont.cpp/.h. notes/auto-continue-load.md.
- Built, not run in game (death respawn / fast-travel routes unverified statically). Undo: `AutoContinueLoad = 0`, the menu row, or the 1.7.21 .asi.

## 2026-10-08 - hook register-safety audit + BossLog thunk xmm fix (built, not run in game)
- `notes/hook-register-audit.md`: every non-COM hook checked for LTCG-kept volatile registers (the 1.7.17 r11 class). One RISK: the BossLog/RematchLog entry thunk saved rcx/rdx/r8-r11 but not xmm0-5, while callers of MissionRecordGet / MissionClearedGet* / MissionUnlockedGet keep floats in xmm0-5 across those calls. Fixed in src/bosslog.cpp (thunk also saves xmm0-5; thunk page 16 KB). Only with BossLog=1 / RematchLog=1 (default 0). All other hooks safe; test_patterns ALL 209 SITES OK; built to bin\ (version unchanged).
- Tools: `re/scripts_re/hook_liveness.py` (--selftest re-finds the 1.7.17 r11), `hook_entry_audit.py`, site lists `hook_sites_*.txt`.
- Undo: revert src/bosslog.cpp.

## 2026-10-08 - GI HIGHEST redundancy audit (design only; nothing applied)
- notes/gi-highest-audit.md: no duplicated math at GI HIGHEST; the exact waste is empty early-out waves (light-group-1 main dispatches 95-99 % empty, 4 water-view dispatches >=95 % empty; ~0.65 ms PC / ~4-5 ms Ally E at HIGHEST). Actionable: A0 probe -> A1 (ASI-only light-group merge via gv_mask_param rewrite, ~0.7-1.9 ms Ally E, exact only if the probe shows the two dispatches differ in gv_mask_param alone). Water views: tile compaction/predication only (engine surgery), measure first. Write-offs: composite #9 not identity at 1.0, #15 exact copy but needs consumer rebinding, no duplicate probe updates, no history pass, #121/#414 search points differ.
- re/symbols/incoming/gi-audit-renames.tsv: 10 per-frame param setters called by GiRenderScalesApply.

## 1.7.23 (2026-10-08)
- ADD (measurement): `[Profiler] ReadProbe = 0` (1 probe, 2 + binary trace; needs LoadStats >= 1). New src/readprobe.cpp: all 13 ReadFile + the ReadFileEx call sites, FileReqAlloc CreateEventA / FileReqFree CloseHandle / FileReqWaitAll WFMO, the FsReadCacheLookup call; per-load "ReadProbe #n" lines (per site: calls, MB, in-call, duration histogram, size classes, OVERLAPPED vs file-pointer, sync vs overlapped handle, locality seq/near/inblk/reread/far, in-flight; top files; event costs; per-pool read/wait share; work-end split) + a calibration line at the title. LoadStats hands its 7 read sites to RP when it is on (same counters).
- ADD: `[Patches] LoadOnlyCpuFixes = 0` (src/loadonly.cpp): GF/W6/RWb on only inside the 58A load window, the user's values restored at the clear; RWb through a switchable cave (first instruction only; test_patterns "LO" proves word 0 == stock, word 1 == RWb).
- ADD: `[Patches] EP = 0`: event pool for the archive reads' CreateEventA/CloseHandle (only signalled events are reused; others really closed).
- patches.json RP1-RP18 + 3 deps (ALL 209 SITES OK), ls_sites.py READPROBE SITES OK, new tests/run_readprobe_harness.py ALL OK; other harnesses unchanged OK. notes/loading-ally-fixed.md 8.6.
- Built, not run in game. Undo: the three keys = 0 (defaults) or the 1.7.22 .asi.

## 1.7.24 (2026-10-08)
- CHANGE: `[Patches] EP` is now -1 = auto (new default): ON under Wine/Proton (ntdll wine_get_version), OFF on native Windows until the PC A/B; 0/1 force. Logs "EP: ini n (auto|forced ...) on <host> -> ON/OFF". Ally 1.7.23 result: EP=1 load 98.9 -> 50.1 s (CreateEventA 36 -> 0.2 us, CloseHandle 47 -> 1.4 us).
- FIX (log only): LoadOnlyCpuFixes was not silent: the 1.7.23 c log has "LO: loads-only gating on..." and "LO: on at 58A set #1" (grep "LO:", not the key name). Added an explicit "LoadOnlyCpuFixes = n (...)" line at start.
- Undo: EP = 0 in the ini, or the 1.7.23 .asi.

## 2026-10-08 10:40 - ASI 1.7.25 (built, not run in game): read fixes SR / RC / AQ / TSY / SH, all default off
- New src\readfix.cpp (+ readprobe.cpp routing, dllmain boot hook): SR synchronous redirect through private read-only handles
  (Wine only), RC read cache, AQ async archive reads, TSY TrySync threads (boot), SH synchronous archive handles (boot, Wine
  only). Design + Ally 1.7.23 data: notes\loading-ally-fixed.md 8.7. ini keys SR/RC/RCMB/RCKB/RCMaxReadKB/AQ/AQThreads/TSY/SH.
- Tests: READFIX HARNESS ALL OK, READPROBE HARNESS ALL OK, test_patterns ALL 212 SITES OK, READPROBE SITES OK. Built on 1.7.24 (EP auto).
- Undo: set the keys to 0 (default) or install 1.7.24.

## 2026-10-08 - ASI 1.7.26 (built, not run in game): PF trace prefetch (default off) + base ini fixes
- ADD: `[Patches] PF = 0` (1 record + prefetch, 2 record only), `PFThreads = 4`, `PFLeadMB = 768`, `PFTraces = 4`,
  `PFMatchPct = 50`. New src\prefetch.cpp/.h; readprobe.cpp's existing hooks at 631FD5 (ReadFileEx, ResourceLoad) and
  31305A / 3130D6 (archive chunks) call prefetch::Note (observation only, no new hook site). Each load's package reads are
  saved to `Nioh3EngineFix_pf<k>.bin` next to the ini; the next load that starts with the same reads gets those ranges read
  ahead (own read-only handles, <= PFLeadMB ahead of the game) so the game's reads hit the page cache. Game I/O untouched.
  Static analysis of the two sites + Ally protocol: notes\loading-ally-fixed.md 8.8. Expected Ally ~3-8 s per load (upper
  bound ~11 thread-s of cold reads), PC ~0.
- FIX (ini): control chars from unescaped paths (`re\scripts_d3d12\vkd3d_cost.py`, `notes\boss-rematch-design.md`), the
  duplicated [Memory] block removed (identical keys; Windows reads only the first), `[GpuOpt] InstallDelaySec = 20`
  documented (was read by gpuopt.cpp but missing). remaining-work 6.6.
- rp_sim.py: "PF sim" upper bound per site (selftest OK).
- Tests: PREFETCH HARNESS ALL OK (new tests\run_prefetch_harness.py), READPROBE / READFIX HARNESS ALL OK, test_patterns ALL
  212 SITES OK, READPROBE SITES OK, release\check_release.py 0 ini warnings.
- Undo: `PF = 0` (default; pf*.bin files may be deleted) or install 1.7.25.

## 2026-10-08 - release tooling (repo only; nothing in the game folder changed)
- `notes\remaining-work.md`: every item left to call the project done (owner, effort, needs-game).
- `release\`: per-device ini profiles (`profiles.json` -> `make_profiles.py`), regression test `check_release.py` (`--full` = test_patterns + harnesses), `package.py` (per-device zips: loader, asi, profile ini, guide, option reference, optional GHDM shader after a hash check, PROFILE.txt, SHA256SUMS; `-rc` while any default is provisional), `USER_GUIDE.md`.
- Undo: delete `release\` (no game-side effect).

## 1.7.27 (2026-10-08)
- ADD: `[Patches] VRROnline = -1` (auto, default): VRR suspended automatically while an online session is detected (net manager connected byte / play-mode mode word, notes\online-session-detect.md), restored after 5 s clean; 1 = force on, 0 = force off. Watcher thread only flips the caves' vrr_enable (no hook, register-safe). Logs "VRR: online session detected" / "back offline".
- test_patterns ALL 212 SITES OK, harnesses OK. Built, not run in game (co-op test pending, remaining-work 6.5). Undo: VRROnline = 1 or the 1.7.25 .asi.

## 2026-10-08 - item 3.3 post chain closed (repo only; no shader or ASI change, nothing in the game folder)
- Ally GP2 1.7.5 per-PSO data: game post chain (MB/DOF/bloom/compose/TAA) = 0.17 ms/frame, exe full-screen copies 1.44 ms
  (bandwidth-bound), FSR3 upscaler 7 ms (user setting). The 8-10 ms was a PC estimate; FSR3 had been labelled "motion blur"
  (runtime_inventory.py rule order, fixed + inventory / GP2 tables relabelled). No rewrite can show an Ally win, so none written;
  post-v1 (5 dxbc) stays staged, not shipped. Details + Ally A/B protocol: notes\gpu-post-runtime.md s7-8.
- New tools\testmatrix\shotdiff.py (screenshot A/B vs an A/A2 noise floor).
- Undo: none needed (docs/tools only).

## 2026-10-08 - crash Nioh3+2526D1 analysed offline (repo only; no ASI change, nothing in the game folder)
- RenderCmd_WriteToken95 2526C8 (RenderFrameBegin -> WaitSubmitThread -> PagePoolFrameUpdate): the first command-stream token of a frame faults, so dev+3C40 (reset to the stream base every frame end) is bad = stream not allocated/freed (device lifecycle). No patch, hook or switch of the mod writes on this path (W6/PP/R1 nearby keep the game's semantics); the ASI writes code only ~1 s after start. No guard added.
- The 10/06 15:54 core dump (61BBD, write to 0x1) is the old torn W1 live patch (new bytes in the dump, RIP mid `rep stosd`), already fixed by Mem.fsx safeWrite - a separate crash.
- Repro protocol (stock / ASI / panel, --enable_output_dump) in remaining-work 6.1; details notes\crash-2526D1.md. Follow-up: ApplyBatch re-snapshot of threads after suspend.

## 1.7.28 (2026-10-08)
- CHANGE: `[Patches] SR` is now -1 = auto (new default): 1 (load windows only) under Wine/Proton (ntdll wine_get_version), OFF on native Windows; 0/1/2 force (1/2 still refused off Wine). Logged at start: "SR: ini n (auto|forced ...) on <host> -> ON (load windows)/OFF". Same pattern as EP auto (1.7.24).
- Why: Ally R2 ABBA on 1.7.25 (EP auto in every arm): SR=1 47.25 -> 38.45 s (-8.8 s); site 740392 avg 47 -> 17 us per read, ~550k reads served, ~7.6k private handles, 0 failed. Cumulative Ally load ~94 -> ~38.5 s. Logs `captures\ally\1725\`.
- release\profiles.json ALLY: SR = -1 (kept). test_patterns ALL 212 SITES OK; READFIX / READPROBE / PREFETCH HARNESS ALL OK. Built, not run in game.
- Undo: SR = 0 in the ini, or the 1.7.27 .asi.

## 2026-10-08 - Ally GPU budget from measured data (repo only; game not run, nothing in the game folder)
- notes\ally-gpu-budget.md: ranked Ally GPU tables, gameplay windows only, from logs on disk. Two tables: USER preset
  (LIGHTEST, REN_RAT 50; GP1 markers 1723/1725, ~31 ms frame) and MAX native (175-L2 GP2 per-PSO, ~61 ms frame).
- Found: the gp2_L*_full tables averaged 26 menu windows of 38. FSR3 dominated them and gameplay PSOs were cut ~5-6x
  (RefProbe #121 0.56 -> 3.14 ms, compose 0.093 -> 0.46, exe blits 1.44 -> >= 3.2). The 175 logs are REN_RAT 100, not 540p
  (render-res FSR passes 3.7-4x the 50 % run, output-res passes equal).
- FSR3 = 3.7 ms at the user's REN_RAT 50 and 7.0 ms at native AA (M). Fit 2.56 + 4.43 r^2 ms; the output-res part ~2.6 ms is fixed.
- Section 3 re-checked: 3.2 GI merge 0.7-1.9 -> ~0.25-0.5 ms MAX / ~0.03 USER (E, anchored). ESM_fast, MPC#23 and ESM 2048
  values were PC-scaled. Top targets: FSR3 accumulate 2.1 ms, culling chain 3.5-4.5 ms, VSM 2.6-3.8 ms (USER).
- New re\scripts_gpu\ally_budget.py (gameplay-window filter for GP1 / GP2 logs). remaining-work s3 updated (+3.8 FSR3 row).
- Undo: none needed (docs/tools only).

## 2026-10-08 - WDO#14 VSM page-flag dedupe staged (repo only; nothing in the game folder changed)
- New `re\scripts_gpu\build_wdo14.py` + `gpu-shaders\vsm\` (flag_dedupe.py rewrite, verify_vsm.py proof): `0x1667d3a0.file` #14 with 36 of 43 page-flag loads behind a same-page cache. Proven bit-exact offline (psim 120 scenes x 600 px, interface parts identical, signed). Staged as `gpu-shaders\replacement\0x1667d3a0.file` (sha256 60e2646c...), default OFF. RGA: Ally-class VGPR 40 -> 48. Notes: `notes\gpu-wdo14-flag-dedupe.md`.
- Install for the A/B (game closed): `gpu-shaders\apply_wdo14.ps1` / `./ally_shaders.sh apply wdo14`; matrix id `S_wdo14`.
- Undo: `apply_wdo14.ps1 -Revert` / `./ally_shaders.sh revert wdo14`, or Steam verify.

## 2026-10-08 - ASI 1.7.29: GIProbe (A0) + GIMerge (A1) GI gather light-group merge (default off; nothing in the game folder changed)
- New `mod\Nioh3EngineFix\src\gimerge.{h,cpp}`; gpuopt thunks get a second hook mask + a held-list slot and can skip a held Dispatch. `[Debug] GIProbe = 0`, `[Patches] GIMerge = 0` (2 = observe), `GIMergeQualify = 60`. With both 0 nothing is installed.
- Merges the two stencil light-group passes of RefProbe #121/#414 into one when they provably differ only in gv_mask_param.xy (notes\gi-highest-audit.md s6). Expected ~0.25-0.5 ms Ally at MAX, ~0.03 ms at the user preset (E). Offline: `tests\run_gimerge_harness.py` ALL OK, gp/r2 harnesses, test_patterns. Not run in game; Ally A/B protocol in the note.
- Undo: GIMerge = 0 / GIProbe = 0 (next launch), or the previous .asi.

## 1.7.30 (2026-10-08)
- CHANGE: `[Patches] LC` accepts a cap value: 0 = off (default), 1 = 16 (as before), 11..64 = that in-flight cap for both resource-load managers (LCa 1FEE98 `and r15,-(N-1); add r15,N`, boot flag 1 still gives 1; LCb 1FF3BA `mov eax,N`). Byte immediates only (no cave, no hook). Log "LC: resource-load managers A/B in-flight cap 10 -> N (ini LC = n)"; invalid values are refused and logged.
- Why: Ally 1.7.25 R2 digest (notes\loading-ally-fixed.md 8.9): the G8 StageAreasObjectsReady wait (~15.5 s of 38.5) is manager A draining ~1000 queued requests at cap 10 while its pools, the disk and ~5 cores sit idle. Expected 3-8 s per Ally load at 16-32, 0 if the 3 TrySync threads are the limit (then TSY = 6). Not run in game; Ally session R4 protocol in 8.9.
- Tests: test_patterns ALL 212 SITES OK (+ LC = 11..64 arithmetic and immediates-only proof), READFIX / READPROBE / PREFETCH / LOADSTATS HARNESS ALL OK.
- Undo: LC = 0 (default), or the 1.7.29 .asi.

## 1.7.31 (2026-10-08) - PJ PSO journal + title-screen prewarm (default off)
- ADD: `[Patches] PJ = 0` (1 = journal + prewarm, 2 = journal only), `PJ_MaxMB = 64`, `PJ_QuietMs = 3000`. New
  src\psojournal.cpp/.h: hooks ID3D12Device CreateGraphics/ComputePipelineState, CreateRootSignature and
  ID3D12Device2::CreatePipelineState on the engine's device (reported by psoswap's D3D12CreateDevice IAT hook; psoswap gets
  `onDevice` / `observeOnly`, its own behaviour unchanged), journals every PSO the engine creates once to
  `Nioh3EngineFix_pj\pj_<os>_<gpu>_sm<NN>.bin` next to the ini (new file in the game folder when PJ != 0), and on the next
  launch creates + releases the journaled PSOs the engine has not created yet on one lowest-priority thread once the title
  is idle (pauses during loads). Engine calls pass through with their own arguments/results; nothing in Nioh3.exe patched.
- Decompile check + PL decision (dropped, no frame gain) + PC/Ally test protocol: notes\stutter.md 3.1 / 6.1.
- Tests: tests\run_pj_harness.py ALL PJ CHECKS OK (WARP device, 6 launches: dedupe with padding garbage, prewarm descs
  identical below PJ, no growth, cut tail, corrupt shader, other build), test_patterns ALL 212 SITES OK. Built + tested on the pre-rebase tree; the rebuild after rebasing onto 1.7.30 (LC) is pending (PC commit limit during a game test). Not run in game.
- Undo: `PJ = 0` (default) and delete `Nioh3EngineFix_pj\`, or install 1.7.30.
## 2026-10-08 - ASI 1.7.32: DQ main-loop retry quantum + P1E7 texture-loop prefetch (default off; nothing in the game folder changed)
- remaining-work 2.5 measured from the existing PC captures (`notes\mainthread-25.md`): [App]Game K1 drain 0-0.19 ms/frame, D retry spin 0.08-0.96 (median 0.58), P1E7 target loop 0.28-0.46, C glyph rebuild 0.10-0.22. Built the two >= 0.2 ms; K1 and C not built.
- New `mod\Nioh3EngineFix\src\drawprep.{h,cpp,asm}`: `[Patches] DQ` (+ `[Tuning] DQ_QuantumUs = 200`) replaces the spin's exit test at 1A74C45 (the 1A744E4 xmm6 load runs once before the loop, so it cannot be patched live); `[Patches] P1E7` = cave at 1E72C6 (port of meshlet_caves.fsx). 0 / 1 / 2 (= installed, off). DsQpc honours the DQ quantum. patches.json sites DQ / P1E7 (kind check) + deps.
- Offline: test_patterns.py (new drawprep checks), `tests\run_drawprep_harness.py` ALL PASSED (caves vs original bytes, full register state). Not run in game; A/B protocol in the note. matrix.json entry pending (remaining-work 4.2).
- Undo: DQ = 0 / P1E7 = 0 (next launch), or the previous .asi.
| 2026-10-08 | Nioh3EngineFix ASI 1.7.33 (mod only; source change, nothing installed by this entry): AutoContinueLoad is now OFF by default (src default 1 -> 0, ini, profiles PC/ALLY = off/user option). The mod-menu row is renamed "Skip loading tip screen" (AutoContinueLoad; "Presses Continue for you when a load finishes."); it already switched live (the thunk reads the switch byte each time the tip page checks) and wrote the ini at once. Test harnesses (abba/abba2/final/run/loadstats_ab, matrix.json alloff) force AutoContinueLoad = 1 in their own ini. Docs: USER_GUIDE, README, notes\auto-continue-load.md. | Menu row on, or `[General] AutoContinueLoad = 1`, or the 1.7.32 .asi |
## 2026-10-08 - Hardware-compatibility review (docs only; no code, nothing installed)
- Read-only review of every ini key, all src modules/caves, the shader pack and release profiles for NVIDIA / Intel Arc / other AMD GPUs and Intel CPUs: `notes\hardware-compatibility.md` (feature -> class -> works? -> benefit -> risk/guard). README: new section "Will this work on my PC?".
- Result: no cross-vendor risk in any default-ON or profile feature, so no code change and no version bump. No vendor SDK calls; caves are baseline x64 (MQ's SSE4.1 is checked; AABB / MenuQuantize use SSE4.1 unchecked, default off). No shader uses wave intrinsics or assumes wave size; GHDM is bit-identical by construction on any D3D12 GPU, gain measured on RDNA3/4 only. Keep AS0 (Intel Arc risk; no-op on NVIDIA) and GpuOpt R2a/R2b/R1 (RADV-specific) off; suggested guards are in the note.
- Undo: n/a (documentation).

## 2026-10-08 - ASI 1.7.34: FnProbe + play session kit (tools\playsession; nothing installed by this entry)
- ASI 1.7.34 = 1.7.33 + `[Debug] FnProbe` (default 0, measurement only; src\fnprobe.cpp, fnprobe_core.h; patches.json group FP):
  entry thunk on the 14 last Unknown_ functions of remaining-work 10.1 (or `FnProbeList` RVAs): hit count (one lock xadd),
  hits/s + peak 1 s, first/last hit, first thread, top 4 callers, first `FnProbeArgs` hits' rcx/rdx/r8/r9 + 16 bytes behind
  pointer args with hash-looking dwords; log every `FnProbeReportSec`. Saves rax/rcx/rdx/r8-r11, xmm0-5, flags; installed
  with all other threads suspended (none inside a patched prologue). Tests: tests\run_fnprobe_harness.py ALL PASSED
  (register-exact, 4-thread exact count, decoder vs capstone on 6614 functions, no branch into patched bytes), test_patterns
  ALL 228 SITES OK. Also fixed the RematchLog start line's `\b` escape. Not run in game yet.
- `tools\playsession\` (README.md): desktop shortcuts 'Nioh3 Session START' / 'Nioh3 Session STOP'. START = save backup to
  D:\nioh3-re\save-backups\<stamp>-session, bridge game lock (refuses if the Ally holds it), keeps the game folder's ini+asi,
  installs bin\ 1.7.34 + the user's own ini with capture keys (FnProbe BossLog RematchLog MemStats LoadStats LoadGates
  CpuStats TS SPR = 1), PresentMon 2.6 v2 capture + monitor.ps1 (Windows counters, mod log per launch). STOP = restore ini+asi
  byte-identical, release lock, summarize.py -> captures\playsession\<stamp>-session\report.md. Dry run (test_session.ps1)
  DRY RUN OK; summarize.py --selftest OK. No input injection anywhere.
- Undo: delete the two desktop shortcuts. A session in progress is undone by STOP (or copy `<session>\orig\*` back to the game folder).

## 2026-10-08 - ASI 1.7.35: PadLatency controller latency measurement (default off; nothing in the game folder changed)
- ADD `[Debug] PadLatency = 0`, `PadLatencyReportSec = 30` (new src\padlat.cpp/.h, padlat_core.h; notes\pad-latency.md). Measurement only:
  chains the game's XInputGetState import (XINPUT1_4 #2, the only pad API besides the Sony HID path; no RawInput/GameInput), stamps each
  pad change as the game reads it, follows it through the TS ring (C->L->W->P, PadLatency switches TS on) to the next hooked Present and to
  that frame's GPU completion (own fence, one Signal per frame on the game's queue). Logs per window + session total: changes, input->present
  and input->GPU-done p50/p95/p99, the game's poll interval p50/p95/p99. osd keeps its Present hook when PadLatency is on.
- Tests: tests\run_padlat_harness.py PADLAT: ALL PASSED (exact timeline stock 32.257 / IK 23.540 ms, GPU, poll, expiry, ring, real hook
  pass-through + cost ~40 ns/call, dump imports, wiring), test_patterns ALL 228 SITES OK, WG harness ALL PASSED. Not run in game.
- Undo: PadLatency = 0 (default), or the 1.7.34 .asi.

## 2026-10-08 - ASI 1.7.36: Tour mode for the stability / performance test (default off; nothing in the game folder changed)
- ADD `[Debug] Tour = 0` (+ TourDwellSec 4, TourPointsPerStage 3, TourList, TourPasses 1, TourLoadTimeoutSec 120, TourStartDelaySec 5,
  TourSettleSec 3; new src\tour.cpp/.h, tour_core.h; patches.json group TR = TT1 SB1 SB2 SB3; notes\tour-mode.md). With Tour = 1 (unlocked
  missions) / 2 (all) the mod loads each TourList mission through the game's own Mission Replay request (MissionRequestById 2239600) and
  fast-travels the player (FastTravelRequestPost 22395AC, request from FillWarpPoint 3B2528 with point id 0) to the arrival spot + the
  farthest warp points, dwells and logs one `Tour P` line per point (fps, frame-time p50/p99/max, CPU cores, main-thread busy, private /
  working set / handles / threads / system commit, VRAM, load + warp seconds). GPUProf / SP windows follow the dwells (gp::LogWindow,
  sp::Begin/EndWindow; memstats::Vram). Resume after a crash from Nioh3EngineFix_tour\progress.txt. Refuses online (net / play-mode signals).
- SAVE SAFETY while Tour != 0 (whole session): SaveFileWriteAtomic call 5B6F37 and BACKUP.BIN writer calls 1FDF2D0 / 1FDF62A report success
  without writing; the game's CreateFileW / MoveFileExW / MoveFileW / DeleteFileW / CopyFileW / SetFileAttributesW imports deny writes
  into ...\KoeiTecmo\NIOH3\Savedata (count logged, expected 0). tools\tour\run_tour.ps1 also backs up the save folder (SHA-256 manifest),
  reports any change after the game exits and restores byte-for-byte.
- tools\tour\: run_tour.ps1 (lock, backup, ini, PresentMon, Steam launch, watch DONE / crash / stall, relaunch + resume, collect, restore,
  report; user presses Continue at the title, no input injection), summarize_tour.py (per stage, worst points + GPU/CPU bound, opportunities
  ranked by ms x points: GP passes, GP2 PSO hashes, SP functions by symbol; leak check pass 1 vs 2), fake_game.ps1 (dry run),
  tourlist_main.txt (15 main missions, ~15 min PC) / tourlist_full.txt (27).
- Tests: tests\run_tour_harness.py ALL PASSED (44 checks: list / resume / sampling / p99 / save path, real thunk bytes: tick pass-through of
  int + xmm + stack args, SB1/SB2 blocked and pass-through; dump heads, E8 sites, single SaveFileWriteAtomic caller, KERNEL32 imports;
  wiring), test_patterns ALL 232 SITES OK, GP and R2 harnesses ALL PASSED, run_tour.ps1 -DryRun verdict done (crash + resume + save
  scribble detected and restored, report written). Not run in game.
- Undo: Tour = 0 (default), or the 1.7.35 .asi.

## 2026-10-08 - Ally GPU budget from the 1.7.31 GP2 session + fogpar shader item (staged OFF; nothing in the game folder changed)
- notes\ally-gpu-budget.md: USER preset per-PSO table (S1, 95 % of 28.7 ms attributed), U1-U6 upscaler arms, REN_RAT 67 = illegal enum value (not a clamp), RCAS at sharpness 0 is still a sharpen (skip rejected), VCLOUD / copies / PB2 / VFSCAT cost analysis + ranked same-look fixes, Ally A/B protocol (s7).
- ADD shader item `fogpar` (gpu-shaders\fog\build_fogpar.py, vf235_par_src.hlsl): VFSCAT #235 with only the merged early-exit VSM level search. Bit-exact on 2,688 interpreter seeds; RGA gfx1103 96 VGPR / 5 waves (= stock), VMEM loads 101 -> 79. Pack 0xa9910db8.file sha256 81007b3f...; it supersedes the dropped `fog` row in ally_shaders.sh / apply_fog.ps1 (old sha recognised for upgrade/revert). No ASI change (no version bump).
- Undo: `./ally_shaders.sh revert fogpar` (Ally) / `apply_fog.ps1 -Revert` (PC), or Steam verify. Not installed anywhere yet.
## 2026-10-08 - ASI 1.7.37 / 1.7.38: VirtualPad + KeepRunningUnfocused + test popup (debug, default off; nothing in the game folder changed)
- ADD `[Debug] VirtualPad = 0` (1 exclusive / 2 merge), `VirtualPadIdleReleaseSec = 600`, `KeepRunningUnfocused = 0` (src\vpad.cpp/.h,
  vpad_core.h; patches.json group VP0-VP3 check-only; notes\virtual-input.md). The game's XInputGetState IAT answers slot 0 from
  `\\.\pipe\nioh3vpad` (press / hold / release / stick / trigger / key / menu get|set on the Present thread / state / realpads); virtual
  keys via the menu's DirectInput hook (real keys hidden while unfocused). KeepRunningUnfocused retargets IsAppActive calls 436758 /
  436A20 / 89B2FD to an al/flags-only thunk (pads + no focus-loss pause; mouse / keyboard / cursor keep the stock answer) and opts the
  process out of power throttling.
- tools\testmatrix: vpad.py / vpad.ps1 backend, `abba2_pc.ps1 -Input vpad`, `tools\tour\run_tour.ps1 -Input vpad`, vpad_live.ps1;
  testpopup.fsx = "test running" popup + tray with an opt-in Block my input / Release toggle (LL hooks, real input only; release by
  button / tray / Ctrl+Alt+Shift+F12 / 30 min / harness or owner exit), wired into abba2, tour, vpad_live and playsession START.
- Live 2026-10-08 (PC, game never in the foreground): title -> Continue -> field in 31 s, menu item set via the pipe (MENU line
  logged), stick moved the player 367 units, clean close. Offline: run_vpad_harness / test_patterns 236 sites / vpad.py selftest /
  testpopup selftest / dry runs ALL PASSED.
- Undo: VirtualPad = KeepRunningUnfocused = 0 (default), or the 1.7.36 .asi.

## 2026-10-08 - ASI 1.7.39: MA main-thread plain loads + live DQ / P1E7 rows; PC CPU ranking (default off; nothing in the game folder changed)
- notes\pc-cpu-next.md: PP KEEP checked (the 'PP scans 110 skipped 0' lines are the OFF windows; ON reads 28/84; -0.26 ms/frame = the
  removed 2523FC work, readers use absolute counts). PC critical path from r5-r9 + run5/run6: [App]Game (render executor) waits ~0.6-0.7
  ms/frame for the logic thread (KIDSTaskScheduler0) in the abba2 scene; ranking AABB > DQ > P1E7 > MA (measured targets, est. savings);
  run5/run6 ran 116-118.5 fps against the 120 cap (compressed); test plan Blocks A-C with -MaxFps 240.
- ADD `[Patches] MA = 0` (code bytes, next launch): 25 `xor eax,eax; lock cmpxchg [m],<zero reg>` atomic loads in 4AA34 / 4B104 / 419A8 /
  1E835B -> `mov eax,[m]` + NOP (patches.json group MA, deps = FNV of the three bodies + 1E8359). Proofs: re\scripts_cpu\ma_zero.fsx (Iced
  reaching-definition: the source register is zero on every path, all chained pieces), mainthread_patches.fsx (operands, flags),
  test_patterns.py MA transform check.
- Menu: live rows 'Shorter no-frame retry (DQ)' / 'Texture-loop prefetch (P1E7)' for the 1.7.32 switches (Save keeps 2 = installed + off),
  next-launch row 'Plain loads in main thread (MA)'. VirtualPad `menu set DQ|P1E7|MA`.
- tools\testmatrix: abba2_pc.ps1 -Arms (explicit launch order) / -MaxFps (writes [VRR] MaxFps) / live items absent from the base ini
  installed off; abba2_report.py 3+ BASE launches -> interpolated baseline, live DQ / P1E7 read-back, cap-compressed flag (selftest OK);
  matrix.json DQ / P1E7 live, MA item; menu_map.py accepts the new rows.
- Tests: test_patterns ALL 261 SITES OK (MA 25/25 + 4 deps), run_drawprep_harness ALL PASSED, run_vpad_harness ALL PASSED, menu_map
  selftest OK, abba2_pc.ps1 -DryRun OK, build OK. Not run in game.
- Undo: MA = 0 (default), or the 1.7.38 .asi.
| 2026-10-08 | RE only (no game change): perf-rename-4 symbol rows (110 new + 14 renames, `re\symbols\incoming\perf-rename-4*.tsv`) bring perf_coverage.py named coverage to 100.00 % in the attributed and --self views; new `--precise` flag | none (delete the two incoming files) |
| 2026-10-08 | Docs only: notes/user-settings.md (ranked user-side PC and Ally settings, labelled measured / community / estimate; nothing applied to any device) and pointer in notes/remaining-work.md 7.4. Undo: delete the note. |
| 2026-10-08 | Docs only (no game change, nothing installed): notes/whats-left.md - audit of what is left to optimize: critical path per device, hot functions with no study, first SP digest at the Ally USER preset with GF on (main 62 % game code; R2a/R1 never run on the Ally; GFX gaps 1.67 ms/frame), 16 ranked candidates, boss-session profiler settings, top-3 plans; pointer in remaining-work.md. | delete the note and the pointer line |

## 2026-10-08 release: one mod for every OS
- release\profiles.json now has a single profile `ALL`; package.py writes ONE zip `Nioh3EngineFix-<ver>[-rc].zip` (PC/ALLY split removed, no other references). OS-dependent keys (W6, GF, MQ, W3_TimeoutMs, MaxFrameLatency) are `-1 = auto by OS`; ASI side on branch `osauto` (pushed, not merged), 261 sites OK. Both DXIL shaders ship in the zip. Details: notes\release-os-profiles.md.

## 2026-10-08 - ASI 1.7.40 (branch `bounds`): BPO BoundsFromPartObbs 71C60 caves (default off; nothing in the game folder changed)
- notes\bounds-partobbs.md: 71C60 per part = 2 locked ready-word reads (71DB2, 1A7AE2), 6 half unpacks, a quaternion
  rebuilt from axis-angle halves every frame (SinCosApprox 54360 + sqrt + div; pure function of 8 static bytes), 5
  Vec4CopyW0 copies whose 16-byte loads fail store forwarding (174613 hot), ObbToAabb. Target 0.27 + 0.10 + 0.12 + 0.045 ms.
- ADD `[Patches] BPO = 0` (1 on, 2 installed off; live row 'Faster part bounds (BPO)'), `[Tuning] BPO_Mask = 7` (1 Q
  quaternion memo, 2 L plain ready loads, 4 V forwarding-friendly copy), `BPO_TableBits = 14`. Caves caveBpoQA/QB/L1/L2/V
  (caves.asm), patches.json group BPO: 10 sites + 4 deps. Q = seqlock table keyed by the 8 halves (re-packed from the
  floats the stock code used) and tagged with MXCSR & E040h. L2 also changes 1A7AAC's 9 other callers (plain load).
- Proof: re\scripts_logic\bounds_offline.fsx (whole 71C60 from the dump, stock vs patched): 200k calls x masks, 4 x 65536
  half sweep, poisoned seq, 7 MXCSR modes + cross-mode, 4-thread stress: 0 bitwise differences; path-sensitive liveness of
  every register / flag / stack slot the caves leave different; MASM = Iced caves (175 instructions). Harness timing
  99 -> 84 ns/part (-15 %, Q -11 %). Est. 0.12-0.25 ms/frame of Scheduler0 (Q ~0.10-0.13).
- Tests: test_patterns ALL 271 SITES OK, menu_map selftest OK, build OK. Not run in game.
- Test: `abba2_pc.ps1 -Live AABB,BPO -LaunchFlags none -Quads 3 -MaxFps 240 -Input vpad`.
- Undo: BPO = 0 (default), or the 1.7.39 .asi.

## 2026-10-08 - ASI 1.7.40 (branch `rematch`, not merged): boss rematch Phase 1 MVP, route R-a (default off; nothing in the game folder changed)
- NEW `src\rematch.cpp/.h` (`[Rematch] Enable = 0`): game-thread pump on MissionLoadStep 223AF50 + BossBattleMode/End (chained onto
  bosslog's hooks when BossLog / RematchLog is on), s.2 gates, R-a = `MenuOpenMissionReplay(0, 0, 6)` 1DE28E8 (the game's own Battle Scroll
  replay list), cleared-mission scan (702B64 per catalogue row), `HiddenDuels = 1` un-hide of 0xD22D / 0xB3F8 / 0xFB96 (record +0x40 bit
  0x4000, snapshot + restore), R-b `DirectStart` path present but always refused (needs the L7 replay capture). No story-flag writes.
- Menu: section 'Gameplay (solo/offline)', row 'Boss rematch...' -> sub-page (cleared bosses, confirm line, abort row), OSD toasts;
  VirtualPad `menu get|set Rematch`. Catalogue `Nioh3EngineFix_bosses.tsv` (62 rows) from `tools\rematch_catalog.py`.
- Byte checks: patches.json RM30-RM36 (call target + code references, check only) + RM1 / BL1 / BL2 / RM14 / RM15 / RM17.
- `tools\rematch\p0_vpad.ps1`: unattended VirtualPad-only Phase 0 capture (P0-5 replay menu + L7 replay start; optional P0-1), -DryRun OK,
  not run. Notes: `notes\boss-rematch-build.md` (design corrections: duel bit 0x4000, queue slots are not cleared).
- Tests: test_patterns ALL 268 SITES OK, run_rematch_harness ALL PASSED (chained thunks, gates, R-a, un-hide / restore lifecycle,
  refusals, catalogue), run_vpad / padlat / tour harnesses ALL PASSED, build OK. Not run in game.
- Undo: `[Rematch] Enable = 0` (default), or the 1.7.39 .asi.

## 2026-10-08 - ASI 1.7.40 (branch diag, merged in integ as 1.7.40): GQ graphics-queue gap census + JT job-timing probe (diagnostics, default off; nothing in the game folder changed)
- ADD `[Profiler] GQ = 0` / `GQ_Top = 5` (gpuprof.cpp, needs GPUProf = 2, Windows + Proton): per DIRECT list Reset in a sampled frame a
  head timestamp before its first GPU command (ResourceBarrier via gpuopt, BeginEvent, GP2 slots, ExecuteBundle / Discard /
  WriteBufferImmediate / BeginRenderPass / Barrier) and an end stamp at Close; ECL order + call site from the overlay's ECL hook. Per
  frame: GAPS between consecutive lists, list HEADS, gaps around unmeasured submissions, top edges [marker -> marker] and largest gaps
  with ECL call sites; closure against GP2 span - covered. One-line hooks in gpuopt.cpp (HkResourceBarrier) and osd.cpp (ECL).
- ADD `[Profiler] JT = 0` / `JT_Thread` / `JT_Top` / `JT_ReportSec` (jt.cpp, jt.asm; `[Patches] JT` accepted too): patches.json group JT,
  5 sites written only when 1 - JobWorkerPoll job call 40E4E (QPC around every job, inner callback / ParallelFor range + grain), STL
  condition_variable wait / wait_for B616B7 / B616F7 (watched thread only) and notify_one / notify_all B61674 / B6165C (only for the CV
  the watched thread sleeps on). Per window: blocked ms per logic tick, wait sites, releasing thread + waker site, critical job, longest
  overlapping job, other threads' busy %, job table with leaves / grain. Measurement only.
- tools\gqjt_digest.py (new): names RVAs from symbols.tsv and prints the decisions (merge lists / R2; reorder / split jobs). notes\gq-jt-probes.md.
- Tests: test_patterns ALL 266 SITES OK, run_gp_harness modes 1-4 (new GQ mode) ALL PASSED, run_jt_harness (new) ALL PASSED, r2 / padlat /
  tour / vpad / wg harnesses ALL PASSED, digest selftest OK, build OK. Not run in game.
- Undo: GQ = 0 and JT = 0 (defaults), or the 1.7.39 .asi.

## 2026-10-08 - ASI 1.7.40 integration (branch `integ`): bounds + rematch + osauto + diag + AABB fix in one build (nothing in the game folder changed)
- One version 1.7.40 for all: BPO BoundsFromPartObbs caves (`bounds`, default 0), boss rematch Phase 1 MVP (`rematch`,
  `[Rematch] Enable = 0`), `-1 = auto by OS` for W6 / GF / MQ / W3_TimeoutMs / MaxFrameLatency (`osauto`, base ini -1),
  GQ gap census + JT job probe (`diag`, built as 1.7.41, renumbered 1.7.40; GQ = 0, JT = 0).
- Conflicts resolved: MODLOG (all entries kept), dllmain.cpp (version comments; cave Target switch BPO + JT; kLastTarget = caveJtWakeAll),
  gen_header.py cave list (BPO then JT), patches.json (BPO + JT sites and deps), CMakeLists / build.bat (rematch + jt.cpp/jt.asm),
  test_patterns.py (Rematch + JT blocks); patches_gen.h regenerated. menu.cpp (kXBPO / kRematch / auto) merged clean.
- AABB fix (notes\run7-analysis.md s2): `AABB = 2` now installs the cave switched OFF (1.7.39 switched it on for any nonzero); boot log
  line `AABB: ... switch ON/off`; menu Save keeps 2; matrix.json AABB `live_installed_value` 2; ini comment.
- tools\testmatrix\menu_map.py: Visible() parser maps `rematch::Installed()` to `[Rematch] Enable` on a known build. main 472795b merged.
- Tests: test_patterns ALL 283 SITES OK, run_rematch / vpad / padlat / tour / drawprep / jt / gp harnesses ALL PASSED, menu_map selftest OK,
  bounds_offline.fsx OFFLINE CHECKS ALL PASSED, build OK (idle priority). Not run in game.
- Undo: the 1.7.39 .asi.
- 2026-10-08 tools\playsession STOCK mode (tooling only; this change does not touch the game folder): `start_session.ps1 -Stock [-KeepOptiScaler]` moves an exact allow-list of mod/OptiScaler files (tools\playsession\README.md 'Stock mode') into `D:\nioh3-re\stock-stash\<stamp>-stock` with a sha256 manifest for an unmodded run; `stop_session.ps1` restores them (hash-verified, never overwrites newer files, works after a crash/reboot with no session state). `summarize.py compare <mod> <stock>` prints the mod-vs-stock table. New desktop shortcut 'Nioh3 Session START (stock)'. Undo / missed STOP: click 'Nioh3 Session STOP' (restores any unfinished stash); manual: copy the files from the stash `files\` folder back into the game folder.

## 2026-10-08 21:45 - Play-session perf digest + FnProbe naming (analysis only; game, mod and game folder not touched)
- `notes\session-2026-10-08-perf.md`: the user's 20:59-21:22 PC session (ASI 1.7.39, own settings, FSR FG, VSync ON) per phase: mission 166 presented / 149 displayed fps from ~106 real, boss 130 / 104; GPU-bound (90-92 % busy); only hitches = first-use PSO compiles (boss kill 98-167 ms, worst create 153 ms); 10-20 % of FG frames discarded under composed flip; no FG-off gameplay recorded (setting stayed FSR).
- Symbols: `re\symbols\incoming\fnprobe-session-renames.tsv` (14 rows, 4 renames: ConditionCheckById F68D8, RenderObjPrepareVirt4D0 4F9750, AudioPanAngleToSpeakerGains 7D48D4, EntryGridUnlockStateRefresh 556118) applied with apply_renames.py; `fnprobe-session.tsv` (3 new rows) + the pending `bounds.tsv` merged with merge_incoming.py. perf_coverage --precise 100.00 %; treemaps/badges regenerated, README badge 6913/211206.
- `notes\whats-left.md` (new row 0 = PJ in real play; PC real play is GPU-bound), `notes\user-settings.md` PC rows 11-13 [M], remaining-work 10.1 DONE.
- Undo: `git revert` of this commit (symbols: restore symbols.tsv and move the three files back from incoming\merged).

## 2026-10-08 22:10 - Play-session game data for the boss rematch + branch `rematch2` (R-b DirectStart, InvLog) (nothing in the game folder changed)
- `notes\session-2026-10-08-gamedata.md`: the 20:59-21:22 PC session (mission 52 replay, Jakotsu-baba 76781). Q10 answered: the shrine replay confirm F1FD80 (= MissionReplayMenu_Decide, choice 0) writes the queue request inline; identical to `MissionRequestById(id, [[47514D0]+8]+10h, 0, {101h,1000000h,10100h,0,0,0})`. All 41 reward/state lines accounted for (0 REWARD: the reward hooks are script natives and fight-window only); data-table mismatches (defeat flag 118, not 115/512; web row mission id 3150). BLOCKER: MissionLoadStep only ticks during loads, so the 1.7.40 rematch pump never runs in the field.
- `notes\boss-rematch-design.md` s.8 answers, `notes\boss-rematch-build.md` s.6 open items; `tools\rematch_catalog.py` LOGGED row for mission 52 (placement 76781, region 0x96, new column `seen`) -> `Nioh3EngineFix_bosses.tsv` regenerated (the mod ignores unknown columns); `tools\playsession\session_lib.ps1` capture key `[Debug] InvLog = 1` (older ASIs ignore it).
- Branch `rematch2` (from main 1.7.40, pushed, NOT merged): rematch.cpp R-b `ExecDirect` (DirectStart = 0 default) + harness/dump pins; bosslog.cpp `[Debug] InvLog = 0` hook on InventoryAddItem 54D324 (patches.json BL31) + `tests\run_invlog_harness.py`; RematchLog opens its replay window on F1FD80 choice 0. test_patterns ALL 284 SITES OK, rematch + invlog harnesses ALL PASSED, build.bat OK. Merge notes: ini/menu DirectStart text, MOD_VERSION, the catalogue column (take main's tool + tsv).
- Undo: `git revert` of this commit; the branch is separate (`git push origin --delete rematch2` to drop it).

## 2026-10-08 - ASI 1.7.40 (main): tour.cpp MissionRequestById params[6] now alignas(16) (game reads it with movaps; crash risk). Audited: FillWarpPoint/FtPost req buffers already alignas(16) 0x80; other game calls pass scalars/db ptrs only; autocont/loadonly/etc CONTEXT-like cur[2] already aligned. Nothing in the game folder changed.

## 2026-10-08 - Ally main-thread draw prep, pass 2 (analysis only; game, mod and game folder not touched)
- `notes\ally-drawprep-2.md`: 216 pooled Ally SP windows (USER, GF on). Game code is 18.4 ms/frame, but 10.2 ms of it is below the per-report top-25 cut. `[ntdll] <- 5DA48` (whats-left #9 SR111) is shown to be a stale label of the GF fence wait (r = -0.57 vs the fence row; a live 111064 Sleep would fold to 111064). 418F40 0.8 ms is backpressure wait, so real work is ~17.6 ms (ceiling ~57 fps). D2 (state-commit skip-unchanged), stream memcpy dedupe and parallel segments: no bit-exact saving. Small candidates: W1b 0.15-0.25 ms, RP (RenderParamsAssign range copy) 0.07-0.1, S2F 0.05. Nothing reached 0.5 ms, so nothing was built and no worktree was made. Next step: Ally SP with TopN 200 / StackBytes 16384 in the #3 bundle session.
- New tool `re\scripts_cpu\sp_pool_ms.py` (SP rows -> ms/frame using the OSD fps/draws lines).
- Undo: `git revert` of this commit.

## 2026-10-08 22:40 - Boss rematch for every boss + the separate add-on Nioh3BossRematch (branch `rematch3`; nothing in the game folder changed)
- `notes\rematch-all-bosses.md` / `.csv` (`tools\rematch_catalog.py`, `--index` scans the LFM): 105 names = 96 bosses + 9 non-boss boss-master names. Routes: R 59, D 8 (master duels), H 3 hidden duels as variants, X 29 (19 field-only, 7 without any boss record, 3 without data), plus the 20 Hundred Demon Realms Picture Scroll records (E). Data proof that the design's record-patch E1 cannot host a named boss (0xCC96 spawns only its own 40-row gadget table; its enemy table is empty).
- Branch `rematch3` (from `rematch2` + main, pushed, NOT merged), two separate commits: (1) Nioh3EngineFix 1.7.41: bosslog.cpp / rematch.cpp / BL+RM patches.json groups / menu row / their harnesses removed; old ini keys log 'moved'; test_patterns ALL 217 SITES OK, build OK; `tools\playsession` writes the logger keys into `Nioh3BossRematch.ini`. (2) `mod\Nioh3BossRematch` 1.0.0: BossLog / RematchLog / InvLog + rematch routes R / H / D / E (E1 = Emaki map redirect with CRC restore) on a per-frame pump, 80 sites by pattern or script-native name with independent group self-disable, own overlay menu (F7 / View + Up), own pipe; test_sites ALL 80 SITES OK (+ patch simulation), harness ALL PASSED, build OK. `[Rematch] Enable = 0` default.
- `tools\rematch\verify_all.ps1` (unattended per-boss verifier, guarded; NOT run; `-DryRun` checked). `notes\boss-rematch-addon.md`.
- Undo: remove `Nioh3BossRematch.asi/.ini/_bosses.tsv` from the game folder (they were never installed); `git revert` of these commits; the branch is separate (`git push origin --delete rematch3`).
## 2026-10-09 - ASI 1.7.42 (branch `w1b`, NOT merged): W1B glyph padding clear stores + RPA RenderParamsAssign range copy (default off; game folder not touched)
- drawprep.cpp/.asm: `[Patches] W1B` (61BAC: game loop with `mov [m],0` instead of W1's `rep stosd`; off = W1 or the original) and `[Patches] RPA` (247950: the 21 byte ranges the original copies, 16-byte moves). 0/1/2, live menu rows. patches.json sites W1B (supersedes W1) + RPA, 3 FNV deps; matrix.json items W1B / RPA.
- Offline proof `mod\Nioh3EngineFix\tests\run_w1b_rpa_harness.py` (dump bytes vs real caves) ALL PASSED; test_patterns ALL 285 SITES OK; build OK. Not run in the game. Note `notes\w1b-rpa.md`. Version 1.7.42 because main took 1.7.41 meanwhile.
- Undo: keep `W1B = 0`, `RPA = 0` (default), or drop the branch (`git push origin --delete w1b`).

## 2026-10-09 - Mod vs stock latency + BossLog fight-start analysis (analysis only; game, mod src and game folder not touched)
- `notes\mod-vs-stock-2026-10-09.md`: the modded run's extra delay is the present mode, not rendering. MOD presented SyncInterval 1 -> `Composed: Flip` from the hub load on (21:00:52); STOCK presented SyncInterval 0 + tearing -> independent flip all session. DisplayLatency p50 14.9 vs 7.6 ms, dropped 10 % vs 2 %; GPU +0.24 ms/presented frame (FSR4 + OSD). ASI ruled out (never writes SyncInterval/flags/VSync); OptiScaler not the trigger (SI1 seen without it in round 3); engine SyncInterval writers located (2EDFBBD ctor, 13503A3 VSync watcher, 2EEBB50 setter). Proposals: OSD present-mode logger (A), opt-in `[Latency] VrrPresent` (B), test plan T1-T4.
- BossLog "fight 26.8 s": BossBattleMode fires only after the mid-encounter cutscene + 5 s load that spawns 76781 (Enenra & Jakotsu-baba row); encounter started no later than 21:17:46. Encounter-window fix proposed for `mod\Nioh3BossRematch` (not edited).
- `notes\user-settings.md` PC row 12 + top 5 updated with the measured mod-vs-stock numbers.
- Undo: `git revert` of this commit.

## 2026-10-09 - ASI 1.7.43 (main): fix boot hang with 3-4 drawprep groups on (run8 Block A)
- Root cause: 1.7.42 grew drawprep `Start`'s `Write w[4]` (DQ, P1E7, W1B, RPA) but `FrozenWrite` kept `DWORD oldProt[2]`. With 3-4 groups set (run8 Block A: DQ,P1E7,W1B,RPA = 2) VirtualProtect wrote oldProt[2..3] over the spilled `w` pointer ([rsp+38h]); the clash check then read through it with every game thread suspended -> AV in the frozen section, process frozen: no window, ~3 s CPU, log ends at "AutoContinue: installed" (the next step is drawprep::Start, then the menu). Not menu.cpp; 1-2 groups (and the user's ini, no drawprep keys) were unaffected.
- Fix (drawprep.cpp): `kMaxWrites = 4` sizes both arrays, FrozenWrite rejects n > kMaxWrites. Regression check in tests\test_patterns.py (fails on the 1.7.42 source): every drawprep Write / oldProt array must be [kMaxWrites] and Start's queued JmpBytes writes must fit.
- Proof (guarded boots, Block A drawprep keys + user ini, logs `captures\pc\hang1743\`): 1.7.42 TIMEOUT 91 s, no window, cpu 3.5 s, log stops at AutoContinue (reproduced); 1.7.43 `MENU: ready` after 10 s with DQ/P1E7/W1B/RPA installed. test_patterns ALL 219 SITES OK, menu_map --selftest OK. User ini + ASI (pre1739.bak, sha256 dd8b5739...) restored and verified; game not left running.
- Undo: `git revert` of this commit (reintroduces the hang with 3+ drawprep groups).

## 2026-10-09 - Windows PC GPU plan + unattended PC GPU session runner (repo only; game not run, nothing in the game folder changed)
- `notes\windows-gpu-plan.md`: the PC is GPU-bound at the user's settings, so only GPU work moves real fps. Every staged shader item is a signed, vendor-neutral DXIL loose file and valid on Windows D3D12 (file route proven by GHDM; psoswap harness OK on this PC). Expected PC gains come from r5 at the user's own settings, not from Ally scaling. Remaining same-look items ≈ 0.3-0.7 ms of ~9.4 ms (realistic +2-4 % real fps), led by wdo28 (0.15-0.25 ms E), gi #414 (-0.15..+0.1 E), esm (0-0.13 E) and AS0 (0-0.3 E, high risk). fogpar / wdo14 / mpc / mshlod / GIM are each ≤ 0.08 ms. FSR3 R1 fp16 does nothing here (XeSS -> OptiScaler FSR4: the game's FSR3 never runs). GHDM is already installed (+3 % / ~+11 % FG, M).
- `tools\testmatrix\pc_gpu_session.ps1`: Phase A = GP2 per-PSO budget on the tour route at the session config.xml (FG on) -> `A_budget.txt`. Phase B = launch A/B (FG off, MaxFps 240) of wdo28, esm, gi, AS0, fogpar, wdo14, GIM, mpc, mshlod in the order BASE x y BASE y x BASE. Each launch measures PresentMon GPUBusy, the GP2 target row and one screenshot (shotdiff). Guard + quiet guard + measuring lock + bridge lock; config / ini / ASI / saves / shader files backed up and always restored (SHA-checked). `-WaitRun8` waits for run8. `-DryRun`: **DryRun OK**.
- `tools\testmatrix\pc_gpu_report.py`: verdicts KEEP / DROP / NEUTRAL / INVALID against BASE interpolated for drift; `selftest` ALL PASSED.
- Run (after run8): `powershell -NoProfile -ExecutionPolicy Bypass -File tools\testmatrix\pc_gpu_session.ps1 -WaitRun8` (~3 h: A ~22 min, B 29 launches ~160 min; `-Items wdo28,esm,gi,AS0` ~72 min for B).
- Undo: `git revert` of this commit. A real run restores every file it touched by itself.
- 2026-10-09 notes/game-size-and-memory.md: game disk/memory reduction audit (research only). 135.8 GB install is 99.9 percent irreducible streamed art, no separable language/voice depot, realistic disk saving 0.1-0.2 GB; memory flat after load, only LM sweeper (0.75-1.1 GB transient swap on Ally, estimate, LM=0) is a look-neutral win.

## 2026-10-09 - ASI 1.7.44 (branch fgproton): FSR frame generation under Proton - root cause + "Proton safe FG" (default off; game not run)
- Root cause (`notesg-proton-fix.md`): vkd3d-proton #2845's crash log decoded onto v2.02. The game's FidelityFX KTGL backend copy-job executor 3718DB4 calls KtglTextureNative 4548C4 on a null FG texture (reads [0+90h]); its clear / compute executors null-check, and CreateResource 3715CCC returns FFX_OK for a failed texture. It is game code, not vkd3d. Ally: live OFF -> FSR worked 3/3, FSR -> OFF crashed 1/1 (v9), boot with FG never tried.
- New (all default off): `[Patches] FGC` = copy-job null guard (sites FGC1 3718DD9 / FGC2 3718DF7, bit-identical while textures exist, logs + `stats/s FG:` generated frames/s). `[Menu] FrameGenToggle = 3` (Proton): FGC + FGP (39A865 menu-open getter -> engine FG pause), FG off = pause without a swapchain rebuild + config.xml OFF next launch, FG on = resume or one OFF -> FSR rebuild, FG kept in config.xml behind the crash marker (armed before the game reads config.xml; a crash reverts to OFF at the next start; clean exit writes the user's last choice).
- FG on the Ally (U6 data): 48.5 fps displayed but the FSR upscaler runs every 2nd displayed frame: real fps 34.3 -> ~24.3, ~7 ms GPU per real frame for FG + one held frame. Worth it only at >= ~40 real fps with FG.
- Ally test protocol: notesg-proton-fix.md section 3 (max 3 launches, config.xml backup/restore).
- Undo: `[Menu] FrameGenToggle = 1` and `[Patches] FGC = 0` (the default), or `git revert`. config.xml: the mod keeps `config.xml.Nioh3EngineFix.bak` before each FRAME_GENERATION write.

## 2026-10-09 - ASI 1.7.44 (branch vrrpresent): present mode log + [Latency] VrrPresent (Windows, default off)
- Why: `notes\mod-vs-stock-2026-10-09.md` - on some launches the engine presents SyncInterval 1 -> composed flip (+~7 ms display latency, 8-20 % presented frames never shown); stock runs that did not hit it presented SI 0 + ALLOW_TEARING (independent flip under VRR).
- RE (byte-verified): the only `[reg+424Ch]` stores are ctor 2EDFBBD (=1), DisplaySettingsWatcher 13503A3 (VSync slot; updates its cache BEFORE a guard that can skip the store, never retries) and setter 2EEBB68 (no static reference); GfxDevicePresent params bit 0 forces SI 0 per call.
- `[Debug] PresentModeLog` (default 1, measurement only): src\pmode.cpp from osd.cpp's bottom-most Present/Present1 hook logs each change of the per-caller present arguments and of the engine's SyncInterval with its writer; PM group (PMw/PMs call sites + 6 deps, src\pmode.asm caves repeat the original store and record value/device/caller). 2 = 30 s summary.
- `[Latency] VrrPresent` (-1 auto = only while game VSync is OFF / 0 default / 1): sends `(0, flags | DXGI_PRESENT_ALLOW_TEARING)` on swapchains created with ALLOW_TEARING that are not exclusive fullscreen; foreign flags untouched; DXGI_ERROR_INVALID_CALL -> re-sent with the game's arguments, chain excluded. FG pacing, engine state and VSync slot untouched; refused under Proton/Wine.
- Offline: test_patterns ALL 221 SITES OK incl. the PM block (stores repeated, no branch into sites, flags dead, exactly 3 writers, hand-off bytes); build OK; `tools\testmatrix\vrrp_report.py --selftest` OK; matrix item `VrrPresent` (pc, launch) + alloff `VrrPresent = 0`. Not run in the game. PC test (ABBA with PresentMon, KEEP if DisplayLatency p50 -3 ms and displayed fps up, no tearing in VRR range): `notes\vrr-present.md` section 3.
- Undo: `VrrPresent = 0` (default) and `PresentModeLog = 0` (no PM sites), or drop the branch.
## 2026-10-09 - ASI 1.7.44 (branch async, not merged): AS - engine async-compute slots with a live switch (default off)
- Study + build: notes\gpu-async-wins.md. KTGL's async slots (table 44C04D0) have a graphics path and an async path per user, with the engine's fences. AS (`[Patches] AS` 0/1/2, `[Tuning] AS_Slots`, default `0`; Ally `0 11 12`; `-3 -4 -6` keeps the shipped fog/clouds/ocean synchronous while ON) creates the requested slots before the renderer init (slot 0 = the AS0 byte 226A497, 11/12 = table enable bytes; refused once the live table is filled; Intel: slot 0 refused) and re-points the one producer call of each switchable slot (ASp0/3/4/6/11/12) to a cave that ANDs the slot mask: menu row "Async compute slots (AS)" OFF = stock, ON = AS. Slot-0 joins (RS+13ACh) untouched, all 32 callers of AsyncSlotGetCtx classified.
- Why AS0 read "neutral": PC 1.7.16 was CPU-bound (CPU busy 6.4 ms = frame, GPU busy 5.38 ms both arms) with 5 % launch noise; Ally 1.7.5 ran with GPUProf = 2 stamps.
- Offline: test_patterns ALL 225 SITES OK; run_asyncslots_harness ALL PASSED (1.2M cases, mutation caught); run_async_overlap_harness (RX 9070, real device): 0.30 ms chain -> ASYNC saves 0.133 ms (splits +0.063, cross-queue sync +0.13), CPU-bound case +0.19 ms, outputs bit-identical; build OK. Expected PC +1-1.5 % GPU-bound, Ally est. 1-1.5 %. Not run in game.
- Also: matrix.json item AS (grp 11, GPU-bound abba2 only), abba2_report.py reads `stats/s AS: switch` and skips the GPU-move guard for AS.
- Undo: `[Patches] AS = 0` (nothing written); or the 1.7.43 ASI.
## 2026-10-09 02:05 - harness only (no game change): run8b diagnosis, display-refresh guard, DryRun no longer closes the game
- notes\run8b-analysis.md. Block C 60.00 fps = desktop at 60 Hz (XV272U, 240 Hz capable; mode reset when the second monitor was unplugged) -> game 'vrr tier 60 cap 60'. Block A 'dead/hung' = a pc_gpu_session -DryRun's Restore-All closed every Nioh3 by name at 00:55:39 (no crash, no dump, no mod bug).
- Harness: Ensure-/Restore-DisplayRefresh (harness_guard.ps1; dynamic mode change for the run, not saved, old mode put back), abba2 Assert-NotCapped before every window + config.xml REN_FPS check + Dead-Report, abba2_report rejects capped windows, pc_gpu_session Close-Game = own launched games only, never in -DryRun.
- Undo: git revert of this commit. No game file touched; the desktop refresh is unchanged (still 60 Hz - set 240 Hz in Windows display settings, or the next run8 raises it for the run only).
- 2026-10-08 pc2: new tools\pc2\ (make_pc2.ps1 + README) and generated pc2_*.ps1 copies of the PC test tools (testmatrix pc_gpu_session/abba2_pc/pc_weakcpu, playsession kit, tour\run_tour, patcher\common, enter_game, notify) remapped to pc2 paths (C:\Users\User\projects, C:\nioh3-re, agent pc2). Originals untouched; no game changes. Undo: git revert of this commit (or delete the pc2_*.ps1 files and tools\pc2).
| 2026-10-09 04:40 | ASI source: `[Patches] ESM1` (default 0, next launch): 3 code bytes at RVA 14733E (EsmLevelFromRes) -> directional ESM RT 1024^2 instead of 2048^2. Not run in game. notes\gpu-resolution-audit.md | `ESM1 = 0` |

## 2026-10-09 - ASI 1.7.45 (branch gamelogic): game-logic audit, TR timer-resolution hold, SkipDebugString (both default off; game not run)
- notes/game-logic-audit.md: the logic frame waits 0.30-0.33 ms/tick for the previous tick's MissionThread (MissionThreadJoin 5AAED4 in FrameEarlyUpdate; kick 233BB4 at the end of GameUpdateTick; gamelogic.md had the MissionThread / GameModeDrawThread objects swapped). Ally GQ gap GFM_TextureClear -> App_CopyMRLROcnScene (0.5-0.8 ms/frame) = likely the async-ocean (slot 6) queue Wait: test AS = 2, AS_Slots = -6. HID reader threads flap timeBeginPeriod(1)/timeEndPeriod(1) every loop; a process with no period sleeps 15.6 ms in Sleep(1) (measured).
- ADD [Patches] TR = 0 (src/timerres.cpp, live menu row 'Steady timer resolution'; 1 = hold 1 ms once + skip the game's own pairs, 2 = hooks + counters only; stats/s TR line incl. the per-process Sleep(1) median). Hooks the two WINMM import slots of Nioh3.exe, no code bytes.
- ADD [Debug] SkipDebugString = 0 (1 = Log() writes the file only; OutputDebugStringA only with a debugger attached).
- Symbols: MissionThreadJoin/Kick/Arm, g_MissionThreadObj 47098F0, GameModeDrawThreadKick/Join (was MissionThreadKick/Join), g_GameModeDrawThreadObj 4709B60, CharacterUpdateEventJoin 90CA90, JobFnEventSet FE79C0, PCTimerTick 437F7C, g_HidTimerPeriod 44B68F0. matrix.json: TR item (live), GL1 note (A/B with JT in every arm).
- Tests: tests/test_patterns.py ALL 231 SITES OK; tests/run_timerres_harness.py ALL PASSED; check_release PASS.
- Undo: TR = 0 / SkipDebugString = 0 (defaults), or the 1.7.44 .asi.

## 2026-10-09 - ASI 1.7.46: PL boot pipeline library (default off; game not run)
- notes/boot-shader-compile.md: the boot PSO list (PreloadPsoBootLoad 1101B90 -> PsoListLoad 3483E04) is already split over the pool's workers (PsoListPreloadJob 34835D4 x min(N, pool workers), JobGroupWait join), creates outside the PSO-cache lock; pc2 boot 5-8 cores busy, ~25 s first launch at a config vs ~10 s next launch (driver cache works). Skipping list PSOs rejected (gameplay hitches).
- ADD [Patches] PL = 0, PL_MaxMB = 1024 (src/psojournal.cpp; 1 = ID3D12PipelineLibrary for the engine's creates before the boot gate, file Nioh3EngineFix_pj\pl_*.bin checked against exe/GPU/driver/size/Hash128 + D3D12's own check, written at the gate when it grew; 2 = boot timing line only). Off on Proton and with ShaderSwap/GPUProf2/GIProbe/GIMerge. Boot timing line also with PJ on.
- Tests: tests/run_pj_harness.py ALL PJ CHECKS OK (+8 PL launches on WARP: rec/warm/.new promote/corrupt/stale/garbage blob/PJ+PL/PL=2); tests/test_patterns.py ALL 231 SITES OK.
- Undo: PL = 0 (default); delete <game>\Nioh3EngineFix_pj\pl_* to drop the library file.

## 2026-10-09 - ASI 1.7.47: MLP meshlet front-end probe, MPCS MPC tail skip + MpcCheck self-check (all default off; game not run)
- notes/meshlet-frontend.md. pc2 GP2 MinRun 1 (#140): 5.9 ms meshlet culling family in dense windows, 4.5 ms of it in five kernels (MshLod#1 1.5 ms = anomaly for an 8-byte copy kernel, MIC#0, MPC#27, MshLod#2 L0-L2, MPC#20); MPC #28-#31/#26 and LOD levels >= 4 are small (~0.1 ms each family) -> MPCS ~0.05-0.1 ms, LOD tail collapse not built (bindings + <= 0.1 ms).
- ADD [Profiler] MeshletProbe = 0 (+ _Stats, _Top, _Dump, _Barriers): GPU half in gpuprof.cpp (per-dispatch / per-barrier stamps in the meshlet markers, CS invocations via pipeline statistics in alternate sampled frames, "GP: MLP:" lines, raw CSV), CPU half mlprobe.cpp (entry thunks 531948 / 5301FC / 53FD3C, "MLPC:" lines: callers, duplicate-resolve test, MPC cnt histogram). Needs GPUProf = 2, Barriers = 0.
- ADD [Patches] MPCS = 0 (src/mpcskip.asm, patches.json group MPCS): drops MPC #28-#31 (1 <= cnt <= 1024) and #26 (#23 recorded, cnt <= 32768) in MeshletPreculling via the engine's bind-failed paths; refused unless shader_dx12\0x29ee0902.file is the proven mpc build (sha256 3947f3d7).
- ADD [Profiler] MpcCheck = 0, MpcCheckMaxMB = 64 (src/mpccheck.cpp): runtime self-check, buffer snapshots before / after the skippable stages compared on the CPU ("MPCK:" lines).
- Tests: tests/test_patterns.py ALL 237 SITES OK; tests/run_mpcs_harness.py ALL OK (+ mutation checks); tests/run_gp_harness.py modes 1-5 ALL PASSED (mode 5 = MLP + MpcCheck on the RX 9070; also fixes the harness link for skipfx); verify_culling.py quick 94 scenes PASS.
- Harness: pc_gpu_session.ps1 Phase M (+ items mpcs / mpc), pc2 copy regenerated; re/scripts_gpu/mlp_report.py.
- Undo: all keys 0 (default).

## 2026-10-09 14:40 - ASI 1.7.48-1.7.52: VS probe + CM CbvMirror (all default off; built, CM not run in game yet)
- 1.7.48-1.7.51 VS probe ([Profiler] MemStats = 2): 1 s VRAM / texture page pool / D3D12 residency line, system-memory allocation census by caller, bind census (root CBV/SRV/UAV, VB/IB per marker and per PSO), CBV/SRV/UAV descriptor census; D3D12CreateDevice IAT hook from the decrypt wait. [Profiler] VSForceSysUpload (PC A/B). notes\root-cause-vram-streaming.md.
- 1.7.52 CM ([GpuOpt] CbvMirror = 0/1 auto/2 force, CbvMirrorMB = 128, CbvMirrorAllSites = 0): per-draw CBVs (recorder 118F89) redirected to a VRAM mirror ring, one UPLOAD -> mirror copy list before each game ExecuteCommandLists (osd hook). Auto only when the UPLOAD heaps are non-local (no ReBAR), never on UMA / Wine.
- Undo: CbvMirror = 0 / MemStats = 0 (next launch), or the previous .asi.

## 2026-10-09 16:xx - Ally RippleDecal lead (#969) closed: measurement artifact, no fix (nothing applied, no build)
- `notes/ally-ripple-decal.md`: the skipped PS c013d811 is the clustered-decal G-buffer pass (4 fullscreen stencil-EQUAL draws, 4 MRTs, blending off); skipping it corrupts the G-buffer, so the 1.92 ms includes cheaper downstream lighting. Own cost 0.55 ms on the Ally (S1 GP2) vs 0.216 ms on pc2 = ~4x per pixel = bandwidth (no Infinity Cache), not a RADV/vkd3d slow path. SKY_all and PB2_be0f are visible (sky washes out; the player model vanishes). New tool `re/scripts_gpu/pj_psodesc.py` (PSO state from the PJ journal).
- Undo: none (analysis only).

## 2026-10-09 16:30 - ASI 1.7.53 fast testing: LiveCtl + ShaderHot (all default off; notes/fast-testing.md)
- `[Debug] LiveCtl = 0`: control file `Nioh3EngineFix_live.ini` polled every 100 ms (`[Live]` menu rows by key, `Hot.<set>`, `Tuning.<key>`), `.ack` with Gen/Tag/read-backs + the mod frame clock (frames / qpc_us / hitch counters from the Present hook).
- Live [Tuning] values (were boot-only): PP_Period, VSMA_K, VSMA_Front, LPVR_N, SKYR_V, FOGT_V, CLDR_V.
- `[ShaderHot] Enabled = 0` (DIAGNOSTIC, never shipped on): twin PSOs for shaders listed in `Nioh3EngineFix_hot\<set>\`, rebound at SetPipelineState / Reset / ClearState through gpuopt's thunks (GpuOptSlow gets the saved argument registers). The game only sees its own PSOs.
- Tools: `tools/testmatrix/fastab.ps1` (one-launch ABBA paired windows), `fastab_report.py`, `hotset.py`, `live_keys.py/json`; proof `tests/run_hot_harness.py`.
- Game folder: fastab.ps1 writes `Nioh3EngineFix_hot\` and the control file for the run and deletes them after; ini/ASI/config.xml/saves are restored by run_tour / fastab.
- Undo: keys 0 (the defaults); delete `Nioh3EngineFix_hot\` / `Nioh3EngineFix_live.ini*` if a run was killed.

## 2026-10-09 21:00 - ASI 1.7.56: RB Resizable BAR check + one-time notice ([General] ReBarNotice = 1; notes/rebar-fix-v3.md)
- `[General] ReBarNotice = 1` (default): ~40 s after the game first renders, one check: the game's adapter (engine device LUID), D3D12 UMA + OPTIONS16 GPUUploadHeapSupported, DXGI non-local usage (median of 10 samples, 2 s apart). Discrete GPU + UPLOAD heaps in system memory (non-local >= 600 MB) = no ReBAR -> log line `RB: Resizable BAR / Smart Access Memory is off: ...` and a 25 s overlay toast, ONCE: the file `Nioh3EngineFix.rebarnotice` next to the ASI is written after it was on screen (or dismissed by opening the mod menu). Skipped on UMA / APUs, under Wine / Proton, with `[Profiler] VSForceSysUpload` A/B runs, and (toast only) in Tour / VirtualPad sessions.
- Measured reason (bridge #1091): pc2 RX 6650 XT ReBAR OFF -> ON = GP2 span 32.5 -> 24.0 ms at the same spot.
- Writes nothing else; the BIOS setting is the player's choice.
- Undo: `ReBarNotice = 0`; delete `Nioh3EngineFix.rebarnotice` to see the notice again.

## 2026-10-09 22:30 - ASI 1.7.57: SBM meshlet scene-buffer mirror + census (all default off; notes/rebar-fix-v3.md)
- `[GpuOpt] SceneBufMirror = 0` (EXPERIMENT, for PCs without Resizable BAR): hook on the call 5D417 (MeshletFrameBegin -> MeshletUpdateSceneBuffers; patches.json SBM1). After the CPU wrote the current scene-buffer set, its UPLOAD buffers' written bytes go into VRAM twins with ONE copy list per frame on an own COPY queue (`SceneBufMirrorQueue = 1`; 0 = the game's DIRECT queue), one fence; each game DIRECT / COMPUTE queue waits once; the engine's SRVs of those buffers are re-created on the twins. 1 = auto (only when the RB check found the UPLOAD heaps in system memory), 2 = force (test).
- `[Profiler] SceneBufProbe = 0`: `SBM set` census lines (per slot usage / heap / bytes / views) + a late-CPU-write check while the mirror runs (mismatch -> SBM off, views restored).
- Harness `tests/run_sbm_harness.py` (RX 9070, no game): all OK; system-memory copy 25.7 GB/s (COPY queue) / 24.0 GB/s (DIRECT) for 4 MB per frame.
- Not run in the game yet; pc2 A/B queued. Game folder untouched.
- Undo: keys 0 (the defaults).

## 2026-10-10 - RE only: notes/renderer/01b-replay-handlers.md (replay-side handlers, no game or ASI change)
- All 185 KTGL token handlers + the 23-entry async sub-stream table 3C02800: role, payload layout, D3D12 calls, SynCtx state, per-call waste. Section 0 = the 6A / 69 per-draw path (pc2 cause: full-size constant-buffer copies into the BAR-mapped UPLOAD ring, Memcpy 118F4D) with fixes F1-F7.
- symbols.tsv `# renderer/backend`: +72 names, 1 heuristic rename, 7 role notes; package 01 11.76 -> 11.96 %; README renderer table regenerated, regenerate-command typo fixed.
- Undo: n/a (documentation and symbols only).

## 2026-10-10 17:15 - RE + prototype: main-thread render submission (notes/renderer/13-main-submit.md), WIP stopped for the weekly limit
- Decompiled the main-thread submit path: SceneRenderView -> DrawListWalk -> DrawEmit -> DrawSubmit -> tokens 69/6A/6B/6C -> KtglCmdStreamFlush kick ring. The list sort runs on the KIDS worker (3B3B4 / 54A94C radix), not on main. Also the meshlet scene-buffer sync wait (sets swapped in 5D214, wait in 4BBEC, record in 5E370 = F waits for F-2; 3-set and reorder options).
- symbols.tsv: +100 functions, +37 fields, 2 renames (blocks renderer/drawlists, renderer/meshlet). Renderer core named 39.0 -> 40.2 %, bytes 20.5 -> 20.7 %. Charts and the README table regenerated.
- Prototype UQS (UI quad build in a cached scratch, branch rdraw-main 4884e33 + tour fix 2482b2a, ASI 1.7.58-rdm1, default off, live Tuning.UQS): harness ALL PASSED, ALL 249 SITES OK. PC A/B uqs4 ran, not yet analysed. Test build branch rdraw-main-fo9 15fab3c (not queued).
- Undo: n/a on main (documentation / symbols). The ASI change is only on branch rdraw-main, default off. The main tree's bin ASI was restored to what it was before the runs.
## 2026-10-10 07:40 - ASI 1.7.59-fo7 (branch frame-overlap; [GpuOpt] FrameOverlap = 0 default, nothing installed)
- FO fixes for the fo6pc hang: a frame recorded for a non-current back buffer REMOVES THE D3D12 DEVICE on the PC (the engine's fatal handler then sits in a modal box = the 'hang'); the next frame's back-buffer index is now read/verified by the TAIL before that frame replays (wrong prediction -> extra present of the current buffer). Stock Present after an FO frame waits for the pending present. Watchdog: thread dump + removed reason; restore fixed (stale index cache, FoPresent site written last / restored last). Live Tuning.FO_Test (tests, incl. 100 = self-test).
- PC: self-test ALL 17 steps PASS; 10.8 min FO ON + ABBA x4 with 0 OFF events; FO +0.5 % +- 0.17 SE (GPU-bound). Game folder: only the tour harness's temporary ASI/ini, restored and verified after every run.
- Undo: [GpuOpt] FrameOverlap = 0 (default) or live Tuning.FO = 0.

## 2026-10-10 09:45 - ASI 1.7.59-fo8 (branch frame-overlap; [GpuOpt] FrameOverlap = 0 default)
- pc2 fo2 'hang' = the replay thread crashed when it lags (CPU-bound): B6 PagePool page ops (7D9C24) of frame N+1 ran while frame N still replayed. New default [Tuning] FO_PPWait = 2 (B6 waits for TAIL N). Sub-stream crossing guard, replay-fault report, full thread dump, FO_ReplayDelayUs lag emulator (test only).
- PC with an 8 ms emulated replay lag: 35 k FO frames, 0 faults (fo7: fault after 71 / 160 frames). Game folder: only the tour harness's temporary ASI/ini, restored + verified after every run.
- Undo: [GpuOpt] FrameOverlap = 0 (default) or live Tuning.FO = 0.

## 2026-10-10 14:20 - ASI 1.7.60-cbu1 (branch cbv-upload; [Patches] CBS / CBN / CBT = 0 default)
- CBU per-draw constant uploads (notes/renderer/cbv-upload.md): CBS streaming copy into the UPLOAD ring (118F4D + 495AF2), CBN skip same-list no-op CB uploads (20 % on the PC), CBT reuse the stage CBV table (16 %); CbvStats / CbvRecord census. Harness byte-exact vs relocated v2.02 code incl. a recorded game trace; PC fps unchanged (GPU-bound), pc2 spec pending.
- Game folder: only the tour harness temporary ASI/ini during cbu1pc, restored + verified (user ASI DD8B5739, Tour = 0, Nioh3BossRematch.* identical); the trace file moved to captures.
- Undo: keys 0 (default) or live Tuning.CBMODE = 0.

## 2026-10-10 16:45 - ASI 1.7.60-cbu2 (branch cbv-upload; CBN + CBT default ON, CBF test-only)
- Default change: `[Patches] CBN = 1, CBT = 1` (skip byte-identical per-draw constant uploads of the same command list + reuse the stage's CBV table), CBS = 0. pc2 +15.9 % (38.3 -> 44.3 fps, #1609), Ally Proton/RADV no-harm (#1668), PC RX 9070 no-harm (cbu2pc: +0.49 % +- 1.78, GPU busy -0.02 ms, #1707). profiles.json CBN / CBT kept.
- CBN: after a ring-full list cut inside SynUploadCbvTable the skipped slots are uploaded again in the new list (insurance; 0 cuts seen in the game).
- CBF (`[Patches] CBF = 0`, test only): CB ring written in system memory, one copy per game submission into a VRAM ring (src/cbvstage.cpp). PC: the first run (cbu2pc) rendered black in the CBF windows (RPF's ECL thunk hid the submissions) - fixed (osd hook passes non-game callers through cbvstage) + a no-flush watchdog; the rerun (cbu2pcf) renders correctly, +0.17 ms GPU, +0.7 ms/f CreateCBV cost. pc2 CBU2 queued.
- Game folder: only the tour harness temporary ASI/ini during cbu2pc / cbu2pcf; saves unchanged + verified, user ini / ASI restored by run_tour (Tour = 0). Note: run_tour warned that the base ini already carried a `TourList =` line before cbu2pcf (left by an earlier run, Tour = 0, harmless) - not removed.
- Undo: `[Patches] CBN = 0, CBT = 0` (or live Tuning.CBMODE = 0); CBF is 0 by default.

## 2026-10-10 - ASI 1.7.59 [ExtraPerf] extra performance settings (branch extra-perf, not merged)
- New opt-in settings that trade look for fps: XCLD XFOG XGI XSHD XSHF XLOD XTER XPRT XAO XPOST + Preset (Performance / Potato); all default off ([ExtraPerf] Enabled = 1 installs the hooks). Menu section "Extra performance (changes the look)". Table + measurements: notes\extra-performance-settings.md.
- Presets re-tuned after screenshot pairs: fog cuts changed 15-23 % of pixels (Potato replaced the volumetric glow with a brown haze) -> Performance leaves fog alone, Potato uses the 48x16 grid. Log lines per override ("EXTRA: S+410 ... engine X -> written Y"), GI / particle / LOD read-backs in the 30 s stats line. fastab.ps1 -Shots / -Asi, shotpairs.py, extraperf_table.py, extraperf_ab.py (pc2 / Ally scheduler); fastab writes only XPRE while a preset is active.
- Undo: [ExtraPerf] Enabled = 0 (installs nothing) or Preset = 0 / all levels 0 (hooks pass the game's values through).

## 2026-10-10 - ASI 1.7.61 integration (Codex lead takeover)
- Merged cbv-upload c5183c2 and extra-perf cba1fbc into main; both build paths include CBU/CBF and ExtraPerf sources. CBN=1, CBT=1, CBS=0, CBF=0; ExtraPerf preset and every level remain 0 (opt-in). FrameOverlap remains off.
- Retained live-ack retries and preset apply ordering; regenerated the live-key catalog. Tour entry validation supports both E9 and FF25 external entry hooks.
- Prior pc2 +15.9% applies to its slow chipset PCIe link; no new performance claim until the merged build is measured. Validation and smoke outcomes: notes/handoff-codex-step1.md.
- Undo: CBN=0, CBT=0; ExtraPerf.Enabled=0 or preset/levels 0.


## 2026-10-10 - FnProbe decoder safety (1.7.61 integration regression)
- Full release check exposed a 66-prefixed C7 word store at newly named handler 9AFDF0: the decoder counted a 32-bit immediate and swallowed the following return/padding. Respect 16-bit immediates for C7/81/B8-BF unless REX.W overrides 66. FnProbe remains diagnostic and default off.
- Added the real handler and width/override regression cases; run_fnprobe_harness.py ALL PASSED, 12,497 accepted of 13,826 named entries, zero disagreements with Capstone.
- Undo: revert this decoder commit; do not use FnProbe on 66-prefixed immediate prologues with the old decoder.

## 2026-10-10 - Restore GPU/R2 release harness links (1.7.61 integration)
- GPU/R2 harnesses supply the disabled memory-bind census stubs already used by the shader-hot harness; R2 also links SkipFx/MLP/MpcCheck required by the real GPU hooks. No production profiler change.
- run_gp_harness.py all five modes ALL PASSED; run_r2_harness.py ALL PASSED, including device safety, barrier rewrites and trace output.
- Undo: revert harness-only maintenance.

## 2026-10-10 - 1.7.61 merged-build PC validation (Codex)
- test_patterns ALL 255 SITES OK; full release regression suite all 36 harnesses PASS (0 errors, 3 warnings); CBU recorded-trace and ExtraPerf harnesses PASS.
- PC fastab rc1761, stage 7356, ABBA x3: CBN/CBT no measurable FPS change (+0.55% +-0.46 SE, 101.9->102.5); opt-in Performance preset +3.14% +-0.08 SE (102.4->105.6), GPU -0.298 ms. Screenshot changes within the A/A animation floor; combined CBN/CBT + preset 30s / 3179 frames PASS, no combined performance claim.
- One locked tour launch; saves unchanged and user INI/ASI/config restored. pc2/Ally equivalent smoke queued (#1767/#1768, kit #223), results pending; RC held until both pass. Details: notes/handoff-codex-step1.md.
- Undo: CBN/CBT=0, ExtraPerf preset/all levels=0 (or Enabled=0); no system settings changed.

## 2026-10-11 - Codex step 2 fo10 PC NO-GO (experimental branch only)
- Offline 255 sites / 36 harnesses PASS, but PC fixed arm hangs after 19 combined transitions on the 20th stock apply. Unsafe control confirms off-main cache frame-end writer and faults at FO frame68. No measured FPS A/B; no FO5 queue or main merge.
- Main stays 1.7.61. Saves unchanged / ASI / INI / config restored and verified; game closed. codex-fo10 cb2b970 pushed, evidence bridge224, Discord result sent. See notes/handoff-codex-step2.md.

## 2026-10-11 - Experimental fo11 PC follow-up (main unchanged)
- codex-fo10 ddabf46 / ASI1.7.63-fo11 fixes reproduced async release starvation and passes 20 combined transitions + three test14 cycles; full release checks and final flush regression PASS. 8ms emulator ABBA3 +11.74% +/-1.54% SE (60.9->68.1), not native performance. Saves/user files restored; shutdown-only watchdog retained. Conditional pc2 FO5 queue1781 / kit225 follows main smoke1767. No FO merge/release; main remains1.7.61, FrameOverlapoff. See notes/handoff-codex-step2b.md.

## 2026-10-11 - Tour report diagnostic handling (tools only)
- Completed tours previously logged summary failure because PowerShell Stop treated Python's normal stderr clock-offset diagnostic as an exception. New tools/tour/report_tour.ps1 captures both streams and checks the actual reporter exit code; run_tour calls it after restoration. Missing logs still fail explicitly, and caller error preferences are restored.
- Saved fo11 capture: old path reproduces failure on +4h diagnostic; new path writes tour_report.md and tour_points.csv. Missing-log negative control, PowerShell parsing and tour harness (44 checks) PASS. No game launch, ASI/default/performance change. Undo: revert this tooling commit.

## 2026-10-11 02:56 - Codex experimental FO deferred-command audit (nothing applied)
- Isolated branch negative controls against actual FO code reproduce overflow, stream bounds and late-producer queue hazards: 11 checks / 0 failures, zero compiler warnings. This is failure evidence, not release validation. Static startup availability / synchronization / drain constraints documented in notes/handoff-codex-fo98-audit.md.
- No game, production source, ASI, user settings or saves changed. Main1.7.61 / FrameOverlap0 remains. Undo: none required. Experimental audit tests can be removed after conversion to repaired safety regressions.
