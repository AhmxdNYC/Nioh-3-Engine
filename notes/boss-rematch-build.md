# Boss rematch: Phase 1 MVP build (ASI 1.7.40, branch `rematch`, 2026-10-08)

Built from `boss-rematch-design.md` s.2 / s.3 / s.5.5 / s.9. **Not run in game yet.** Default off (`[Rematch] Enable = 0`): with 0 nothing
is installed and the menu row is hidden. Offline proof: `tests\test_patterns.py` ALL 268 SITES OK, `tests\run_rematch_harness.py` ALL
PASSED, the vpad / padlat / tour harnesses still ALL PASSED, `build.bat` builds without new warnings.

## 1. What is built
| piece | where | notes |
|---|---|---|
| game-thread pump | `src\rematch.cpp` hook on `MissionLoadStep` 0x223AF50 | Every game call and every write of the module runs here. **Chains** onto bosslog's hook when BossLog / RematchLog is on: the entry then holds `E9 rel32 + NOPs` (bosslog's), rematch verifies that shape plus the bytes after the prologue and only rewrites the rel32 (its thunk jumps on into bosslog's thunk). bosslog.cpp is unchanged; dllmain starts rematch **after** bosslog. Same for BossBattleMode 23A2DCC / End 23A2F70 (the "in a fight" gate). |
| gates (s.2) | `Gates()` | online (`[4709B50]+34` 1/2), co-op (`[45C84F8]+1DC`), loading (G+58A), paused (58B/58C), no player (`[4745348]+0xCD*0x18`), mission queue changed in the last 10 s, in a Picture Scroll (`[[45B9E40]]+2C18`), in a boss fight, a game menu open (`menu::GamePauseReason() == 1`), our own state not idle; the menu side also says "not in game" when the pump has not ticked for 2 s (it does not tick at the title). Checked when the row is drawn and again on the game thread before the call; a request waits up to 5 s for the gates, then is refused with the reason (OSD toast + log). |
| R-a | `ExecOpen()` | `MenuOpenMissionReplay(0, 0, 6)` 0x1DE28E8 = exactly what the script native `UI::OpenMissionReplayMenu` 23F90CC does, after the same "busy" vcall (+0x48 of `[g_MenuMgr+0x4E0]`) that `BattleScrollTop_Select` makes before its own call (1DAFF17). The player picks the mission in the game's list; the game's own replay runs. |
| lifecycle | `Track()` | Idle -> MenuOpen (game menu signal or +0x48 seen; never seen in 6 s -> idle) -> InReplay (queue slots changed, a load, or the mission-state id changed) -> replay mission recognised 3 s after its load -> exit when the id changes for 5 s (a retry reload of the same id is not an exit). Menu closed without a start for 5 s -> idle. Load never happening in 180 s -> idle. |
| cleared scan | `DoScan()` | On the game thread when the sub-page opens: `MissionClearedGetSlot(db = [4751848], id)` 0x702B64 per catalogue row (the getter the replay list builder 378674 uses, current journey). The first scan also logs `MissionUnlockedGet` 909218 per row (`Rematch: scan mission ...`), for the s.4 getter question. |
| HiddenDuels | `UnhideApply / UnhideRestore` | `[Rematch] HiddenDuels = 1`: clears **bit 0x4000** of record `+0x40` for 0xD22D / 0xB3F8 / 0xFB96 right before R-a (records found by scanning `[[45B9E30]]`, 0x198 B, id at +0x158; VirtualProtect only if the page is not writable), restores the snapshot on menu close / mission exit / abort / timeout. If the game changed the u32 meanwhile, only our bit is put back (logged). Game data, never saved. |
| R-b | `ExecDirect()` | `[Rematch] DirectStart = 1` routes the confirm here; **always refused** in this build (`refused: direct start needs the L7 replay capture`). TODO in the code: template of the 0x58-byte request + mission-id offset from a real L7 capture, then R-13. |
| menu | `src\menu.cpp` (small hunks) | Section **Gameplay (solo/offline)**, row `Boss rematch... (Rematch)`: value = "ready (A: list)" or the gate reason, yellow "active: ..." while running. A opens the sub-page: header + gate line, `Open the game's Battle Scroll replay menu (R-a)`, `Abort and restore now` (only while active and before the replay mission loaded), then one row per catalogue mission cleared on this journey: `name  region  beaten  replay | hidden duel` (ShowAll lists the rest greyed; hidden duels need HiddenDuels = 1 and show `unverified` unless ShowUnverified = 1). A on a boss row = confirm line (`Start rematch: <name>? A confirm / B back. Solo only. Drops: as the game's replay.`), A again = queue + close the menu + toast `Rematch queued: <name>...`; then `Rematch: pick '<mission>' in the replay list` or `Rematch refused: <reason>`. B: confirm -> list -> main menu. VirtualPad: `menu get Rematch` (0 idle, 1 replay menu open, 2 in replay), `menu set Rematch 1` (R-a), `2` (abort). |
| catalogue | `tools\rematch_catalog.py` -> `mod\Nioh3EngineFix\Nioh3EngineFix_bosses.tsv` (copied to bin\ by build.bat; installed next to the ini) | 62 route-R rows from `notes\battle-scrolls.csv`: the 59 listable missions with a fight (tutorials / training dropped) + the 3 hidden duels (hidden = 1, verified = 0). Design s.4 columns + `mission_name`; E0/E1 columns are `-`. The mod reads columns by header name; `--check` = in sync. |

Byte checks: rematch.cpp `kSites` (13 entries) = hooks RM1 / BL1 / BL2 (their patches.json len = our relocated head, so the chained shape is
exact), call targets RM14 / RM15 / RM17 / RM30 (`MenuOpenMissionReplay`, 45 B), and code references RM31 (IsOnlinePlay `+34 == 1`), RM32 (queue
free-slot scan, stride 0x58), RM33 (clear db `[4751848]` -> 702B64), RM34 (mission table `[[45B9E30]]`), RM35 (+0x158 = id), RM36 (list filter
`+0x40 & (1 | 0x40 | 0x4000)`). Nothing is enabled unless all 13 match. test_patterns checks every kSites entry against the dump and patches.json.

## 2. Corrections found while building (design note and battle-scrolls.md were wrong here)
- **Hidden duels use bit 0x4000, not 0x40.** The list builder skips a record when `+0x40` has bit 1, 0x40 or 0x4000 (378B83); the three duels
  carry `0x24180` (CSV `flags40`), i.e. 0x4000. Un-hide clears 0x4000.
- **A consumed mission-queue slot is not cleared.** Every RematchLog capture shows slot 0 = `{+0 1, +8 E0, +C stage}` from the save load for
  the whole session, which is "occupied" by the game's own free-slot test (`+4 == 0 && u8 +8 == 0`). The s.2 gate "no slot busy" would never
  pass; the gate is now "slot bytes changed in the last 10 s" and a replay start = any slot change after the menu opened.
- `MissionLoadStep` does not run at the title (first L4 tick only after the save loads), and mission-state `+0x14` is a stage id
  (0x3D5538 / 0x3D9F1D in the captures), not the 16-bit record id: the tracker only watches it change.

## 3. ini (`[Rematch]`, next launch)
`Enable = 0` (1 = row + hooks), `HiddenDuels = 0`, `DirectStart = 0` (leave 0: refused until Q10), `ShowAll = 0`, `ShowUnverified = 0`,
`Catalog = Nioh3EngineFix_bosses.tsv`. Log lines: `grep "Rematch:"` (plus `RematchLog` / `BossLog` when those are on). `UnlockEarly` is not in
this build: no story-flag writes at all.

## 4. Test cases (user, PC or Ally; solo, offline; save backup first)
- **R-1** Enable = 0: no row, log has no `Rematch:` line. Enable = 1: `Rematch: on (... 62 catalogue rows ...) Hooks: MissionLoadStep
  BossBattleMode BossBattleModeEnd` (with BossLog on: `(chained after BossLog)` on each). Row value: `ready (A: list)` in the field; `loading`
  during a load, `in a boss fight` after the boss bar appears, `in a Picture Scroll` inside one, `game menu open` with a game menu up,
  `not in game (no mission tick)` at the title.
- **R-11** At a shrine (shrine menu closed): F10 -> Boss rematch -> A -> pick a beaten main boss -> A, A. Expect the toast `pick '<mission>'`,
  the game's replay list, log `Rematch: R-a MenuOpenMissionReplay(0, 0, 6) called`, `replay menu +0x48 = 1`. Start that mission in the list:
  `replay start seen`, `IN replay mission`, the boss is present; quit -> `replay mission ... exited`. Also once from the open field (Q7: does
  the menu work outside a shrine?).
- **R-12** HiddenDuels = 1, ShowUnverified = 1: the list also shows Honda Tadakatsu / Yoshitsune / Jubei & Chuya (if cleared); open the
  replay list -> `HiddenDuels: 3 of 3 mission records un-hidden ... D22D:00024180->00020180 ...`; the three appear in the game's list; one
  replays and finishes; after exit `HiddenDuels restored (replay mission exit): ... (no "changed by game")`. Backing out of the list instead:
  `restored (replay menu closed)` within ~5 s.

## 5. Phase 0 capture still needed (unattended script, NOT run)
`tools\rematch\p0_vpad.ps1` drives the game only through VirtualPad (owner pid, MaxMinutes, commit headroom, crash limit, game lock,
ini / asi / catalogue backup + restore, save-folder copy to `D:\nioh3-re\save-backups\<stamp>-rematchp0`). Run line (PC game turn, nothing
else using the game):
```
powershell -NoProfile -File tools\rematch\p0_vpad.ps1 -OwnerPid <agent pid> -AsiPath mod\Nioh3EngineFix\bin\Nioh3EngineFix.asi
           [-ShrineDown 3] [-ListDown 0] [-InteractBtns B,Y] [-PictureScroll [-ScrollScript cmds.txt]] [-QuitScript cmds.txt]
```
`-DryRun` (checked: OK) touches nothing and prints the step plan and the test-ini values. Output `captures\rematch\p0_<stamp>`
(`trace.txt`, `rematchlog.txt`, `result.json`, both launch logs). Steps: Continue -> interact at the shrine (B, then Y; checkpoint
`menu_pause=1`) -> Down x3 + A (+ one A for the Battle Scroll top menu) -> checkpoint `L7: MissionReplayMenu_Setup / replay menu open`
(P0-5, with the 702B64 / 37AFC0 / 909218 getter lines); not reached -> back out + the mod's R-a via `menu set Rematch 1` -> A until
`L7: MissionRequestById / MissionRequestEnqueue` (**Q10, what R-b needs**) -> `IN MISSION` + field -> 20 s -> quit (QuitScript or a normal
game close). `-PictureScroll`: second Continue, then the item-menu `-ScrollScript` or shrine -> Battle Scroll -> option 1 -> A until `L2`.
Needs: a save made **standing at a shrine**, Battle Scroll unlocked (flag 0xAB, after "Crucible Manifested"), at least one cleared mission
in the replay list (the default selection is started), VirtualPad-capable ASI (>= 1.7.37; 1.7.40 for the mod fallback).

## 6. Open, and why (updated 2026-10-08 from the PC play session, `notes\session-2026-10-08-gamedata.md`)
- **BLOCKER - the pump**: `MissionLoadStep` runs only while a mission loads (RematchLog: 2 ticks in 150 s of field play). Every request
  and the "not in game" gate depend on it, so R-a / R-b never execute in the field. Move the pump to a per-frame game-thread site (the
  PerFrameUpdater call 56529C, tour TT1, chained) before R-1 / R-11. Not done in `rematch2` (needs a new site + test_patterns change).
- **R-b direct start (Q10): DONE on branch `rematch2`** (`ExecDirect`, default `DirectStart = 0`): `MissionRequestById(id, u8
  [[47514D0]+8]+10h, 0, alignas(16) {101h, 1000000h, 10100h, 0, 0, 0})` = the shrine confirm F1FD80's inline write (choice 0, logged byte
  for byte); refuses uncleared / unverified rows, a full queue, and a call that leaves no slot with the id and mode 2; then the replay
  tracker. Harness: R-b cases + dump pins of both request writers. The replay marker `+CF8h` is not written (the logged path did not).
  p0_vpad step B (the L7 capture) is no longer needed; R-13 (3 bosses) stays a game test after the pump fix.
- **Q7 (field) / +0x48 / hidden duels**: unchanged (below). **Getter conflict: closed** (702B64 == 909218 on all 271 logged lines).
- `menu.cpp` / ini text still say DirectStart is "not built": update when `rematch2` is merged (the src owner).
- Superseded (kept for history):
- **R-b direct start (Q10)**: needs the replay's own enqueue arguments; only the Continue-path enqueue is logged so far. p0_vpad step B.
- **Q7 / context**: whether `MenuOpenMissionReplay(0, 0, 6)` with no owner menu works in the open field, and what the list returns to on B.
  The script native passes owner 0, the shrine passes its own menu object; R-11 checks both.
- **+0x48 vcall meaning**: used as the game's own "already open / busy" guard (BattleScrollTop) and as a second "menu open" signal; if it
  does something else the log shows it (`replay menu +0x48 = ...`).
- **Getter conflict (s.4)**: 702B64 (cleared, used) vs 909218 (unlocked); the first scan logs both per mission.
- **Hidden duels (R-12)**: whether their scripts run cleanly from a replay is untested; rows stay `unverified`.
- **p0_vpad guesses**: the shrine interact button (B, then Y), the Battle Scroll position in the shrine menu (Down x3: item 3, assuming
  items 0-2 are visible), the replay list's default row, the number of confirms. Every step is log-verified and backs out with B on a miss;
  the item-menu Picture Scroll path needs a `-ScrollScript`. A Picture Scroll may be consumed (Q6), so that step is opt-in.
- Not built (later phases): E0/E1 routes, Drops = 0, UnlockEarly, per-cycle picker.

## 7. Merge notes (for the src owner)
New files: `src\rematch.cpp/.h`, `tests\rematch_harness.cpp`, `tests\run_rematch_harness.py`, `tools\rematch_catalog.py`,
`tools\rematch\p0_vpad.ps1`, `mod\Nioh3EngineFix\Nioh3EngineFix_bosses.tsv`. Hunks: `menu.cpp` (include; enum `kRematch` before
`kWorkerCap`; one `kItems` row after AutoContinueLoad; one `kOrder` row; Visible / Available / Value / Change / Move / B / remote / Close;
DrawImpl: toast when closed, sub-page rows, help, footer), `dllmain.cpp` (include, MOD_VERSION 1.7.40, start block after bosslog, RM comment),
`build.bat` / `CMakeLists.txt` (rematch.cpp, catalogue copy), `Nioh3EngineFix.ini` (`[Rematch]` at the end), `patches.json` RM30-RM36
(+ regenerated `patches_gen.h`), `tests\test_patterns.py` (one block before the summary). If another branch also took 1.7.40, renumber.

## E1 straight-to-boss ("Fight now", add-on Nioh3BossRematch 1.1.0, 2026-10-09)
Built offline, **not run in game yet** (the game was running when it was built: nothing was installed). `[Rematch] FightNow = 0` by default.

**Why not the Emaki record patch.** The design's E1 (point one Emaki record at a story boss, call `EmakiMissionRequest`) cannot spawn a
story boss: under mission 0xCC96 `PlacementBuild` 1C1E470 walks only 0xCC96's own start-point/gadget table (record +0x6C) and keeps the
row equal to `EmakiRecordGet(byte)+0x10` (1C1E5AB..1C1E5E4); 0xCC96's enemy table (+0x54) is empty and story bosses live in their own
mission's +0x54 table, which 0xCC96 never loads (`rematch-all-bosses.md` s.3, re-checked in the disassembly). So E1 is built on the
mission start point instead, which needs no patch at all.

**Mechanism (static, v2.02).**
- `MissionRequestById(mission, slotByte, startPoint, params)` 2239600: the third argument is the request's +0xC start point; 0 means
  "the mission default" (`1C17D8C`: record +0x90, else row 0 of the start-point table). `MissionStartFromSave` passes
  `[[MissionMgr]+8]+14h` there (22372CB -> 22375A9): that is how Continue resumes a mission at the last shrine. `+28h` of the same state
  is the running mission id (22372C3). `Player::SetRetryPointIDAndSave` 23AB7C4 writes `+14h`, so `+14h` = the retry point.
- Start-point table = mission record +0x6C, 0xA4-byte rows: +0 id, +8 flags, +0xC gadget kind (0 none, 906/907/909 shrines, 908 other
  gadget), +0x10.. gadget transform, **+0x40 spawn x y z yaw**. Boss position = enemy table (+0x54, 0x138-byte rows) +0x40. Boss-area
  PlacementBoxes = +0x5C table (0xA0 rows, +0x10 centre, +0x20 yaw, +0x28 full size, name pool). Index:
  `py -I tools\rematch_catalog.py --fight-index` -> `notes\rematch-fight-index.csv` (299 start points, 36 boss boxes, 3152 boss records).
- Choice (`pick_fight_start`): one start point = the mission already starts at the fight (high); flags 0x200 = post-boss / script retry
  points, never used; flags 0x40 within 40 m = the boss retry point class (Jakotsu-baba's 77032 sits 0.6 m outside BOSS_AREA, facing the
  boss) = high; a spawn/gadget point 8-40 m or a shrine 8-60 m = medium; anything else within 120 m (incl. < 8 m, probably inside the
  arena) = low; farther = not offered. Result: **68 rows: 35 high, 23 medium, 10 low**; 53 rows none (E, X and 4 Novice rows without a
  boss placement). Catalogue columns: `stage_mission` = start point, `warp_x..warp_yaw` = its spawn transform, `box_id` = boss box,
  `region_byte`, `fight_kind`, `fight_dist_m`, `fight_conf`.

**Add-on.** Each beaten R / H / D row gets a line `Fight now (straight to the boss)  <kind>, <n> m  [conf]` (A, then A to confirm;
pipe `fight <id>`). Game thread: the usual gates (solo/offline, no co-op, not loading/paused, no queue change in 10 s, not in a scroll,
not in a boss fight, nothing active) + cleared on this journey, then `MissionRequestById(id, slot, start, params)`; the queued slot is
checked (`FightSlotOk`: +4 mission, +0xC start point, +0x14 mode 2). Tracker: after the load it logs `F landed: retry point (+14h) X
== the requested start point`, counts reloads inside the mission (death / retry come back to the same start point because +14h is that
point), and ends when `+28h` stops being the mission for 5 s (falls back to the R-b rule if `+28h` did not read the mission id). New
site group FNOW (3 code checks proving the +0xC argument, the +0xC store and the +28h / +14h state fields; 83 sites OK).
Log lines: `grep "Rematch: F"`.

**What is written.** The add-on writes nothing (no record, no flag, no save). The game may save inside the mission exactly as in its own
replay of a cleared mission: the retry point (+14h, via `SetRetryPointIDAndSave` or a shrine rest), inventory / amrita / playtime, and
repeat (idempotent) defeat / clear flags (session 2026-10-08: flag 118 + clear 52 on the replay). Picture Scroll (E rows) is unchanged:
there the carrier scroll's clear / unique-defeat flag is the only scroll state written (carrier already cleared).

**Test first (solo, offline, save backup; F7 -> the boss row -> the Fight now line below it):**
1. **Jakotsu-baba** (Crucible Manifested, 0x34): boss retry point 77032, 23 m, high. The mission is confirmed by the 2026-10-08 log.
2. **Ryomen Sukuna** (The Apparition of Ryotan Temple, 0x1352): boss retry point 112646, 17 m, high.
3. **Yamagata Masakage** (Escape from Hitokoto Slope, 0x1CBC): the shrine before the boss, 67064, 35 m, medium (a shrine = the most
   ordinary resume point; walk ~35 m through the gate at x ~337900).
Pass = you load next to the arena, the log shows `F landed ... == the requested start point`, the boss fight starts, a death puts you
back at the same point, and leaving logs `Fight now over`. Report a fail with the `Rematch: F` lines.

### E1 v2: "Fight now (in the arena)" + "Shrine before the boss" (add-on 1.2.0, 2026-10-09, built, not run)
First game test of 1.1.0 (19:51, Yamagata Masakage): loaded at shrine 67064, walked in; trace = `SetMissionFlag 456` (42.06 s) ->
`Character::Create 65092` (42.36) -> `BossBattleMode` (42.78). The 0x1CBC script tests `Player::IsEnterPlacementBox(66599
CH02_MAIN01_HITOKOTO_BOSSAREABOX)` and then sets flag 456: **the arena trigger is that box**.
- **Warp = the game's own path.** Script native `Player::SetPos` 23AB4DC (x, y, z, yaw deg) ends with: `CharacterSetPositionYaw(ch, pos,
  yaw)` 1C27010, the physics vcall `[[ch+3B0h]+28h|18h]->+D0h`, actor manager `[[45B9E40]+48h]+D0h = ch+110h` + 42A9B4, 1C2A3CC(ch+110h),
  scene cfg `[45B9E58]` +10B0h = pos / +10C0h = 1 / 3A083C(+500h, +10B0h), `StagePathInitStep(+500h, 1)` 3B35B4. The add-on replays that
  tail with its own values (`CallWarp`); ch = `PlayerCharacterGet(0)` = `[[4745348]+0xCD*0x18]`; position = ch+110h. Site group WARP
  (2 checks of 80 bytes over 23AB658 / 23AB6A7; call targets read from their rel32s, the actor-manager global must equal g:CharMgr).
- **Arena point** (catalogue `arena_x..arena_yaw`, `arena_box`, `arena_src`, `arena_conf`; `--fight-index` adds `trig` rows = boxes the
  mission script tests with Player::IsEnter* / Wait_Invade* / Loop_*): the trigger box with BOSS in its name <= 40 m from the boss
  (medium; high when seen in a trace), else a BOSS area box of the table (low). Point = from the box centre towards the start point,
  60 % of the way to the edge (inside under either size convention), y = start/boss heights interpolated + 1 m, yaw toward the boss.
  Yamagata: 337691 10244 206714 (2 m inside the gate edge x 337898), high.
- **Rows:** 16 with an arena point (Yamagata high; Takeda Shingen, Hino-enma, Ippon-Datara, Magatsu Warrior, Ibaraki Doji, Kiyomizu
  (Hijikata/Kondo/Onmoraki), Scion of the Gods (Okita/Yoshinobu) medium; Jakotsu-baba, Yoritomo, Kunimatsu (Light of Hope), Bakumatsu
  wave 0x6351... low) + 30 that already start at the fight (duels, rushes, teacher missions: no warp). The rest: "Fight now" greyed
  with "no arena position yet"; "Shrine before the boss" stays available.
- **Flow:** `F landed` -> 1.5 s -> warp (only if not loading / paused, player present, no fight running, mission +28h and retry point +14h
  still the Fight now ones, solo). After every reload inside the mission (death) it warps again 4.5 s after the load. Log: `Rematch: F
  ... warp #n into the arena ... player a b c -> x y z`. No flag, no spawn, no save by the add-on: the game's trigger does the rest.
- **Capture:** every BossBattleMode start logs `Rematch: arena: ... player x y z, mission, retry point, placements`. `[Rematch]
  ArenaCapture = 1` appends `mission placements x y z when` to `Nioh3BossRematch_arenas.tsv`; at launch a capture overrides the
  catalogue point of the row whose boss placement is in its list (conf `captured`, y + 0.5 m). So playing a boss once fills its row.
- ini: `FightNowWarp = 0`, `ArenaCapture = 0` (repo defaults); pipe `fight <id>` (arena) / `shrine <id>`. Harness 54 checks, 85 sites OK.
- Open: the warp itself (physics / camera after `CallWarp`), the arena y on uneven floors, the yaw convention (0 = +z assumed).

### E1 v2.1 (add-on 1.2.1): the 1.2.0 test, diagnosed (2026-10-09 20:08, Yamagata Masakage)
- **(a) Exception = our bug, not the game's warp.** Player::SetPos reads the actor manager with two loads (`mov rax,[rip+45B9E40]` 23AB685,
  `mov rcx,[rax]` 23AB693); 1.2.0 used one, so it wrote 16 bytes at the wrong object's `[+48h]+D0h` and called A429B4 on it -> exception
  after `CharacterSetPositionYaw` had already moved the player (log: player -> target). Fixed (`**g:CharMgr`); the log now names the
  failing step. Not a timing / thread / nav-mesh problem: the pump is the game thread and the position write itself succeeded.
- **(b) The "reload" was the game's boss intro, not our warp.** Order: warp 15.690 -> `SetMissionFlag 456` 16.788 (= the BOSSAREABOX
  trigger fired from the warp point) -> loading flag 16.795 -> Create 65092 17.124 -> BossBattleMode 17.518 with the player at
  334650 10100 206713, 15 m behind the boss's placement. The script's `HITOKOTO_RTM_RELOAD` box (centre 337537, 104 x 13 x 38 m) covers
  the arena: the trigger plays an intro (RTM) with a load and places the player at the fight start. A death / out-of-bounds would
  not set 456 first, and the fight then ran 24.1 s normally.
- **(c) Keep the trigger warp; do not warp to 334650.** 334650 is outside BOSSAREABOX (x 336834..337898), so a warp there skips the
  trigger, and the fight (flag 456, Create, BossBattleMode) would not start. The capture now records the position on the last frame
  before the intro load (`src before-load` = at the trigger) or the current one when no load preceded the fight (`fight-start`);
  1.2.0 lines (no src column, post-intro position) are ignored.
- Warp now 1 s after the load (was 3 s settle + 1.5 s): shrine visible ~1 s, then the trigger -> the game's intro -> the fight.

### E1 v3 (add-on 1.3.x): self-tested on the PC; boss gates, duels, state-based timing (2026-10-09 21:05-23:30)
**Self-test** `tools\rematch\fightnow_test.ps1 -OwnerPid <pid> [-IncludeAtFight] [-Rows ...]`: waits until Nioh3 is closed, takes the
bridge game lock `nioh3`, copies the save folder to `captures\save-backups\<stamp>-fightnow-test`, installs the build + test inis
(and a VirtualPad perf ASI: the user's 1.7.32 has none), launches (Steam cloud dialog via steam_cloud_click), title -> Continue
by VirtualPad, then per row: `row <id>` (uncleared on this journey = `skip`, never started) -> `fight <id>` -> status `fnow=pass|fail`
-> `exit` (MissionRequestReturn = the mission menu's leave) -> next. At the end it closes the game, restores the inis + perf ASI and
the save folder (robocopy /MIR, verified byte for byte) and releases the lock. CSV `captures\rematch\fightnow-<stamp>.csv` (id, result,
warps, load_s, seconds, player position). No-pipe variant for Linux / Proton: `[Rematch] SelfTest = arena` (the add-on runs the rows
itself, `SELFTEST <id> PASS|FAIL|SKIP`, `SELFTEST DONE`) wrapped by `tools\rematch\fightnow_selftest.py`.

**What starts a story boss (mission scripts, `--fight-index` kind `gate`).** The boss start function waits in
`Function::WaitWarpStart(gate)` = `Online::IsExclusiveRightsValue(slot, flag, mission, gate, value)`. 27 rows: the gate is a
**boss door** gadget (`door_state_change` -> `Function::StartWarpByGadgetRights`, 22 missions: Jakotsu-baba, Takeda Shingen,
Yoritomo, Kunimatsu, Okita/Yoshinobu, Kasha, ...) or an **arena box** (`sub_event_check_boss_area`: `Player::IsEnterPlacementBox` ->
`Function::StartWarp`, 5: Yamagata, Hino-enma, Ibaraki Doji, Magatsu, Kiyomizu). After it the script itself fades, `Player::SetPos`
into the arena, plays the intro (RTM), creates the boss and calls `BossBattleMode`. The area-box warp alone could never open a door.
**1.3.0:** the call 3B2323 (native -> check core 3B2354(slot, key)) is retargeted (site group GATE): while a Fight now runs, the
first polled key whose id is the row's gate is copied, and `RightsRequest 1CA52C4(slot, key)` requests exactly that key, as the door
interaction / StartWarp do (key seen: `01 .. .. .. <mission> <gate> 01`). Mission-local runtime state only.
**Duels** (master duels, teacher / hidden duels; `Function::DuelCommonFlow`): `WaitPlayerPrepareDuel` = the player within 8 m
(800 units), then the boss's awareness. Fuma Kotaro and Tomoe Gozen start 8-9 m away: 1.3.2 warps once to 4 m in front of the boss
(catalogue `boss_pos`) when no fight began 10 s after the landing. Kneeling masters (`IsKneelDownFlag`) would need a talk prompt;
none of the cleared ones on this save did.

**Timing is state-based** (the user's question about HDD / slow PCs): landing = no load + a player + mission state readable for 3 s
in a row (a load that pauses and resumes restarts that), then 1 s; the warp re-checks loading / pause / player and is retried next
frame if the player cannot be read; the gate request waits for the boss start's own poll (up to 30 s of play, loading and pause time
not counted); verification (PASS = a BossBattleMode after the warp / request) stops its clock while loading or paused (60 s after a
warp, 180 s after a gate request: Okita's unskipped intro takes ~60 s); a re-warp happens only when a load put the player back at the
start point and never during a gate intro; the request -> load timeout counts only "no load began in 120 s" (a running load may take
up to 20 min). The status reports `loadms` (request -> load end) and the CSV `load_s`, for the slow-load check on other machines.
Fixed delays left: the 3 s / 1 s settles after a load has finished, 1.5 s between the re-warp checks, the 10 s duel wait: all start
after the game reports the load done, so a slow disk only delays them.

**Results on this PC (save of the user, journey 1; 68 catalogue rows with a start point, 59 of them Fight-now capable):**
run 4 (1.3.0): 19 PASS, 1 FAIL (Solitary Blade: deadline shorter than its intro), 9 not cleared. Run 5 (1.3.1, all 59): 30 PASS,
3 FAIL (Solitary Blade, Fuma Kotaro, Tomoe Gozen), 26 not cleared. Run 6 (1.3.2): the 3 + Yamagata all PASS. Final run 7: below.
(Run 7, 1.3.3: 33 PASS / 0 FAIL, per STATUS.md.)

### E1 v4 (add-on 1.4.0): in the arena from the first frame - start point spawn override (2026-10-10, branch `rematch-instant`)
User: "Fight now still sends you to the shrine first, then to the fight." In 1.3.x the load ends at the start point (often a shrine),
the add-on waits for a stable landing (3 s + 1 s) and then warps. 1.4.0 makes the game itself place the player in the arena.

**Options checked**
- (a) a start point in or next to each arena: **not available.** The start points are the mission's fixed `+0x6C` table (the only
  ids MissionRequestById / Continue accept); the catalogue already takes the closest one. Yamagata 0x1CBC has four (67064 shrine
  35 m = the one used, 67971, 67982, 68174), none inside BOSSAREABOX; the 16 arena rows all start 8-60 m away (2 within 5 m).
- (b) the arena position during the load: **chosen** (below). No hook needed: the game reads the start point's record at
  placement time, so an in-memory record override is enough.
- (c) keep the screen faded until the warp: not needed (fallback only if (b) had failed).

**RE (v2.02, `py -I` capstone over re\Nioh3.dump.exe).** Warp-point records = the point-set registry `[47098E0]+18h` (loaded
`+0x6C` tables, heap, writable), found by `WarpPointFind(id)` 137DCC (pure lookup).
- Player placement 1C0F24C (from 379620 when `[[45B9E40]]+26ABh`): id = mission state `[[47514D0]+8]+14h` (the retry point =
  our start point) -> 137DCC -> position 3B271C = record `+40h/+44h/+48h` (gadget-relative transform only with its flag argument
  and a gadget at `+0Ch`; the 1.3.3 landings sat exactly on `+40h`), yaw 3B2678 = `+4Ch` -> 669AB8(player, pos) + rotation -> ch+120h.
  (If `word [[[45B9E40]]+48h]+110h != 0` the position comes from DataMgr `+188h` instead: then the override does nothing and the
  warp fallback runs.)
- Stage streaming centre 223B8F0: `[45BC42C]` -> 137DCC -> 3B271C -> `WorldGridFindCellContainingPoint` (streams around the arena).
- Arrival camera 2239C3C: `+14h` -> record; when `+8 & 4` the camera gets the preset angles `+7Ch/+80h` (aimed for the shrine).
- Death / retry reloads use the same `+14h` path, so the override also respawns the player in the arena.

**Mechanism.** For a Fight now row that would warp (an arena point, `FightNowWarp = 1`, `[Rematch] FightNowSpawn = 1` default), the
add-on, on the game thread at the first tick of the mission load, finds the start point's record, checks it still spawns where the
catalogue says (warp_x..z within 1 m), saves it, and writes the arena point into `+40h..+4Fh` and clears bit 4 of `+8` (no
shrine camera preset). Checked once a second (every tick while loading) and re-applied if the game rewrote / reloaded it.
Restored in `ToIdle` (mission exit, timeouts, abort, any new request) only if the bytes are still ours; removed if an online
session appears. The start point id (what the game keeps in `+14h` and may save) never changes: the save holds the same id as in
1.3.3 and a plain Continue / replay spawns at the shrine again. The warp stays as the fallback: when the first frame after a load
still has the player within 5 m of the start point (override did not take), the 1.3.3 warp runs; otherwise no warp (the arena-box
trigger fires from the spawn, door rows still get the gate request). Gate-only rows (no arena point: 13) and the start-at-fight
rows are unchanged. Log: `Rematch: F <id>: spawn override applied (load began) ...`, `first frame after the load (#n in the
mission): ... in the arena`, `no warp - the player spawned in the arena`, `spawn override restored (...)`. Pipe status `spawn=`
(`-` / pending / applied / arena / start / failed); CSV column `spawn`; SELFTEST lines `spawn <v>`.

**Patch resilience.** New site group SPAWN (6 sites: fn:WarpPointFind, chk:WarpPointPos_40, chk:WarpPointYaw_4C,
chk:PlayerPlace_Retry14, chk:PlayerPlace_Find, chk:CamFlag4_Retry14) + run-time cross-checks (the placement calls WarpPointFind /
the pos / yaw getters; placement and camera read `+14h` of g:MissionMgr). Group fails -> `arena spawn ... OFF: <why>` and Fight
now behaves exactly as 1.3.3. Offline: `tests\test_sites.py` ALL 94 SITES OK (+ patch simulation), `tests\run_rematch_harness.py`
ALL PASSED (60 checks, incl. record match / flag / at-start tests on the Yamagata row; 14 of 16 arena rows spawn > 5 m from their
start point), build clean.

**1.4.0 one-boss check (PC, 2026-10-10 16:19-16:25, Yamagata Masakage R1CBC; `fightnow_test.ps1 -Rows R1CBC -AgentId pc-rematch
-PcLock nioh3-pc -SaveCopy D:\nioh3-re\save-backups`; locks `nioh3-pc` (queued) + `nioh3`; saves backed up to
`D:\nioh3-re\save-backups\20261010-161939-fightnow-test` and restored byte for byte (8 files); game closed normally): PASS.**
`spawn override applied (load began)` 0.6 s after the request (record loaded already: the point sets are resident) -> load 19.7 s ->
`first frame after the load: player 337042 10100 206691 - in the arena (0.0 m from the arena point, 26.5 m from the start point)`
(on the ground: y 10100) -> BOSSAREABOX fired by itself -> the game's intro load (6.5 s) -> BossBattleMode at 334650 -> `F PASS
(fight before the warp)`, **warps = 0**, request -> fight 24.5 s; `+14h` stayed 67064 (the shrine id) throughout; on leaving
`spawn override restored: record 67064 spawn 339660 10192 206266 yaw -146 flags 4`. Not seen: the screen (no screenshot, camera
untested) and a death reload (queued below).
Bug found and fixed in the same build: the arena capture (ArenaCapture = 1) took the post-intro spot 334650 as `fight-start`
(the intro load ran past its 5 s window) - outside BOSSAREABOX, it would have broken the row at the next launch. 1.4.0 now never
captures while the override is on; the bad line was removed from the game folder's `Nioh3BossRematch_arenas.tsv` (copy with it:
`D:\nioh3-wt\rematch-instant\captures\rematch\fightnow-20261010-161939\arenas_with_bad_line.tsv`). The test build left installed by
`-KeepBuild` is swapped for the fixed one by `D:\nioh3-wt\rematch-instant\captures\rematch\swap_asi_when_closed.ps1` once Nioh3 is closed (it was running for
another agent; log next to the script).

#### QUEUED (not run): full 1.4.0 test plan
Run only on a free PC turn (`nioh3-pc` lock, queued as its own bridge id) plus the exclusive `nioh3` lock (these runs write saves);
save folder backed up and restored exactly by the scripts, second copy in `D:\nioh3-re\save-backups`. Solo/offline. Exit gracefully.
1. **All rows, PC** (~60-90 min):
   `powershell -NoProfile -ExecutionPolicy Bypass -File tools\rematch\fightnow_test.ps1 -IncludeAtFight -AgentId pc-rematch -PcLock nioh3-pc -SaveCopy D:\nioh3-re\save-backups -PerfAsi <perf ASI with VirtualPad>`
   Pass = every row that passed in run 7 still PASS; CSV `spawn` = `arena` on the 14 arena rows > 5 m from their start point
   (Yamagata, Takeda Shingen, Hino-enma, Ippon-Datara, Magatsu Warrior, Ibaraki Doji, Kiyomizu x3, Scion of the Gods x2, Jakotsu-baba,
   Yoritomo, Kunimatsu ...), `start` + warps=1 allowed only on the 2 rows whose arena point is within 5 m of the start point,
   `-` on gate-only / start-at-fight / duel rows; warps = 0 on `arena` rows; no `spawn override NOT applied` / `RE-applied` lines
   (a RE-applied line = the game rewrites point sets during loads: note which rows).
2. **Kinds, by hand or pipe, one each** (log `Rematch: F`): arena box (Yamagata 0x1CBC, Hino-enma), door + arena point (Takeda Shingen,
   Jakotsu-baba 0x34), door without arena point (Kasha: unchanged 1.3.3 path), duel (Fuma Kotaro, Tomoe Gozen: duel approach warp
   still fires), start-at-fight (Hanzo RC362: no override). Check: first frame after the load already shows the arena (no shrine),
   the trigger / gate intro plays, BossBattleMode, no second warp during the intro.
3. **Death / retry** on Yamagata, Takeda Shingen and Fuma Kotaro: die in the fight -> `F load inside the mission #1` -> `first frame
   after the load (#1 ...): in the arena` -> the fight restarts (box trigger / gate) with no warp; Retry from the game-over menu
   the same; 3 deaths in a row (no drift, no `RE-applied` storm). If the script moved the retry point (`+14h` != start point) the
   respawn is the game's own boss retry point: fine, note it.
4. **Restore**: leave each mission (menu leave + `exit`) -> `Fight now over` + `spawn override restored (replay mission exit)`; then
   "Shrine before the boss" and the game's own Battle Scroll replay of the same mission start at the shrine again; quit to title
   during a Fight now -> Continue -> resumes at the shrine (nothing persistent). Abort / timeout paths log `restored`.
5. **Saves**: `py -I re\scripts_re\savediff.py` before/after one Fight now with a death: only the 1.3.3 kinds of writes (retry
   point = the start point id, playtime / inventory / amrita, repeat clear flags); no position or record data.
6. **Camera** (bit 4 cleared): screenshots on the first frame for 3 shrine-start rows (Yamagata, a door row, a spawn-point row):
   the camera sits behind the player facing the arena, not the shrine preset angle. If it looks wrong, try keeping bit 4 and writing
   `+7Ch/+80h` instead (convention unknown).
7. **A/B regression**: `[Rematch] FightNowSpawn = 0` on 5 rows = identical to 1.3.3 (warp after the landing, same PASS).
8. **Patch resilience**: `tests\test_sites.py` on the next game build; a SPAWN failure must log `arena spawn ... OFF: <why>` and leave
   Fight now on the 1.3.3 warp.
9. **Proton / Ally**: `[Rematch] SelfTest = arena` via `tools\rematch\fightnow_selftest.py` (no pipe) -> SELFTEST lines carry
   `spawn arena`; plus one manual Fight now on Yamagata watching the screen (HDD/SD load: `spawn override applied (load began)`
   must come before `first frame after the load`). Linux check rule as usual.
10. **Online gate**: Fight now stays refused online; a co-op session joining mid-Fight-now logs `spawn override removed`.
