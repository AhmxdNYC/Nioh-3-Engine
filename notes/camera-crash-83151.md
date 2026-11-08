# Crash analysis and camera smoothing #83151

## Crash evidence
PC reports D:/nioh3-re/crash-83148/Nioh3EngineFix-crash-20261108-035155.txt and -035158.txt describe the same crash and subsequent error popup, not two independent crashes. Exception at Nioh3.exe+468FC reads 0x20; RCX=0x14, RAX=0. Native stack: +468FC, +27EA01, +40489F, +40330E, then worker dispatch. No BR/CamKi stack frame.

Existing offline decomp D:/nioh3-re/decomp/0x468fc.c identifies TimedLerpAdvance, whose first elapsed field is argument+12: 0x14+12=0x20. Caller 0x27e92c.c traverses render-object list +458 and invokes TimedLerpAdvance(node+0x14). Thus immediate cause is a null list node passed into native timed-lerp advancement. 0x404780.c is CharacterUpdateMainBody; 0x4032e8.c JobFnCharacterUpdateByMode. R14=1B8C229BB00 matches newly spawned core 80D5F actor; log bind 03:51:55.706, exception .731. This was entry, not exit. Underlying list invalidation/corruption cause is NOT established; this build does not claim to fix that native crash. Prior root-checkout notes/soulcore-swap-A-la.md associate the same RVA with old borrowed-model lifetime failures, but do not prove that mechanism here. Narrow Opus review requested from lead in #83154 (Opus model unavailable in local subagent tool).

## Camera changes
0.7 second smootherstep (6t^5-15t^4+10t^3) handovers from current applied offset; first frame unchanged. Both eye and look target receive the SAME Y offset through existing ApplySoulVertical. Persistent 20 ms filter and full current seal-player Y retained. Exit uses stored numeric state, never an actor pointer.

Producer checks actor-table membership before actor reads; captures key/root per seal+capture time, brackets position read with key/root checks, rejects changed identity for remainder of capture. Existing SEH Copy protects all reads with no VirtualQuery scans. This detects identity changes, not same-key/same-root ABA reuse; no native generation counter claimed.

## Validation
444 camera checks + 51 ground/restore checks PASS; both Release builds PASS; CamKi probe_test PASS; diff check PASS. Includes absent actor mid-enter with zero further actor reads during exit, recycled identity rejection, failed position copy, and production SEH Copy against VirtualAlloc storage after VirtualFree. 2000-unit 60Hz stationary handovers max step 89.151 each direction (<90), down from 124.73; zero initial discontinuity; steady unity gain within 5%. Low FPS or moving targets can exceed this fixture bound.

BR c7315ca390ceb7842d02f2da7256e36a8a6fb27bbdc0a867f20b4f5194f22cc1
CamKi 617d93cd4c61d6124fd546a04762558a1592729fd20f97b3cfc7d042a72b4fbd
No game/process/install touched. Claude pc owns live validation and native-crash follow-up.
