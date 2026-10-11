# CBV reuse: native reset identity

2026-10-11, OpenAI Codex, bounded inert native/C read; no test/compiler/source
edits, game, Ghidra, bridge, main or public operations. Complements the separate
renderer_coverage adversarial harness; its synthetic recreation is not native
reachability evidence.

## Verified normal reset forces a miss

Production `cbvupload.cpp:36-46,279-318` reads exactly C+6D0 tag, C+580 list,
C+940 CPU staging descriptor base, and per-slot C+F30 native upload sizes.
CBT also compares table n/root; it does not compare C+9A8 shader-visible GPU
heap base. `Find(C)` retains records by raw C address; harness-only TestReset
is not a native destruction hook.

This C+6D0 is the previously proved primary list-ring stamp, not a similar
unrelated tag. Native490DC3 increments L+30 when closing a list;490E0B passes
that closed sequence to allocator+28. Target490E50 increments the argument
and490E57 stores it to C+6D0. Normal reset's2F00A74 ->2F01108 ->496078
closes the current list before resource teardown. Ring teardown/reinit retains
L+30;2EFF7B2 reads it and2EFF7C1 stores L+30+1 to C+6D0 after opening the new
list. Thus ordinary successful reset advances beyond the old open-list tag.
Both CBN and CBT tag equality fail for prior records even if list/staging
addresses repeat. This uses normal serialized lifetime and no uint64 wrap;
producer exclusion remains separate. The null-current cleanup waits on the
submitted prefix as documented in the owned reset-boundary handoff.

## Verified resource recreation and fresh-construction barriers

- Initializer2EFF484 calls2EF3310 on C+B28 to recreate/map the upload ring.
  2EFF52C calls2EF3944 on C+920 for CPU staging descriptors. The helper first
  releases the old wrapper (2EF396F ->2EF47B4), creates the heap via device+70
  at2EF39AF, and stores CPU start at wrapper+20 (C+940) at2EF39EE.
- 2EFF587 calls2EFE5D0;2EFE605 selects C+980,2EFE645 calls2EF3260, which
  creates a shader-visible descriptor heap through2EF3944. Its GPU start is
  stored at wrapper+28 (C+9A8),2EF3A08. Descriptor stamps/cursor are reset by
  2EF3260. These reviewed creation paths run within initialization, before the
  final C+6D0 assignment, rather than proving a same-open-list heap replacement.
- Fresh context construction calls2EFA770, which zeroes L+30/L+38 at
  2EFA791/795. Its first initialized tag can therefore be1; monotonicity across
  destruction plus fresh construction at the same raw address is not proved.
- Initialization2EFF491..2EFF4CB zeroes all eight stages' B70 dirty masks,
  B90 copied-table sizes, and all F30 native upload sizes. Layout-dirty helper
  114B00 sets bit31 when new n exceeds B90; for each declared entry it compares
  F30 with declared size at114B80/88 and marks its slot dirty at114BAA if larger.
  A first positive-size dirty CBV cannot pass production CBN's native-size
  equality against fresh zero, so its bit remains set and CBT also misses.
  This closes that first-positive-upload case even if ancient tag/list/staging
  values and retained cache bytes repeat.

## Explicit remaining boundary

Native118DA8 invokes the wrapper only for nonzero B70 and nonzero layout n.
But bit31 alone satisfies that caller and is ignored by CBT's maskWould test.
Fresh construction's sentinel-only table case therefore remains conditional:
all first active CBV layouts must be shown to contain a positive-size dirty
entry, or an independent invalidation must precede serving the old table.
The native reads do not prove such a layout reachable, actual repeated COM/heap
addresses, or same-open-list heap replacement. No native defect is demonstrated.
Normal reset's changed-tag proof is closed; whole-lifetime address reuse is not.

## Provenance

Inert exports `D:/nioh3-re/decomp/0x<rva>.c`; native `re/Nioh3.dump.exe` flat
RVA windows. SHA256, RVA/hex-byte-count:

```text
490DC3/15  d75711f132b9bdbeb28cc43bebd86fe577bf64d8537f63d8f47c087577414433
490E05/D   262f3a0bb341b41d8afb9e1a64ee1e3ec9194e0ddff7f7928f858e4644e46bbc
490E50/F   115260c06b8595eaf37c8b142de5507b43c4c945de46febd60686891f64cbb02
2EFF796/32 0b78ca4442f9344298ba5b94569ddfa168b5cda9e8e9ac0f1282f36e20dd7040
2EFF491/4D 0630c55fbadd8df62a8560a01eb30d8c094cdcaafd080ab164c92e4fcc6a1f87
2EFF514/1D ed3b9eacb185ba20440e4f0797828c434cd18ee97fd275f0893da8fec4ef88f3
2EFE605/45 4b753b3c829058f32800942fb2053806bf08519545c16050363d5afff6ded03d
2EF3944/EB a4c2f6c4adb3e2b428f048a306a2716e94152ed662bc2574922154f5678875a2
2EF3260/AE 6177b8657312a636860667a28daf23dcaa11a1af8deff9baaec40983e746b017
118DA8/80  de0826d655627888b158b53e66424a27fad3907ff07cbe4b1c381a4e715251b0
114B00/10B 8f523efc720a1ffb6ab0ba3c1d5aa4d0352ab42e437351d1479a0af4dce2d68a
2EFA791/C  8ed6dc7b7f419fdb23fdde2f070e555c115f65c1fe4bc35029d94c6ef55e688f
```
