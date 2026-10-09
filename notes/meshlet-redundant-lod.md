# "Redundant" meshlet LOD resolves (9046AA + 8DA16D) - NOT redundant, NOT built (2026-10-09)

Input: pc2 MeshletProbe #692 (bridge #1070, file #174: M_report.txt, M1/M2 run01.log + mlp_events.csv). The MLPC duplicate test
reported 127-128 pairs/window with "identical inputs on the same MeshletSys" (last pair 9046AA + 8DA16D). Question: can the
second resolve be skipped and the first one's buffers reused?

## Verdict
**No.** The two calls are the main-view LOD resolve and the VSM clipmap (shadow) LOD resolve. They run a different kernel
permutation, write an extra output buffer the main resolve does not, and the cluster set may differ by clipmap. The probe called
them identical because its test compared only MeshletSys, the 32-byte block at param_5, levels, roots and cull-code count, not
param_9 / param_10 / param_11 / the culling context. Even a perfect skip would be worth ~0.1 ms/frame on pc2 (~0.4 %), far below
the >= ~5 % rule. Nothing was built, queued or tested in the game; there is no Ally check because nothing ships.

## 1. Who calls what (Ghidra, re\decomp.cmd 0x9046aa 0x8da16d 0x236fbd 0x2373f7)
Both call MeshletResolveLOD 531948 on the same MeshletSys (`view+18D0h`):

| | 9046AA (in 904610) | 8DA16D (in 8DA0A8) |
|---|---|---|
| caller | 237044 (scene view render node, vtable): LOD, then MIC@23729A / PRE@2372E8 of the same view | 236E14 (from 1F8340; 3 more callers 579E7C / 33CE648 / 34578EC). GPU parent marker `[Clipmap 4]` |
| param_5 (the "camera block") | 8 LOD distance thresholds from 237488(view, i) | the same 8 thresholds, but per clipmap: `t = override >= 0 ? override : (t >= 0.01 && base <= clip ? (clip - base + 1) * step + t : t)` (view +2678h / +2674h / +2670h). Equal to the main ones only when the bias does not apply, which is when the probe's hash matched |
| param_9 (-> ctx+1434h) | the view value from 51DE8C | 1.0f |
| param_10 (extra UAV, ctx+13D0h, bound for the level kernel, UAV-barriered every level) | 0 (or +16A0B0->+70h on the reset bit) | a per-view buffer `+416F8h[i]` (default +16A0B0->+68h); 236E14 hands the next one (+41708h[i]) to 899B50 afterwards |
| param_11 | 0 | 1 -> 91BF54(ctx, 1): forced permutation flag |
| ctx fields | - | +1430h = clip << 4 \| 1, +1438h, +1298h state +8 = 1 / +1444h = 1 for the call (shader model >= 0Ch), reset after |
| level-0 kernel (pc2 probe) | MshLod#2 63c07c21 / MshLod#10 7c6d0fc6 | MshLod#7 fe77ee2c (every 8DA16D row) |

So the outputs are not the same buffers or contents: 8DA16D's kernel writes the VSM per-view buffer (param_10) that the main
resolve never writes, and it selects clusters with a different predicate permutation into the shared MeshletSys lists
(+16A170 / +16A160 / +16A168). Skipping it would leave the clipmap's consumers (MIC@235993 / PRE@2359D3, 899B50) with the main
view's selection and a stale param_10 buffer: wrong shadow LODs, not a same-picture skip.

## 2. Cost ceiling (why it would not matter even if it were redundant)
pc2 dense (Mirage 7292, GPUBusy 27.2 ms): all 8DA16D regions = **0.114 ms/frame (M1) / 0.095 (M2)**; ~1.0-1.1 ms per call
in 0.20 calls/frame (~19 % of frames, sequence 2). Only about half of those were "identical" pairs -> a perfect skip = ~0.05-0.1 ms
= **0.2-0.4 %**. The expensive item (MshLod#1 e6a62bda 0.56 ms/frame, MshLod#2 0.45 + 0.27 + 0.18) is the MAIN resolve at 9046AA,
which runs once and has no duplicate. RX 9070 PC: the whole front-end is ~0.92 ms -> the 8DA16D share is ~0.015 ms. Ally: even at
x2-4 latency, <= ~0.3 ms in a 30+ ms frame (< 1 %). Below the >= ~5 % rule on every device -> stopped.

## 3. Probe fix (mlprobe.cpp, measurement only)
The duplicate test now also compares param_9, param_10, param_11, the context pointer (param_3) and ctx+1430h. The next
MeshletProbe run should report 0 identical pairs for 9046AA + 8DA16D. Build + tests/test_patterns.py: ALL 243 SITES OK.

## 4. What is left on the meshlet front-end
The same as meshlet-frontend.md section 6: the main resolve's own kernels (MshLod#1 0.56 ms, level 0..3 ~0.97 ms on pc2) are the
target. Removing a pass is not one of the options.
