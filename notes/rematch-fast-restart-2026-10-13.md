# Faster boss rematches: current boundary and next change

2026-10-13, static review of the installed source in `D:/nioh3-wt/mw-hosted-20261013` (Engine 1.7.73 / addon 1.4.21). No game inputs, native calls, binary edits or deployment. User reported defeating Takeda, losing to Yoritomo and returning to an empty arena in the matched WG/LK/MW session. That report is a retry defect, not evidence that a no-load restart already exists.

## What Fight now actually skips

`mod/Nioh3BossRematch/src/rematch.cpp:881` (`ExecDirect`) always calls the native mission request with the selected mission, journey slot and boss start point. The queue still drives native mission loading. Fight now changes the initial player placement to the arena, with a later warp fallback and native boss-door rights request where necessary. It eliminates the run through the mission, not its resource/script initialization.

The prior Ibaraki/Yamagata session recorded 19.612 / 16.297 seconds from request to control-ready. Those include load, native intro and readiness; they are not pure disk-loading measurements. A log saying “fight began before the warp” means the fallback warp was unnecessary. It does not mean mission loading was skipped.

First entry into a different arena requires its geometry, collision, placements, characters and mission script. Repeating in an already loaded arena is the plausible separate optimization: keep that arena while the native encounter is reset. There is no proven universal native API for that complete reset in the inspected source/notes.

## Concrete routes and their limits

| Route | What is established | What remains |
|---|---|---|
| Native mission request, RVA `2239600` | Existing working rematch entry; queue and arrival checks already implemented | Still loads/reinitializes the mission; requesting the same ID is not evidence of seamless reuse |
| Native death/retry plus boss gate | Existing retry stays in the owned rematch; `TrackReplay` detects loading and arms a warp; `GateTick` requests the genuine gate right | Current one-shot gate/verification state is initialized on initial entry but not similarly on retry. Repair that generation first; do not carry old key/readiness evidence into a new script lifetime |
| Existing placement activation, `1180740` | Uses a registered placement wrapper and native construction/resource path | Does not reset a live boss's HP, AI, script progress, arena gates or rewards; activation success is not actor readiness |
| Existing Yamagata entrance hold / Summon work | Can withhold the one fresh script entrance and later let the original script perform Create/intro | A first spawn under the original script, not repeated respawn after death/clear. Prior phase-2 hold milestone is not a live Summon or instant-reset acceptance |
| Battle Scroll/Emaki | Native boss-only placement filtering within its selected stage | Entry still uses a mission request and native load; not a generic no-load arena swap |

The stale proposal in `boss-fights.md` / `boss-arenas.md` to call `ResetReceivedDead(id)` then Create is superseded by `training-spawn-contract.md`. `1180940` takes a VM context, not a placement ID. Its downstream write clears one received-dead bit; it does not restore HP or rewind an encounter. Do not implement that recipe.

Existing placement activation polls native resource readiness through `669388` and constructs only when the existing wrapper has no live object. `669110` bypasses its allocation branch when that pointer is already present. Keeping or recreating an actor therefore cannot substitute for resetting the controlling script.

## Yoritomo is a concrete first target

The installed catalogue maps **R7B35**, mission `0x7B35`, to Yoritomo placement **375545**, boss start **374664**, box **371660**, door **62177372**. **RA4B4 is Gozuki / Tokugawa Kunimatsu**, not Yoritomo; the two reported failures must retain separate row identities even if they share the retry defect.

Existing `GateTick` (`rematch.cpp:1220`) refuses while `g_gateReq` remains true; initial `ExecDirect` resets it and the seen/key/slot state. The reload branch (`:1365`) currently only schedules a warp and continues `GateTick`/`FnVerify`. A new native gate poll must belong to the retry generation before requesting anything again. Clearing the booleans every frame would instead risk duplicate requests during the first gate intro.

Read-only disassembly of Yoritomo script `6D14B07C9687` also confirms the encounter is more than actor activation. `sub_event_boss_clear_check` releases door rights (`1180E`), checks player game-over (`1181E`, `1184C`), performs fade/control/load/cutscene work, invokes `Character::Create(375545)` (`118F4`) and `BossBattleMode(375545)` (`1194E`), then continues script creation/duel preparation. These are bytecode offsets, not executable RVAs or independently callable native entry points. The function name alone does not prove that it can be re-entered safely after defeat.

## Smallest implementable sequence

1. **Repair ordinary death retry first.** On a positively identified retry generation of the same owned encounter, invalidate previous fight/gate readiness and require a fresh native gate poll before one rights request. Preserve the spawn recovery ownership rules. Validate Yoritomo: enter, intentionally lose, restart fight, then win and return; repeat a second loss to catch a one-shot-only fix. This should repair the empty-arena bug but does not promise shorter native loads.
2. **Implement a row-specific restart only after proving the native script loop.** For R7B35, trace the VM coroutine's post-game-over continuation and its gate-right wait, actor lifetime, temp values and reset-owned script. Identify an actual native encounter re-entry boundary with its required state, rather than starting a duplicate VM coroutine or clearing persistent mission flags. If native retry destroys this owner, reuse cannot be asserted without an alternative owner-preserving reset.
3. **Measure time where it is actually spent.** Keep request, native loading start/end, new boss-mode start and control-ready timestamps separately for first entry versus same-row retry. This permits a real comparison if script/intro work can be shortened independently of resource loading. Do not shorten protection cover while the boss can attack and the player cannot control the character.

No generic arena-only load, instant respawn, arbitrary boss transfer or speed gain is claimed. Current investigation supports a focused retry repair now and an exact per-boss script-reset investigation next.

## Source trail

- Correct installed worktree: `mod/Nioh3BossRematch/src/rematch.cpp` (`ExecDirect`, `GateTick`, `TrackReplay`); catalogue `Nioh3BossRematch_bosses.tsv`.
- `notes/training-spawn-contract.md`: corrected ResetReceivedDead ABI and existing-placement lifecycle.
- `notes/training-yamagata-native-placement-summon.md`: one-shot native placement activation versus script ownership.
- `notes/training-yamagata-arena-preload.md`: destination-owned resource acquisition/readiness; script-owner teardown drains references, so no cross-mission asset lease is assumed.
- `notes/boss-rematch-paths.md`, retained `D:/nioh3-re/decomp/0x223af50.c`: native mission queue/load path.
- `re/scripts_re/lfm_disasm.py 6D14B07C9687 --calls`: current read-only script evidence above.
