# Next playable training step: one native boss practice route

2026-10-11, Codex. Design/data review only; no game, source/build/install, bridge or hold changes. Candidate catalogue/source inspected at private `codex-settings-menu-v4` HEAD `6fe593b` while the lead integrates further safety work. Existing design/progress notes predate the current read-validity, transaction and recovery fixes; their actor/arena/persistence limits still apply.

## Concrete next outcome

After the lead's safety fixes and request-only pilot pass, test **one cleared Yamagata native replay with actual player control and combat**, using the existing `shrine R1CBC` action. This gives real repeatable native encounter access rather than another observer-only feature. Walk into the native arena and use the game's own boss script. The immediately useful training feature to expose afterward is a single allowlisted **Practice Yamagata ? native arena** action backed by this same transaction, with native replay rewards/save behavior stated. It is not an arbitrary enemy/arena combination or reward-free sandbox.

No new callable enemy factory is needed for that route. Do not turn the reserved Training Enable setting on and assume it grants this feature; the current product still exposes training observation separately. A future practice action must share the existing mutation owner, refuse duplicate/pending commands, and never race legacy rematch/self-test. Native retry can be exercised manually first. Automated restart must wait for the stricter intended-return and queue-consumption evidence, not just the operational Idle label.

## Exact selected encounter

Current `Nioh3BossRematch_bosses.tsv` SHA256: `7c0c158df9b7c5bc0ce89b6b6fee3ee384c7db94bde892eb38ad78741aad221b` (candidate source file, not an installed-file claim).

| Descriptor | Value |
|---|---|
| Row / native mission | R1CBC / 0x1CBC (7356), Escape from Hitokoto Slope |
| Encounter | Yamagata Masakage, placement65092 |
| Native start/retry | 67064, shrine approximately35m before arena |
| Start transform | 339660,10192,206266; yaw-145.5 |
| Native trigger | Box66599, CH02_MAIN01_HITOKOTO_BOSSAREABOX; **box**, not a door request |
| Candidate arena transform | 337691,10244,206714; yaw-89.2 |
| Box geometry record | 337366_10274_206788_-4.0_1064_569_1535 |
| Eligibility | Native current-journey clear status; catalogue verified1 is not live approval |

The descriptor remains a bound mission/placement/start/trigger tuple. Existing `ApplyArenas` can override the catalogue from a sidecar and adds50 to captured Y, so the lead must record the effective installed descriptor and sidecar provenance before any future direct-arena run. The source-file digest alone does not pin effective arena coordinates.

## Existing executable path

1. UI's Shrine before the boss, or existing pipe command `shrine R1CBC`, posts `kReqShrine`; pipe `queued` is submission only.
2. Game-thread Tick dispatches `ExecDirect(row,s,t,true,false)`. Native clear status is rechecked on the current journey; journey byte comes from mission-state+10h; queue has to be readable with an available native slot. Current quarantine and recovery obligations must be clear.
3. `CallRequestById` calls the resolved site `fn:MissionRequestById` (native RVA2239600) as `void(uint32_t mission,uint8_t journey,uint32_t start,const uint32_t* params)`, with mission0x1CBC/start67064 and 16-byte-aligned `{0x101,0x1000000,0x10100,0,0,0,0,0}`. The native reads24 parameter bytes. Do not bypass the wrapper or pass a VM context.
4. Require the postcall native queue witness mission+4=0x1CBC, start+C=67064, mode+14=2; then valid observed load and stable mission/retry/player state. A native exception/read failure/wrong explicit start is indeterminate, not cancellation/retry permission.
5. With shrine action and FightNowWarp/Spawn disabled for the first live scenario, no arena point patch or player warp is requested. The player walks into box66599; native script sets flag456, activates placement65092, handles intro/gate and calls BossBattleMode. That exact chronological chain is recorded in `boss-rematch-build.md:150` from a previous run; it is a target to revalidate, not a new PASS.
6. Explicit `exit` uses `fn:MissionRequestReturn` RVA22396D8 through `CallReturn`, with the same aligned params and byte1. The native chooses saved/fallback return fields; this is not a chosen training-lobby API. Wait for its operational exit/cleanup and do not auto-submit another practice attempt while uncertain.

Relevant existing groups are CORE/PUMP/FIGHT/FNOW and EXIT; the first shrine/manual-entry scenario deliberately does not depend on enabling WARP/SPAWN or synthetic gate activation. All sites still require the installed-build resolver gate. The user's normal game controls perform walking/combat; existing Create-entry and fight logs correlate evidence without proving live actor ownership by themselves.

## Smallest live acceptance scenario for the lead

Use the lead's existing device-local locks, current-entry save backup, exact installed hashes and recovery guard. Once the current safety candidate is reviewed/built and the independent request gate passes:

- Verify current-journey R1CBC is cleared; otherwise refuse and report that exact eligibility block, with no unlock bypass.
- Keep SelfTest off; select one shrine request with spawn/warp off. Record submission, exact queue witness, first controllable frame, mission/retry, elapsed loading, and visible shrine/terrain/camera.
- Walk into the native box. Capture visible Yamagata identity and one visible target, native intro/gate, collision/navigation and basic AI/combat while correlating placement65092 plus a fresh fight event. A global F PASS or distance-from-shrine label alone is insufficient.
- Perform **one** native death/retry if control/recovery remain valid, verify return to67064 and re-entry through the normal trigger. Then request one native return and verify departure plus recovery; close gracefully and finish the existing save/installed-state receipts.

This small scenario establishes a candidate for usable native practice. Three death/retry cycles, defeat then full reload, title/Continue, camera, interrupted/late-load paths and ordinary shrine replay after restoration remain the broader acceptance matrix before advertising support. The request-only C72E pilot is useful admission evidence but not a substitute: RC72E is a multi-boss shrine/door route with no arena coordinate in the current catalogue.

## Direct arena and other enemy choices

Only after shrine/manual-entry evidence should the lead re-test `fight R1CBC` with the existing v4 spawn override. Require effective coordinates inside the actual trigger/ground, first controllable arrival, visible camera/collision, zero fallback warps if claiming arena-first, then restoration. Historical 2026-10-10 v1.4.0 evidence reports zero warps and24.5s request-to-fight, but did not capture the screen or test death/reload. Its observed coordinate337042,10100,206691 differs from the current catalogue, so that PASS cannot certify today's descriptor.

RC362 Hattori Hanzo is a later separate mission-native candidate (mission0xC362, placement1102178, start1107817, starts near the fight). C72E's Enenra/Great Tengu/Jakotsu-baba sequence is not a single selectable enemy; a mission row does not prove arbitrary phase selection.

Arbitrary Spawn/Despawn/Revive stays unavailable. `Character::Create` native1180740 activates an existing registered placement wrapper with resource prerequisites; its accepted result is not completed creation. CreateDirect/CreateDynamic and ResetReceivedDead addresses are VM wrappers requiring parsed contexts, not callable `(enemyId)` APIs. ActorFindById returns a borrowed registry pointer without a retained lifetime. Those contracts, cross-mission assets/navmesh/script ownership and reward suppression require separate work; none blocks the narrower genuine native replay practice above.
