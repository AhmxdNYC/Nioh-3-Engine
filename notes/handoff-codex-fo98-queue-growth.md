# FO98 retirement queue growth - 2026-10-11

Bounded offline native trace, main parent a16449d; experimental 24494d5
unchanged. FO remains NO-GO. No production repair or performance claim.

## Correction: 2EEF3B0 grows storage; it does not drain objects

The prior next-step label "retirement drain2EEF3B0" was inaccurate.
Native 2EEF3B0 reads pool+1C, returns false if zero, otherwise selects the
unsigned minimum of pool+18 and (queue+38 + pool+1C), with queue=pool+890.
The 32-bit addition has no visible overflow guard. Its callee 2EEE310 asks
pool's allocator vtable+38 for capacity*24 bytes; native RAX is used as the
allocation result despite the decompiler's void signature.

On allocation success, 2EEF3B0 invokes 2EF08B8(queue), then
2EF01F4(queue, capacity, newStorage). On success, 2EEF950 calls allocator
vtable+58 on pool+10 (old storage) and replaces pool+10 with newStorage.
On resize failure it calls the same allocator slot on newStorage and
returns false. Neither path dereferences/releases queued object pointers.
The caller's second enqueue result remains unchecked, as recorded in the
prior retirement note; growth is not proof that a slot became available.

## Quiescence helper and repacking

2EF08B8 releases one semaphore unit through queue+58 and tail-jumps to
2EF08E0. That helper atomically samples queue+18, conditionally calls
126984 if bit0 is set, calls1266FC, then WaitForSingleObject(queue+10,
INFINITE), and tail-jumps126A14. The meanings of queue+10 and these
synchronization helpers are unproven; do not infer a complete producer
closure or GPU fence from this wait alone. Their side effects are outside
this unit, so this is not a claim that no downstream retirement occurs.

2EF01F4 samples queue+40 with lock-cmpxchg, saves the first sampled count,
and tests a second atomic read for zero. If nonzero, it copies that first
count of 24-byte triples from queue+30 in ring order starting queue+44,
modulo queue+38, into temporary stack storage. It creates a new capacity
semaphore (initial capacity-count, maximum capacity) and notification
semaphore (initial0, maximum capacity+1), replacing queue+50/+58.
Failure paths restore old handles and close a successfully created first
handle. Success closes both old handles and copies the saved triples into
newStorage; then resets queue+44/+48 to0, sets queue+30=newStorage,
queue+38=capacity, and queue+3C/+40=savedCount. These are storage and
handle operations; no object vtable/refcount operation appears here.
The two count reads require the surrounding quiescence contract to make
their relationship safe; this unit neither proves nor disproves it.

Imports independently resolved from the mapped PE import directory:
38E2598 ReleaseSemaphore; 38E25E8 CreateSemaphoreA;
38E2390 CloseHandle; 38E2340 WaitForSingleObject (KERNEL32).
Named Capstone windows include the success tail2EEF434 and complete
2EF01F4 return2EF0395. No Ghidra or raw dump output/file.

## Evidence, limits and next unit

Run `py -I re/scripts_renderer/fo98_queue_growth_provenance.py`:
five new static identity checks PASS, not runtime/lifetime/safety proof.
Previously passed suites were not repeated. ResList allocation/capacity,
pool+D20 transitions, actual retirement worker/GPU ordering and emitter
caller modes remain open. Next: trace queue+10 worker creation and
126984/1266FC/126A14, then the consumer of queue+30/+40. Establish real
quiescence/retirement before designing the combined FO queue repair.

No local game at entry/checkpoint; no game lock, test, build, deployment,
installed ASI/INI/config/save/default-visual/settings/hardware or controller
change. Prior restoration retained; pre-existing captures untouched.
Phone inbox empty at entry; user1802 ("Are u work working?") arrived at
checkpoint and was acknowledged in commentary and the Discord result.
No Claude call/wake. Remote1767/1768/1781 remain
pending at last known bridge1785; no fresh query. Serial ownership until
exit. Topic checkpoint committed/pushed with Codex attribution; Discord
result via existing notifier after push.
