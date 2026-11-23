# Performance features (Nioh3EngineFix)

For engineers joining the project. What every performance feature in `Nioh3EngineFix.asi` does, what ships on by
default, why, and where the code is. How the numbers behind these decisions are taken is in
[performance-measurement.md](performance-measurement.md).

## The rules every performance feature follows

1. **Same picture, same game.** A performance patch must not change what the player sees or how the game plays
   (same simulation, same frame cap). Changes that lower image quality are kept apart and are opt-in
   (`[ExtraPerf]`, `[HalfRate]`), never defaults.
2. **Game-side only.** We fix the game's own code (CPU spins, wasted uploads, slow file reads, GPU passes). We do not
   ship device tweaks (power plans, driver settings, upscaler ladders). The one exception is the Resizable BAR notice
   below, which only *tells* the user.
3. **Off unless measured.** A patch ships on only after an A/B on a real device beat the noise floor
   (performance-measurement.md). Neutral results stay off with their numbers kept in `release/profiles.json`.
4. **Every patch has a switch.** `0` = stock game code (nothing written), `1` = on, `2` = installed but switched off
   (a live row in the F10 mod menu, used for one-launch A/B), `-1` = auto by OS (resolved at startup with
   `wine_get_version`: a different value under Proton/Steam Deck/ROG Ally Linux than on Windows).
5. **Refuse, do not guess.** Each patch checks the exact bytes it expects in `Nioh3.exe` (game build v2.02) before it
   writes. Unknown build or different bytes -> the log says `SKIPPED <reason>` and the stock code runs.

## Where things live

| what | where |
|---|---|
| patch install, ini parsing, startup order | `mod/Nioh3EngineFix/src/dllmain.cpp` |
| one module per feature | `mod/Nioh3EngineFix/src/<feature>.cpp/.h` (e.g. `cbvupload.cpp` CBN/CBT, `readfix.cpp` EP/SR/RC, `prefetch.cpp` PF, `asyncslots.cpp` AS, `gpuopt.cpp` barriers/tile batching, `extraperf.cpp` ExtraPerf, `loadmem.cpp` LM, `upmirror.cpp`/`scenemirror.cpp` CbvMirror/SceneBufMirror, `timerres.cpp` L1/FG timers, `rebar.cpp` ReBAR check) |
| live menu rows | `src/menu.cpp`, `src/livectl.cpp` (toggles write the ini and log `MENU: <label> -> ON/OFF`) |
| overlay (fps, frame time, game speed) | `src/osd.cpp` |
| per-patch unit harnesses | `mod/Nioh3EngineFix/tests/*_harness.cpp` (offline, no game) |
| user-facing option reference | `mod/Nioh3EngineFix/README.md` (section per release: what, why, measured where) |
| per-key description + default | `mod/Nioh3EngineFix/Nioh3EngineFix.ini` comments (source of the table below) |
| what the release zip changes and why | `release/profiles.json` (status `kept` / `provisional` / `off` + measured reason) |
| replacement shaders | `gpu-shaders/` (`GHDM_fast.hlsl` shadows, `fog/` fogpar, build/apply scripts per shader) |
| deeper engine notes | `docs/subsystems/*.md`, `docs/WHY_NIOH_RUNS_BADLY.md`, `notes/*.md` (each ini comment names its note) |

## What ships on, in plain words

- **Real speed below the cap (`VRR`)**: stock Nioh 3 slows the whole game down when fps drops under the 30/60/120
  cap. VRR makes the game step by real elapsed time. Paused automatically online (`VRROnline = -1`).
- **CPU spins removed (`W3`, `R1`, `L1`, `FG`, `W6`, `GF`, `WK`, `WL`)**: the game busy-waits in many places (message
  pump, frame-latency wait, frame limiter, frame-generation presenter, Present, GPU fences, futile worker wakes,
  a leaked wait handle). Each is replaced with a real sleep/wait. Same fps, less CPU (and on handhelds the saved watts
  go to the GPU). Some are `-1` auto because Proton and Windows measured differently.
- **Cheaper main-thread writes (`W1`, `W2`)**: two loops writing GPU upload memory rewritten (no read-modify-write
  on write-combined memory).
- **Skip duplicate constant uploads (`CBN`, `CBT`)**: the render thread re-uploads per-draw constants that did not
  change. Skipping byte-identical uploads gave +15.6 % fps on the PC2 (RX 6650 XT on a slow chipset PCIe link).
- **Faster loading under Proton (`EP`, `SR`)**: event pool + synchronous redirect for archive reads. ROG Ally load
  98.9 -> 50.1 s (EP), 47.25 -> 38.45 s (SR).
- **Resizable BAR notice (`[General] ReBarNotice`)**: one-time message when ReBAR/SAM is off on a desktop GPU (measured
  -26 % GPU time per frame once enabled on the RX 6650 XT). The mod never changes the BIOS.
- **Optional same-picture shaders** (in the zip under `optional/shader_dx12/`, copied by hand): shadow `0x0104840c`
  (GHDM) and fog `0xa9910db8` (fogpar). Gains are small and device-dependent (release/USER_GUIDE.md).

Everything else is opt-in or a diagnostic (profilers under `[Profiler]`, never on in a clean measurement).

## Adding or changing a performance feature

1. Write the patch in its own `src/<name>.cpp`, guarded by exact byte checks, default `0` in the ini with a comment
   block: first line `KEY: what it does`, then risk, how to verify in the log, and the note it comes from.
2. Give it a live menu row (`2` = installed, off) when it can switch at runtime; that enables the in-launch A/B.
3. Unit harness in `tests/` (zig cc `-O0`: `-O2` defines NDEBUG and removes asserts).
4. Measure it (performance-measurement.md). Record the verdict in `release/profiles.json` with the device, numbers
   and test id in `why`.
5. Run `py -I tools/docs/gen_perf_features.py` to refresh the table below, and `--check` in regression.

## Every setting

<!-- BEGIN GENERATED: gen_perf_features.py -->

_Generated from `mod/Nioh3EngineFix/Nioh3EngineFix.ini` + `release/profiles.json` by `tools/docs/gen_perf_features.py`. Do not edit by hand; edit the ini comment / profile entry and rerun._

`shipped` = the value in the release zip (profile override applied). `-1` = auto by OS (Proton vs Windows), `2` = installed but switched off (live menu row), `0` = stock game code. `evidence` is the profile's status and measured reason where one exists.

### [Patches]

| key | shipped | evidence | what it does (ini comment) |
|---|---|---|---|
| `MW` | `0` |  | Experimental paired meshlet completion wait. 0 stock, 1 enabled next launch. |
| `RD` | `0` |  | Experimental paired renderer-drain wait. Startup only; no work skipped. |
| `FWP` | `0` |  | Experimental frame-wait park (#101247): RenderWaitForFrame job spin -> kick once + timer sleep. 0 stock, 1 park, |
| `W1` | `1` |  | zero-fill loop on GPU upload memory rewritten as rep stosd (main-thread CPU time, all PCs) |
| `W2` | `1` |  | per-vertex quad writer without read-modify-write on GPU upload memory (all PCs) |
| `VRR` | `1` |  | variable-dt frame timing - the game runs at real speed when the frame rate is below the in-game cap |
| `VRROnline` | `-1` |  | what VRR does in an online / co-op session. -1 = auto (default): suspended automatically while an |
| `W3` | `1` |  | window message pump sleeps until input/1 ms instead of spinning a CPU core |
| `R1` | `0` |  | game thread waits for the frame-latency signal with a 2 ms timeout instead of a busy poll. |
| `L1` | `1` |  | frame limiter sleeps on a high-resolution timer instead of Sleep(0)-spinning (needs Windows 10 1803+; |
| `WK` | `1` |  | job-system wake gate - skips waking a worker when no job queue holds work (fewer futile wake-ups: |
| `WorkerCap` | `0` | off: handheld/power-limited opt-in only: cap 10 on a 16-thread APU cuts the weighted pools 127 -> 78 threads (job-system.md WC); costs paralle... | HANDHELDS ONLY. Shrinks the engine's job thread pools by capping their base at N threads |
| `KidstsCap` | `1` |  | 1 = size the job pools for the CPUs in the process affinity mask at boot (N >= 2 = as if N CPUs). |
| `FileLoadPrio` | `1` |  | 1 = the two KTGLGraph.Cache.FileLoad loader threads run BELOW_NORMAL while the process affinity has |
| `FileLoadPoll` | `1` |  | 1 = the two KTGLGraph.Cache.FileLoad loader threads stop spinning while they wait: an idle pass of |
| `PollFix` | `1` |  | EXPERIMENT, opt-in (#108505). The same loader back-off as FileLoadPoll but at ANY CPU count (no |
| `Inflate` | `0` |  | EXPERIMENT, opt-in (#108787). Faster unpacker for the game's 16 KiB zlib chunks, same bytes out; falls back to |
| `FileLinger` | `0` |  | OFF by default (PC2 #118013 more hitches ON, Ally #118161 neutral; opt-in for GH #113; menu Settings > CPU "Keep data files open"; A/B #117226 no measured gain). Keeps up to Fil... |
| `FileLingerMax` | `32` |  | FileLinger: OFF by default (PC2 #118013 more hitches ON, Ally #118161 neutral; opt-in for GH #113; menu Settings > CPU "Keep data files open"; A/B #117226 no measured gain). Kee... |
| `FileLingerFree` | `24` |  | FileLinger: OFF by default (PC2 #118013 more hitches ON, Ally #118161 neutral; opt-in for GH #113; menu Settings > CPU "Keep data files open"; A/B #117226 no measured gain). Kee... |
| `AS0` | `0` | off: neutral on PC (results_pc_1716.md) | EXPERIMENT, opt-in (1.7.5). The engine creates an extra async-compute queue for its effects path (async slot 0, |
| `AS` | `0` |  | EXPERIMENT, opt-in (1.7.44, notes\gpu-async-wins.md). Successor of AS0 with a LIVE switch. The game has 13 async-compute |
| `W3WineSleep` | `1` |  | W3 under Wine/Proton: 1 = sleep 1 ms on an empty message queue (Wine returns from the Windows-style wait at once, |
| `L1OnWine` | `0` |  | Under Wine/Proton (Steam Deck) L1 is skipped unless this is 1 - Wine may accept the high-resolution timer |
| `FG` | `1` |  | FSR frame-generation presenter thread sleeps on a high-resolution timer instead of spinning on the clock |
| `FGOnWine` | `0` |  | Under Wine/Proton FG is skipped unless this is 1 (same timer caveat as L1OnWine) |
| `FGC` | `0` |  | frame-generation copy-job guard. The game's FidelityFX backend skips a missing texture everywhere except |
| `P12` | `0` |  | CPUs with 12 or fewer threads (Steam Deck, ROG Ally, 6-core desktops) get 3 fewer worker threads in two |
| `PTS` | `1` |  | crash fix (game start race, "Fatal error" 0xC0000005 at Nioh3+1A822CC 1-2 s into the boot, seen on the ROG Ally): |
| `W6` | `-1` | kept: auto: Proton 1 (shipped in every Ally zip since 1.1.0: main-thread spin gone, fps flat, watts to GPU), Windows 0 (PC 1.7.16 ~0, exact; pc... | the main thread waits for Present to return with a short spin then a real sleep (WaitOnAddress) instead of a |
| `B` | `0` | off: dropped on PC: 2026-10-08 ABBA -0.11 % fps, screenshots identical; the 1.7.16 +15.6 % was not reproducible; fps-neutral when GPU-bound (A... | draw-list group-length search gallops instead of a full binary search (main thread, ~+5 % on PC) |
| `PP` | `0` | off: page-pool sweep every 4th frame: PC +5.3 % (one window), texture-sharpness gate still open; staleness argument weak at 16-30 fps (Ally).... | streaming page-pool usage sweep only every PP_Period-th frame (main thread) |
| `FPC` | `1` |  | per-frame cache for material parameter lookups (main render thread) |
| `AABB` | `1` |  | character bounding boxes computed in SSE, bit-identical results (game-update thread). 1 = on, 2 = installed, switched off (live A/B) |
| `M1` | `0` |  | meshlet-request workers stop re-summing the page-pool capacity for every chunk (worker threads) |
| `M2` | `1` |  | meshlet workers read their page bitmaps with plain loads instead of locked ones (code bytes: on/off at launch) |
| `M2S` | `0` |  | completion-CAS seed reads only, retained locked CAS barrier; startup-only; full M2 takes precedence |
| `J1` | `1` |  | idle job workers poll their queues with plain loads instead of locked ones (code bytes: on/off at launch) |
| `X1` | `0` |  | the per-frame active-pad scan skips XInput slots the game already knows are empty (input thread) |
| `E1` | `1` |  | effects nobody can see are updated every E1_N-th pass with E1_N x the time step (game-update thread) |
| `S1` | `1` |  | PAUSE in the main thread's end-of-frame spin on the submit thread (code bytes: on/off at launch) |
| `IK` | `1` |  | the game reads the pad at the START of each logic frame instead of the end of the previous one (the sample is |
| `TS` | `0` |  | timestamp probes, measurement only. The menu row shows the median pad read -> frame presented time (ms); the log |
| `WakeStats` | `0` |  | counts the engine's job-worker wakes (SetEvent per site), worker sleeps per pool and where the wake checks |
| `ED` | `1` |  | effect-update (Disp) workers whose list is empty are not woken and not waited for (same output). |
| `GF` | `-1` | kept: auto: Proton 1 (~0.6 core of fence spin removed, GPU clock up, fps flat; measured on ROG Ally), Windows 0 (measured on RX 9070 PC 2026-10... | where the game waits for the GPU with `while (fence not reached) Sleep(0)` (main thread 7D9E1C, worker poll |
| `WL` | `1` |  | the game asks the swapchain for its frame-latency wait handle every frame and never closes it (Windows: |
| `WG` | `1` |  | wake gate on the job system's two wake-one sites: while a worker of the same pool has been woken and has not yet |
| `TG` | `0` |  | EXPERIMENT, opt-in. Logic tick governor: the game's logic tick cap follows the rendered frame rate (floor TG_Floor, |
| `DS` | `1` |  | the main loop's 1 ms "no new frame yet" retry spin sleeps on a high-resolution timer for most of the millisecond |
| `DQ` | `1` |  | the same retry spin re-checks for a new frame after DQ_QuantumUs (default 200 us) instead of a blind 1 ms, |
| `P1E7` | `1` |  | the main thread prefetches the next texture's streaming object in the per-texture loop of 1E7050 (only |
| `LK` | `1` |  | 18 locked "atomic loads" in worker code (object update, meshlet jobs, job system) become plain loads (same values, |
| `MA` | `1` |  | the same idiom on the MAIN thread: 25 locked "atomic loads" (xor eax,eax; lock cmpxchg [m],<zero>) in the |
| `BPO` | `1` |  | BoundsFromPartObbs 71C60 on the game-update thread (notes\bounds-partobbs.md). Each frame it rebuilds |
| `W1B` | `1` |  | main thread, glyph padding clear 61B1C (notes\w1b-rpa.md). Each glyph row clears pad = 1 dword in the |
| `RPA` | `1` |  | main thread, RenderParamsAssign 247950 (notes\w1b-rpa.md): 29.6 KB of dword moves -> the same 21 byte |
| `CP` | `1` |  | collision workers prefetch the colliders they will process next (same results, fewer memory stalls). |
| `SPF` | `1` |  | the stage-update worker prefetches placeables 4 entries ahead (same results). 0 / 1 / 2 as CP. |
| `MQ` | `-1` | provisional: auto: Proton 1 (bit-exact, CPU power only; not yet run on a handheld), Windows 0 (measured on RX 9070 PC 2026-10-08 abba2 run5: no gain) | skeleton bone matrices (MatrixFromScaleQuatTrans) in SSE, bit-identical results (~1 % of each scheduler thread on PC). |
| `PT1` | `1` |  | texture tile requests take the streaming pool lock once per texture instead of once per tile touch (same state). |
| `DL` | `0` |  | collision update skips a part-map search whose result is thrown away (second site of the CP group; installs the CP |
| `CpuStats` | `0` |  | 1 = count MQ calls and DL skips for the "stats cpu:" line (every 30 s). PT1 counters are always on. Costs a bit. |
| `RWb` | `1` |  | next launch): the resource-state spin (ResStateWaitSwap) wakes a ResourceLoad worker |
| `ESM1` | `1` |  | next launch, GPU): renders the directional exponential shadow map at 1024x1024 instead of 2048x2048 |
| `MPCS` | `0` |  | next launch, GPU): MPC tail skip in MeshletPreculling (notes\meshlet-frontend.md 3). With the |
| `TR` | `0` |  | TR (1.7.45, Windows; live menu row "Steady timer resolution"): the game's HID pad-reader threads wrap every Sleep of their |
| `RTP` | `0` |  | RTP (1.7.58; live menu row "Render threads first"): raises the Windows scheduling priority of the render critical path - |
| `PCM` | `1` |  | PCM (1.7.58; live menu row "Faster pipeline lookup"): the D3D12 replay thread (SynergisticDispatcherTask) looks every dirty |
| `CBS` | `0` |  | streaming copy (forward, full 64-byte lines, non-temporal stores) instead of memcpy; one sfence per table |
| `CBN` | `1` | kept: skip byte-identical per-draw constant uploads (same command list): pc2 CPU-bound on its slow chipset PCIe link +15.6 % (CBU1 #1609, ABBA... | a slot whose bytes equal its previous upload in the same command list is not uploaded again (the old block stays |
| `CBT` | `1` | kept: with CBN: reuse the stage's CBV table of the same list when nothing changed; pc2 on its slow chipset PCIe link: CBN+CBT +15.9 % (CBU1 #16... | with CBN, a stage whose slots all were no-ops binds its previous CBV table of the same list instead of a new copy. |
| `CBF` | `0` |  | CBF (1.7.60-cbu2, v2.02 only, TEST ONLY, default 0; live Tuning.CBF = 0/1, Tuning.CBF_QUEUE = 0/1, or Tuning.CBMODE bits 32 / 64): |
| `VSMA` | `0` |  | VSMA (1.7.49, v2.02 only, code sites at launch; live menu row "Fewer shadow tile redraws"): the virtual shadow map renders one |
| `LPVR` | `0` |  | LPVUpdateFrequency = LPVR_N (game 1). SKYR: SkylightAmbientUpdateFreq = SKYR_V (game 1). FOGT: |
| `SKYR` | `0` |  | Engine update-rate knobs (1.7.49, v2.02 only; one live menu row each; notes\same-look-package.md). Each writes one field of |
| `FOGT` | `0` |  | Engine update-rate knobs (1.7.49, v2.02 only; one live menu row each; notes\same-look-package.md). Each writes one field of |
| `CLDR` | `0` |  | Engine update-rate knobs (1.7.49, v2.02 only; one live menu row each; notes\same-look-package.md). Each writes one field of |
| `SPR` | `0` |  | times the runtime graphics-PSO creates and the upload-heap dedicated fallbacks. One |
| `PJ` | `0` |  | PSO journal + title-screen prewarm against first-use PSO hitches (notes\stutter.md 3.1). |
| `PJ_MaxMB` | `64` |  | journal file size cap in MB (4-512); when full nothing new is journaled (delete the folder to start over). |
| `PJ_QuietMs` | `3000` |  | the prewarm starts once the game has created no PSO for this long and no level load is running. |
| `PJH` | `0` |  | PJ journal v2 (1.7.45): only pipelines built AFTER the boot list are journaled (the game rebuilds its ~17,000-entry boot |
| `PJ_KeepMax` | `4096` |  | most prewarmed PSOs kept for the hand-over (0-16384); beyond it they are released after the prewarm. |
| `PL` | `0` |  | boot pipeline library against the start-up "Compiling shaders" wait |
| `PL_MaxMB` | `1024` |  | largest library written (16-4000); a bigger one is not saved (logged). |
| `GL1` | `1` |  | the mission thread's two task-list walks (proximity pass over ~4,400 tasks, task-state pass) ask the CPU to fetch the |
| `LC` | `0` |  | the two resource-load managers keep up to 16 requests in flight instead of 10 (the boot-flag value 1 stays 1). |
| `LW` | `1` |  | the resource-load managers' Sleep(1) polls wait on an event instead; a finished resource step wakes them at once. |
| `LoadOnlyCpuFixes` | `0` | off: pending Ally A/B (R2 later pair) | GF, W6 and RWb only while a level loads. Plain language: the three CPU-power fixes (GPU-fence waits, the |
| `EP` | `-1` | kept: auto = on under Proton; Ally load 98.9 -> 50.1 s (1.7.23 R1); no-op on Windows | event pool for the archive reads. Plain language: the game makes and destroys one kernel event per file read (~640k |
| `SR` | `-1` | kept: auto = on (load windows) under Proton; Ally R2 ABBA 47.25 -> 38.45 s, 0 failures (1.7.25); off on Windows | synchronous redirect. Plain language: under Proton an "overlapped" file read costs ~55 us of bookkeeping even when the |
| `RC` | `2` |  | read cache. Small reads (<= RCMaxReadKB) are copied from aligned RCKB blocks kept in RAM (RCMB MB); a miss reads the |
| `RCMB` | `64` |  | RC: read cache. Small reads (<= RCMaxReadKB) are copied from aligned RCKB blocks kept in RAM (RCMB MB); a miss reads the |
| `RCKB` | `64` |  | RC: read cache. Small reads (<= RCMaxReadKB) are copied from aligned RCKB blocks kept in RAM (RCMB MB); a miss reads the |
| `RCMaxReadKB` | `64` |  | RC: read cache. Small reads (<= RCMaxReadKB) are copied from aligned RCKB blocks kept in RAM (RCMB MB); a miss reads the |
| `AQ` | `0` |  | async archive reads. Reads at the three overlapped submit sites are queued to AQThreads worker threads and the game's |
| `AQThreads` | `4` |  | AQ: async archive reads. Reads at the three overlapped submit sites are queued to AQThreads worker threads and the game's |
| `TSY` | `0` |  | TrySync worker threads (stock 3; 3..8). Boot-time: written before the game sizes its thread pools; the log line |
| `SH` | `0` |  | open every archive handle synchronous (FileOpenWin32 314181 imm 200h -> 0). Boot-time, Wine/Proton only. The cruder |
| `PF` | `0` |  | Plain language: on the Ally the resource loader reads ~2.7 GB in 12.6k cold random reads (ReadFileEx 631FD5, ~0.8 ms |
| `PFThreads` | `4` |  | helper threads (1..16), how far ahead of the game they may read (MB, 64..4096), how many traces are kept (1..8), and how |
| `PFLeadMB` | `768` |  | helper threads (1..16), how far ahead of the game they may read (MB, 64..4096), how many traces are kept (1..8), and how |
| `PFTraces` | `4` |  | helper threads (1..16), how far ahead of the game they may read (MB, 64..4096), how many traces are kept (1..8), and how |
| `PFMatchPct` | `50` |  | helper threads (1..16), how far ahead of the game they may read (MB, 64..4096), how many traces are kept (1..8), and how |
| `GIMerge` | `0` |  | the RefProbe GI gathers (#121 diffuse, #414 reflection) run twice per frame over the full screen, once per stencil |
| `GIMergeQualify` | `60` |  | GIMerge: the RefProbe GI gathers (#121 diffuse, #414 reflection) run twice per frame over the full screen, once per stencil |

### [ExtraPerf]

| key | shipped | evidence | what it does (ini comment) |
|---|---|---|---|
| `Enabled` | `1` |  | 1 installs the hooks (v2.02 only; with every level 0 they pass the game's values through), 0 installs nothing. |
| `Preset` | `0` |  | 0 = use the keys below, 1 = Performance, 2 = Potato, 3 = Risky / max speed (#66661: every key at its strongest |
| `XCLD` | `0` |  | 0 game (<= 256), 1 = 128, 2 = 64, 3 = 32 |
| `XFOG` | `0` |  | 0 game (LIGHT 80x32 .. HIGH 160x64), 1 = 64x24, 2 = 48x16, 3 = off (the game's VOLUMETRIC LIGHTEST) |
| `XGI` | `0` |  | 0 game, 1 = 0.75, 2 = 0.5, 3 = 0.35, 4 = 0.25 (floor 0.1) |
| `XSHD` | `0` |  | 0 game (fade 250-500 m), 1 = 0.6, 2 = 0.35, 3 = 0.2 |
| `XSHF` | `0` |  | 0 game (4), 1 = 2, 2 = 1 |
| `XLOD` | `0` |  | XLOD  meshlet LOD scale x (bigger = coarser; the game's MESHLET_QUALITY LIGHT is x2): 0 game, 1 = 1.5, 2 = 2, 3 = 3 (cap 10) |
| `XTER` | `0` |  | 0 game, 1 = 0.7, 2 = 0.4, 3 = 0.2 |
| `XPRT` | `0` |  | 0 game (100 %), 1 = 75 %, 2 = 50 %, 3 = 25 %, 4 = off |
| `XAO` | `0` |  | XAO   1 = ambient occlusion off (the game's AMBIENT_OCCLUSION OFF values) |
| `XPOST` | `0` |  | XPOST 1 = motion blur + depth of field off, 2 = also bloom off (the game's own OFF values) |
| `XFOGS` | `0` |  | 0 game, 1 = 75 %, 2 = 50 % of the game's fog cells per axis (X and Z; never raised; ignored |
| `XSSR` | `0` |  | 0 game, 1 = half-size trace (the flag the game's SSR STANDARD sets), 2 = + ray steps x0.5 |
| `XSHARP` | `0` |  | XSHARP (#67772, live) FSR 3 sharpening (RCAS) strength: 0 = the game's FSR_SHARPNESS, 1 = 0.0, 2 = 0.25, 3 = 0.5, 4 = 0.75, |
| `XHALF_Force` | `0` |  | XHALF (#66661, OFF unless XHALF_Force = 1 since #80829: it made the lighting flicker; restart needed, changes the look): compute the GI / reflection probe gathers (1), and also... |
| `XHALF` | `0` |  | XHALF (#66661, OFF unless XHALF_Force = 1 since #80829: it made the lighting flicker; restart needed, changes the look): compute the GI / reflection probe gathers (1), and also... |
| `XHALF_Period` | `4` |  | XHALF (#66661, OFF unless XHALF_Force = 1 since #80829: it made the lighting flicker; restart needed, changes the look): compute the GI / reflection probe gathers (1), and also... |
| `XHALF_Run` | `-1` |  | XHALF (#66661, OFF unless XHALF_Force = 1 since #80829: it made the lighting flicker; restart needed, changes the look): compute the GI / reflection probe gathers (1), and also... |
| `XHALF_Mask` | `-1` |  | XHALF (#66661, OFF unless XHALF_Force = 1 since #80829: it made the lighting flicker; restart needed, changes the look): compute the GI / reflection probe gathers (1), and also... |
| `XRES` | `0` |  | XRES (#67206, restart needed, changes the look): internal render size x 0.9 (1), 0.8 (2), 0.7 (3), 0.6 (4) on top of the game's |
| `RenderDistance` | `1.00` |  | how far entities and grass are drawn. |

### [VRR]

| key | shipped | evidence | what it does (ini comment) |
|---|---|---|---|
| `Tier30` | `1` |  | Which in-game FPS caps (Max FPS setting 30 / 60 / 120) get variable-dt timing. |
| `Tier60` | `1` |  | Which in-game FPS caps (Max FPS setting 30 / 60 / 120) get variable-dt timing. |
| `Tier120` | `1` |  | Which in-game FPS caps (Max FPS setting 30 / 60 / 120) get variable-dt timing. |
| `Min30` | `20` |  | Measured-fps clamp per cap. Below Min the game slows down again (like stock); Max can't exceed the cap. |
| `Max30` | `30` |  | Measured-fps clamp per cap. Below Min the game slows down again (like stock); Max can't exceed the cap. |
| `Min60` | `24` |  | Measured-fps clamp per cap. Below Min the game slows down again (like stock); Max can't exceed the cap. |
| `Max60` | `60` |  | Measured-fps clamp per cap. Below Min the game slows down again (like stock); Max can't exceed the cap. |
| `Min120` | `48` |  | Measured-fps clamp per cap. Below Min the game slows down again (like stock); Max can't exceed the cap. |
| `Max120` | `120` |  | Measured-fps clamp per cap. Below Min the game slows down again (like stock); Max can't exceed the cap. |
| `MaxFps` | `120` |  | With the in-game cap at 120: 120 = stock cap, 144 / 165 / 240 = high-refresh cap, 0 = unlocked (1000). |
| `MenuQuantize` | `0` |  | 1 = quantize menu hold/repeat timers (only if menu key repeat misbehaves). Leave 0 when using Nioh3Fix. |
| `Tier120Floor` | `0` |  | with the in-game cap at 120, once the game runs below this fps for |
| `Tier120FloorHoldMs` | `2000` |  | Tier120Floor (GitHub #98 stopgap, 0 = off): with the in-game cap at 120, once the game runs below this fps for |
| `Tier120FloorProbeS` | `30` |  | Tier120Floor (GitHub #98 stopgap, 0 = off): with the in-game cap at 120, once the game runs below this fps for |

### [Latency]

| key | shipped | evidence | what it does (ini comment) |
|---|---|---|---|
| `MaxFrameLatency` | `-1` | kept: auto: Proton 1 (shipped in Ally zips since 1.1.0; explicit under vkd3d), Windows 0 (game value 1 is already the lowest latency; measured... | How many frames the game may queue ahead of the GPU (IDXGISwapChain2::SetMaximumFrameLatency on the game's waitable |
| `VrrPresent` | `0` |  | the game's own Present "SyncInterval" sometimes gets stuck at 1 (wait for the |

### [GpuOpt]

| key | shipped | evidence | what it does (ini comment) |
|---|---|---|---|
| `Barriers` | `0` | off: measured -7 % on PC (results_pc_1716.md); RADV already merges barriers on Proton (R2a/R1 trace work only) | D3D12 barrier batching on the game's command lists (notes\gpu-barriers.md). |
| `FrameOverlap` | `0` |  | overlap frame N's D3D12 replay tail + Present (moved to the replay thread) with frame |
| `TileBatch` | `0` |  | merges consecutive UpdateTileMappings of the game on the same queue into one call |
| `MergeAcrossBinds` | `1` |  | 1: state setters do not split a batch; 0: only literally adjacent ResourceBarrier calls merge |
| `DropDuplicateUav` | `1` |  | drop a UAV barrier already covered by one in the same batch (result-neutral) |
| `DropUndoPairs` | `0` |  | drop A:S1->S2,A:S2->S1 pairs for RT/depth-write/read-only S1 (result-neutral; counted anyway) |
| `R2a` | `0` |  | a COMMON -> X barrier on a BUFFER without ALLOW_UNORDERED_ACCESS gets its real previous state (last tracked state / |
| `R2b` | `0` |  | EXPERIMENT - the same for buffers WITH ALLOW_UNORDERED_ACCESS (execute-indirect arguments, UAV binds). WARNING: this |
| `R1` | `0` |  | EXPERIMENT - drops read -> read transitions whose new state is a superset of the old one (only VCB/IB/NPSR/PSR bits). |
| `MaxBatch` | `64` |  | R1: EXPERIMENT - drops read -> read transitions whose new state is a superset of the old one (only VCB/IB/NPSR/PSR bits). |
| `DeferThreads` | `SynergisticDispatcher` |  | R1: EXPERIMENT - drops read -> read transitions whose new state is a superset of the old one (only VCB/IB/NPSR/PSR bits). |
| `AnyCaller` | `0` |  | R1: EXPERIMENT - drops read -> read transitions whose new state is a superset of the old one (only VCB/IB/NPSR/PSR bits). |
| `TraceRecords` | `0` |  | e.g. 300000: write Nioh3EngineFix.gpuopt-trace.csv once (resource-level barrier log) |
| `TraceDelaySec` | `30` |  | e.g. 300000: write Nioh3EngineFix.gpuopt-trace.csv once (resource-level barrier log) |
| `InstallDelaySec` | `20` |  | seconds after d3d12.dll + dxgi.dll are loaded before the GpuOpt hooks install themselves (0..600; default 20) |
| `AllowProxyLists` | `0` |  | 0 (default) = when another mod wraps the game's D3D12 command lists (ReShade's dxgi.dll or a similar |
| `CbvMirror` | `0` |  | GPUs WITHOUT Resizable BAR / SAM read every per-draw constant buffer from system memory over PCIe |
| `CbvMirrorMB` | `128` |  | CbvMirror (1.7.52, CM): GPUs WITHOUT Resizable BAR / SAM read every per-draw constant buffer from system memory over PCIe |
| `CbvMirrorAllSites` | `0` |  | CbvMirror (1.7.52, CM): GPUs WITHOUT Resizable BAR / SAM read every per-draw constant buffer from system memory over PCIe |
| `CbvMirrorSites` | `1` |  | which CBVs CM mirrors (ignored with CbvMirrorAllSites = 1). 1 = per-draw recorder 118F89 (default, |
| `CbvMirrorQueue` | `0` |  | 0 = copies in a prologue list on the game's own queue, same submission, no cross-queue waits |
| `CbvMirrorComputeCopy` | `1` |  | CbvMirrorQueue (1.7.54): 0 = copies in a prologue list on the game's own queue, same submission, no cross-queue waits |
| `SceneBufMirror` | `0` |  | SceneBufMirror (1.7.57, SBM; EXPERIMENT, notes\rebar-fix-v3.md): for PCs WITHOUT Resizable BAR / SAM that cannot turn it on. |
| `SceneBufMirrorQueue` | `1` |  | 1 = own COPY queue (DMA engine, default), 0 = the game's DIRECT queue. grep 'SBM' in the log. |

### [Memory]

| key | shipped | evidence | what it does (ini comment) |
|---|---|---|---|
| `LM` | `0` |  | load-memory sweeper. A thread takes each engine heap's own lock once a second and |
| `LM_Force` | `0` |  | LM_Force = 1 skips the self-test (use if the log says the working set did not fall but swap still drops) |
| `LM_PeriodMs` | `1000` |  | LM_Force = 1 skips the self-test (use if the log says the working set did not fall but swap still drops) |
| `LM_MinKB` | `256` |  | LM_Force = 1 skips the self-test (use if the log says the working set did not fall but swap still drops) |
| `LM_BudgetUs` | `1000` |  | max microseconds one heap lock is held per bin walk, and max MB one MEM_RESET call covers |
| `LM_SliceMB` | `128` |  | max microseconds one heap lock is held per bin walk, and max MB one MEM_RESET call covers |

### [Tuning]

| key | shipped | evidence | what it does (ini comment) |
|---|---|---|---|
| `FileLoadPoll_MaxCpus` | `6` |  | the poll wait runs only while the process affinity has at most |
| `AS_Slots` | `0` |  | AS_Slots (1.7.44, used only with [Patches] AS = 1 / 2): async slots to create at startup, space separated. Live switch for |
| `VSMA_K` | `2` |  | a waiting shadow tile past the front is drawn on 1 visit of its clipmap out of K (phase = tile |
| `VSMA_Front` | `2` |  | VSMA_K (1.7.49, [Patches] VSMA): a waiting shadow tile past the front is drawn on 1 visit of its clipmap out of K (phase = tile |
| `VSMA_First` | `0` |  | VSMA_K (1.7.49, [Patches] VSMA): a waiting shadow tile past the front is drawn on 1 visit of its clipmap out of K (phase = tile |
| `LPVR_N` | `2` |  | values written by [Patches] LPVR / SKYR / FOGT / CLDR (1.7.49; notes\same-look-package.md) |
| `SKYR_V` | `4.0` |  | values written by [Patches] LPVR / SKYR / FOGT / CLDR (1.7.49; notes\same-look-package.md) |
| `FOGT_V` | `0.5` |  | values written by [Patches] LPVR / SKYR / FOGT / CLDR (1.7.49; notes\same-look-package.md) |
| `CLDR_V` | `0.5` |  | values written by [Patches] LPVR / SKYR / FOGT / CLDR (1.7.49; notes\same-look-package.md) |
| `W3_TimeoutMs` | `-1` | provisional: auto: Proton 3 (WinMain pump polls ~890/s at 1 ms; cpu-hotspots-ally.md row 4; not A/B tested), Windows 1 (stock) | W3 wait per empty message queue (ms) |
| `R1_TimeoutMs` | `2` |  | R1 wait per round (ms); 0 = stock busy poll |
| `L1_MarginUs` | `1000` |  | L1: keep the last N microseconds before the frame deadline as the original precise spin |
| `L1_MinSleepUs` | `500` |  | L1: don't sleep for less than this (microseconds) |
| `PP_Period` | `4` |  | PP: page-pool sweep period in frames (2, 4, 8, 16) |
| `KL_Div` | `0` |  | KL (1.7.58-bk1, live: LiveCtl 'Tuning.KL_Div = n'): hand a command-stream segment to the D3D12 replay thread every |
| `FO_Pct` | `25` |  | FO_Pct (1.7.59-fo1, [GpuOpt] FrameOverlap, live): the main thread may run ahead of the previous frame's replay tail up to the |
| `BPO_Mask` | `7` |  | CBS_Fence (1.7.60-cbu1, live Tuning.CBS_FENCE): with [Patches] CBS, 1 = one sfence per CBV table, 0 = none |
| `BPO_TableBits` | `14` |  | BPO: quaternion memo size, 2^N entries of 32 bytes (10-18; 14 = 16384 entries = 512 KB) |
| `E1_N` | `2` |  | E1: invisible effects update every N-th pass (1, 2, 4, 8) |
| `W6_SpinUs` | `50` |  | W6: microseconds to spin (pause) before sleeping on the present flag; 0 = sleep at once |
| `W6_TimeoutMs` | `1` |  | W6: WaitOnAddress timeout per round (ms) - bounds the wake-up delay if a wake is ever missed |
| `IK_PadWaitCapMs` | `0` |  | IK: longest wait (ms) for the fresh pad read at the frame start. 0 (default) = don't wait there: the game update |
| `GF_TimeoutMs` | `100` |  | GF: longest single sleep on the fence event (ms); the game re-checks the fence after each one |
| `WG_MaxPending` | `1` |  | WG: skip a wake-one while at least this many workers of the pool are woken and not yet running (1-8) |
| `WG_WindowUs` | `2000` |  | WG: a pending wake older than this (microseconds) no longer gates (safety net; it can only make WG wake more) |
| `TG_Floor` | `30` |  | TG: lowest logic tick rate the governor uses (24-60; never below the VRR clamp minimum + 1) |
| `TG_RatioPct` | `125` |  | TG: tick cap = rendered fps x this % + 2 (100-200), so the render rate can still grow until the GPU is the limit |
| `RTP_Level` | `1` |  | priority RTP gives the render threads: 1 = ABOVE_NORMAL, 2 = HIGHEST. Read at launch. |
| `DS_MarginUs` | `150` |  | DS: wake this many microseconds before the 1 ms retry spin ends; the rest is spun as before (50-900) |
| `DS_DqMarginUs` | `80` |  | DS with DQ on: wake this many microseconds before DQ's short (DQ_QuantumUs) re-check ends (20-150). Late wakes are counted in the DS stats line. |
| `DQ_QuantumUs` | `200` |  | DQ: the retry spin's quantum in microseconds while DQ is on (50-1000; stock is 1000) |

### [Profiler]

| key | shipped | evidence | what it does (ini comment) |
|---|---|---|---|
| `MemStats` | `0` |  | one "stats mem:" line every 30 s - process private/working set/commit, handles, threads, VRAM local and |
| `NodeTimer` | `0` |  | 0 = off (default). 1 = arm with F11 only. In a scene, F11 stores the engine's own stats |
| `VSForceSysUpload` | `0` |  | 1 = UPLOAD heaps created as CUSTOM write-combine L0, 2 = write-back L0 |
| `SceneBufProbe` | `0` |  | one hook on the meshlet scene-buffer update (MeshletFrameBegin 5D417); every 30 s |
| `LoadStats` | `0` |  | per-level-load measurement - MEASUREMENT ONLY (notes\loading.md). For every loading window (engine stop flag |
| `LoadGates` | `0` |  | LoadStats = 3 (1.7.17): like 2, plus a 1 s tick line OUTSIDE the 58A window too (whenever frames advance: 58A, frame delta, FT/B sequencer state, |
| `ReadProbe` | `0` |  | probe RP - MEASUREMENT ONLY (notes\loading-ally-fixed.md 8.2). Hooks all 13 |
| `TileStats` | `0` |  | D3D12 tile-mapping census - MEASUREMENT ONLY. One "stats tiles:" line every 30 s: UpdateTileMappings |
| `GPUProf` | `0` |  | GPUProf (1.7.0; 0 = off, 1 = markers, 2 = markers + per-PSO attribution): GPU pass profiler - MEASUREMENT ONLY. Turns on the engine's own debug markers (pass names) and times |
| `GPUProf_Markers` | `0` |  | 1 = only switch the engine's markers on (no queries), e.g. for a PIX / RenderDoc capture on the PC. |
| `GPUProf_MaxMarkers` | `2048` |  | most markers timed per frame (256-8192); beyond that a frame's extra markers are not sampled |
| `GPUProf_Top` | `30` |  | passes listed in the 30 s log line (5-60) |
| `GPUProf2_MaxStamps` | `8192` |  | GP2: most PSO timestamps taken per frame (1024-65536); beyond that the rest of the frame is not sampled (counted as "dropped: cap") |
| `GPUProf2_Top` | `40` |  | GP2: rows in the "GP2:" 30 s table (5-100) |
| `GPUProf2_AB` | `1` |  | GP2: 1 = measure the mode's own overhead: the 2nd and 4th 30 s windows run with the PSO stamps OFF, and the log compares |
| `GPUProf2_SampleEvery` | `8` |  | GP2 (1.7.8) SAMPLING: stamp only 1 frame in N (1-64, default 8). The other frames take NO timestamps and the hook slots for |
| `GPUProf2_MinRun` | `4` |  | GP2 (1.7.8) COARSE STAMPS: an interval is cut only once it holds at least this many draws+dispatches (1-64, default 4; a copy / clear |
| `GQ` | `0` |  | graphics-queue gap census - MEASUREMENT ONLY, needs GPUProf = 2 (Windows and Proton / vkd3d timestamps alike). |
| `GQ_Top` | `5` |  | GQ (1.7.40): graphics-queue gap census - MEASUREMENT ONLY, needs GPUProf = 2 (Windows and Proton / vkd3d timestamps alike). |
| `MeshletProbe` | `0` |  | meshlet front-end probe - MEASUREMENT ONLY (notes\meshlet-frontend.md). CPU half (v2.02 code entries of the |
| `MeshletProbe_Stats` | `1` |  | MeshletProbe (1.7.47): meshlet front-end probe - MEASUREMENT ONLY (notes\meshlet-frontend.md). CPU half (v2.02 code entries of the |
| `MeshletProbe_Top` | `40` |  | MeshletProbe (1.7.47): meshlet front-end probe - MEASUREMENT ONLY (notes\meshlet-frontend.md). CPU half (v2.02 code entries of the |
| `MeshletProbe_Dump` | `0` |  | MeshletProbe (1.7.47): meshlet front-end probe - MEASUREMENT ONLY (notes\meshlet-frontend.md). CPU half (v2.02 code entries of the |
| `MeshletProbe_Barriers` | `1` |  | 1 = timestamps before and after every barrier call (barrier column = the flush itself; the wait for the |
| `MpcCheck` | `0` |  | MPCS self-check - DIAGNOSTIC, needs GPUProf = 2 and [GpuOpt] Barriers = 0; run it with MPCS = 0. In the stats |
| `MpcCheckMaxMB` | `64` |  | MpcCheck (1.7.47): MPCS self-check - DIAGNOSTIC, needs GPUProf = 2 and [GpuOpt] Barriers = 0; run it with MPCS = 0. In the stats |
| `JT` | `0` |  | job-timing / logic-join probe - MEASUREMENT ONLY (v2.02; 5 code sites written only when 1, nothing otherwise). |
| `JT_Thread` | `KIDSTaskScheduler0` |  | JT (1.7.40): job-timing / logic-join probe - MEASUREMENT ONLY (v2.02; 5 code sites written only when 1, nothing otherwise). |
| `JT_Top` | `6` |  | JT (1.7.40): job-timing / logic-join probe - MEASUREMENT ONLY (v2.02; 5 code sites written only when 1, nothing otherwise). |
| `SP` | `0` |  | in-process sampling profiler - MEASUREMENT ONLY, never changes the game. Samples the game's main thread ([App]Game) |
| `IntervalUs` | `1000` |  | JT_ReportSec = 30   (not set here: the default is the [Profiler] ReportSec value; 5-600 s window length for the "JT:" log lines) |
| `Workers` | `1` |  | JT_ReportSec = 30   (not set here: the default is the [Profiler] ReportSec value; 5-600 s window length for the "JT:" log lines) |
| `ReportSec` | `30` |  | JT_ReportSec = 30   (not set here: the default is the [Profiler] ReportSec value; 5-600 s window length for the "JT:" log lines) |
| `TopN` | `25` |  | JT_ReportSec = 30   (not set here: the default is the [Profiler] ReportSec value; 5-600 s window length for the "JT:" log lines) |
| `StackBytes` | `2048` |  | JT_ReportSec = 30   (not set here: the default is the [Profiler] ReportSec value; 5-600 s window length for the "JT:" log lines) |
| `StartDelaySec` | `20` |  | JT_ReportSec = 30   (not set here: the default is the [Profiler] ReportSec value; 5-600 s window length for the "JT:" log lines) |
| `Focus` | `0` |  | 1 = KIDSTaskScheduler0 (the logic thread) gets its own "SP logic" group and is sampled every tick with |
| `ThreadState` | `0` |  | kernel snapshot of all game threads every ThreadStateUs (NtQuerySystemInformation; |
| `ThreadStateUs` | `100000` |  | ThreadState (1.7.58, TSTATE, measurement only): kernel snapshot of all game threads every ThreadStateUs (NtQuerySystemInformation; |
| `ThreadStateTop` | `14` |  | ThreadState (1.7.58, TSTATE, measurement only): kernel snapshot of all game threads every ThreadStateUs (NtQuerySystemInformation; |
| `Backend` | `0` |  | ThreadStateSec = 30   (default: ReportSec)   ThreadStateDelaySec = 20   (default: StartDelaySec) |
| `BackendSampleUs` | `1000` |  | ThreadStateSec = 30   (default: ReportSec)   ThreadStateDelaySec = 20   (default: StartDelaySec) |
| `ReplayProf` | `0` |  | profiles SynergisticDispatcherTask, the single thread that replays the |
| `CbvStats` | `0` |  | counts the per-draw constant uploads of SynUploadCbvTable 118E28: uploads, bytes, |
| `CbvRecord` | `0` |  | CbvStatsSec = 10 |

### [ShaderSwap]

| key | shipped | evidence | what it does (ini comment) |
|---|---|---|---|
| `Enabled` | `0` |  | 1.7.1: hook PSO creation (D3D12 device vtable) to LOG every shader hash once and, optionally, DUMP the bytecode or |
| `Folder` | `Nioh3EngineFix_shaders` |  | relative to this ini's folder. Dumps go to <Folder>\dump\<digest>.dxbc/.dxil (zip that folder after a session) |
| `DumpMode` | `0` |  | 0 = off, 1 = write the exe-embedded originals to <Folder>\dump, 2 = write every shader blob seen |
| `LogAll` | `0` |  | 1 = also log non-exe shaders once (module + key + stage); long at boot |

### [ShaderHot]

| key | shipped | evidence | what it does (ini comment) |
|---|---|---|---|
| `Enabled` | `0` |  | fast-testing (notes\fast-testing.md): DIAGNOSTIC runtime shader A/B in ONE launch. NEVER SHIP OR PLAY WITH IT ON. |
| `Folder` | `Nioh3EngineFix_hot` |  | relative to this ini's folder |

<!-- END GENERATED -->
