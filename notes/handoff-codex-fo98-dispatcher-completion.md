# FO98 dispatcher consumer completion boundary - 2026-10-11

Bounded offline/native trace, main parent 5eaca34; experimental 24494d5 unchanged.
FO remains NO-GO. No runtime, lifetime, safety or performance verdict.

Native 1A7C3E0 waits on ring+10, checks write+24 against read+28,
then copies the entire 32-byte record into stack locals (1A7C41D/425).
Only after that copy does 1A7C433 lock-xadd read+28. The ring slot can
therefore become available while its copied record is still executing;
equal read/write counters alone do not prove replay completion.

The token loop at 1A7C44C checks the current cursor against the copied
end pointer, dispatches table[ushort opcode] at 1A7C463, and passes
rcx=context, rdx=&cursor. Native code recovers the cursor argument missing
from decomp. Each handler must return before the next iteration. This
proves the synchronous call ordering, not completion of asynchronous work
that a handler may publish. Handler internals are outside this unit.

For nonzero submit byte, optional immediate-draw flush is followed by
496078, then 4930C8 with context+5A0, context+588, returned RAX and flag1.
496078 tail-jumps to 1A7C4FC after optional flush; decomp labels it void,
but the native consumer uses its returned RAX. That target's internals
and GPU completion are unresolved. 4930C8 can skip enqueue when the target
is not newer or indexes are invalid; otherwise it calls 86F760 before
updating submitted target+38 and its list indices. Do not assume every
submit-marked record causes a new ECL item.

86F760 constructs an ECL record, calls 86F828 on owner+88, releases
owner+80 semaphore, stores owner+60 and returns. 86F828 reserves capacity
via ring+8 semaphore (zero timeout, then infinite wait in cold 1921A30),
copies 32 bytes and increments its count. That observed wait is a queue
capacity wait; it does not itself prove the submitted GPU work completed.
The downstream ECL consumer/fence contract remains open.

Only after the token loop and optional submission call return does
1A7C4AF lock-decrement dispatcher ring+30, then return to the semaphore
wait. Thus the end-frame predicate's in-flight term covers this CPU
processing interval even after read+28 advanced. This still does not
close producers: another publication can follow an idle observation.
No safe FO flush/lifetime boundary follows from this trace alone.

## Verification and checkpoint

`py -I re/scripts_renderer/fo98_dispatcher_completion_provenance.py`:
six newly reviewed byte windows PASS. Hashes pin static evidence, not
behavioral safety. ECL hot body is bounded at RET (140 bytes); the decomp
size includes cold-tail accounting and must not be used as a contiguous
hot-body boundary. Cold tail separately pinned through its jump back.
Prior passed suites not repeated; no build or game test justified.

No local game/Ghidra at entry/checkpoint; no lock needed for offline
reads. No installed ASI/INI/config/save/settings/hardware/default visual
changes; prior restoration retained. Controller untouched; no Claude
call/wake. Phone inbox empty. Remote1767/1768/1781 pending at last known
bridge1785; no fresh query. Unrelated captures untouched.

Next ready unit: trace native close target 1A7C4FC and bounded downstream
ECL consumer/fence ordering. Resource vtable+28 caller modes, producer
closure, cache+20 identity, retirement restart and combined repair remain open.
