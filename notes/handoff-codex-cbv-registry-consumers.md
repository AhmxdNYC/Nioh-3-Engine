# CBV registry consumers: bounded discovery

2026-10-11, OpenAI Codex. Offline inert-image provenance only.

Two additional native consumers load global RVA `4BD0A38` and test it:
`61D2F1` loads R15 and branches nonnull to `19E97E2`; `A08D6C` loads R12
and branches nonnull to `1A5D117`. This proves optional-path admission at
these sites, not registry initialization or a concrete virtual implementation.

Discovery of the first nonnull branch shows an allocation sized from eight
stage counts, with a 0x40-byte pointer header and eight-byte handles. The
pinned window `19E9876..19E9902` builds array pointers at header offsets
0,8,10,18,20,28,30,38 (hex), passes the registry in RCX, and calls `3032CFC`.
The boundary stops before proving that helper's behavior or connecting its
returned records to the renderer's bound layout. Array-pointer header fields
must not be confused with layout fields +38/+40/+44.

The SHA256-pinned `.text` interval `1000..38E128B` was scanned for single-REX
MOV/LEA RIP-relative byte forms, allowing overlapping candidates. There are
21 candidates targeting the registry and one matching store at `30345E3`,
the already-known setter. No matching address-taking candidate targets the
setter `30345DC`. These are limited byte-form results: instruction-boundary
validation is pinned only for the new consumer windows. Other instructions,
additional prefixes, indirect/address arithmetic, non-text code and runtime
writes are outside this census. This is not an exhaustive xref or writer proof.

An absolute 64-bit pointer search of the inert image found no occurrence of
its snapshot image-base plus either target. Snapshot global content was zero.
Those observations cannot establish runtime initialization, object identity,
registry availability or unreachability; heap objects are not in this image.

Validation: `py -I -B tools/bench/audit_cbv_registry_consumers.py` PASS:
pinned text census, three native windows (178 bytes),17 instruction checks,
truncated/corrupted memory-only rejection, signed-negative displacement,
adjacent-instruction, unrelated-target and truncated-instruction scanner
controls. `git diff --check` PASS. No prior passed suites repeated.

Next useful unit: trace `3032CFC` registration/virtual calls, then resolve the
concrete registry implementation and the returned-object/bound-layout link.
Positive declared size or independent generation invalidation is still open.
No native defect, safety, performance or visual-equivalence claim. Stable
1.7.61, guard PR1 unmerged and FO NO-GO retained. No game/capture/Ghidra,
build/deployment or restoration; other writers and device-pinned inputs,
settings/hardware/controller/holds and Claude wake state untouched. Checkpoint
phone inbox empty.
