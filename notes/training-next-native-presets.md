# Next native practice presets: offline admission plan

2026-10-11. Plan only; no launch, admission grant, input, native call or modified source. The current hosted Yamagata session must finish and restore before the lead considers either preset. These are alternatives for a later bounded session, not authorization to run two sessions or an automatic switching loop.

## Frozen descriptors

Catalogue inspected: `D:/nioh3-wt/codex-settings-menu-v4/mod/Nioh3BossRematch/Nioh3BossRematch_bosses.tsv`, SHA256 `7c0c158df9b7c5bc0ce89b6b6fee3ee384c7db94bde892eb38ad78741aad221b`. Runtime status/current-journey eligibility is **unknown**; metadata verified1 is not a new PASS.

| Choice | Catalogue row | Native mission | Native start | Expected placement | Enemy-record scalar |
|---|---|---|---|---|---|
| First, simplest additional native encounter | RC362, The Demon's Regimen | 0xC362 /50018 | 1107817 | 1102178 | 131751 |
| Later compatible named-boss alternate arena | RE18C, The Way of the Ninja: Adept | 0xE18C /57740 | 5044637 | 5043775 | 339773 |

Both are routeR, native mission-start encounters with Hattori Hanzo in the catalogue. Neither needs an arena coordinate override to request its native start. They are different native encounter variants, not the same configured enemy moved between maps. RE18C is not yet an equally accepted fallback if RC362 fails.

## Exact request contract

Correction against frozen e9941b4: use **Fight now** for these already-at-fight rows, or existing `fight RC362` / `fight RE18C` in a separately authorized harness. `ExecDirect:892` and `CanShrine:1853` reject Shrine for atFight rows; the previous shrine instructions here are superseded. Fight dispatches `ExecDirect(row,s,t,true,true)`; atFight suppresses warp/spawn and gate injection (`:945,951,963`). Do not add a raw native call or pass a guessed journey.

The native call is `MissionRequestById(mission, currentJourneyByte, start, params)`, existing wrapper in candidate `rematch.cpp:395`. Params are16-byte-aligned `uint32_t[8] {0x101,0x1000000,0x10100,0,0,0,0,0}`; native reads24 bytes. `ExecDirect:879` first resolves owned spawn/map recovery, checks attempt admission, requires FightNow/FNOW/start availability, rechecks `ClearedNow` on the current journey, and reads the byte at `[[G_MS]+8]+0x10`. A catalogue region byte is **not** that journey byte.

Before issue require the existing strict session/read gates, no active/uncertain attempt, no boss fight/load/pause, zero warp/spawn and a stable queue/field. Capture the original mission/start tuple after12s continuously ready, resetting on identity/read/gate changes. Perform pre-scan row-generation capture, scan once, and require a newer clear-status2 row matching the exact descriptor. Keep FightNowWarp=0, FightNowSpawn=0, SelfTest empty and empty arena override data.

One queued reply is only submission. Require a fresh native slot witness in slots0..2: mission+4 equals selected mission, start+C equals selected start, mode+14 equals2. Queue/read/call ambiguity is indeterminate; no repeat. Then require observed loading, stable destination mission/start/player and zero warp/spawn. Native start can immediately precede an intro: a loading boss event is evidence retained for later control review, never a reason to close early or a control proof.

## Boss and retry acceptance

Correlate fresh BossBattleMode for the selected mission with its selected placement, then root confirms visible Hanzo, arena, camera/controller response, collision and ordinary AI/combat. Do not substitute a global F PASS, arbitrary position change or character-creation entry for these facts. Because both descriptors are already-at-fight starts, do **not** copy Yamagata's `infight=0` shrine predicate: a legitimate destination/retry can already have its native fight active.

One explicitly authorized native death/retry should produce a fresh load and return to the selected mission/start. Preserve any new boss event and obtain root confirmation of death/retry and restored control. If the script selects a different retry point, stop the exact-preset claim for review rather than rewriting it. Reward/progression/save effects remain native; this is practice, not a reward-free sandbox.

## Exact return and ownership

Generic native `exit`/existing leave action is the appropriate return for these **native at-fight** starts. The new **Return from practice** UI is Yamagata-specific and intentionally refuses them; do not bypass its ownership gate or use it as a generic return API.

Before one return, require the selected current mission/start, readable native EXIT/session gates and settled active attempt; the native wrapper remains authoritative. Capture a new log boundary, issue once, distinguish queue acknowledgement from `MissionRequestReturn(params,1) ... -> requested`, then require the later native `replay mission exited` log and idle/player/nonloading/gateok telemetry matching the **original entry tuple**. Original mission0 is allowed only when so bound. Any refusal, exception, quarantined state, replaced log prefix or timeout blocks another request. Restore saves, INIs and exact binary ownership through the existing lifecycle finalizer; unresolved recovery retains its record.

## Minimum tooling delta before a live test

Reuse current C72E binary staging/finalizer/owner guards and continuous-ready helper. Add a separately reviewed fixed-preset observation adapter with only these two exact descriptors; do not loosen the existing Yamagata-only runner or hosted derivative admission. The adapter must parameterize exact mission/start/placement in queue/battle/return evidence and allow native-at-fight readiness, while preserving the root control/death evidence stages and single request/return limits. New route admission binds its own candidate hashes/source lineage and prerequisite receipts; the current hosted Yamagata admission does not authorize either destination.

Offline fixtures should exercise both exact descriptors, wrong journey-source rejection by the native wrapper tests, swapped mission/start/placement witnesses, intro loading followed by active-fight control readiness, valid original mission0 return, return to the wrong tuple, explicit refusal and missing/late load. A first live RC362 result then determines whether RE18C is worth separately admitting. No independent enemy-spawn or asset-retention claim is made.
