# Ally main-thread draw prep, pass 2: new same-result savings? (2026-10-08)

Static analysis + the existing Ally SP logs only. No game, no bridge, no Ghidra run, nothing built (no item reaches the
0.5 ms bar with a bit-exact proof). Inputs: `captures\ally\gpu_budget_1531\*.log` (ASI 1.7.31, USER-class presets, GF on,
9 logs, 216 gameplay SP windows of 5 s), decomp `D:\nioh3-re\decomp`, dump disassembly (capstone), the PC cpuprof r9 callers.
New tool: `py -I re\scripts_cpu\sp_pool_ms.py <logs...>` (pairs every SP report with the OSD fps/draws line -> ms/frame).

## TL;DR
- **No new bit-exact draw-prep item >= 0.5 ms exists in what the Ally data can see.** The visible draw prep is flat
  (largest single row 1.0 ms) and already change-detected; the largest untouched block (state commit, ~1.1 ms) has no
  redundant output to skip (s.3 D2).
- **Two corrections to whats-left fact 1 / #9:**
  - `[ntdll] <- 5DA48` (0.95 ms in S1, 0.66 ms pooled mean) is a **stale stack label of the GF fence wait**, not the
    111064 `Sleep(1)` loop (proof in s.2). SR111's Ally target is ~0.
  - `418F40` (0.80 ms) is the frame-end drain spin = dispatcher/GPU backpressure, i.e. wait. Real main-thread work is
    **~17.6 ms**, not 18.4 (ceiling ~57 fps, not 54-56).
- **10.2 of the 18.4 ms of game code are invisible**: every SP report keeps only its top 25 rows. The next >= 0.5 ms item,
  if any, is in that tail. Cheapest next step: one Ally session with `[Profiler] TopN = 200, StackBytes = 16384` in the
  same session as the whats-left #3 CPU bundle (s.5).
- Best small same-result items on the Ally (each < 0.5 ms, none built): **W1b** (studied 2026-10-06, never built) 0.15-0.25
  ms, **RP** RenderParamsAssign as range copies 0.07-0.1 ms, **S2F** per-frame memo in 3C4B74 ~0.05 ms. With the #3 bundle
  (PP, MA, P1E7, FPC) the total is ~0.8-1.3 ms: enough to move the main-thread ceiling from ~57 to ~60 fps.

## 1. Measured Ally budget (USER, GF on, 216 windows, fps median 34.3, 7604 draws/frame, frame 29.15 ms)
`py -I re\scripts_cpu\sp_pool_ms.py captures\ally\gpu_budget_1531\*.log`. ms/frame = share x frame time per window;
median (mean). Rows are the logged .pdata fragments (1E81AE = DrawEmit body, 1E7154 = TextureStreamUpdate loop, 1D8778 /
1D84F8 = StageSlotTableUpdate loops). Rows that fall below the per-report cut in some windows have medians < means.

| block | rows | ms/frame | covered by |
|---|---|---|---|
| GF fence wait (idle, GPU-bound) | `[ntdll] <- 1180A4` + `<- 5DA48` | 9.49 + 0.29 (0.66) | GF (wait, not work) |
| DrawEmit, per draw | 1E81AE, 1E80B0, 1E8065 | 0.99 (0.96) = **130 ns/draw** | MA 1E835B, FPC; rest miss-bound |
| frame-end drain spin | 418F40 | 0.80 | wait (S1 PAUSE exists) |
| memcpy into stream / upload | B94230 + B94250 | 0.58 + 0.48 | none same-result (s.3 M) |
| meshlet bookkeeping | 4AA34, 419A8, 4B104, 4DB6C, 5DF70 | 0.57 + 0.23 + 0.18 + 0.17 + 0.15 | MA (+P4AA/C419 caves) |
| texture streaming | 1E7154, 128EF0, 1290B8 | 0.56 + 0.35 + 0.17 | P1E7 |
| state commit family | 1D8778, 1D84F8, 1DAF6C, 1D7340, 1E5854, 1E0B64 | 0.23 + 0.20 + 0.21 + 0.23 + 0.15 + 0.09 | none (s.3 D2) |
| page-pool sweep | 2523FC | 0.36 | PP (PC KEEP -0.26 ms) |
| draw-list walk + group length | 39E20, 3BC0C | 0.36 + 0.23 | B |
| glyph padding clear (WC) | 61B1C | 0.23 (0.32) | **W1b** (not built) |
| render-params copy | 247950 | 0.15 (0.11) | **RP** (new) |
| S2F technique params | 3C4B74 | 0.10 | **S2F** (new) |
| listed game rows | | **8.12** | |
| below the top-25 cut | | **10.21** | unknown |

## 2. `[ntdll] <- 5DA48` is a stale label (SR111 is not a 0.95 ms main-thread item)
- 5DA48 (68 B) calls only `1A5DA0` (a 32-byte struct copy, no calls) and `111064`. Its only return addresses are 5DA61 /
  5DA78. For RIP-in-ntdll to be attributed *live* to 5DA48, one of those two would have to enter ntdll without a deeper
  Nioh3 return address on the stack. Neither can.
- 111064's wait is `mov ecx,1; call [Sleep]` at **194ADC1** in its cold chunk (.pdata 194AD66-194ADCF, chained to
  111064). `sp.cpp IsReturnAddress` accepts 194ADC7 (`FF 15` form), so a live sample there is logged as `fn 0x194AD66`
  and folded to **111064**, never 5DA48 (checked: `sp_aggregate.primary(0x194ADC7) = 0x111064`, `primary(0x5DA78) = 0x5DA48`).
  Its other calls (ResourceIsReady etc.) push 1110xx return addresses, which also fold to 111064.
- So the row is a stale slot. The GF event wait runs in ASI code and a deep Wine syscall frame, beyond the default
  2 KB stack scan, and the scanner meets old return addresses first. That is the same mechanism as the 1180A4 label.
  Per window, the 5DA48 row anti-correlates with the fence-wait row (**r = -0.57**, 216 windows) and not with the
  game-code share (r = 0.02). In windows where it jumps to 12.8-13.1 %, the fence row drops to 20-22 %.
- Consequence: whats-left #9 (SR111) has no measured steady-state Ally target. The G5 load-phase 28 % is the same
  label. A real 111064 sleep would show as `<- 111064`; FnProbe 111064 stays the cheap confirmation if wanted.

## 3. Draw-prep candidates studied in this pass

| id | what | Ally target | est. saving | proof approach | risk | verdict |
|---|---|---|---|---|---|---|
| D2 | skip-unchanged stage commits (DrawStateCommit 1DAF6C -> 8 x StageBindingsApply -> 1D8720/1D84A0/1D8220/1D7EB0) | ~1.1 ms family | **~0 bit-exact** | - | - | **NO** (below) |
| D2n | NULL-set re-walk: Apply with `set == last == NULL` still re-runs 1D8720(tbl, 0) over the 5 extra tables (+A8h) | part of 0.23 | <= 0.1 ms | needs a writer-generation guard on every slot writer; first a counter probe of how often the branch runs | medium | probe only |
| M | memcpy into the KTGL stream (B94230/B94250; PC callers 242EB8 19 %, 371863C 13 %, 242FC4 11 % / 4DD4C 33 %, 1D7340 24 %) | 1.06 ms | 0 | - | - | **NO**: token 0x66 copies the blob per call (242CD4 already caches the *build* via +5866h and the field compares); dedupe would change the stream; 4DD4C is the parity double-buffer copy |
| G2 | prefetch next draw's `**item` and `+18h` in the 1DC2F0 group loop | 0.99 ms (130 ns/draw) | 0-0.2 ms | prefetch only, cannot change results; A/B decides | low | research; the round-9 prefetch caves were neutral or negative |
| **W1b** | 61BAC: W1's `rep stosd` with count = pad = 1 on WC memory -> store-only loop (22 B in place, ui-mainthread.md 1) | 0.23-0.32 ms | **0.15-0.25 ms** | native harness: original vs new bytes on random pad 0..8, row pointers into a buffer; dst bytes identical; register audit | very low | **next to build** (below the 0.5 bar) |
| **RP** | RenderParamsAssign 247950: 29.6 KB of straight-line dword moves (about 2.4k) for one ~0x3D30-byte block; also evicts L1i at each call | 0.11-0.15 ms | 0.07-0.1 ms + i-cache | harness derives the exact copied byte ranges by running the original on random src/dst; the cave copies the same ranges (padding untouched); 300k random runs bit-exact | low | candidate |
| **S2F** | 3C4B74, once per technique bind: two linear hash scans (7602870Dh, F385931Eh) over the RenderParams list (+3D30h), plus a by-name constant lookup. The results are constant within a frame | 0.10 ms | ~0.05 ms | memo keyed on list ptr + count + stream epoch (FPC pattern), native test vs the original | low-medium | small |
| PAR | parallel draw-list segments | - | <= 0 on the Ally | - | - | **NO-GO** unchanged (parallel-drawprep.md s.4: one context, order-dependent shadow, per-draw side effects) |

**D2: why nothing is skippable bit-exactly.**
- The applies already skip at set level: `if (!forced && set == last) -> only re-emit 0x29/0x23/0x28 if ctx shadow != *set`.
- `forced` (`+13Ch` dirty, or `ctx+2578h != *lastSet`) means another state object emitted, or the program changed.
  It then re-dirties every slot with `+12h != 0` by design: the new program's slot layout may differ.
- 1D8720's loop (hot fragment 1D8778) *is* per-slot change detection. It compares the descriptor id against `+0Ch`,
  grows the dirty range only, and sets the `[tbl]` / `[tbl+8]` bits that 1E0B64 then flushes into tokens.
- Skipping a content-identical new set would need the same per-slot compare. The cost is the 0x90-byte slot stride
  (2-3 lines per slot), not redundant output.
- DrawStateCommit itself recomputes the stage tables only on a program change (`+20h != prog`).

## 4. Already-designed items, with their Ally targets (whats-left #3 bundle)
| item | Ally target (this pool) | est. Ally saving | status |
|---|---|---|---|
| PP | 2523FC 0.36 ms | ~0.27 ms (75 %) | built, PC KEEP; ALLY profile PP = 0 |
| MA | 4AA34 0.57 + 419A8 0.23 + 4B104 0.18 + DrawEmit 1E835B | 0.15-0.3 ms | built 1.7.39, launch flag |
| P1E7 | 1E7154 0.56 + 128EF0 0.35 + 1290B8 0.17 | 0.1-0.3 ms | built 1.7.32, live switch, needs its own A/B |
| FPC | 3C7750/3C768C (in the tail on the Ally) | ~0.1 ms | built |
| W1b | 61B1C 0.23-0.32 | 0.15-0.25 ms | design only |
Bundle + W1b ~0.8-1.3 ms: main work 17.6 -> ~16.4-16.8 ms, ceiling ~57 -> ~60 fps (measured only in the A/B).

## 5. Next measurement (the only way to a >= 0.5 ms item now)
One Ally session, USER preset, GF on, same spot. Add to the #3 bundle ABBA:
```
[Profiler]
SP = 1
TopN = 200            ; was 25: 10.2 of 18.4 ms of game code fell below the cut
StackBytes = 16384    ; fewer stale "<- fn" labels (1180A4 / 5DA48)
ReportSec = 10
```
Digest with `sp_pool_ms.py` (both arms). Then rank the tail by ms/frame. A tail function or family >= 0.5 ms with no
note becomes the next pass-3 target.

## 6. Files
- `re\scripts_cpu\sp_pool_ms.py`: new, the pooled ms/frame digest plus the 5DA48/fence correlation.
- Read and not changed: decomp 0x1d8720, 0x1d69cc, 0x1d5f20, 0x1daf6c, 0x1dc2f0, 0x1e80b0, 0x242eb8, 0x242fc4,
  0x247950, 0x3c4b74, 0x5da48, 0x111064, 0x1a6030; dump disassembly of 5DA48, 111064 + cold chunk 194AD66, 1D8720,
  242CD4, 61B1C.
