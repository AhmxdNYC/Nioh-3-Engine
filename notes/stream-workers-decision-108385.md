# In-play streaming CPU: Opus decision (human #108320), 2026-11-16 ~3:00 am EST

Inputs: DeepSeek maps A/B/C on deepseek/stream-workers-108311 (bb560ba0, 576fd7e9, b29c394e);
GH #70/#93 (i7-6700 4c/8t, freezes 110-163 ms, 6.9-7.5 of 8 threads busy: Scheduler0 71-77 %,
RequestLoad 43 %, Cache.FileLoad 42 %, [App]Game 59 %).

## Decision: make the work CHEAPER, for every CPU. Throttle only as a later fallback.
Why: the maps show waste, not just volume. Cheaper work helps every core count; a throttle only
moves the cost and risks late streaming (pop-in), which breaks "same picture".

Order (each default OFF until measured, same output bytes):
0. Measure first (1 short live run, PC with the game limited to 4c/8t by process affinity):
   LoadStats (MB in->out, inflate CPU-s) + per-thread sampling of FileLoad to split its 42 %
   (submit vs wait vs ring). Decides which of 1-3 matters most on 4 cores.
1. RC read cache (src exists: 96592c1a, default off, never built): repeat block reads were
   60-80 % on Ally. Serve them from memory.
2. ResourceIsReady fast path: skip ResourceDependenciesReady 0x1A6820 when the resource is
   already fully ready (map A: it always walks the tree, per render node per frame).
3. Inflate: one inflate state per worker (inflateReset instead of Init/End per 16 KiB at
   0x420654), later libdeflate. Same bytes out.
Fallback only if 0-3 are not enough on 4c/8t: lower the Windows priority of RequestLoad/FileLoad
threads below [App]Game/render (thread counts and request cap unchanged). Do NOT cut the request
cap (10) or pool sizes: that is slower streaming.

Status: decision only. Building is optimization work under hold #93970: needs the human's go.
