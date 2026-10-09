# Nioh3BossRematch: the boss logger + rematch as a separate, patch-resilient add-on (2026-10-08, branch `rematch3`)

Built offline; **not run in game**. Add-on 1.0.0 (`mod\Nioh3BossRematch`), perf ASI 1.7.41 (boss code removed). Route list and the
evidence per boss: `notes\rematch-all-bosses.md`. Design background: `boss-rematch-design.md`; session data: `session-2026-10-08-gamedata.md`.

## 1. Why a separate ASI
A game patch that moves boss code must not break the perf ASI, and vice versa. So:
- `Nioh3BossRematch.asi` + `Nioh3BossRematch.ini` + `Nioh3BossRematch_bosses.tsv` (+ `_verified.tsv` from verify_all) next to the
  other ASI mods; own log `Nioh3BossRematch.log`; own menu (overlay window, F7 / hold View + D-pad Up 1 s); own pipe.
- Nioh3EngineFix 1.7.41 hooks **no** boss site: `bosslog.cpp`, `rematch.cpp`, their harnesses, patches.json groups BL / RM (67 sites) and
  the `Boss rematch...` menu row are gone. Old keys (`[Debug] BossLog / RematchLog / InvLog`, `[Rematch] Enable`) only log
  `moved to Nioh3BossRematch`. This also removes the double hook on MissionLoadStep / BossBattleMode.
- Coexistence: named mutex `Local\Nioh3BossRematch.<pid>` (a second copy installs nothing); export `Nioh3BossRematch_Version`. The only
  site both ASIs can touch is the per-frame call 56529C (the perf ASI's debug Tour, `[Debug] Tour`): the add-on chains onto whatever
  it calls; Tour refuses to install after the add-on (its byte check). Entry hooks already taken by another module (E9 + NOPs over
  the head) are chained, not refused. XInputGetState IAT: both swap the slot and always call the previous pointer.

## 2. Patch resilience (sites)
`tools\derive_sites.py` derives `sites.json` + `src\sites_gen.h` from the v2.02 dump: **80 sites, 31 by script-native name** (the
native table `{char* name, fn*}` is found at run time from the string `Mission::BossBattleMode`; name -> function; head bytes checked)
and **49 by wildcard pattern** (rip-relative displacements and call / jmp / jcc rel32 wildcarded; literal bytes keep struct offsets, so
a match also proves the offsets; window grown until unique). Globals (11) come from an anchor instruction's disp32. Known build
(timestamp + SizeOfImage) = fast path at the expected RVA; any other build = full scan, exactly-once rule. The add-on waits for
SteamStub to decrypt `.text` (3 sentinel patterns, `[General] DecryptWaitSeconds`).

Groups self-disable independently, with the first failing site in the log (`Sites <group> n/m resolved - first failure: ...`):
| group | sites | used by |
|---|---|---|
| PUMP | per-frame call 56529C | the rematch game-thread tick (MissionLoadStep only ticks during loads: session note s.5) |
| FIGHT | BossBattleMode, BossBattleModeEnd, Character::Create | fight gate, placements ring, verify_all |
| BOSSLOG | 27 (flags, gauge, clear, 23 reward natives, GameClearRewardGrant) | `[Debug] BossLog` |
| RLOG | 29 | `[Debug] RematchLog` (Phase 0 logger, unchanged behaviour) |
| INV | InventoryAddItem | `[Debug] InvLog` |
| CORE | 2 functions, 9 globals, 2 offset checks | every rematch route |
| RA | MenuOpenMissionReplay + MenuMgr | R-a |
| EMAKI | EmakiMissionRequest, 2 offset checks, Inventory | E routes |
The add-on never refuses to load: a failed group switches off its features only (e.g. EMAKI off -> R / H / D still work).

## 3. Routes (game thread only; solo / offline gates re-checked right before every call; no story-flag write)
- **R** (59): R-a = `MenuOpenMissionReplay(0, 0, 6)` (the game's list), or `[Rematch] DirectStart = 1` -> R-b =
  `MissionRequestById(id, [[MissionMgr]+8]+10h, 0, params)` with `alignas(16)` params (movaps at 2239660), only for a mission
  `MissionClearedGetSlot` reads cleared on this journey; success = the free slot holds the id with `+14` = 2.
- **H** (3 hidden duels): R-b directly (no patch needed: the request does not look at the list flag) or R-a + `HiddenDuels = 1`
  (bit 0x4000 cleared, restored on close / exit).
- **D** (10 rows: 8 master duels + 2 variants): R-b on the unlisted record, cleared-on-this-journey only. Unverified.
- **E** (20 Picture Scroll records): carrier = the first owned Picture Scroll item (ids 1E82 / 516D / E604 / DD82 / D523; key built as
  EmakiItemUse_MenuHandler path 0); every valid entry of the Emaki byte -> record map is pointed at the chosen record (snapshot + CRC32),
  then `EmakiMissionRequest(item+28h, key, 0, params)`; restored when charMgr `+2C18` stays 0 for 5 s (exit), or 30 s after a request
  that never loaded (timeout; loading time does not count). A retry inside the scroll keeps the redirect. A foreign write to the map is
  detected by the CRC and logged; the snapshot is restored anyway. `Use an owned Picture Scroll (the game's pick)` = the same call without
  the redirect. The record-patch E1 of the design is impossible for named bosses (all-bosses note s.3), so E covers the 20 records only.
- **X** (29): listed greyed with the reason.
- Per-row `verified`: catalogue column (Jakotsu-baba 0x34, from the session log; R rows are verified for R-a because the game's own list
  decides) OR `Nioh3BossRematch_verified.tsv` (verify_all). Unverified rows (H / D / E) need `ShowUnverified = 1`.

## 4. ini (`Nioh3BossRematch.ini`, defaults)
`[Rematch] Enable = 0`, `DirectStart = 0`, `HiddenDuels = 0`, `ShowAll = 1`, `ShowUnverified = 0`; `[Menu] Overlay = 1`, `Hotkey = F7`,
`PadGesture = 1`; `[Remote] Pipe = 0`; `[Debug] BossLog = RematchLog = InvLog = 0`. Menu: D-pad / arrows move (LB / RB or PgUp / PgDn
= 10 rows), A / Enter select + confirm, B / Backspace back; the pad is masked from the game while it is open. Rows:
`name  region  status(beaten / not beaten)  route  [unverified]`; footer = mission / route or the X reason.
Pipe `\\.\pipe\nioh3bossrematch` (local only): `ping`, `status`, `scan`, `row <id>`, `start <id>`, `abort`, `open`, `scroll`.

## 5. Offline proof
- `py -I mod\Nioh3BossRematch\tests\test_sites.py`: **ALL 80 SITES OK** (31 native, 49 pattern) + a patch simulation (16 bytes inserted
  mid-.text: the 18 sites after it re-found at +16 by pattern; globals' anchors re-found).
- `py -I mod\Nioh3BossRematch\tests\run_rematch_harness.py`: **ALL PASSED** (40 checks: catalogue 121 rows, verified merge, E1 redirect /
  CRC restore / foreign-write detection / range refusal, E lifecycle incl. retry and timeout, the real hooks.cpp entry thunk with two
  handlers + chaining onto another module's hook, the pump retarget); ini defaults; catalogue and sites in sync.
- `build.bat`: no warnings (/W4). Perf ASI: `test_patterns.py` **ALL 217 SITES OK** after the removal (was 284 with BL / RM); build OK.

## 6. verify_all (`tools\rematch\verify_all.ps1`, NOT run: the game lock belongs to others)
```
powershell -NoProfile -ExecutionPolicy Bypass -File tools\rematch\verify_all.ps1 -OwnerPid <agent pid> [-Routes R,H,D,E] [-Rows R0034,...]
     [-SkipVerified] [-FightWaitSec 90] [-AcceptStart] [-QuitScript quit.txt] [-MaxMinutes 360] [-DryRun]
```
Per row: scan -> status (R / H / D must read cleared, else `skip`) -> `start` -> `in mission` -> a BossBattleMode whose placements
include the expected id (E rows: any fight) -> leave (QuitScript to the title + Continue, else close + relaunch) -> results.tsv +
`Nioh3BossRematch_verified.tsv` (repo + game folder). Guard: owner pid, MaxMinutes, commit headroom before each launch, 2-crash limit,
game lock, every touched file restored, save folder copied first. `-DryRun` walks the success path with simulated replies (checked:
92 rows planned, 3-row walk passes). Estimate: ~3.5 min per row with a relaunch -> **~5.4 h for all 92 rows** (less with a QuitScript;
story missions whose boss sits at the end of the map end as `loaded, fight not reached` unless `-AcceptStart`).

## 7. Open
- Nothing ran in game: the per-frame pump at 56529C, the overlay on Proton / gamescope (a topmost GDI window may not be composited
  there: use the pipe), E key building from an inventory item (offsets from the F1F060 decompile), D starts, the hidden duels.
- Item consumption by E (Q6), and which yokai each Picture Scroll ends with (verify_all logs it).
- `tools\playsession` writes the logger keys into the game folder's `Nioh3BossRematch.ini` when it exists (backed up / restored).
- `tools\rematch\p0_vpad.ps1` is superseded (its perf-ini keys no longer exist).
