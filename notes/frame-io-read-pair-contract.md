# Meshlet read pair: preparation, alias and completion contract

2026-10-11, lightweight existing decomp reads only while root owns native pilot.
No game/process queries, build, native/source mutations or device actions.

## Answer

The successful first readback's **payload is not consumed by MeshletRequestJob
before the second map**. That removes one local dependency, but does not prove
the maps can always be combined. The first map mutates its wrapper and staging
ownership; an aliased wrapper makes the stock second map fail. The manager's
request count is also reread after the first map returns. There is no proven
whole-lifetime/producer exclusion here. A general deferred-map hook is NO-GO.

The useful next candidate is a narrowly guarded *same-job pair preparation*
path, preserving same-frame feedback, stock fallback and both completion/error
contracts. Two40-byte copy records can already be recorded by native5C344;
implementing a new copy queue is unnecessary. Native mutation remains gated.

## Local success/failure ordering actually observed

`D:/nioh3-re/decomp/0x4a2d0.c:124-150`:

1. Read current scene-set+8 wrapper A; map mode0, offset0, size=count*4.
2. If returned CPU pointer A is null, set manager failure bit and recount; do
   not attempt map B.
3. Otherwise read scene-set+10 wrapper B and **reread scene-set+4 count**; map
   mode0, offset0, size=count*4.
4. Only after B succeeds publish pointers into manager+5D8/+5E0 at212-213 and
   process/fan out requests. Between the maps the caller checks A's pointer,
   initializes locals and obtains B/size; it does not dereference A's contents.
5. Worker fanout3E5F8 calls JobGroupWait before returning. Job clears published
   pointers at339-340 and closes B then A at354/364. Entry9811F8 clears manager
   busy byte+5F8 only after the whole job returns.

This is local sequencing, not proof that all external producers honor that
busy byte or that the source count cannot change during the first wait.

## First map has observable side effects even without CPU payload consumption

`0x5b3ac.c` checks wrapper+70==4 (unmapped), performs7F49B4 validation, then
maps wrapper+20 through4D874. On success it writes wrapper+70=mode0,
wrapper+78=offset and+80=size. Thus **A==B is a concrete non-equivalence case**:
stock first map sets+70=0 and stock second map returns0 without reading again.
A naive paired operation that ignores this state could incorrectly succeed.

Distinct wrapper addresses alone are insufficient. Underlying source object
atwrapper+18, resource identity/range and staging resource/offset can alias.
The viewed exports do not prove construction guarantees unique A/B objects or
exclusive ranges. Future eligibility must bind these identities or fall back.

7F49B4 checks device+6EC0 then takes shared SRW device+6EA8 around6BE00C over
device+67A8 registry state. The lock is released before mapping. It is not a
pair-spanning source-lifetime lock.6BE00C checks covered64KiB ranges through
2514DC; its exporter has incomplete calling conventions, so do not reinterpret
that validation as a lifetime pin without actual instruction/ABI proof.

## Read-side operation and lifetime

Mode0 table at3BE29D0 is already bound in `audit_frame_io.py`: read selector1,
close-copy selector0. No copyback optimization is available at these closes.

4D874 chooses62E680.62E918 may reuse or release existing staging according to
wrapper flags/width;62E898 ->4E250 obtains readback staging from either the
existing resource, UploadHeapSuballoc or a dedicated allocation. Allocation
and reuse can mutate allocator state before any new copy is submitted.

62E680 then calls62E9B4 ->62EA08 ->1A807F8 ->5C0D4(count1). Only after that
helper returns successfully does it transfer staging ownership into the map
wrapper and call4D9C4 (resource vtable+40, Map) to expose a CPU pointer. Temporary
allocations/references are cleaned by4E808 and release paths. Map itself can
fail after the copy completed; preserving just the copy result is insufficient.

5C0D4 acquires a copy context7F6200, obtains its list5C5B8 and records through
5C344.5C344 iterates count records, stride0x28, calling list vtable+78 with five
qword fields (CopyBufferRegion shape: destination, destination offset, source,
source offset, bytes). It then applies the existing optional dependency logic,
submits5C268, polls117D6C, and returns the context5C1A4.117D6C advances/reset its
allocator ring only after fence completion. Both staging objects, resource
references and copy-context allocator must survive the combined completion.

Original A's staging already survives while B maps and until job cleanup, so
two CPU-visible staging objects are not inherently new peak occupancy. However,
preparing B while A's copy is incomplete introduces a new pending-copy lifetime
and changes allocation/failure timing. It cannot reuse the existing normal
cleanup path if that path releases A before the GPU finishes.

## Producer ordering not yet closed

MGLRC4B104 binds current scene-set+8/+10 at several paths.5C708 clears both
request buffers for both scene sets on its initialization/update path. Existing
`renderer/13-main-submit.md:152-174` shows modulo2 scene-set reuse and a sync
point for F-2 at frame begin. That establishes a broader protocol, not a direct
proof that **this job's two sources** cannot be rewritten between copies or that
their dependency handles are equal. No copied handle may be invented from
that relationship. An actual two-record call must preserve both producer waits,
or fall back unless they are proven identical/ordered.

## Smallest implementable next gate

Read exact create/reset writers for scene-set+8/+10 and count+4, plus scheduler
join/wait sites guarding busy+5F8; bind the map wrapper ABI and dependency
arguments omitted by the exporter. Prove both objects/ranges and count stable
through preparation. Then model stock sequential versus paired operations,
including A==B, shared-resource ranges, invalid first map, B allocation failure,
Map failure after successful copy, submit failure and delayed fence completion.

Until that closure, an observer can measure same-job pairs/bytes/wait durations
and source identities, but cannot claim eligibility or serve paired copies.
The earlier1.983–3.099% meshlet-worker sample share remains a real attributed
wait observation, not removable frame time or a same-picture acceptance result.
