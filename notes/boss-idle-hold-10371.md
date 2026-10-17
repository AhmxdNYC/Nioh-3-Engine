# Boss idle pause (#10371)

Implemented on `pc2/move-toggle`, building on the 202-entry attack menu (#10183).
**Built and tested offline; persistent idle behavior is not live-proven.**
The branch baseline predates the latest HP-0/gate/debug-freeze builds. Integrate
the source changes into the current full build before owner validation; do not
overwrite the player's current full build with this isolated ASI pair.

## Controls and semantics

Boss Rematch -> Training: current boss moves -> **Pause boss at idle**.
A toggles, Right requests ON, Left requests OFF. Command/pipe equivalents:
`idlehold 1`, `idlehold 0`, `idlestat`. Exports:
`Nioh3Training_SetIdleHold(int on)` and `Nioh3Training_GetIdleHold()`.
Status values: 0 OFF, 1 awaiting safe idle, 2 held, -1 acquisition failed.
This is session-only and starts OFF; no INI preference is written.

ON binds to the current session/round and the exact boss actor/controller.
At an eligible native selection, a recorded ordinary action (ID >= 0x4E20)
is replaced with idle 0x2. This uses the existing selection hook, not a direct
action write or immediate force. Unknown and lower status/reaction IDs pass
through. The feature requires a recorded idle entry for that boss.

Once action 0x2 is observed with AI enabled, the feature calls the existing
boss-only AI-disable native (0x668D74), records ownership and reports ON.
It keeps AI off, with a 100 ms check for game re-enables. Hits can still play
native reactions; it does not freeze animation time or write boss/player position.
The player remains usable. Existing debug freeze is a separate control; its
hotkeys, flags and time scale are not changed by this source patch.

Acquisition excludes nonpositive/unknown Ki, Ki-break and finisher flags,
HP<=1/death, hidden loading screens and another loading/cover AI hold. If no
safe idle is reached in 10 seconds, the request fails without taking AI ownership.
OFF, Return, retry, a new session/round or boss replacement cancels the request.
Restoration calls AI-enable (0x66982C) only for the same live actor/controller
whose AI this feature disabled. A replacement or dead actor is not written;
a loading/cover owner keeps its AI hold. An already-disabled AI is never adopted.
Cleanup is wired before BossModeEnd/TrainClear and before replacing an arena
session. Epoch checks also stop queued requests leaking into another round.

## Offline evidence

`py mod/Nioh3Training/tests/run_move_toggle_tests.py` compiles and executes the
actual move and idle headers against synthetic native state. Both suites pass.
Assertions are explicitly enabled even in Zig's optimized build (which otherwise
defines NDEBUG). The original 202-entry suite was rerun with assertions active.

The idle suite covers acquisition and selection filtering, OFF and session-exit
restoration, same-boss round changes, replaced actors, loading-owner takeover,
already-disabled AI, timeout, HP floor, unreadable actor identity, native
re-enable, reaction pass-through, hook/disable failure, pre-despawn cleanup,
unrecorded bosses and player exclusion. It does not prove the native AI functions'
animation, movement or hit-response behavior. Both ASIs compile; Training retains
the existing unused `g_noForceHide` warning.

## Exact live checks for pc2/pc game owner

Use PC2, infinite player health, ActionLog=1 and normal boss AI. Keep individual
move switches ON for this comparison; keep all other gameplay settings identical.
Back up saves/config, acquire the game lock and use the standard launch harness.

1. **OFF baseline:** Shingen, `idlehold 0`, observe 30 seconds at attack range.
   Record actions/attacks and verify normal movement. A passive baseline is
   inconclusive; confirm the boss can act before testing ON.
2. **ON:** `idlehold 1` during an ordinary attack; observe up to 10 seconds.
   Expect `IDLEHOLD pending`, optional `transition ... -> idle 0x2`, then `held`.
   An in-flight attack may finish before ON. After `held`, observe 30 seconds:
   no new attacks or AI locomotion, idle animation visible, player can move/attack.
   Record starting/ending boss positions and unexpected action IDs. Test a hit:
   normal hit reaction is allowed, continued AI attacks are not.
3. **Release:** `idlehold 0`; expect one stop log and normal AI within the next
   pump. Observe 30 seconds and record resumed actions. Move toggles retain their
   prior preferences. Repeat ON/OFF once from the menu (Left/Right).
4. **Cleanup:** hold, Return, and Fight again; then hold and retry. The new
   session/round must start OFF and the next boss must act. A cover/loading hold
   must never be released by this control. Test request during Ki break: no forced
   idle until normal positive-Ki state resumes, otherwise timeout with no hold.
5. Repeat the basic OFF/ON/release checks on Yoritomo. Check existing debug freeze
   separately: it must still pause mid-animation and release normally, while idle
   pause alone never freezes player control. Restore config/saves afterwards.

Report device, boss, settings, OFF/ON durations, time to held, attack/movement
counts after held, OFF recovery, cleanup and debug-freeze verdicts. A synthetic
PASS or idle 0x2 in a log alone is not evidence of a stable visible idle stance.

## Clarification #10396
The requested pause is an idle stance with attacks paused, never a mid-attack
animation freeze. Existing implementation above follows that contract. Rechecked
both offline suites after the clarification: PASS (202 catalogue entries, 181
veto cases, 21 protected entries, idle lifecycle suite). Actual visible idle
validation remains pending with pc; offline success does not close that check.
