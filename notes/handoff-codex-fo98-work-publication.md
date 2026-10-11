# FO98 retirement work publication - 2026-10-11

Bounded offline native/decomp trace, main parent 1a65b12; experimental
24494d5 unchanged. FO remains NO-GO. No runtime or performance claim.

## Pending versus active records

25208C acquires a capacity token from queue+50 with a zero-time wait.
On success it reserves queue+3C modulo queue+38 using lock-cmpxchg,
copies the three pointers into queue+30[index*24], then lock-increments
queue+40. It does not publish queue+48 or release a work token.
Its explicit caller 250ADC holds owner+D00 exclusive during enqueue,
growth/rebuild and retry. This establishes exclusion for this caller,
not for every possible caller or the experimental FO command queue.

250650 holds owner+CF0 then owner+D00 exclusive. It atomically samples
queue+40 at owner+8D0. For that pending prefix it calls 1278D8, 127CDC,
127B4C and 127A80 before 1267A8 publishes work. The latter advances
queue+44 modulo capacity, lock-increments queue+48, lock-decrements
queue+40, releases one token on queue+58 and calls 126984 for each
record, then writes the advanced queue+44. It also increments
owner+944 and atomically increments owner+94C accounting counters.
The sampled prefix cannot gain records from the explicit 250ADC
producer while this lock is held. This is a local publication contract;
it does not close the experimental deferred-command producers.

Previously traced consumer 1A757B8 waits on queue+58, tests queue+48,
releases each record, returns its capacity token and decrements queue+48.
Therefore pending+40 and active+48 are distinct stages. Rebuild resetting
+48=0 is consistent with pending records needing a later publication
pass, rather than fresh work-semaphore tokens being an enqueue side effect.
Restart and accounting across all growth races still need a full proof.

## Scheduler and unresolved GPU boundary

2EF0938 (existing symbol CacheFlushOnGfx) gates on owner+D20, waits for
owner+728 via 2EF08E0, acquires owner+CC0/+CD0 and calls 250650 with
its original owner/context arguments. Later it starts/waits other workers
at owner+758/+788. It does not directly publish retirement queue+48.
The preceding completion wait is a CPU event, not established GPU proof.

1278D8 native code preserves a fourth argument omitted by its decomp.
For each pending record it calls 1A72080(scratch, record[0]), then if
record[1] is non-null calls its vtable+B8 with the original scheduler
context. 1A72080 forwards record[0] by address to 12796C. These concrete
edges identify the next GPU-ordering investigation; none establishes a
GPU wait by itself. Trace 12796C/127CDC/127B4C/127A80 and concrete
record[1] vtable+B8 before attributing fence or submission semantics.

## Verification and next unit

py -I re/scripts_renderer/fo98_work_publication_provenance.py:
6 new named-window SHA256 checks PASS. Focused native windows reviewed
with Capstone and existing decomp; no Ghidra, raw dump export or prior
passed-suite rerun. Byte identity is provenance, not behavioral safety.

Next ready unit: follow the four pre-publication helpers above into
submission/fence ordering, then relate that boundary to producer closure
and ring restart. Concrete resource vtables and map replacement remain
open. No production patch or combined lifecycle safety gate passed.

No local game at entry/checkpoint; no game test/lock, build/deployment,
installed ASI/INI/config/save/default-visual/settings/hardware change.
Prior restoration retained; controller untouched; no Claude call/wake.
Phone inbox empty. Remote1767/1768/1781 pending at last known bridge1785;
no fresh query. Serial ownership until exit; captures untouched.
