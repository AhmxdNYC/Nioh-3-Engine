# Level-load swap burst (class A): arena heap free path and the LM sweeper design

Date: 2026-10-07. Static RE only, game not run. Inputs: notes\stutter.md §3.3, notes\memory-leaks.md §5.1. Check: `py -I re\scripts_stutter\loadmem_check.py` -> ALL LOADMEM CHECKS OK.

## 1. Free path (RVAs)
- `EngineArenaInit` 294C2E0: one `VirtualAlloc(MEM_COMMIT)` of 6 GiB (294D214), then ~62 pool wrappers (registry `arena+3F5C50`, 8 B each; arena object = static 475A440, pointer in `[4B50348]`, flag byte `+3F5F00`).
- Every pool wrapper carries one **heap** object (0xB0 B, built in place; vtable 408F940, variant 408FB78; init 1343E48 / 9C0A3C / 13474A0 / 13475E0). Small blocks (<= 0x80 B) go through a front end (12E36C) that takes chunks from this heap.
- Heap vtable: `+38h` HeapAlloc 3123B4 (locked) -> HeapAllocLocked 31242C; `+58h` **HeapFree 130D0C** (ends with `jmp _Mtx_unlock` at 130E8C); **HeapReset 9C0A94** (whole region becomes one free chunk; reached through vtable thunks 30AB828 / 30AB840 / 3254080).
- Nothing ever decommits: VirtualFree runs only at shutdown (1FE6948). Freed chunks go back on the heap's bins; **their pages stay committed and dirty**. On the Ally (RAM = VRAM pool, 15.3 GiB, zram) the new stage's allocations push those dead-but-dirty pages to swap: that is phase A.

Heap object offsets: +10h payload bytes, +18h header size (0x20), +20h region base, +28h first chunk, +30h end, +38h bin table (16 x 8 B, at the region head, before chunk 0), +44h bin count, +48h top bin, +50h peak, +58h used, +60h mutex (lock B614A8, unlock B61524).
Chunk: `[0] prev physical chunk, [8] payload size (negative = allocated, positive = free), [10h] bin prev, [18h] bin next`; payload at +20h; next chunk at `c+20h+|size|`. HeapFree coalesces eagerly with both neighbours, so a free chunk is maximal.

## 2. Free payload is never read before it is rewritten
Decompile of 130D0C, 31242C, 9C0A94 and the vtable slots: the allocator touches **only the four header qwords** of free chunks (size test, bin links, split). A split writes all four fields of the new remainder header (31242C), including the alignment-pad header. HeapFree writes only neighbour headers. Game code reading a block after free is a use-after-free (the old bytes are stale, not zero), and reads of fresh `alloc` memory before writing already see arbitrary old bytes. The residual risk is a latent use-after-free race, so the sweeper only discards chunks that stayed free **unchanged for >= 2 sweeps (>= 1 s)**.
Model check (re\scripts_stutter\loadmem_check.py): 20 seeds x 6000 random alloc/free/split/merge ops, each run twice (with and without zeroing the discard ranges). Returned addresses and all chunk headers are identical, live payloads intact, ranges never touch a header or the next chunk (2.3 GiB zeroed in total).

## 3. Fix design (LM, default OFF, no game bytes patched)
A sweeper thread in the ASI (snippet in §5):
1. Find heaps: scan the arena object (4 MiB at `[4B50348]`) for qwords equal to `image+408F940` or `image+408FB78`; validate `[+18]==0x20` and `[+20]` inside the 6 GiB block.
2. Every `LM_PeriodMs` (1000): per heap lock with the game's `_Mtx_lock` (B614A8), walk the bins (`[+38]`), and for each free chunk with payload `S >= LM_MinKB` (256) take the range `[pageUp(c+20h), pageDown(c+20h+S))`. A chunk seen with the same `(c,S)` in the previous sweep and not yet reset gets `VirtualAlloc(a, n, MEM_RESET, PAGE_READWRITE)`; remember `(c,S)`. Unlock.
3. Start-up self-test: 64 MiB scratch, touch, `GetProcessMemoryInfo`, MEM_RESET, re-read. If the working set does not fall by >= 50 %, the sweeper stays off and logs it (a lazy MADV_FREE-style reset can false-negative; `LM_Force=1` skips the test).
`VirtualAlloc(MEM_RESET)` is used instead of DiscardVirtualMemory/OfferVirtualMemory: documented on Windows and implemented in Wine's NtAllocateVirtualMemory, where it ends in madvise on the backing pages (Wine mapping not verified in this tree, hence the self-test and the MemStats probe). It is the game process's own memory call; no system tweak.
Semantics: pages stay committed, contents become undefined (zero or old), a later write re-faults a fresh zero page.

## 4. Expected effect (estimate, not measured)
- Swap: the freed stage-pool ranges are the only large dead-dirty set the game owns (stage pools are 250-1450 MB), so 40-100 % of the +0.75-1.1 GB load swap burst can be dropped instead of swapped. The part caused by the ~2 GB of new GPU memory (GTT) remains.
- Class A frames (40-134 ms, CPU 86-96 % from zram compression): shorter and lower roughly in proportion to the swap avoided. No effect on class C (GPU-bound 190-305 ms) or steady play. Cost: re-faulting zero pages on reuse (about 0.2-0.5 ms per 100 MB), cheaper than swap-out plus swap-in.
- Risk low (headers excluded, 1 s stability, heap mutex held). Revert: `LM = 0`.

## 5. ASI snippet (new file, not wired; the src owner integrates)
```cpp
// loadmem.cpp  [Memory] LM=0 LM_PeriodMs=1000 LM_MinKB=256 LM_Force=0
// MtxLock = IMG+0xB614A8, MtxUnlock = IMG+0xB61524; heaps: scan *(IMG+0x4B50348) (4 MiB) for vtable IMG+0x408F940 / 0x408FB78
struct Key { uintptr_t c; int64_t s; };
static void Sweep(std::vector<uintptr_t>& heaps, std::unordered_map<uintptr_t, std::vector<Key>>& seen, size_t minS) {
  for (uintptr_t h : heaps) {
    if (MtxLock((void*)(h + 0x60))) continue;               // returns 0 on success
    auto& prev = seen[h]; std::vector<Key> now;
    int64_t* bins = *(int64_t**)(h + 0x38); int nb = *(int*)(h + 0x44);
    for (int b = 0; b < nb; ++b)
      for (uintptr_t c = (uintptr_t)bins[b]; c; c = *(uintptr_t*)(c + 0x18)) {
        int64_t S = *(int64_t*)(c + 8); if (S < (int64_t)minS) continue;
        bool stable = false; for (auto& k : prev) if (k.c == c && k.s == S) { stable = true; break; }
        if (stable && !AlreadyReset(h, c, S)) {
          uintptr_t a = (c + 0x20 + 0xFFF) & ~0xFFFull, e = (c + 0x20 + S) & ~0xFFFull;
          if (e > a) { VirtualAlloc((void*)a, e - a, MEM_RESET, PAGE_READWRITE); MarkReset(h, c, S); }
        }
        now.push_back({c, S});
      }
    prev.swap(now); MtxUnlock((void*)(h + 0x60));
  }
}
```
`AlreadyReset/MarkReset`: small set of reset `(c,S)`, entries dropped when the chunk no longer matches. Log every 30 s: `lm: heaps N, reset X MiB, sweeps, max sweep ms`.

## 6. Ally test plan
1. Baseline, `LM=0`: MangoHud (swap_used, ram, gpu_vram, frametime) + MemStats working set across 3 loads (title->level, shrine travel x2).
2. `LM=1`, same route; per load compare the swap delta (expect below +0.75-1.1 GB), count and length of 40-134 ms frames in the first 12 s, load time, MemStats working-set drop after the stage frees (within about 2 s), final GPU memory unchanged (6.8 GB).
3. Verify no crash or corrupt asset over 3 loads + 20 min play; the `lm:` line shows hundreds of MiB reset at load start; self-test line reports the working set fell.
4. If the working set does not fall (lazy reset) but swap still drops, keep `LM_Force=1`.
