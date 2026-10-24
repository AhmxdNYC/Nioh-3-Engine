# Live arena swap in Training mode (user #12340, open #6466 / #4430 / #5332)

2026-10-17, pc, offline study + build only (no game launch). Ask: once the player is INSIDE a Training boss arena, switch to another boss arena in real time with a fade/whiteout; the initial load is fine as it is.

Code: branch `pc/live-arena-swap` (worktree `C:/Users/Ahmad Hamza/nioh3-liveswap`), based on `pc/idlehold-wait-12099` (b776ddb, newest Training line). Built with `mod/Nioh3Training/build.sh` (zig cc), no new warnings. **Not tested in game yet.**

## 1. What already works (do not rebuild it)

| Piece | Where | Evidence |
|---|---|---|
| In-place ARENA swap: warp the player to the arena cell + CreateDynamic the boss, no mission load | `StartArena` / `ArenaTick` / `FightNowImpl` in `mod/Nioh3Training/src/training.c` | done.md: same-world click to visible ~4.1 s on pc (`pc/fnow-visible`, AreaWaitMs 500); pc2 confirmed the fight is inside the boss's own arena (#5332, Shingen + Hino-enma) |
| Fade transition around the swap | `FadeWant` / `FadeTick` (bridge 6519), D3D quad from EngineFix `Nioh3EngineFix_Fade`, GDI window fallback | black out 250 ms, swap behind black, fade in 400 ms after stage 5 + boss spawned + player released |
| Arena session survives the swap | `TrainAttachArena` (round 1 pose, NoDeath, retry stays in the arena) | Return gate / Fight again work in arena sessions |
| Boss cannot act under black | `BhTick` BOSSHOLD (AI off until visible + BossGraceAfterCoverMs 1500) | #9133 fix |
| Cross-world switch | `XwStart` / `XwTick` (WarpOtherWorld, CrossWorldEarlySwap=1) then the same in-place swap | pc 15.6 s click to visible; pc2 lsfade black 23.4 -> 8.5 s, control 15.5-17 s; Ally 26-40 s |
| Prewarm (#544) | `XwPrewarm`, CrossWorldPrewarm=2 | 2.2 s cover after a ~27 s menu dwell (pc2). User #8236: do not use menu prewarm as THE solution |
| Native white-out flag | `oworld <world> <start> 1` sets the WarpOtherWorldWhiteOut bytes (prm[4], prm[0xF]) | command only; the game's own cross-world white-out, not used by Fight now |
| Old-boss teardown + re-attach | `StartArena` despawns the previous swapped boss; `HomeCapture` keeps the original Return spot while a session with the same mission runs | read from code |

So the ARENA swap itself is a solved problem for **same-world** pairs (~4 s, most of it the cover hold). The 22-40 s black screen is the **cross-world** world switch (game load cover GAMESTATE[2] + mission sequencer), the same open problem as #9140 / open "cross-world load NOT solved".

## 2. Smallest design: 'Swap arena' = queued Fight now with a session hand-off

Nothing new native. One Training export + one menu row:

1. Player (inside arena A, boss alive or dead) picks **Swap arena -> boss B** in the hosted Boss Rematch / Training menu.
2. Training (game thread): hold boss A's AI (668D74), fade the overlay out in `[Fade] SwapColor` (white default).
3. Once opaque: `TrainClear` (idle hold, boss mode, BGM, HUD bar, despawn boss A), end leftover native boss mode (+26AA), reset the Return gate session, then call the existing `Nioh3Training_FightNow(B)`.
   - same world: in-place ARENA swap happens on the same frame (overlay already opaque), FadeTick holds until settled, fades in (from white). Expected ~2-4 s total.
   - other world: XwStart; overlay stays up through the world switch. Today that is 8.5-40 s depending on device/settings; the swap is correct but this gap is exactly what the user calls the problem.
4. When the screen is clear again the overlay colour returns to black so Fight now / Return keep their black look.

### What blocks it

| Blocker | Status |
|---|---|
| Rematch `Gates()` refuses every Fight now with **"in a boss fight"** while `fight::g_active` (native BossBattleMode) is on = always while the arena boss is alive (`mod/Nioh3BossRematch/src/rematch.cpp:374`) | Rematch side, **not done**: the new menu row must call `Nioh3Training_SwapArena` directly, not go through `ExecDirect`/`Gates` |
| Return gate state survived `TrainAttachArena` (`g_gate.session/up` never reset because `g_tr.active` never goes 0) -> arena B would get no gate | **fixed** in AsTick (gate session reset) |
| Old boss could keep attacking during the fade-out; F9 freeze would stall the swap | **fixed** (AI off + freeze released first) |
| Overlay is hard-coded black: EngineFix `BuildOverlay` draws `AddQuad(..., kSolid, alpha << 24)` | Training side **done** (asks optional `Nioh3EngineFix_FadeColor`); EngineFix export **not done**: ~10 lines in `mod/Nioh3EngineFix/src/osd.cpp` (store rgb, OR it into the quad colour) |
| **22-40 s black for cross-world swaps** (game load cover + sequencer states 5/6/8) | **open**, same as #9140. Same-world pairs avoid it. Options already measured: lsfade (LoadingScreenEndMs) 23 -> 8.5 s black; CrossWorldForceCells 20.4 -> 14.7 s; prewarm 2.2 s but the user rejected menu prewarm (#8236) |
| The old Return-gate gadget has no delete native and stays in arena A | accepted: player is warped away; arena B gets its own gate |
| KiBreakNoFinisher per-actor state (`g_nf`) for the deleted boss | assumed cleaned when the actor leaves the table (as on retry); check the log |

### Making cross-world feel real-time (later, not in this change)

The user explicitly asked for background preload "if possible" (#6466) but rejected menu prewarm as a load-skip (#8236). For **Training swaps only**, the honest fast path is: while the player fights in arena A, prewarm is not possible across worlds (one world resident; Ally RAM fails). So cross-world swaps stay at the cross-world cost until #9140 is solved. Recommendation: menu lists same-world arenas first ("instant"), other-world arenas marked "(loads)", and the swap uses the native WhiteOut flags (`prm[4]=1, prm[0xF]=1` in `XwStart`) so the game's own cover is white too.

## 3. Exact functions / files

Done on `pc/live-arena-swap` (`mod/Nioh3Training/src/training.c`):

- `g_as` state (next to `g_rh`), `AsRefuse`, `AsHoldOldBoss`, `AsTick` (after `GateTick`), exports `Nioh3Training_SwapArena(uint32_t mission)` (1 queued, 0 unknown row, -1 busy) and `Nioh3Training_SwapArenaBusy()`.
- `h_pump`: `AsTick()` after `RhTick()`, before `GateTick` / `XwTick` / `FadeTick` (so FadeTick swaps on the same frame).
- `Nioh3Training_ReturnInfo`: bit 8 (swap running) also set during a live swap.
- Fade colour: `g_fd_rgb`, `FadeColor()`, `p_osdColor` (optional `Nioh3EngineFix_FadeColor`), GDI fallback paints the colour (`FdProc` WM_ERASEBKGND, `FadeThread` repaint).
- `Exec`: command `aswap <mission|R-id>` in `Nioh3Training.cmd`.
- ini `[Fade] SwapColor = 0xFFFFFF` (`mod/Nioh3Training/Nioh3Training.ini`).

Still to do:

- `mod/Nioh3BossRematch/src/rematch.cpp`: menu row `kItSwapArena` (shown when `Nioh3Training_ReturnInfo()` says a session runs and `Nioh3Training_SwapArena` exists) opening the boss list; pick -> `Nioh3Training_SwapArena(r.missionId)`, toast the result, no `Gates()`/`ExecDirect`. Mark rows `CurWorld != row.world` as "(loads)".
- `mod/Nioh3EngineFix/src/osd.cpp`: `extern "C" int Nioh3EngineFix_FadeColor(int rgb)`; `BuildOverlay` uses `(alpha << 24) | rgb` (check the quad's channel order).
- Optional: `XwStart` takes a white flag for swaps (native WhiteOut bytes).

## 4. Test recipe (pc, Windows)

Standing rules: back up saves, take the game lock, install, launch via `tools/enter_game.ps1` (steam_autoplay clicker running before launch), restore afterwards. Not on the main PC while the human is on it.

Install: `C:/Users/Ahmad Hamza/nioh3-liveswap/mod/Nioh3Training/Nioh3Training.asi` + its ini over the game copy (back up the old asi/ini as `*.pre-liveswap.bak`). Keep the current EngineFix/Rematch (the swap fades through black until the EngineFix export exists).

Steps (commands go into `<game>/Nioh3Training.cmd`, read from `<game>/Nioh3Training.log`):

1. In world 0xB1: Fight now Yoritomo (R7B35) from the Rematch menu (or `fnow 0x7B35`). Wait until `ARENA ... CONTROL` and the boss fights you for 60 s (gameplay gate).
2. **Same-world swap, boss alive:** `aswap R2EE9` (Otoroshi, same world 0xB1).
3. Repeat back: `aswap R7B35`. Then once more after killing the boss (boss dead, Return gate up) and once after losing a round (TrainLost, gate up).
4. **Cross-world swap:** from Yoritomo `aswap R15FF` (Shingen, world 0x96). Record the black time.
5. Refusal checks: `aswap R7B35` twice in quick succession (second -> `-1 busy`); `aswap` during a Return (refused); `aswap 0x1CBC` (Yamagata, NativeMissions-listed -> refused, old round kept).
6. After each swap: pick Return to where I was from the gate/menu -> back at the original open-world spot (not arena A).

Pass criteria (log + screenshot / fcap):

- `SWAP arena -> ... fading out` then `SWAP arena: covered ... old round cleared, Fight now ... -> 1` then `ARENA ... CONTROL` then `SWAP arena done: screen clear N ms`.
- Same-world: N <= 5000 ms; boss B visible, attacks within ~10 s of the screen clearing; no second boss / old boss left (`reg` command shows one boss); boss HP bar belongs to B; BGM is B's.
- No damage taken during the fade (BOSSHOLD lines; old boss `AI off for the fade-out`).
- Return gate appears in arena B after B is beaten (`GATE ... arena session`), not in arena A.
- Return goes to the ORIGINAL spot (`RETURN home stored` logged only once, before the first Fight now).
- Cross-world: no crash, correct boss/arena; black time recorded (expected 8.5-40 s = the open #9140 cost, not a fail of this feature).
- No exceptions (`SWAP arena tick exception`, `FADE tick exception`) and the game keeps running 60 s after the last swap.

Fail -> collect `Nioh3Training.log`, `Nioh3BossRematch.log`, fcap, and note which step.
