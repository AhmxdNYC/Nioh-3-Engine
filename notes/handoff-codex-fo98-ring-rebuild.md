# FO98 retirement ring rebuild - 2026-10-11

Bounded offline trace, main parent3d27386; experimental24494d5 unchanged.
FO remains NO-GO. No production repair, runtime test or performance claim.

## Admission is capacity growth

2EEF3B0 is called by250ADC with owner+D00 exclusive already held after
capacity enqueue fails. If owner+1C (growth step) is nonzero, it computes
min(owner+18 maximum, queue+38 capacity + growth step), using unsigned
32-bit arithmetic. It does not itself check overflow or strict growth.
2EEE310 requests count*24 bytes from owner[0] vtable+38, with alignment8
and descriptor {5,0}. Its decomp incorrectly declares a void return;
native RAX is the allocation result used by2EEF3B0.

After successful allocation,2EEF3B0 calls2EF08B8(queue), which wakes the
work semaphore and waits via2EF08E0 for the worker completion event.
That event establishes the previously traced CPU callback completion,
not a GPU fence. The callback/worker protocol must still be related to
GPU submission before safe resource retirement can be claimed.

## Rebuild helper2EF01F4 (418 bytes)

The helper saves old capacity/work handles (queue+50/+58), atomically
samples queue+40 pending count, and copies that many24-byte records
into stack scratch in order (queue+44 + i) modulo old queue+38.
It does not release resource references during these copies.

It creates a capacity semaphore with initial=newCapacity-pending,
max=newCapacity, then a work semaphore with initial0,
max=newCapacity+1. Both calls use the same IAT slot38E25E8 as the
CreateSemaphoreA calls in2EEFEB0. CloseHandle uses shared slot38E2390.
On success it closes both old handles, copies scratch records to the
new buffer beginning at index0, sets queue+44/+48=0, publishes the
new buffer/capacity at+30/+38 and sets queue+3C/+40=pending.
It returns true. Resetting the consumer index to0 therefore agrees
locally with the callback's initial index0. Restart/work publication
and every producer's exclusion remain to be proved.

If capacity creation fails, the old+50 handle is restored and false
returned. If work creation fails, the newly created capacity handle
is closed, old+58 restored, then the common failure block2EF02E3
restores old+50 before returning false. Both saved handles survive.
An initial closed-handle suspicion was corrected by following this jump;
there is no such finding. No API failure was triggered at runtime.

2EEF3B0 frees the fresh buffer via owner[0] vtable+58 on rebuild failure.
On success2EEF950 calls that same allocator slot on owner+10 (old ring
buffer), then stores the fresh buffer there. The growth caller retries
enqueue without checking the retry result, as previously recorded.
No concrete allocator vtable target or GPU completion was established.

## Evidence and next unit

py -I re/scripts_renderer/fo98_ring_rebuild_provenance.py:
4 new named-window SHA256 identity checks PASS. Windows reviewed with
Capstone and existing decomp; no Ghidra or raw dump output/file. These
pin static provenance only, not behavioral or release safety gates.
Prior passed suites were not repeated. Optional pefile import lookup was
unavailable; shared IAT slots were instead checked against decomp/native
2EEFEB0, without installing dependencies.

Next: trace scheduler2EF0938 and callers that publish queue+48/work
semaphore tokens, connecting pending+40, worker restart and actual GPU
completion; then establish producer closure for the FO queue repair.
Concrete resource vtable, live-map replacement, ResList capacity, D20
transitions and emitter caller modes remain open.

No local game at entry/checkpoint; no test/lock, build, deployment,
installed ASI/INI/config/save/default-visual/settings/hardware change.
Prior restoration retained; controller untouched; no Claude call/wake.
Phone inbox empty. Remote1767/1768/1781 pending at last known bridge1785;
no fresh remote query. Serial ownership until exit; captures untouched.
