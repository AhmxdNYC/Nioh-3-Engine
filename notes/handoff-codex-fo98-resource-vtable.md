# FO98 concrete resource vtable - 2026-10-11

Bounded native/decomp trace, main parent b18b055; experimental 24494d5
unchanged. FO remains NO-GO. No runtime, safety or performance verdict.

## Two concrete allocation paths

24FBC5 and 62BAE4 call constructor 254BA4. Both store its returned object
in stack argument six ([rsp+28]) before calling 2509EC at 24FC4B/62BB6D.
2509EC forwards that argument to 251660, which stores it as the optional
second resource pointer. These two paths therefore identify record[1],
subject to the previously traced map-to-retirement flow. This is not an
exhaustive inventory of every allocator or possible vtable replacement.

Native 254BB3 loads vtable RVA 38EB460 and 254BBD assigns it to object+0.
The +B8 slot (38EB518) points to 127A48; +B0 (38EB510) to 128838.
127A48 calls +B0, then, if true and object+1C4 is nonzero, subtracts that
32-bit count from context+7578. 128838 consists only of a comparison of
object+178 against zero, setne and return. Neither establishes a fence or
wait. Native 1278D8 already passes its fourth argument as the context to
+B8. This resolves that indirect edge for these constructed resources.

The same vtable's +28 points to 2F06AB4: it calls 12B0D0 and 152D48, then
compares context+6EB8 with object+190. On change it emits opcode 1010098
and the object pointer into context+3C40 (12 bytes), records the context
value in object+190, and calls 12AF40. This ties this concrete resource
class to a 98 producer, but says nothing yet about producer closure or
GPU completion. Its helpers and caller modes remain to be traced.

Do not substitute this +28 target into 492A04: the cache entry+20 object's
construction/assignment provenance has not been established. That call
supplies only the object argument; this resource method consumes additional
arguments. Cache-object identity is still open.

A separate native review resolves 490B44 as bitmap bookkeeping: it clears
one bit in the buffer at object+8 and updates byte-range extrema at +58/+5C.
It has no call or wait. The old GpuMemEvict symbol at 2F11614 still only
identifies an indirect device+118 call; no completion proof follows from
its name.

## Verification and checkpoint

py -I re/scripts_renderer/fo98_resource_vtable_provenance.py: nine new
named SHA256 byte-window checks PASS. Native windows reviewed with Capstone
and existing decomp; no Ghidra or dump export. Byte identity is provenance,
not a behavioral test. Missing decomp for 128838/490B44 was resolved by
bounded native review. Prior passed suites were not repeated.

No local game at entry/checkpoint, so no active test closure or restoration
needed. No lock, build/deployment, installed ASI/INI/config/save/default
visual/settings/hardware change. Prior restoration retained. Controller
untouched; no Claude call/wake; phone inbox empty. Remote1767/1768/1781
pending at last known bridge1785, with no fresh query. Unrelated captures
untouched. Serial ownership until exit.

Next ready unit: trace 2F06AB4's 12B0D0/152D48/12AF40 helpers and caller
modes into the 98 producer exclusion/closure contract; resolve cache+20
object separately before using its +28 semantics. GPU completion, ring
restart, combined queue/bounds/lifetime repair and native gates remain open.
