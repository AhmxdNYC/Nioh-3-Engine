# Level loads: phase timings, the load pipeline, same-result speedups (Steam v2.02)

Date: 2026-10-08. Inputs: Ally mod logs + MangoHud CSVs in `captures\ally\` (nothing new run), static RE (decomp_ro,
dump scans). There is no PC load capture. Scripts: `re\scripts_load\load_timeline.py` (5 s buckets of a MangoHud CSV
inside the 58A window), `load_sp.py` (SP rows in the load window vs the 4 min after it), `ls_sites.py` (LoadStats sites).
Symbols: `re\symbols\incoming\loading.tsv` (30 new) + `loading-renames.tsv` (5), merged.

## 1. What the logs show (Ally)

**Window = engine stop flag G+58Ah** (set and cleared by the mission-script command Pause::SetFlag 23A763C).

| log | route | 58A window |
|---|---|---|
| 175-L1 / L2 / L3 (3 mod configs) | same save, title -> level | **118.7 / 118.7 / 118.7 s** |
| v7.1 (MAX settings) | same | 118.3 s |
| v4 / v5 (Futamata, lightest 50 %) | same destination | 100.7 / 84.5 s |
| v6 / v9 | other | 132.0 / 94.0 s |

**Phases** (v4/v5 MangoHud, `load_timeline.py`; the L-run GP lines agree):

| phase | time | frames | CPU | GPU | memory |
|---|---|---|---|---|---|
| A | first ~25-30 s | 30-57 ms | **85-95 %**, 15-19 W | 60-75 % at **800 MHz** | v5: swap +907 MB in 10 s (zram, CPU-bound); v4: no swap, CPU still 87-94 % |
| B | to ~-20 s | 50-65 ms (max 80-130) | 70-85 % | 80-85 % at 900-1100 MHz | VRAM +15-40 MB/s |
| C | last 12-20 s | **190-300 ms** | 52-62 % | 90-92 % at 1.7-2.0 GHz | VRAM flat |

- GPU power is pinned at 18-19 W in every phase: the APU is power-shared, so CPU work in A/B costs GPU clock.
- Rendered frames per 30 s fall from 686-801 (first bucket) to 304-406 (last); markers per frame rise from 47-61 to
  121-133 (gameplay after the load: 62-85). The full scene renders behind the cover in the second half.
- VRAM at the clear is 6.3-6.7 GB and keeps rising for 20-30 s after it: the cover does not wait for streaming.
- SP (`load_sp.py` on L1): main thread 50 % FenceWait Sleep(0) + 19 % GfxFencePollWorker (waits on the GPU);
  dispatcher 63 % waiting on the ECL ring + 22 % in the SRW lock of 7F64DC (3 % after the load); ResourceLoad pool 1
  wake storm 26-165k SetEvent/s through the whole window (v7.1).

**Verdict.**
1. **The 118.5 s windows are not throughput-bound.** Four loads of one route agree to +-0.2 % although their frame
   rates differ by 20-30 % (L1 406 vs L2 321 frames in the last 30 s) and v7.1 runs MAX settings. CPU-, GPU- or
   I/O-bound work is never that reproducible; a timer or a fixed-length sequence (most likely an RTM in-engine
   sequence or movie behind the cover: 121-133 markers/frame like an event scene) ends that window. No load speedup
   can shorten such a window; LoadStats measures how much of it is real loading.
2. Not I/O: streamed textures and meshlets (95 of 112 GiB in `root.rdb`) are stored uncompressed, and VRAM grows at
   15-40 MB/s on an NVMe that reads 1-3 GB/s.
3. Not decompression wall time (estimate, to be measured): zlib runs per 16 KiB chunk, already spread over job
   workers; ~1-2 GB inflated per load ~ 4-8 CPU-s on the Ally ~ 1 s of wall time.
4. Where throughput matters (A/B on v4/v5/v9-type loads) the bound is **CPU power**: CPU 85-95 % while the GPU sits at
   800 MHz. Futile CPU work (pool-1 wake storm, Sleep(0) fence spins, zram compression) slows both.
5. Phase C is GPU-bound frames under the cover (VSM blackout floor 64 tiles/frame, `re\scripts_stutter\ld_verify.py`)
   while the load end waits for characters: LoadIsLoadEnd's ready wait counts **frames** (dt60, timeout 120 = 120
   frames at target 60), so 4-5 fps frames stretch it to up to 24-30 s.

## 2. The load pipeline (static)

| stage | where | notes |
|---|---|---|
| request managers | ResourceLoadIdlePoll **1FEE28** (job 1343720) and twin **1FF320** (job 13438B0) | pending [m+200h]+[m+210h] (B: +1E0h/+1F0h); active vector +230h..+238h (B: +218h..+220h). Each pass takes min(pending, cap - active); **cap = GameMain+4Dh ? 1 : 10** (1FEE80 `and r15,-9; add r15,0Ah`; +4Dh is a boot param, CreateGameWindow 298D6C8). Steps run as ResourceStepJobs on **TrySync (pool 6, 3 threads at every WorkerCap)** |
| polling waits | **Sleep(1)** at 1FEE68 (A idle), **1FF18C** (A: pass without progress -> wake pool 1 + Sleep(1)), 1FF371 (B idle) | completion is noticed only by the 1 kHz poll: up to ~1.1 ms latency per step under Proton |
| archive reads | FileReadAsyncSubmit **312F7C**: overlapped ReadFile with offset (site 31305A); FileReadSyncAt 312D98 (312DD2) | `package\root.rdb` + 148 fdata; one read per entry (IDRK header + chunks) |
| decompressor | **zlib 1.1.4** (inflate B34670, "inflate 1.1.4 Copyright 1995-2002"), one inflateInit/inflate/End per **16 KiB chunk** with a 46 KiB stack arena (ZlibInflateChunk16K 420654; variants 324D064, A09A40) | InflateStreamRead 258930: 1 chunk inline; 2+ chunks -> **CInflateTask jobs** (4205BC), 1 chunk per job below 51 chunks, then up to vfunc+48h per job; the reader blocks in JobGroupWait 3E7CC (pool-1 wakes) |
| what is compressed | root: 190k entries 29.6 -> 12.2 GiB zlib (TexContext, ModelData, G1A, object/name DBs); 51k entries 99.9 GiB raw (StreamingTexContext, StreamingMeshletModelData); system: 0.6 -> 0.3 GiB | `data\rdb_list.tsv` |
| PSO creation | stage PSO lists: PsoListLoad 3483E04 -> PsoListPreloadJob jobs -> PsoPreloadGfxCreate 2EFE440 / Compute 2EFE1CC (outside the cache lock); runtime misses 11970A / 119AE1.. | on vkd3d the cost is the pipeline compile (Mesa cache makes repeats cheap) |
| uploads | UploadHeapSuballoc 62D688 ring; full ring -> TextureStagingAllocDedicated 9C2EE0 (9 call sites) | notes\stutter.md §3.3 |
| load end | script Load::IsStageLoadEnd A40A30 ([45B9E58]+760h == 3), Load::IsLoadEnd 3B33B0 -> **LoadIsLoadEnd 3B3424** | waits for every character (LoadCharacterReady 3B39AC: model +804h == 6); accumulator 45C84A4 += dt60 per call, done when all ready or > 120 (timeout) |

WorkerCap = 10 shrinks ResourceLoad 7 -> 6 and RequestLoad 8 -> 6 threads; TrySync stays 3, KIDS_MMM_SUB_REQLOAD 32 and
TS_MESHLET_REQ 14 are fixed. With the CPU at 85-95 % in phase A, more threads would not run more work.

## 3. Same-result speedups, ranked (Ally, seconds per load, estimates until LoadStats)

| # | fix | same result because | expected | condition (LoadStats) |
|---|---|---|---|---|
| 0 | **LoadStats** (measure) | read-only | 0 | decides 2-8 |
| 1 | none possible for timer/sequence-bound windows (the 118.5 s route) | - | 0 | ready-wait / loaders drained long before the clear, event scene seconds high |
| 2 | turn the existing CPU-waste fixes on for loads: **RWb/RWc** (pool-1 wake storm, 0.3-1.5 cores), **GF** (FenceWait Sleep(0) spin, main thread 50-70 % of the window), WG/W6 | same work, fewer futile wakes/spins | **3-10 s** (A/B 5-15 % shorter; GPU MHz up in C, like W6's +37 %) | CPU >= 6 cores, wakes > 20k/s |
| 3 | **LM** sweeper (MEM_RESET dead arena pages) | allocator touches only headers (notes\load-memory.md) | 3-6 s on loads that start with a swap burst, 0 otherwise | swap jump at 58A set |
| 4 | **in-flight cap 10 -> 16** while 58A is set (A: imm8 at 1FEEA3, B: the 10 at 1FF3B0 path) | same requests, same queue order; dependencies are explicit (ResourceDependenciesReady) | 2-10 s if the managers are the critical path | at cap > 70 % of busy time, loaders busy until stage ready |
| 5 | **completion event instead of Sleep(1) polls** (signal from ResourceStepJob_Run 1343D00; keep a 1 ms timeout) | same steps, only earlier noticed | 0.5-3 s | many no-progress Sleep(1)s (1FF18C) while busy |
| 6 | faster bit-exact chunk inflate (libdeflate-class decoder behind 420654/324D064/A09A40; offline check: all 190k entries byte-identical with `data\tools\n3rdb.py`) | DEFLATE decode is deterministic | 0.5-2 s (CPU power) | inflate CPU-s > 10 |
| 7 | PSO pre-creation of the next stage's list earlier (title/menu) | same PSOs, earlier | 0-5 s | PSO CPU-s > 5 in the window |
| 8 | raise WorkerCap only during loads | - | <= 1 s, may be negative (more wakes) | max inflate parallel = pool width and CPU < 80 % |
| 9 | larger/aligned reads, prefetch | - | < 1 s | read in-call ms high (not expected) |

Not same-result (rejected): converting the 120-frame ready-wait timeout into seconds (characters could appear
unready), the VSM blackout budget patches VBF/VAT/VBB in `ld_verify.py` (change the shadow refresh policy), frame caps on
the cover screen.

PC: no load is logged yet. 2-3 do nothing there (no shared power, 32 GB RAM); 4-7 apply. Expect the same 118.5 s floor
on the timer-bound route.

## 4. Measurement: LoadStats (ASI, new files)

`mod\Nioh3EngineFix\src\loadstats.{h,cpp}`, integration: `mod\Nioh3EngineFix\loadstats_INTEGRATION.md`. `[Profiler]
LoadStats = 1` logs per 58A window:
```
LoadStats #n: total | frames, mean ms | CPU cores | stage ready +s | loaders busy s (first/last), in-flight avg and % at cap,
              pending avg | ready-wait +s..+s (acc max, TIMEOUT) | event scene s
LoadStats #n io: MB read (archive, overlapped, in-call ms, other) | inflate CPU-s, chunks, MB in -> out, max parallel,
              threads | PSO gfx/compute runtime/preload, CPU-s, max ms | manager Sleep(1) counts
LoadStats #n split [set->stage ready] / [stage ready->clear]: the same counters per segment
```
`LoadStats = 2` adds a per-second tick. Polling (no hooks): stop flag, frame counter, stage state, ready-wait
accumulator, event scene, GetProcessTimes, manager queues. Hooks (25 call sites, bytes verified, one interlocked
write each, EB FE path for the 4 that straddle 16 B): manager Sleep(1)s (capture the manager from r14), the 3 inflate
wrappers (8 sites), CreateGfxPso/CreateComputePso (7 sites; 11970A skipped when SPR's SPg owns it), ReadFile at 7
sites. Checks: `py -I re\scripts_load\ls_sites.py` -> LOADSTATS SITES OK; `py -I tests\run_loadstats_harness.py`
(live patching of both paths under a concurrent caller) -> LOADSTATS HARNESS: ALL OK; compiles clean with build.bat
flags (/W4).

## 5. Test plan (one session per device, Ally first)

1. ASI with LoadStats = 1, MemStats = 1, wake stats on; MangoHud on the Ally, PresentMon on the PC.
2. Loads: title -> continue (the 118.5 s route) x2, shrine fast travel x2 (variable route), one more stage.
3. Read per load: total, stage ready, loaders last busy, in-flight / at cap, ready-wait acc max (timeout?), event
   scene s, CPU cores, MB read, inflate CPU-s, PSO CPU-s. Decide with the "condition" column of §3.
4. A/B the chosen fixes, one per load pair, same route: LoadStats total and stage-ready time, a screenshot 5 s after the
   clear (same look), VRAM after 60 s (same content).
5. Expected outcome: the 118.5 s route stays ~118 s (sequence-bound); variable loads shrink by fix 2 (+3 when they
   swap) first.

## 6. Open
- What sets the 118.5 s length (RTM / Movie / script wait): LoadStats' event-scene seconds and the ready-wait timing
  answer most of it; the script that sets/clears 58A is not identified.
- GameMain+4Dh is expected 0 (cap 10) in retail; LoadStats logs it.
- Manager queue fields are read from the decompile only (+200h/+210h are counts); LoadStats ignores implausible values.
