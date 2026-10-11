# FO98 live-map ownership - 2026-10-11

Bounded static trace, main parent32b8109; experimental24494d5 unchanged.
FO remains NO-GO. No runtime, GPU ordering or performance verdict.

## Map insertion and paired references

2509EC allocates a pool range under owner+CE8 exclusive, then calls
251660(owner+38, allocated-key, argument5, argument6). Native registers
and stack slots confirm argument5/6 are the two stored object pointers;
the earlier decomp omits these arguments in several allocation callers.

251660 compares map count+8 with capacity+10 and returns false if equal.
Otherwise it searches the sorted 24-byte map by key (251900), inserts
through2517B4 on a miss, and stores argument3 at entry+8 and argument4
at entry+10. It calls argument3 vtable+8 with RCX=argument3; a non-null
argument4 receives lock-inc of its32-bit reference count at object+8.
An existing-key path also writes and retains those pointers; uniqueness
and replacement invariants are not proven by this bounded trace.

The existing250904 removal transfers both pointers without a decrement.
Consumer1A757B8 calls record[0] vtable+10 and releases record[1] through
532C64 before returning the range through251B5C. Together this establishes
paired virtual retain/release-shaped operations for record[0] and explicit
reference acquisition/release for optional record[1]. The concrete virtual
implementation and resource type are still unidentified.

2F03E80 provides an explicit caller example: [param4] becomes argument5
and argument6 is zero; the resulting allocation goes into its descriptor.
The texture allocation callers24F408/62B4E4 supply local resource pointers
and engine wrapper pointers in their outgoing stack slots, but their
concrete resource factory and vtable identity require another trace.

## Correction and limits

Earlier notes called record[0] vtable+10 a wait target or GPU-wait gate.
That description was a hypothesis, not evidence. Insertion's vtable+8
and retirement's vtable+10 support reference retention/release instead;
do not count the +10 call as a CPU or GPU synchronization operation.
A COM AddRef/Release interpretation is plausible but remains an inference
until the object factory/interface or concrete vtable is identified.
Even a confirmed release would not establish safe GPU retirement timing.

`py -I re/scripts_renderer/fo98_live_map_provenance.py`: three new byte
identity checks PASS, pinning the insertion, allocator publication and
explicit caller windows reviewed with Capstone. These are source evidence
checks, not behavioral or release safety tests. Prior passed suites were
not repeated. No game, Ghidra, build, deployment or installed state change.

Next narrow unit: identify how retirement batches are admitted after GPU
completion, starting2EEF3B0 ->2EEE310/2EF01F4 ->2EEF950 and callback dispatch.
Use this to locate the real completion boundary before planning producer
closure/ring rebuild. Concrete resource vtable, replacement invariants,
ResList capacity, D20 transitions and emitter caller modes remain open.
