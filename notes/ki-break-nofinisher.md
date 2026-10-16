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
