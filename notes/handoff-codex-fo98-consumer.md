# FO98 native consumer registration - 2026-10-11

Read-only bounded trace, main parent5609017, experimental24494d5 unchanged.
FO remains NO-GO. No production repair, shipping build or performance claim.

## Verified consumer path

Read the native table at3C02220 +98*8: mapped pointer7FF74FCA1F24,
RVA891F24 (image base7FF74F410000). Capstone5.0.7 decoded only named
windows from re/Nioh3.dump.exe, raw offset=RVA. Missing891F24 decomp was
resolved without Ghidra. No raw dump file or full-image output.

891F24 consumes a header then two pointer dwords, advancing the cursor12bytes.
Header byte2 becomes r8d; byte3 becomes r9b. For native01010098 both are1.
It loads render globals via RIP-relative memory, adds61A0 (PagePool), and
jumps891F80 with (pool, object pointer, byte2, byte3). The handler itself
does not dereference the object or visibly acquire a reference.

891F80 tests pool+D20. When zero,891FFE returns false without appending.
The token handler tail-jumps to this result; cursor advancement already occurred.
No interpretation of D20 or proof that this path is safe for deferred FO tokens.
When nonzero, it takes pool+CF8 shared, copies a16-byte stack record whose
first8bytes are the pointer and next4bytes are byte2 to
[pool+6E0 + count*16], increments pool+6F4, then releases shared.
The last4record bytes are not initialized by this decoded routine; do not
assert byte-exact contents for them. If byte3 is nonzero it increments
pool+928 after unlocking; returns true. Imports resolved independently:
38E2628 KERNEL32.AcquireSRWLockShared;38E2630 ReleaseSRWLockShared.
No visible capacity check, AddRef or ownership transfer call in891F80..892002.
That is a local observation, not proof of overflow or absence of external ownership.

## Downstream object use

pool+6E0 and pool+6F4 match ResList at pool+6D0's array+10 and count+24.
7F6964 iterates that list in16-byte strides, dereferences each object's
vtable and calls slot+90(ctx, fence pointer). 8F1764 likewise calls slot+98(v)
and clears count+24 after iteration. Both agree with their existing decomp.
The native consumer registers work whose object is dereferenced later;
it does not finish the object's use when it reads the token.
Caller/context ordering comes from existing symbol/decomp evidence, not a
new complete PagePool lifetime audit. The remove routine252104 was only
partially decoded and has out-of-window branches; no retirement conclusion.

## Reproduction and limits

Run py -I re/scripts_renderer/fo98_consumer_provenance.py: native table pointer
and four named byte-window hashes,5static provenance checks PASS. These are
identity checks for the reviewed windows, not runtime, safety or equivalence tests.
Windows include trailing bytes; sizes/hashes live in the script. Prior passed
startup, negative-control and epoch suites were not repeated.

Next bounded unit: trace pool+6D0 list removal/retirement (252104,250ADC and
out-of-line branches), allocation/capacity and D20 admission transitions.
Also resolve real caller modes for emitter slots+20/+28. Prove pending-token
and deferred-record ownership before combined queue/space/producer-closure repair.
Do not assume shared locking serializes writers or extend object lifetime.

No game at entry/checkpoint, no game lock/test, Ghidra, installed ASI/INI/config/
save/default visuals/settings/hardware or controller writes. Prior restoration
retained. Phone inbox empty. Read-only controller enabled/working monitor45016,
worker38044; serial ownership until exit. No Claude call/wake. Remote1767/1768/
1781 pending at last known bridge1785; no fresh bridge query. Existing captures
untouched. Topic committed/pushed with Codex attribution; notifier after push.
