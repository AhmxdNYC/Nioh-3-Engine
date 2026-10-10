# renderer/cbv-upload: cheaper per-draw constant uploads on the replay thread (CBU: CBS / CBN / CBT), ASI 1.7.60-cbu1

2026-10-10, branch `cbv-upload` (from replay-prof f08cc34). Follows 01b-replay-handlers.md section 0 (6A per-draw path) and
the pc2 RPF2 confirmation (captures/pc2/rpf2: UPLOAD in BAR VRAM = 6A 4.1-4.6 us/draw of CPU; UPLOAD in system memory =
1.07 us/draw but GPU +8 ms). Tags: **S** static, **M** measured, **E** estimate.

## 0. Answer

__ANSWER__

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

__HARNESS__

## 3. PC run (M)

__PC__

## 4. pc2 spec (ready to send; not queued by this agent)

__SPEC__

## 5. F2 / F8 (not built)

__F2F8__

## 6. Files

- `mod/Nioh3EngineFix/src/cbvupload.{h,cpp}`; dllmain (1.7.60-cbu1, Start + `LiveTuning` + 30 s status line), patches.json
  CBU1-3 (+ patches_gen.h), ini keys, build.bat / CMakeLists, `tools/testmatrix/live_keys.py` (CBS / CBN / CBT / CBALL / CBSTATS).
- `tests/cbvupload_harness.cpp` + `tests/run_cbvupload_harness.py` (stock vs CBU on the relocated v2.02 code; optional trace
  files), `tests/cbs_bench.cpp` + `tests/run_cbs_bench.py` (copy variants into a mapped UPLOAD buffer).
- `tools/cbu_digest.py` (per-arm RPF 6A / 69 / 7E rows + CBU census from a log).
