# FO98 native list removal and retirement trace - 2026-10-11

Read-only bounded trace, main parent78dbe23, experimental24494d5 unchanged.
FO remains NO-GO. No production repair, shipping build or performance claim.

## Removal and its out-of-line branches

Decoded252104..2521AF and196CA82..196CAC3 with Capstone from the mapped
native image (raw offset=RVA), without Ghidra. Missing252104 decomp was
resolved with these named windows only. List base is pool+6D0; pointers at
list+0/+8/+10, counts at+1C/+20/+24. All three lists use16-byte records.
The third array/count is the token98 registration list traced previously.

Each list compares its records' first8bytes to the supplied pointer. At the
first match, its cold branch decrements the respective count, copies the
last16-byte record over the matching record, and continues to the next list
(or writes all counts and returns). At most one match is removed per list
per invocation; duplicate entries are not all swept. Last-record self-copy
is permitted. No calls, AddRef, release or reference-count operation occur
inside the complete removal function, including its three cold blocks.
This establishes list mutation semantics, not a duplicate-entry hazard verdict.

## Caller and retirement enqueue

Native250ADC agrees with its existing decomp. It takes pool+CE8 exclusive,
uses250904 to find/remove a24-byte map entry at pool+38, then releases CE8.
On a hit, the returned pair is in stack+20 (first pointer) and+28 (object).
For a nonnull object it holds pool+CF8 exclusive around252104(pool+6D0,
object). This lock matches the consumer's shared registration lock. It
excludes exclusive removal versus shared registration, but shared locking
alone does not serialize simultaneous registration writers.

It then holds pool+D00 exclusive and invokes25208C(pool+890, first pointer,
object, original key). That routine performs a zero-time WaitForSingleObject
on queue+50. On success it atomically advances queue+3C modulo queue+38,
writes a24-byte triple into queue+30, and atomically increments queue+40.
This is a local enqueue trace; capacity/admission, the handle's producer,
publication ordering to its consumer and GPU completion remain unproven.

On enqueue failure250ADC calls2EEF3B0(pool). A true result causes a second
enqueue attempt whose result is not tested here. A false result calls the
first pointer's vtable+10, then, for a nonnull object, lock-xadd decrements
the32-bit count at object+8. If its previous value was1, it invokes that
object's vtable+18. It finally releases D00; a map miss releases CE8.
Virtual-call semantics and2EEF3B0 internals are not established by this unit.
Do not label the path unconditional destruction or completed GPU retirement.

## Consequence and limits

Native removal manipulates existing registered lists; it cannot remove an
object still held only by the mod's deferred FO98 queue. No reference-taking
operation was found in the native token consumer or list removal. These
local observations do not prove absence of external lifetime protection or
an actual use-after-free. A safe repair still requires a lifetime/admission
contract across deferred publication and retirement, rather than assuming
that list deletion or queue locking extends the object's lifetime.

Run py -I re/scripts_renderer/fo98_retirement_provenance.py to verify four
named-window SHA256 identities.4static checks PASS; no runtime or safety
verdict. Prior passed suites were not repeated. This utility reads only the
reviewed windows; no raw dump output/file, secret or installed game change.

Next unit: allocation/capacity of the three ResLists and D20 admission
transitions; queue drain2EEF3B0 and its downstream object/GPU ownership.
Then resolve real emitter caller modes before the combined FO queue repair.

No game at entry/checkpoint; no lock acquired, test started, Ghidra, build,
deployment, ASI/INI/config/save/default-visual/settings/hardware/controller
change. Prior restoration retained. Phone inbox empty. Read-only controller
enabled/working monitor45016/worker45124; serial ownership until exit.
No Claude call/wake. Remote1767/1768/1781 remain pending at last known
bridge1785; no new remote query. Pre-existing captures untouched.
