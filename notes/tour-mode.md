# Tour mode: an automatic stage / map tour for the stability and performance test (remaining-work 6.2)

Static work only (decomp_ro + capstone on `re\Nioh3.dump.exe`, v2.02, RVAs); the game was not run. Goal: the mod itself
(no input injection) loads every stage in turn, moves the player to a few representative points per stage, dwells there,
and logs per point fps / frame-time p99 / CPU / GPU passes / memory / handles / load time; crash-resumable; save-safe.
Target: a full tour in ~15 min on the PC (user request), optional second pass as the leak check.

## 1. What the engine offers (a): stages, missions, loading

| Item | Where | Use in the tour |
|---|---|---|
| Mission master table | `**(45B9E30)` = dm: records `*dm+8+idx*0x198` (count `*dm+4`), id->index map `dm[4]` ({i32 key,i32 idx} slots at +8..+10h, empty key at +4; `MissionIdToIndex` 377B8C) | the **mission id is the u16 key at record +0x158** (`notes\missions.csv` col 2; confirmed: Emaki 0xCC96 = 52374 and quest 0x86EC = 34540 are row keys). 130 records, 20 main slots |
| Mission start | `MissionRequestById(id, slotByte, sub=0, params*)` 2239600 -> `MissionRequestEnqueue` 223953C (3 x 0x58 queue at 45BC420; `+4` id) -> `MissionLoadStep` 223AF50 | **the tour's stage load.** Same call as `Mission::ChangeMission` (23A313C) and the shrine **Mission Replay** menu (F1FD80, decide branch): params `{0x101, 0x1000000, 0x10100, 0, 0, 0}`, slot = `[[47514D0]+8]+10h` (journey/cycle byte). The replay menu additionally sets `[[g_ClearDb 4751848]+8]+0xCF8 = 1` (replay marker); the tour does this only for already-cleared missions |
| Unlock / clear state | `MissionUnlockedGet(g_ClearDb,id)` 909218 (byte +0x18 of the 2E0h map entry), `MissionClearedGetSlot(g_ClearDb,id)` 702B64 | Tour 1 filter (= what the replay / mission menus show) |
| Region (era) warp | `MissionRequestRegion` 22399B8 (World::WarpOtherWorld*) | not needed: a mission request already loads its world |
| Load window | `G+58Ah` (G = `*(4759910)`), set/cleared by the sequencers; LoadIsLoadEnd 3B3424 gates G1-G9 (loading-bottleneck.md 3.2) | "load done" = queue empty, 58A seen set then clear, player present, both warp sequencers idle, +3 s settle. LoadIsLoadEnd itself is **not** called by the tour (it advances the 120-unit ready-wait accumulator) |
| Tip page after a load | slot 2Bh "press to continue" (auto-continue-load.md) | the wrapper sets `[General] AutoContinueLoad = 1`; without it every mission load would wait for a button |

There is no stage table separate from missions: a "stage" is what a mission loads (stage families S00_EDO..S04_BAK, S90_DLC1,
`data\stage_names.tsv`). The 0xC4 area records (`missions-areas.csv`, +0x34 "position") are map-icon coordinates, not
safe world positions; not used for teleports.

## 2. Moving the player (b)

| Mechanism | RVA | Verdict |
|---|---|---|
| **Fast-travel sequencer** `g_FastTravelSeq = *(4757470)`, ticked by `FastTravelSeqTickCall` 3B29E4 from PerFrameUpdater 5651D0 (call at **56529C**) | request: `FastTravelRequestPost(req)` 22395AC -> `FastTravelSeqRequest` 3B2488 (copies a 0x78-B request to seq+10h, state 1) | **used.** States: 1 checks, 2 sets 58A + loading UI + fade, 4 places the player (`669AB8(player,pos,0)`, yaw, physics reset), 5/6 wait LoadIsLoadEnd + path init + UI idle (streaming around the new spot), 7 clears 58A, 8 fade in (1.5 s) and reset. Exactly a shrine fast travel, so it streams the destination and measures the load |
| request layout (FastTravelRequestTo 1C282CC / 3B23AC) | +0 pos xyzw, +10h yaw deg, +14h cam flag, +18h/+1Ch cam angles, +20h long ground snap (raycast +-100000), +21h short ground snap (+-100), **+24h point id**, +29h..+2Bh flags (+2Bh = 1 native), **+2Ch "no redirect"**, +30h popping wait, +38h..+77h functor (null) | tour: `FillWarpPoint(req,id)` 3B2528 (the game's own pos/rot incl. gadget-relative points), then **+24h = 0** (state 4 with a point id registers it as the retry shrine `[[47514D0]+8]+14h` and calls 7D3518/A62110/5B635C = autosave request), +2Ch = 1 (else state 4 may redirect to the current retry point when `[[g_ClearDb]+8][slot]` != 0), +21h = 1, +2Bh = 1 |
| state-1 refusals | A61E98: `[[45B9E40]]+27A0h` != 0 (no-warp mode, `Mission::StartNoWarpMode`) aborts; 7D6F6C waits while `**(47098A0)` (WarpSeqB) runs | tour checks both before posting; a refused warp is logged and skipped |
| warp points | registry `*(47098E0)`: vector +18h..+20h of 16-B entries; entry ptr+10h = point set ps: `ps[0]` data (`+460h` != 0, `+418h` hdr: count +4, 0xA4-B records at +8; pos +40h, flags +8, gadget link +0Ch, cam +7Ch/+80h), `ps[3]` id->index map (same layout as the mission map) | the tour enumerates all loaded points (designer warp / shrine / event points), so it never teleports to an arbitrary grid position |
| `Player::SetPos` 23AB4DC (script) | 1C27010 + streaming focus `[45B9E58]+10B0h` + path init | instant, no load wait, needs the VM context; not used |
| free / debug camera | none found: only `Camera::*` script bindings (talk/script/stage-anim cameras); no debug free-cam code referenced | not available; the tour moves the player instead |

Points chosen per stage (default 3): the arrival spot (mission spawn) + farthest-point sampling over the loaded warp points
(designer positions, so on walkable ground). "Heaviest spots" cannot be known statically; the per-point log finds them and
`summarize_tour.py` ranks them, so a second run can list them in the TourList (`key@x,y,z` lines are accepted).

Invulnerability / AI pause: `Player::NoDamageMode` (23AA9B8) / `Mission::NoDamageMode` (23A5308) write a byte at
`owner+3BAh / +3B9h` where owner = `[[[vmctx+8]+8]+90h]` (the calling script's owner, reachable only through a VM context);
`Character::SetNoDamage*` (2396F5C / A30824) queue an actor command built by script helpers; `Character::SetAllEnemyDeactive`
(23937D4) switches enemy areas off (changes what is rendered and simulated). **No safe, script-free flag found -> not used.**
The player is parked on designer warp points for 4 s; a death is detected (player gone / 58A outside our warp) and logged,
the game respawns at its shrine and the tour carries on.

## 3. Progress, flags, saving (c)

- In memory a mission load or a warp can change: mission flags (`MissionFlagSet` 70275C, scripts), clear-db entries
  (unlock/clear/discovered `+2Bh`), the retry shrine (`[[47514D0]+8]+14h`, only for warps WITH a point id - the tour sends 0),
  the replay marker +0xCF8, map discovery. A warp into a not-yet-unlocked mission (Tour 2) runs that mission's script on a save
  that does not expect it: possible story flags set early, an event/cutscene that waits, a soft-lock. None of it reaches the
  disk while saves are blocked (below); the wrapper restores the save folder anyway.
- **Save writes** (all found statically): `SaveRequest(saveMgr 45C8480, type, reason, flag)` 5B63A8 (14 call sites, incl.
  `AutosaveRequest` 5B635C after every fast travel / mission load) and `SaveEnqueue` 5B6CB0 (3 more entry points) feed the
  [App]SaveLoadThreadMain. Its write step 5B6D68 encrypts (5B7F4C) then calls the only user-save writer
  **`SaveFileWriteAtomic` 5B7BB4** (CreateFileW 5B7AE4 `%s.tmp`, WriteFile, FlushFileBuffers, MoveFileExW to SAVEDATA.BIN)
  at **5B6F37** (single call site). `BACKUP.BIN` is written by 1FD9A48 from the load path (1FDF2D0, 1FDF62A).
- **Block (layer 1, ASI, whole process lifetime when Tour != 0):** the three E8 sites (patches.json SB1-SB3) go to a near
  thunk that returns "written OK" (al = 1, `*err = 0` for SB1) without calling the writer; every skip is counted and logged.
  The game believes the save worked: no error dialog. Register-safe: the original writers call imports (all volatiles
  clobbered), `hook_liveness.py sites`: nothing live and preserved after any of the three.
- **Verify (layer 2):** while Tour != 0 the game's IAT entries CreateFileW / MoveFileExW / MoveFileW / DeleteFileW / CopyFileW /
  SetFileAttributesW are chained: any write-type operation on a path containing `\KoeiTecmo\NIOH3\Savedata` (includes
  `Savedata_backup`) is denied (ERROR_ACCESS_DENIED) and logged as `Tour: SAVE GUARD denied ...`. Expected count: 0 (layer 1
  catches everything); a non-zero count means a write path we did not know about, and it still did not reach the disk.
  Reads stay allowed. The mod's own files are untouched (the mod has its own imports).
- **Restore (layer 3, wrapper):** `run_tour.ps1` copies `%LOCALAPPDATA%\KoeiTecmo\NIOH3` to
  `D:\nioh3-re\save-backups\<stamp>-tour` (+ SHA-256 manifest) before launch, compares after the game exits (reports any
  change = a guard miss) and restores byte-for-byte (then re-hashes: must equal the manifest).
- Steam Cloud: it syncs at game exit. With layers 1-2 the files are unchanged, so nothing new is uploaded. If the post-exit
  compare ever shows a change, the cloud may hold the toured save: the wrapper prints that and the user picks the LOCAL copy in
  Steam's conflict dialog. (Risk, not seen statically.)

## 4. Online (d)
Signals from `online-session-detect.md`: `net = *(45C84F8)`, connected = `net+1DCh`; play mode `*(4709B50)+34h` != 0. The tour
refuses to arm while either is set and disarms for good (latched) if one appears mid-tour (SEH-guarded reads).

## 5. Decision: modes

| Mode | Stages | Writes | Risk |
|---|---|---|---|
| `Tour = 1` | TourList filtered to missions the save has unlocked or cleared (what the replay menu offers) | blocked + restored | low: the game's own replay/fast-travel paths, positions from the game's warp table |
| `Tour = 2` | the whole TourList, unlock state ignored | blocked + restored | medium: scripts of not-reached missions may wait on events (watchdog skips after TourLoadTimeoutSec; the wrapper kills and resumes on a hard stall) |

Defaults (built-in list = 15 main missions covering every area group: 7356 52 7292 5631 50990 12009 60021 31541 13587 42164 19854
7658 45645 4953 46078; `tools\tour\tourlist_full.txt` = all 20 main + 6 crucible stories), TourDwellSec 4, TourPointsPerStage 3:
per stage ~30 s load + 3 s settle + 3 x 4 s dwell + 2 x ~5 s warp = ~55 s -> **15 stages ~ 14 min + boot/title ~1.5 min = ~15 min on
the PC** (Ally loads are 3-4x longer: ~30 min). `TourPasses = 2` repeats the list for the leak check (~30 min PC); the summary
compares VRAM / commit / handles per point between passes.

## 6. Per-point measurements (the tour log, plus the existing profilers in "window" mode)
- tour (game-thread tick): frames, avg fps, frame-time p50/p99/max, process CPU cores busy, main-thread busy %, private / working
  set / commit, handles, threads, system commit, VRAM local/non-local (memstats' adapter), player position, mission, region,
  load / warp seconds. Lines: `Tour P <pass>/<stage>/<pt> ...`.
- `[Profiler] GPUProf = 2` (GPUProf2_SampleEvery 8) and `SP = 1`: while the tour runs, their periodic 30 s windows are replaced
  by one window per dwell (`gp::SetExternalWindows`, `sp::SetExternalWindows`): "GP window tour ..." + top passes / PSO hashes,
  "SP window tour ..." + top functions per thread group. Outside the dwell SP does not sample.
- `[Profiler] LoadStats = 1, LoadGates = 1`: per-load window + which gate held; the summary joins by time.
- PresentMon (wrapper): per-frame GPU busy / CPU busy / display latency; joined by wall time.

## 7. Risks (honest)
1. Never run live. Every structure offset above is from the decompile; the first run must be `Tour = 1` with the save backup.
2. Title screen: the mod does not press Continue. The wrapper waits for the user to press Continue once per launch (one
   press, not a robot); the tour arms itself ~5 s after the field is reached. Making the title step automatic needs the title
   menu's decide path (not traced).
3. Mission scripts may start cutscenes / dialogs that wait for input (Tour 2 mostly): the per-load watchdog logs STALL and
   moves on; a hard stall (no tour line for TourStallSec) makes the wrapper kill the game, the next launch resumes after it.
4. Warp points include event / gadget points: some may be odd spots (inside an arena, a boss trigger box). FT ground-snaps
   +-100 units; a boss trigger or death is logged and skipped.
5. No invulnerability. Death at a point -> the game's own respawn (its autosave is blocked).
6. In-memory state after the tour is not a valid play state: the wrapper closes the game at the end; never keep playing.
7. Steam Cloud (section 3). 8. Online sessions refused.

## 8. Build, run, tests (1.7.36)
- Code: `mod\Nioh3EngineFix\src\tour.cpp` (+ tour.h, tour_core.h), patches.json group TR (TT1 SB1 SB2 SB3, check-only), small window
  hooks in gpuprof.cpp (`SetExternalWindows`, `LogWindow`), sp.cpp (`SetExternalWindows`, `BeginWindow`, `EndWindow`) and
  memstats.cpp (`Vram`). Ini: `[Debug] Tour = 0` + Tour* keys (Nioh3EngineFix.ini documents them).
- Run: `powershell -File tools\tour\run_tour.ps1 [-Mode 1|2] [-List main|full|builtin|<file>] [-Points 3] [-DwellSec 4] [-Passes 2]
  [-Resume] [-NoProfile]`. Press Continue at the title once per launch; nothing else. Output: `captures\tour\<stamp>-tour\`
  (logs\runNN.log per launch, progress.txt, presentmon.csv, save_check.json, tour_report.md, tour_points.csv).
- Profiling during the dwell only: GPUProf = 2 with GPUProf2_SampleEvery 16 / MinRun 8 / AB 0 (the "GP2: overhead" line reports
  the cost), SP = 1 at IntervalUs 5000 (~200 main-thread samples/s, ~800 per 4 s dwell). `-NoProfile` drops both.
- Offline tests: `py -I mod\Nioh3EngineFix\tests\run_tour_harness.py` (ALL PASSED), `tests\test_patterns.py` (ALL 232 SITES OK),
  `run_tour.ps1 -DryRun` (fake game: crash at stage 2 point 2 + save scribble -> relaunch, resume, DONE, save change detected and
  restored byte-for-byte, report written).
- First live run: `-Mode 1 -List main` on the PC with a fresh backup; check in the log: `Tour: installed`, `ARMED`, the first
  `stage 1 arrived` and `Tour P` lines, `Tour: stage 1 points: N loaded warp points` (N > 0 confirms the registry walk), and
  `save writes skipped` > 0 after the first warp's autosave with `guard 0`.
