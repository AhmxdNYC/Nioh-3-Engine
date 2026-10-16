# KiBreakNoFinisher toggle (user #7465 / #7467, 2026-10-16)

Branch `pc/ki-break-nofinisher` (worktree `C:/Users/Ahmad Hamza/nioh3-kibreak2`), Nioh3Training. Builds on `notes/ki-break-finisher.md` (#7290 detector).

## What it does
- `[Summon] KiBreakNoFinisher = 0` (default): nothing installed, native behaviour (no hooks are patched unless the toggle is on at load or turned on at run time).
- `= 1`, for every boss with a boss HP/Ki bar (gauge rows) and the boss-mode boss:
  - Ki-broken boss: **no grapple** (LB "Grapple: press next to an enemy who is out of Ki"). The player keeps comboing; the break lasts its native 3.9 s.
  - While Ki-broken the boss **Ki bar is gold** (Ki row only, HP bar untouched).
  - **HP 0**: the boss survives at HP 1, is set Ki-broken, its gold Ki bar drains over `KiBreakHp0WindowMs` (2000) and it dies (native EnemyForceKill(WithItemDrop) per BossLoot) only after that long **without a hit**. Every hit refills the bar and restarts the window.
- `[Summon] KiBreakHp0WindowMs = 2000` (min 200).
- Run time: Training command `nofin 0|1 [ms]`; Boss Rematch root row **"No finisher (Ki break)"** (exports `Nioh3Training_GetNoFinisher` / `Nioh3Training_SetNoFinisher(on, persist)`, saves the ini key).

## How (RVAs, Nioh3 v2.02)
- **Grapple gate (the real one)**: the player ctrl's per-frame target update 2F6EC8 fills the grapple target slot `ctrl+0xB68` from FindGrappleTarget **867258** (27FA04 nearest-target search, range param 0x9A2D, filter 1174A60 = `!StateTest 9 && StateTest 0x23` on the candidate). Hook: the call at **2F7045** returns 0 when the target is a watched boss. Found with the `nofintrace` aid (StateTest 2E9948 callers on the boss ctrl around an LB grapple, live 03:13): CanGrapple A5F7F8 (the earlier guess) is never called, so the first version did not block anything (02:05 grid: grapple with the toggle on).
- Kept from the first version: CanGrapple A5F7F8, grab box 117A9EE, bit-31 box 117AC17 (finisher-only boxes). Counters stayed 0 in every test. The bit-28 box hook (117ABF7, boxes that skip broken victims) was removed: it would change non-finisher hits.
- **Gold Ki bar**: GoldQuad paints the Ki-row atlas strips 0.375/0.438/0.625 (HP-fill UVs) while a watched boss is Ki-broken or in its HP-0 window (commit 3b33033); the HP-0 drain is the Ki snapshot handed to KiRowWidget 235F9A4 (call 1D220E3).
- **HP 0**:
  - lethal-hit check 329C9F (cond 0x27 in 327DEC): for a watched boss the stub jumps to the native **SetHP-1 path 329D1A** (66C188(hp,1)), not the death-blow branch (that one sets bit 46 -> finisher-down state 2C, live 03:22).
  - damage apply 786C84's own cond 0x27 (787004, h_cond): a watched boss always takes the revive-to-1 branch (live 04:05: once its break ended a hit killed it natively here).
  - that revive leaves **ctrl+0xEA0 = 1 (death-blow pending)**: every normal hit whiffs while it is set (found with `nofinsnap`/`nofindiff`, live 03:49). NfTick clears it each frame of the window.
  - hits in the window are counted by the **hit probe 326E81** (326C94's victim StateTest 0x23, once per landed hit).
  - break at HP 0: **A4F098(ctrl)** (native break-window start: runtime bit 0x3A = state 0x23 + timer ctrl+0x1014), re-tried every 250 ms while not broken; plus once the script kneel-down (238E1BC body: ctrl+0x994 = 0x27, 668D74(actor), actor+0x91B = 1) - no visible kneel with the AI off.
  - no hit for `KiBreakHp0WindowMs` -> ForceKillActorEx (BossLoot), retries as before.

## Test evidence (live, pc, Yamagata summoned via Training `summon R1CBC`, boss AI off, scripted pad via tools/testmatrix/vpad.py)
Final Training ASI sha256 `7dff1590e6927453e7659cc56d6afacca8907a7a15f46c581f33f727da02c4ff` (tests 04:04-04:15 ran on `1b85852d...`, identical except the menu exports). Logs + images: `captures/kinofin7465/` (`final_run_0404-0416.log`, `grapple_trace_0313-0321.log`).
- (a) toggle on, break + LB + X (04:05): full 3891 ms break, 4 hits (4038 dmg), no grapple, grapple target blocked 391 frames; gold Ki row only (`F_a_on_bars.jpg`, `a_mixon_bars.png`).
- (b) toggle on, HP 600 then 8 X (04:08): HP 600 -> 1, window start, state 0x23 on, hits at +781 / +1797 / +2937 ms each restart the window, kill 2001 ms after the last hit (window 4937 ms) (`F_b_hits_*.jpg`, `hp0i_bars.jpg` shows the gold bar draining and refilling).
- (b) toggle on, one lethal hit and nothing else (04:12): Ki-broken for the whole window, kill exactly 2000 ms after the hit (`F_b_nohit_*.jpg`).
- (c) toggle off (`nofin 0`, 03:21 and 04:15): LB grapples (break ends at 516 / 531 ms, grapple damage 1850 / 3435), HP 0 = native death (`a_mixoff_grid.png`, `F_c_*.jpg`).

## Untested / caveats
- Boss AI was off in every test: not tested with the boss attacking (does the HP-0 break / kneel stop it? does the boss attack at HP 1?).
- Only Yamagata (summoned). Native mission bosses and Boss Rematch arenas not tested; multi-boss fights not tested.
- The HP-0 break: re-break after the native break ends is a best effort (A4F098 no-ops while bit 0x3A is still set); the gold bar and the window do not depend on it.
- At HP 0 the empty part of the Ki row shows light grey/white (native lost-Ki trail), not dark.
- Boss Rematch menu row: builds (Rematch ASI sha256 `1fc84dbf45dba2814d3611bbc21e2b886fec3bafb3acbb830ea0c8925ce9735d`) but not opened live (synthetic F7/F10/Esc did not open the menu); the installed Rematch ASI was restored.
- Gameplay with the toggle on outside the finisher: hits on a broken boss land as before (bit-28 hook removed); a lethal hit no longer kills instantly (the 2 s rule, by design).
- Test aids left in the build (inactive unless typed): `nofintrace`, `nofinsnap`/`nofindiff`, `nofinkneel`, `nofintest`, `nofinuv`, `nofinhide`, `nofindump`, `nofinstrips`, `kibreaknow`, `nearboss`.

## User play 2026-10-16 15:41 (boss AI on, Training 96FC29CD) and fix (branch pc/nofin-gold-kneel)
Log `Nioh3Training.log` 59247-59661 (game folder).
- **No gold**: the boss had Ki bar style 0 (`GOLDKI: first boss Ki row: charParam+0x80 = 1, native style 0`). The strips 0.375/0.438/0.625 were found on Yamagata, style 1 (`charParam+0x80 = 0, native style 1`, every 01:40-04:15 dev run). Style-0 Ki row (pc/jumpfix2 dump 23:30 `GOLDKI vtx`): frame 0.000, 0.500, 0.625, **Ki fill 0.688** (the strip GoldKiTest=5 painted gold, user #7394). Style 1 never draws 0.688 (all `nofindump` lines). Fix: strip list 0.375/0.438/0.625/**0.688**. The `GOLDKI diag ... gold elements 0` line is the old width learner (GoldKiBar/GoldKiLearn), a separate path that never matches by design; NoFinisher's gold is the strip path (`NOFIN gold Ki bar ON/off`, which did fire 31.589-35.649).
- The boss was never Ki-broken during the fight (Ki >= ~0.5; `KIBREAK start #1` only at HP 0), so gold could only show in the HP-0 window.
- **Finisher anyway**: HP-0 window start set A4F098 + the script kneel-down (ForceKneelDown 238E1BC). ~1 s later (after the hit reactions) the boss was in the finisher/down action `KIFINISHER state on ... 35=1 B0=0 2C=0 ... HP floor (death blow)` for 1078 ms; the grapple target hook had blocked 67 frames before that, so the finisher did not come through the grapple target slot. The kneel-down is the scripted death-blow kneel; with the AI off (all dev tests) it never played. Hit 6 (+2062 ms) ended state 35 and the break together (`KIBREAK end ... grapple yes`, `Ki recovered` at HP 1) = the finisher hit; A4F098 re-break then no-ops as before (known caveat; gold + window do not depend on it).
- Fix: kneel-down off by default (`[Summon] KiBreakHp0Kneel = 0`, run time `nofinkneel 1` to get it back).
- Not verified in game (user playing). To check next play: `KiBreakNoFinisher ... KiBreakHp0Kneel 0 ... Ki strips 4` at load; gold Ki row on a style-0 boss during a break / the HP-0 window; no `KIFINISHER state on` at HP 1. If a finisher still starts at HP 1 without the kneel, the next suspect is the death-blow prompt itself (cond 0x27 / runtime bit 101 = `1068 0000002000000000`, set at HP 0 in this play but not in the dev runs).

## User play 2026-10-16 16:09 (Yoritomo, combo build pc/combo-gate-gold-kneel) and fixes (#8827)
Log `Nioh3Training.log` ~60990-61180 (game folder).
- **Gold only at the end**: break #1 (16:09:11, 3875 ms, 13 hits) fired `NOFIN gold Ki bar ON`, but every auto-dumped quad said `kifill 0`. During a native break the Ki is 0 and the Ki row widget draws its *broken* layout (empty fill + pink layer, other strips), so there is no fill quad to tint. In the HP-0 window the boss was natively broken 18.4-22.6 s (no gold seen either) and NOT broken 22.6-24.6 s: that last part is the only time the normal fill (with our drain snapshot) was drawn and tinted = "yellow at the end". Fix (`KiBreakGoldUnbreak = 1`): while a watched boss's bar is gold, NfKiRow calls the native KiRowWidget with `broken = 0`, so the normal fill quad is drawn at the snapshot value (full during a break, draining in the HP-0 window) and GoldQuad tints it. Display only. Also the auto dump now logs only bar-shaped quads (w/h 20..150, 48 lines) so the next log shows the boss rows.
- **Boss vanished mid death animation**: the NOFIN kill (16:09:24.604) went to KillFadeNow = 2 s fade at the kill, delete 6 s later. Fix: every kill path waits `KillFadeDelayMs` (default 5000, capped by BodyFadeDelayMs; 0 = old behaviour) before the fade starts.
- **"Grapple buggy" / `KIBREAK end #2 ... grapple yes`**: `grapple yes` = the detector saw a finisher state during that break: `KIFINISHER state on 35=1 ... HP floor (death blow)` at 16:09:20.691 for 1921 ms, at HP 1, while the grapple-target hook had blocked 746 frames. So it is the death-blow finisher at the HP floor, not the LB grapple. Both user plays (15:41, 16:09) had runtime bit 101 (`bits1068 0000002000000000`) set from the HP-0 window on; no dev run had it. Fix (`KiBreakHp0ClearDeathBlow = 1`): clear ctrl+0x1068 bit 37 every pump frame of the window (logged `death-blow runtime bit 101 ... cleared`). NOT verified in game: if `KIFINISHER state on` still appears at HP 1 with the bit cleared, the finisher comes from another path.
- Run time aids: `nofinunbreak 0|1`, `nofin101 0|1`.

## Toggle matrix (#8827)
| Combination | Behaviour now |
|---|---|
| KiBreakNoFinisher 1 + DeathBlowRequired 1/0 | NoFinisher owns the HP floor: boss survives at HP 1, gold window, native kill after KiBreakHp0WindowMs without a hit (h_cond returns before the DeathBlowRequired logic). DeathBlowRequired is n/a; the Rematch menu shows it dim "(n/a: No finisher is ON)". |
| KiBreakNoFinisher 1 + CutsceneOnKill 1 | n/a (there is no finisher to end); dim in the menu. The death animation plays from the native kill; body fade waits KillFadeDelayMs. |
| KiBreakNoFinisher 1 + BossLoot 1/0 | kill = EnemyForceKill(WithItemDrop) per BossLoot (unchanged). |
| CutsceneOnKill 1 + DeathBlowRequired 0 | CutsceneOnKill only acts on the finisher kill (needs DeathBlowRequired 1); dim in the menu "(n/a: Final blow required is OFF)". |
| any kill path + BodyFade 1 | fade starts KillFadeDelayMs (5000, capped by BodyFadeDelayMs) after the kill; delete 6 s after the fade. BodyFade 0: body stays. |
| AutoRetryOnDeath 0 + ReturnGate 1 | defeat: boss removed, Return gate offers Return / Fight again. |
| AutoRetryOnDeath 0 + ReturnGate 0 | was a dead end (no gate, no respawn). Now a defeat resets the round as AutoRetryOnDeath 1 (log `player beaten, ReturnGate 0`). |
| AutoRetryOnDeath 1 + ReturnGate 1 | defeat resets the round; the gate still appears after a boss kill (idle session). |

## User play 2026-10-16 16:29-16:31 (Yoritomo, Training b77ded2f) and fixes (#8894)
Log `Nioh3Training.log` from line 61743 (launch 16:27:55).
- HP-0 window: gold worked with KiBreakGoldUnbreak (`NOFIN Ki row ... native broken layout replaced`, goldquad 881x12 v 0.625 / 0.688 `kifill 1`). Kill fade delay worked (kill 16:30:56.44, fade 16:31:01.44, delete +6 s).
- **Staggers before HP 0 were never detected**: the boss Ki read 0.00 at 16:29:52, 16:30:27-32 (5 s+), 16:30:47, but there was no `KIBREAK start` and no `NOFIN ... BROKEN`: state 0x23 and the exhausted fields stayed 0. So no gold and no extra block on those staggers. Fix: `Ki <= 0` counts as a stagger (outside the HP-0 window).
- **Finisher at HP 1 still happened** (`KIFINISHER state on 35=1 'HP floor'` 16:30:50.716, 3.7 s), with bit 101 set again at that moment, so clearing it is not enough. Static RE (re/Nioh3.dump.exe): the player target update 2F6EC8 fills five slots: ctrl+B50 <- 8671D4 (filter 1174A10 = StateTest 9 = DOWNED target), B68 <- 867258 (grapple, hooked), B80 <- 8670D4 (filter 1174C60), B98 <- 867034, BE0 <- 866F24 (filter 1174BD0). Fix: hook the 2F7015 call (8671D4) and drop a watched boss from the B50 'down' slot while its Ki bar is gold. Log: `NOFIN hook down target 2F7015 ... on`, the counter `down target N` in the NOFIN BROKEN/recovered lines. NOT verified in game.
- Not done: making the HP-0 state play the normal stagger animation instead of the HP-floor kneel (needs RE of the boss's HP-floor action; A4F098 only sets the break state).
