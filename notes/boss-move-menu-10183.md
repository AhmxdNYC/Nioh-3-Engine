# Training move switches (#10183)

Branch `pc2/move-toggle`, based on the Shingen idle-fallback fix `aa0e466`.
This branch predates the latest HP-0/gate fixes: integrate these commits into the
current fleet build before a player session. The two ASIs built here are an
isolated test pair, not a replacement for the current full build.

## Behavior

Boss Rematch root -> **Training: current boss moves**. The page follows the active
Fight now / summon Training session and displays only that character's entries.
The shared generated catalogue contains 202 boss/action pairs across 11 bosses;
181 switches are editable, while 21 initialization/idle entries (0 and 2) remain
ON to preserve the proven veto fallback. Yoritomo has 14 rows, 12 editable.
Unknown labels remain unlabelled, and observational names carry `(?)`.

A/Enter toggles, Left disables, Right enables. Changes apply to the next move
selection and save immediately in `Nioh3Training.ini`, e.g.:

```ini
[Moves.16102]
0x4E64=0
```

This disables Yoritomo's recorded 0x4E64 action only. `1` allows it. Unset entries
inherit the legacy `[Summon] MoveBlock` list (empty by default); explicit ON also
overrides that legacy list for this boss. Switching bosses never carries a menu
preference across characters sharing an action number.

The veto requires the controller to match the active Training session's
BossBattleMode actor. Other enemies, native bosses outside that session, unknown
owners and the player pass through. UI exports read published identity and atomic
preferences; hook installation remains on the game pump. Preferences remain
visible if hooks cannot validate, with a failure message in the menu footer.

## Proven offline on PC2

`py mod/Nioh3Training/tests/run_move_toggle_tests.py` builds an executable which
includes the actual `move_toggle.h` callbacks/API, with synthetic native selectors
and actor reads. All 202 entries pass: 181 disable/re-enable, selection fallback,
reroll, commit refusal, cross-boss isolation and ownership cases; 21 protected
entries reject disabling. It also tests actual Windows INI save/reload, save
failure, deferred hook installation and prologue mismatch. This is not a live
engine or animation test.

Both Training and BossRematch ASIs compile with Zig 0.16.0. Training has the existing
unused `g_noForceHide` warning; the first full Rematch build emitted the existing
`_InterlockedCompareExchange128` declaration warning. No game was launched,
installed into, stopped, read, locked or benchmarked by the companion worker.

## Live verification queued for the game owner

1. Merge into the current build, back up saves/config, acquire the game lock, then
   launch PC2 under the approved harness. Use infinite player health and ActionLog.
2. Yoritomo: 1 baseline run with all 12 editable moves ON; record a repeatable
   action (candidate 0x4E64, observed in the earlier catalogue). Then 1 run under
   the same conditions with that move OFF. Report duration, baseline action
   entries, blocked-arm entries, veto count and whether other actions continue.
   The catalogue has only 1 occurrence per Yoritomo action; zero baseline
   occurrences is inconclusive, not proof of veto.
3. Verify menu Left/Right and reopening retain the value. Switch to Shingen:
   0x4E64 should still be ON. Restart to check persistence. Restore after testing.
4. Ki-break: log normal HP>0 breaks with all moves ON; identify an attack/dash
   actually selected during the break from time-aligned KIBREAK + ACTLOG lines.
   Repeat with that ID OFF, report occurrences and continued recovery/stagger.
   Do not use idle or initialization as the blocked target.

**Ki-break verdict: unproven live.** The veto has no Ki/HP condition and should
apply if a Ki-break move traverses the same selection/commit callbacks. This
does not prove that every Ki-break move uses them or that forced-call refusal
is safe after earlier state mutations. Do not label simulated cases as gameplay
results or ship an HP-0-specific behavioral change on this evidence.

## Build commands (repository root)

```powershell
py tools/gen_boss_moves.py
py mod/Nioh3Training/tests/run_move_toggle_tests.py
# Run in mod/Nioh3Training:
py -m ziglang cc -target x86_64-windows-gnu -shared -O2 -fms-extensions -Wall src/training.c -o Nioh3Training.asi -lgdi32
# Run in mod/Nioh3BossRematch (create bin first):
py -m ziglang c++ -target x86_64-windows-gnu -shared -O2 -fms-extensions -std=c++17 -DUNICODE -D_UNICODE src/main.cpp src/sites.cpp src/hooks.cpp src/bosslog.cpp src/rematch.cpp src/training.cpp src/ui.cpp src/remote.cpp -o bin/Nioh3BossRematch.asi -lgdi32 -luser32
```

## Existing Yoritomo engine evidence reconciled (2026-10-17, task #10183)

PC2 game owner results #10276/#10356 already verify the second boss with the
legacy INI block list, not the new menu build. Independent recount of the local
`training.log` files on this task restart confirms:

| PC2 Windows, Yoritomo chr16102 | Blocking OFF | Blocking ON |
| --- | ---: | ---: |
| ACTLOG action entries | 198 | 126 |
| Starts of the six blocked actions | 35 | 0 |

Same reported settings: idle-ish player, AutoRetryOnDeath=1, idle fallback 2.
Blocked IDs: 4E60/4E62/4E6C/4E74/4E7B/4EA7; OFF counts respectively
4/4/9/6/7/5, ON counts all zero. Evidence lives in the main worktree under
`captures/rematch/xworldprobe-20261017-002849` (OFF) and
`captures/rematch/xworldprobe-20261017-010758` (ON). These are action-start
counts, not matched-duration performance rates. Tested code was
`pc/boss-atk-toggle` at `8b3c001`; this does not validate the menu integration.

The actual callback/offline suite passes again at `c098c77` (202 entries,
181 editable, 21 protected, plus startup/ownership/persistence/idle lifecycle).
Remaining owner checks: menu integration after the startup-isolation issue,
and time-aligned Ki-break attack/dash veto. Ki-break veto remains unproven.
No game process was touched by the companion worker.
