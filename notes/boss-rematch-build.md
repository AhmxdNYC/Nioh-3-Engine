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

## 6. Open, and why
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
