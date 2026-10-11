# FO98 concrete allocator submit/reuse - 2026-10-11

Bounded offline native trace, main parent 9d27938; experimental24494d5
unchanged. FO remains NO-GO. No runtime, safety or performance verdict.

## Concrete identity and submission

Constructor2EFA840 installs vtable409A620 at context+588. Initialization
2EFF727 sets allocator+8 (context+590) to the graphics owner and allocator+10
(context+598) to the context itself. This is an embedded interface, not a
pointer loaded from context+588. Vtable slots +10/+18/+28/+30 resolve to
492278/492220/490E50/4F90DC respectively.

ECL token0's +30 target4F90DC loads owner+4270 as queue, calls queue vtable+50
with count1 and the decoded list, then queue vtable+70 with context+6C8 and
the decoded sequence. This establishes execute-then-signal call ordering
for this allocator, not completed execution on return. +28 target490E50
stores sequence+1 to context+6D0 (the next recording stamp).

## Reuse check, wait and reset

+10 target492278 skips when the context is null or the old slot sequence is
at most context+2510. Otherwise it checks context+6C8 via1180A4, then9C9EF0
queries that fence's completed value and CAS-updates context+2510. If the
first check failed, cold tail19B0992 calls7D9E1C with allocator+8 graphics
owner, the old sequence, and a null event. That helper polls OWNER+6D8
through1180A4 with Sleep(0) until completion. Therefore the cold path waits
on a different field than the context fence just checked/signaled.

Do not assume owner+6D8 aliases context+6C8 or has an equivalent ordered
signal stream. Their creation/assignment and shared timeline must be traced
before this becomes a reuse safety proof. No hazard verdict follows solely
from the distinct fields; they may share a proven contract elsewhere.

+18 target492220 calls list vtable+50 with the owner-selected allocator at
owner+4290 + 8*owner+4730 and null initial PSO. It then calls49349C to begin
context state,493464 for optional state helpers, and4933F4 for query setup.
Reset follows the old-slot reuse method in4920E0; it is not a retirement
resource completion proof. The newly submitted list's sequence can differ
from that reused slot's old sequence.

## Verification and checkpoint

`py -I re/scripts_renderer/fo98_allocator_provenance.py`: ten newly reviewed
static byte windows PASS, including constructor/initialization, vtable,
cold wait tail and actual graphics fence wait. Hash checks pin the reviewed
input bytes; they do not establish behavioral safety. Prior passed suites
not repeated. Broad decomp search was cancelled without a result; bounded
native scans resolved the interface instead. No Ghidra/build/game launch.

No local game/test at entry/checkpoint; no lock or restoration action needed.
No installed ASI/INI/config/save/settings/hardware/default visual changes;
prior restoration retained. Controller untouched; no Claude call/wake.
Phone inbox empty. Remote1767/1768/1781 pending at last known bridge1785;
no fresh query. Unrelated captures untouched. Serial ownership until exit.

Next ready unit: trace creation/assignment and signal ordering for graphics
owner+6D8 versus context+6C8, connecting the old-slot cold wait to submit
sequences. Producer closure, resource caller modes, cache+20 identity,
retirement restart and combined FO repair remain open.
