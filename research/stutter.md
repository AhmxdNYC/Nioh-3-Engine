# Nioh 3 (v2.02): hitches and frame-time spikes, and what stands between us and a consistent 60

Date: 2026-10-07. Scope: game-side only, same look (ASI / live memory patches / the game's own data). Nothing was run in
the game for this study: the inputs are the existing captures, the mod logs, the decrypted dump and Ghidra.

Inputs:
- Ally: 4 MangoHud CSVs (10 Hz, frame time + CPU/GPU load, clocks, power, GPU memory, RAM, swap),
  6 mod logs (v2-v7.1; v7.1 has 146 SP profiler reports and the wake statistics through a level load).
- PC: 105 non-FG PresentMon captures (10-30 s each, ~37 min in total; static benchmark spots: no traversal, no
  combat, no level load).
- Static: the notes named in each section, symbols/symbols.tsv, Ghidra queries, Iced sweeps of the dump.

New tools (private dev repo; all read the captures or the dump only, except the probe):
- `ally_spikes.py`: Ally spike census. Splits phases by the mod log's stop flag 58A and records the
  memory/swap deltas around every spike (writes a spike table, `ally_spikes.tsv`).
- `pc_spikes.py`: PresentMon spike census; classifies each spike as CPU, GPU or WAIT.
- `devcalls.fsx` (ID3D12Device create-method call sites), `fieldrefs.fsx` (every access to a struct
  offset), `vt.fsx` (vtable dump).
- `stutter_probe_live.fsx`: SPR, a measurement-only live probe (section 6).

## 1. TL;DR

- Target: a consistent 60 on the Ally. Today it renders 16-24 fps at the measured presets (p50 44 ms), so **throughput, not hitches, is the gap**.
- Spike census (frames > 2x the local 20 s median, 10 Hz MangoHud): **0 spikes in 5,000+ settle/steady frames** (v4/v5), 1 in v1. All hitches sit in the level-load window (stop flag 58A = the loading screen): 36 frames > 200 ms (v4), 21 (v5), max 265-305 ms.
- Load-window spikes are not a gameplay stutter: game logic is stopped there. They do dominate a whole-session p99 (v5: 132 ms with the load, 60 ms without).
- Spike classes: (A) swap-out burst at load start, +1150 MB in ~10 s; (B) streaming ramp with 80-130 ms frames and GPU-memory steps (13/30 and 15/55 spikes within 1.5 s of a >= 50 MB VRAM rise); (C) end-of-load GPU burst, 190-300 ms at 92 % GPU, flat memory; (D) wake storm after the load, 25-150k/s futile wakes, which costs 0.3-1.5 cores of a shared 25-33 W budget and so GPU clock, not a spike; (E) first-use PSO creation: not seen on the Ally, 106 CPU-side 20-50 ms spikes on the PC (~2.3/min uncapped, 0.8/min at a 60 cap).
- Top fixes: RWb/RWc (stop the pool-1 wake storm; byte patch built, offline PASSED; helps steady-ish throughput in the first ~4 min), SPR (measure PSO / upload-ring misses; in the ASI since 1.7.2), PJ (PSO journal + title-screen prewarm, built in 1.7.31, default off, test protocol 6.1), PL dropped (no frame-time gain, 3.1), UF ring enlargement (only if SPR shows dedicated allocations). Steady 60 needs GPU/CPU-power work (the gpu-*.md notes, worker-jobs-opt.md), not stutter fixes.

## 2. Spike census

### 2.1 Ally (MangoHud, 10 Hz)

Method:
- MangoHud writes one row per ~100 ms carrying the frame time current at that moment. A frame of F ms therefore shows up
  in ~F/100 rows (consecutive identical rows are de-duplicated as one frame). Every frame >= 100 ms is caught; shorter
  ones are caught with probability ~F/100 ms (~45 % at the Ally's 44 ms).
- A spike is a frame > 2 x the median of the surrounding 20 s.
- Phases come from the mod log: `engine stop flags ... now: 58A` opens the level-load window and `now: none` closes it.
  "settle" is the 4 minutes after the window, "steady" what follows. v1/v2 have no flag line, so their load window
  (~t 120-205 s) is taken from the GPU-memory ramp.

| capture | phase | frames | > 66.7 ms | > 100 ms | > 200 ms | mean ms | p99 ms |
|---|---|---|---|---|---|---|---|
| v4 (lightest 50 %, Futamata) | title/menu | 997 | 1 | 0 | 0 | 28.0 | 53.8 |
| | **load (t 106-207)** | 965 | **194** | **86** | **36** | 60.7 | **269.8** |
| | settle | 982 | 3 | 0 | 0 | 49.9 | 63.3 |
| v5 (same, W6 A/B) | title/menu | 1158 | 3 | 0 | 0 | 24.2 | 49.3 |
| | **load (t 122-207)** | 834 | **177** | **75** | **21** | 59.1 | **236.2** |
| | settle | 2377 | 9 | 0 | 0 | 46.5 | 61.9 |
| | steady | 1103 | 1 | 0 | 0 | 44.0 | 56.4 |
| v1 (mod L1, FG build) | load (approx.) | 887 | 33 | 7 | 0 | 36.3 | 91.9 |
| | settle | 1556 | 1 | 0 | 0 | 32.4 | 46.7 |
| v2 | load (approx.) | 885 | 47 | 24 | 0 | 34.6 | 116.4 |
| | settle | 1049 | 1 | 0 | 0 | 24.9 | 34.5 |

**After the load the Ally has almost no hitches.**
- In 5,000+ settle/steady frames there are 1-9 frames > 66.7 ms per capture and none > 100 ms.
- No frame exceeds 2 x its local median: the census finds 0 spikes in settle/steady for v4/v5, and 1 in v1.
- Steady frame times form one continuous lump from 37 to 55 ms (v5, t > 300 s). There is no vsync quantisation and no
  periodic spike (autosave, GC, timers).
- The steady p99/p50 ratio of ~1.3 is ordinary render-time variance on a GPU/power-bound APU, not stutter.
- The settle phase runs 2-6 ms slower on average than steady and has a slightly higher p99 (62-63 vs 56 ms). That is
  extra background work (wake storm, streaming tail), not spikes.

**Every Ally hitch is in the level-load window**, and it decides the session p99:

| v5, t > 100 s | p50 | p95 | p99 | p99.9 | max |
|---|---|---|---|---|---|
| everything | 44.4 | 62.9 | **132.0** | 242.5 | 264.5 |
| without the last ~27 s of the load (t 182-209) | 44.2 | 57.5 | 76.4 | 99.6 | 134.5 |
| without the whole load window | 44.2 | 53.8 | **60.1** | 70.3 | 74.7 |

v4 shows the same picture: p99 243.9 -> 97.4 -> 66.7 ms.

The load window has three phases (v5; v4 matches within a few seconds):

| phase | t (s) | frames | CPU | GPU | memory | what it is |
|---|---|---|---|---|---|---|
| A: swap-out burst | 120-132 | avg 40-57 ms, max 134 ms | 86-96 %, 17-19 W | 60 % at 800 MHz | **swap 1355 -> 2503 MB (+1150 MB in ~10 s)**; RAM used 10.9 -> 10.1 GB | the new level's allocations push resident pages to swap (zram on this Bazzite kernel = CPU compression). CPU-bound |
| B: streaming ramp | 136-190 | avg 40-65 ms, max 80-130 ms | 70-85 % | 80 % at **800-1100 MHz** | GPU memory 4.9 -> 6.3 GB at ~25 MB/s in ~119 MB steps | stage streaming, job wake storm, stage PSO list preload; CPU-side, GPU clock low |
| C: end-of-load GPU burst | 192-204 | **avg 190-210 ms, max 265 ms** (v4: 305 ms) | 53-57 %, 10 W | **90-92 % at 1.8-2.0 GHz** | GPU memory step to 6.5 GB, then flat | the GPU has a huge per-frame workload; the main thread waits on fences |

Correlation with memory (`ally_spikes.tsv`):
- 13 of 30 (v4) and 15 of 55 (v5) load-window spikes fall within +-1.5 s of a GPU-memory rise of >= 50 MB.
- 9 of 55 (v5) fall within +-1.5 s of a swap rise of >= 20 MB, all in phase A.
- The phase-C frames themselves have flat memory: they are GPU work, not paging.

### 2.2 Ally SP profile through a load (v7.1 log, MAX settings, load window 05:35:54-05:37:52)

- [App]Game (8 reports, 05:34:48-05:38:18): FenceWait Sleep(0) `[ntdll] <- 1180A4` 37 %, `<- 7D9E90` (Present-thread
  wait) 8 %, GfxFencePollWorker `<- 117D6C` 16 %, MeshletExtraDispatch `<- 5DA48` 2.4 %.
  - The main thread mostly waits for the GPU.
  - No file I/O, no PSO creation and no resource creation appear on [App]Game.
- Dispatcher: 67 % blocked in CmdListRingSubmitUpTo 4930C8, i.e. waiting for the ExecuteCommandLists ring
  (GPU-bound; steady state 69 %).
  - **13.3 %** is blocked in the shared SRW lock of SynSharedLockedUpdate 7F64DC (c+CF8h). Steady state is 4.6 %, so
    something holds that resource-state lock exclusively more often during loads. The holder was not identified: the
    lock is not taken through a plain `lea rcx,[reg+0CF8h]` anywhere (byte scan).
  - No `[winevulkan]/[unix] <- 9290D8 / 119670` rows, so no visible PSO compile on the dispatcher.
- Over the whole 70 min, the only PSO-path rows on the dispatcher are `[winevulkan] <- 58BE90 / 119C08` at a steady
  0.7-3 %. That is the per-draw SetPipelineState / root-table recording, which vkd3d forwards to Vulkan, not creation.
  One `[ntdll] <- 119670` row (0.7 %, 05:56:19) is the PSO-cache critical section.
- Wakes (`stats wakes/s`): wake-one 48C6B on **pool 1 runs at 25-150k/s from 05:34:58 to ~05:38:28**. That is the title
  screen -> load -> ~35 s after the load. Steady state is ~1k/s. See section 3.5.

### 2.3 PC (PresentMon)

`pc_spikes.py` over all PC captures (FG captures skipped):

- 105 captures, ~37 min, **110 spikes**: 106 CPU-side, 0 GPU-side, 4 WAIT.
  - Spike size: median 23 ms, p90 41 ms, ratio to the local median 2.5.
  - Spikes come in short clusters (median gap 0.4 s) with no periodicity.
- Without the two load-in outliers the rate is **~2.3 per minute uncapped (100-180 fps) and ~0.8 per minute at a 60 cap**. The
  outliers are r8-motionblur (a fresh launch: **407 and 374 ms** frames 7-8 s after load-in, see 3.8) and r9-mesh-on1
  (22 spikes of 25-48 ms in its first 12 s; its repeat mesh-on2 has none).
- At a 60 cap the PC already holds 60:
  - bench-cap60-stock: p50 16.64, p99 19.5, p99.9 21.3, max 21.5 ms;
  - bench-G/H/I: max 20-23 ms;
  - bench-cap60-patched has one 35 ms spike.
- The PC's steady-state p99 is not the problem. What is untested: traversal, combat with new effects, and loads. The
  20 s static captures cannot show first-use PSO creation or streaming bursts; the SPR probe (section 6) is the tool for
  that.

## 3. Classes, engine code paths and fixes

| # | class | where seen | frequency / size | cause (code) | fix | section |
|---|---|---|---|---|---|---|
| A | load start swap burst | Ally t 120-132 | +1150 MB swap, 40-134 ms | allocation pressure, zram | DiscardVirtualMemory idea | 3.3 |
| B | streaming ramp / upload ring | Ally load t 136-190 | 80-130 ms, tied to VRAM steps | UploadHeapSuballoc 62D688 fallback 9C2EE0 (untested) | SPR UF/UP, then ring size | 3.3 |
| C | end-of-load GPU burst | Ally t 192-204 | 190-305 ms, 92 % GPU | GPU work in the loading screen (unidentified) | GPUProf in phase C | 3.2 |
| D | pool-1 wake storm | Ally first ~4 min | 25-150k wakes/s | ResStateWaitSwap spin 1A31473 | RWb / RWc | 3.5 |
| E | first-use PSO create | PC 2.3/min uncapped, none on Ally | 20-50 ms (407 ms after launch) | PsoGetOrCreateGfx 119670 -> CreateGfxPso 9290D8 under lock | SPR, PL, PJ | 3.1 |
| F | steady variance | Ally everywhere | p99/p50 1.3, no outliers | GPU/power bound | throughput work | 3.4 |

### 3.1 Pipeline-state (PSO) creation on first use

**Path (high confidence, decompiled):**
- Every draw whose PSO state is dirty (`c+B24h & 20h`) calls **PsoGetOrCreateGfx 119670** on the SynergisticDispatcher
  thread, from SynFlushPsoAndBindings 118604 (call 11871A) and 118758. 119670 does:
  1. hashes the 0x80-byte key (PsoKeyHash 1199A4);
  2. `EnterCriticalSection([c+818h]+10h)`, the one PSO-cache lock;
  3. looks the key up in the cache at `c+708h` (11976C);
  4. **on a miss, grows the cache if it is full (1899A54) and calls CreateGfxPso 9290D8 synchronously while still holding
     the lock**, then inserts the PSO (9B05BC).
- CreateGfxPso builds the D3D12 desc from the key and calls `CreateGraphicsPipelineState` (device `g+4268h`, vtbl+50h).
  - The key holds shader-object pointers at +0 and +20h; the root signature is looked up under the CS at `g+4760h`.
  - With mesh/amplification shaders (`key+30h != 0`) it calls `ID3D12Device2::CreatePipelineState` (vtbl+178h, 0x268-byte
    stream) instead.
- Compute has the same pattern, creating with CreateComputePso 58C2B0 (vtbl+58h):
  - PsoGetOrCreateCompute 119A48 and SynFlushComputeStateEx 119C08 use a per-context cache `c+128h` under `[c+18h]`;
  - SynFlushComputeState 58BE90;
  - PsoGetOrCreateComputeShared 58C000 uses the shared cache under the same `[+818h]` lock.
- The dispatcher's stall is the frame's stall: the main thread busy-waits at frame end until the dispatcher has drained
  (418F40). A synchronous create of X ms adds ~X ms to that frame. On vkd3d-proton a create is DXIL -> SPIR-V plus a
  RADV/ACO pipeline compile, tens of ms when the disk caches miss. On Windows the AMD driver cache usually makes it
  ~1-5 ms.

**The game's own preload (PreloadPSO):**
- Boot state 3 is `Game::Mode::PreloadPSOMode` (state machine 1C35188). Its request PreloadPsoBootLoad 1101B90 opens
  resource 0x17A4, the boot PSO list.
- Per-package PSO lists go through the resource handler PsoListResourceLoad 3484594.
- Both run **PsoListLoad 3483E04**:
  - header 0x18 bytes;
  - graphics records of 0x80 bytes and compute records of 8 bytes, where every shader slot is
    `{u32 shader-group file id, u32 index}` (FFFFFFFFh = none, 56CB1360h = built-in);
  - loads the referenced shader groups, then preloads inline or as parallel PsoListPreloadJob 34835D4 jobs.
- Each record is resolved by PsoListResolveGfx 34837DC / PsoListResolveCompute 3483484, normalised (79E990 clears key
  byte +3Dh when no target blends) and created **outside** the lock: PsoPreloadGfxCreate 2EFE440 / PsoPreloadComputeCreate
  2EFE1CC. Then it is inserted, unless already cached.
- ComputePsoWarmOrCache 2EC8BF4 also creates and immediately releases 16 compute PSOs at init (driver-cache warm-up).
- The runtime key is built by PsoKeyBuildGfx 2EFF9C8 (into `c+740h`, the key 119670 receives) with the same
  normalisation call (79E990). Preload keys and runtime keys therefore share one format: a list record that resolves is
  a cache hit later.

**Is the preload complete?** It covers exactly the PSOs that appear in a shipped list. Statically there are three ways a
draw can still miss:
1. **A record whose shader group is not loaded when its list is preloaded.** The resolver returns 1, the record is
   skipped and the list is flagged incomplete (`[list+48h] = 0`). That PSO is then created on first draw.
2. **A runtime key that no list contains**: render-state or format combinations the lists did not record, for example a
   graphics setting that changes a render-target format, DLC content, or rare effect/material variants.
3. **A settings change in a session**: every PSO keyed on a changed format or state misses at once.

**Evidence of runtime misses:**
- No sign on the Ally in 70 min of steady play (2.2). The dispatcher rows never show `[winevulkan]/[unix] <- 9290D8 /
  119670`, and steady Ally play has no spike class at all. The dispatcher is sampled only ~6 times/s, so rare short
  creates could still hide.
- On the PC the steady CPU-side spikes (20-50 ms, ~2/min uncapped) are *consistent* with occasional misses but not proven.
- The stage PSO lists, preloaded inside the load window, are a likely contributor to phase B's CPU load on vkd3d.

**Fixes (same look):**
- **SPR first (section 6)**. Count and time every runtime create (sites G1, C1-C4) next to the preload creates (PG, PC).
  If G1/C* stay at 0 after the first minutes of a level, this class is closed.
- **Confirmed from the decompiles of 119670 and 418F40:** the create runs on the
  SynergisticDispatcher thread (callers SynFlushPsoAndBindings 118604 / 118758), inside
  `EnterCriticalSection([c+818h]+10h)` .. `LeaveCriticalSection`, as `CreateGfxPso(g_RenderGlobals 45BA588, key+8)`; the
  main thread blocks for it at frame end in WaitDispatcherDrainSpin 418F40 (`do { ... } while (g_DispatcherDrainBusy)`,
  from CmdStreamFinalKick 418C70). A failed insert (9B05BC) releases the new PSO and returns NULL (the draw keeps the old
  PSO state). CreateGfxPso's mesh path builds a stream of subobject types 0/2/8/9/10/11/14/15/16/18/24/25 (capstone at
  9292F9..9293EA), all of which PJ serializes.
- **PL - create outside the lock: dropped (not built).** Safe in principle (the preload path 2EFE440 already does
  create-outside + insert-or-release), but it cannot shorten a frame: the stall that reaches the frame is the dispatcher's
  own create, which PL leaves unchanged. The other holders of the CS are short (preload find/insert) or init-only
  (PsoGetOrCreateComputeShared 58C000, only from ComputePsoWarmOrCache at init), so the dispatcher never waits long on
  someone else's compile. PL would need an entry detour on a per-PSO-dirty-draw function and a re-implementation of five
  internal calls for no frame-time gain. Revisit only if SPR / SP show the dispatcher blocked in `[ntdll] <- 119670` for
  long.
- **PJ - PSO journal + title-screen prewarm: BUILT (ASI 1.7.31, `[Patches] PJ = 0` default, section 6.1).** Built at the
  D3D12 level rather than at the game-key level (no reverse map group -> shader object, no game function
  called): every PSO the engine creates is journaled once per device class with its full desc, shader containers and
  root-signature blob; the next launch creates the ones the engine has not created yet on one lowest-priority thread once
  the title screen is idle, then releases them. The game's own ComputePsoWarmOrCache does the same create-and-release for
  16 compute PSOs. Effect: the engine's later create of the same PSO hits the driver's warm caches (in-memory, plus a
  refilled disk cache after a driver / Proton update or cache eviction). It does not remove the create call itself (the
  engine still creates and caches its own object), so the measure is the create time (SPR avg/max, creates >= 1 ms) and
  the route's frame-time spikes, not the SPR count.

### 3.2 End of the level load: GPU-bound 190-300 ms frames (Ally phase C)

- The frames are GPU-bound (92 % GPU at 1.8-2.0 GHz, CPU 46-57 %, memory flat, `ally_spikes.tsv` rows t 192-204), 190-305 ms each, ~12 s long. They happen inside the stop-flag window, so the player is on the loading screen.
- Static identification is not done: the v7.1 SP profile shows [App]Game waiting on fences (FenceWait 37 %), no creation calls. Candidates are stage-entry GPU work (cubemap/IBL/shadow-atlas bakes, texture mip generation, first draws of the new stage with cold caches).
- Fix design (same look): none until measured. Test: ASI GPUProf (`[Profiler] GPUProf = 1`, gpuprof.cpp) across t 190-210 of a load; log the per-pass times of the top 5 passes. If one pass owns the time and it is a one-shot, move it to run at load-screen start (identical output) or split it over frames while the screen is up. Risk: low, but it only shortens the loading screen.
- If the loading screen is a real frame-presented screen the player sees, these frames only matter as load time, not as stutter.

### 3.3 Start of the level load: swap-out burst (Ally phase A) and upload-heap overflow

- Swap goes 1355 -> 2503 MB in ~10 s (v5; v4 similar; memory-leaks.md §5.1: 0.75-0.85 GB), RAM used 10.9 -> 10.1 GB. The CPU compresses pages (zram), so frames are 40-134 ms and CPU 86-96 %.
- Cause: the game holds a fixed 6 GiB arena plus ~6.8 GB GTT on a 15.3 GiB machine; the new stage's allocations evict dirty pages of the old stage.
- Fix idea (same look): before the new stage allocates, hand pages the old stage no longer needs back to the OS with `DiscardVirtualMemory` / `VirtualUnlock` on freed arena ranges, so the kernel drops them instead of swapping them. Where: stage teardown free path in the arena allocator (memory-leaks.md §5.1; RVAs still to locate). Risk: medium (a discarded range read before rewrite returns zeros under Proton); only ranges proven freed. Gain: removes phase A (~10 s of 40-134 ms frames, loading screen).
- Ally test: log `swap_used` per second plus the arena allocator's free-range count at teardown; compare the swap jump with and without the discard.

**Upload heaps (synchronous resource creation on the producing thread):**
- Every CPU->GPU upload goes through UploadHeapSuballoc 62D688. It is called by GpuBufferMap 4FD84, TextureStagingAlloc
  69A720, GpuIndexBufferMap 62CE7C, the meshlet upload path 4E250 and 10 more; mostly on [App]Game during draw prep and
  on the streaming jobs.
- Each call does `GetResourceAllocationInfo` (vtbl+C8h), a suballocation from a heap ring (UploadHeapRingAlloc 62D7F0;
  ring object `[g+4678h]` upload / `[g+4680h]` readback) and **one `CreatePlacedResource` per upload** (62D784).
- The ring is N fixed heaps. UploadHeapRingInit 2F035C8 creates `p+170h` heaps of size `p+168h` (upload) and `p+180h` x
  `p+178h` (readback) from the device-parameter block.
- When the request is larger than one heap or no heap has room, it falls back to **TextureStagingAllocDedicated 9C2EE0 =
  a CreateCommittedResource per upload** (62D7E7). That is a full allocation, plus zero-fill on vkd3d, on the thread that
  asked for the upload, and then a release when the upload retires (GpuAllocRelease 4F0FC).
- During a streaming burst this is the classic "staging ring full -> synchronous allocation" hitch. It also churns
  GTT/VRAM in large steps.
- Evidence so far is indirect: the 119 MB GPU-memory steps in phase B, and the phase-B spikes that line up with
  GPU-memory rises.
- Fix:
  - **SPR sites UF and UP** count both paths (section 6).
  - If UF fires in loads or traversal, raise the ring's heap count (`p+170h`) at device init. That is a data change
    before 2F035C8 runs, and same look: more staging memory, no dedicated allocations.
  - Where `p` comes from (the device-parameter block written during renderer init) is the open item. Mark it at
    2F035C8 entry (`r9 = p`) with one live read.

### 3.4 Steady gameplay on the Ally: variance, not hitches

- p99/p50 = 1.27-1.35 (v5 steady 56.4 / 44.0 ms). The distribution is continuous, with no outliers above 75 ms.
- The causes are throughput:
  - GPU-bound frames at 1.4-1.7 GHz under a shared ~33 W APU budget;
  - logic over-ticking (44.6 ticks/s for 16 rendered fps at MAX);
  - worker churn.
- The levers for that are the CPU-power options in the ASI: GF, WG, TG (opt-in), LK/CP/SPF
  (cpu-hotspots-ally.md, research/worker-jobs-opt.md). They are meant to hand CPU power back to the GPU budget; the one
  measured on the Ally so far, GF, cut main-thread busy time (about 85 % -> 32 %) but left fps unchanged because the
  GPU is the limit (docs/BENCHMARKS.md). No mean or p99 gain from them is measured.
- On the Ally, 60 fps is a throughput target, not a hitch target: the game renders 16-24 fps at the presets measured.

### 3.5 Job-system wake storm after a level load (pool 1)

**Evidence:**
- v7.1 `stats wakes/s`: wake-one 48C6B (inside JobWakeOneInPool 48B9C) ran at 25-150k/s on pool id 1 from the title
  screen through ~35 s after the load. The average over the 8 storm reports is ~55k/s. Steady state is ~1k/s.
- `pools id:n 1:101306 ... 1:158569` worker sleeps match the wakes almost one to one, and there are almost no wake
  checks: the woken workers find nothing.
- Cost: ~5-10 us of CPU per futile wake + sleep under Proton, so ~0.3-0.55 core averaged over ~4 min and 0.75-1.5 cores
  at the peak. On a ~25-33 W shared APU those watts come out of the GPU's budget, and this lands in phases B/C and the
  settle phase.

**Pool 1 is "ResourceLoad"** (static, high confidence):
- SchedCreatePool 32475C4 hands out the lowest free id. CreateResourceLoadPool 2703048 runs first, so pool table entry 5
  "ResourceLoad" gets **id 1** (stored at `g_GameMainPtr+6A0h`).
- 27049CC then creates 2 StageUpdate, 3 CharacterUpdate, 4 UpdStgEdt, 5 UpdSclEdt, 7 RequestLoad, 6 TrySync,
  8 MiscAsync, 9 CollisionUpdate, 10 StgGdtUpd, 11 BackupThread and 12 PhysUpdate (`+6A4h..+6CCh`), plus the fixed
  pools 0x42 TS_MESHLET_REQ and 0x43 TS_BINDLESS_UP.
- These ids match the thread names `KIDSTS[s%d:%s]` (KidsWorkerThreadName 32509B4).
- The scheduler's "background pool" `sched+3A8h` (sched = `g_GameMainPtr+530h`) is set to 1 by MainLoopFrame.

**The source: a wait loop, not a submitter** (static; medium-high confidence that it is the dominant source):
- ResSetWithStateWait 367674 (356 callers: resource set/commit) calls **ResStateWaitSwap 8BBB84**. That function CASes
  the object's slot and, while the resource's state bits (4 bit planes at `sched+268h`) are not clear, spins in the cold
  fragment 1A31388. Every iteration:
  1. SchedRequestFlush 1A862F4 sets bit 27 of `sched+2D8h`, for the main loop's SchedCtxKick 8FE3BC;
  2. **`JobWakeOneInPool(sched, [sched+3A8h] = 1)` at 1A31473**;
  3. `inc ebx`, and Sleep(0) only when `bl` wraps (every 256 spins).
- Nothing is pushed, so the woken ResourceLoad worker scans empty queues and sleeps again. The bits clear when a frame or
  load epoch completes, which takes tens of ms at 12-20 fps. One spinning thread at ~6.7 us per iteration (mostly the
  SetEvent syscall) is the observed 150k/s.
- The `[ntdll] <- 48B9C` rows on [App] threads appear only in the storm reports, which fits (the resource step job
  cannot run on [App] threads).
- The other pool-1 wakers are rate-limited:
  - ResourceStepJob_Run 1343D00 (1343D5E) wakes once per job run on TrySync workers; pool-6 activity stays below
    370-2467/s, which does not fit a storm.
  - ResourceLoadIdlePoll 1FEE28 (1FF184) wakes at <= 1 kHz.
  - Its dispatch wake 1FF06A goes to pool 6, not pool 1.
- Confirmation in one Ally run: split the WS2 counter by the caller's return address (`[rsp+30h]` inside the 48C6B cave):
  `1A31478` = this spin, `1343D63` = step job, `1FF189` = manager idle.

**What WG (ASI 1.6.0) already does:**
- It skips a wake while a pool-1 worker is still waking, so the spinner wakes at most once per wake-up latency.
- Every remaining wake is still futile (an estimated ~20-100k/s depending on Proton wake latency; read it from
  `stats/s WG`).
- The spinner's own core is untouched by WG and by both fixes below.

**Fixes (same result: identical exit condition, flush request, Sleep(0) cadence in spin count, work and ordering):**
- **RWb, byte-only, built: `reswait_live.fsx`** (offline ALL PASSED).
  - 22 bytes at 1A3146A move the wake behind the `bl` wrap test, so the spinner wakes once per 256 spins, together with
    its Sleep(0).
  - orig `8B 95 A8 03 00 00 48 8B CD E8 24 77 61 FE FF C3 84 DB 75 08 33 C9`
  - new `FF C3 84 DB 75 16 8B 95 A8 03 00 00 48 8B CD E8 1E 77 61 FE 33 C9`
  - Checks: both versions call 48B9C and branch to 1A31486; the Sleep(0) call at 1A31480 is untouched; rax/rcx/rdx/r8-r11
    and the flags are dead at 1A31486; no branch target inside the range; no overlap with any patch set.
  - Expected: 25-150k -> ~1-10k wakes/s. With SetEvent gone the spins get faster, so a residual 10-20k/s is possible.
- **RWc, cave, ASI design:** replace the call at 1A31473 (`E8 24 77 61 FE`, AOB
  `8B 95 A8 03 00 00 48 8B CD E8 ?? ?? ?? ?? FF C3 84 DB 75 08 33 C9` +9) with a gate.
  - The gate walks `sched+3A0h` to the pool with id edx and tests every worker's queue words (`[[w+448h]] .. +0Ch`) the
    way JobWorkerPoll does. It calls 48B9C only when pool-1 work is queued; otherwise it returns 0 (eax is dead after the
    call).
  - It uses only rax, r8-r11 and the flags.
  - Optional second site: 1343D5E.
  - Removes essentially all of these wakes. Offline checks of both sites: `ws_verify.fsx` (ALL
    PASSED).
- Not designed: a back-off in the spinner itself (pause/Sleep between spins). That would also return the spinner's core.

### 3.6 Autosave / save

- Saves are written by **[App]SaveLoadThreadMain** (thread proc 5B66C0, started by 5B7390 with a 32 KB stack). The
  user-save writers (SaveUserWritePlayerSection 105AE20 and friends) are called from the save state machine
  (106D7A0 / 106DD30 / 106E9D0 / 1072340).
- No capture shows a periodic or event-aligned spike: steady Ally play has 0 spikes in 5-9 min, and the PC spikes have
  no period.
- Not a measured hitch class. If one is suspected, the test is SPR watch + a manual save at a shrine.

### 3.7 GC-like cleanup bursts

- Engine CPU memory is fixed pools in one 6 GiB arena (memory-leaks.md §5.1): no heap compaction, no GC.
- GPU-side releases go through GpuAllocRelease 4F0FC / 4F180 as uploads retire, one by one. There is no batched
  "release everything" step outside stage teardown (inside the load window, 3.2/3.3).
- No steady-state spike class matches.

### 3.8 Load-in tail on the PC (seconds after a fresh launch)

- The round-8 sweep relaunched the game for every setting and benchmarked from the untouched load-in camera.
- One of those 22 runs, bench-r8-motionblur, has two CPU-side frames of **407 and 374 ms** at t = 7.6 and 8.0 s. The
  other 21 have none.
- The round-9 run mesh-on1 has 22 spikes of 25-48 ms spread over its first 12 s. Its repeat mesh-on2 (same toggle) has
  none.
- These are the PC's version of the Ally's load tail: sporadic, CPU-side, in the first seconds after a load. The likely
  candidates are first-use PSO creation for content outside the lists (3.1) and the upload-ring fallback (3.3). SPR
  distinguishes them directly. A settings change in the menu is expected to cost the same order of time (every PSO keyed
  on a changed format misses at once).

## 4. Fixes ranked by p99 impact

| rank | class | steady p99 / 1 % low impact | load impact | effort | note |
|---|---|---|---|---|---|
| 1 | F steady variance (GPU/power bound) | all of it: p99 56 ms, p50 44 ms | none | existing GPU/CPU-power work | the real 60 gap |
| 2 | D wake storm (RWb/RWc) | ~4 min after each load, 0.3-1.5 cores freed for the GPU budget | phases B, C | RWb built, offline PASSED | first Ally test: `stats/s WG` + fps in settle |
| 3 | C end-of-load GPU burst | none | p99 270 ms (loading screen) | measure first | session p99 132 -> 76 if cut |
| 4 | A swap burst | none | 40-134 ms | DiscardVirtualMemory idea | |
| 5 | B/E PSO and upload ring | not seen on Ally; PC 0.8-2.3/min | 80-130 ms | SPR (ASI 1.7.2); PJ built 1.7.31 (6.1) | PL dropped (3.1) |
In steady gameplay the only measurable class on the Ally is variance; all spikes live in loads.

## 5. Ally test plan (ASI SP profiler + MemStats + WakeStats, MangoHud on)

ini for the run:
- `[Profiler] SP = 1`, `IntervalUs = 1000`, `Workers = 1`, **`ReportSec = 5`** (30 s blurs the 10 s phase C into one
  report);
- `MemStats = 1`, `WakeStats = 1`;
- everything else as the current Ally zip.

MangoHud: `log_interval=0` (one row per frame instead of 10 Hz) with the memory columns on.

Runs (same save, same shrine pair, Futamata as in v4/v5, lightest 50 % and MAX):
1. **Baseline load x3.** Title -> continue -> level, then stand still for 4 min; teleport to the second shrine and back
   (2 more loads). Gives three load windows with 5 s SP resolution, MemStats every 30 s and WakeStats every 30 s.
   - Expect: phases A/B/C as in 2.1.
   - The SP main rows in phase C are GPU waits. The `pools id:n 1:` storm runs from the title screen to ~35 s after the
     load.
2. **RWb / WG A/B.** Repeat run 1 with WG = 2 (installed, switched off) and toggle it per load from the mod menu.
   - Once RWb is in the ASI, the same for RWb, or RWc if built.
   - Read `stats wakes/s` 48C6B and pool-1 sleeps, `stats/s WG`, CPU W (MangoHud), and the phase-B/C frame times.
   - Success: pool-1 wakes < 5k/s and the same final GPU memory.
3. **Load-end fix A/B** (3.2): one load with the fix off and one with it on, same shrine.
   - Compare phase-C frames (count > 200 ms, max), the time from the 58A clear to the first frame < 66.7 ms, the final
     GPU memory and a screenshot after the flag clears (same look).
4. **Swap test** (3.3): MemStats `working set` and MangoHud `swap_used` across phase A, with the stage-pool discard off
   and then on.
   - Success: a smaller swap jump (target < 300 MB instead of 0.75-1.15 GB) and no phase-A frame > 100 ms.
5. **Steady gameplay sanity:** 10 min of combat after the settle phase. Expect 0 frames > 2 x median, as in v5.
   - Any spike: note the SP report around it (5 s granularity), and the ASI's SPR line once SPR is integrated.

Reading the results:
- Phase boundaries: the log's `engine stop flags ... 58A` / `none` lines. MangoHud t = log time - log start (offset 0,
  checked on v5).
- Phase A cause: `stats mem:` working set before and after the swap jump, and system `swap_used`. If the working set
  drops while swap rises, the kernel is evicting **our** dirty pages, so the stage-pool discard applies.
- Phase C cause: SP main rows `[ntdll] <- 1180A4 / 117D6C` (GPU waits) + the GPU-side work named in 3.2. If 3.2's fix is
  installed, phase C frames should fall to phase-B levels with the same final GPU memory (6.8 GB) and the same image
  once the flag clears.
- Wake storm: `stats wakes/s` 48C6B and `pools id:n 1:...`, together with the `stats/s WG` skip counts.

## 6. SPR - the stutter probe (`stutter_probe_live.fsx`, PC, memory only, default not applied)

What it does:
- Wraps nine call sites with a timing cave: call the original target with the original arguments (including the 4 stack
  arguments of CreatePlacedResource), return its result.
- Counts calls, total and max time, and a <0.5 / <2 / <8 / <32 / >=32 ms histogram, and logs every call >= 2 ms with its
  TSC stamp in a 256-entry ring.
- Has a global switch (off = the cave tail-jumps to the original target).

| id | site | original | wraps |
|---|---|---|---|
| G1 | 11970A | `E8 C9 F9 80 00` | runtime graphics PSO create in 119670 (dispatcher, PSO-cache CS held) |
| C1 | 119AE1 | `E8 CA 27 47 00` | runtime compute PSO create, per-context cache 119A48 |
| C2 | 119D86 | `E8 25 25 47 00` | runtime compute PSO create in the compute state flush 119C08 |
| C3 | 58BF92 | `E8 19 03 00 00` | runtime compute PSO create in SynFlushComputeState 58BE90 |
| C4 | 58C07A | `E8 31 02 00 00` | runtime compute PSO create, shared cache 58C000 |
| PG | 2EFE47C | `E8 57 AC A2 FD` | preload graphics PSO create (PSO lists) |
| PC | 2EFE1F2 | `E8 B9 E0 68 FD` | preload compute PSO create |
| UF | 62D7E7 | `E8 F4 56 39 00` | upload heap ring full -> dedicated CreateCommittedResource |
| UP | 62D784 | `41 FF 93 E8 00 00 00` | CreatePlacedResource per upload suballocation |

`offline` result (no game access): **ALL PASSED**.
- Bytes match the dump and decode to the expected calls.
- No direct branch or call target lands inside a site (whole-executable sweep).
- No overlap with OTHER_SITES or any of the ASI's `patches.json` rvas.
- Native transparency test: 8,000 harness runs over both cave kinds, switch on and off, 0 differences.
  - The callee sees the same rcx/rdx/r8/r9 and stack arguments and is entered with rsp = 8 mod 16.
  - The return value and rbx/rbp/rsi/rdi/r12-r15/rsp are preserved, and the caller's stack arguments are untouched.
  - Counters, histogram and ring move only when switched on.

Usage on the PC:
- `apply` at the title screen.
- `watch 1` while playing a level load, a traversal route and a fight with new enemies/effects, next to PresentMon.
  Every `slow G1 37.20 ms at 21:14:03.512` line can be matched to a PresentMon spike by wall clock.
- `revert` or restart when done.
- Caveats: the caves have no unwind info. Nothing throws through these D3D12 calls, but a crash dump inside one would
  show a frame without a module. Measurement only.

For the ASI (Ally), the same nine sites are the same `E8 rel32` replacement the ASI already uses for P8/P9-style calls.
An `SPR = 1` key with a 30 s `stats/s SPR:` line (count/ms/max per id) would give the Ally numbers that sections 3.1 and
3.3 need.

## 6.1 PJ - PSO journal + title-screen prewarm (ASI 1.7.31, `mod/Nioh3EngineFix/src/psojournal.cpp`, default off)

What it does (details in `mod/Nioh3EngineFix/src/psojournal.h`):
- Hooks ID3D12Device::CreateGraphicsPipelineState / CreateComputePipelineState / CreateRootSignature and
  ID3D12Device2::CreatePipelineState on the vtable of the engine's device (psoswap's D3D12CreateDevice IAT hook reports
  the device; with PJ on and ShaderSwap off psoswap arms that hook only to report it, its own state stays off).
- Each engine create that succeeds is serialized after the call (fields copied one by one so padding is zero; shader
  stages as container digests, the root signature as the key of its serialized blob, the input layout with its semantic
  names; a cached-PSO blob is dropped) and appended once to `Nioh3EngineFix_pj\pj_<win|proton>_<apu|dgpu>_sm<NN>.bin`
  next to the ini. Shaders / root signatures are stored once. Writer = one background-priority thread, queue <= 32 MB.
- Next launch: once the engine has created PSOs, none for `PJ_QuietMs` (3000) and G+58Ah is clear (title idle), one
  THREAD_PRIORITY_LOWEST thread creates every journaled PSO the engine has not created in this session through the same
  device method (the call goes through the hooks below PJ, e.g. psoswap) and releases it. Pauses while a level load runs.
- Bounds: file <= `PJ_MaxMB` (64), <= 16,384 PSO records; a chunk with a bad checksum or a cut tail ends the journal there
  (the tail is truncated); a corrupt shader chunk only skips the PSOs that use it. Other exe build or device class ->
  new journal. `PJ = 2` journals without prewarming.
- Same look: the engine's calls get their own arguments and their own result; prewarm PSOs are never bound.
  Register-safe: COM vtable hooks (plain Win64 ABI, hook-register-audit.md).

Offline (`py -I mod/Nioh3EngineFix/tests/run_pj_harness.py`, real D3D12 WARP device, six launches): **ALL PJ CHECKS OK**.
- The device reaches PJ through psoswap's observe-only D3D12CreateDevice IAT hook, as in the mod (psoswap stays off
  and hooks nothing itself).
- Padding garbage does not split a desc (2 creates with different garbage -> 1 record); 3 records, 3 shaders, 1 root sig.
- Launch 2: the PSO the engine already created is skipped, the other two are prewarmed, and a spy hooked BELOW PJ
  describes every prewarm desc (shader bytes, root-signature blob, every field, input-layout names) **identically** to the
  engine's launch-1 descs. Launch 3: the file does not grow.
- Cut tail -> dropped and truncated; corrupt shader chunk -> its 2 PSOs reported missing, no crash; other exe stamp ->
  new journal.

### Test protocol (PC + Ally): SPR + frame-time hitches on a first-visit route, PJ off vs on across two launches

Setup (both devices): ASI 1.7.31 (or later), the device's release profile ini plus `[Patches] SPR = 1` (the `stats spr:` line:
runtime graphics PSO creates, avg/max us, creates >= 1 ms). PC: PresentMon CSV for the route; Ally: MangoHud with
`log_interval=0`. Same save, same settings, same route R in every
launch: title -> Continue -> an area with content the save has not visited in this session (new enemy types, one fight
with effects, one shrine teleport), ~3 min, inputs as identical as possible. FPS cap as in the profile.

Runs (4 launches per device, then repeat once):
1. **A1** `PJ = 0`: route R. **A2** `PJ = 0`, new launch: route R. (A2 = baseline; A1 warmed the driver's disk cache.)
2. Delete `Nioh3EngineFix_pj\`. **B1** `PJ = 1`: route R (journal written; log `PJ: journal ...: 0 PSOs`, then
   `stats/s PJ: journal 0 PSOs (+N new)`). **B2** `PJ = 1`, new launch: wait at the title until the log shows
   `PJ: prewarm done in X s: N created ..., 0 refused`, then route R.
3. Optional cold-cache pair (the after-driver / Proton-update case; check with the tester first, it costs a shader rebuild on the
   next normal launch): reset the driver shader cache before A2 and before B2 (PC: Adrenalin "Reset Shader Cache"; Ally:
   the game's `steamapps/shadercache/<appid>` and the vkd3d-proton cache in the prefix; Steam shader pre-caching off for
   the run, its Fossilize replay is a confounder).

Read:
- SPR over the route window (sum the 30 s lines): runtime gfx creates (expect the same count in A2 and B2 - PJ does not
  stop the engine's own creates), **creates >= 1 ms and max us (expect lower in B2)**.
- Frame-time hitches over the route: `pc_spikes.py <dir>` (PC; CPU-class spikes > 2x the local
  median) / `ally_spikes.py` (Ally). Count, max ms, and spikes inside a 30 s window whose
  `stats spr` >= 1 ms count rose.
- PJ cost: the `prewarm done` line (seconds at the title, ms total, max), `stats/s PJ` (journal size; `not journaled`
  should be ~0 - a high `rs` count means the device hook came late, check the PSOSWAP IAT line), the Ally's CPU W at the
  title during the prewarm (MangoHud), and that Continue is not delayed.
- Same look: one screenshot at the same spot in A2 and B2 (image diff).
- Load time of the first level (`LoadStats = 1` if wanted): the stage PSO lists may load faster in B2.

Keep / drop: `PJ = 1` on a device only if B2 beats A2 on creates >= 1 ms or route spikes in both repeats, with 0 refused
and no visual difference; otherwise leave `PJ = 0`. (The Ally showed no PSO spike class so far, 2.2 - the cold-cache pair
is where PJ can still pay off there.)

## 7. Open items

- SPR numbers from the Ally (G1/C1-C4/UF/UP counts). The ASI's `[Patches] SPR` (1.7.2) times G1 + UF; loadstats counts
  G1/C1-C4/PG/PC per load.
- PJ A/B on both devices (protocol 6.1).
- Identify the phase-C GPU work with GPUProf.
- Who holds the SynSharedLockedUpdate 7F64DC lock exclusively during loads (13.3 % vs 4.6 %).
- Arena free-range RVAs for the swap-avoidance idea.
- A longer Ally capture with a real traversal and combat (all captures are static spots or loads; no combat spikes known).
