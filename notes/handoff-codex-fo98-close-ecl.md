# FO98 close/recycle and ECL completion - 2026-10-11

Bounded offline native trace, main parent 5059eb1; experimental24494d5
unchanged. FO remains NO-GO. No runtime, lifetime, safety or performance verdict.

1A7C4FC supplies context+588 as allocator and context+5A0 as ring, then
tail-jumps to 490F64. That wrapper calls 490D5C with flag0, saves its RAX,
calls 4920E0 flush/recycle, and returns the saved close result. Therefore
the dispatcher's close call includes recycling before its later 4930C8 call.

490D5C calls allocator vtable+20, list vtable+58 with zero, and list
vtable+48. A negative result takes the diagnostic path and returns zero;
a missing ring list array also returns zero. Success increments ring+30,
stores that sequence in the current 24-byte slot+8, writes an ECL token
through 490E9C, and stores the resulting stream end in slot+10.
Native R9 supplies the sequence to the token writer; the decomp's displayed
three-argument call omits it. Allocator vtable+28 receives that sequence
before the ring indices advance. This is sequence assignment, not observed
GPU fence completion.

490E9C writes token0 followed by list pointer and sequence (20 bytes).
ECL table0 resolves to 4F90A0, which decodes those fields and tail-calls
the ECL item's allocator vtable+30 with list and sequence. It does not
directly invoke the separate token6 ExecuteCommandLists handler. Concrete
allocator identity and +30 implementation remain open; do not silently
replace this virtual dispatch with a proven GPU submit/signal contract.

4920E0 selects the NEXT/current ring slot after close advanced ring+20.
If its old sequence is newer than submitted+38 and pending indices are
valid, it publishes pending tokens through that sequence using 86F760.
It then calls allocator vtable+10 with that slot's OLD sequence, clears
slot+8, and tail-calls allocator vtable+18 with its list. These virtual
calls are the next concrete targets for reuse/fence proof. Any wait there
has not been decoded; a wait for an old reuse slot would not establish
completion of every newly closed list or unrelated retirement resource.

Native ECL consumer1A75970 picks the slot at completed index ring+18,
calls 4F9054 to replay it, then calls 2526E4. Replay dispatches each token
synchronously until its end cursor. ItemDone increments completed+18,
releases capacity semaphore+8, and decrements pending+1C. These are CPU
token-consumption/capacity boundaries; GPU completion does not follow from
the counter alone. Separate token6 calls queue vtable+50 with one list;
token7 tail-calls queue vtable+70 with fence/value. Their call return is
not a decoded fence-completion check. Their presence in the table does
not prove they occur in the token0 virtual path.

## Verification and checkpoint

`py -I re/scripts_renderer/fo98_close_ecl_provenance.py`: 12 newly reviewed
static byte windows PASS, including table targets. This pins provenance,
not behavioral safety. Prior passed suites not repeated. No build/test
launch justified for this documentation/static evidence unit.

No local game/test or Ghidra at entry/checkpoint; no game lock needed for
offline reads. No installed ASI/INI/config/save/settings/hardware/default
visual changes; prior restoration retained. Controller untouched; no
Claude call/wake. Phone inbox empty. Remote1767/1768/1781 pending at last
known bridge1785; no fresh query. Unrelated captures untouched.

Next ready unit: identify context+588 concrete allocator vtable and trace
+10/+18 reuse and +30 submit/signal into fence ordering. Producer closure,
resource caller modes, cache+20 identity, retirement restart and combined
FO repair remain open.
