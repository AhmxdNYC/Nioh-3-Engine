# Compute preload: joined children and the actual display-reset pause

2026-10-11. Bounded native continuation of the cache lifetime audit. No game,
hook, observer, serving cache, build or installed changes. The useful result
is a real scheduler-pause protocol above reset, with its precise scope still
short of all-preload exclusion.

## What is closed

The two retained direct callers of2EFE1CC are **PsoListPreloadJob34835D4**
(call3483631) and the serial path of **PsoListLoad3483E04** (call3484514).
The parallel compute path allocates48-byte jobs with vtable40EF300, attaches
a pointer to its stack descriptor at job+38, sets range+40/+44, pushes through
3FAD8 at3484444, then **joins through3E7CC at348445F before leaving the load**.
Graphics is separately joined at348431D. There is a prior group drain at
34841EF. These are actual instruction edges, not assumptions based on names.

The compute descriptor carries the saved graphics-device pointer at+10;
the child passes it+10 to2EFE1CC. Progress at descriptor+40 is incremented
after each record; resolve failure1 clears the result flag through+48 but
does not cancel the remaining range. The loop does not poll a reset/cancel
flag. A false result or completed progress increment is not a worker join.

3E7CC waits while group+34>1, helping through403A0, then decrements the waiter
reference. PsoListLoad frees backing records only after this join or after
its inline serial loop. Thus its child compute jobs do not legitimately
escape the PsoListLoad call. No detached preload worker needs to be invented.

Boot request1101B90 calls PsoListLoad at1101C61 and marks request+98 done only
after return. The other entry3484594 is a resource-type stream wrapper that
also calls PsoListLoad synchronously (34845EE). Boot completion alone cannot
rule out later per-package resource-list loads.

## The reset barrier is stronger than dispatcher-empty

Actual **DisplayResetRebuildSwapchain226D140** first waits on3B2970
(loader manager475A420 busy bytes+248/+249), then at226D1A8 calls
**3242CB8** on `[global4759910]+530`. Only afterward does it call
RecreateSwapchainDevice at226D30E. Successful reset resumes that owner through
18CBCB8 at226D331. A failed reset does not take this success-only resume edge.

3242CB8 serializes changes with owner+1F0 bit25, sets pause-request bit24,
and waits under bit27 protection for owner+E8 to become null or its state+48
bit29 to be set. It releases the short locking bits before returning; the
pause-request bit remains set. **This is an acknowledged persistent pause,
not simply a sampled empty queue.**

The native acknowledgment writer is closed: scheduler root8FE5FC sees
owner+1F0 bit24 and branches at8FE670 to cold block**1A37BC7**. It sets
pool+48 bit28, snapshots pool+28 minus1, waits for pool+30 to equal that
value at1A37C19..21, drains/requeues deferred work during the wait, then sets
pool+48 bit29 at1A37C23..2E. It sleeps while8FE690 reports pause still
requested, then clears bits28/29. This is the same +48 bit the reset waiter
observes. The cold block is separately hashed; exported function size668 is
not a contiguous complete function body and must not be used as one.

Resume18CBCB8 clears owner+1F0 bit24 and waits until owner+E8 state bit29
clears (with bounded inner retries/reassertion). This confirms the pause is
held across reset, rather than being released on3242CB8 return.

## What this does not establish

The parked-worker count pertains to that scheduler pool. PsoListLoad chooses
workers through its supplied group owner; resource stepping may use TrySync,
and the loader busy bytes belong to a separate manager. No current evidence
equates every PsoListLoad group with the paused owner+E8 pool. Also the loader
busy poll occurs **before** requesting the scheduler pause. To establish a
global exclusion, show that no other producer can submit a new resource-list
load between that busy observation and the acknowledged pause, or that such
work is joined by the same acknowledgment. This ordering concern is a proof
gap, not an observed native game race.

The general graphics-device destructor2EE0694 goes straight to2EEC4C8;
the display-reset pause above is not inside2EEC4C8. Its outer shutdown owner
must establish its own producer join. Reusing the display-reset argument to
claim destructor exclusion would therefore be invalid.

## Decision and next concrete closure

Serving cache remains NO-GO. A valid **conditional** protocol is now concrete:
retire the memo, preserve the native loader-idle / acknowledged scheduler
pause / graphics drain sequence, perform native reset, bind a new epoch only
after successful initialization, then preserve native resume. It is not valid
to replace that sequence with taking the old+818 critical section: native
reset releases/recreates that lock and preload creates outside it.

The remaining exact identity bridge is from the preload request's supplied
job group (boot1101B90 arg[0], resource3484594 arg2) to the parked scheduler
owner+E8 and its shutdown join. Follow the resource-step/boot-request owner
registration, not another cache hit observer. A serving implementation cannot
manufacture an independent lifetime lease for the raw native context.

`tools/bench/audit_compute_preload_reset.py <private dump>` passes12 pinned
windows,12 altered-byte negative checks and15 exact native instruction
assertions. This verifies the retained evidence only; no native execution,
universal concurrency proof, performance measurement or FPS gain is claimed.
