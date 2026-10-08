# remaining-work 2.5: K1 / D / P1E7 / C - measured PC cost and what was built (2026-10-08, ASI 1.7.32)

Rule for this item: build only what has a **measured** PC main/render-thread cost >= 0.2 ms/frame (or a clear stutter
link). No game was run; numbers come from the existing PC captures.

## Method
- `captures\cpuprof-<run>.txt.tsv`, thread `[App]Game` (the main/render thread that records the draws), `self` view. A
  share is % of that thread's wall-clock samples (sampling includes blocked time), so ms/frame = share x mean frame time.
- Frame time = mean `MsBetweenPresents` of the matching `captures\bench-<run>.csv` (r9-all -> bench-r9-ALL-core0.csv);
  r5 = 8.85 ms (113 fps, `pc-submit-overhead.md`). `ingame` / `ingame2` predate the W1 fix and have no bench file: not used.
- Script: one-off (counts per function, `tsv` + `csv` only); reproduce with the function lists below.

## Results (PC, RX 9070 + 7600X, r5 / r6-A1..B2 / r7-A1..B2 / r9-all)

| item | measured function(s) on [App]Game | share | ms/frame | verdict |
|---|---|---|---|---|
| **K1** CmdFlushLimit (frame-end drain) | 418F40 drain spin | 0-2.1 % | **0.00-0.19** (r6/r7/r9 <= 0.13; r5 0.19) | **not built**: below 0.2 in every capture, no stutter link. `patcher\k1_live.fsx` stays the A/B tool (its engine stat +3D28 was never read live) |
| **D** retry quantum 1 -> 0.2 ms | ntdll QPC from 1A74BE0 (the spin) + 1A74130 self (loop) | 0.9-8.8 % + 0-2.9 % | **0.08-0.96** spin (median 0.58) + 0-0.31 loop; 0 in r5 | **built: `[Patches] DQ`** |
| **P1E7** texture-loop prefetch | 1E7050 + 128EF0 + 1290B8 self (the loop it targets) | 2.5-5.3 % | **0.28-0.46** (r5 0.33) | **built: `[Patches] P1E7`**. Caveat: round 9 ran it only in the C419+P4AA+P1E7 bundle, which measured slower (mesh-on 96.0/104.9 vs off 103.6/108.4 fps; on1 had 22 stutter spikes) - needs its own A/B |
| **C** glyph atlas kept across frames | 61B1C + 62000 + 61EA0 + 96A2C0 self (the whole per-frame rebuild) | 1.1-2.5 % | **0.10-0.22** (median 0.14; r5 0.16) | **not built**: the whole rebuild is ~0.14 ms and C removes only part of it (new glyphs still decode); still needs the live `[[Nioh3+4BD0AC0]+37Ch]` read |

D is wait time, not work: the main loop parks in a blind 1 ms QPC spin when the logic side's frame packet is not new
yet. What D removes is the overshoot (a packet ready 0.1 ms in still waits 0.9 ms), so the gain shows up as earlier
frame starts (fps near the cap, latency) in the frames that hit the wait, not as less CPU.

## What was built (1.7.32, `src\drawprep.{h,cpp,asm}`, default off)
- **DQ** (`[Patches] DQ` 0/1/2, `[Tuning] DQ_QuantumUs` 200, 50-1000). The xmm6 quantum load (1A744E4) runs **once
  before the loop**, so the offline-designed displacement patch would never take effect when written live after the loop
  started. The cave replaces the loop's exit test instead: 1A74C45 `comiss xmm6,xmm0; ja 1A74BD0` (5 B) -> `jmp`
  caveDq. Off: the original comiss/ja. On: `comiss xmm0,[quantum]; jp exit; jb loop; exit`, i.e. loop iff
  quantum > elapsed and ordered (same as `ja`). Flags only; every register preserved. Counts retries that came earlier
  than stock (`stats/s drawprep: ... early retries`). DS (sleep in the spin, 1A74BDA) still works: DsQpc asks
  `drawprep::DqQuantumUs()` and only sleeps inside the shorter quantum (at 200 us it never sleeps).
- **P1E7** (`[Patches] P1E7` 0/1/2). Port of `re\scripts_meshlet\meshlet_caves.fsx` cP1E7: 1E72C6 (11 B) -> `jmp`
  caveP1e7 + nop6; prefetch tex[k+2]+68h and the 5 lines of `[tex[k+1]+68h]` that 128EF0/1290B8 stall on, then the
  two displaced loads. Scratch rcx/r10/flags.
- Both: v2.02 only; site + dependency bytes checked at run time (after DS has checked its own); one frozen write with
  no thread stopped inside a site (same scheme as ApplyBatch); 2 = installed but off.

## Offline proof
- `tests\test_patterns.py`: sites DQ / P1E7 + deps (patches.json, kind check), drawprep.cpp's byte arrays == dump,
  DQ head check stops at DS's site, exit/resume = next instruction, no branch into either site.
- `tests\run_drawprep_harness.py`: the real caves vs the original bytes from the same full register state (all GPRs,
  xmm0-15), DQ 60,000 cases incl. NaN/inf/-0/denormals/boundaries (off == original exactly; on == quantum rule; counter
  exact), P1E7 20,000 cases with the texture array ending at a PAGE_NOACCESS page (proves the k+1 / k+2 bounds).
- Register audit (`hook-register-audit.md`): DQ writes flags only (dead at 1A74BD0 and 1A74C4A: `hook_liveness.py
  after`); P1E7 rcx/r10/flags dead at 1E72D1 except rcx escaping at the ret of 1E7050, which 1E7050 clobbers itself.

## A/B (needs the game; one launch each, 2 = installed + off for the B leg)
- DQ: a CPU-limited spot near the cap where the r6/r7/r9 wait shows (shrine, 120 cap). PresentMon avg fps + p50/p99,
  `stats/s drawprep` early-retry count; a profile should show the 1A74BE0 QPC share drop. Expect no change where the
  wait never happens (r5-like, render-bound).
- P1E7 on its own (not with C419/P4AA): ABBA at the r9 spot; KEEP only on a fps win without new spikes.
