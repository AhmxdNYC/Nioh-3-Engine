# Six fallback rows — task 72263

Base origin/cx-pc/door-3rows 71ea9f0b; branch cxl-pc/fallback-rows. Offline only, no install/game/save/lock. Public stays 1.8.6. Source checkpoint 238fead6 contains the first built gate/wave fix; final extension preserves ordinary prerequisite actors as well.

| Row | Source/log conclusion | Generic repair | Still needs live proof |
|---|---|---|---|
| R25E4 Ippon-Datara | WaitWarpStart1047905 -> WaveBattle6134; catalogue1043843 was an invasion box, not the right. Old landing fell before fallback. | Derive gate through selected enemy's exact wave group; observe active earlier wave members instead of requiring final boss. | Exact right observed/granted, native GatherWarp floor and all-wave completion. |
| RB24D Death Veil | WaitWarpStart573194 -> Create573339; catalogue had the gate but kind box was excluded by GateDoor. Old near-boss nudge fell. | Generated literal native wait can arm regardless legacy door/box label. Native script owns SetPos/intro/creation. | Correct native relocation, phases and normal victory. |
| R2068 Otazu | WaitWarpStart1072821 -> WaveBattle57187; selected Otazu is wave3, not first. Old catalogue lacked gate. | Recover wave-linked gate and retain native ownership once an exact active early wave member appears. | Earlier waves, native Otazu and completion. |
| R8FF1 Cavern Ascetic | HostOnly box wait -> GatherWarp -> WaveBattle61753; selected boss is wave3. Original landing already lies inside a valid trigger, so missed-box entry is NOT proven. Logs did not enumerate early waves. | Derive reproducible native pose inside trigger; critically, recognize active earlier native wave actors and prevent replacement across later gaps. | Confirm which wave previously ran; fight all3 stages. Geometry-only success is not claimed. |
| RFCCB Kamaitachi | WaitWarpStart1089694 -> indirect onryo controller -> ordinary enemies -> final CreateAndWait1088859. Old catalogue lacked gate. | Recover call-graph-linked gate; after the grant, recognize exact active prerequisite actors and retain script ownership even beyond120s. | Onryo/ordinary sequence and eventual Kamaitachi, without timeout duplicate. |
| R9A25 Kajiwara | WaitWarpStart262638326 -> CALL_FN creator353508; old catalogue lacked gate. Earlier IsMissionClearedMissionID39461 (0x9A25, this mission) can return before the wait. | Recover indirect gate; keep existing exact-polled-right guards. | Own-clear predicate result is unobserved. If that early return fires, this build deliberately does not bypass it; native start remains unresolved in that branch. |

No per-row/mission overrides: generators scan all catalogue missions, original scripts, enemy and box records. 23 unambiguous gate mappings, 37 exact wave members, 50 literal post-gate creator members across21 rows. Runtime requires exact placement/spawn-key/registry identity, excludes player/owned summons, and requires active AI/positive health. Prerequisite actors are accepted only after the gate grant so pre-created actors cannot prevent it. A validated native wave/sequence permanently ends fallback for that attempt, including gaps between waves. BR still only requests the right actually observed from the native script; all gate grant guards and Training-only movement remain. No script temp, story or clear writes, wave requests or direct native boss creation added.

## Fallback kill result

All six supplied failures are `killid <catalogue placement>` returning before any force-kill call. The live dynamic handles differ:

| Row | Harness catalogue ID | Live CreateDynamic handle |
|---|---:|---:|
| R2068 |1071509|269505520|
| R25E4 |1047415|269475499|
| R8FF1 |1065308|269496227|
| RFCCB |1088859|269515497|
| R9A25 |353508|268786927|
| RB24D |573339|269010402|

Production ActorByHandle uses live actor+4. Arena creation sends its returned dynamic handle to BossModeBegin; confirmed-death paths record g_dead_id=g_bm.id, and TrainTick uses that proof, not catalogue placement. The compiled actual ActorByHandle/BossKilledNow/TrainTick test reproduces all6 pairs: old command misses and no win is correct; an actual confirmed death of each tracked dynamic actor yields exactly1 win. Thus these logs show a harness target mismatch, not a lost reported kill. No win-reporter patch is justified by this evidence. This offline proof assumes normal enabled boss-mode/death tracking; real damage and mission/script completion still require the live gate. Harness should target the returned active dynamic handle (or its normal tracked-boss kill control), never silently remap arbitrary killid targets.

R1CBC active-vs-dormant label is already fixed in this base via #72084/#72150. Existing executable label checks pass; no second label patch.

## Validation and artifacts

BR and TR builds exit0; actual output SHA256: BR b9940e8a45a44b9e48614dbef0f7e52a453af0e382003b07eb6f0b7ff583f925; TR 8fb076323aefaaf0cbaf1754516096394e974ec489ff82eadae03e5a5d39b2cf.

Offline PASS: 7 gate generator/runtime tests, 13 box/wave data tests, exact37 wave +50 prerequisite identity observers, native-entry scenarios (no pre-grant completion; no fallback after later >120s gaps), native-return/ownership/dedupe, 6 roster-data tests, all23 native/fallback rosters, phase/early-exit/retry tests, BR phase ownership and9 outcome cases, plus6 actual fallback kill identity pairs. Both generators reproduce checked-in output. Original files/offsets: notes/fallback-row-script-a-72263.md, notes/fallback-row-script-b-72263.md and generated JSON evidence.

Next: one device tests these6 rows, records exact gate grant, native initial enemies/waves/final boss, no fallback/duplicate through >120s ordinary play, and normal kills. R9A25 needs own-clear predicate observation if the right never appears. No live success is claimed here.
