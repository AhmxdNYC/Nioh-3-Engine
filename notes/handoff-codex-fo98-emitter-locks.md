# FO emitter lock ownership - 2026-10-11

Read-only static work unit; main parent `af88e7a`, experimental `24494d5`.
Production and experimental source unchanged. **FO remains NO-GO.**

The three missing decomp files were not a blocker: decoded only the named
routine windows from `re/Nioh3.dump.exe` using Capstone 5.0.7, with raw offset
equal to RVA as documented in the brief. Resolved RIP-relative IAT targets
through the PE import descriptors and OriginalFirstThunk names using Python
stdlib. No Ghidra, raw dump output, ASI build, or game launch.

## Verified lock selectors

`12B0D0` and `12AF40` add `0x198` to the object pointer. Each subtracts one
from the low 32 bits of the mode argument; mode1 takes the shared branch,
mode2 takes the exclusive branch, every other value returns without locking.
The acquire/release pairing is exact in these windows:

| Mode | Acquire IAT RVA / import | Release IAT RVA / import |
|---|---|---|
| 1 | 38E2628 / KERNEL32.AcquireSRWLockShared | 38E2630 / KERNEL32.ReleaseSRWLockShared |
| 2 | 38E2618 / KERNEL32.AcquireSRWLockExclusive | 38E2620 / KERNEL32.ReleaseSRWLockExclusive |
| other | none | none |

This verifies selector semantics, not the modes used by real callers.

## Verified emitter sequences

Both emitter entries preserve `r9d` and pass it as the mode to `12B0D0`.
Both call an update with `(obj+C0, obj+10, a3)`, then compare `ctx+6EB8`
with `obj+190`. If different, both write `0x01010098` and the object pointer,
advance `ctx+3C40` by4 then8, and stamp `obj+190` with the loaded epoch.
They pass the saved mode to a tail jump into `12AF40` after the compare/write/stamp.

- `7C50B4`: acquire at7C50D7, update12B0FC at7C50EA, epoch read7C50EF,
  compare7C50F6, token header7C512B, stamp7C5154, release tail7C5118.
- `2F06AB4`: acquire at2F06AD7, update152D48 at2F06AEA, epoch read2F06AEF,
  compare2F06AF6, header2F06B0D, stamp2F06B36, release tail2F06B56.

The second sequence also agrees with existing `D:/nioh3-re/decomp/0x2f06ab4.c`.
The first emitter's compare/write/stamp behavior is now independently checked;
the update functions differ. Their full equivalence is **not** established.

## Consequences and remaining proof

Exclusive mode serializes this object's native update/compare/stamp against
other holders of the same lock. Shared mode allows concurrent shared holders;
other modes establish no synchronization through these helpers. Do not assume
that every emitter call uses exclusive mode or that this object lock protects
the device stream, device epoch, object retirement, or GPU consumption.

Actual Fo98 forwards a4 unchanged. Off-main deferral holds g_98lock, invokes
the original on its fake context, then publishes the captured pointer after
the original returns and releases the object lock. The queue lock therefore
does not extend the native object's lock through publication or consumption.
The main route invokes the original without g_98lock. An exclusive object lock
alone cannot close delayed publication, fake epoch sampling, or retirement.
No new concurrency failure or runtime use-after-free was tested in this unit.

Next bounded unit: locate caller mode values for both vtable slots and the
0x98 consumer; trace PagePool registration/retirement ownership from there.
Record the precise admission/lifetime contract before choosing a combined
queue/space/producer-closure repair. Keep drains outside g_98lock. Existing
startup, negative-control, and epoch suites were not rerun without changes.

## Reproducible byte provenance

Read each below window directly at its RVA; SHA256 covers only those bytes,
not the full executable. Helper windows exclude the trailing INT3 padding.

| RVA | Bytes | SHA256 |
|---|---:|---|
| 12B0D0 | 43 | 71d32cf9155ca1525e758eb105d207c11d6c0be473e3bc6644b8af84f6d08e5d |
| 12AF40 | 43 | cd5b8d45b2a8ff70ba2bba5b0ac94857e39bb1ead54a03488e922029f6cba4f9 |
| 7C50B4 | 169 | 18af3221cdb4cef1f684792e8ad6523d3e4cc5fc3b72875374515f8a792d47b5 |
| 2F06AB4 | 167 | 31b68063354eb3c623c94fdbf1a56d0d9bf06c0d2f97883e614fae7cf8f29687 |

PE mapped image base7FF74F410000; import directory RVA44B0488. Initial
pefile attempt failed because that optional package is absent; stdlib import
resolution succeeded without installing packages. Static findings only;
no release validation, output-equivalence or FPS claim.

No game at entry/checkpoint, no game test or game lock acquired, no installed
ASI/INI/config/save/default visual/settings/hardware changes. Prior restoration
retained. Phone inbox empty. Controller read-only enabled/working,
monitor45016/worker29784; serial ownership until exit. No controller write,
Claude call/wake, or fresh bridge query. Remote1767/1768/1781 remain pending
at last known bridge1785. Existing untracked captures left untouched.
