# PC2 Task A: user-settings audit (2026-10-14)

Verdict: FAIL the requirement that the mod never changes user-set game settings. There is one game-config writer with six call sites, plus two calls to the game's frame-generation setter and one dirty-flag persistence call. The addon has no game graphics-settings file access in the audited source.

Task: bridge #3234 from pc. Deliverable is the requested read-only audit and minimal patch plan. No game launch, build, installation, save access, restore, or source patch was performed. Future implementation belongs in a separate branch/worktree, never the installed bin.

## Source scope and method

- Workspace HEAD: `cad8a55`. Fetched origin/main: `556deac8a2d65a0e9b58607c9ed23793fb239e14`. `git diff HEAD origin/main -- mod` is empty: all audited mod source is identical at these refs.
- Searched 118 tracked files under `mod/Nioh3EngineFix/src` and `mod/Nioh3BossRematch/src`, plus patch descriptions and relevant tooling. Used `git grep` because `rg` is unavailable.
- Traced profile APIs, file read/write/create/copy/rename APIs, game configuration paths, known settings slots, native FG setter calls, and startup/exit hooks. All line numbers below refer to this source, not a deployed binary.
- The game's persistent graphics file here is `%LOCALAPPDATA%\KoeiTecmo\NIOH3\config.xml`, not the mod's INI. EngineFix reads its adjacent, module-derived `.ini`; BossRematch reads adjacent `Nioh3BossRematch.ini`.

## Complete direct game-config write inventory

All six calls below use `mod/Nioh3EngineFix/src/menu.cpp::WriteGameConfigFg` (line 691). It reads the entire XML (693-706), validates one `<FRAME_GENERATION>` element, copies a backup (730), writes a reconstructed whole file to `.Nioh3EngineFix.tmp` (735-738), then replaces the original with `MoveFileExW` (740). The backup path is `config.xml.Nioh3EngineFix.bak` and is overwritten on subsequent writes.

| Call site in menu.cpp | Trigger | Value written |
| --- | --- | --- |
| 1264, `SaveThread` | User selects Save to ini after a mod-menu FG change; `StartSave` fills the job at 1341-1349 | Current FG name; Proton excludes non-OFF here |
| 1433, `ChangeFgSafe` | Proton FrameGenToggle=3, user switches FG off | OFF; also pauses runtime FG if available |
| 1465, `ChangeFgSafe` | Proton FrameGenToggle=3, user switches/resumes FG on | FSR |
| 1502, `ChangeFg` | Proton FG on-to-off in the other enabled toggle mode | OFF for next launch |
| 2366, `ProtonFgEarly` | Startup finds a leftover FG marker and reads non-OFF FG | OFF automatically |
| 2383, `ProtonFgExit` | Armed marker, exit observes FG different from the stored desired value | Stored launch/mod-menu value, or OFF |

The startup call comes from `dllmain.cpp:1444`, inside the Wine check, without testing whether FrameGenToggle is enabled. Setting FrameGenToggle=0 therefore does NOT disable stale-marker recovery. `ProtonFgArmAtLaunch` (menu.cpp:799-804) also arms recovery for an already user-selected non-OFF value in mode 3, without any mod-menu change that session.

Exit writes are reached through the game's imported ExitProcess (`dllmain.cpp:1342`), self TerminateProcess (`1346`), and DLL detach (`3353`). Exit hooks are installed at `1445-1446`. A game-options change after marker arming can be replaced by the older launch/mod-menu value at exit. A user edit after a crash can likewise be replaced by OFF at the next startup.

Although this writer intentionally changes only FG in its input snapshot, it replaces the ENTIRE file after closing its read handle. There is no compare-before-replace against later game/user edits. A concurrent later edit to resolution, window mode, or another key can therefore be lost. This is a code-level race, not an observed reproduction of the user's resolution issue.

## Indirect writes to game settings

- `menu.cpp:622-629`, `CallFgWrite`, invokes native `FrameGenSettingWrite` at RVA 0x1FDDFCC. Calls are at 1449 (`ChangeFgSafe`, FSR) and 1512 (`ChangeFg`, selected type).
- This changes the persistent settings slots in memory, not just mod-owned state. Existing reverse-engineering notes (`notes/frame-generation.md:50-54`) record additional side effects: DLSS selects Reflex=1, XeLL=0, VSYNC=0; XeSS selects XeLL=1, Reflex=0; OFF/FSR clear Reflex and XeLL. Thus the impact is not limited to FRAME_GENERATION.
- `menu.cpp:632-636`, `SetConfigDirty`, writes the game's dirty byte at RVA 0x45C6F80; its call at 1524 is used when the menu change is considered persistent. The native SaveConfigIfDirty path serializes game settings. Merely removing direct XML writes is insufficient.
- Even if the mod stops setting the dirty byte, the game can later save changed in-memory slots when its Options/pause menu closes (the source explicitly documents this at menu.cpp:749-755). A session-only setter plus exit restoration does not meet NEVER and can overwrite newer user choices.

## Game-settings reads

| Location | Reads and purpose |
| --- | --- |
| menu.cpp:691-727 | Entire config.xml and existing FG value during every attempted direct write |
| menu.cpp:766-781, callers 2355 and 2377 | FG from config.xml at early launch and exit |
| menu.cpp:675-686 | Game's config path buffer at RVA 0x4B736F0, with LOCALAPPDATA fallback; early path at 2353 |
| menu.cpp:603-606 | Live FRAME_GENERATION slot at RVA 0x45BB008, used by the menu/setter result checks |
| menu.cpp:610-615, 648-658 | FG capability flags and game descriptor names/values; checks before enabling changes |
| osd.cpp:156, 361, 407, 564, 579, 1450-1451 | Live FRAME_GENERATION, RESOLUTION_SCALING, REN_FPS and DYNAMIC_FPS slots for display/telemetry |
| pmode.cpp:34 and Slot callers | Live VSYNC and FRAME_GENERATION slots for present-mode logging and VrrPresent auto selection |
| pmode.cpp:315-334 | Swapchain description/fullscreen state and current display frequency; no persisted mode write |
| samelook.cpp:322-354; extraperf.cpp:112-136 and ApplyS | Derived per-frame renderer configuration read before optional runtime overrides (see below) |
| BossRematch ui.cpp:215-246 | Existing game window/client geometry for positioning the addon's own overlay, not setting game resolution/window mode |

No explicit SCR_REZ, DISP_MODE, custom-width/height setter, or game-INI profile read/write was found in these two modules. This does not rule out whole-file replacement races or native serialization of changed FG-related slots described above.

## Own settings and runtime overrides

- `dllmain.cpp:412,478` derives EngineFix's INI from its module path; module initialization passes that path to feature readers and `menu::Configure` (`menu.cpp:2393`). `GetPrivateProfile*` calls in feature modules read mod settings. `livectl.cpp:159,234-236` reads the mod's separate live-control INI.
- Only two mod-INI write operations exist: `menu.cpp:1255` saves section/key/value triples to the mod INI (cache flush at 1261), and `1390` persists its own General.AutoContinueLoad switch. These do not target the game's XML.
- BossRematch `main.cpp:132` constructs its own INI; `IniInt` at 47 and the rematch/UI profile readers use it. The addon writes its own log/CSV and pipe replies; no game graphics-settings writer was found.
- `samelook.cpp:322-354` writes LPVR/SKYR/FOGT/CLDR into derived frame RenderConfig after ApplyGraphicsSettings; `extraperf.cpp:120-136` writes/restores cloud/fog/shadow/tessellation/AO/post fields. ExtraPerf also alters derived GI/LOD/particle values. These are runtime render overrides, not XML/INI writes or persistent user option slots.
- `pmode.cpp` can change Present arguments (SyncInterval/tearing) under the mod's VrrPresent switch; `osd.cpp:1034-1086` changes/restores swapchain maximum latency. These do not write the user's VSYNC/window-mode settings but can override effective presentation behavior.
- `bootnotice.cpp` BN2 deliberately returns false for the heavy-settings warning; it bypasses the reset prompt as No and does not request a settings reset. `tour.cpp` save-path hooks deny writes to protected save paths; they are not graphics-config setters.
- The separate renderer/presentation overrides should remain explicit mod-owned controls. If NEVER is intended to forbid even effective runtime quality/presentation overrides, those features need a separately scoped removal as well.

## Minimal patch plan

1. In an isolated branch/worktree, make the mod's FG row status-only in code for all legacy FrameGenToggle values. Keep read-only OSD/FG telemetry. Direct users to the game's Options for persistent game settings.
2. Remove WriteGameConfigFg and all six calls; remove gameCfg/fgValue and XML result handling from SaveJob/StartSave/SaveThread. Save to ini must save only EngineFix-owned keys to its own INI.
3. Remove CallFgWrite/SetConfigDirty and their call paths. A runtime native FG setter cannot meet this invariant even without explicit XML saves, because the game can persist its side effects.
4. Remove the Proton XML startup/exit repair and marker arming, plus their dedicated exit hooks/declarations. Ignore old markers; never use one to change the game file. Do not restore a historical backup or launch value. Retain any independent frame-generation crash fix that does not write user settings.
5. Update menu help/save feedback, FrameGenToggle comments/defaults and release/USER_GUIDE.md:34 (currently says the mod keeps Proton FG off in config.xml). Legacy INIs must not re-enable settings mutation.
6. Validate offline: build in the worktree and run the required pattern test, plus a focused settings-isolation regression harness with fake game files containing custom resolution/window-mode/FG values. Cover menu save, all legacy toggle values, stale marker/startup, exit, and edits after startup. Assert byte-identical game files and zero native setter/dirty-store calls, while own-INI saves still work. No live game test or deployment is part of Task A.

Removing only FG XML persistence, defaulting the toggle to zero, or restoring original values on exit is insufficient. The first leaves the native setter, the second leaves stale-marker startup recovery, and the third can erase newer user decisions.

## Adjacent tooling lead

Outside the ASI/addon, benchmark/setup tools do write or restore whole game configurations. Examples: `tools/testmatrix/gfxlib.ps1:37,50,57`, `fastab.ps1:199-210`, `cost_vs_looks.ps1:168-179`, `samelook_ab.ps1`, `tools/vs_capture.ps1`, and `tools/xsys_capture.ps1`. These were not run. Do not assume eliminating ASI writes alone prevents a separate harness from replacing custom values. The local modification to pc2_session_lib.ps1 was left untouched.
