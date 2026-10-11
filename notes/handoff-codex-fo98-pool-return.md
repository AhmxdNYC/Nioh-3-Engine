# FO98 retirement pool return - 2026-10-11

Bounded offline native trace, main parent94b359f. Experimental24494d5
unchanged. FO remains NO-GO; no runtime or performance claim.

## Pool return

Consumer1A757B8 calls251B5C with RCX=owner+58 and RDX=record[2]
under owner+CE8 exclusive, after record[0] vtable+10 and optional
record[1] release (prior callback note).

251B5C calls251CB4 to find/remove a key from the pool's live map at
pool+30. 251F54/251F88 perform lower-bound search of sorted16-byte
key/value entries and require exact key equality. 251CB4 returns the
matched value at entry+8;251EE4 removes equal-key entries. A missing key
returns zero and251B5C does nothing further. On a hit251B5C clears the
returned descriptor+10 marker, then calls251B88.

251B88 manages free ranges: descriptor+0 is size, +8 start, +18 next;
pool+28 is the free-range head, pool+20 the spare-descriptor head.
It walks by start address, links nonadjacent returned ranges, or adds
sizes when predecessor/successor ranges touch (comparisons use >=).
Consumed descriptors have start/size cleared and are linked into
pool+20. Cold196CA3E installs the first free head;196CA47 merges at
the tail. This is free-range bookkeeping, not a GPU synchronization
operation. No direct or indirect calls occur in the reviewed251B88
hot/cold windows. The full caller/object lifetime contract is unproven.
Do not generalize the >= coalescing branches into a corruption verdict
without allocation invariants.

## Optional object reference and unresolved wait target

532C64 uses lock-xadd to decrement the32-bit count at object+8;
when the resulting count is zero it tail-calls object vtable+18.
This resolves the optional record[1] helper as reference release,
without proving what its virtual target destroys or how references
were acquired. 250904 supplies record[0]/record[1] from the owner+38
24-byte live-map entry+8/+10, then removes that entry. Therefore the
record[0] wait target needs the map's insertion/allocation provenance;
it is not identified by the retirement queue enqueue alone.

## Evidence and next work

`py -I re/scripts_renderer/fo98_pool_return_provenance.py`:
9 new byte-window identity checks PASS. Reviewed native windows using
Capstone; checked250904/532C64 against existing decomp. Missing pool
helper decomp files were handled by bounded disassembly, no Ghidra.
Identity checks pin reviewed bytes, not behavior or release safety.
Prior passed suites not repeated. No game, lock, build or deployment;
installed ASI/INI/config/saves/settings and default visuals untouched.

Next narrow unit: trace insertion into owner+38 live map to identify
record[0]'s concrete vtable+10, then establish CPU/GPU wait semantics.
Producer admission/closure, ring rebuild ordering, ResList capacity,
D20 transitions and emitter caller modes remain open. Combined repair
and native pc2 gates are still required before any FO release.
