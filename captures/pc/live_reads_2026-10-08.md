# Live reads, Nioh 3 PC, 2026-10-08 (read-only, gameplay idle, stop flags G+588..58D = 000000 throughout)
Tool: `patcher\live_reads_ro.fsx` (VM_READ|QUERY handle only, 1 Hz). Image base this run 0x7FF64AF80000 (RVAs below).
Raw: `live_reads_pagepool.csv`, `live_reads_gl1.csv`. Total ~2 min, no writes.

## A4 meshlet LOD table (done)
g=[+0x45B9E30], byte g+0xB08 = 1, T=[g+0xA08], [T] block count = 8 entries of 0x1C bytes (7th dword is an INT = category key 0..7, not a float).
Entry floats [0..5] (identical for every category except e[0]):
| cat | e0 (base) | e1 | e2 | e3 | e4 | e5 | key |
|---|---|---|---|---|---|---|---|
| 0 | 2 | 2 | 1.5 | 1 | 1 | 1 | 0 |
| 1 | 2 | 2 | 1.5 | 1 | 1 | 1 | 1 |
| 2 | 2 | 2 | 1.5 | 1 | 1 | 1 | 2 |
| 3 | **8** | 2 | 1.5 | 1 | 1 | 1 | 3 |
| 4 | **0** | 2 | 1.5 | 1 | 1 | 1 | 4 |
| 5 | 2 | 2 | 1.5 | 1 | 1 | 1 | 5 |
| 6 | 2 | 2 | 1.5 | 1 | 1 | 1 | 6 |
| 7 | 2 | 2 | 1.5 | 1 | 1 | 1 | 7 |
Scene cfg [+0x45B9E58]: +0x8BC = 2.0, +0x8C0 = 0.0. MESHLET_QUALITY (0x45BAFB0) = 2 (STANDARD).
Formula result with the A4 recipe (param = 2.0 sentinel, entry index 1+q = e[3] = 1.0 at q=2): out = {2,2,2,8,0,2,2,2} (cat 0 uses 8BC=2; +8C0=0, cap 10 no effect).
At q=1 the multiplier is e2=1.5 (cats: 3,3,3,12->10 cap on 0-3,6 only after add; cat3 = 12 capped 10, cat4 0), at q=3 e4=1. So LIGHT is the 1.5x entry: the table is flat except cat 3 (8) and cat 4 (0 = off/unused).
Not read: the 8 live params (RenderConfig+0x6EC0) and outputs at +0xA108 (owner object not located); sentinel 2.0 assumed from code default.
The key map vector at T+0x20 read as inverted (+8 > +0x10), so entries are taken in storage order (key field matches position 0..7).

## GL1 list sizes (partial)
Proximity pass E266B0 list: P=[[0x45B9E40]], V=[P+0x48]: vector {begin,count} = **4380** 16-byte entries (qword V+8), constant over 20 samples (idle).
State pass 2866F0 (E26680 from 89A328, same V struct): V+0x60 count = **314**, V+0x78 count = 0 (which of V's vectors the 89A328 loop walks was not confirmed from the decompile; 4380 is also a candidate).
Mission object set g_4759950 (20B900): head word [0x4759950+8] looked like a module pointer, [+0x10]=2; layout unconfirmed, count NOT obtained.
So 4380 proximity entries/tick is 4-5x the 800-1000 estimate in gamelogic.md; the GL1 prefetch case holds (more entries, still pointer-chasing) but the saving estimate should be revisited. Caveat: idle, one scene.

## R1 page pool (25 s at 1 Hz, pool = [0x45BA588]+0x61A0; D20=1, frame D10 ticks ~109/s)
Device pointer is g_RenderGlobals itself ([0x45BA588]), not +0x70.
- cur frame counters (+918/+91C/+90C) are sampled mid-frame/mid-clear so jittery: hits 24..7370, **misses 0, queue-full 0** every sample.
- per-scan copy (+A30/+A34/+A24): hits 5750..7540 per scan (~7k), **misses 0, queue-full 0** in all 25 samples.
- Hits per scan (~7k, one scan per ~4 frames, ~27 scans/s) = ~190k hit-requests/s of already-resident tiles: re-requests are what the stat counts. Whether those hits are charged against the 16/128 per-frame tile budget cannot be seen from counters (needs 1290B8 free-budget math); with 0 misses and 0 queue-full in an idle scene there was no starvation signature. Not tested during streaming/camera cuts.

## Task 4 (hash names)
Not attempted: no runtime name table was located in the cheap look (the hashes are immediates in generated stubs, A3); a full-process string hunt (6.5 GB) was judged too heavy while the game runs.
