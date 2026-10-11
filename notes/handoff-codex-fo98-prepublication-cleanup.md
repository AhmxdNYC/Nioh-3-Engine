# FO98 pre-publication resource cleanup - 2026-10-11

Bounded native/decomp trace, main parent d66de6d; experimental 24494d5
unchanged. FO remains NO-GO. Static ordering only; no runtime/performance claim.

## Resource set, rather than a completion wait

12796C lower-bounds record[0] in the scratch pointer array using 127F6C.
If absent, 1279F0 inserts the pointer and increments the count. The returned
iterator/inserted flag does not establish GPU completion. 250650 subsequently
passes this sorted set to the three cleanup paths before 1267A8 publishes
retirement work. Existing owner+CF0/+D00 exclusion still applies locally.

127CDC samples the owner+678 cache count through 1265EC, walks entries with
170D6C and matches entry+8 against the set. On a match it calls 492A04 then
resets the 56-byte entry. 492A04 calls non-null entry+20 object's vtable+28
and vtable+8. These indirect targets are unresolved; do not label +28 a
GPU wait based on cleanup placement. The existing PauseSpin name for 1265EC
is incomplete: it acquires a small atomic lock, reads a count and unlocks.

127B4C scans owner+6A8's 40-byte record ring from +24 to +0C, modulo +8.
Types 0, 6, 9 and 11 bypass cleanup. Native 127BB3/127BB6 supplies both the
set and record+8 pointer to 127EB8; decomp omitted that second argument.
127EB8 is a lower-bound membership test, with no call or wait instruction.
Matched type 2 is zeroed; 5 and 4 become 6 and call resource vtable+8;
10 becomes 11 with the same call. Type 8 marks the indexed owner+7B8
entry+39 and becomes 9. Other handled types clear the record, with optional
slot-object vtable+28 cleanup except type 12. Type 4 also uses that cleanup.
These are CPU bookkeeping/virtual-call edges, not established fence semantics.

127A80 traverses three list heads at owner+608 through node+30. 127AEC
passes node+10, the set, owner+590 and owner+8F0 to 127DC0 when node+18
is nonzero. 127DC0 scans 64-byte records, matches record+10 against the set,
then calls 490B44 and 1282FC. When node+18 reaches zero, 127AEC calls the
existing-symbol GpuMemEvict at 2F11614. That name alone is not GPU proof.
1282FC decomp shows optional record+28 vtable+8 and record+38 vtable+B0,
accounting updates and record reset; concrete targets remain open.

## Verification and limits

py -I re/scripts_renderer/fo98_prepublication_cleanup_provenance.py:
8 new named byte-window SHA256 checks PASS. Reviewed native windows using
Capstone plus existing decomp; no Ghidra or raw dump export. Hash identity
is provenance, not a behavioral or release safety test. Prior suites not rerun.

Next ready unit: resolve record[1] vtable+B8 in 1278D8 and the cache object's
vtable+28 via construction/assignment provenance; trace 490B44/2F11614 only
as needed to establish the actual submission/completion boundary. Then
connect that boundary to producer closure and ring restart. No combined
queue/bounds/lifetime repair, concrete resource-vtable proof or native FO gate.

No local game at entry/checkpoint; no active game test to close or restoration
to perform. No lock, build/deployment, installed ASI/INI/config/save/default
visual/settings/hardware change. Prior restoration retained; controller
untouched; no Claude call/wake. Phone inbox empty. Remote1767/1768/1781
pending at last known bridge1785; no fresh remote query. Serial owner until
exit; unrelated captures untouched.
