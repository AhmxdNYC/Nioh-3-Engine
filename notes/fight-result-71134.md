# Native Boss Match result audit ? #71134 / reply71111

Base: cx-pc/rc-187-timeline a7f27be0. Fix branch: cx-pc/fight-result-71134 (not a replacement release candidate until lead integrates it).

## Real bug: yes, normal player kills can be logged as quit

In the base, bosslog.cpp:53-75 MaybeStart/MaybeEnd owns native fight rows. HkEnd:131-140 immediately sends phase4 unless g_clearWin was set. HkClear:164-169 sets it only while g_active; its registration:6210-6213 is gated by Debug.BossLog OR Debug.RematchLog. Both default off (main.cpp:154). Thus a normal native kill with those loggers off has no win signal. Even when logging is enabled, rematch.cpp:953-962 VetoClear deliberately skips the native mission-clear write for protected unbeaten matches, so the downstream SetMissionClearFlag observer cannot see it. Also BossBattleModeEnd before clear closes the row as quit; subsequent clear cannot change it because HkClear ignores inactive fights and fightlog.h FlEnd refuses already-closed rows.

Training receives native BR marks at training.c:1828-1833. fightlog.h:46 FlClose latches the first end and clears open; it does not infer results from damage or loot. The timeline formatter is not the source of this bug. The older two wins in the four-fight fixture are Training-owned in-place sessions; those use TrainTick:1738-1739 g_dead_at/g_dead_id, not BR's native clear flag.

HP poke versus real hit: the native path never checks damage amount or HP delta; lowering HP to1 then landing a native lethal hit does not explain or exempt these defects. HP0 alone without native death/clear is not enough to assert victory. Training-owned sessions confirm death in BossKilledNow:997-1007 and BossTick:1087-1119 (including stable-HP0 fallbacks); TrainTick then latches win. TrainStop:1683-1684 before that tick can close an unresolved round as quit. That separate early-return window is not the reported native route and this patch does not change Training gameplay/death detection.

Native-first versus in-place selects different result producers: native BR observes mission completion; Training owns spawned in-place actors. Return/world load forces native end; if victory was not latched before return, the old ring remains quit forever. Current fix deliberately does not turn an explicit force-end into a win from an unrelated later mission event.

Ally #71113 establishes native route + real hit +HP0/loot +quit, but the full BR/Training event logs were requested (#71145); precise ordering/debug ini for that particular run is unconfirmed. No claim that the poke caused it. Main pc #297 owns real-hit confirmation.

## Fix

Observe the script mission-clear request in existing VetoClear BEFORE the save-protection decision. Never remove/bypass the veto or write clear/story progress. Capture running mission and Training attempt token at native fight start. ConfirmNativeWin accepts only matching mission and the active fight or20-second existing post-boss-mode window. It calls a new Training export to close the exact attempt as win, or correct its just-closed quit while retaining its end timestamp. Token/most-recent-row checks prevent modifying a newer round or a loss. Explicit ForceEnd clears the token. No boss list, address list, setting/version bump, HP threshold, loot inference or forced game action added.

BR and TR must be used together for this fix; EF and public camera stay unchanged. This is offline evidence, not a live PASS.

## Validation

Actual production ConfirmNativeWin + VetoClear + fightlog.h executable harness passes9 scenarios: default debug-off clear, unbeaten veto still returns true, end-before-clear retains end time, loss preserved, wrong mission rejected, newer attempt rejected, explicit return invalidated, expired grace ignored, no-clear stays quit.
Rematch77 checks, native attempt96 transaction plus admission/return/map checks, Boss Match persistence, timeline unit and4-fight recorded replay PASS. BR and TR release build exit0 (existing warnings).
Evidence: Temp/result71134 in D:/nioh3-wt/fight-result-71134. Installed main-PC build unchanged: EF85ebffdc BR4c219306 TR0e30328a Cam7482c03d. No game, install or release.
