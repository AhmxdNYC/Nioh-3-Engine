# R1DEA cover branch correction (private)

Base: frozen dd1cc4d; isolated codex-cover-branch-readiness. Engine source/binary unchanged (1.7.71); addon1.4.19. No training merge or installation.

## Actual failure

Manual PID43900 token2: gate C3D27090 requested02:45:25.329; Create4097963 at28.787; BossBattleMode29.516 and native player position181124/28306/140840 inside. Original cover expected4092708 forever. Load02:45:49.436 returned player outside53.125; cover timed out02:46:13.674. The first door was already opened automatically. Death is plausible for the later reload, but those log lines alone do not prove it.

## Exact private script evidence

archive09/29/D38178C9B28E1FB9B8A3F119B56F8433477BCB93384C8AE389FDB8499AEB13: strings pool4990 (141 entries) ends623A. Box handlerF990 tests4092678 and starts pool125/126: okita_missionflag_false/true. False branchF9F2 excludes parallel, tests flag187 false, starts pool128 Souji; trueFA5A accepts flag187 true or parallel, starts pool130 Yoshinobu-only. Souji wait gateFB30, Create4092708 FC22, BossBattleModeFC6E. Yoshinobu-only wait same gateFFE8, native SetPos1008C, Create4097963 10130, BossBattleMode1017C. Existing older CSV function labels/offsets are shifted; these offsets came from actual function markers/symbol IDs.

## Bounded correction

Only tuple1DEA/4095000/canonical4092708 admits alternative4097963. Same-attempt, exact mission/start native Create latches the selected actor before BossBattleMode. Conflicting allowed creations fail closed. Current actor/algo/native-not-dead/player/control/readability and500ms continuous settlement remain required. BossBattleMode entry exposes a VM context, not a decoded placement argument; this patch does not claim to decode it or use the historical log ring as identity. Creation plus the exact allowlist/current actor supplies branch identity under the shipped script contract.

Known stable current-player DEAD under the exact matched attempt ends only the cover. Missing/unreadable players during loading do not count as death. A load after an observed fully readable controlled encounter ends only the cover; earlier intro loads remain allowed. No request, gate, warp, restore ownership or timing expansion is introduced.

## Checks

Before correction the compiled actual Model fixture with Create4097963/BossBattleMode/all current ready predicates failed Ready. After correction48 branch/death/reload cases pass; existing73 model,200 reader,37 Present and emitted-thunk checks plus20 native anchors pass. Actual included rematch.cpp UI harness exercises exact-start hook gating, actor selection, real passive player reader/CoverTick known-dead versus unknown, wrong attempt and retained native/spawn ownership sentinels. Both actual menu harnesses pass. Native/manual acceptance remains pending.

