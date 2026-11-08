# Fight now destination ownership and native exit — task 71362 / thread 71350

Base: origin/pc/rc-187-final 34912c12. Branch: cx-pc/no-fight-fix-71350.

Training publishes per-mission entry ownership before admission; BR retains that owner until its attempt ends. BR does not arm spawn overrides, gate injection, warp/rewarp or proximity nudges for these attempts. Missing Training export preserves the old BR-only route. BR still owns native admission, queue verification and save-protection vetoes.

Training native return cancels pending spawns/retries, stops its fallback session, drops its custom gate/input mask and calls the original MissionRequestReturn with unchanged arguments. Native trips do not intercept native exit prompts or create a custom ReturnHome gate. Ordinary in-place Training retains its existing gate behavior.

The 30-second native wait remains. Route values are 0 ended/unowned, 1 native pending, 2 native actor ready, 3 fallback pending, 4 fallback actor alive, 5 failed. BR status/log identifies the route, and fight-history names carry [native] or [fallback]. A fallback request alone is not reported as a live fallback actor. BOSSMON now uses XYZ distance.

The existing exact-placement duplicate remover is retained: world, record placement and character must match; player, tracked dynamic actors and actors absent from the live table are excluded. Offline tests exercise each exclusion and the exact-match deletion. The new observer never deletes actors and logs late native candidates. Runtime late-arrival/deferred-deletion behavior still requires the requested live pass; logs explicitly mark that uncertainty.

## Offline checks

`mod/Nioh3Training/tests/native_entry_return_test.py` compiles extracted production handshake, return hook and duplicate-removal functions with mocked game services. Checks wrong-mission callbacks, refusal, ownership after entry completes, all five native route exits, unchanged native arguments, session/pending cleanup, ordinary Training gate behavior, wrong-actor deletion guards. Additional source checks cover all BR mutation entry guards, unchanged 30-second threshold and XYZ distance.

Save veto 190a33b3 is an ancestor of the base. No saved-progress code, mission flags, boss/mission lists or address lists were added or changed.

## One quick live pass (Claude-owned, install BR and TR as a pair)

For every row: verify the selected mission/world and expected living opponent; no falling; BR log reports destination owner=Training and zero BR warps/overrides/nudges. Record native versus fallback from BR status and FIGHT history. After observing the fight, use the native mission exit (BR return/native pause-menu exit), verify mission ID actually changes and the next row is admitted without a stuck replay gate. Back up/restore saves and verify story progress unchanged.

| Row | Specific evidence required in addition to the common exit checks |
| --- | --- |
| R2068 Otazu no Kata | Training placement remains effective, no BR overwrite; one Otazu actor in the intended arena, native or explicitly labeled fallback. |
| R8FF1 Cavern Ascetic | Training placement survives the old retry-point mismatch case; one active expected opponent, no BR rewarp. |
| RFCCB Kamaitachi | No second controller overwrites Training landing; one Kamaitachi and no duplicate if native appears after fallback. |
| R9A25 Kajiwara Kagetoki / Ryomen Sukuna | Correct mission and first intended encounter; route accurately labeled. This checks ownership/exit only, not completeness of its multi-boss wave sequence. |
| R2EE9 Otoroshi | Correct mission/arena and one expected Otoroshi; fallback, if needed, stays labeled fallback and native exit changes mission. |

If a fallback occurs, keep the actor identity log through the exit and inspect exact-placement dedupe messages. PASS requires no second live copy and no deletion of the tracked fallback or unrelated actors. Report an unobserved late-arrival case as unproven, not passed.

## Explicitly excluded follow-ups

- R25E4 box-point floor: derive safe ground/3D landing from game collision/placement data and correlate it with the mission trigger; this change does not repair the floor.
- Multi-boss rows: derive wave/order and setup preconditions from the mission script and verify the intended opponent sequence, not only a global BossBattleMode signal.
- Tutorial rows: derive tutorial/input/script prerequisites and expected opponent from mission data; proximity alone does not establish a started encounter.

Exit timing baseline correction (pc #71494 / pc2 #71455): 31-52 seconds covers only the 8 duel rows. RA0CB and Yamagata exit timings are unknown. Compare R9A25 exit timing against those duels; do not count this as a 10-row timing baseline.
