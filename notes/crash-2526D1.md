# Crash Nioh3+2526D1 (remaining-work 6.1) - offline verdict (2026-10-08)

**Verdict: our mod does not write any code or data on the faulting path. No ASI guard was needed or added.** The only
link to the mod is timing. On 10/06 the legacy panel froze the whole process (NtSuspendProcess, retried several times
per script) about 10-110 s before the crash, while the game was loading in. A freeze cannot corrupt a pointer in
correct code, so at most it widened a race that already exists in the game. The ASI does all its frozen code writes
about 1 s after process start, before the renderer runs frames. Its later hooks are atomic (vtable xchg or 2-byte spin
then body), and its menu switches only flip data flags. A "defer toggles to load end" guard would therefore change
nothing for the ASI. Open follow-up: one hardening item, see section 5.

## 1. Occurrences (all evidence on this PC)
| when | where | exception | source | cause |
|---|---|---|---|---|
| 2026-10-06 ~21:36 | Nioh3+**2526D1** | 0xC0000005 (game "Fatal error" dialog) | MODLOG row; no dump (the sweep did not pass `--enable_output_dump`) | this note |
| 2026-10-06 15:54 | Nioh3+**61BBD** | 0xC0000005, **write to 0x1** | `%LOCALAPPDATA%\KoeiTecmo\NIOH3\dump\core_dump_1006155402.dmp` (game 2.0.2.0, no ASI loaded; OptiScaler 0.9.4 dxgi, sl.interposer 2.9, FSR 4.1.1) | **torn live patch, already fixed** (below) |

- WER / Application log, last 30 days: no APPCRASH 1000/1001 for Nioh3.exe. The game's own handler catches the
  exception and shows the dialog, so WER never sees it. The only WER entry is RADAR_PRE_LEAK_64 (10/06 13:37), a leak
  heuristic and not a crash. `%LOCALAPPDATA%\CrashDumps` has no Nioh3 dumps. The dotnet.exe dumps from 10/08 09:10 are
  CLR 0x80131506 crashes in the test harness, not the game.
- 61BBD dump decoded (`md.py`-style minidump parse): the code bytes at 61BAC are the **new** W1 bytes
  (`... F3 AB 0F 1F 40 00`), and RIP is 61BBD, the second byte of `rep stosd`. The main thread was suspended at the old
  hot `cmp ecx,r15d` (61BBD) when the panel's first cpu_patches wrote W1. It resumed mid-instruction and ran `stosd`
  with the stale rdi=1. This is the crash `Mem.fsx safeWrite` describes ("W1+0x11"), and safeWrite has fixed it since.
  It is a **different crash from 2526D1**. A torn patch faults at the patched bytes, which matters for section 3.
- 2526D1 context (from `captures\sweep\sss.png`, the round-8 sweep `patcher\round8_settings.ps1`, launch `sss`, config
  SSS ON -> OFF): game pid 17844. The panel log shows "Applying your fixes" at 21:35:28, then cpu_patches W1/W2 at
  21:35:30, ft_vrr at :32, dynfps=0 at :34, cpu_caves W3/R1/L1 at :36 and fg_cave F1 at :38. The screenshot at 21:37:22
  shows the game already gone, and there is no `bench-r8-sss.csv`. MSI Afterburner/RTSS had been installed at 18:00
  (MODLOG). An overlay is a known confounder for fatal errors (community-2026-10-06.md section 4).

## 2. The faulting function
```
RenderFrameBegin 4A529C              ([App]Game main thread; rbp = GfxDevice = [renderer+70h])
  4A5346..4A5357 frame-latency wait   (R1 site)
  4A54D8 call WaitSubmitThread 7D9B6C(dev)
    if [dev+3F80]: SpinWaitFlagSleep0 7D9E90 (while [dev+7728] Sleep(0))   (W6 site 7D9EA2)
    7D9BA3 call PagePoolFrameUpdate 252510(dev+61A0, dev)
      252677 call PagePoolScanAll 2522C4                                     (PP site, ASI-only, default 0)
      25269C call RenderCmd_WriteToken95 2526C8(dev)
        2526C8 mov rax,[rcx+3C40h]     ; KTGL command-stream write pointer
        2526D1 mov dword [rax],95h     ; <-- AV: the write pointer is bad, the device itself is readable
        2526D7 add qword [rcx+3C40h],4 ; then jmp KtglCmdStreamFlush 254110(dev,0)
    CmdStreamBeginFrame 7D9C78 (more tokens into the same stream)
```
- **Bad pointer** = `GfxDevice+3C40` (stream write ptr). The device read at +3C40 succeeded, so the object is mapped.
  The stream is single-producer (main thread) and lives inside `[+3C30 base, +3C38 end)`. Three places write the pointer
  wholesale: CmdStreamFinalKick 418C70 resets it to the base at every frame end (via GfxDevicePresent ->
  CmdStreamEndFrame), the wrap path 1910CAF resets it the same way, and all token writers add to it.
- Token 0x95 is the **first** stream write of a frame. The previous frame end reset the pointer to the base, so a fault
  here means the **base itself** is invalid: the stream is not allocated yet or was freed. That points to the device
  lifecycle (first frames after the GfxDevice / stream is (re)created, or a teardown). It does not point to an overrun,
  which would fault in DrawEmit and the other high-volume writers.
- Other entries into the same stream outside the normal frame: WaitSubmitThread is also called by 32FADF4 ("present N
  frames" loop: WaitSubmitThread + RenderFrameDebugOverlay + GfxDevicePresent, N = [dev+4240]), by
  PostEffectLibraryInit 32FB94B (init path, from 1351709 / 32FE658), and by 3485114 / 35FED85 / 35FF379.
  PagePoolFrameUpdate is also called directly by 2EE0FC4. If one of these runs on another thread, or during device
  (re)creation while a normal frame is in flight, that is a game-side race. A 1/60 launch rate, during the first load
  after a graphics setting changed in config.xml, fits that pattern. The SSS link is circumstantial: one occurrence, and
  SSS was the setting that changed.

## 3. Does any mod component touch this state or its timing?
| component | relation to the path | verdict |
|---|---|---|
| Panel 10/06 (W1/W2, VRR P1-P4, dynfps, W3/R1/L1, F1) | No site in 2526C8 / 252510 / 7D9B6C / 254110 / 418C70 / 7D9C78. R1 (4A5346) is earlier in RenderFrameBegin: the poll loop exits on the same condition (`WaitForSingleObject(h,..) == 0`), only the timeout differs. dynfps writes DYNAMIC_FPS 45BAF58, which the game's own governor 5AF4E4 reads (the governor changes tiers itself at runtime). | no data or code coupling. Timing only: whole-process freezes during load (safeWrite: freeze, check RIPs, retry up to 200) |
| Torn-patch class | A torn write faults at the patched bytes (61BBD proved it). 2526D1 is 0x55 bytes past PP (ASI-only, not loaded on 10/06) and not inside any site. Panel safeWrite was active by then, and so is the ASI ApplyBatch RIP check. | excluded |
| ASI code writes | ApplyBatch (suspend all, RIP not strictly inside a site, retry) runs at about 0.5-1.0 s after start (mod logs: "code is decrypted (200 ms)", W1..W6W applied by +0.6-1.0 s), before any frame. Later hooks (loadstats/readprobe/autocont/bosslog/loadonly) use 16-byte CAS / 2-byte spin, vtable hooks (osd/gpuopt/psoswap/tilestats/menu) use an 8-byte xchg. Neither writes in the stream path. | no mid-load freezes, no coupling |
| ASI on the path: W6 (7D9EA2), PP (252677), R1 | W6 replaces only the `Sleep(0)` inside `while([dev+7728]) Sleep(0)`. The game re-reads the flag after W6Wait returns, so a timeout can never let the frame start early. PP skips PagePoolScanAll on 3 of 4 frames and does not touch +3C40. R1: see above. hook-register-audit.md: PP, W6/W6W and R1 are all safe (nothing live+preserved). | semantics preserved |
| ASI runtime switches | menu `sw[]` = data flags read by the caves (pp_enable, g_w6Enable, ...). The FG live switch calls the game's FrameGenSettingWrite. That is a real swapchain rebuild, but FG was OFF in round 8 and Proton already blocks FG ON->OFF (1.7.0). | not involved |

## 4. Repro protocol (moved into remaining-work 6.1)
Use the PC only, and never the lobby or online. Launch with **`--enable_output_dump`** so the game writes
`%LOCALAPPDATA%\KoeiTecmo\NIOH3\dump\core_dump_*.dmp`. Without a dump a hit cannot be diagnosed. Close
Afterburner/RTSS for arms A-C.
- A **stock** (no ASI, no panel): 30 launches, alternating SSS ON/OFF in config.xml per launch (round8-style relaunch,
  enter_game.ps1, 25 s, title-menu scroll 20 s with the character model on/off screen).
- B **ASI release profile** (no panel): same 30 launches.
- C **legacy panel freeze**: same, with the panel applying fixes ~20 s after the window. This is the 10/06 condition.
- D (optional) A + RTSS running.
Count fatal errors per arm. A hit in A or D means the bug is the game's (report it to KT, keep the mod as is). Hits only
in C mean freeze timing: retire the panel's live apply, since the ASI path already avoids it. Each dump: read RIP/rax
(`rax` = bad stream pointer, null vs freed), the thread name (the minidump's ThreadNames stream), and `[dev+3C30]`
/ `[dev+3C38]`.

## 5. Follow-up (not done here; src has one owner at a time)
- ApplyBatch takes its thread snapshot **before** suspending. A thread created between the snapshot and SuspendThread
  runs unsuspended and unchecked while a multi-byte site is memcpy'd. At about 1 s after start the game is spawning its
  pools, so this is a small torn-patch window, unrelated to 2526D1. Fix: after suspending, re-snapshot. If a TID is not
  in the suspended set, resume all and retry (a pure retry, register-neutral). The same applies to Mem.fsx, which
  freezes via NtSuspendProcess and so is safe.
- The legacy panel's live apply (Mem.fsx safeWrite) checks only RIPs, not return addresses. cpu_caves documents its
  return-address choice, but W1/VRR/F1 rely on their sites having no inner calls. The panel is not in the release zips
  (release\package.py), so leave it as a dev tool.
