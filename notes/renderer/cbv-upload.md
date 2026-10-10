# renderer/cbv-upload: cheaper per-draw constant uploads on the replay thread (CBU: CBS / CBN / CBT), ASI 1.7.60-cbu1

2026-10-10, branch `cbv-upload` (from replay-prof f08cc34). Follows 01b-replay-handlers.md section 0 (6A per-draw path) and
the pc2 RPF2 confirmation (captures/pc2/rpf2: UPLOAD in BAR VRAM = 6A 4.1-4.6 us/draw of CPU; UPLOAD in system memory =
1.07 us/draw but GPU +8 ms). Tags: **S** static, **M** measured, **E** estimate.

## 0. Answer

1. **Built, all default off:** CBS (F3 streaming copy), CBN (F1 skip same-list no-op uploads), CBT (F4 CBV table reuse),
   CbvStats census, CbvRecord trace; live keys `Tuning.CBS / CBN / CBT / CBS_FENCE / CBSTATS / CBALL / CBMODE`.
2. **Census on the PC (M, stage 7292, cbu1pc):** ~3,900 graphics CB uploads per frame, avg ~3.0 KB, **~11 MB/frame** into the
   UPLOAD ring (more than the 5.4 MB estimated for pc2). **20 % of the uploads are byte-identical to the slot's previous
   upload in the same list** (CBN skips them; "any list" is 20.4 %, so same-list loses almost nothing). Replaying the
   recorded trace: CBN skips 20 % of uploads and **16 % of the CBV tables** become CBT reuse hits.
3. **Offline proof (M):** stock relocated v2.02 118E28 vs CbuTableCall + CbuStreamCopy, byte-exact for every bound view, on
   8 synthetic streams and on the recorded game trace (30,000 calls, 270 lists, 99.5 MB); test_patterns ALL 248 SITES OK.
4. **PC (M):** fps unchanged in every arm (the PC is GPU-bound and its BAR writes are cheap). On this PC the switches cost
   CPU on the replay thread: 6A CPU 436 ns/draw stock; CBN 547 (tracking memcmp + copy ~0.2 ms/f, while CBV creates fell
   4243 -> 3476/f); CBS 594 (the per-table sfence waits for the WC drain). The win is expected only where BAR writes are
   slow (pc2: 6A 4.1-4.6 us/draw). pc2 decides (section 4).
5. **F2 not worth it (M):** the shaders read 99.5 % of their declared cbuffer bytes (400 sampled VS/PS DXIL blobs).

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

**List cuts (coordinator caveat, S):** the code that re-dirties tables after a cut is still untraced; CBN / CBT never rely on
it: they only reuse a block / table made in the SAME open list (tag + list pointer), never across lists, and every new list
still gets its own SetGraphicsRootDescriptorTable from the same call sequence as stock (CBT calls it itself). The stock
engine's own cross-list reuse of untouched slots (staging CBVs pointing at older-list blocks) is unchanged.

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

`captures/pc/fastab/20261010-1333-cbu1pc` (build E84ED980 = this code minus CBMODE / CBS_FENCE / census fix; tour 7292,
1440p XeSS, saves blocked, lock nioh3-pc as pc-cbu, user ASI / ini (Tour = 0) / Nioh3BossRematch.* byte-identical after).
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

## 4. pc2 spec (ready to send; not queued by this agent)

- **ASI:** `D:\nioh3-wt\cbvupload\mod\Nioh3EngineFix\bin\Nioh3EngineFix.asi`, 1.7.60-cbu1, sha256
  `ACA5031B7F4AAA1C11CD26C581808457358E40BAC7AA47D3E40CCABC5EC04A05` (commit 1f9d329+, branch cbv-upload).
- **ini (on pc2's usual CPU-bound ini, ReBAR ON):** `Patches.CBS=2; Patches.CBN=2; Patches.CBT=2; Profiler.ReplayProf=1;
  Profiler.ReplayProfSec=10; Profiler.Backend=1; Profiler.BackendSec=10; Debug.LiveCtl=1; Profiler.CbvStatsSec=10`;
  **MemStats OFF** (`Profiler.MemStats=0`, no VSForceSysUpload, `GpuOpt.CbvMirror=0`), GpuOpt.FrameOverlap=0, SP / GPUProf / JT off.
- **Run:** Continue spot (CPU-bound, as RPF2), 2 min in the field, then one `cpu_live_sched.ps1` session, live ABBA, 3 pairs,
  RPF on throughout, one key per item (`Tuning.CBMODE` is one key, so combinations do not get overwritten):
  `-Items Tuning.CBMODE=0/1,Tuning.CBMODE=0/9,Tuning.CBMODE=0/2,Tuning.CBMODE=0/6,Tuning.CBMODE=0/7,Tuning.CBMODE=0/16`
  (1 CBS + sfence, 9 CBS no sfence, 2 CBN, 6 CBN + CBT, 7 = CBALL, 16 census only). Exit normally.
- **Send back:** Nioh3EngineFix.log (RPF / BKS / LIVE / CBU lines), report.txt + manifest.jsonl, PresentMon CSV. Digest here:
  `py -I mod/Nioh3EngineFix/tools/cbu_digest.py <log>` -> per arm: 6A own CPU us/draw, replay handlers ms/f, CreateCBV /
  CopyDescriptorsSimple; CPUBusy / GPUTime / fps per arm from PresentMon.
- **Decides:** CBS keep if 6A CPU/draw drops (fence or no fence); CBN / CBT keep if the skipped 20 % of BAR writes + 16 % of
  table copies beat the tracking cost (~0.2 ms/f). Target from RPF2: CPUBusy toward ~13 ms with GPUTime staying ~19 ms.

## 5. F2 / F8 (not built)

- **F2 (copy only the bytes the shader reads): dropped.** Census (`dxc -dumpbin` of 400 random VS/PS DXIL blobs from
  shader_dx12, 582 cbuffers): declared 2,838 B avg, highest constant row read 2,825 B -> **99.5 %** read (100 % for
  cbuffers >= 1 KB). The 2.9 KB is real data, not padding; memcpy already copies the declared size, not size256.
- **F8 (next round, after pc2):** sysmem staging for the CB ring (CPU writes cheap, RPF2 arm B) plus one batched DMA copy
  per list segment into a VRAM ring with the CBVs on the VRAM copy. CbvMirror v1/v2 lost 10-15 ms through per-submission
  fences and per-ECL prologues (rebar-fix-v3.md 3); a design must copy once per list on the DIRECT queue head or one COPY
  queue fence per frame, and CBN cuts its volume by 20 %.

## 6. Files

- `mod/Nioh3EngineFix/src/cbvupload.{h,cpp}`; dllmain (1.7.60-cbu1, Start + `LiveTuning` + 30 s status line), patches.json
  CBU1-3 (+ patches_gen.h), ini keys, build.bat / CMakeLists, `tools/testmatrix/live_keys.py` (CBS / CBN / CBT / CBALL / CBSTATS).
- `tests/cbvupload_harness.cpp` + `tests/run_cbvupload_harness.py` (stock vs CBU on the relocated v2.02 code; optional trace
  files), `tests/cbs_bench.cpp` + `tests/run_cbs_bench.py` (copy variants into a mapped UPLOAD buffer).
- `tools/cbu_digest.py` (per-arm RPF 6A / 69 / 7E rows + CBU census from a log).
