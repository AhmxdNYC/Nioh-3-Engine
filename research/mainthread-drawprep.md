# Main thread ([App]Game): what the draw-prep hot spots are, and what is actually waste

Static analysis only (Ghidra + dump disassembly + the r5/r6/r7 sampling profiles). Nothing was attached, written or
applied. Date: 2026-10-06. Helper scripts and raw decompiles stay in the private dev repo.

## TL;DR

About 55% of the main thread is **preparing and recording draws**:
- the scene/model renderer 535188 → 39E20 → 1DC2F0 → 1E80B0 takes ~35%;
- render-frame-begin 4A529C takes ~19%, of which ~10% is meshlet-streaming bookkeeping and ~8% is waiting for the submit thread.

Most of it is **necessary work bound by cache misses**: one pass per draw over scattered objects, with state filtering and
token recording into KTGL's own deferred command stream at `ctx+0x3C40`. No single bug is as big as the old W1, and
there is no WC read-modify-write in the draw path itself. What *is* waste:

| rank | id | what | where | main-thread cost now | expected saving | patch | risk |
|---|---|---|---|---|---|---|---|
| 1 | **A** | 25 "atomic loads" written as `xor eax,eax; lock cmpxchg [m],0`. They serialize pointer-chasing loops (no memory-level parallelism) and dirty every flag line | 4AA34 / 4B104 (MeshletCheckRequestLoad), 419A8 (meshlet state flags), 1E835B (per draw) | 4AA34 2.3–2.8 %, 4B104 ~0.6 %, 419A8 0.8–1.1 %, plus a per-draw share of 1E80B0 | **~1–2.5 %** | in place, same length, 25 sites, **verified offline** | low |
| 2 | **D** | Blind **1 ms busy-wait retry** in the main loop: when the frame function isn't ready yet, it spins on QPC for a fixed 1 ms, never checks readiness meanwhile, then retries | 1A744E4 (constant), loop 1A74BD0..1A74C48 | 0 % in r5, **5–10 %** of samples in r6/r7 (RtlQueryPerformanceCounter ← 1A74BE0) | up to ~0.5 ms per frame, **only in frames that hit this wait** (near the cap) | 4-byte displacement change: 1 ms → 0.2 ms, **verified offline** | low |
| 3 | **B** | Group-length **binary search** (~11–15 dependent pointer-chase probes) for every skipped draw group, where groups are usually tiny | 3BC0C ← 39E20 @3A51A | 1.2–1.4 % | ~0.5–1 % estimated (if groups are mostly 1–4 items); **measured later as mod option B: paired ABBA -0.11 %, so no gain; B is off** (docs/BENCHMARKS.md) | cave (136 B), galloping search; **24,000 randomized tests vs the original bytes, 0 mismatches** | low |
| 4 | **C** | **UI glyph atlas wiped every frame**, so every visible glyph is re-decoded and re-written into write-combined upload memory every frame, then re-uploaded | reset 96A2C0, called from 4A5420 (render begin); rebuild 62000/61B1C/61EA0 | 61B1C alone 1.1–2.5 %, ~2–3 % total | ~1.5–2.5 % | cave: reset only when the atlas is getting full. **Only valid if the atlas texture keeps persistent staging** (`tex+0xA8 != 0`); one live read needed first | medium |
| 5 | **E** | **W1-class bug**: the UI quad builder multiplies vertex alpha *in place* in a write-only mapped vertex buffer (map mode 3 = write-only upload), so each `movzx` is an uncached read | 1CD8BC..1CD914 in 1CD690 | 1.1–1.6 % | ~1–1.5 % | not a byte patch (see E). First confirm with `ripprobe` | medium |
| 6 | F | Material **parameter lookup by string** (hash + linked-list walk with a virtual call per node), done per group (`IndexedRim`, `LightMap`, `SSSTransmit`, `Refraction*`) and **per draw** (`"Meshlet"` in 1D2F60) | 3C768C / 3C7750 | ~1–2 % | ~1 % | needs a cache keyed on (material, name): ABA/staleness risk | not recommended yet |
| 7 | G | Software prefetch in the pointer-chasing loops (39E20 groups, 1E7050 meshlet parts, 4AA34 entries) | caves | n/a | unknown, possibly 1–3 % | caves; prefetch can't change results | low risk, research |

Estimated total from the safe ones (A + B, plus D where the wait occurs), from profile shares: **~2–4 % of the main
thread**, which would only show as fps when CPU-bound. C and E could add ~2–4 % more once confirmed live. These are
estimates, not measurements: B was later measured with no gain (see above), and the test PC is GPU-bound at its
settings (docs/BENCHMARKS.md). Everything else in the profile is real work (see "Necessary work").

---

## 1. What the hot functions are

```
1A74130  main frame loop ([App]Game)             QPC-timed retry loop (candidate D)
 └ 3D1F20 → 112DD8 → 113204 (37 %)
    └ 535188 (35 %)  model/scene renderer for one view: per render bucket (stencil/highlight layers
                     "HighlightStencilOnly_*", "SpecificStencil_Character", "CustomHighlightPass",
                     shadow "Cascade %d") → 25 call sites of 39E20
       └ 39E20 (31 %)  DRAW-LIST WALKER. param_11 = list of draw items; a group = consecutive items whose
                       object+0x18 (model resource / material set) is equal. Per group: pick material
                       for the pass, filter the pass with FindParamByName("SSSTransmit"/"RefractionPrePass"/
                       "Refraction"/"IndexedRim"/"LightMap") + name prefix tests ("CNS-", "O?L"), bind the
                       technique (3C77F4, 6.7 %), then 1DC2F0 per sub-pass, unbind 3C5D64.
                       Skipped group → 3BC0C (group length). Self time = cache misses on item → material
                       (hot RIPs 3A0D8, 3A2B0, 3A338 are each right after such a load).
          ├ 3BC0C     group length: binary search if list type==3, else linear      (candidate B)
          └ 1DC2F0 → 1E80B0 (17.5 %, self 6.1–6.6 %)  DRAW EMITTER, once per draw per sub-pass:
                       compares each state against a cached shadow copy (ctx+0x2550/0x2558/0x25B8/0x25BC/
                       0x26AB/0x29A3/0x2604...) and on change writes tokens to the KTGL command stream
                       (*(ctx+0x3C40) += 4..: 0x3D, 0x3B, 0x14, 0x13, 0x17, 0x85, 0x0B, 0x3C ...),
                       binds material params (1E6664), meshlet residency check (lock cmpxchg 1E835B, A3),
                       refcount swaps (lock inc/xadd ×2 per draw in the meshlet path), then
                       1D2F60 (8.2 %): draw token 0x6A, or the meshlet path (FindParamByName("Meshlet")
                       per draw, 1D2AAC / 32F5248).
                       Hot RIPs 1EA170 / 1E8873 / 1E82EE / 1E8138 are all right after loads of
                       per-draw or per-material data (cache misses), not locks.
       1DF880 → 1D7340  flush dirty shader-constant slots into the command stream (tokens 0x2B/0x2C/0x37/
                       0x38 + inline memcpy: B94250 @1D76C5)

577E50  tiny vtable wrapper: pre-hook (vtbl+0x30) → 4A529C → post 577EBC
 └ 4A529C (18.7 %)  RENDER-FRAME BEGIN: latency markers / xellSleep, frame-latency poll (R1, patched),
       ├ 7D9B6C (8.4 %)  wait for the submit thread: 7D9E90 Sleep(0) loop (W6, out of scope)
       │                 + 252510 page-pool frame bookkeeping → 2523FC usage scan (2.2 %, see below)
       ├ 96A2C0          GLYPH ATLAS RESET, every frame                              (candidate C)
       └ 5D214 (9.9 %)   graphics-context begin-frame — NOT a job-system join/steal. It is the
                         RADEN meshlet streaming:
            ├ 4BBEC (6.6 KB) per-object meshlet LOD/streaming → 1E7050 → 128EF0 / 1290B8 per part
            │                (1.5 + 2.0 + 1.0 %, cache misses on part objects) and 4DD4C upload memcpy
            ├ 5B278 → 419A8  per-meshlet state flags: up to 16 locked reads + 4 CAS loops per entry  (A2)
            └ 5D1F4 → 4AA34  "MeshletCheckRequestLoad": scans all meshlet entries, builds the
                             load-request compute dispatches (token 0x7E), 3–5 locked reads per entry (A1)
 5DC18 → 4B104             same idiom as 4AA34 (A1f-h)
```

Other main-thread hot spots:
- **B94250 / B94230** (memcpy, ~5 %): 1D7340 (constants into the command stream), 4DD4C (CPU buffer → mapped
  upload heap, double-buffered, 4DB6C/695328), and 242EB8/242FC4 (vtable methods writing token 0x66 plus an inline
  blob of 8 KB–1.5 MB via `rep movsb`). The source loads miss (hot RIP B943D7 is right after the first source load).
  This is KTGL's design: data is copied into the deferred stream, then copied again by SynergisticDispatcherTask.
  Removing the double copy would mean redesigning the command stream, so it isn't a patch.
- **2523FC** (2.2 %): 252510 → 2522C4 scans every page pool (1 or 3 types × slots × list × 64-byte entries) and counts
  entries with `lastUsedFrame >= frame`. **It's not debug-only**: 126C64 reads the previous count
  (`device+0x6CD0`) for a >85 % occupancy heuristic, and 28B1B1C reads it too. An incremental counter would need a
  redesign, so it's left alone. (A later mod option, PP, ran the sweep only every N-th frame; paired re-tests showed no
  change and PP is off by default, see docs/BENCHMARKS.md.)
- **1CD690** (1.1–1.6 %): UI quad builder, candidate E.
- **3C77F4** (6.7 % incl): technique/material bind per group. Not examined in depth; it contains more by-name lookups.

---

## 2. Candidates in detail

### A — atomic-load idiom → plain load (25 sites, in place)
**Evidence.**
- The engine reads shared flag words with `InterlockedCompareExchange(p, 0, 0)`, compiled as
  `xor eax,eax; lock cmpxchg [m], rZ` with rZ == 0.
- Since eax == rZ == 0, the only architectural effects are `eax = [m]` and flags. A failed `lock cmpxchg` still writes
  the line back, and every locked instruction is a full fence.
- In these loops each entry is a 2–3 level pointer chase. 4AA34's hot RIPs are 4AC70/4AC76 (74 %), right after
  `mov rax,[rcx+150h]` / `mov r8d,[rax]`. 419A8's hot RIP is 41AA2 (72 %), right after
  `lock cmpxchg [rcx+0D0h],ebp` on a pointed-to object.
- A locked op can't start until all earlier loads complete, and later loads can't pass it. So iteration i+1 can't
  overlap iteration i's misses: zero memory-level parallelism, plus a dirtied flag line per read that the streaming
  worker threads then have to fetch back.

**Patch.** Replace only the `lock cmpxchg` with `mov eax,<same operand>` + NOP of the same length. The `xor eax,eax`
stays, so both old instruction starts remain valid. All 25 sites are listed and verified by
`mainthread_patches.fsx`, which checks bytes, operand equality, NOP filler, and that the next
instruction re-sets flags.

| id | rva | orig | new |
|---|---|---|---|
| A1a–e | 4AC14, 4AC2B, 4AC42, 4AECF, 4AFAC | `F0 44 0F B1 14 0A` | `8B 04 0A 0F 1F 00` |
| A1f–h | 4B319, 4B32C, 4B33F | `F0 44 0F B1 14 0A` | `8B 04 0A 0F 1F 00` |
| A2a | 419E8 | `F0 0F B1 A9 C0 04 00 00` | `8B 81 C0 04 00 00 66 90` |
| A2b, j–p | 41A10, 41AAC, 41AC3, 41ACC, 41AED, 41AF6, 41B12, 41B7B | `F0 41 0F B1 28` | `41 8B 00 66 90` |
| A2c–h | 41A28, 41A38, 41A4A, 41A5A, 41A6A, 41A7A | `F0 41 0F B1 2C 09` | `41 8B 04 09 66 90` |
| A2i | 41A9A | `F0 0F B1 A9 D0 00 00 00` | `8B 81 D0 00 00 00 66 90` |
| A3 | 1E835B | `F0 0F B1 11` | `8B 01 66 90` |

**Zero source registers (checked by hand).**
- 4AA34: r10d is zeroed at 4ABB9, 4AD97 and 4AFDD. The one write in between (4AF21) is on a path that is re-zeroed before the next iteration's reads.
- 4B104: r10d is zeroed at 4B2CA and 4B5DE, never written otherwise.
- 419A8: ebp is zeroed at 419C4, never written.
- 1E80B0: `xor edx,edx` immediately before the site, `xor eax,eax` at 1E834C.

Ghidra's decompile agrees (`if (v == 0) *p = 0`).

**Not touched:**
- the real CAS loops in 419A8 (`lock cmpxchg [r8],ecx`, still locked);
- the two sites whose ZF feeds a branch (4AA7A, 4AB36, once per frame);
- refcount `lock inc` / `lock xadd`;
- 4BBEC/4BA8C, which is per object rather than an inner loop; they can be added later the same way.

**Semantics.**
- An aligned 32-bit `mov` is atomic and has acquire ordering on x64, and coherence is unchanged.
- Only the StoreLoad fence is lost. None of these loops stores a flag and then re-reads a peer's flag (Dekker style); they read status words written by streaming workers.

**Apply.** Same format as `cpu_patches.fsx` (name, rva, orig hex, new hex, same length; `safeWrite` with the
process frozen). Rows can be appended to that list as-is.

**Verify.**
- Profile shares: 4AA34 (now 2.3–2.8 %), 4B104 (~0.6 %) and 419A8 (0.8–1.1 %) self should fall, and 5D214 inclusive (~10 %) should fall with them.
- PresentMon A/B at the shrine spot, same as round 6/7. Expect +1–2 % fps; no visual effect is possible.
- Watch for meshlet pop-in regressions anyway (there's no reason for any).

### D — 1 ms blind retry quantum in the main loop
**Evidence.**
- 1A74130: `sil = (3D1F20(...) == 0)` (3D1F20 returns 0 when no frame step ran this iteration). If set, the loop at
  1A74BD0..1A74C48 runs `start=QPC; while ((QPC-start)/freq < xmm6) ;` and then jumps back to the retry at 1A744F1.
- xmm6 is loaded once at 1A744E4 from .rdata 0x3BD4688 = **0.001f**. The spin never re-checks readiness, so when the
  awaited frame becomes ready 0.1 ms into the wait, the main thread still idles 0.9 ms.
- Its share was 5.2 % (r6-A1), 7.8 % (r7-A1) and 0.9 % (r7-B1) of main-thread samples (RtlQueryPerformanceCounter
  ← 1A74BE0, plus 1.5–2.3 % self in 1A74130 loop 1A74BD0..1A74C48), and absent in r5. So in some runs about
  0.5–0.9 ms per frame went into this quantum, while fps stayed below the 120 cap.

**Patch.** Re-point the RIP-relative displacement at an existing 0.0002f in .rdata (0x3BD4600):
`1A744E4: F3 0F 10 35 9C 01 16 02` → `F3 0F 10 35 14 01 16 02` (`movss xmm6,[rel 3BD4600h]`).
- xmm6 is used only at 1A74C45 and is callee-saved.
- CPU use is unchanged, since it spins either way; only the retry of the frame function becomes 5× more frequent (a few µs each).

**Risk.** Low. Worst case, slightly more retries of 3D1F20's prologue.

**Verify.**
- PresentMon at a spot near the 120 cap: avg fps, and frametime p50/p99.
- In a profile, the share of the 1A74BD0 loop should drop or stay the same, with no new hot spot.
- At ~113 fps CPU-bound (r5-like) expect no change.

### B — galloping group-length search (cave)
**Evidence.**
- 3BC0C is called 86–94 % from 39E20 @3A51A, the skip-group path.
- For list type 3 it runs a branchless binary search over [s, n) for the first item whose key differs. Each probe is
  3 dependent loads (`arr[mid]` → item → +0x18); hot RIPs 3BC3F/3BC3B/3BC38 sit right after them.
- That's ~log2(n) ≈ 11–15 probes per call, regardless of group size. Draw groups are usually small; 1DC2F0's own
  draw path walks the group linearly.

**Patch.**
- At 3BC17 (`44 8B 54 24 28  mov r10d,[rsp+28h]`, the first instruction of the type-3 path), write a 5-byte `jmp rel32` to a cave.
- The cave probes s first (one probe for a singleton group), then s+1, s+3, s+7 ..., then runs a binary search inside the last bracket.
- It ends with the original tail (`sub/inc/store/mov rbx,[rsp+8]/ret`). The type≠3 linear path is untouched.
- The cave code (136 bytes, Iced) is in `gallop_3BC0C.fsx: buildCave` (private dev repo).

**Proof.** The script copies the original 150 bytes from the dump into its own executable memory, builds the patched
copy plus cave, and runs both on 24,000 random sorted inputs (runs of 1..400, sub-ranges, s ≥ n). Result:
**0 mismatches**.

**Assumption.** The type-3 list is sorted by key. The original binary search already relies on this, and the draw
path's linear group walk would disagree with it otherwise.

**Trade-off.** Probes ≈ 1 + 2·log2(g). That's better below g ≈ 16 and up to ~1.5× worse for groups in the hundreds
(e.g. massively instanced props).

**Measured.** As mod option B, a paired ABBA test on PC gave -0.11 % (no gain); B is off (docs/BENCHMARKS.md).

**Verify.** 3BC0C self (1.2–1.4 %) should drop. If it rises instead, groups are large: revert, or cap the gallop at
step 16 and fall back to the plain binary search.

### C — glyph atlas rebuilt every frame
**Evidence.**
- 96A2C0 runs `atlas=[this+28h]; atlas.count(+74)=0; maxY(+8C)=0; x(+84)=y(+88)=pad(+7C)`. It is called
  unconditionally from render-begin 4A529C at 4A5420, on the object returned by vtbl+0x158.
- 7D6B20 sets the text renderer's current atlas `+0x30 = +0x28`.
- The glyph cache 62000 looks glyphs up in a per-code tree whose nodes are valid only if `index <= count`. So after
  the reset, every glyph on screen misses and is decoded again into the mapped atlas (4-bit / 8-bit / RLE → RGBA
  dwords on write-combined memory), padding-cleared (61B1C: the W1 site, still 1.1–2.5 % after the W1 fix), and the
  rect {0,0,W,maxY} is re-uploaded (4A2E94 → 699EC8).
- The engine already has a persistence switch:
  - 69A3DC resets the atlas on map only when `tex+0xA8 == 0`, i.e. transient staging, allocated from a ring and released at unmap (69A0B4);
  - with `+0xA8 != 0` the staging buffer is dedicated (9C2EE0), kept across maps, and the unmap copies a rect (69A138).
  - So for a persistent atlas texture the per-frame wipe in 96A2C0 is redundant.

**Patch idea.**
- Cave at 4A5420 (call 96A2C0): reset only if `atlas.count > cap/2` or `maxY > height/2`, or the atlas texture's `+0xA8 == 0`. Otherwise keep the glyphs.
- Keep a "failed alloc last frame" flag (set from the 61EA0 failure path) that forces a reset next frame.

**Before building it, read live:**
1. The atlas texture's `+0xA8` (atlas+0x40 → +0xA8). If it is 0, the per-frame rebuild is required and C is dead.
2. atlas+0x74 at the end of a frame, to get glyphs per frame.

**Risk.**
- Medium: if the atlas fills mid-frame, the original drops the rest of the text for that frame (1CE818 sets `bVar1=false`).
- A threshold policy keeps that from becoming more likely, but text-heavy menus must be tested.

**Expected saving.** ~1.5–2.5 % of the main thread (61B1C + 62000 + 61EA0 + the per-frame upload), plus some GPU copy work.

### E — UI alpha read-modify-write on a write-only mapped vertex buffer (W1 class)
**Evidence.**
- 1CD690 maps the UI vertex buffer with 4FD84 (GpuBufferMap, …, mode 3). In the mode table at 0x3BE29D0, mode 3 = {read 0, write 1}, a write-only upload mapping, i.e. write-combined memory.
- For each quad (0xB0 bytes, 4 vertices × 0x2C) whose element opacity ≠ 1.0 it does
  `v.alpha = (byte)(v.alpha * opacity)` four times: `movzx eax,byte [rbp+27h/53h/7Fh/0ABh]` … `mov [..],al`.
- The profile's hot RIPs are 1CD8C0, 1CD8D6, 1CD8EC and 1CD905, i.e. **each right after one of the four byte loads, with
  roughly equal weight (20–43 %)**: the uncached-read signature, as at W1/W2. The z stores to the same lines just
  before (1CD871..1CD883) force WC-buffer flushes too.

**Confirm first.** `ripprobe.fsx <tid> 1CD8BC 1CD91A` should report rbp in WRITECOMBINE memory.

**Fix.** It can't be a simple byte patch, because the original alpha values only exist in WC memory. Options:
- (a) Use MOVNTDQA streaming loads of the whole quad (~3 line reads instead of 4 serialized byte reads, so a small gain).
- (b) Redirect the mapping (`[obj+358h]`) to a cached scratch buffer for the build and stream-copy it to the real
  mapping before 4FBD4 (GpuBufferUnmap). This is semantically exact, but a medium-size cave, and the quad-builder callbacks
  must take the pointer only from `[obj+358h]`.
- (c) Apply opacity in the producers (vtbl callbacks): more RE needed.

**Expected saving.** ~1–1.5 %.

### F — material parameter lookup by string
3C768C (material vtbl+0xF8 `FindParam(name)`):
- computes `h = h*27 + c` over the string;
- walks the parameter list (`+0x1330`, next `+8`), calling a virtual compare per node (3C7750: hash, then strcmp).

It's called per group in 39E20 (several names), per draw in 1D2F60 (`"Meshlet"`; a full walk when absent) and in
1E80B0 (`"WritePositionBuffer"`, `"ControlPointMeshDeformation"`, `"SoftBody"`). 3C7750 is 0.8–1.2 % self and the
whole lookup ~1–2 %.

A (material ptr, name ptr) → result cache would work, but materials can be freed and reallocated (ABA), and negative
results can't be validated cheaply. **Not recommended** without instrumentation.

### G — prefetch
The three biggest loops are all latency-bound on 2–3 level pointer chases: 39E20 (item → object → material),
1E7050 (part → +0x68 → object) and 4AA34 (entry → +0x10 → +0x150). A cave that `prefetcht0`s the next 2–4
iterations' first level (and the second level one iteration later) can't change results. The gain is unknown;
it's worth trying after A.

### Minor
1E80B0's meshlet path swaps a refcounted pointer at ctx+0x1A80 per draw: `lock inc` on the new one, `lock xadd -1`
on the old one, even when they are the same object. A cave with `if (new == old) skip both` would save 2 locked ops
per draw (≤0.3 %).

---

## 3. Necessary work (honest)
- 1E80B0 / 1D2F60 / 1D7340 / 1E6664: per-draw state filtering and token recording. It's already redundancy-filtered
  (every state is compared against a cached shadow before emitting). Its cost is the cache misses of a scattered
  object graph, typical of a single-threaded deferred recorder. The real fix is parallel recording, an engine change.
- 39E20's group loop and 3C77F4 technique binds: real work, apart from F and B.
- 1E7050 / 128EF0 / 1290B8: per-frame meshlet LOD/streaming per part, early-out on a frame stamp. Cost = misses (G may help).
- 2523FC: page-pool usage count feeding a real occupancy heuristic (126C64).
- memcpy into the command stream: design (double copy main → stream → dispatcher).
- W6 (7D9E90) and 418F40: waits, out of scope for this note.

## 4. Files (private dev repo)
- `mainthread_patches.fsx`: offline verifier for A (25 sites) and D. Reads the dump only.
- `gallop_3BC0C.fsx`: B cave (Iced) + native equivalence test against the original bytes (own process only).
- `lockscan.fsx`: lists every LOCK op in an RVA range and classifies the atomic-load idiom.
- `scan_disp.fsx`, `scan_store*.fsx`: find code touching given struct offsets (used to find the atlas reset and the page-pool stats consumers).
- Functions decompiled for this note: 535188, 39E20, 3BC0C, 1DC2F0, 1E80B0, 5D214, 4A529C, 4AA34, 419A8, 1E7050, 128EF0, 1290B8, 4DB6C/4DD4C, 61B1C/62000, 61EA0, 1CE818, 69A3DC/69A494/699EC8, 252510/2522C4/2523FC, 126C64, 1D2F60, 1DF880, 1D7340, 1CD690, 5DC18, 3D1F20, 7D9B6C. Disassembly and lock-site listings were kept alongside.
