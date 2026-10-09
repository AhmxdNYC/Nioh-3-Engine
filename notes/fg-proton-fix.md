# FSR frame generation under Proton: root cause and the safe-FG fix (ASI 1.7.44, branch `fgproton`)

Status: 2026-10-09. Static RE, the Ally logs and the public vkd3d-proton report. **Not run in-game.** Default off.

## Plain summary

- **The crash people report** ("enable FG on Linux / Steam Deck and the game crashes, then won't start until config.xml
  is deleted", vkd3d-proton #2845) is a **bug in the game's own code, not in vkd3d-proton**. The game's FidelityFX
  backend copies one frame-generation texture per generated frame. It checks for a missing texture in every job type
  except this copy. When one FG texture failed to be created, the first generated frame reads address `0x90` and the
  game dies. The game also turns a failed texture creation into "success", so nothing warns before that.
- **On the ROG Ally, FG on worked 3 out of 3 times** (mod menu, OFF -> FSR, live), with the AMD FSR presenter
  running. **FG off crashed 1 out of 1 time** (FSR -> OFF, live, ~2 s after the swapchain rebuild; v9 log). Starting the
  game with FG already on in config.xml was never tried on the Ally.
- **The fix (default off):** `[Menu] FrameGenToggle = 3` ("Proton safe FG") under Proton:
  1. **FGC** guard: the copy is skipped when a texture is missing (the stock code already skips it when the native
     resource is missing). Bit-identical whenever both textures exist. Also available alone: `[Patches] FGC = 1`.
  2. **FG off never rebuilds the swapchain.** It uses the engine's own FG pause (the one it applies while a menu is
     open) through a tiny call-site switch (**FGP**), so FG stops at once, and config.xml gets OFF for the next launch.
     FG on again = un-pause, no rebuild.
  3. **FG on is kept in config.xml**, so the next launch starts with FG. A marker file guards every session that runs
     with FG on: if it does not end cleanly, the next start puts FRAME_GENERATION OFF back before the game reads
     config.xml. No crash loop, no deleting config.xml by hand.
- **FG on the Ally at the user's settings costs latency.** In the U6 run FG showed 48.5 fps, but the FSR upscaler ran
  on only every 2nd displayed frame: **real fps fell from 34.3 to ~24.3**. The rest are generated frames. FG adds about
  7 ms of GPU work per real frame, and FSR FG holds back one real frame. Input-to-display roughly doubles (about 29 ->
  41 ms per real frame plus a 41 ms hold). FG is worth it on the Ally only when real fps with FG stays at 40 or more,
  which means about 50 fps or more with FG off. At 34 fps it trades smoothness for clearly worse input delay, as the
  user noticed.

## 1. Evidence

### 1.1 Ally runs (v2.02, Proton 11.0, gamescope 1080p @ 119.93 Hz)
| run | switch | result |
|---|---|---|
| v4 03:50 (`captures\ally\nioh3_ally_osdlines_v4-fg-on_*`) | OFF -> FSR at 04:03:20 (mod menu, FrameGenToggle 2) | new swapchain flags 0x842, "AMD FSR Presenter Thread"; a 2nd FSR rebuild at 04:10:48 (game-side) also fine |
| v9 14:18 (`nioh3_ally_modlog_v9-FGoff-crash_*`) | OFF -> FSR 14:21:16 ok; **FSR -> OFF 14:21:32** | new swapchain (flags 2) presented at 14:21:33.98, the log stops at 14:21:35.8: **crash**. No dump |
| U6 15:38 (`gpu_budget_1531\*U6*`) | OFF -> FSR 15:38:43 | ran 1.5 min; FFX FI passes in the GP profile (below) |

The config.xml FSR seen at 04:18 came from the game's own SaveConfigIfDirty, not from a launch with FG. The Ally agent
reset it before the next launch, so a **boot with FG on is untested on the Ally**.

### 1.2 The public crash, decoded (vkd3d-proton #2845, steam-3681010.log, Feb 2026 build, RX 9070 / mesa-git; doitsujin: also RDNA2, "the crash seems to happen inside the game's own code")
The FG swapchain is created (two `dxgi_vk_swap_chain_init` 2560x1440, "AMD FSR Interpolation/Presenter Thread"), then
4 ms later on `[App]Game`: `c0000005 read 0x90`, rcx = rdx = 0, at Nioh3+BA2AC. No vkd3d error is logged.
The backtrace maps onto v2.02 by matching the .pdata unwind shape (same size, prolog and stack allocation):

| that build | v2.02 | what |
|---|---|---|
| BA2AC | **4548C4** `mov r8,[rcx+90h]` | `KtglTextureNative(tex, sub)`: reads the texture's native resource, unchecked |
| 356DD10 (ret 356DD58) | **3718DB4** (ret 3718DFC) | FFX KTGL backend copy-job executor, 2nd lookup = destination |
| 356D078 | **371811C** | backend `fpExecuteGpuJobs` (interface +90h; table filled by 3719384) |
| 35DBB1C (9F50h frame) | **3788BA4** | FidelityFX frame-interpolation dispatch ("Clear Reconstructed Depth Interpolated Frame"), call `[ctx+0A8h]` at 3789A70 |
| 35D1FD8 / 3B70C | 377F060 / 4A4F94 | callers (FI context dispatch / frame function) |

### 1.3 Root cause in the game's backend (v2.02)
- The copy job is FSR 3.1's "store current buffer": `src = srv[CURRENT_INTERPOLATION_SOURCE]` (ctx+64D40),
  `dst = uav[PREVIOUS_INTERPOLATION_SOURCE]` (ctx+64E04). The destination is an FG-internal 2D texture at display size,
  backbuffer/HUD-less format, **UAV usage**, created when the FG context is created.
- `fpCreateResource` 3717F80 -> 3715CCC (Texture2D): if the KTGL create (2EE5798 for UAV textures, 3A7DC0 else) returns 0,
  it **stores the null and returns FFX_OK** (3715EA9 -> 3715F5E). Types other than buffer / 1D / 2D are not created at
  all and also return OK.
- Clear executor 37187CC (`test rcx,rcx` at 37187EE) and compute executor 3718934 (`test r9,r9` at 371899D) skip a null
  texture. **Only the copy executor 3718DB4 calls 4548C4 on it unchecked.** Its own null test (3718DFF) comes after the
  lookups.
- Why the texture is null on those systems is not known (no vkd3d `err:` was logged around the create, so the KTGL layer
  refused it, or its allocation failed). The likely triggers are the UAV format support vkd3d-proton reports for the
  backbuffer format, or a size or heap limit at 2560x1440. It is deterministic on the affected systems, so a boot with
  FG on crashes every time, which explains "won't start until config.xml is deleted". On the Ally (RDNA3, 1080p, B8G8R8A8)
  the create succeeds.
- **FSR -> OFF (v9):** no dump, so no proof of the cause. The rebuild tears down the FFX FG swapchain and its presenter /
  interpolation threads. `FfxFiWaitFence` 370F41C waits with `INFINITE` when `g_FfxFenceWaitMode` 4BD2580 = 1, which
  ApplyGraphicsSettings sets for FSR FG and **never clears**. That is a candidate, not a finding. Safe FG avoids the path
  completely.
- `g_FfxFenceWaitMode`: 0 = spin on GetCompletedValue, 1 = SetEventOnCompletion + WaitForSingleObject. Mode 1 is the
  right one under vkd3d-proton (ntsync waits). No change.

## 2. The fix (1.7.44, all default off)
| piece | site | what |
|---|---|---|
| FGC1 / FGC2 (`[Patches] FGC`, or FrameGenToggle 3 under Proton) | 3718DD9 / 3718DF7, `call 4548C4` -> caveFgcSrc / caveFgcDst | null texture -> return 0 (what 4548C4 returns for a texture without a native resource), so 3718DFF skips the copy. Non-null: tail jump to 4548C4. Counts copies (= generated frames = real fps while FG runs) and null skips. Logs `FGC: ... NULL texture skipped` within a second, plus `stats/s FG:` every 30 s |
| FGP (FrameGenToggle 3 under Proton) | 39A865, `call 5A37D4` (menu-open getter in ApplyGraphicsSettings) -> caveFgp | `fgp_pause = 1` answers "menu open". The engine takes its own FG pause path (S+2055h = 1, 39A945). Real frames only, no swapchain rebuild. 0 = tail jump (stock) |
| `[Menu] FrameGenToggle = 3` | menu.cpp `ChangeFgSafe`, `ProtonFgEarly/ArmAtLaunch/Exit` | OFF <-> FSR only. **Off:** FGP pause + config.xml OFF (without FGP: config OFF only, FG runs until the next launch). **On:** un-pause, or (setting OFF) the game's setter = the OFF -> FSR rebuild that ran 3/3 on the Ally. Then the marker is armed **first**, and only then does config.xml get FSR. **Launch with FSR in config.xml:** the marker is armed before the game reads config.xml. **Clean exit:** config.xml = the user's last choice (it fixes the FSR the game writes back from memory when its Options menu closes during a pause); marker deleted. **Marker left over:** the next start puts OFF back (existing 1.1.0 logic) |

All sites are pattern-verified with their callees and dependency bytes (`patches.json` groups FGC / FGP;
`tests\test_patterns.py`). Windows: FrameGenToggle 3 = 2 (the switch); FGC only with `[Patches] FGC = 1`.
What it cannot fix: if the missing texture is the previous-frame copy, FG keeps running without it. The generated
frames may then be wrong, because they blend against stale or empty data. The log says so, and the user turns FG off
(pause) in the mod menu.

## 3. Ally test protocol (send to the Ally agent; max 3 launches; crash-safe)
Setup: ASI 1.7.44 from branch `fgproton`. In the ini set `[Menu] FrameGenToggle = 3` and `[Patches] FGOnWine = 0` (F1
off, to isolate). Keep every other key. No RTSS / overlays besides MangoHud. Add the game's own launch argument
`--enable_output_dump`, so a crash leaves `...\KoeiTecmo\NIOH3\dump\core_dump_*.dmp`. Use the U1/U6 save and spot
(Futamata shrine, standing), USER settings.
0. Game closed: `cp config.xml config.xml.fgtest.bak` (in `.../compatdata/3681010/pfx/drive_c/users/steamuser/AppData/Local/KoeiTecmo/NIOH3/`).
   FRAME_GENERATION must be OFF. Make sure no `Nioh3EngineFix.fgmarker` exists in the game folder.
1. **L1 (live, the known-good direction):** launch. 60 s FG off (baseline). In the mod menu set FG ON. The log must show
   `OFF -> FSR (Proton safe FG)`, `marker ... written`, `config.xml FRAME_GENERATION = FSR`, `AMD FSR Presenter Thread`.
   Take 60 s. Then FG OFF: the log must show `FSR FG paused now ... FGP`, and **no** `presenting swapchain changed`.
   Take 60 s. Then FG ON again (`back ON ... FGP pause cleared`) and take 60 s. Quit to desktop from the title menu. The log must
   show `Proton exit (ExitProcess) ... is FSR, wanted FSR`.
2. **L2 (boot with FG on, never tested on the Ally):** launch. The top of the log must show `Proton safe FG ... launching
   with FRAME_GENERATION FSR - crash guard armed`. Load the save and take 60 s with FG. Then FG OFF (pause) and quit. The log must
   show `wanted OFF`, and config.xml must have OFF.
3. **L3 (OFF restored / crash recovery):** launch and take 30 s, then quit. Expect `FRAME_GENERATION OFF at startup`. **If L1 or L2
   crashed**, L3 is the recovery test: the log must show `ended with FG switched on by the mod (crash?) ... putting OFF
   back`, and the game must boot normally. Stop there.
- After any crash or freeze: no more FG launches. After L3, copy `config.xml.fgtest.bak` back with the game closed,
  unless it already matches.
- Report: the three mod logs (`nioh3_ally_modlog_fgp-L1/L2/L3_*.log`); the lines `FGC|FGP|Proton|FG state|stats/s FG|swapchain|Presenter|OSD: stats`;
  the MangoHud CSV for each 60 s window (fps, frametime p99, GPU %, GPU W); any `core_dump_*.dmp`. Also one sentence on
  feel (latency, artifacts) for FG on vs paused. Key numbers: `stats/s FG: copy jobs N` = real fps with FG (expect about 24 at
  USER settings), displayed fps, and real fps while paused (expect about 34, i.e. the FG overhead is gone).

## 4. Files
`mod\Nioh3EngineFix\src\caves.asm` (caveFgcSrc / caveFgcDst / caveFgp, `fgdata`), `src\dllmain.cpp` (FGC / FGP install,
stats lines), `src\menu.cpp` / `menu.h` (safe FG), `patches.json` + `tools\gen_header.py` (sites, deps, targets),
`Nioh3EngineFix.ini` (`FrameGenToggle = 3`, `[Patches] FGC`). The decode scripts were scratch only (capstone on
`re\Nioh3.dump.exe`, .pdata unwind matching).
