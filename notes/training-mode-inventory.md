# Training mode: bounded inventory and isolated prototype plan

2026-10-11, Codex; read-only source/data inspection at main parent7977cb9. No live implementation, build, game test or performance claim. Rendering safety remains the next implementation priority.

## What can be reused

| Evidence inspected | Available mechanism | Limit |
|---|---|---|
| `mod/Nioh3BossRematch/sites.json`, `src/rematch.cpp:1367` | CORE/FIGHT/PUMP required before enabling; 88 tracked sites; game-thread pump at0x56529C | Older addon note's80-site count is stale; resolution is not runtime validation |
| `src/rematch.cpp:569` | Cleared-mission request, current journey slot, chosen start point, free queue check and accepted-request verification | Loads a mission; no seamless arena swap or in-place reset proof |
| `src/rematch_core.h:20` | Catalogue routes, arena/trigger/gate/start metadata and per-run verification override | Geometry confidence and catalogue verified tags are not training validation |
| `src/rematch.cpp:1165` | EXIT group gates MissionRequestReturn, refuses online/co-op/loading | Exit request is not itself proof of exit completion or reward suppression |
| `src/rematch.cpp:445,1012` | Emaki map snapshot/restore tracking and online warning | Redirected scroll can become online; warning alone cannot make training safe |
| `src/bosslog.cpp:104,850`, `sites.json` | Character::Create hook records placement ID and extra arguments | Observation hook, not a callable arbitrary-spawn API |

Comment-filtered TSV inventory: **121 rows**, R59/H3/D10/E0 20/X29; **59 catalogue verified=1**,62=0. No `_verified.tsv` runtime results were inspected or asserted. Candidate **R1CBC**, Yamagata Masakage: mission0x1CBC, placement65092, start67064, high-confidence box arena, seen=`-`. Alternative **RC362**, Hattori Hanzo (The Demon's Regimen): mission0xC362, placement1102178, start1107817, high-confidence arena, seen=`-`. These are candidates for observation, not newly validated fights. **65092 is a placement ID, never an executable RVA.** Prefer a single supported actor in its native arena first.

## Symbol and export gaps

- `re/symbols/symbols.tsv` maps0x1180740 to PlacementEnemyActivate: finds an existing placement/actor and initializes it. The add-on resolves this as a FIGHT hook, not a `fn:` spawn entry. Full calling contract, actor ownership, asset streaming and cleanup remain unproven.
- Existing names for Character::CreateDirect/CreateDynamic at0x238C9BC/0x238CD5C are script VM wrappers. Names do not prove native signatures or suitability for calls outside a script context.
- `notes/boss-arenas.md` suggests ResetReceivedDead0x1180940 then Create for rearming. No ResetReceivedDead/despawn callable site was found in the tracked add-on sites. The exact static decomp paths `D:/nioh3-re/decomp/0x1180740.c` and `0x1180940.c` were absent; this inspection did not regenerate them or establish reset semantics.
- The inspected add-on `src/main.cpp` exports only Nioh3BossRematch_Version. No training/spawn/reset C export was identified there. Use the existing request/pump architecture inside an isolated add-on branch, rather than assume an external ABI.
- Reward natives are BOSSLOG observations, not prevention hooks. Story/save writes, loot, achievements, boss-script completion and autosaves need separate policy and proof. Save backups do not establish that an operation is harmless.

## Staged prototype and acceptance evidence

1. **Read-only observer first**, isolated BossRematch branch, training off by default. Inventory current mission, placement IDs, actor lifetime, queue state, arena gates and offline/session state on the existing pump. Select one catalogue candidate only after its replay/arena is runtime verified on an eligible cleared save. UI/pipe posts requests; every game call stays on the pump.
2. **Known arena plus existing-fight restart**: reuse native mission reload/start-point route, not in-place flag clearing. Confirm the request is consumed, expected mission and actor appear once, loading completes, gate/AI/navmesh behave normally and exit completes. Prove retry/death/abort/transition cleanup and refusal on unknown build, missing groups, occupied queue or online state. Keep this an experimental replay/restart tool until save/story/reward policy is proven; do not label ordinary replay as a reward-free sandbox.
3. **One native-arena summon**, only after proving placement lookup, model/animation/AI asset readiness, full native ABI, actor ownership and despawn, gate/script ownership, navmesh compatibility, death/reset semantics and story/reward/autosave effects. Never synthesize a placement ID or call a VM wrapper as a native function. Prove exactly one live instance and exactly-once cleanup across death, reload, failure and online transition. If cleanup or persistence cannot be bounded, do not enable it.
4. Cross-stage or multiple-enemy selection stays a later research gate. Each enemy/arena pair requires its own loading and navigation evidence; metadata availability alone does not support all pairs.

Before any runtime stage: follow existing game lock/save/config/ASI backup and restoration workflow; check game availability on both devices, no Ghidra concurrently, close naturally and verify restoration. Tests must cover restart/exit and persistence, not just spawn visibility. No hardware/settings/visual changes. Training performance is not claimed; any later performance claim needs A/B evidence.

## Next steps

Return to rendering: read the experimental FO queue producers/flush/endFrame/drain call ordering narrowly and write an explicit epoch/publication/lifetime contract plus a viable repair order. Do not merge FO or attempt a queue-count-only repair while overflow, stream bounds and late producers remain unsafe. Existing startup suite passed3b89c04 and does not need repetition without changes. Main1.7.61/FO0 remains usable; remote smoke1767/1768 and conditional native FO1781 remain pending at last bridge1785, not freshly queried here.

Training follow-up after rendering work: isolate a read-only observer, resolve missing reset/cleanup/asset-loading contracts from static callers before implementing a summon, then obtain the documented player/runtime evidence. Full BossRematch validation remains a separate player gate.
