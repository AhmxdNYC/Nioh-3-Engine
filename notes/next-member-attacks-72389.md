# Next-member attacks (#72389)

Offline candidate, NOT a confirmed damage fix. Base 0de78b69, branch cx-pc/next-member-attacks.

## Evidence
Ally #72315 / file #5462: R0034 native Jakotsu dies 23:16:17.926, mod Enenra appears 23:16:28 after grace; no player-hit lines in 61 seconds. Enemy team 1, player 0, awareness and motions active. Practice and roster fallback share SummonStart -> SpawnTick -> CreateBoss and Character loading/readiness. AutoBind resolves CE_ENENRA_00 at 100% from block-0 rows. 'attack-family motion, no attack block' is move_catalog.h's catalogue label, not proof of a runtime resource failure. No like-for-like Practice Enenra live control exists in this evidence.

Corrected script symbol-ID audit: R0034 sub_event_boss_clear_check calls StartMissionClearState at 0xf7a4 BEFORE BossBattleModeEnd at 0xf7e0. StartMissionClearState calls Mission::ClearGadgetAccessWaitMode at 0x8040. The mod delays its own win but leaves this native transition unguarded, before the missing roster identity spawns.

Native wrapper 0x23a3260 sets [[G_45B9E40]]+0x270d, stop flag 0x39 and mission+0x3b9; end wrapper 0x23a3d20 reverses them. The 0x270d byte feeds AI condition 0xa8 (table 0x1016b4 -> 0xf82a7) and state predicate 0x9e (0x25c510 dispatch -> 0x25d781). This proves an unguarded state transition, NOT that it causes the observed zero hits. Contact handler 0x117a6d0 also has independent team/no-hit/no-damage gates. No live flags were inspected.

## Candidate
BR resolves ClearGadgetAccessWaitMode by the native-table NAME and uses the existing VM-balanced InstallVeto. TR's read-only query restricts it to an admitted mission/world and active native generated roster. Exact placement identities, owned handles, confirmed kills and dead flags decide pending; missing actors are not treated as dead. No per-boss list or direct AI/team/invulnerability/progress writes. Final deaths visible before frame poll pass through. Single/ambiguous rows, Practice, ended attempts, different missions and older TR pass through. Existing unbeaten SetMissionClearFlag veto remains unchanged.
Suppressed clear-wait is not replayed after final death; final win/Return remains a live regression gate.

Hit logging previously stopped at 20 hits per process. Ally logs had 8+8+4 across rows: that cap cannot explain Enenra (only 16 then), but can hide later rows. Now first 20 log, then at most once per second with cumulative total; a hit after the 8-second grace is visible.

## Checks
TR and portable-MSVC BR builds pass (existing unused-variable/fixed-RVA warnings). roster_combat_guard_test.py passes 14 production query/veto cases plus production ParseNative on the actual clear-wait binding bytes (ctor 139938, fin 1181730). Native roster/fallback sequence, native phase, unbeaten-menu regressions pass.
Full test_sites.py retains 10 failures: 5 unchanged ambiguous soul-core signatures plus their patch simulations (ScProbe_AsyncLoad, ScProbe_SpiritActive, SealProbe_65A8A8, SealProbe_68157C, SealProbe_3203EC). New named site passes; no existing sites changed. Not a fully green site suite.
No game, installation, saves or locks touched.

## One live gate for Claude
Both candidate DLLs, fresh process: menu Fight now R0034, infinite health ON, kill native Jakotsu, idle close to Enenra 60 seconds, finish and Return. Require 'remaining-roster combat guard installed' and 'ClearGadgetAccessWaitMode SKIPPED', positive Enenra player-hit lines/total, final-only win and successful Return. Existing baseline is zero hits. If still zero, reject this cause and compare Practice Enenra with actual contact/no-hit flags next. Do not call fixed before this result.

TR bdcb0c0785fab741e56db563793a19838929a3e253a6811154e4add675d52668.
BR 4e50e541a405959692d8ce050cee6364f9f9035227fbb50d3d6a8c92263247a1.
Installed PC remains EF 1de96af5 BR 65d31658 TR 99600c91 Cam 7482c03d. Public remains 1.8.6.
