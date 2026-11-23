# Nioh3EngineFix and Nioh3CamKiProbe: feature reference

**What this is.** A per-feature map of `Nioh3EngineFix.asi` (CPU/GPU/loading/pacing patches, overlays, in-game menu,
crash and report plumbing) and `Nioh3CamKiProbe.asi` (a small camera add-on) for Nioh 3, Steam v2.02 only.
**Who it is for.** An engineer who needs to find the code for a feature, its ini key, its default and how it hooks the game.
**Truth source.** The current source tree (release label 1.8.39). Defaults are the ones in
`mod/Nioh3EngineFix/Nioh3EngineFix.ini`, which match `mod/Nioh3EngineFix/src/menu_shipped_defaults.h` (generated from the
packaged ini). Where a file is missing from this repository's `mod/` copy (a 2026-11-02 snapshot) the feature is marked
. Files present in both trees may also be older here.

Related pages, not repeated here: [docs/BENCHMARKS.md](../BENCHMARKS.md) (the only checked fps/load numbers),
[docs/WHY_NIOH_RUNS_BADLY.md](../WHY_NIOH_RUNS_BADLY.md), [docs/subsystems/](../subsystems/README.md) (what each engine
area costs), [research/README.md](../../research/README.md), [PUBLIC_EXCLUSIONS.md](../../PUBLIC_EXCLUSIONS.md),
[CONTRIBUTING.md](../../CONTRIBUTING.md).

---

## 1. Conventions used by every feature

### 1.1 Reading a row

| column | meaning |
|---|---|
| Key | `[Section] Key` in `Nioh3EngineFix.ini`. The ini is the full reference; every key has a comment above it. |
| Default | Value in the shipped ini. `-1` means "auto by OS": resolved at start with Wine's version query (Proton/Wine vs native Windows). |
| 0 / 1 / 2 | Most hook-based features use `0` = not installed, `1` = on, `2` = installed but switched off (so the menu or `LiveCtl` can A/B it live in one launch). Code-byte patches ("launch") can only be chosen at start. |
| Live | Has a row in the in-game menu (or a `Tuning.*` live value) that changes it without a restart. |
| Target | Where the work happens: main thread (`[App]Game`), logic thread (`KIDSTaskScheduler0`), job workers, D3D12 replay thread (`SynergisticDispatcherTask`), Present thread. See [docs/subsystems/](../subsystems/README.md). |

All addresses are image-relative RVAs for v2.02 (PE timestamp `0x6AA7C106`). Names follow [symbols/symbols.tsv](../../symbols/symbols.tsv).

### 1.2 How a patch gets in

- **Build gate.** `mod/shared/known_build.h` classifies the exe (Steam DRM-wrapped v2.02, v2.02 stripped, or unknown). On an
  unknown build the mod refuses to run unless `[General] AllowUnknownBuild = 1`, which falls back to pattern matching
  (`mod/Nioh3EngineFix/src/gamerva.cpp`, table `rvas.json` -> `rvas_gen.h`; entries with no unique pattern stay "known build only").
- **DRM wait.** The game code is encrypted until the Steam wrapper decrypts it; the mod polls up to `[General] DecryptWaitSeconds`
  (60) before scanning or writing.
- **Site table.** `mod/Nioh3EngineFix/patches.json` is the single source of truth for byte-level sites. `tools/gen_header.py`
  generates `src/patches_gen.h`; `tests/test_patterns.py` checks the generated header against the json. A site has an `id`,
  `group` (the feature key), `rva`, expected original bytes and a `kind`:
  `bytes` (in-place rewrite), `call` (E8 call retargeted), `jmp` (E9 to a cave trampoline), `vptr` (a vtable pointer in `.rdata`
  repointed), `ripload` (rip-relative load redirected), `check` (bytes verified only; the hook itself is written by code).
  Every site is verified byte for byte before it is written; a mismatch skips that site and logs why.
- **Caves and thunks.** Hand-written assembly lives in `src/caves.asm`, `drawprep.asm`, `extraperf.asm`, `jt.asm`, `mpcskip.asm`,
  `pmode.asm`, `replayprof.asm`, `samelook.asm`, `asyncslots.asm`, `gpuopt_thunks.asm`.
- **Other hook styles.** D3D12 / DXGI vtable hooks (`gpuopt.cpp`, `osd.cpp` Present hook), IAT chaining (`XInputGetState`,
  timer and sleep imports), and vectored exception handlers for crash guards.
- **Offline harnesses.** Almost every feature has a harness in `mod/Nioh3EngineFix/tests/` (a `run_*.py` wrapper plus a C++/asm
  harness) that runs the cave or the pure logic outside the game. Source comments say when a feature has only been proven offline.
- **Logging.** `Nioh3EngineFix.log` next to the `.asi`; a `stats ...` line per feature every 30 s (`[Profiler] ReportSec`).
- **Orchestration.** `src/dllmain.cpp` (about 4.6k lines) reads the ini, installs everything, runs the stats worker and owns the
  `MOD_VERSION` string. Treat it as the index: each feature's installer is called from there.

### 1.3 Which defaults ship

The ini in `mod/` and `src/menu_shipped_defaults.h` agree. `release/profiles.json` still lists some per-device overrides (for
example `B`, `PP` and `[GpuOpt] Barriers` at 0) that do not match the ini or the generated defaults; treat the ini and
`menu_shipped_defaults.h` as authoritative and the profile json as possibly stale (unverified).

---

## 2. Frame pacing, speed and caps

| Feature | Key (default) | What the player gets | How it works |
|---|---|---|---|
| **VRR frame step** | `[Patches] VRR = 1` | Game runs at real speed when fps is below the in-game cap (stock slows below 120/60/30 because the logic step is tied to the cap). | 16 `ripload` sites in `patches.json` (group `VRR`, first at `0x43593A`) feed the logic the measured frame interval, clamped per cap by `[VRR] Min30/Max30/Min60/Max60/Min120/Max120`. Groups `VRRMENU` and `VRRHOLD` keep menu and hold timers consistent. Per-cap switches `[VRR] Tier30/Tier60/Tier120`. |
| VRR in co-op | `VRROnline = -1` | VRR suspends itself in an online session (net manager connected or online play mode) and returns 5 s after it ends. `1` forces on (unsafe for co-op), `0` forces off. | Detected in `dllmain.cpp`; logged on `VRR:` lines. Only offline play has been tested. |
| High-refresh cap | `[VRR] MaxFps = 120` | With the in-game cap at 120: 144/165/240 raise the cap, `0` = unlocked (1000). Above 120 the engine's tier stays 120 (experimental). | Raises the clamp upper bound. |
| Tier 120 floor | `[VRR] Tier120Floor = 0` | Stopgap for held-attack misfires under the 120 cap well below 120 fps: below the floor for `Tier120FloorHoldMs` the mod runs the 60 cap for `Tier120FloorProbeS` s, then re-measures. | `tests/run_vrr_floor_harness.py`. `HoldKeyGap` is the targeted fix. |
| Menu key repeat | `[VRR] MenuQuantize = 0` | Only for menu key-repeat oddities. | Quantizes menu hold/repeat timers. |
| **L1 limiter** | `L1 = 1`, `L1_MarginUs`, `L1_MinSleepUs`; `L1OnWine = 0` | Frame limiter sleeps on a high-resolution timer instead of `Sleep(0)` spinning. | `call` site `0x19DF57E`. Needs Windows 10 1803+. Skipped under Proton unless `L1OnWine = 1` (Wine may accept the flag without honouring it). |
| **DS / DQ** | `DS = 1`, `DQ = 1`; `DS_MarginUs`, `DS_DqMarginUs`, `DQ_QuantumUs = 200` | Less main-thread CPU near the cap. The main loop retries the frame function after a blind 1 ms QPC spin when the logic packet is not ready; DQ re-checks every `DQ_QuantumUs`, DS sleeps most of the quantum. | `src/drawprep.cpp` + `drawprep.asm`; sites in groups `DS` (`0x1A74BDA`) and `DQ`. `tests/run_drawprep_harness.py`. Background: [research/mainthread-drawprep.md](../../research/mainthread-drawprep.md). |
| **TG tick governor** | `TG = 0` (opt-in) | Logic tick cap follows the rendered fps (floor `TG_Floor` 30) so the CPU stops computing ticks that are never shown. Changes the simulation step to 1/30 s; a button tap must then be held about 33 ms. | Needs VRR and L1; refuses to engage and logs why otherwise. `TG_RatioPct = 125`. |
| **R1 frame-latency wait** | `R1 = 0` | Waitable-swapchain wait with a 2 ms timeout instead of a busy poll. Hook installed, behaviour off by default. | `ripload` sites at `0x4A5346`; `R1_TimeoutMs = 2`. |
| **WL handle leak** | `WL = 1` | Stock game fetches the swapchain latency handle every frame and never closes it (about one leaked handle per frame on Windows). WL closes the previous one. | `call` site `0x4A5338`. Never triggers under Proton (no waitable swapchain). |
| Max frame latency | `[Latency] MaxFrameLatency = -1` | `IDXGISwapChain2::SetMaximumFrameLatency`. The game never sets it (value 1). `-1` = set 1 explicitly under Proton, leave alone on Windows. 2-3 can raise fps when GPU-bound at the cost of up to a frame of delay. | Applied to the game's waitable swapchain from the overlay/Present hook code. |
| **VrrPresent** | `[Latency] VrrPresent = 0` | On a FreeSync/G-Sync PC the game's Present `SyncInterval` can stick at 1, turning independent flip into composed flip (more latency, frames never shown). `1` forces interval 0 + `ALLOW_TEARING`; `-1` only while the game's VSync is off. Windows only. | `src/pmode.cpp`, `pmode.asm`. Companion log `[Debug] PresentModeLog = 1` records who sets `SyncInterval`. |
| **IK input at frame start** | `IK = 1` | The game reads the pad at the start of the logic frame instead of the end of the previous one (a 4-9 ms fresher sample, same image). Toggle it while not pressing. | `call` sites at `0x5AB6CF`; `IK_PadWaitCapMs = 0`. Safe online (only read time moves). |
| **Timer resolution** | `TR = 0`, live row "Steady timer resolution" | The game's HID pad threads wrap every `Sleep` in `timeBeginPeriod(1)/timeEndPeriod(1)`. TR holds 1 ms once and skips those pairs. No effect under Proton. | `src/timerres.cpp`; `stats/s TR:` line. |
| **FG presenter** | `FG = 1`, `FGOnWine = 0` | FSR frame-generation presenter thread sleeps on a high-resolution timer instead of spinning on the clock (about 0.2 core less with in-game FG on). | `call` site `0x3777A40`. |
| **FGC copy guard** | `FGC = 0`; `[Menu] FrameGenToggle = 3` enables it under Proton | The game's FidelityFX backend skips a missing texture everywhere except its copy job, which reads address `0x90` and crashes on the first generated frame (a known vkd3d-proton report). FGC skips that copy. | `patches.json` groups `FGC` and `FGP`. `FrameGenToggle` is otherwise **status-only** for every value: change frame generation in the game's own Options. |
| **Held attack fix** | `[General] HoldKeyGap = 0` (mode `3` = fix) | At a high fps *setting* the game counts button-hold time by the setting, not real fps, so under load a held attack can miss its martial art and swing a heavy first. Mode 3 counts by the real clock. Modes 1 and 2 are developer experiments that change move timing. | `src/holdkeygap.cpp` patches the input class's held-getter vtable slot (verified equal to `0x901BA4`) to return `max(stock, real seconds held * 60)`. Only a small manual in-game count exists; no fps claim. `tests/run_holdkeygap_harness.py`. |

Background research: [research/frame-timing.md](../../research/frame-timing.md), [research/input-thread.md](../../research/input-thread.md).

---

## 3. CPU fixes: main thread, logic thread, workers

All are v2.02 only and give stock output. Most save CPU time with no fps change on a GPU-bound machine; section 14 lists
what was actually measured.

### 3.1 Main thread (`[App]Game`) and render prep

| Key (default) | Change | Source |
|---|---|---|
| `W1 = 1` | Zero-fill loop on GPU upload memory rewritten as `rep stosd`. | `patches.json` `W1` (`0x61BAC`) |
| `W2 = 1` | Per-vertex quad writer without read-modify-write on upload memory (4 sites). | group `W2` |
| `W1B = 1`, `RPA = 1` | Glyph-padding clear with a plain store (120 -> 25 ns per row on write-combined memory, offline); `RenderParamsAssign` (`0x247950`) copies 21 byte ranges with 16-byte moves instead of 29.6 KB of dword moves; bit-exact on 300k random calls. | `tests/run_w1b_rpa_harness.py`, `tools/gen_rpa_copy.py` |
| `B = 1` | Draw-list group-length search gallops instead of a full binary search. | group `B` |
| `PP = 1`, `PP_Period = 4` | Page-pool usage sweep only every Nth frame. | group `PP` |
| `FPC = 1` | Per-frame cache for material parameter lookups (a `vptr` patch). | group `FPC` |
| `P1E7 = 1` | Prefetch of the next texture's streaming object in the loop at `0x1E7050`. | `src/drawprep.cpp` |
| `MA = 1` | 25 locked "atomic load" idioms (`lock cmpxchg` against zero) on the main thread become plain loads. | group `MA` |
| `S1 = 1` | `PAUSE` in the main thread's end-of-frame spin on the submit thread. | group `S1` |
| `MW`, `RD`, `FWP` (all 0) | Experimental waits: paired meshlet completion wait, paired renderer-drain wait, frame-wait park (`FWP = 1` park, `2` measure the stock spin only). | `src/meshletwait.h`, `drainwait.h`; `framewaitpark.h`; harnesses `tests/run_fwp_harness.py`, `meshletwait_harness.cpp` |

### 3.2 Logic thread and job workers

| Key (default) | Change |
|---|---|
| `AABB = 1` (`2` = installed, off) | Character bounding boxes in SSE, bit-identical (`jmp` cave at `0x6EA50`). |
| `BPO = 1`, `BPO_Mask = 7`, `BPO_TableBits = 14` | `BoundsFromPartObbs` (`0x71C60`): per-part rotation rebuilt from packed halves every frame; cached per 8-byte key (bit-identical), plus plain loads for the ready word and a forwarded box copy. |
| `MQ = -1` | Skeleton bone matrices (`MatrixFromScaleQuatTrans`) in SSE; on under Proton, off on Windows (no gain measured). Needs SSE4.1. |
| `E1 = 1`, `E1_N = 2` | Effects nobody can see update every Nth pass with Nx the time step (logic thread). |
| `ED = 1` | Effect-update workers whose list is empty are neither woken nor waited for. |
| `CP = 1`, `SPF = 1` | Collision workers and the stage-update worker prefetch the next colliders/placeables. |
| `DL = 0` | Collision skips a part-map search whose result is discarded. |
| `PT1 = 1` | Texture tile requests take the streaming pool lock once per texture instead of once per tile touch. |
| `GL1 = 1` | Mission thread prefetches task entries 3 ahead in two list walks (sites `0xE266D1`, `0xE26690`). |
| `LK = 1` | 18 locked "atomic loads" in worker code become plain loads. |
| `M1 = 0`, `M2 = 1`, `M2S = 0`, `J1 = 1` | Meshlet-request workers: no per-chunk capacity re-sum (M1), plain loads for page bitmaps (M2), completion-CAS seed reads only (M2S, startup only), idle job polls with plain loads (J1). |
| `X1 = 0` | Per-frame pad scan skips XInput slots known to be empty. |
| `CpuStats = 0` | Counts MQ calls and DL skips on a `stats cpu:` line. |

The "atomic load" rewrites (`LK`, `MA`, `M2`, `J1`) rest on one argument in the sources: an aligned load is already atomic, and the
`lock` prefix only adds a full fence that stops pointer-chasing loops from overlapping cache misses. Research:
[research/worker-jobs-opt.md](../../research/worker-jobs-opt.md), [research/job-system.md](../../research/job-system.md),
[research/cpu-profile.md](../../research/cpu-profile.md).

### 3.3 Wakes, spins and thread policy

| Key (default) | What it does |
|---|---|
| `W3 = 1`, `W3_TimeoutMs = -1`, `W3WineSleep = 1` | Window message pump sleeps until input/1 ms (3 ms under Proton) instead of spinning a core. |
| `W6 = -1` | Main thread waits for Present to return with a short spin (`W6_SpinUs = 50`) then `WaitOnAddress`, instead of a `Sleep(0)` loop. Auto: on under Proton and on native Windows per the ini comment. |
| `GF = -1` | Where the game spins `while (fence not reached) Sleep(0)` (main thread `7D9E1C`, worker poll `117D6C`) the mod registers an event with `SetEventOnCompletion` and sleeps on it. Auto: on under Proton, off on Windows. |
| `WK = 1` | Job-system wake gate: skip waking a worker when no queue holds work. |
| `WG = 1`, `WG_MaxPending`, `WG_WindowUs` | Skip a second wake-one while a worker of the same pool is already woken and not yet running. |
| `WorkerCap = 0`, `KidstsCap = 1`, `P12 = 0`, `TSY = 0` | Job pool sizing. `WorkerCap` is for handhelds only; `KidstsCap = 1` sizes pools to the process affinity at boot; `P12` keeps the stock pool width on 12-thread CPUs; `TSY` sets the TrySync pool (3..8). |
| `RTP = 0` (live "Render threads first"), `RTP_Level = 1` | Raises the Windows priority of the render critical path (`[App]Game`, `SynergisticDispatcherTask`, the Present thread) so logic work does not delay the frame on CPU-bound 6-core PCs. `src/thrsched.cpp`. |
| `FileLoadPrio = 1`, `FileLoadPoll = 1` (`FileLoadPoll_MaxCpus = 6`), `PollFix = 1` | Loader threads (`KTGLGraph.Cache.FileLoad`) run below normal and back off their poll loop on low-core affinity; `PollFix` applies the back-off at any core count and adds wait-on-work. `tests/run_pollfix_harness.py`. |
| `PCM = 1` (live "Faster pipeline lookup") | Memo (4096 slots, same hash and same 128-byte key) in front of the PSO cache B-tree walk on the replay thread. `src/psomemo.cpp`. |

Research: [research/d3d12-submission.md](../../research/d3d12-submission.md).

---

## 4. Loading

Background: [research/loading.md](../../research/loading.md), [research/loading-ally-fixed.md](../../research/loading-ally-fixed.md).
Most of these keys matter only under Wine/Proton, where per-file-read overhead is large.

| Key (default) | What it does | Source |
|---|---|---|
| `EP = -1` | Event pool for archive reads: the game creates and closes one kernel event per file read (hundreds of thousands per load; each a server call under Proton). EP reuses events that were signalled. Auto: on under Proton. | `patches.json` groups `EPL`, `EPF`; `src/readfix.cpp` |
| `SR = -1` | Synchronous redirect: tiny overlapped reads become plain reads on a second read-only handle and are reported finished at once. Wine/Proton only (refused on Windows). `1` = during loads, `2` = always. | `src/readfix.cpp` |
| `RC = 2`, `RCMB = 64`, `RCKB = 64`, `RCMaxReadKB = 64` | Read cache: small reads are served from aligned RAM blocks (1 = during loads, 2 = always). | `src/readfix.cpp` |
| `AQ = 0`, `AQThreads = 4`, `SH = 0` | Async archive reads on worker threads; synchronous archive handles (the crude sibling of SR). Opt-in. | `src/readfix.cpp` |
| `PF = 0` (`PFThreads`, `PFLeadMB`, `PFTraces`, `PFMatchPct`) | Trace prefetch: remembers which file ranges a load read (`Nioh3EngineFix_pf0..3.bin`) and reads them a little ahead so the game's reads hit the OS cache. Only `package` files. | `src/prefetch.cpp` |
| `LC = 0`, `LW = 1` | In-flight limit of the two resource-load managers (10 -> 16, or 11..64); `LW` replaces their three `Sleep(1)` polls with an event wait capped at 1 ms. | groups `LC`, `LW` |
| `RWb = 1` | The resource-state spin wakes a ResourceLoad worker once per 256 spins instead of every spin. | `0x1A3146A` |
| `LoadOnlyCpuFixes = 0` | GF, W6 and RWb are active only while a level loads (a 10 ms thread watches the load-window byte `G+58Ah`); stock code while playing. | `src/loadonly.cpp` |
| `Inflate = 0` (`2` = verify) | libdeflate in place of the game's zlib 1.1.4 for its 16 KiB chunks, same bytes out, falls back to the game on error. | `src/zinflate.cpp`, `third_party/` |
| `FileLinger = 0` (`FileLingerMax = 32`, `FileLingerFree = 24`) | Keep released archive handles open instead of close/reopen per streaming touch. Off by default (more hitches on one device, neutral on another). | `src/fhlinger.cpp` |
| `[General] AutoContinueLoad = 0` | Closes the post-load tip page as soon as the game would accept a press (one call site, `0x1CECAED`, made to read "pressed"). Live menu row "Skip loading tip screen". | `src/autocont.cpp` |
| `[Memory] LM = 0` | Load-memory sweeper: once a second `VirtualAlloc(MEM_RESET)` on dead pages of free heap chunks, with a start-up self-test that disables it on failure. | `src/loadmem.cpp` |
| `[Profiler] LoadStats`, `LoadGates`, `ReadProbe` | Measurement of each load window (section 11). | `loadstats.cpp`, `readprobe.cpp` |

---

## 5. PSO and shader-cache work

Research: [research/stutter.md](../../research/stutter.md).

| Key (default) | What it does | Source |
|---|---|---|
| `[Patches] PJ = 0` (`PJ_MaxMB`, `PJ_QuietMs`) | PSO journal: records pipelines the game builds after its boot list into `Nioh3EngineFix_pj\` and prewarms them on one lowest-priority thread while the title screen is idle (pauses during loads). `2` = journal only. | `src/psojournal.cpp` |
| `PJH = 0` (live "Pipelines ready before first use", `PJ_KeepMax = 4096`) | Hand-over: the game's `Create*PipelineState` for a desc the prewarm already built returns that object instead of compiling mid-frame. Implies `PJ = 1`. | `src/psojournal.cpp` |
| `PL = 0` (`PL_MaxMB`) | `ID3D12PipelineLibrary` for the boot pipeline list (`Nioh3EngineFix_pj\pl_*.bin`), checked against game build, GPU, driver, size and checksum. Windows only; off when ShaderSwap, GPUProf 2, GIProbe or GIMerge hook PSO creation. | `src/psojournal.cpp` |
| `SPR = 0` | Measurement: times runtime graphics-PSO creates and upload-heap fallbacks (`stats spr:`). | `dllmain.cpp` |
| `[ShaderSwap] Enabled = 0` | Hooks PSO creation to log every shader hash once, dump bytecode, or substitute `<Folder>\<digest>.dxbc/.dxil` for matching shaders (this is how shaders embedded in the exe can be replaced). Replacement shader files are **not** distributed. | `src/psoswap.cpp` |
| `[ShaderHot] Enabled = 0` | **Diagnostic only, never play with it on.** One-launch A/B of a shader pair via twin PSOs switched by `LiveCtl Hot.<set>`. | `src/psoswap.cpp`, `livectl.cpp` |

---

## 6. GPU-side features

Research: [research/renderer/](../../research/renderer/00-map.md), [research/gpu-tiles.md](../../research/gpu-tiles.md),
[research/shadows-redundancy.md](../../research/shadows-redundancy.md), [research/vkd3d-audit.md](../../research/vkd3d-audit.md).

### 6.1 Submission, uploads and render-config tweaks

| Key (default) | What it does |
|---|---|
| `[GpuOpt] Barriers = 2` | D3D12 `ResourceBarrier` batching on the game's command lists (`src/gpuopt.cpp`). `2` = hooked with batching **off** so the live row works; `1` = on. Options `MergeAcrossBinds`, `DropDuplicateUav`, `DropUndoPairs`, `MaxBatch`. Measured negative on one PC when on (section 14). |
| `R2a`, `R2b`, `R1` | Proton/vkd3d-only barrier rewrites (give COMMON->X transitions their real previous state; drop read->read transitions). `R2b` and `R1` can corrupt output; judge by image compare only. |
| `TileBatch = 0` | Merges consecutive `UpdateTileMappings` on one queue when the final mapping is provably identical (`2` = count only). |
| `[Patches] CBN = 1`, `CBT = 1`, `CBS = 0`, `CBF = 0` | Per-draw constant upload reduction on the replay thread (`cbvupload.cpp`, `cbvstage.cpp`). CBN skips an upload whose bytes equal the slot's previous upload in the same list; CBT reuses the previous CBV table when every slot was a no-op; CBS = streaming copy; CBF = stage the ring in system memory and batch-copy to VRAM (test only). Detail: [research/renderer/cbv-upload.md](../../research/renderer/cbv-upload.md). |
| `[GpuOpt] CbvMirror = 0` (`CbvMirrorMB`, `CbvMirrorSites`, `CbvMirrorQueue`, `CbvMirrorComputeCopy`) | For GPUs without Resizable BAR: mirror per-draw constants from the system-memory upload ring into a VRAM ring. `1` = only when the heaps really are in system memory; never on UMA or Proton. `src/upmirror.cpp`. |
| `SceneBufMirror = 0` (`SceneBufMirrorQueue`) | The same idea for the meshlet scene buffers (instance matrices, LOD roots). Experimental. `src/scenemirror.cpp`. |
| `FrameOverlap = 0`, `[Tuning] FO_Pct = 25` | Moves frame N's replay tail and Present to the replay thread so frame N+1 records meanwhile. Hard fallbacks on resize or device reset. Experimental; see [research/renderer/frame-overlap.md](../../research/renderer/frame-overlap.md). `src/frameoverlap.cpp`. |
| `[Tuning] KL_Div = 0` | Hands command-stream segments to the replay thread sooner (live only). |
| `AllowProxyLists = 0`, `InstallDelaySec = 20` | If another mod wraps the D3D12 command lists (a ReShade-style proxy) GpuOpt leaves them alone; hooks install a while after d3d12/dxgi load. |
| `[Patches] AS = 0`, `AS0 = 0`, `[Tuning] AS_Slots` | Create the engine's own async-compute "slots" that stock disables on non-NVIDIA GPUs (slot 0 effects, 11 grass, 12 trees), with a live on/off row. Risk: stock disables slot 0 for unknown reasons; corruption or a hang is possible. `src/asyncslots.cpp`. |
| `ESM1 = 1` | Directional exponential shadow map at 1024 instead of 2048 (it feeds fog in-scattering and indirect shadowing only). Fog light shafts may be slightly softer. Code bytes, launch. |
| `MPCS = 0` | Drops redundant MeshletPreculling tail recordings; requires a specific replacement shader file whose hash the mod checks (file not distributed). Self-check: `[Profiler] MpcCheck`. `src/mpccheck.cpp`, `mpcskip.asm`. |
| `VSMA = 0` (live "Fewer shadow tile redraws"; `VSMA_K`, `VSMA_Front`, `VSMA_First`) | The virtual shadow map redraws waiting tiles on every visit although nothing samples them; VSMA draws them 1 visit in K. `src/samelook.cpp`. |
| `LPVR`, `SKYR`, `FOGT`, `CLDR` (all 0; values `LPVR_N`, `SKYR_V`, `FOGT_V`, `CLDR_V`) | Write one render-config field right after the game's `ApplyGraphicsSettings` (light-propagation volume, skylight, fog history blend, cloud reprojection update rates). "Same look" package; none is a default. |
| `GIMerge = 0` (`GIMergeQualify = 60`) | Skips the nearly empty stencil light-group-1 GI gather pass and lets the second accept both groups; merges only after N clean pairs in a row; `2` = observe only. `src/gimerge.cpp`. |

### 6.2 Extra performance (changes the look): `[ExtraPerf]`

`Enabled = 1` installs hooks that pass the game's values through until a level is set. Every key is live from the menu page
"Extra performance (changes the look)" (`src/extraperf.cpp`, `extraperf.asm`; `extraperf_menu.h`).
Each drives an engine control (a render-config field the game refills per frame, a quality-table result or a fraction), not a draw skip.

`Preset`: 0 = use the keys, 1 Performance, 2 Potato, 3 Risky, 4 Extreme, 5 Extreme HQ. Per key: `XCLD` cloud steps, `XFOG`/`XFOGS`
fog grid, `XGI` GI/reflection gather scale, `XSHD` sun shadow distance, `XSHF` shadow filter taps, `XLOD` meshlet LOD scale, `XTER`
terrain tessellation, `XPRT` GPU particle share, `XAO` ambient occlusion, `XPOST` motion blur / depth of field / bloom, `XSSR` SSR,
`XSHARP` FSR sharpening, `XRES` internal render scale (restart), `RenderDistance` (live; characters excluded by default). `XHALF*`
(alternate-frame compute reuse) is off unless `XHALF_Force = 1` because it made lighting flicker; its draw-level skips live in `src/skipfx.cpp`.

### 6.3 Variable-rate shading

`[Perf] VrsCeiling` (`0` off, `1` whole-frame 2x2, `2` 4x4 where supported, `3` selective) with `VrsLayouts`, `VrsRate`,
`VrsSkipSkinned`, `VrsKeepCharacters`, `VrsGBufferMotion1x1`, `VrsWideExclude`. Mode 3 captures per-PSO metadata at creation
(`src/vrs_selective.h`) and coarsens only audited world meshlet draws. Menu page "Variable rate shading" (`vrs_menu.h`). Started
with 0, nothing is hooked and a restart is needed to turn it on. Hook code: `gpuopt.cpp`, `vrs_ceiling.inl`.

### 6.4 ReBAR notice

`[General] ReBarNotice = 1`: about 40 s after the first rendered frame the mod checks Resizable BAR / SAM once (log line `RB:`);
on a desktop GPU with it off it shows a one-time overlay notice, and the file `Nioh3EngineFix.rebarnotice` records that it was
shown. Never on handhelds or Proton. `src/rebar.cpp`. The fix is a BIOS setting, not a patch.

---

## 7. Crash guards (default on, restart to change)

All except `FGC` are. Each guard verifies the code bytes at start, acts only on the exact
fault, and leaves every other fault to the normal crash report.

| Key | Guards against | Mechanism | Source |
|---|---|---|---|
| `[General] PadListGuard = 1` | Null link in the pad-manager list while another thread relinks it (crash a few seconds after launch with a controller). | 13 list-walk sites (group `PG`, first `0x42FB3E`) with caves: a null `next` ends that walk for the frame. | `padguard.cpp`, `padguard_cave.h` |
| `AiChainGuard = 1` | Boss AI chain-row copy reading past the end of a list (`0x31074A`). | Vectored handler resumes at the loop exit, so the vector keeps the readable rows. | `aichainguard.cpp` |
| `MotionGuard = 1` | Missing or freed animation data right after a level load (root-motion track lookup `0x48A164`; motion key walk `0xA0C2A`). | Handler returns the game's own "no data" result for that frame. | `motionguard.cpp`, `motionguard_bytes.h` |
| `ResInfoGuard = 1` | Resource header whose sizes do not add up reaches a leftover breakpoint during load (`0x18CBDF7`). | Mismatch branch (`0x2E0384`) retargeted to the existing "header rejected" return. | `resinfoguard.cpp` |
| `AssertGuard = 1` | Any failed game assert reaches the same leftover `int3` and closes the game. | Handler on that one breakpoint resumes after it and logs the game's message (`ASSERTGUARD:`). | `assertguard.cpp` |
| `[Patches] PTS = 1` | Start-up race: the Present thread starts before its wake event exists (fatal error 1-2 s into boot). | A 7-byte load becomes a call to a stub that waits for the handle. | `ptstart.cpp` |
| `FGC` | See section 2. | | |

---

## 8. Crash reports, player reports, update check

| Feature | Key (default) | What it does | Source |
|---|---|---|---|
| **Crash report** | `[Debug] CrashReport = 1`, `CrashDump = 0` | On an unhandled exception, abort/terminate/invalid parameter or the engine's fatal popup, writes `Nioh3EngineFix-crash-<date>.txt` and `-latest.txt` beside the `.asi`: exception, module+offset stack, registers, mod build hashes, loaded modules, last mod state, last log lines. A helper thread with its own stack writes it, so stack overflow works; stock crash handling then continues. | `src/crashreport.cpp` |
| Crash blame | (part of the above) | Names the owner of each frame: our mods, the game, Windows, a GPU driver, a proxy DLL in the game folder (mod loaders, injectors) or an injected overlay. Classes come from paths and version resources; nothing is hardcoded per mod. | `src/crashblame.cpp` |
| GPU removal reason | `[Debug] Dred = 0` | Adds `GetDeviceRemovedReason` to crash records; DRED breadcrumbs only when `Dred = 1` (costs GPU time). | `src/devremoved.cpp` |
| **Automatic crash upload** | `[Report] CrashUpload = 1`, `CrashUrl` | Each crash is also written as JSON to `Nioh3CrashReports\pending\` and posted without a prompt (once at crash time, then by a low-priority thread on later launches until accepted). A 4xx answer moves the record to `rejected\`. No names or user paths. | `src/crashupload.cpp` |
| **Report box** | `[Report] Url`, `ReportUrl`, `SteamKeyboard = 1` | The menu "Report" row opens a text box (on a controller: Steam's keyboard, else a built-in key grid). Send writes the snapshot, adds log tails and the last menu/input edges, scrubs user and computer names, home paths and 17-digit ids, keeps a copy in `Nioh3Reports\`, and POSTs JSON with WinHTTP (works under Wine). Body cap 256 KB; the game thread never waits. | `src/reportsend.cpp`, `menu_reportbox.h` |
| Performance snapshot | (part of reports) | Last ~10 s of fps, 1 % low and frame time, a CPU-bound / GPU-bound verdict, hardware, the game settings that matter (decoded on v2.02), the mod's perf keys, per-thread CPU. One float per frame; no new hook. | `src/perfsnap.cpp` |
| Input ring | (part of reports) | Last 150 controller, hotkey and menu edges kept in memory for crash records; typed report-box characters are ignored. | `src/inputlog.cpp` |
| **Update check** | `[Update] Check = 1`, `Url` | Background HTTPS fetch of a small version json; only the parsed numeric version reaches the HUD (a line under the pause hint). The "Update + Page" menu row opens the website and shows the updater in the game folder. No identifiers, cookies or redirects; silent offline. | `src/updatecheck.cpp`, `updatecheck_core.h` |
| Reverse Mods row | (menu) | Same effect as the "Restore Vanilla Game" script: marks the mods off for the next start. | `menu.cpp` `DoReverseMods` |

Opt-in DXGI crash evidence: `[Debug] DxgiDiag = 1|2` writes `Nioh3EngineFix-dxgi-report.txt` plus detail files next to the ini (`src/dxgidiag.cpp`).

---

## 9. Overlays, OSD, boot notice

### 9.1 In-game overlay (`src/osd.cpp`, `osd.hlsl`, `osd_font.h`)

Drawn into the game's own frame from a hook on the bottom-most `IDXGISwapChain::Present/Present1`, so it appears under gamescope
and on generated frames, with no external files. The menu draws through the same primitives (see the header comment of `osd.h`).

| Key (default) | What it shows |
|---|---|
| `[Overlay] FpsCounter = 1`, `FrameTime = 1`, `SpeedFactor = 1` | "FPS" = frames the game presents (plus "game N Hz" when update ticks differ), "FG FPS" = frames shown including generated ones, frame time in ms, game-speed factor (1.00x = real time). |
| `Position`, `Scale`, `Brightness`, `OffsetX/OffsetY` | Corner, text size (whole numbers look sharpest), brightness (HDR compensated), offsets in 1080p pixels (TopLeft starts 160 px down to clear the game's bars). |
| `[General] StartupPopup = 1` (`StartupPopupSeconds`, `StartupPopupDelay`) | Status popup once the game renders: mod loaded, active fixes, frame generation, VRR, warnings. |
| Notice toast | The one-time ReBAR notice (up to 4 lines) after the popup. |
| `[Menu] PauseHint = 2`, `PauseHintSource = Both`, `PauseHintFlags = 58B 58C` | Top-right "Mod menu: F10 / Esc x3 / Start x3" reminder. The source decides "a game menu is open" from the game's own menu signal, the engine stop flags, or both. |

### 9.2 Controller and input displays

| Key (default) | What it does | Source |
|---|---|---|
| `[Menu] ControllerDisplay = 0`, `Size`, `Opacity`, `PressColor`, `ControllerDisplayX/Y` | Live controller picture: pressed buttons light, sticks move, triggers fill. `ButtonStyle` picks the skin (PlayStation = DualSense, Xbox = Xbox). Fed from the game's `XInputGetState` import *before* the menu masks anything. Art credits: [CREDITS.md](../../CREDITS.md). | `src/padviz.h`, `padviz_atlas*.h`, `padviz_png.h`, `tools/gen_padviz_atlas.py` |
| `InputDisplay = 0`, `InputDisplaySide`, `InputDisplayRows = 20` | Fighting-game style input history (newest on top: frames held, direction, buttons). Read-only. | `src/inputhist.h`, `inputhist_draw.h`, `inputhist_dpad.h` |
| `PadDebug = 0` | Shows the raw pad as the mod reads it and whether a mod is hiding the pad from the game. | `paddebug.h` |
| `ButtonStyle = Auto` | Footer glyphs follow the connected pad family (PlayStation, Nintendo by position, else Xbox). | `pad_family.h`, `padcard.h` |

A DualShock/DualSense read through the game's own HID path does not go through XInput, so the pad-latency probe (`src/padlat.cpp`) cannot see it.

### 9.3 Boot notice skip

`[Debug] SkipBootNotice = -1`: the game shows its autosave notice on every boot and, when its VRAM check fails, a Yes/No "reset all
settings?" warning. `1` skips both (the warning is treated as No); `2` only logs the boot sequencer state; `-1` = on only when `Tour` or
`VirtualPad` is on, otherwise stock. Sites `BN1` (`0x1C35393`) and `BN2`. `src/bootnotice.cpp`.

---

## 10. In-game mod menu (`src/menu.cpp`, `menu_pages.h`, `menu_style.h`, `settings_catalog.h`)

- **Open:** keyboard `[Menu] Key = F10`, `KeyTripleTap = Escape` (3 presses inside `TapWindowMs`; the first presses are held back for
  `TapGapMs`), controller `Gesture = TripleTap` on `HoldButton = Start` (or `Hold` for `HoldSeconds`). `SwallowTaps = 1` hides tap
  sequences from the game. Works on the title screen, in gameplay and in the game's pause menu.
- **Freeze:** `FreezeGame = 1` adds one request to the engine stop-flag counter (`58B`) while the menu is open and takes exactly that one back.
- **Layout:** `[Menu] Layout = 0` (default) is the two-page menu. The first screen has Boss Matches and Practice (hosted by the add-on
  mods), Display Overlays, Camera settings, Optimize and Close. `Layout = 1` is the older grouped pages. Pages are the `MenuPage`
  enum in `menu_pages.h`; row ids are append-only so saved state and remote control stay stable.

| Page | Contents |
|---|---|
| Optimize | Preset row, then CPU / GPU / Game / Frame Gen groups. Rows map to ini keys through `settings_catalog.h` (generated from `mod/settings/catalog.json`, checked by `mod/settings/test_catalog.py`). A row that is not live says "next launch" or why it is unavailable. |
| Display Overlays | Controller overlay, Frame meter (the Training add-on's meter: style, size, per-row toggles), Input display, FPS counter, Time of day / Weather (these write Training's persistent sky stats and show "needs Training" without it), Game HUD. |
| Game HUD | `HideGameHud` master plus per-widget hides (`HideHudPlayerBars`, `Style`, `Stance`, `Items`, `Guardian`, `Amrita`, `Money`, `BossBar`, `Minimap`, `EnemyBars`, `LockOn`, `Weapons`, `SoulCores`). One shared "should I show" check at `0xF0228` makes every HUD widget virtual return 0; the first 8 bytes are swapped atomically (`src/hudhide.h`). Per-element patterns: `hud_patterns.h`. |
| Camera settings | Left/right, down/up, distance, lens and reset. Writes `Nioh3CamKiProbe.ini` (section 12); applies within 2 s. Also the motion blur / DoF / bloom option (`XPOST`). |
| Save files | Native (no Python) save-slot library: first-idle-title backup, named copies, switch/backup actions gated to the title screen. Keys `[SaveFiles] Enabled`, `LiveTitle`, `Native = 1`. `src/savefiles.cpp`, `savelib.cpp`, `titlegate_core.h`. Real saves are never changed by setup. |
| Game | "Player log" toggles the Training add-on's player logging (exports `Nioh3Training_PlayerLogGet/Set`). |
| Report, Update + Page, Reverse Mods | See section 8. |
| Configuration by ini section | Every shipped key. "Save to ini" writes back with `WritePrivateProfileString` (comments stay). Startup rows need save plus restart. |
| Experimental / Diagnostics / Next launch | CPU, GPU, scheduling and Proton rows and the profilers of section 11. |

Tests: `tests/run_menu_harness.py`, `menu_defaults_test`, `menu_prompts_test`, `menu_tabbar_test`, `menu_home_test`, `menu_mode_test`,
`run_settings_preservation.py`.

---

## 11. Measurement and profiling features

These do not change what the game does unless noted. Default 0 unless noted.

| Key | Output | Source |
|---|---|---|
| `[Profiler] GPUProf = 1` / `2` | **GPU pass profiler.** `1` switches on the engine's own debug-marker bytes (record `g+2A62h`, replay `g+552h`) and times each marker with timestamp queries read back 6 frames later; the menu shows the top 5 and the log has `GP:` lines. `2` also bills GPU time between PSO changes to the PSO shader hashes (`GP2:`); sampling `GPUProf2_SampleEvery = 8`, coarse stamps `GPUProf2_MinRun = 4`. Works on Windows and under vkd3d-proton. `GPUProf_Markers = 1` only enables markers for PIX/RenderDoc captures. | `gpuprof.cpp`, `gpuopt.cpp` |
| `GQ`, `MeshletProbe`, `MpcCheck` | Graphics-queue gap census; meshlet front-end CPU/GPU probe; MPCS self-check. All need `GPUProf = 2`. | `gpuprof.cpp`, `mlprobe.cpp`, `mpccheck.cpp` |
| `JT`, `SP`, `ThreadState` | Job-timing / logic-join probe; in-process sampling profiler (`[App]Game` plus round-robin workers, `SP main ...` lines); kernel thread-state census (CPU, READY %, context switches). | `dllmain.cpp`, `sp.cpp`, `thrsched.cpp` |
| `Backend`, `ReplayProf`, `CbvStats`, `CbvRecord` | D3D12 back-end waits (`BKS:`); per-token-handler and per-D3D12-call profile of the replay thread (`RPF:`); constant-upload counters; offline-replay trace. | `bkstats.cpp`, `replayprof.cpp`, `cbvupload.cpp` |
| `LoadStats` (1-3), `LoadGates`, `ReadProbe` (1-2) | Per-load window summary, gate timing of `LoadIsLoadEnd`, and per-call-site read statistics with an optional binary trace. | `loadstats.cpp`, `readprobe.cpp` |
| `MemStats` (1-2), `VSForceSysUpload`, `SceneBufProbe`, `TileStats` | Process, VRAM and system memory census with a VRAM-streaming probe; scene-buffer census; tile-mapping census. | `memstats.cpp`, `scenemirror.cpp`, `tilestats.cpp` |
| `NodeTimer` (F11 arms) | Stores the engine's own stats record on each render entry so the game times that pass. No code patch. | `rnt.cpp` |
| `FnProbe` (+ `FnProbeList`) | Live hit, caller and argument probe of up to 32 functions chosen by RVA (their first bytes must be plain moves/pushes). | `fnprobe.cpp` |
| `PadLatency` | Controller-to-present and to-GPU-done latency percentiles via a chained `XInputGetState`. | `padlat.cpp` |
| `[Debug] HitchLog = 1`, `HitchLogMs = 100` (on) | One `HITCH` line per frame gap over the threshold with game-thread busy vs wait split, last mod-instrumented wait, loader queues and memory. Summarize with `tools/hitch_from_log.py`. | `hitchlog.cpp` |
| `GIProbe`, `WakeStats`, `TS`, `CpuStats` | GI gather census; worker wake counts; pad-read-to-present timestamp probes; MQ/DL counters. | `gimerge.cpp`, `dllmain.cpp` |
| `[Debug] SkipDebugString = 1` (on) | Mod log lines go to the log file only, not also to `OutputDebugStringA` (an exception plus a server round trip under Proton). | `dllmain.cpp` |

Digest scripts: `mod/Nioh3EngineFix/tools/cbu_digest.py`, `rpf_digest.py`, `dxgidiag_report.py`; `tools/gqjt_digest.py`, `tools/hitch_from_log.py`.

### 11.1 Test-harness features (never play with these on)

| Key | Purpose |
|---|---|
| `[Debug] Tour`, `TourList`, `TourPasses`, ... | The mod loads each listed mission through the game's own mission-request queue and fast-travels to a few warp points per stage, logging fps, frame time, CPU, memory, handles and VRAM per point. **While `Tour != 0` the game does not save.** Refuses online play. `src/tour.cpp`. |
| `VirtualPad`, `VirtualPadIdleReleaseSec` | Scripted controller over a local named pipe, answering `XInputGetState` slot 0 (`1` exclusive, `2` merge with real pads); can also set mod-menu items by key. `src/vpad.cpp`. |
| `KeepRunningUnfocused = 0` | Pads keep working and no pause menu opens while the window is inactive; also opts out of Windows power throttling. |
| `SkipFx` (+ `SkipPSO`, `SkipMarker`, ...) | Draws of listed PSOs/markers are not rendered, for cost-vs-looks measurement. `src/skipfx.cpp`. |
| `LiveCtl`, `LiveCtlFile` | Re-reads a control file every 100 ms to flip live menu rows and `Tuning.*` values for one-launch A/B windows. `src/livectl.cpp`. |
| `CrashTest`, `ForceCrashTest`, `CrashTestDelaySec` | Forced-crash hooks (access violation, stack overflow, abort, fatal popup, terminate, invalid parameter) to test reporting. |

---

## 12. Nioh3CamKiProbe (`mod/Nioh3CamKiProbe`)

Despite its name this is mainly a **camera add-on**; the original probe features are diagnostics.

- **Camera override (default off).** `Nioh3CamKiProbe.ini` keys `CamDist` (valid 50..4000), `CamYawDeg` (-180..180), `CamPitchDeg`
  (-80..80), `CamFovDeg` (20..90), `CamOffsetRight`, `CamOffsetUp`. A missing key leaves the game camera alone. The file is re-read
  about every 2 s, so the EngineFix menu's Camera page (section 10) can change it live. Menus, equipment appearance and photo
  mode use the game camera; saved values return after.
- **How.** Pattern-verified hooks (a byte mismatch skips that hook and logs it): camera constructor `3B4288`, view `9FD530`,
  orbit `2B0E4C`, distance/lens blend `9565E4`, FOV `8DAD94`, and the output-eye offset at `243ED0` (acts only for the return into the
  third-person output camera). Constants are in `src/probe.cpp`; `src/gamerva.cpp` + `rvas.json` resolve RVAs by pattern on other builds.
- **Proton.** `[CamKiProbe] OnProton = 0` installs the hooks only after a player exists (patching at boot crashes the pad list on a
  handheld); `1` installs at startup; `2` never.
- **Diagnostics.** F7 takes three snapshots (stick left / right / still) and diffs them to find camera-related memory; F8 dumps the
  Ki-pulse ring after about ten R1 pulses. Read-only. Log: `Nioh3CamKiProbe.log`. Exports `Nioh3CamKiProbeVersion` and
  `Nioh3CamKiProbeActive`. Shared vertical-camera math: `mod/shared/soul_camera_vertical.h`.

---

## 13. Files the mod writes (beside the `.asi` or ini)

`Nioh3EngineFix.log`, `Nioh3EngineFix-crash-*.txt`, `Nioh3CrashReports\{pending,rejected}\`, `Nioh3Reports\`, `Nioh3EngineFix.rebarnotice`,
`Nioh3EngineFix_pj\` (PSO journal, pipeline library), `Nioh3EngineFix_pf0..3.bin` (prefetch traces), `Nioh3EngineFix_reads_*.bin/.txt`
(ReadProbe 2), `Nioh3EngineFix_shaders\` (ShaderSwap), `Nioh3EngineFix.gpuopt-trace.csv`, `Nioh3EngineFix_mlp.csv`, the DXGI diag files.
No game files are modified; everything is in-memory patching.

---

## 14. Measured effects (device and settings attached)

Only [docs/BENCHMARKS.md](../BENCHMARKS.md) is a checked source. The numbers below are quoted from it and are not general claims.
Device labels there: a Ryzen 5 7600X + RX 9070 Windows PC (1440p, render 75 %, FSR4, GPU-bound near 100 fps); a Ryzen 5 5600X +
RX 6650 XT PC whose GPU sits in a chipset slot at about 0.8 GB/s (a worst case, CPU-bound); a ROG Ally X under Bazzite/Proton at 1080p
(99 % GPU-bound).

| Feature | Result | Device |
|---|---|---|
| CBN + CBT (constant uploads) | 38.2 -> 44.3 fps, +15.9 % | slow-link 6650 XT PC, 1.7.60 |
| CBN + CBT | +0.55 % +-0.46: no change (Ally -0.16 % +-0.16) | 9070 PC |
| VRR real-time step | 0.83-0.90x game speed -> 1.00x when the update thread is below 120 ticks/s | 9070 PC, CPU-limited run |
| GF (fence sleep) | main thread about 85 % -> 32 % busy, fps unchanged | Ally |
| EP then SR (loading) | Level-136 load 96.0 s -> 47.3 s (EP), then 47.25 -> 38.45 s (SR); chained, not one measurement; not re-measured on current builds | Ally, cold first launch |
| Extra Performance "Performance" / "Potato" | +3.14 % +-0.08 and +10.5 % (PC); +3.48 % +-0.07 and +9.57 % (Ally); opt-in, changes the look | 9070 PC, Ally |
| `GpuOpt Barriers = 1` | -7.1 % over two 20 s windows (why the shipped value is "hooked, batching off") | 9070 PC |
| ReBAR on | GPU time 27.2 -> 19.1 ms, but fps only 36 -> 38 (+6 %) because the CPU became the limit | slow-link 6650 XT PC |
| FrameOverlap | +11.74 % with an artificial 8 ms replay delay (an emulation, not a native gain); about +0.4 % native | not shipped on; offline audit still fails |
| PP, B, P12, GF/MQ on Windows | within noise, or no valid result | 9070 PC |

Estimates in ini comments (for example 0.1-0.2 ms/frame for BPO, 0.15-0.25 ms/frame on the Ally for W1B) are design targets, not
results. BENCHMARKS notes that a full "stock vs shipped defaults" benchmark does not exist yet.

---

## 15. Open problems and known gaps

From source comments, the ini, the mod README's "Known limitations" and BENCHMARKS:

- **No end-to-end A/B.** Single features are measured; the shipped bundle against stock is not. Several defaults (`B`, `PP`, `MA`,
  `LK`, `BPO`, `W1B`, `RPA`, `P1E7`, `GL1`) are on with estimated, not measured, gains.
- **Untested hardware.** Never run on NVIDIA, Intel Arc, Intel CPUs, or AMD GPUs beyond the three above. Async slot 0 behaves
  differently on NVIDIA (stock enables it).
- **Online / co-op.** VRR pauses itself online; everything else was tested offline.
- **VRR limits.** The step uses the previous frame's interval, so one long hitch is followed by one long step. Script timers in the 30
  cap and anything above 120 fps are only partly verified. Menu fades are frame-counted (cosmetic).
- **Frame overlap** is built but off: the offline lifetime audit still fails and the native gain is about zero.
- **Barrier batching** measured negative on one PC; `R2b` and `R1` rewrites are not spec-exact and can corrupt output.
- **Proton specifics.** `L1OnWine` and `FGOnWine` stay off because Wine may accept high-resolution timer flags without honouring
  them. Lowering `MaxFrameLatency` under Proton only applies after the swapchain is rebuilt. Stability of async slot 0 on non-NVIDIA
  GPUs is tested on one GPU only.
- **PSO prewarm** (`PJ`, `PJH`, `PL`): proven offline; the in-game gain per device is not established. `PL` does not help the first
  start after a driver or game update and is off on Proton.
- **Opt-outs after mixed results:** `FileLinger` (off), `XHALF` (flickers, off), `HoldKeyGap` modes 1 and 2 (disproven or developer-only;
  the mode 3 fix is opt-in with a small manual sample).
- **MPCS** needs a replacement shader file that is not distributed, so it cannot be used from this repository alone.
- **Camera add-on.** The name no longer matches the function; offset and lens limits are hand-tuned; the delayed Proton install exists
  because early patching crashed the pad list; pattern lookup on other builds is untested.
- **Stale or unverified.** `release/profiles.json` appears older than the ini; the `MOD_VERSION` string in `src/dllmain.cpp` may lag
  the release label (it reads 1.8.38 in this tree); the `third_party/` libdeflate wrapper and the updater executable were not
  reviewed for this page. The public `mod/` copy lacks many files named above, so it cannot be built to match the current release.
- **No tech-debt file exists** in the source tree; this section is assembled from comments, the ini and BENCHMARKS.
