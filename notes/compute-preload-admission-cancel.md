# Native preload admission and cancellation are not a shutdown join

2026-10-11. Bounded read-only continuation; no live hooks, game or ASI edits.
This closes the producer/cancel primitive semantics and rejects a proposed
shortcut. It does not establish that native reset has an actual race.

## Actual boot-request enqueue body

**11022FC** allocates the boot request with vtable3AAD6C8, publishes its owned
pair, then loads loader manager475A420 at1102456. The enqueue path:

| Native site | Operation |
|---|---|
| 1102466/6D | Test manager+24E admission, return/skip if zero |
| 1102484/8E | Acquire manager+98 mutex |
| 110249B | Read pending-list sentinel manager+1D8 |
| 11024EF | Allocate20-byte list node |
| 11024FA | Retain request pair control strong reference |
| 1102506 | Increment pending count manager+1E0 |
| 110250D..1C | Link node containing the request/strong pair |
| 1102526 | Set manager+24A pending-change flag |
| 110252E | Unlock manager+98 |

There is **no admission recheck after acquiring the mutex**, and no scheduler
pause-bit test in this enqueue region. Request+57 affects request priority
state+50, not the enqueue lock. These are concrete instructions; the dynamic
caller/thread of this producer has not yet been established. A direct-edge
scan found no pdata-validated call/jump to11022FC; absence does not establish
that an indirect/native entry is unreachable.

## Actual admission close / cancel body

Global wrapper2948B2C loads the same manager and calls**2948B48**. That body
first clears manager+24E at2948B55. Then **2947ED8** locks the pending queues
in turn (+98 and+48), marks queued requests+56=1, and unlocks. 2948B48 also
marks active request vectors+218..220 and+230..238 under their respective
mutexes. It returns after releasing these short queue locks. It performs
**no worker-count wait, job-group join or request-body completion wait**.

The boot request's virtual+58 is2002A0, a byte+56 getter. RequestLoad callback
1343CB0 checks this before calling virtual+8=1101B90. Once1101B90/PsoListLoad
has started, its compute loop and child jobs do not poll this byte. The joined
PsoListLoad return remains the relevant completion, not cancellation marked
in the request. Do not interpret request+56 as proof that PSO creation stopped.

## Two native-bound counterexamples to shortcut protocols

These are allowed interleavings of the reviewed primitives **unless their
outer owner excludes concurrency**; they are not observed game failures.

1. Producer passes1102466 while admission is1 and pauses before110248E.
   Closer clears+24E, locks/cancels all currently queued requests and returns.
   Producer then acquires+98 and links its new request at1102506..1C without
   rechecking admission. Therefore close+queue sweep alone does not exclude
   an already admitted producer.
2. Callback passes its+56 check and enters1101B90. Closer marks the active
   request+56 and returns. Already-running PsoListLoad can still be inside
   CreateComputePso before its later cache-lock acquisition. Therefore cancel
   return alone cannot authorize releasing context+818 or serving cached PSOs
   through a context whose lifetime has ended.

The display reset path previously reviewed does not call2948B48: it polls
loader busy and then pauses the default scheduler. A higher serialized owner
could make that sufficient, but the queue mutex and request cancellation
primitives themselves do not supply the missing producer join.

## Upstream destruction ownership status

2EE0D90 is referenced by vtable409A020; it invokes2EE09FC then optionally
frees the allocation. No ownership acquisition or worker join exists in that
wrapper. The reviewed2EE09FC ->2EFB020 ->2F00BC4 chain therefore remains a
common retirement boundary, while its upstream virtual owner-release call is
unresolved. Likewise2948B2C had no pdata-validated direct caller in the bounded
direct-edge scan. Do not connect cancellation to graphics destruction merely
because both primitives exist; the actual caller ordering has not been found.

## Decision

Reject both candidate shortcuts: default-pool acknowledgment is not a
RequestLoad join (prior note), and closing/cancelling the request queue is
not an in-flight-producer or executing-body join (this note). No serving
cache implementation is admitted. The useful remaining evidence must be an
actual outer owner that serializes enqueue and joins RequestLoad before
graphics-context release; another observer or synthetic cache core would not
advance this gate.

Expanded `audit_compute_preload_reset.py` passes24 pinned windows,24
changed-byte rejection fixtures and34 exact instruction assertions. The
whole producer hot body and cancellation bodies are bound; no game bytes
are committed and no CPU/FPS savings are asserted.
