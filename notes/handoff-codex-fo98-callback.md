# FO98 retirement callback and consumer - 2026-10-11

Bounded offline native trace, main parent65cdf23. Experimental24494d5
unchanged. FO remains NO-GO; no production repair or performance claim.

## Descriptor and context

2EEF6D4 constructs descriptor+0=2EEF8F4 and calls2EEFEB0 with
RCX=owner+890, RDX=descriptor, R8=record buffer, R9=owner.
Descriptor+28 receives initialization input+E8 (capacity).2EEFEB0
creates capacity semaphore queue+50 with initial/max count=capacity,
and work semaphore queue+58 with initial0/max capacity+1. It passes
owner context to2EF00F8, then installs record buffer at queue+30 and
capacity at queue+38. Queue initialization failure cleanup is not fully
proved here; no lifecycle verdict follows from this construction.

Callback2EEF8F4 is a 30-byte tail stub. It replaces RDX (the worker's
queue+18 flag address) with owner+58, sets R8=owner+CE8,
R9=owner+8F0 and RCX=owner+890, then jumps to1A757B8.
This callback does not inspect the worker flag argument. The flags'
meaning elsewhere remains open; do not infer producer closure from them.

## Retirement consumer order

1A757B8 caches queue+50 (capacity semaphore), queue+58 (work semaphore)
and queue+30 (24-byte record buffer). It waits INFINITE on work semaphore,
atomically samples queue+48 with lock-cmpxchg, and returns when zero.
For each nonzero sample it:

1. Calls record[0]'s vtable+10 with RCX=record[0]. The concrete vtable
   target and any CPU/GPU wait semantics are not identified in this unit.
2. Calls532C64 with RCX=record[1] if that pointer is nonzero; helper
   ownership semantics remain untraced.
3. Acquires owner+CE8 exclusive, calls251B5C with RCX=owner+58 and
   RDX=record[2], then releases that lock.
4. Clears all24bytes, releases one capacity token, atomically decrements
   queue+48, advances local index modulo queue+38, increments owner+948
   and atomically decrements owner+94C. It then waits on work again.

Those owner counter offsets follow R9=owner+8F0 and native +58/+5C.
The local ring index begins at0 on callback entry. Restart/rebuild state
must be traced before treating this as safe for arbitrary pending rings.
2EF08B8 releases one work-semaphore token before tail-calling2EF08E0's
completion wait. This supports an empty-queue wake/exit path, but does not
prove admission closure or all-producer ordering.

## Verification and limits

`py -I re/scripts_renderer/fo98_retirement_callback_provenance.py`:
5 static byte-window identity checks PASS against the existing dump.
Native instructions cross-checked against decomp2EEF6D4/2EEFEB0/1A757B8;
callback stub is absent from the per-function decomp catalogue.
These are provenance checks, not runtime tests or release safety proof.
Prior passed suites not repeated. No game, Ghidra, build or deployment.

Next narrow unit: trace251B5C pool-return behavior and identify the
record[0] vtable+10 implementation from retirement enqueue provenance.
Then prove GPU/producer closure and ring rebuild ordering before the
combined overflow/stream-space/late-producer repair. ResList capacity,
D20 transitions and emitter caller modes remain open.
