# Yoritomo death/retry empty arena — 2026-10-13

Status: reproduced by user in installed addon 1.4.21; addon lifecycle defect confirmed by retained log and source. No source/binary/game mutation made in this investigation. Native encounter reset behavior remains unproven.

## Retained evidence

`D:/nioh3-re/manual-wg-lk-matched-20261013/reported-loss/Nioh3BossRematch.log`

- R7B35 / mission 0x7B35 / retry point 374664 / target placement 375545.
- 03:50:51.179 native mission request; 03:51:16.850 gate right 62177372 requested.
- 03:51:22.095 target Create; 03:51:22.622 BossBattleMode Begin; 03:51:23.180 control ready.
- User reports losing. 03:53:22.591 same-mission load; 03:53:26.520 player respawned at overridden arena position.
- 03:53:26.526 flag 464=0 is misleadingly labeled `in fight`; 03:53:31.021 reports `the fight is already on - no warp`.
- No second target Create, fresh BossBattleMode Begin, or BossBattleModeEnd appears in the retained post-death log. Absence of Create alone is not an actor census; the user directly reports an empty arena.

## Source-backed defect

Source baseline: `D:/nioh3-wt/mw-hosted-20261013` commit 592a888.

1. `bosslog.cpp:63-96` sets two addon active latches at Begin and clears them at End. These are historical hook state, not native encounter-state queries.
2. `rematch.cpp:1697-1708` clears the public latch only when mission identity changes. Same-mission death retains it.
3. `rematch.cpp:1365-1380` schedules a post-load warp but neither resets the verification epoch nor re-arms gate evidence. At `1077-1085` the retained active latch cancels that warp and falsely describes combat as current.
4. `rematch.cpp:1223` independently blocks GateTick with the old `g_gateReq=true`. Clearing the active latch alone still cannot restart this gate boss.
5. `g_fn` remains PASS from the first battle; `FnVerify` at1157 only works in Warped state. Existing PASS is not evidence of a successful retry.
6. `RightsCheckHook` at71-84 stores only the first matching key (`!g_gateSeen`). Simply resetting g_gateReq would reuse stale evidence unless gate observation is also epoch-scoped.
7. Spawn override intentionally remains active during same-mission reloads, placing the player inside the arena. That explains why the user respawns there while the addon fails to repeat the initial entry sequence.

This matches the earlier RA4B4 failure documented in `notes/handoff-codex-to-claude-2026-10-12.md` and the separate worktree `D:/nioh3-wt/ra4b4-retry-observer-20261012/notes/ra4b4-retry-observer.md`. It is not a CPU optimization regression specific to WG/LK/MW. We have not proven the exact native script state that prevents target recreation; addon latches alone must not be presented as full native reset proof.

## Smallest correct repair direction

Add a retry epoch to the existing owned rematch transaction. Existing anchored player-death reads can distinguish a confirmed death followed by reload from an intro load. After a confirmed death reload, invalidate historical PASS/active interpretation for that attempt, snapshot a fresh Begin counter, and discard the old operational gate key/request status. Preserve any fresh Begin occurring during loading. Re-arm once only after a fresh matching native gate wait/reset observation in the new epoch; preserve solo/ownership/native-request gates. Do not call native battle-end, clear story flags, or reuse the old gate key as a shortcut.

Native fresh-wait semantics remain the specific unknown to resolve. The existing default-off RetryTrace candidate is scoped to RA4B4 and is not present in this installed addon; it cannot be claimed to have captured this Yoritomo retry. Reuse its anchored read policy for R7B35 rather than writing a new generic observer framework.

An explicit user-facing `Restart rematch` using existing native return followed by a fresh rematch request is a practical fallback while same-mission reset is unverified; it costs a load and must wait for verified return before re-entry. It is not seamless arena reset.

Focused verification: confirmed death/reload, boss-intro load, Begin during loading, stale-key rejection, unknown player/mission state, and normal mission exit; then real Yoritomo defeat and retry with fresh target/combat/control evidence. A second boss death cycle checks generalization. No broad release sweep is required before that focused test.
