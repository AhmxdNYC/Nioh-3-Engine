# renderer/cbv-upload: cheaper per-draw constant uploads on the replay thread (CBU: CBS / CBN / CBT / CBF), ASI 1.7.60-cbu2

2026-10-10, branch `cbv-upload` (from replay-prof f08cc34). Follows 01b-replay-handlers.md section 0 (6A per-draw path) and
the pc2 RPF2 confirmation (captures/pc2/rpf2: UPLOAD in BAR VRAM = 6A 4.1-4.6 us/draw of CPU; UPLOAD in system memory =
1.07 us/draw but GPU +8 ms). Tags: **S** static, **M** measured, **E** estimate.

## 0. Answer

1. **Shipping default since 1.7.60-cbu2: CBN + CBT ON, CBS off** (ini `[Patches] CBN = 1, CBT = 1, CBS = 0`; profiles.json
   kept). Evidence: pc2 CBU1 (5600X + RX 6650 XT, CPU-bound, ReBAR on, ABBA x3): **CBN+CBT +15.9 % (38.2 -> 44.3 fps,
   SE 0.06)**, replay handlers 20.5 -> 17.1 ms/f, CPUBusy 25.9 -> 22.3 ms, GPUTime -0.5 ms, no corruption; CBS no gain (+0.3 %
   without sfence, -0.3 % with). **That gain is specific to pc2**, a slow-link machine (its GPU sits on a ~0.8 GB/s chipset
   slot, so BAR stores are expensive there); treat it as a worst case, not a typical result. The PC (RX 9070) showed **no
   measurable change** (cbu2pc: fps +0.49 % +- 1.78, GPU busy -0.02 ms +- 0.02, image identical) and the Ally (Proton/RADV)
   **no change** (clean CBU-A1 rerun: -0.16 % +- 0.16, no faults): section 3b and docs/BENCHMARKS.md.
2. **Census (M):** PC ~3,900 graphics CB uploads/frame (avg 3.0 KB, ~11 MB/f); pc2 100k uploads/s at 3.4 KB = ~2,600/f,
   ~8.7 MB/f. 20-22 % are byte-identical to the slot's previous upload in the same list (CBN skips them; "any list" adds only
   0.4 %, so cross-list dedup is worth nothing). 16 % of the CBV tables become CBT reuse hits.
3. **Offline proof (M):** stock relocated v2.02 118E28 vs CbuTableCall + CbuStreamCopy (+ CBF staging), byte-exact for every
   bound view, on 12 synthetic streams and on the recorded game trace (30,000 calls, 270 lists, 99.5 MB); test_patterns
   ALL 248 SITES OK.
4. **F1 caveat closed (S):** CBN only drops a dirty bit whose slot was uploaded in the SAME open list (fence tag c+6D0h and
   list c+580h unchanged), so a list boundary never inherits a skip, whoever re-dirties after it. The remaining hole was a
   ring-full cut INSIDE SynUploadCbvTable (CmdListRingSubmitIfBehind + wait 945610 + 9C9EF0; the loop keeps its mask
   snapshot): a slot CBN skipped stayed on a block of the retired list while the new list's table read it. cbu2: on such a
   cut CbuTableCall uploads exactly the skipped slots again in the new list and binds the table again (census
   `cuts (re-uploaded n)`). The game never cuts there (0 cuts in every PC and pc2 census window), so this is insurance.
5. **Next lever = CBF (F8, built, test only):** after CBN, ~2,050 uploads/f (~7 MB/f) still go into BAR VRAM. CBF writes them
   to system memory and copies them in one batch per submission into a VRAM ring (section 7). Expected on pc2 (E):
   **net -2 to -3 ms/f on the replay thread -> +8 to +12 % fps (44.3 -> ~48-50)**, capped by GPUTime (18.6 ms + the copy
   -> ~52 fps max). PC (M, cbu2pcf): copies 8.7 MB/f in 16 single-run copies, +0.17 ms GPU, image correct, but
   CreateConstantBufferView on the VRAM ring costs 245 vs 31 ns/call in the game (+0.7 ms/f; not reproduced in the bench).
   The first PC CBF run rendered black (RPF's ECL thunk hid the submissions): fixed + watchdog before pc2 sees it.
   Per-slot / dirty-range dedup was dropped: cross-list identical slots are 0.4 %, and a partial upload is impossible (a new
   block must hold the whole slot; patching an in-flight block races the GPU). F2 dropped earlier (shaders read 99.5 % of
   their cbuffer bytes).

## 1. What each switch does (S)

All in `mod/Nioh3EngineFix/src/cbvupload.{h,cpp}`; v2.02 only; every key 0 = not installed (default), 1 = on, 2 = installed
off; live `Tuning.CBS / CBN / CBT / CBALL / CBSTATS = 0/1` (LiveCtl). Sites (patches.json group CBU, check kind; each a 5-byte
`call rel32` -> relay `mov rax, imm64; jmp rax` in a page near the image; FNV-1a of 118DA8..118E27, 118E28..1190BE,
1192B0..119363 and 4959EC..495C58 checked first):

| key | site | what |
|---|---|---|
| `[Patches] CBS` (F3) | 118F4D (graphics, Memcpy B94250 in SynUploadCbvTable) and 495AF2 (compute, CmdListUploadComputeCbvTable 4959EC) | `CbuStreamCopy`: forward copy, 16-byte `movntdq` stores, whole 64-byte lines in order; only [dst, dst+size) written (same bytes as memcpy). Graphics: ONE `sfence` per table, issued by CbuTableCall after SynUploadCbvTable returns (so CBS installs the 118E19 wrapper too); compute: `sfence` after each copy. A ring-full cut inside 118E28 submits through ExecuteCommandLists (serialising), the same guarantee the stock regular WC stores rely on. |
| `[Patches] CBN` (F1) | 118E19 (SynFlushCbvTables -> SynUploadCbvTable) -> `CbuTableCall` | For each dirty slot of the stage's layout: if the shadow bytes (c+BB0h[idx], declared size) equal the bytes of that slot's last upload, AND that upload was made in the same open list (SynCtx+6D0h fence tag and +580h list pointer unchanged), with the same declared size, c+F30h[idx] == size and the same staging heap (c+940h), its dirty bit is cleared before the original runs: no ring block, no memcpy, no CreateConstantBufferView. The staging CBV still points at the previous block (identical bytes, fenced by the same list: the ring retires per list, 06-resources.md 1.3). Table copy + SetGraphicsRootDescriptorTable still run. A list cut inside the call (tag or list changed) forgets the stage's records. |
| `[Patches] CBT` (F4) | same wrapper | When (after CBN) no slot bit is left (bit 31 alone is fine) and the stage's last table copy was made by the original in the same list, same n (layout+44h), same root index, same staging heap: the original is not called; `SetGraphicsRootDescriptorTable(root + stage, previous table)` and c+B90h[stage] = n. The previous table's index is base + cursor-before-the-call (ShaderVisibleDescRingAlloc 1192B0 allocates at the cursor; the early wrap resets the cursor only afterwards), verified through the cursor delta / wrap count and the per-descriptor fence stamps (== the open list's tag). Only 118E28 writes the graphics CBV staging slots 0..97 (grep of every c+940h user), and every 118E28 call goes through the wrapper (single caller 118DA8, single call site). |
| `[Profiler] CbvStats` | same wrapper | census line `CBU:` every CbvStatsSec: tables/s, uploads/s, MB/s, avg size, no-op share (any list / same list), skipped, table-reuse candidates, cuts, CBS copies. Tracking (memcmp + a cached copy of each uploaded slot) runs only while CBN / CBT / stats is on; switching it on starts a new epoch (all records invalid). |
| `[Profiler] CbvRecord = N` | same wrapper | inputs of the first N table calls after CbvRecordDelaySec: stage, mask, layout entries, n, root, ctx, tag, list, dirty shadow bytes -> `Nioh3EngineFix_cbvtrace.bin` (replayed by the harness). |

CBN / CBT are refused with `[GpuOpt] CbvMirror != 0` (mirror CBVs have their own lifetime). Not covered: compute CBV tables
(4959EC: CBS only), async-compute sub-tables (token 90 / 3C02800) and 7E dispatch constants other than through 4959EC.

**List cuts (review caveat, S):** the code that re-dirties tables after a cut is still untraced; CBN / CBT never rely on
it: they only reuse a block / table made in the SAME open list (tag + list pointer), never across lists, and every new list
still gets its own SetGraphicsRootDescriptorTable from the same call sequence as stock (CBT calls it itself). The stock
engine's own cross-list reuse of untouched slots (staging CBVs pointing at older-list blocks) is unchanged.
**In-call cut (cbu2, S):** SynUploadCbvTable 118E28 handles a full ring by CmdListRingSubmitIfBehind(c+5A0, c+588, tag) +
945610 (submit, Sleep(0) spin on the fence) + 9C9EF0, then continues its loop with the mask snapshot it took at entry. If the
open list itself filled the ring this closes it and opens a new one inside the call. CbuTableCall sees the tag / list change
after the original returns; if CBN cleared any bit for this call it sets c+B70h[stage] = exactly those bits and calls the
original once more (fresh blocks in the new list, table copied and bound again; the first bind is replaced before any draw).
Census: `list cuts in a table n (re-uploaded m)`. Harness: the tiny-ring arms run thousands of these (ALL PASSED); the
`patched view gone there` counter after cuts is dominated by the arms cutting at different points (CBN allocates less),
which is the engine's own pre-cut hazard, not something CBN adds.

## 2. Offline proof (M)

`py -I tests/run_cbvupload_harness.py [trace.bin]` -> ALL PASSED. Checks per SetGraphicsRootDescriptorTable: (1) patched
bytes == stock bytes for every view the layout declares, (2) a stock view on a block of the open list is on a live block of
the open list in the patched arm, (3) at every list close, each bound table and each safe view still holds its bind-time
bytes (no ring / descriptor reuse under an open list), (4) CbuStreamCopy == memcpy on [0, n) for n = 0..4160 x 16 source
alignments, nothing outside written. The ring model enforces the fence-tag rule and poisons freed blocks; tiny rings force
the ring-full submit path (thousands of in-call cuts). After an in-call cut the arms cut at different points (CBN / CBS
allocate less), so pre-cut views are compared for bytes only (the engine's own hazard, unchanged).

| input | binds | views compared | CBN skipped | CBT hits |
|---|---|---|---|---|
| 8 synthetic streams (20,000 flushes each, big / small / tiny rings) | ~18-19 k each | 46-61 k each | up to 16 k | up to 4.8 k |
| recorded PC trace (cbu1pc `cbvtrace.bin`, 30,000 calls, 270 lists, 34,496 dirty slots, 99.5 MB) | 30,000 | 43,886 (big ring) | 6,893 (20.0 %) | 4,873 (16.2 %) |

`tests/run_cbs_bench.py` (memcpy vs CbuStreamCopy into a mapped UPLOAD buffer, RX 9070, game running alongside so noisy):
memcpy 0.47 ms per 4 MB "frame", stream 0.42 (x1.1); with an sfence after every copy 1.27 / 1.09: the drain per copy costs
~400 ns, which is why CBS fences once per table.

## 3. PC run (M)

`captures/pc/fastab/20261010-1333-cbu1pc` (an earlier build = this code minus CBMODE / CBS_FENCE / census fix; tour 7292,
1440p XeSS, saves blocked, installed ASI / ini (Tour = 0) / Nioh3BossRematch.* byte-identical after).
fastab [RESULT]: every item no difference (fps +-0.2..1 %). RPF rows (`tools/cbu_digest.py`, frame-weighted, 5 s windows):

| arm | windows | fps | handlers ms/f | 6A CPU ms/f (ns/draw) | CreateCBV n/f | CopyDescr ms/f |
|---|---|---|---|---|---|---|
| stock | 91 | 99.7 | 5.98 | 0.814 (436) | 4243 | 0.321 |
| census on | 15 | 99.3 | 6.36 | 1.082 (580) | 4242 | 0.314 |
| CBN | 15 | 100.1 | 6.20 | 1.029 (547) | 3476 | 0.317 |
| CBS (sfence per table) | 15 | 99.9 | 6.42 | 1.109 (594) | 4251 | 0.430 |
| CBT alone (no hits without CBN) | 15 | 98.9 | 6.38 | 1.086 (583) | 4230 | 0.311 |

Caveat: the 'CBALL' item measured CBT alone: fastab rewrites the whole control file each apply, so CBN = 0 / CBS = 0 came
after CBALL = 1. Use the single key `Tuning.CBMODE` (bitmask) for combinations. Screenshots A/B per item: differences equal
to the measurement-only CBSTATS pair (animation / fog), no corruption.

## 3b. CBU1 / CBU2 device results (M)

**pc2 CBU1** (`captures/pc2/cbu1`; 5600X + RX 6650 XT, CPU-bound Continue spot, ReBAR on, one cpu_live_sched per arm,
ABBA x3, A = CBMODE 0, RPF on). **Slow-link machine only:** pc2's GPU sits on a ~0.8 GB/s chipset slot, so the BAR stores
these switches avoid are unusually expensive there. The PC and the Ally showed no change (below; docs/BENCHMARKS.md).

| arm | fps B-A (SE) | 6A self ms/f | handlers ms/f | CreateCBV ms/f | CopyDescr ms/f | CPUBusy ms | GPUTime ms |
|---|---|---|---|---|---|---|---|
| 1 CBS + sfence | -0.3 % (0.06) | 11.26 -> 12.24 | 20.57 -> 20.90 | 1.52 -> 0.84 | 2.29 -> 2.39 | 25.95 -> 26.04 | 19.14 -> 18.78 |
| 9 CBS no sfence | +0.3 % (0.07) | 11.22 -> 11.15 | 20.54 -> 20.47 | 1.52 -> 1.01 | 2.26 -> 2.33 | 25.89 -> 25.77 | 19.16 -> 19.17 |
| 2 CBN | **+15.6 %** (0.07) | 11.23 -> 9.13 | 20.49 -> 17.11 | 1.50 -> 0.99 | 2.28 -> 1.67 | 25.84 -> 22.29 | 19.13 -> 18.64 |
| 6 CBN + CBT | **+15.9 %** (0.06), 38.2 -> 44.3 | 11.24 -> 9.07 | 20.53 -> 17.07 | 1.52 -> 1.01 | 2.28 -> 1.71 | 25.86 -> 22.25 | 19.11 -> 18.58 |
| 7 all three | +15.1 % (0.07) | 11.22 -> 9.79 | 20.49 -> 17.27 | 1.51 -> 0.61 | 2.28 -> 1.31 | 25.80 -> 22.39 | 19.13 -> 18.53 |
| 16 census only | -0.4 % | 11.23 -> 11.47 | 20.48 -> 20.65 | | | 25.81 -> 25.88 | 19.10 -> 18.93 |

pc2 census: 85k tables/s, 100k uploads/s (329 MB/s, avg 3,441 B), no-op any list 21.8 %, same list 21.4 %, table reuse
candidates 15.5 %, list cuts 0. Digest of the log (`cbu_digest.py`): 6A = 1,096 draws/f at 5.86 us of CPU each; CBN takes
6A + 69 handler CPU 7.54 -> 6.07 ms/f and their D3D12 time 5.56 -> 3.98 ms/f, i.e. ~2.6 us of CPU per skipped upload
(the BAR stores are slow there). Shots: every arm within the animated-scene floor.

**Ally CBU-A1** (Proton / RADV, GPU-bound UMA; clean single-session run): CBMODE 0/6 -0.16 % +- 0.16 (40.62 -> 40.56),
no faults, no visible difference (an earlier run was invalid: two sessions drove one launch).

**PC cbu2pc** (`captures/pc/fastab/20261010-1608-cbu2pc`, RX 9070, 1440p XeSS, stage 7292, GPU-bound, fastab ABBA x3, build
1.7.60-cbu2 before the CBF ECL fix): **CBMODE 0/6 no-harm**: fps 94.3 -> 94.7 (+0.49 % +- 1.78 SE), GPU busy 9.90 -> 9.88 ms
(-0.02 +- 0.02), frames > 33 ms 0.67 -> 0 per window; shot diff 5.4 (animation floor). RPF: handlers 6.47 -> 6.59 ms/f
(the CBN tracking costs ~0.1 ms/f on a PC whose BAR stores are cheap), CreateCBV 4,115 -> 3,314 /f.
The same launch's CBF arms rendered **black** (writes 317k/s, flushes 0/s): section 7.3.

## 4. pc2 CBU1 spec (done)

ASI 1.7.60-cbu1. Arms `Tuning.CBMODE=0/1, 0/9, 0/2, 0/6, 0/7, 0/16`, one
cpu_live_sched per arm, RPF on, MemStats / CbvMirror / FrameOverlap off. Results in section 3b.

## 5. F2 / dedup (dropped)

- **F2 (copy only the bytes the shader reads): dropped.** Census (`dxc -dumpbin` of 400 random VS/PS DXIL blobs from
  shader_dx12, 582 cbuffers): declared 2,838 B avg, highest constant row read 2,825 B -> **99.5 %** read (100 % for
  cbuffers >= 1 KB). The 2.9 KB is real data, not padding; memcpy already copies the declared size, not size256.
- **Per-slot dedup across lists: dropped (M).** "Any list" no-ops are 21.8 % vs 21.4 % same-list on pc2: +0.4 %.
- **Dirty-range upload: not possible on the CPU side (S).** A changed slot needs a NEW ring block (the old one is read by
  draws already recorded in the open list), and the new block must hold every byte the shader reads (99.5 %). Uploading
  only the changed range means a GPU copy of the rest (old block -> new block) per upload: the CbvMirror many-copies pattern
  that cost pc2 10+ ms (rebar-fix-v3.md 3).

## 7. CBF (F8): system-memory staging + one copy per submission into a VRAM ring (built, test only)

### 7.1 Design (S)

`src/cbvstage.{h,cpp}` (+ hooks in cbvupload.cpp and osd.cpp). `[Patches] CBF = 0` (1 on, 2 installed off), live
`Tuning.CBF`, `Tuning.CBF_QUEUE`, or `Tuning.CBMODE` bits 32 (CBF) and 64 (COPY queue).
- The graphics SynCtx's CB ring (SynCtx+B28h: [0] resource, [1] size, [2] CPU base, [3] GPU base; only UploadRingAllocate
  1190C0 reads [2]/[3]; its callers are 118E28 and 4959EC) gets [2] = a CUSTOM WRITE_COMBINE / L0 buffer (system memory) and
  [3] = a DEFAULT buffer (VRAM) of the same size (111 MB on the PC). Allocations keep their ring offsets: CPU stores go to
  system memory, the CBVs are created on the VRAM address. Swapped / restored by CbuTableCall at a table call (replay thread).
- The memcpy relays at 118F4D / 495AF2 (CBS's sites; plain memcpy unless CBS is on) note each written range; ranges are
  coalesced (gap <= 4 KB; the ring is a bump allocator, so 1 run per submission in practice).
- At the next ExecuteCommandLists on the game's DIRECT queue (learned from the first submission carrying a SynCtx list) the
  noted ranges are copied: `CBF_Queue = 0` a prologue list in the same ExecuteCommandLists (CopyBufferRegion +
  COPY_DEST -> VERTEX_AND_CONSTANT_BUFFER barrier; buffers decay to COMMON at the end of the submission), `= 1` our COPY
  queue + Signal, the game queue Waits. 64 allocators, fenced.
- Lifetime: a VRAM range is rewritten only when the engine hands its ring offset out again (after the list that used it
  completed: the engine's fence-tag rule), and its copy runs before the lists of that submission. Gap bytes re-copy what
  staging already holds (identical bytes for live blocks). Every relay write is noted under an SRW lock (a locked op: it
  also drains the write-combining buffers before the copy can read them).
- Safety: refused with CbvMirror != 0; a record failure turns CBF off; **watchdog** (cbu2): ~100k ring writes with no
  submission taking them -> CBF off for the session (bases restored, never re-armed).
- Harness: 4 synthetic CBF arms (big / small / tiny rings, live on/off toggling, with CBS+CBN+CBT) and 2 trace arms,
  byte-exact vs stock for every bound view after the simulated submission copy.

### 7.2 Copy cost (M, `tests/run_cbf_bench.py`, RX 9070, no game)

| pattern | 11 MB/frame in 16 submissions | 7 MB/frame |
|---|---|---|
| DIRECT prologue, one copy per submission (CBF_Queue 0) | 0.47-0.48 ms GPU = 24 GB/s | 0.32 ms = 23 GB/s |
| COPY queue, one copy per submission (CBF_Queue 1) | 0.46 ms = 25 GB/s | 0.31 ms = 24 GB/s |
| DIRECT, one 3 KB copy per CBV (CbvMirror) | 1.8-1.9 ms = 6 GB/s | 1.2 ms = 6 GB/s |
| COPY queue, one 3 KB copy per CBV | 0.7-0.9 ms = 13-16 GB/s | 0.48 ms = 15 GB/s |

Batching is worth 4x on the DIRECT queue. CreateConstantBufferView costs 6-10 ns on UPLOAD / CUSTOM / DEFAULT buffers alike,
also with 6,000 other resources created first (`cbf_bench.exe 11 16 cbv 6000`).

### 7.3 PC runs (M)

- **cbu2pc (bug):** with ReplayProf on, the game's ExecuteCommandLists reaches osd's hook through RPF's thunk (return address
  in the ASI), and osd passed non-game callers straight through, so CBF never copied: VRAM ring all zeros, black frames, GPU
  busy -4.5 ms. Fixed in 8213538 (non-game callers go through cbvstage too; copying early is always safe) + the watchdog.
  pc2 runs RPF as well, so this would have hit pc2.
- **cbu2pcf** (`captures/pc/fastab/20261010-1625-cbu2pcf`, fixed build, A = CBMODE 6): CBF works: 1,470
  flushes/s (16 per frame, 1.0 run each), 800 MB/s copied (~8.7 MB/frame), 0 allocator waits, 0 other-queue; shots
  6 vs 38 / 102 diff 6.7 (animation floor), image correct. The schedule overran the dwell (slow first toggles), so the
  COPY-queue item got 1 pair.

| arm | fps B-A (SE) | GPU busy B-A (SE) | handlers ms/f | 6A self ms/f (cpu / d3d) | CreateCBV ns/call (ms/f) |
|---|---|---|---|---|---|
| A: CBMODE 6 | | | 6.65 | 2.06 (1.24 / 0.81) | 31 (0.10) |
| 38 CBF DIRECT | +2.8 % (3.1, noise) | **+0.17 ms (0.02)** | 7.35 | 2.59 (1.27 / 1.32) | **245 (0.79)** |
| 102 CBF COPY queue | 1 pair only | n/a | 7.38 | 2.59 (1.27 / 1.32) | 244 (0.79) |

Read: on the PC (cheap BAR stores) CBF saves no CPU and costs +0.7 ms/f of replay-thread time, almost all of it in
CreateConstantBufferView (31 -> 245 ns/call on the VRAM ring addresses). The bench does not reproduce that (6-10 ns for any
heap kind): suspect a layer in the game's device chain (OptiScaler dxgi.dll / Streamline) resolving CBV addresses with a
fast path for the game's own ring. Open; pc2's RPF api rows show its size there. GPU: the copy costs +0.17 ms busy/frame.

### 7.4 Expected on pc2 (E)

After CBN+CBT pc2 still writes ~2,050 uploads/f (~7 MB/f) into BAR VRAM. RPF2 (VSForceSysUpload = the same CPU writes in
system memory): 6A 1.07 vs 4.1-4.6 us/draw = ~3.3 us/draw x 1,096 draws = ~3.6 ms/f; CBU1 says ~2.6 us of handler CPU per
upload moved -> 2,050 x ~2.1 us = ~4.3 ms/f upper bound. Minus the CreateCBV penalty (+0.7 ms/f on the PC, ~+1 ms on pc2's
slower CPU) and ~0.1 ms of extra submission calls: **net -2 to -3 ms/f on the replay thread (CPUBusy 22.3 -> ~19.5-20.3 ms)**,
GPUTime 18.6 + the copy (~0.3-0.6 ms on PCIe 4.0 x8) as the new floor: **+8 to +12 % (44.3 -> ~48-50 fps)**, ceiling
~52 fps. If pc2's CBV penalty is much larger than the PC's, CBF loses: the cbf_bench run decides the copy cost, the RPF api
rows the CBV cost.

## 8. pc2 CBU2 test spec

- **ASI:** 1.7.60-cbu2 (branch cbv-upload, commit 8213538+). **cbf_bench.exe** (needs no game).
- **Step 1 (no game, ~1 min, game closed):** `cbf_bench.exe 7 16` and `cbf_bench.exe 7 16 cbv 6000`; post the console.
- **Step 2 (game):** pc2's usual CPU-bound ini, ReBAR on, + `Patches.CBN=1; Patches.CBT=1; Patches.CBS=0; Patches.CBF=2;
  Debug.LiveCtl=1; Profiler.ReplayProf=1; Profiler.ReplayProfSec=10; Profiler.CbvStatsSec=10`; MemStats 0, CbvMirror 0,
  FrameOverlap 0, SP / GPUProf / JT off. Continue spot as CBU1, 2 min in the field, then one `cpu_live_sched.ps1` session
  per arm, ABBA x3, A = the new default CBMODE 6: `Tuning.CBMODE=6/38` (CBF DIRECT), then `Tuning.CBMODE=6/102` (CBF COPY
  queue). One shot per B window + plain. STOP if the log shows `CBF OFF for this session` / `FAILED`, or the image goes
  black / garbage.
- **Send back:** mod log (RPF / LIVE / CBU / `CBU CBF` lines), report.txt + manifest per arm, PresentMon CSV, shots + diff.
  Digest: `py -I mod/Nioh3EngineFix/tools/cbu_digest.py <log>` (arm key shows `CBF 1 q 0/1`).
- **Decides:** keep CBF (DIRECT or COPY) as a default candidate if fps B-A > 2 SE with no image change; read 6A cpu,
  CreateCBV ns/call and GPUTime per arm.

## 6. Files

- `mod/Nioh3EngineFix/src/cbvupload.{h,cpp}` (CBS / CBN / CBT / census / record / cut re-upload), `src/cbvstage.{h,cpp}`
  (CBF), osd.cpp (ECL hook calls cbvstage for game and non-game callers), dllmain (1.7.60-cbu2), ini keys, profiles.json
  (CBN / CBT kept), build.bat / CMakeLists, `tools/testmatrix/live_keys.py` (CBS / CBN / CBT / CBALL / CBSTATS / CBMODE / CBF
  / CBF_QUEUE).
- `tests/cbvupload_harness.cpp` + `run_cbvupload_harness.py` (stock vs CBU incl. CBF arms on the relocated v2.02 code;
  optional trace files), `tests/cbs_bench.cpp` + `run_cbs_bench.py`, `tests/cbf_bench.cpp` + `run_cbf_bench.py` (copy and
  CBV cost).
- `tools/cbu_digest.py` (per-arm RPF 6A / 69 / 7E rows + CBU census from a log; CBMODE bits incl. CBF).
- `tools/testmatrix/fastab.ps1 -Asi <branch build> -Shots` (run_tour / session_lib honour `$env:NIOH3_ASI`).
