# Meshlet paired-read writer and helper closure

2026-10-11. Read-only native investigation, existing decompilation plus bounded
Capstone windows from `re/Nioh3.dump.exe`. No live process inspection, game,
build, hook, payload installation or executable mutation.

## Result

The request-job busy setter, count writer, A/B creation/release sites and exact
read-copy helper ABI are now identified. Both optional dependency arguments to
the stock copy helper are **zero on these map-mode-0 calls**; inventing or copying
per-buffer fence handles is unnecessary. Existing frame-level synchronization
still matters. A native paired-map implementation remains **NO-GO** until
source/staging lifetime, aliasing and failure retirement are modeled and checked.

One earlier claim is corrected: `5C708` does **not** clear GPU request-list
contents. Its calls enter a residency/budget path. Therefore it is not evidence
of a CPU-side overwrite of A/B between the maps.

## Busy flag and scheduling

Let M be MeshletManager (`MeshletSys + 169DD0h`).

| Action | Concrete evidence |
|---|---|
| Initialize M+5F8 to zero | Native `32F8693 xor r12d,r12d`; `32F8718 mov [rsi+5F8],r12b`, with rsi=M |
| Set busy before scheduling | `5A8C4` loads M+1C0 and tests its +3AC bit4; on the job-pool branch, `5A8EF mov esi,1`, `5A8F7 mov byte [rdi+5F8],sil` |
| Attach M to the job | `5A95F mov [r8+38],rdi`; pool flags at job+24 become 42000h; `5A988` calls JobPushToPool with pool42h; `5A996` wakes that pool |
| Clear only after request job returns | `9811F8` calls `4A2D0`, then `981211 mov byte [[job+38]+5F8],0` |
| Wait before GPU request generation | `5DC18` calls `3FD10(M)` before `4B104(M,producerCtx)`, then records the scene-set sync point through `5E370` |

`3FD10` has two distinct modes: when owner+3AC bit4 is set it spins on M+5F8;
otherwise it waits for M+5C8's job flag bit0 to clear, signaling that object's
event as needed. `5A8C4`'s other branch calls `3D0718` on the existing job object.
Thus the busy byte is not a universal lock to test regardless of scheduler mode.

`4BBEC` calls `5A8C4` at the normal end of its scene-buffer update, and on a
missing-scene fallback when M+4EC is nonzero (export lines 164–170 and 1235).
The viewed schedule supports a useful local invariant: normal frame-end MGLRC
recording follows completion of this CPU request job. It does not establish
whole-program exclusion of all teardown, reinitialization or alternate users.

Sources: `D:/nioh3-re/decomp/0x5a8c4.c`, `0x9811f8.c`, `0x3fd10.c`,
`0x5dc18.c`, `0x4bbec.c`, `0x5e370.c`; native locations above independently decoded.

## A/B and count writers

The initializer `32F8670` uses two separate `2EE5358` creation calls per scene
set (`32F8BE4`, `32F8C02`), each with element-size argument4 and flags88h.
Starting from rdi=M+1F0, it stores the first result at `[rdi-8]` (`32F8C07`,
set+8=A), and the second at `[rdi]` (`32F8C0E`, set+10=B), then advances80h.
The loop bound2 is established at `32F87D8`. This proves separate creation
calls, not a universal non-alias guarantee for the underlying resources/ranges.

Release `3302A10` records/waits a stream sync point before clearing M+1C8
(`3302A3E`, `3302A4D`, `3302A52`). The scene-set loop starts from M+238,
calls `1FEB74` for A at `3302AE7` and B at `3302AF9`, then zeros both pointer
slots. This closes the identified release sites; it is not a CPU-job lifetime
proof because the inspected release prefix does not show a join on M+5F8/+5C8.

`MeshletRequestCountGroups 4A85C` computes the count by summing
`(MeshletResGetCount + 31) >> 5` over the selected active entries. It stores
that result at scene-set+4, native `4A8F9 mov [rbp+4],r11d`. Its retained sole
caller is the request job. The job calls it on count-zero/first-map-failure/
second-map-failure paths and after successful processing, **not between the
successful first and second map calls**. Later code also updates parity-indexed
per-resource request offsets, so replacing the routine with a captured count
would change more than map lengths.

Current-set selection remains M+2E0: native initialization `32F98DE` and
per-frame swap `5D31B`. This pass identifies these concrete writers; it does
not claim an exhaustive field-write census across arbitrary alias expressions.

Sources: dump windows around `32F8670`, `3302A10`; existing
`0x4a85c.c`, `0x4a2d0.c`, `0x5d214.c`.

## Correction: 5C708 is residency preparation, not a clear

Native `5C73D` atomically reads each set's count, multiplies it by4, then calls
`1A70258(M,A,*,count*4)` and the same for B. `1A70258` sets offset0, control1,
and final flag0 before calling `5B000`. That function accounts M+488/M+490
against residency budgets and may set current-set flag2, or calls `5B100`.
`5B100` validates the wrapper range, adds wrapper+58, and enters `4A134`.
The latter checks the source range and calls `StreamPageLookupOrInsert` under
the device registry's shared SRW lock. There is no observed zero-fill/copy-clear
operation on this path. It can mutate residency bookkeeping and must not be
treated as a source-lifetime pin either.

Sources: `0x5c708.c`, `0x5b000.c`, `0x4a134.c`; actual instruction windows
`1A70258..1A70277` and `5B100..5B14E`.

## Actual map and copy ABI

The Windows x64 register/stack paths were decoded rather than inferred from
the decompiler's incomplete signatures.

`5B3AC(wrapper, cpuOut, mode, offset, bytes, skipValidation)` takes the first
four arguments in RCX/RDX/R8/R9, with bytes and skipValidation on the stack.
It returns byte count or0, initializes `*cpuOut=0`, checks wrapper+70==4, and
publishes wrapper mode/offset/size only after successful mapping.

At `5B418` it explicitly writes zero to the outgoing eighth argument of
`4D874`; mode0 comes from the request-job call. `4D874` forwards this eighth
argument as `62E680`'s seventh argument (native `4D893..4D8A3`). The latter
reads it at `[rbp+7F]` (`62E797`) and forwards it as `62E9B4` argument5.
That becomes `62EA08` argument6, then `1A807F8` argument3, then `5C0D4` R9.
Therefore the helper's optional sync-point dependency is zero for these calls.

`62EA08` constructs the actual40-byte record:

| Record offset | Value |
|---|---|
| +00 | staging destination resource, argument2 |
| +08 | destination offset0 |
| +10 | original source resource, argument4 |
| +18 | source byte offset, argument5 |
| +20 | byte count, argument7 |

Its third argument is not used in the constructed copy record. It passes
`device+43E8` as the copy-pool owner and a local sentinel pointer separately.
`1A807F8` sets record count1 in R8D, moves incoming argument3 to R9, and
explicitly zeros outgoing argument5 (`1A8080F`). It forwards incoming argument5
as argument6. Consequently both `5C0D4` optional branches are absent here:

    CopyAndWait(poolOwner, records, count=1, syncPoint=0, extraDependency=0, auxiliary)

The auxiliary pointer is only consumed by the extraDependency branch. The
helper nevertheless submits, obtains its own fence value, polls completion,
and returns its context. Its sync-point branch at `1906B50` is real for other
callers and must not be globally removed.

The zero helper dependency does **not** mean producer completion is unnecessary:
the normal job is scheduled after `4BBEC`'s existing current-set sync wait, and
frame-end future producer recording waits for this job. Pairing must preserve
that same source epoch and normal frame protocol.

## Next safe candidate and remaining no-go

The next executable work can be an **offline same-job pair model** using this
actual ABI and five-field records, with dependency0 for the bound mode0 path.
It should accept a pair only after both wrappers are unmapped, A/B and their
underlying resource/range identities are checked, the count remains unchanged,
and temporary staging ownership is explicitly retained until the combined
fence completes. Model first/second allocation failure, first/second Map failure,
submit failure and delayed completion. Preserve stock first-map failure behavior
and fallback before altering observable wrapper ownership.

Do not yet replace native maps: the constructor calls and scheduling evidence
do not prove all aliases, alternate scheduler behavior or teardown exclusion.
A future compare-only observer could capture pair identity/count stability,
both source/staging ranges, job duration and copy waits without changing work.
The old worker-local sample percentages are not frame-time savings.

## Provenance

The retained PE was checked for `PE\0\0` and raw-backed section RVA==file-offset
before the first native decode. The following SHA256 values bind exact bounded
windows, not whole function extents (hot/cold exports may report combined sizes):

| RVA / bytes | SHA256 |
|---|---|
| 5A8C4 / 10Ch | `95b9a3a4f959451bcc25e5ce40472750e561a9d08360d89d1a24b5eb61f92d6f` |
| 32F8670 / 1380h | `fcbce21e7af6b1f5cfa506a45e4c4f9f5356c5bc77cebf718eecf6482e791b69` |
| 3302A10 / 850h | `bd261bdef4db6ee147559573c0ed967d0daa56c75f7a0e4d196adb92aa36805a` |
| 1A807F8 / 27h | `afc11fabdb493e01fa9ebc45521201b8d4d459ced66a33bbca0538a249016bf2` |
| 62EA08 / 5Dh | `57799c7bd82c6e05f215b6ed5720fe44909f202c9ca00e6275804a599fd44194` |
| 5C0D4 / FDh | `901a84e4f1a720f952d167f1c1d13236df90ed1ece03b2b92a2431aa82164554` |

Only this note was added for this task. No exhaustive decompile/rebuild was run.
