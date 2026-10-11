# Native mission queue consumer: bounded proof

2026-10-11, Codex. Existing decompilation exports and symbols only; no game,
Ghidra, native hook, build or source edit. Export directory:
`D:/nioh3-re/decomp`. RVAs identify the known dump, not a new build contract.

## Result

There is a concrete **request-applied-to-mission-state** witness stronger
than an empty queue. It happens early in loading and does not establish
completion, cancellation, or absence of a future request. The current exports
also expose a promising native head-retirement path, but its copy/shift
helpers and control flag provenance are missing. Do not weaken uncertain
admission quarantine from these findings alone.

## Proven from the inspected exports

| Native path | Evidence | Meaning and boundary |
| --- | --- | --- |
| `0x223953C` enqueue | `0x223953c.c:18-34` scans three 0x58-byte slots for mission+4=0 and byte+8=0, then calls `0x2233DA0` and `0x2239C94` | One selected slot on this normal path; exception and concurrent producer closure not established. |
| `0x2239C94` post-enqueue | `0x2239c94.c:27-41` sets request+38/+3C/+3D from online/session state | This is session decoration, **not** a completion acknowledgement or cancellation token. |
| `0x5A29F0` loader dispatcher | `0x5a29f0.c` dispatches controller+4 states3/4/5 to `223B3F8`/`223B8F0`/`223AF50` | A separate native controller owns the load progression. Add-on Idle is unrelated. |
| `0x223B3F8` state3 | `0x223b3f8.c:45-51`: queue head+0 -> mission state+10 journey; head+14 -> state+24 mode; head+4 -> state+28 mission; head+C -> state+14 retry | This function applies the **head request** to the current mission tuple. Those fields can change before geometry/actors are ready. |
| same state3 exit | `0x223b3f8.c:184-185` writes controller+4=4 and global state=4 | Successful passage through the body, not completed loading. |
| `0x223B8F0` state4 | `0x223b8f0.c:77-87` looks up the head start point and gets coordinates; `:125-145` performs cell/stream setup; `:213-214` selects state5 | Streaming uses the head's start point after mission-state mutation. |
| `0x223AF50` state5 | `0x223af50.c:39-45` requires a resource predicate then calls original `LoadIsLoadEnd`; `:177-179` sets state6 | A loader readiness milestone; the dispatcher still has states6+ afterward. Never call LoadIsLoadEnd an extra time to observe it. |
| `0x223C53C` state6 | `0x223c53c.c:18-27` waits on another predicate, performs actions, selects state7 | State6 is not the end of the controller. |

The published `symbols.tsv` wording that MissionLoadStep "consumes" the queue
must be read in this limited sense. Its inspected body still reads the head
throughout and does not clear the three-slot queue. A mission tuple plus
loading-down sample is not a native retirement receipt.

An observer at the **original return** of state3, copying head and applied
tuple, could establish that the observed head was applied during that call.
An entry-only hook cannot claim the writes completed. A later pump snapshot
of equal tuples is weaker: other calls/loads may have intervened and identical
requests have no proved unique token. This is a candidate witness contract,
not a proposed new hook installation in this task.

## Candidate retirement path and precise missing edges

`0x223AC50` eventually sets controller state `0x13` (`0x223ac50.c:103-104`).
Dispatcher state13 invokes `0x223AE90`. Once its accumulated timer reaches
30.0, that routine conditionally invokes `0x223568C` **only when controller
byte+0x11 is nonzero**, clears that byte, and later selects state14
(`0x223ae90.c:16-40`). The timer units/scale are not proven here; do not turn
30.0 into a wall-clock cancellation deadline.

`0x223568C` has one exported caller, `223AE90`. Its complete 91-byte export
(`0x223568c.c:17-22`) zeroes a temporary 0x58-byte record, calls `0xA74400`,
calls `0x2233D28` with queue head and returned temporary, then calls
`0x10D5B90` with queue/head+0x58/head+0x160 addresses. This **suggests** head
replacement and queue advancement. The end pointer `0x45BC580` is head+0x160,
four 0x58 strides from head; it is not the three-slot end head+0x108.
Do not silently reinterpret it as a three-slot memmove without the helper.

These exports are absent in the current decomp directory:

- `0x2233D28`: destination mutation/copy semantics.
- `0xA74400`: temporary normalization/default record construction.
- `0x10D5B90`: range direction/count and final slot behavior.
- `0x5A2A90`: state14 transition/next-request processing.
- `0x223C6B8`: state16 branch (hex16, dispatcher decimal22).

Also unresolved: writer/meaning of controller+0x11, its controller allocation
and identity, all bypass/exception/reset paths, and complete queue producers.
No request sequence/unique identity was found in the inspected path. A return
from `223568C` cannot yet support a proof of cancellation or "nothing can load
later." Its entry could precede partial mutation, just like the admission call.

The most useful next static investigation is those five missing bodies plus
the +0x11 writer, using bounded native disassembly when the lead authorizes
expanding beyond this task's existing-export-only scope. If they establish a
retirement boundary, bind an original-call completion event to the observed
head identity and uninterrupted transaction history. Then separately prove
no alternate pending copy remains. Until then, ordinary exact-matched replay
tracking remains operational evidence; uncertain admission requires the
existing conservative latch.

## Useful adjacent admission fact: Emaki

`0x10dbed0.c:139-159` independently writes selected request mission+4=`0xCC96`,
resolved start+C, eight-byte mode+14=`2`, key+20..28, carrier reference+30,
and params+40..57. Its free-slot scan is at `:119-134`. This supports an
Emaki admission witness stronger than changed queue hash or mission alone:
changed previously free slot, mode2, expected key and carrier reference.
Requested start zero is resolved from data before enqueue and must not be
compared against literal zero. Sent this evidence to the admission agent.

These are decompilation-based facts, not new independent instruction-window
verification or native runtime acceptance. No cancellation call is proposed.

## Follow-up: bounded native bytes close the local retirement operation

Lead subsequently authorized Capstone inspection of exact missing targets in
the inert `re/Nioh3.dump.exe`. PE section RVA/raw equality was asserted;
targets were checked inside executable section bounds. `.pdata` bounded the
nonleaf routines. Two leaf routines have no covering unwind entry: inspected
their exact referenced entry through straight-line `ret`, within the next
unwind-entry boundary and a 256-byte cap. No branch/call occurs in those leaves.

An initial assumption that each `.pdata` entry covered a whole routine was
rejected: reversal helper has three contiguous unwind fragments. All three
were inspected, following its direct branch/fallthrough, not just the first.

| Inspected half-open RVA range | Bound source | SHA256 |
| --- | --- | --- |
| `2233D28..2233D9D` | straight-line leaf through ret | `2515d67fd92d8c57af63098e2b0a6f61f84c3730bb581973ed7a61cd18f78773` |
| `A74400..A74458` | straight-line leaf through ret | `8b9dbee58c6cbd5c2579e6cb6f488430937c22b68288a8750978827c93a5cab1` |
| `10D5B90..10D5C29` | .pdata | `1fafd6a07fafa7a34a0fb2afdcf92f5268ada4afe331a9bfca2fbfb8dc296eb8` |
| `10D59F0..10D5B87` | contiguous .pdata fragments ending59FD/5B82/5B87 | `5edc8af8dcc14a3e4cbae910241134eae6556a701e466b825fa181bf1fecd896` |
| `223568C..22356E7` | .pdata | `133c9d9dccdc1f99daf014000efdd09b77ce2d586925786f11fef54de2e9aa66` |
| `5A2A90..5A2C1E` | .pdata | `3116287bd4f20e0ef50be34d85c330d4c63ba8223242dc2ed8a96347b56fa5cb` |
| `223C6B8..223CBEE` | .pdata | `0dcd5fa55bbe97b6a320181b8bfbeec5b288e30127aba5068fa44eec7f15d181` |

Concrete operation:

1. `A74400` constructs an empty/default request: mission+4=0, byte+8=0,
   start+C=0, mode+14=0, journey+0=1, carrier+30=-1 and default parameters.
   Thus "empty" does not mean all zero bytes.
2. `2233D28` copies the request's semantic fields. It does not copy padding.
   Covered fields are +0 byte, +4 dword, +8 byte, +C dword, +10 word,
   +14/+18 dwords, +1C byte, +20 qword, +28 dword, +30 qword, +38 dword,
   +3C/+3D bytes, +40 sixteen bytes and +50 eight bytes. It returns destination
   in RAX and leaves RCX intact.
3. `223568C:22356B5..22356DA` supplies actual addresses head=`45BC420`,
   middle=`45BC478`, end=`45BC580` to `10D5B90`. The fourth stride is real.
4. `10D5B90` invokes `10D59F0(first,middle)`, `(middle,last)`, `(first,last)`
   at `10D5BD8`, `10D5BE3`, `10D5BEE`. `10D59F0` swaps semantic fields of
   the outer pair, moves inward in 0x58 strides, and returns; it has no calls.
   Therefore this is a left rotation by one of **four** request records.

For semantic record values `[A,B,C,D]`, normal completion of `223568C`
produces `[B,C,D,empty]`: A was overwritten before the rotation. No archive
or extra pending copy is created by this local helper chain. Do not compare
all padding bytes to prove this; use the semantic field mask above.

This is **head retirement**, not a cancellation API: invoking it independently
would discard whatever head is current and advance unrelated requests. No
such invocation is authorized or proposed.

### Control-flow facts closed by the same inspection

`223C6B8` is the dispatcher state `0x16` path. It can return early when the
character-manager state+58 is nonzero (`223C6F9..223C6FC`). On its main path,
`223C7AB` writes controller+11=1; after substantial native setup it sets the
loading stop flag and controller/global state1 (`223CBA3..223CBC3`). Thus
this flag is armed as a new native transition starts, not merely after a
request has finished. Entry to this function does not prove that it passed
the early return or reached state1.

Native bytes at `223AED6..223AEE1` confirm the state13 path calls retirement
only when +11 is nonzero and clears the flag **after** the call returns.
The following state14 writes occur later at `223AF3A..223AF42`. Therefore
controller+11=0 alone can mean "never armed" as well as "retirement returned."

`5A2A90` (state14) performs other work, then checks that controller state is
still14 and head mission/byte is nonempty (`5A2BDF..5A2BF5`). It then calls
`223A504` with predicate `223CC40` (`5A2BF7..5A2C0E`), permitting another
queued request to start. It also has an earlier conditional transition via
`5A2C5C`; that helper branches into cold block `19DBBF2`, not inspected here.
Consequently sampling state14 does not guarantee the next request stays idle.

### What this enables, and the remaining release condition

There is now a precise candidate **original-call completion receipt**:
observe `223568C` before and after its original call, copying all four records
with the semantic field mask, and require exact
`[A,B,C,D] -> [B,C,D,empty]`. With a prior matched attempt at the head and
uninterrupted/loss-free ownership history, this proves that observed head
was retired from this queue by the native. It is stronger than a pump snapshot
of an empty slot and needs neither calling nor modifying the loader.

This still does not automatically recover an attempt whose admission was
never witnessed. Identical mission/start/mode tuples have no native unique
token. Enqueue helper's normal index<3 check was reverified at
`2239575..223957C`, while rotation reads four records; provenance/initialization
and all writers of the fourth record remain unclosed. Other producers,
exceptions between record writes, session changes and observer event loss can
break attribution. A retirement entry-only hook or the post-call flag without
its matching pre-call record is insufficient.

Next implementation candidate, after independent hook-site review: a bounded
private original-return receipt for this retire operation, paired with exact
admission and source-to-head tracking across earlier rotations. Require
known offline session, no observation gaps, semantic queue-transition match,
destination lifecycle and restoration before allowing another training run.
If any association is uncertain, preserve quarantine. This narrows the proof
gap substantially but does **not** yet justify clearing arbitrary uncertain
admission in the current production code.
