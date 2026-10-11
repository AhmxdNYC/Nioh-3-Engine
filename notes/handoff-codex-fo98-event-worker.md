# FO98 event and worker protocol - 2026-10-11

Bounded offline native trace, main parent c38358f. Experimental24494d5
unchanged; FO remains NO-GO. No production fix or performance claim.

## Wait target and worker initialization

2EF00F8 creates queue+8 via2EEE73C(manualReset=0,initialState=0),
then queue+10 via2EEE73C(manualReset=1,initialState=0). The helper
resolves to CreateEventA at IAT38E25C8. Thus queue+10 is a manual-reset
event, not the worker thread handle. Queue+0 receives the handle returned
by2EEE880. Queue+20 stores the context argument; queue+28 stores the
callback from initialization descriptor+0.

2EEE880 passes entry1A71FDC and queue context to BDBAE4,
with creation flags4; it then invokes SetThreadPriority (IAT38E2460)
and ResumeThread (38E2470). The internal creation wrapper is not traced
here; no claim about its full failure contract.

Native1A71FDC waits on queue+8 with INFINITE. When queue+1C is zero,
it invokes queue+28 with RCX=context(queue+20), RDX=queue+18, then
SetEvent(queue+10) and loops. Nonzero queue+1C exits before callback.
The decompiler omitted the second callback argument; native instructions
1A72001/05/08 establish it. Completion-event signaling follows callback
return; it does not itself prove GPU completion or producer closure.

## Atomic flags and wait protocol

1266FC uses a lock-cmpxchg retry loop to OR (1 << EDX) into *RCX.
126A14 similarly ANDs its complement. Neither waits or takes a queue
lock.2EF08E0 sets bit1 of queue+18 before waiting on queue+10, then
clears bit1 afterward. It conditionally calls126984 when its initial
atomic sample has bit0 set.

126984 probes queue+10 and queue+8 with zero-time WaitForSingleObject,
returning true only for WAIT_OBJECT_0 through1269B8. If completion is
signaled and wake is not,1269D4 clears bit0, ResetEvent(queue+10), then
SetEvent(queue+8). Since queue+8 is auto-reset, the zero-time probe may
consume a signaled wake event; it must not be modeled as a pure read.
The specific synchronization contract and callback use of bits0/1 are
still open. Do not infer that a signaled completion means all producers
closed, pending triples drained or their GPU work retired.

## Verification and next work

`py -I re/scripts_renderer/fo98_event_worker_provenance.py`: eight new
static byte-identity checks PASS. These pin reviewed code windows, not
runtime/lifetime/safety proof. Import names independently resolved from
the mapped PE import directory with Python struct; pefile was unavailable
and was not installed. No Ghidra, raw dump output/file or passed-suite rerun.

Next: follow2EEF6D4's descriptor/context into2EEFEB0, identify the
retirement callback and its queue+18 bit contract, then prove consumer/GPU
ordering. ResList capacity, D20 transitions and emitter caller modes remain
open before the combined FO repair.

No local game at entry/checkpoint, no game lock/test/build/deployment or
installed ASI/INI/config/save/visuals/settings/hardware change. Prior
restoration retained; captures untouched. Phone1802 acknowledged; no new
steering at checkpoint. Controller untouched; no Claude call/wake.
Remote1767/1768/1781 pending at last known bridge1785; no fresh query.
Serial ownership until exit. Commit/push with Codex attribution and Discord
result through existing notifier after push.
