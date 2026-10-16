# Yamagata Summon: paused-controller selection gate

Frozen source `dbd6752b0b54c51e037a2999976ce94a348ab676`, addon1.4.14. No ASI changes in this work. The running1ffa569 cover pair does not include this experimental training feature.

## Reproduced actual behavior

`tools/rematch/test_summon_controller_gate.py` pins the frozen worktree HEAD/clean mod tree, compiles its existing native-stub harness with the real `rematch.cpp`, and extracts the exact Summon dispatch branch without rewriting its condition. PASS31 checks. No game/native calls.

- `training_entry_hold.h::Model::Release` requires real `boxAt` no older than1500ms.
- `rematch.cpp::HoldTick` preserves a known paused hold, but does not refresh that witness. This is correct.
- `UiAct` posts one Summon and says requested. Tick consumes it, clears pending, then refuses when `HoldRelease` finds an aged witness. No intent remains to retry.
- Fixture: genuine held stock-true observation, simulated pause with no subsequent box polls, aged observation, fresh owned pump. Actual HoldTick preserves phase2; actual UI+dispatch consumes/refuses Summon; unpause plus genuine positive Query refreshes the witness but does not summon. Only another selection then releases.
- If native box queries DO continue while paused, fresh queries permit current release. The test does not establish which scheduler behavior occurs on PC2/Ally. Therefore frozen UX is unreliable under the stopped-query pause case, not universally impossible.

## Smallest source integration proposal

One bounded intent, not a second native request and not a synthetic witness:

1. Add attempt-bound `summonPending`/`summonSequence`/`summonDeadline` to the existing entrance-hold model. Selecting Summon on an owned phase2 hold records intent even if its previous real positive witness is old. It does NOT call Release, refresh boxAt, change pause, invoke Create or report success. Keep exact current native admission/ownership gates. A proposed30-second intent lifetime is capped by the existing ten-minute hold deadline.
2. Close only the hosted addon page on accepted intent, with truthful text: **Summon selected. Resume the game to begin.** If the native pause menu remains, the player closes it manually. Do not synthesize B/Start or clear game pause flags.
3. Inside the existing `PracticeBoxHook` lock, after stock predicate first and all current exact box/player/mission/attempt/heartbeat checks: a matching pending intent can release only on this invocation's genuine stock=true. Call the real Filter/Release using this same native query's timestamp; return the original true only once. This supplies fresh evidence atomically and eliminates a UI-to-next-frame1500ms race. A menu tick/poll must never update boxAt.
4. Real stock=false clears the pending intent as the player is no longer in the held box; unknown/foreign/epoch-loss follows existing terminal abandonment. Cancel/return/Create/BossBattle/unload clears intent. Expiring just the intent clears that intent while preserving the existing hold, rather than accidentally restoring stock and starting a boss. No same-attempt auto-rearm, repeated request or automatic mission replay.
5. Required focused integration tests: delayed paused selection followed by one genuine true query releases once; absence of queries never releases; false/mismatched placement/player/attempt/unreadable state/cancel/expired intent never releases from intent. Keep1500ms pump freshness and native unknown-passthrough terminal rules unchanged. Add actual UI test that pending intent closes only the hosted page and does not claim a boss exists.

No ABI change or new general spawning API is required. Source locations are `training_entry_hold.h`, PracticeBoxHook/HoldRelease, Summon dispatch and UiAct in rematch.cpp; update the current hold telemetry with pending/intent deadline only to make this actual operation testable. The implementation owner must integrate into a new candidate; dbd6752 remains frozen.

## PC2 first-Summon case (native gate, no guessed motion commands)

Use a separately reviewed exact training pair/config with `[Training] EntryHold=1`, default-off opt-in Rematch enabled, hosted menu and a cleared current-journey Yamagata. Preserve PC2's own save/lease/admission/controller policy. Do not substitute the1ffa569 cover pair or claim arbitrary summon/arena support.

1. Outside mission1CBC, open the existing hosted **Boss Rematch / Practice** page using the configured controller gesture. Select **Practice Yamagata**, then **Start at boss shrine (native mission replay)**; A once opens confirmation, A once confirms. One fresh native replay only.
2. After native arrival1CBC/start67064, manually walk to the entrance. Read-only `practicehold` must show enabled1, phase2, a new nonzero attempt, loadSeen1, arrived1, broken0 and actual recent boxAt. Status mission0x1CBC/stage67064/loading0/player1/infight0. No coordinates or distance substitute for the real held predicate.
3. Open the menu, enter **Practice Yamagata**, wait at least2seconds before selecting **Summon Yamagata**. Record whether native pause is visible and whether boxAt advances. On frozen dbd6752, a stopped box clock predicts refusal; record it as the reproduced UX failure, not a successful Summon. Do not use repeated rapid clicks as acceptance.
4. On the future intent integration, one A selects Summon; close any remaining native pause menu manually. Expect the same attempt's next real positive query to transition phase2->3 once, then shipped rights/intro/Create65092/BossBattleMode. Confirm boss visibility and actual player control separately. No Create call, forced true, native flag edit or auto-walk.
5. One owned Practice return using its menu confirmation, exact original entry restoration, and one native replay required to repeat. This first case does not prove a different arena or arbitrary repeated spawning.
