# Nioh3EngineFix

> **Public copy, developer reference.** Player install steps: [release/USER_GUIDE.md](../../release/USER_GUIDE.md).
> Performance numbers in this file come from development notes written at the time; the evidence-checked numbers are in
> [docs/BENCHMARKS.md](../../docs/BENCHMARKS.md) and [docs/final-bench/report.md](../../docs/final-bench/report.md),
> and they win wherever the two disagree. Paths such as `notes\x.md` refer to the maintainers' notes; published ones are in `research/`.

A small native mod for **Nioh 3 (Steam, v2.02)**. It fixes CPU and power waste in the engine and makes the game
run at real speed when your frame rate is below the in-game cap. It patches the game in memory only. No game
files are changed, and nothing is left behind when the game exits.

> **Status (2026-10-08): version 1.7.40, shipped as one universal zip (Windows and Proton; settings that differ by OS are `-1` = auto). It has run in the game on a Windows PC (Ryzen 5 7600X + RX 9070) and on an ASUS ROG Ally X (Bazzite + Proton). New keys since 1.7.14 (EP, SR, RC, AQ, TSY, SH, PF, LoadOnlyCpuFixes, VRROnline, MA, BPO, AABB, DQ, P1E7, GQ, JT, Tour*, VirtualPad, ...) are documented next to each key in `Nioh3EngineFix.ini`; the player-facing guide is `README.md` in the zip, the full change list is `MODLOG.md` in the project.**

## Which settings for which device

- **ROG Ally X (RC72LA, Z1 Extreme, Radeon 780M, Bazzite + Proton):** GPU-bound (about 99 %; stock about 16 fps at MAX in Futamata). The GHDM shadow shader gives +6.4 to +8.7 % fps and the fogpar fog shader a little more (+0.15 to +0.30 fps). CPU fixes (W6, GF, WK, WG, CP, SPF, LK, ED) are fps-neutral but save CPU power. Loads: about 94 s -> 37-38 s with EP + SR (automatic under Proton). Needs `WINEDLLOVERRIDES="dinput8=n,b" %command%`.
- **Desktop PC (Ryzen 5 7600X, RX 9070, 1440p, Windows 11):** CPU main-thread-bound, so the CPU fixes are the ones that matter. Stock all-off baseline about 149-153 fps standing, p99 14.3-14.7 ms; PP is kept (its "+2.9 %" is retracted), B / GF / MQ gave nothing. Both devices use the same zip and ini.

### What is new in 1.7.13 and 1.7.14 (plain language first)
- **Open the menu three ways:** F10, Esc three times, or Start three times on a controller.
- **Easier menu:** plain names with the technical id small beside them, grouped under headings, with a two-line explanation of the selected row. A "Turn frame gen ON/OFF" row sits in the Frame generation group (turning it OFF while running under Proton takes effect on the next launch).
- **Reliable frame-gen status:** it shows ON running (real -> shown fps), ON paused, or OFF, and now works on the PC too.
- **Overlay moved down** so it does not cover the health bars; the hint "Mod menu: F10 / Esc x3 / Start x3" stays while the game's own menu is open.
- **1.7.14** adds GL1, LC and LW (below): default off, untested in game. If a load gets stuck or the game crashes, set the key back to 0.
- Technical detail is in the 1.7.13 menu section and the 1.7.14 section of this file, and in `MODLOG.md`. What helped: only the GHDM shader gave a confirmed fps gain on the Ally (GPU-bound); the CPU patches here are fps-neutral there.

## What it does

| Patch | Effect | Who benefits |
|---|---|---|
| **W1** | Main-thread zero-fill loop on GPU upload memory no longer does a slow read-modify-write per element (it took 11-22 % of the main thread). | everyone |
| **W2** | Vertex quad writer does one store instead of a read-modify-write on GPU upload memory. | everyone |
| **VRR** | Variable-dt frame timing. In stock Nioh 3, the whole game slows down when you run below the FPS cap (e.g. 46 fps at a 60 cap = 77 % speed). With VRR, every frame advances by the real elapsed time, so the game runs at 1.00x speed at any frame rate inside the clamp. Works with the 30, 60 and 120 caps, with or without frame generation. | handhelds, VRR displays, anyone below their cap |
| **W3** | The window-message thread sleeps (≤1 ms, wakes instantly on input) instead of spinning a core at 26-62 %. | everyone, especially handhelds (power/thermals) |
| **R1** | The game thread waits for the frame-latency signal with a timeout instead of a busy poll. | GPU-bound / capped play (handhelds) |
| **L1** | The frame limiter sleeps on a high-resolution timer until ~1 ms before the deadline, then does its original precise spin. | capped play below what your CPU can do |
| **FG** | With in-game FSR frame generation on, its presenter thread spun on the clock between the real and the generated frame. It now sleeps until ~1 ms before, then spins the rest, so pacing is unchanged. Measured: about 0.2 of a CPU core less, no fps change. | frame-gen users, handhelds (power) |
| **P12** | On CPUs with 12 or fewer threads the engine gives two worker pools (KIDS_RESOURCE, StageUpdate) 3 fewer threads each. P12 keeps them (7 → 10 and 4 → 7 on a 6-core/12-thread CPU). It has to be written at startup, before the pools are created, which the mod does from its decrypt-wait loop. | 6-core desktops, Steam Deck, ROG Ally |

On an APU (ROG Ally / Steam Deck), every watt a spinning core burns comes out of the GPU's shared power budget.
W3, R1 and L1 hand those watts back to the GPU.

### Experimental CPU-power patches (1.1.0, all off by default)
Ports of memory patches that were built and verified offline with the live scripts (most were never applied in the
game yet). They exist so a power-bound handheld (ROG Ally: CPU + GPU share 25 W) can A/B them: every CPU watt removed
goes to the GPU clock. Each has its own `[Patches]` key (0 = not installed). An installed patch with a runtime switch
is turned on/off **live** in the mod menu (section *EXPERIMENTAL CPU POWER*); M2, J1 and S1 are code bytes only and
switch at the next launch. They are applied on **v2.02 only** (they use engine offsets), each after its sites, original
bytes and the original code it depends on (`patches.json` `deps`) are verified.

| Key | What | Thread | Source script | Live switch | Stats (log `stats/s exp:` every 30 s, menu `Exp/s` line) |
|---|---|---|---|---|---|
| **W6** | The main thread's wait for Present to return (`while (flag) Sleep(0)`, 7D9E90) becomes a short pause-spin (`W6_SpinUs`, 50 µs) then `WaitOnAddress` on the flag (`W6_TimeoutMs`, 1 ms per round). A waker at the Present thread's flag clear (1A822D9) calls `WakeByAddressAll` when someone waits, so the wake is immediate; without the waker the timeout bounds it. GPU-bound handhelds spend most of the frame in this loop (Ally: [App]Game 96-99 % busy at 33 fps). | [App]Game | notes\d3d12-submission.md | yes | waits/s, average wait µs, slept vs spun, wakes |
| **B** | Draw-list group-length search (3BC0C, type-3 path at 3BC17): galloping instead of a full binary search. +5 % main thread on PC in round 9. | [App]Game | patcher\gallop_live.fsx | yes (added: off = the original binary search) | - |
| **PP** | Streaming page-pool usage sweep (2523FC via 252677) only every `PP_Period`-th frame (4); skipped frames republish the last measurement. | [App]Game | patcher\drawprep_elim.fsx | yes | sweeps skipped/s |
| **FPC** | Per-frame cache for material `FindParam` (vtable slot 38FB9D8). Cached only on [App]Game, for string-literal names; entries expire every frame. The mod finds [App]Game by its thread name once frames advance (log `FPC: [App]Game is TID ...`). | [App]Game | patcher\drawprep_elim.fsx | yes | hit rate |
| **AABB** | OBB→AABB (6EA50) in SSE, bit-identical results (300k random boxes in the script). | game update (Scheduler0) | re\scripts_logic\logic_live.fsx | yes | - |
| **M1** | Meshlet-request workers cache the page-pool capacity sum (rdtsc window + list-head key) instead of re-walking the list per chunk (49131). | workers | re\scripts_workers\workers_live.fsx | yes | cache refreshes/s |
| **M2** | 5 `lock cmpxchg` "atomic loads" in the meshlet worker become plain loads. | workers | workers_live.fsx | next launch | - |
| **J1** | 8 `lock cmpxchg` loads in the job-queue polls (411C0/40800/403A0/3E7CC) become plain loads. | all job workers | workers_live.fsx | next launch | - |
| **X1** | The per-frame active-pad scan (435BB7) skips XInput slots the game already marked as not connected. Chains through the current IAT entry, so the mod menu's pad hook keeps working. | [App]InputUpdateWorker | re\scripts_input\input_live.fsx | yes | skipped calls/s |
| **E1** | Effects nobody can see (NoDisp) update every `E1_N`-th pass (2) with N× the time step. Visible effects untouched. | game update | re\scripts_fx\fx_live.fsx | yes | skipped/s |
| **S1** | `PAUSE` in the main thread's end-of-frame busy spin on the dispatcher (418F68, 17 bytes). | [App]Game | re\scripts_d3d12\drainspin_pause.fsx | next launch | - |

**W6 is confirmed on the ROG Ally** (FG off): main thread 98 % -> 23 % CPU, GPU clock 1064 -> 1450 MHz (the "+7-12 % fps" once quoted here is retracted; see docs/BENCHMARKS.md).
The others are still untested in-game.

Skipped: none of the requested patches. AABB's optional call counter (`count` mode of the script) was not ported (a
shared counter on a multi-threaded hot path).

### Input latency (1.2.0) - notes\input-latency.md
| Key | What | Live switch | Stats |
|---|---|---|---|
| **IK** | Late input kick: the pad poll (`InputWorkerKick` 5AB7B0) is kicked at the **start** of the logic frame, right before the engine's own pad-stage wait (5ACAAA), instead of at the end of the previous frame (5AB6CF, before the frame limiter). The sample is ~4-9 ms fresher when consumed; still one poll per logic frame, so button edges behave as stock. The switch is latched once per frame at 5ACAAA: switching IK off never skips a poll; **the frame IK is switched on polls twice** (a press in that frame can read as "held" instead of "pressed" - toggle while idle). The fresh-poll wait is capped at `IK_PadWaitCapMs` (2). Safe online (only the read time moves). | yes | `stats latency:` kicks at frame end / start, input age K->C mean/max, wait timeouts |
| **TS** | Timestamp probes, measurement only: K (pad kick), C (pad consumed), L (limiter exit 5AD2BB), W (render begin 4A52D4), B (after the waitable wait 4A535E), P (present hand-off 3D6552) into a 4096-entry ring. Every 30 s the log prints medians of K->C, C->L, L->W, W->B, B->P, C->C, P->P, K->P and **input->display = B(n+1) - K(n)** (on vkd3d the latency-1 waitable releases B when the previous frame was presented; add half a refresh for mid-screen). The menu row *TS input -> display* shows that median, updated every 2 s. TS needs the IK sites (installed with IK off when only TS = 1). | yes | as described |

**IK in 1.3.0:** the Ally run with 1.2.0 showed no effect (GPU-bound: the limiter never sleeps, so the input age is
~2 ms either way) and a pad-stage wait timeout on every frame: the 2 ms cap was spent on the logic thread each frame.
`IK_PadWaitCapMs` now defaults to **0 = do not wait at the frame start** (the game update joins the input worker later,
5AB7E0, so the fresh read is still used before it is needed). The Ally zip has IK = 0.

Ports of `patcher\latency_live.fsx` (same 6 sites, same original-byte checks, patterns from
`re\scripts_latency\derive_lat_patterns.fsx`). TSb uses the R1-independent pattern (`75 ??`): it starts on R1j's bytes,
and the pattern test checks it still matches once R1j is written. The pad-stage event and `KtglEventWaitFor` are read
from the verified bytes of 86C018.

`[Latency] MaxFrameLatency = N` (0 = the game's value) calls `IDXGISwapChain2::SetMaximumFrameLatency(N)` on every
presenting swapchain that was created with `DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT` (the game's chain has
flags 0x842). **The game never calls SetMaximumFrameLatency, so its value is the default 1** (the lowest latency); 2-3
is a throughput lever when the GPU is the limit, at up to one frame more delay. Under vkd3d-proton
SetMaximumFrameLatency only ever adds permits: raising it applies at once, **lowering it applies only after the
swapchain is rebuilt or the game restarts** (the menu says so). For an A/B set it in the ini and restart. The game's
value is logged; the setting is re-checked every 2 s, so rebuilt swapchains get it again. Live menu item *Max frame
latency*: game / 1 / 2 / 3.

`[Latency] VrrPresent` (1.7.44, default 0, Windows only; notes\vrr-present.md): the engine's Present SyncInterval
(GfxDevice+424Ch) sometimes stays 1 for a whole launch, which turns VRR's independent flip into composed flip (+~7 ms
display latency, 8-20 % presented frames never shown). 1 = the bottom-most Present hook sends SyncInterval 0 +
`DXGI_PRESENT_ALLOW_TEARING` on swapchains created with ALLOW_TEARING that are not exclusive fullscreen (the exact
call the stock game makes when it is not stuck); -1 = auto, only while the game's VSync setting is OFF. Only the two
Present arguments change: no timing, no image change; a call DXGI refuses is re-sent with the game's arguments and that
swapchain is left alone. `[Debug] PresentModeLog` (default 1, measurement only) logs every change of the arguments per
Present caller and every change of the engine's SyncInterval with its writer (PM group: watcher 13503A3 / setter
2EEBB68 caves that repeat the original store; ctor 2EDFBBD by inference). grep `PresentMode` / `VrrPresent`.

### Handle leak fix + memory line (1.5.0) - notes\memory-leaks.md
**WL** (ini `WL`, **default 1**; live menu switch): `RenderFrameBegin` 4A529C calls
`IDXGISwapChain2::GetFrameLatencyWaitableObject` (vtbl+108h at 4A5338) every frame and never closes the handle. DXGI
returns a new handle per call, so Windows leaks one handle per rendered frame (~13M after 60 h at 60 fps; the
per-process limit is 16.7M). The 6-byte call is redirected to `caveWL`, which gets the handle exactly as before and
then closes the **previous** frame's handle. That handle is dead by then: its only use is the wait loop (4A534C, the R1
site), and rbp is overwritten at 4A5461 with no read in between (checked in the dump; the test verifies those bytes).
If the runtime returns the same value again, it is kept (no duplicate to close). The site does not overlap R1
(4A5346 / 4A5357). Under Proton the swapchain has no waitable flag, the getter returns NULL and nothing happens.
`stats/s WL:` line every 30 s (handles closed).

**MemStats** (`[Profiler] MemStats`, default 0; Ally zip 1) integrates `src\memstats.cpp` (written by another agent). It
logs one `stats mem:` line every 30 s: process private bytes, working set, commit, handles, threads, VRAM local and
non-local, system RAM, and the change since start. Measurement only.

### 1.6.0 (v10): opt-in CPU items - WG, TG, DS, LK, CP, SPF
From `notes\cpu-hotspots-ally.md` (Ally SP profiles: 70 min of MAX gameplay) and `notes\worker-jobs-opt.md`. **Everything
here is off by default** (main ini). v2.02 only; every site, original byte and dependency is verified before writing.
Menu section *1.6.0 - OPT-IN*; the `Exp/s` menu line adds `WG skip` and `DS`.

| Key | What | Default (main / Ally zip) | Switch | Log line (30 s) |
|---|---|---|---|---|
| **WG** | wake gate on the two wake-one sites | 0 / **2** (installed, off) | live | `stats/s WG:` skipped, counted, stale, pending |
| **TG** | logic tick governor - **EXPERIMENT** | 0 / 0 | live (whenever VRR is installed) | `stats/s TG:` state, cap, ticks/s vs rendered/s, L1 sleeps |
| **DS** | timer sleep in the main loop's 1 ms retry spin | 0 / 0 | live (2 = installed, off) | `stats/s DS:` sleeps, average |
| **LK** | 18 locked loads -> plain loads (worker code) | 0 / 0 | next launch | - |
| **CP** | collision worker prefetch (D8180) | 0 / **2** | live | - |
| **SPF** | stage placeable prefetch (DB8BF) | 0 / **2** | live | - |

**WG - wake gate** (`[Patches] WG`, `[Tuning] WG_MaxPending` 1, `WG_WindowUs` 2000). The Ally's WakeStats showed
storms of 25-150k wake-one calls/s on pool 1 in the first minutes in a level: a submitter pushes small jobs one at a time
with a wake-one each, and every woken worker finds nothing. WG: at the wake-one sites (WS2 48C6B in
`JobWakeOneInPool` 48B9C, WS3 48D06 in `JobWakeOne` 48C84) a wake is skipped while at least `WG_MaxPending` workers of
the same pool have been woken and have not yet left their wait. The skipped target gets its "sleeping, wake me" bit
(`[WorkerThread+10h]` bit0, just cleared by the game's CAS) back and stays asleep; the caller gets its usual result.
Never gated: the pop chain 40B0A, the task wake 40D2F, wake-except 48DEC and the forward 48E3C.
- *What "on its way" means, verified against the code:* the design note's single per-pool counter (increment at
  SetEvent, decrement after the wait) is **not** exact: a worker can return from its wait on a stale signal with bit0
  still set (SetEvent on an auto-reset event while it runs), and a later counted wake on it would then count a worker
  that is already running. So WG keeps a **per-worker** state: `waiting` while the worker is inside its wait at
  1A6BC35 (the WSs cave became a C++ wrapper, `WgSleep`), and `pending` when a counted wake (WS2/WS3) targets it while
  it is `waiting` (CAS). Leaving the wait clears both atomically and, if `pending` was set, takes one unit off the
  pool's count. The count is therefore exactly "workers of this pool woken by a counted wake and still inside their
  wait". Pools are told apart by `[pool+34h] & 63` with an owner check (a second pool on the same slot is never gated);
  workers by `(slot - [pool+10h]) / 458h` (< 64, exact multiple).
- *Why a skipped wake cannot stall a job (proof):* (1) the worker that owns the pending unit returns from its wait
  (its waker's SetEvent is already issued or follows right after the count), clears the unit with a locked
  instruction and then, in the same worker loop (1A6BC48 -> 1A6BC5C), runs `JobWorkerRunUntilIdle` 3FE14, which runs
  its own queue and steals from every other worker of the pool (411C0 over all slots) until a full pass finds nothing.
  (2) The submitter pushed its job before calling the wake function, and the game's `lock cmpxchg` on the target's
  bit0 comes before WG's read of the count. With full fences on both sides (submitter: push, locked CAS, read;
  worker: locked clear, then queue loads), x86-TSO forbids "the submitter saw the unit still pending" together with
  "the worker's scan missed the job" (store-buffer litmus). (3) The wake-one functions pick the target as "highest
  index with bit0 set", so stock already relies on *any* worker of the pool finding the job; the pending worker is such
  a worker. (4) The only sleeper that sets bit0 is the worker loop at 1A6BC15 (the twin wait 3FD8C has no caller),
  and every return from it goes to RunUntilIdle. A skip therefore changes which thread runs the job and when (fan-out
  ramps through the woken worker's own pop-chain wakes), never whether it runs. The window `WG_WindowUs` only makes WG
  wake more (a pending unit older than the window does not gate). The `deps` in patches.json pin every instruction this
  relies on (slot arithmetic, the CAS before each SetEvent, the unused results, the sleep path).
- *Test on this PC:* `py -I tests\run_wg_harness.py` compiles the real `WgIndex/WgWake/WgSleep` source (extracted from
  dllmain.cpp) into a model of the pools with the v2.02 layout (8 workers, 3 submitters with bursts, nested submissions,
  stock model free of lost wake-ups) and an unbounded window: WG off / on with MaxPending 1 and 2, ~2-4 M jobs per run,
  **all complete, ~1.5-2.6 M wakes skipped, pending back to 0**. `--negative` (a gate that ignores the count) stalls
  within a second, so the oracle works. Not run in the game.
- WG installs the WS sites even with WakeStats = 0 (counters then off; WakeStats stays a separate live switch).

**TG - logic tick governor: an OPT-IN EXPERIMENT** (`[Patches] TG`, `[Tuning] TG_Floor` 30, `TG_RatioPct` 125). The
Ally runs the game logic at 44.6 ticks/s while it shows 16.2 fps, so ~2/3 of all logic ticks (collision, stage, AI,
effects, scripts) produce state nobody sees. TG makes the logic tick cap follow the rendered rate:
`cap = clamp(ceil(rendered x TG_RatioPct %) + 2, max(TG_Floor, VRR clamp minimum + 1), tier cap)`; raised at once,
lowered only after it stayed more than 2 below for 2 s; "nothing to govern" (cap would reach the tier cap) = stock.
`caveLim` (P4, the limiter's target) got 4 instructions (`tg_cap`, eax and flags only, as before); the governor runs on
the Present-hook thread after each overlay stats window (~0.5 s, rendered = the game's own frames, FG excluded); a
watchdog drops the cap within 100 ms when TG is switched off, L1 or VRR go off, or presents stall for 250 ms.
- **Why opt-in:** it is *not* "same behaviour". The simulation step becomes ~1/30 s (at the floor) instead of
  ~1/45 s - exactly what the game does in its own 30 FPS mode (tier 30, dt 1/30), so it stays inside the engine's
  designed envelope, and VRR keeps the game speed at 1.0x. But input is sampled once per tick: **a tap must be held
  ~33 ms instead of ~22 ms to be seen** (normal taps are 50-100 ms), and the newest frame packet is ~6 ms older on
  average when shown. Offline only (same caveat as VRR).
- **Needs VRR** (armed tier; otherwise stock dt) **and L1** (installed and switched on). Without L1 the limiter slack
  would be a Sleep(0)/job-steal spin on Scheduler0 instead of a sleep, so TG **refuses to engage** and logs
  `TG: refused - L1 is not active ...`; the menu row says `REFUSED: L1 off`. Under Proton that means `L1OnWine = 1`.
- It also needs the overlay's Present hook (installed whenever the FPS counter, the startup popup or the mod menu is
  on - the menu is on by default): without frame-rate data the governor never engages.
- Expected (notes 4): ticks 44.6 -> ~30 = ~-33 % of the logic-side work, ~1.0-1.4 cores on the Ally at MAX. Untested.

**DS - retry-spin sleep** (`[Patches] DS`, `[Tuning] DS_MarginUs` 150). The main loop retries the frame function after
`while ((QPC - start) / freq < 0.001f) ;` (1A74BD0..1A74C48: no pause, no readiness check). The loop's
`call [QueryPerformanceCounter]` at 1A74BDA goes to `DsQpc`: it reads the clock exactly as before and, when more than
200 us of the millisecond (minus the margin) is left, first sleeps on its own high-resolution timer (one thread only:
the first caller, [App]Game). The loop's exit test is the game's own, unchanged, so the retry never comes *earlier*;
it can come up to the timer's overshoot later (Windows ~0.05-0.5 ms). ~0 CPU today (0.36 % of the main thread); it
matters with TG. Needs L1 installed (the high-resolution timer is proven usable on this system only then).

**DQ / P1E7 - main-thread fixes, 1.7.32** (`[Patches] DQ`, `[Tuning] DQ_QuantumUs` 200, `[Patches] P1E7`; 0 / 1 / 2 =
installed but off; src\drawprep.cpp, notes\mainthread-25.md). **DQ**: the same retry spin re-checks for a new frame
after DQ_QuantumUs instead of a blind 1 ms (cave on the spin's exit test 1A74C45; the 1A744E4 quantum load runs once
before the loop). PC captures: 0.08-0.96 ms/frame of [App]Game sat in this spin. With DS on, DS only sleeps inside the
shorter quantum. **P1E7**: prefetch of the next texture's streaming object in the per-texture loop of 1E7050 (cave at
1E72C6; loads and prefetches only); target loop 0.28-0.46 ms/frame on PC, needs its own A/B. Both v2.02 only.

**LK / CP / SPF - worker jobs** (notes\worker-jobs-opt.md; ports of `patcher\lockload_live.fsx`,
`collprefetch_live.fsx`, `stageprefetch_live.fsx`, all offline-verified there with 0 mismatches).
- **LK**: 18 `lock cmpxchg [m],zero` used as loads (source and eax provably zero) become `mov` (+`test` where a ZF
  jump follows), same length: object update 1A18C9 / 1A1935 / 42877, meshlet job 4A545 / 4A55A / 4A708 / 4A719,
  130096 / 1301B4 / 1301C5 / 1301E4 / 130225, job system 40921 / 40A14 / 40CE4 / 40D75 / 41325 / 41447. eax = [m] and
  memory unchanged in both cases; only rows with a clean StoreLoad screen. The four 1301xx rows sit in a function that
  has a byte-identical twin at 12F3xx, so their patterns include the rel32 bytes (known build only anyway). Next launch.
- **CP**: cave at the collision loop head D8180 (`CollisionObjectUpdate` D80B0): prefetch colliders 3 strides ahead,
  their part/count/radius arrays 2 ahead, their first 4 parts 1 ahead. Writes only rax/rcx/flags (dead there) and
  saves rdx/r8; no stores. Measured on the real code in a harness: -16 to -27 % of the loop.
- **SPF**: cave at DB8BF in the placeable-map loop of DA830: prefetch the placeable 4 entries ahead and its +B0h
  object 2 ahead. Writes rax (its original value), rsi and flags (dead there); no stores.
- Live switches for CP / SPF (2 = installed, off). Expected: Ally ~0.1-0.2 core of worker CPU in total, no fps change.

**Skipped in 1.6.0:**
- **RL** (ResourceLoad idle Sleep(1) -> Sleep(4), 1FEE65): not built. The SP profiles never sampled it in gameplay
  (0.000 %), the notes say "only if a profile shows `<- 1FEE28` rows", and it delays new load requests by up to 3 ms
  (a behaviour change for a <= 0.02-core saving).
- **IATS** (point the ASI loader's kernel32 IAT shims back at kernel32): not built. It rewrites slots owned by a
  third-party loader (Ultimate ASI Loader, also used by Nioh3Fix); whether a loader version needs its shims after
  loading cannot be proven from the game side, and the saving after GF is 0.01-0.05 core.

### 1.7.12: MQ, PT1, DL - three offline-proved CPU patches (all default 0, v2.02 only)
From `notes\streaming-anim.md` section 4 (MQ, PT1) and `notes\collision-opt.md` (DL); ports of `patcher\mq_live.fsx`,
`pt1_live.fsx`, `collbook_live.fsx`. Every site, original byte and dependency range is verified before writing.
Same results as stock; PC-side CPU only (the Ally is GPU-bound: expect 0 fps there, a little less worker CPU / power).

| Key | What | Sites | Switch | Log (30 s `stats cpu:`) |
|---|---|---|---|---|
| **MQ** | `MatrixFromScaleQuatTrans` 88780 rebuilt in SSE, **bit-exact** (2.75 M inputs, 0 diffs incl. NaN payloads and MXCSR flags; ~1.7 % of each scheduler thread -> body -53 %) | 88780 | live (0 / 1 / 2) | calls (needs `CpuStats = 1`) |
| **PT1** | `TextureStreamRequestTiles` holds the pool SRWLOCK (shared) **once per texture** instead of once per tile touch | 114CA8, 114CEE, 129651, 12982E, 1298B3 | live (0 / 1 / 2) | textures claimed, lock pairs saved |
| **DL** | collision update skips the always-discarded first `CollisionPartMapFind` on the global-owner path | D83F4 (second site of the **CP** group) | live (0 / 1 / 2) | lookups skipped (needs `CpuStats = 1`) |

* **MQ**: the cave works on volatile xmm0-5 only and falls back to the stock body (displaced prologue, then 8878A) when
  the switch is off or the output buffer overlaps the scale input. Needs SSE4.1 (`insertps`).
* **PT1**: the gate is "owner thread AND `114C38` called from phase 2" (return address check), so the second caller and
  every other thread stay stock. **A live switch is enough**: the bytes are never restored mid-loop; switching off only
  stops new textures from claiming the lock, and the texture in progress releases it at the end of its loop (the owner
  and held lock are cleared at the single join point). Exclusive lock takers may wait up to one texture's phase 2
  (microseconds). Needs the IAT slots of `AcquireSRWLockShared` / `ReleaseSRWLockShared` to be the known ones.
* **DL**: DL is a second site of the **CP** patch group, so `DL = 1` (or `2`) installs the CP site group (CP's own switch
  stays as `CP` says; CP itself prefetches only when `CP = 1`). CS (conditional stores) is deliberately not ported.
* `CpuStats = 1` enables the MQ / DL counters (plain increments from several threads: approximate); PT1 counters are
  always kept. The `stats cpu:` line appears when any of the three is installed.
* Menu: *MQ bone matrix SSE*, *PT1 texture pool lock*, *DL skip dead lookup* in the 1.6.0 - OPT-IN section.
  Items installed with the switch off are saved as 2.

### GPU-fence waits (1.4.0) - GF
The Ally profiler (SP) found 58 % of the main thread's samples in `IsFenceComplete` 1180A4 called from `FenceWait`
7D9E1C. When the engine passes no event, that function spins `do Sleep(0); while (fence->GetCompletedValue() < value)`
until the GPU is done, which burns a core while GPU-bound (same class as W6). The worker fence poll 117D6C has the
same loop when its second argument is NULL.

**GF** (ini `GF`, main default 0, Ally zip 1; live menu switch) replaces the `xor ecx,ecx; call [Sleep]` inside both
loops (7D9E66 and 117D9F, 8 bytes each). `GfWait` registers a per-thread auto-reset event (TLS, created on first use,
never closed) with `ID3D12Fence::SetEventOnCompletion(value, event)`, the vtbl+48h call the engine makes itself when it
has an event, then sleeps on it for at most `GF_TimeoutMs` (100). The engine's own loop re-checks the fence after every
return, so the exit condition is unchanged: a timeout or a stale signal costs one more round, never a missed
completion. If `SetEventOnCompletion` fails (its HRESULT is checked), or no event can be made, that round falls back to
the stock `Sleep(0)` and is counted. Switched off, it is exactly `Sleep(0)`. The `stats/s GF:` log line (30 s) shows
waits/s, average wait (us) and fallbacks; the menu's `Exp/s` line shows `GF n x us`.

### Worker wakes (1.3.0) - notes\worker-power.md
| Key | What | Live switch | Stats |
|---|---|---|---|
| **WakeStats** | Measurement only. Counts the real `SetEvent` calls at the six worker-wake sites (pop chain 40B0A, task 40D2F, wake-one 48C6B / 48D06 / 48DEC, forward 48E3C), worker sleeps (`WaitForSingleObject` at 1A6BC35) per pool (pool+34h), and the wake checks at 40A4C split by the caller of `JobWorkerPoll` 40800 (3FE14 / 403A0 / 207200 / other; read from 40800's return address, frame verified as 8 pushes + 68h). The split rides on the WK cave, so with WakeStats = 1 the WK site is installed even when WK = 0 (its gate then stays off = stock). | yes | `stats wakes/s:` every 30 s, incl. wasted ~= sleeps - checks from 3FE14 and ED kicks/skips |
| **ED** | EffectDisp skip: the kick loop (3D6F14, call at 3D6F46) does not set the start event of a Disp worker whose list is empty, and the join loop (3D6F60, call at 3D6F8B) skips exactly those workers. Layout verified: worker i = mgr + i*1A8h (vtable 402D768, run = slot 2 = 3D83BC), start event +8, done event +0A8h, list = `[obj+1A0h]` = `std::vector` of 48-byte entries (push_back 73A784); the run function returns at once on an empty list (3D8421). The lists are filled by 15FC80 on the same thread before the kick (39E9E7 -> 39EA31). The skip is recorded at kick time per worker, and the join reads it whatever the switch says, so toggling between kick and join stays matched. 0 off, 1 on, 2 installed + off. | yes | ED kicks / skipped in the wakes line |
| WK2 | *Not shipped.* See below. | - | - |

**Why WK2 (stricter wake gate) is not in this build:** the gate needs "queued jobs" and "woken but not yet running
workers", and neither exists exactly.
1. The per-worker queues are lock-free stacks with no length; only a per-priority non-empty bitmap exists. Counting jobs
   means walking other workers' stacks while they pop and free nodes (unsafe). The bitmap is only a lower bound, so a
   50-job fan-out on one priority looks like 1 job and the gate would cut its parallelism.
2. A sleeping worker does `lock inc pool+30h` (1A6BBFC) *before* it sets bit0 (1A6BC15) and waits, with no queue
   re-check between. In the idle count a woken-not-running worker can't be told from one that is about to sleep (or a
   paused one), so "idle - bit0 set" overcounts the workers already on their way.
3. No stall would follow from a skipped pop-path wake (each worker drains its own queue before sleeping, waiters help,
   main-thread submissions use the separate wake-one paths), but the gate can't be made "wake only as many as queued".
   It would be a parallelism guess, not the proven-safe filter that was asked for. WakeStats shows whether it's worth
   building a heuristic version.

*1.6.0:* **WG** is the gate that *can* be proven: it does not count jobs or idle workers; it tracks, per worker, "woken
by a wake-one and not yet out of its wait" with its own wrapper around the worker's wait (see the 1.6.0 section).

### GPU barrier batching (1.3.0) - notes\gpu-barriers.md
`[GpuOpt]` integrates `src\gpuopt.cpp` (D3D12 barrier batching on the game's command lists, written by another agent;
see `gpuopt.h` and the notes). `Barriers`: 0 off (main ini), 1 on, 2 hooked with counters only (Ally zip). It starts
from dllmain (`gpuopt::Start`) and installs from the overlay's install thread after the Present hook (`InstallNow`,
inside SEH; a failure is logged and only gpuopt stays off). The Present hook calls `gpuopt::OnFrame()` once per frame of
the drawing swapchain. Menu: *GPU barrier batching* (live on/off when hooked) plus a `GPU:` status row. The log prints
`stats/s gpuopt:` every 30 s. Expected: ~0 on the Ally (RADV already coalesces), 2-3 % on a GPU-bound PC.

### FPS counter (1.1.0)
* **FPS** = frames the game renders and presents. **FG FPS** = frames shown including generated ones.
* With FSR frame generation behind Streamline's interposer, two swapchain objects present: the game thread to the FG
  proxy chain, the "AMD FSR Presenter Thread" to the inner chain. Presents are now counted per swapchain (all threads);
  displayed = the busiest chain, rendered = the quietest active one. Before 1.1.0 the presenter's presents were not
  counted and FG showed "off" (x1.07) while it ran.
* FG counts as active when displayed/rendered > 1.4 (off below 1.2), or when the FSR presenter thread presents and at
  least 1.15x frames reach the screen (in menus/cutscenes the presenter only passes frames through).
* `G+0x610`, the old "Real FPS" source, counts **game-update ticks** (`add [rdi+610h],1` at 233B73, end of the update
  tick 2338B0), not rendered frames. When the render side presents fewer frames than the game updates (Ally, FG off:
  54.6 ticks/s vs 35.8 presents/s), the counter now shows the presented rate as FPS and adds `(game N Hz)`. VRR
  measures its interval per tick, so its "armed" values (39-56 there) follow the tick rate and the game speed stays
  right; the frames simply aren't all presented.

### Game settings preservation
Frame generation is status-only. The mod does not write the game's config.xml, call its FG setter,
or restore settings at startup/exit. Old FG marker files are ignored. Save to ini saves only mod choices.

### 1.7.14: GL1, LC, LW - queued game-logic and loading patches (all default 0, v2.02 only, next launch, no menu rows)
From `notes\gamelogic.md` (GL1) and `notes\loading.md` section 3 rows 4-5. Same checks as every patch (pattern, bytes,
dependency hashes). None of them changes what the game computes or loads, only when memory is fetched or how soon a poll
notices finished work. The faster zlib inflate (row 6) is not built: a bit-exact drop-in would be a new decoder and is
not worth the risk for an estimated 0.5-2 s.
- **GL1** (`[Patches] GL1 = 1`): prefetch of the task 3 entries ahead in the proximity pass (E266D1) and the task-state
  pass (E26690). `prefetcht0` is only issued while entry i+3 lies inside the vector; it cannot fault. PC: about 2-3 % of one
  core (MissionThread ~20 % lower); Ally: about 0 fps (GPU-bound). The segmented object walk (20B900) is not touched.
- **LC** (`LC = 1`): resource-load in-flight cap 10 -> 16 in both managers (two immediates; the boot-flag cap of 1 is kept).
  LoadStats then logs the cap as 16.
- **LW** (`LW = 1`): the managers' `Sleep(1)` becomes `WaitForSingleObject(event, 1 ms)`; `ResourceStepJob_Run` signals the
  event after setting its done bit, and only while a manager waits. A/B with LoadStats (its three manager-Sleep hooks are
  skipped while LW is on).
A/B plan: LoadStats baseline, then one flag per load pair on the same route (LoadStats total, stage-ready time, loaders
last busy, in-flight at cap), plus `Mission` thread CPU at a fixed spot for GL1.

### 1.7.40: BPO - BoundsFromPartObbs (default 0, v2.02 only, live row "Faster part bounds (BPO)")
From `notes\bounds-partobbs.md`; port of the Iced caves in `re\scripts_logic\bounds_offline.fsx`. The game-update thread
(KIDSTaskScheduler0) builds every body part's bounds each frame in 71C60. `[Patches] BPO = 1` (2 = installed, switched off)
installs three parts, picked by `[Tuning] BPO_Mask` (default 7):
- **1 Q**: each part's rotation quaternion is rebuilt from 4 packed halves (half unpack, SinCosApprox 54360, sqrt, divide).
  It is a pure function of those 8 bytes, so it is cached in a 16384-entry seqlock table (512 KB): a hit returns exactly what
  the stock code computes; a miss runs the stock code and stores its result.
- **2 L**: the two locked reads of the object's ready word per part (71DB2, and 1A7AE2 in ObjResTreeAllReady 1A7AAC, which has 9 other
  callers) become plain loads (same value; only the StoreLoad fence is lost, as in LK / MA).
- **4 V**: the 6 Vec4CopyW0 (174610) calls in 71C60 read with 4-byte loads and write one 16-byte store (same bytes), so neither
  side fails store-to-load forwarding.
Offline: the whole 71C60 copied from the dump runs stock vs patched on 200k random calls, 4 x 65536 halves and a 4-thread
stress: 0 bitwise differences. Needs SSE4.1. Est. 0.1-0.2 ms/frame of the update thread on the PC; 0 fps on the GPU-bound Ally.

### 1.7.42: W1B / RPA - two small main-thread copies (default 0, v2.02 only, live rows "Glyph clear plain stores (W1B)",
"Range copy render params (RPA)")
From `notes\w1b-rpa.md` (whats-left #3 bundle; `notes\ally-drawprep-2.md` s.3). Both installed by drawprep.cpp, 0 / 1 / 2 as DQ.
- **W1B** (`[Patches] W1B`): the glyph padding clear 61B1C clears pad = 1 dword per glyph row in the write-combined atlas
  staging. W1 replaced the game's read-modify-write `and [m],0` with `rep stosd`, which pays a string start-up per row
  (97-100 % of 61B1C's samples sit right after it). W1B runs the game's own loop with `mov [m],0`: same stores, same
  order, same registers and flags as the original. Offline on WC memory: original 265, W1 120, W1B 25 ns per row.
- **RPA** (`[Patches] RPA`): RenderParamsAssign 247950 (operator= of the 0x3D38-byte render params, 29.6 KB of dword
  moves) copies the same 21 byte ranges with 16-byte moves. The ranges were derived from the original code; the gaps
  (padding) are never read or written. Offline 463 -> 272 ns per call with the code hot (in the game the 29.6 KB also
  evicts L1i each call).
Proof: `py -I tests\run_w1b_rpa_harness.py` runs the original bytes from the dump against the real caves (200k rows,
300k calls; all bytes, registers) - ALL PASSED. Est. Ally main thread 0.15-0.25 + 0.07-0.1 ms/frame; 0 fps while GPU-bound.

### 1.7.44: AS - the engine's async-compute slots, live (default 0, v2.02 only, live row "Async compute slots (AS)")
From `notes\gpu-async-wins.md`. KTGL runs selected compute passes on their own D3D12 COMPUTE queues ("async slots"); each
user is written twice by KT (graphics-queue path / async path with the engine's fences: the async context waits on the
graphics fence of that point, the consumer waits on the async fence right before it reads). Stock: 3 (sky clouds), 4 (fog),
6 (ocean) async; 0 (GPU effect simulation) only on NVIDIA; 11 (grass displacement) and 12 (tree wind) nowhere.
- `[Patches] AS` 0 / 1 / 2 and `[Tuning] AS_Slots` (default `0`; Ally `0 11 12`; `-3 -4 -6` keeps shipped passes synchronous while ON).
  At startup (decrypt-wait loop, before the renderer init, like AS0) slot 0 gets the AS0 byte and 11 / 12 their table
  enable byte, so the engine creates their queues. Live: the one producer call of each switchable slot (`call
  AsyncSlotGetCtx`; all 32 callers classified) goes through a cave that ANDs the slot mask: OFF = the stock slot set,
  ON = AS's set. Slot 0's deferred joins (RS+13ACh) stay stock, so a toggle mid-frame is safe.
- Log: `AS: boot: async slots requested 0 11 12 -> added 0 11 12 ...`, `AS: live switch at the producers effects(0) GVD(11)
  Tree2Wind(12) ...`, every 30 s `stats/s AS: switch ON|off, ... producer calls: effects(0) N/s ...` and `stats/s ASYNC: 6
  async compute queue(s), slots 0 3 4 6 11 12`.
- Gain only when GPU-bound (A/B with the GPU as the limit, not in a CPU-bound scene; the 1.7.16 AS0 "neutral" was CPU-bound).
Proof: `py -I tests\run_asyncslots_harness.py` (the original AsyncSlotGetCtx bytes from the dump vs the cave path, 1.2M
random scheduler states, all GPRs / xmm) and `py -I tests\run_async_overlap_harness.py` (real D3D12 device: the same
compute chain on the graphics queue vs a compute queue with the engine's fence pattern, bit-identical output, overlap gain).

## Requirements

* Nioh 3 on Steam, **version 2.02**. The mod checks the executable and **refuses to patch any other build**
  (the log says `UNKNOWN BUILD`). After a game update, wait for an updated mod.
* Windows 10/11 x64, or Linux/Steam Deck via Proton.
* An ASI/DLL loader (see below). The mod is a plain x64 DLL with no runtime dependencies (static CRT).

## Install (choose one loader)

### A) Ultimate ASI Loader (recommended; also what Lyall's Nioh3Fix uses)
1. Get Ultimate ASI Loader x64 and put its `dinput8.dll` in the game folder (the folder with `Nioh3.exe`).
   If you already use **Nioh3Fix**, this `dinput8.dll` is already there.
2. Copy `Nioh3EngineFix.asi` and `Nioh3EngineFix.ini` into the game folder (or into a `scripts\` / `plugins\`
   subfolder).

### B) Nioh Native Mod Loader (Nexus mod #45, `Nioh3ModLoader`)
1. Put its `dinput8.dll` in the game folder. Create a `mods\` folder next to `Nioh3.exe`.
2. This loader only loads files ending in **`.dll`**. Rename `Nioh3EngineFix.asi` to **`Nioh3EngineFix.dll`**.
   Put it with `Nioh3EngineFix.ini` in `mods\` (or `mods\Nioh3EngineFix\`, as in the loader's README).
   The `.ini` must keep the same base name as the DLL and sit next to it.

Both loaders are `dinput8.dll`, so use only one of them. If the mod gets loaded twice anyway, the second copy
detects the first one and does nothing.

### Steam Deck / Linux (Proton)
Add this to the game's Steam launch options so Proton loads the loader's `dinput8.dll`:

```
WINEDLLOVERRIDES="dinput8=n,b" %command%
```

Under Proton the mod detects Wine and **skips L1** by default. Wine may accept the high-resolution timer flag
without honouring it, and then the limiter would overshoot. You can try `L1OnWine = 1` and compare frame times.
Everything else is the same as on Windows.

## Online co-op: use VRR offline only
VRR changes the size of the per-frame simulation step. Other players' games don't do that, and nobody has tested
how the online sync code reacts. **For online co-op set `VRR = 0`** in the ini. The CPU/power patches
(W1, W2, W3, R1, L1) don't change game logic and can stay on.

## Settings (`Nioh3EngineFix.ini`)
* `[Patches]` turns each patch on/off: `W1 W2 VRR W3 R1 L1` (1/0). Changes apply on the next game start.
* `[VRR]`
  * `Tier30/60/120`: which in-game FPS caps get variable timing.
  * `MinXX/MaxXX`: the measured-fps clamp per cap. Below Min the game slows down again, like stock.
  * `MaxFps`: with the in-game cap at 120, raise the limiter to 144/165/240, or 0 = unlocked. This is
    **experimental**: script timers and frame pacing above 120 have not been fully verified.
  * `MenuQuantize = 1` only if menu key-repeat misbehaves. Leave it at 0 if you use Nioh3Fix.
* `[Patches]` also has the experimental keys `W6 B PP FPC AABB M1 M2 J1 X1 E1 S1` (0 = off, see above).
* `[Tuning]`: W3/R1 wait times and the L1 margin. The defaults match the tested values. Experimental:
  `PP_Period` (2/4/8/16), `E1_N` (1/2/4/8), `W6_SpinUs` (0-2000), `W6_TimeoutMs` (1-16).
* `[Patches] IK`, `TS` (input latency, 0 = off) and `[Tuning] IK_PadWaitCapMs` (0-8, default 0 = no wait).
* `[Patches] WakeStats`, `ED` (worker wakes, 0 = off; ED 2 = installed, switched off) and `[GpuOpt]` (see above).
* `[Patches] GF` (GPU-fence waits, 0 = off) and `[Tuning] GF_TimeoutMs` (1-1000, default 100).
* `[Patches] WL` (waitable-handle leak fix, default 1) and `[Profiler] MemStats` (memory line, default 0).
* 1.7.12: `[Patches] MQ`, `PT1`, `DL` (0/1/2, default 0 everywhere, live in the menu) and `CpuStats` (0/1, counters for MQ / DL).
* 1.7.11: `[Profiler] LoadStats` (0/1/2, next launch, measurement only, default 0; Ally zip 1).
* 1.6.0, all opt-in (default 0): `[Patches] WG` (0/1/2), `TG` (0/1, experiment), `DS` (0/1/2), `LK` (0/1, next
  launch), `CP` (0/1/2), `SPF` (0/1/2); `[Tuning] WG_MaxPending` (1-8, 1), `WG_WindowUs` (100-100000, 2000),
  `TG_Floor` (24-60, 30), `TG_RatioPct` (100-200, 125), `DS_MarginUs` (50-900, 150).
* 1.7.5: `[Patches] AS0` (0/1, next launch, default 0 everywhere, EXPERIMENT): keeps the engine's async effects compute queue (async slot 0, `KIDS_ASync_LLCulling_Effect`) on for non-NVIDIA GPUs (one byte, `226A497` 74 -> EB, written in the decrypt-wait loop before the renderer initialises; refused if the live slot table at `.bss 0x45C0460` is already filled). Koei Tecmo disabled it on non-NVIDIA on purpose: outcomes may be faster, slower, effects one frame late, corruption or a GPU hang. Log: `AS0: slot 0 forced on`; `stats/s ASYNC: N async compute queue(s)` every 30 s. The culling-skip (CS) idea of notes\gpu-culling-rewrite.md section 3 is NOT in 1.7.5 (the draw-count equality with the shaders' constant buffer could not be proven statically).
* 1.7.6: `[Memory] LM` load-memory sweeper (default 0 everywhere, see its section below): MEM_RESET of dead free-chunk pages in the engine heaps to cut the load-time swap burst. Self-test at start; `stats lm:` every 30 s.
* 1.7.4: `[Profiler] GPUProf = 2` (per-PSO GPU attribution), `GPUProf2_MaxStamps` (1024-65536, 8192), `GPUProf2_Top` (5-100, 40), `GPUProf2_AB` (0/1, 1), 1.7.8: `GPUProf2_SampleEvery` (1-64, 8), `GPUProf2_MinRun` (1-64, 4); measurement only, GPUProf stays 0 by default.
* 1.7.40 (diagnostics, measurement only, default 0): `[Profiler] GQ` (0/1, needs `GPUProf = 2`; graphics-queue gap census), `GQ_Top` (1-20, 5); `[Profiler] JT` (0/1, v2.02; job-timing / logic-join probe, 5 code sites written only when 1), `JT_Thread` (thread description, KIDSTaskScheduler0), `JT_Top` (1-20, 6), `JT_ReportSec` (5-600, = ReportSec). Log format and the decisions they feed: notes\gq-jt-probes.md.
* 1.7.2: `[Patches] RWb` (0/1, next launch), `SPR` (0/1, measurement), `[Profiler] TileStats` (0/1/2), `[GpuOpt] TileBatch` (0/1/2); all default 0.
* `[Latency] MaxFrameLatency`: 0 = the game's value, 1-16 frames (see Input latency).
* 1.7.44: `[Latency] VrrPresent` (-1 auto / 0 off / 1 on, default 0, next launch, Windows only) and `[Debug] PresentModeLog` (0/1/2, default 1, measurement only; 2 = 30 s summary); see Input latency.
* 1.7.56: `[General] ReBarNotice` (1 default / 0): one Resizable BAR / SAM check ~40 s after the game first renders (log `RB:`);
  when a desktop GPU's UPLOAD heaps sit in system memory (no ReBAR), a 25 s overlay notice is shown ONCE (marker file
  `Nioh3EngineFix.rebarnotice` next to the ASI; delete it to see it again). Never on UMA / handhelds, under Proton, with
  `VSForceSysUpload` A/B runs or (toast only) in Tour / VirtualPad sessions. Measured reason: RX 6650 XT ReBAR off -> on =
  32.5 -> 24.0 ms GPU per frame (notes\user-settings.md, notes\rebar-fix-v3.md).
* 1.7.57 (EXPERIMENT, default 0, next launch, v2.02 only): `[GpuOpt] SceneBufMirror` (0 off / 1 auto = only without
  Resizable BAR, never UMA / Proton / 2 force) copies the meshlet scene buffers the CPU writes every frame (instances, LOD root
  lists, offsets) from UPLOAD memory into VRAM once per frame on its own COPY queue and points the game's views of them there;
  `SceneBufMirrorQueue` (1 own COPY queue / 0 the game's DIRECT queue); `[Profiler] SceneBufProbe` (0/1, census of those buffers
  + late-write check). Log `SBM`. notes\rebar-fix-v3.md. Turning ReBAR on in the BIOS is the real fix.
* `[General] AllowUnknownBuild = 1` makes the mod try pattern matching on other builds. Each site is still
  verified, and anything unexpected is skipped. Not recommended.

## Load-memory sweeper (`[Memory] LM`, 1.7.6, v2.02 only, default 0)

Nioh 3 commits a 6 GiB arena at boot and never decommits it: when a stage is freed its chunks go back on the engine heap's bins but their pages stay committed and dirty, and on the Ally (RAM = VRAM pool, zram swap) the next stage pushes those dead pages to swap (class A load hitches, `notes\load-memory.md`). LM starts a sweeper thread (`src\loadmem.cpp`) that every `LM_PeriodMs` (1000) takes each engine heap's own mutex (the game's `_Mtx_lock`, heap+60h), walks the free bins and calls `VirtualAlloc(MEM_RESET)` on the page-aligned interior of free chunks of >= `LM_MinKB` (256) whose address and size were identical in the previous sweep (free and unchanged for >= 2 sweeps). The 0x20-byte chunk header and the next chunk's header are never touched, a chunk that changed is skipped until it is stable again, nothing is reset twice, and the lock is held at most `LM_BudgetUs` (1000 us) per bin walk and `LM_SliceMB` (128) per reset call. No game bytes are patched; contents of free memory become undefined (the allocator reads only free-chunk headers, proof + model: `re\scripts_stutter\loadmem_check.py`).

* Self-test at start: 64 MB scratch is touched and reset; if the process working set does not fall by >= 50 % within 1 s, LM turns itself off (`LM: self-test FAILED`). `LM_Force = 1` skips the test (use it if the log says the working set did not fall but swap still drops; on Windows a reset does not shrink the working set at once, so the test fails on the PC).
* Log: `LM: on ...`, `LM: self-test PASSED|FAILED`, `LM: N engine heaps found`, and every 30 s `stats lm:` (heaps, sweeps, chunks / MB reset with deltas, lock waits > 1 ms and max wait, max lock hold, budget cuts, bad nodes, reset fails, self-test result).
* Keys (next launch): `LM`, `LM_Force`, `LM_PeriodMs`, `LM_MinKB`, `LM_BudgetUs`, `LM_SliceMB`. ### Cheap GP2 (1.7.8)
Measured problem (Ally, log 175-L1): ~2540 draws and ~1400 timestamps per frame, hook CPU 2.3 ms/frame (~450 ns/call, 5000 hooked calls: global lock, list lookup, PSO map), but wall frame 30 ms against 16.7 ms OFF (the OFF windows were taken in the lobby, so the delta was inflated by the scene; still, the game is 99 % GPU-bound, so a 2.3 ms CPU cost on the recording threads cannot explain +10 ms). Conclusion: the cost is GPU-side. Each `EndQuery(TIMESTAMP)` on vkd3d-proton / RADV is a bottom-of-pipe write that makes the GPU drain between draws (~5-8 us each), so cost scales with stamps, not draws. Design, in order of effect:
1. `GPUProf2_SampleEvery` (default 8): stamps only in frame f when `f % N == 0`. In the other frames `OnFrame` clears the SetPSO / Draw / Dispatch / Copy / Clear slots of `gpuopt_hook`, so those calls never leave the asm thunk (no CPU hook cost either), and marker begin/end take no query. Lists that straddle a switch forget their open interval. Per-frame normalisation counts only frames that were stamped.
2. `GPUProf2_MinRun` (default 4): coarse stamps. Stamps are still only at PSO-run boundaries, never per draw; now a boundary is cut only when the interval already holds >= K draws, so 1-3 draw runs are merged into the neighbouring run and the interval is billed to its longest run (copies and clears count as K and always cut). Cuts ~1400 -> ~600 stamps/sampled frame at K = 4 (~340 at K = 8).
3. Amortised cost: ~(600 / 8) = ~75 stamps/frame instead of 1400. The overhead line reports it live: `sampled X ms vs unsampled Y ms`, so the A/B is in the same scene. Set `SampleEvery = 1, MinRun = 1` for the old exact behaviour.
Output format is unchanged (the frame line gains a ` | sampled N frames (1 in S), min run K draws` suffix); `gp2_digest.py` / `gp2_ab.py` read it as before and each sampled frame is one window frame. Caveat: with K > 1 a PSO whose runs are shorter than K draws shows up under its bigger neighbour (the total covered time is unchanged); use `MinRun = 1` with `SampleEvery` 8 to keep per-PSO detail at a sampled cost. Not measured on the Ally yet.

Test: `tests
un_loadmem_harness.py` (synthetic heap in real memory: stability rule, page-interior bounds, lock held around every reset, real MEM_RESET leaves headers and live data intact).
* Ally A/B: MangoHud (swap_used, ram, gpu_vram, frametime) + `MemStats = 1` over 3 loads with `LM = 0`, then the same route with `LM = 1`; compare the swap delta per load, the 40-134 ms frames in the first 12 s, load time and the working-set drop after the stage frees. Revert: `LM = 0`.

## Shader dump / swap (`[ShaderSwap]`, 1.7.1)

Hooks D3D12 pipeline-state creation (before the engine creates its device) and logs every shader hash once
(`psoswap:` lines; for exe-embedded shaders `exe <stage> <key> <size> rva=...`). Optional: `DumpMode = 1` writes the
exe-embedded shader blobs, `DumpMode = 2` writes every shader the game builds, to `<Folder>\dump\<digest>.dxbc|.dxil`.
If a file `<Folder>\<digest>.dxbc|.dxil` exists, that bytecode replaces the game's for the matching shader (a runtime
that rejects it falls back to the game's own shader). No replacement files ship; this build observes and dumps.

| Key | Default | Meaning |
|---|---|---|
| `Enabled` | 0 (main ini) | 1 = hook PSO creation. 0 = nothing is installed |
| `Folder` | `Nioh3EngineFix_shaders` | relative to the ini's folder (the game folder) |
| `DumpMode` | 0 | 0 off, 1 exe-embedded originals, 2 every shader |
| `LogAll` | 0 | 1 = log non-exe shaders too (long at boot) |

Design and proofs: `notes\gpu-post-exe.md`. There is no live switch: PSOs are built at boot, so restart the game.

## GPU profiler, per-PSO mode (`GPUProf = 2`, 1.7.4)
Why: with `GPUProf = 1` the engine's own debug markers cover only about 25 % of the Ally's GPU time (14.5 ms of a 59 ms frame, GPU 99 % busy). `GPUProf = 2` attributes the rest.

How: gpuopt's command-list hooks also take `SetPipelineState`, `DrawInstanced`, `DrawIndexedInstanced`, `Dispatch`, `ExecuteIndirect`, `DispatchMesh`, the `Copy*` / `ResolveSubresource*` calls and `Clear*View`. Per command list the mod tracks the bound PSO and a current *bucket* (a PSO, or "copy / resolve / clear"). A GPU timestamp is written (1) at a list's first hooked call, (2) when the work changes bucket (the next draw or dispatch after a `SetPipelineState`, or a copy / clear), (3) at every marker begin / end, (4) just before `Close`. The GPU time between two timestamps of one list is billed to the bucket that was current, together with the enclosing marker name. PSO pointers are mapped to shader hashes by psoswap's creation hooks (`psoswap::PsoSetId`); the key is the set of container digests of all stages (`vs=... ps=...`, the same 32-hex names psoswap uses for replacement files). `GPUProf = 2` turns that tracking on by itself; with `[ShaderSwap] Enabled = 0` nothing is swapped, dumped or logged. PSOs created before the hook, or not through the device hooks, show as `(PSO not created through the hooks @xxxxxxx)`.

Readback: the same 8-slot ring as `GPUProf = 1`. The dispatcher records behind the Present thread and the GPU queues frames, so a slot is read on the Present thread 6 frames after it was current (never a wait). The PSO stamps have their own per-frame cap (`GPUProf2_MaxStamps`, default 8192) and a larger query heap (8 x (2 x MaxMarkers + MaxStamps + 16384) queries, about 1.8 MB of readback at the defaults). Failure is safe: an exception or a failed heap switches GP off and the hooks pass through.

Log, every 30 s:
* `GP2: frame X ms GPU span (wall W ms); covered Y ms (n % of span); async-compute Z ms; ... intervals/frame, ... draws+dispatches/frame, PSO stamps ON | dropped: cap, rec, unresolved, heap full`. Span = first to last DIRECT-queue timestamp of the frame; covered = sum of the billed DIRECT-queue intervals. What stays uncovered is the time between command lists (switches, fences) and the work before a list's first hooked call.
* `GP2: top 40 of N buckets`, then rows `rank desc | ms/frame | draws+dispatches/frame | intervals/frame | [marker]`. desc is `vs=<hash> ps=<hash>`, `cs=<hash>`, `(copy / resolve / clear)` or `(no PSO bound yet)`; async-compute lists are tagged and excluded from "covered".
* `GP2: overhead: ...` (1.7.8, `GPUProf2_SampleEvery` > 1): wall-clock frame time of sampled vs unsampled frames, measured live in the same scene (no more lobby-vs-match A/B windows), the cost of a sampled frame, the same averaged over all frames, stamps per sampled frame, and hook CPU per sampled frame. With `SampleEvery = 1` the old line applies: the 2nd and 4th 30 s windows run with the PSO stamps off (`GPUProf2_AB = 1`) and the wall-clock frame time ON vs OFF is compared (scene changes between windows add noise).

Accuracy caveat: `EndQuery(TIMESTAMP)` between draws on one queue measures *GPU-pipelined intervals*, i.e. the time from when everything before the previous stamp finished to when everything before this stamp finished. Draws of consecutive PSOs overlap on the GPU, so a PSO's number is an approximate cost (it includes the tail of the previous work that had not drained and any barrier wait recorded before the stamp), not an isolated one. The sums are exact; the per-PSO split is good for ranking, not for absolute savings. A marker boundary also splits an interval, so one PSO can appear under two markers.

Test: `tests\run_gp_harness.py` runs mode 1, mode 2 and mode 3 (GP2 sampled 1 in 4, min run 2) on a real D3D12 device (two compute PSOs of different weight, hash names, dispatch counts, clears in the copy bucket, covered <= span).

## Tile-mapping census / merge (`TileStats`, `TileBatch`, 1.7.2)

`src\tilestats.cpp` hooks the D3D12 command-queue vtable slots UpdateTileMappings, CopyTileMappings, Signal and Wait (and
ExecuteCommandLists through the overlay's hook, which calls `tilestats::OnEcl` first). `[Profiler] TileStats` (0 off, 1 on,
2 = also a one-shot 256-call trace) logs one `stats tiles:` line every 30 s: calls, regions, ranges and tiles per frame by
queue type, CPU us inside the call, and ECL / Signal / Wait per frame. `[GpuOpt] TileBatch` (0 off, 1 merge, 2 count only)
merges consecutive game UpdateTileMappings on one queue when the final mapping is provably identical; the merged call is
issued before any other operation on that queue and at every Present. Needs the overlay's Present hook (no hooks without
it). Main ini 0/0; Ally zip TileStats = 2, TileBatch = 2 (count only, nothing changes). Harness:
`py -I tests\run_tilestats_harness.py` (real D3D12 device). Background: `notes\gpu-tiles.md`.

## Stutter: RWb and SPR (`[Patches]`, 1.7.2, v2.02 only)

* **RWb** (default 0, code bytes: next launch). `ResStateWaitSwap` (behind `ResSetWithStateWait`) woke a ResourceLoad worker
  (JobWakeOneInPool, pool 1) on every iteration of its cold spin and called Sleep(0) only every 256th. Nothing is pushed,
  so each woken worker found empty queues and slept again: the 25-150k wakes/s storm seen on the Ally for ~4 minutes around
  a level load. RWb is a 22-byte patch at 1A3146A that moves the wake behind the Sleep(0) wrap test: one wake per 256
  spins. Same exit condition, same flush request, same Sleep(0) cadence in spin count; any job pushed to pool 1 brings its
  own wake. Proven offline by `patcher\reswait_live.fsx`; `tests\test_patterns.py` verifies the site. Read the effect from
  `stats/s WG` / `stats wakes/s` and the fps in the settle window after a load. Undo: `RWb = 0`.
* **SPR** (default 0 in the main ini, 1 in the Ally zip; measurement only). Two timing wrappers (`caveSprG`, `caveSprU`)
  around `CreateGfxPso` 9290D8 (a runtime graphics PSO create on the dispatcher thread, called from `PsoGetOrCreateGfx`
  119670) and `CreateDedicatedUpload` 9C2EE0 (the upload-heap ring full or oversized request, `UploadHeapSuballoc` 62D688).
  They call the original with the original arguments and return its result. One `stats spr:` line every 30 s: count (total
  and per 30 s), average and max us, and calls >= 1 ms. Tells whether the PSO-cache or upload-ring fixes in
  `notes\stutter.md` are worth building. Undo: `SPR = 0`.

## Load times: LoadStats (`[Profiler] LoadStats`, 1.7.11, v2.02 only)

Measurement only, default 0 (Ally zip 1). Hooks 25 call sites (resource-manager `Sleep(1)` polls, the zlib inflate
wrappers, PSO creation, `ReadFile`) with one interlocked write each, and polls the engine's loading stop flag (58A).
For every loading window the log gets one summary: seconds, frames, process CPU, stage-ready time, loader occupancy
(in-flight vs the cap of 10), load-end ready wait (timeout or not), event-scene seconds, MB read, inflate CPU time,
PSOs created, plus the same split at "stage ready". `LoadStats = 2` adds one line per second inside the window.
Sites another patch already owns are skipped (log "N skipped"). Next launch. `grep LoadStats` of the log is the whole
result. Background: `notes\loading.md`. Checks: `tests
un_loadstats_harness.py`, `re\scripts_load\ls_sites.py`.

## In-game mod menu
Change the fixes and the overlay while you play, with no restart. The menu opens at any time: in gameplay, in the
game's pause menu, or on the title screen.

| | Keyboard | Controller (ROG Ally / Steam Deck / Xbox pad) |
|---|---|---|
| open / close | **F10**, or press **Esc three times** (1.7.13) | **triple-tap Start (Menu)** within 0.9 s (`Gesture = Hold` = old 5 s hold, with a countdown) |
| move | Up / Down | D-pad or left stick up / down (hold to repeat) |
| change a value | Left / Right | D-pad left / right |
| toggle / run an action | Enter | A |
| close | Esc, Backspace, F10 | B, or triple-tap Start again |

* A short Start press still goes to the game, so its pause menu opens as usual. While the mod menu is open the game
  sees a neutral controller and no arrow, Enter, Esc or Backspace keys. This lasts until you let go of the buttons
  after closing, so B doesn't also back out of the game's menu. The game never sees F10.
* While **any game menu is open** (pause, options, title, ...), a reminder shows top-right: `Mod menu: F10 / Esc x3 / Start x3` (the whole time, never in gameplay; `Gesture = Hold` shows the hold text). 1.7.13 reads the game's own "a menu is open" signal (the two getters its frame generation uses to pause itself, `0x5A37D4` / `0xD63C0`, bytes checked at start-up). The old engine stop flags (G+58B / 58C) only change in the pause menu and at loads, which is why the reminder never showed on the PC before. `[Menu] PauseHintSource = Menu | StopFlags | Both` (default Both) picks the signal; the log prints `MENU: game menu signal now OPEN/closed`.
* **Escape x3** (`[Menu] KeyTripleTap = Escape`, `None` = off): same logic as the controller's triple-tap (`TapWindowMs`, `TapGapMs`, `SwallowTaps`). The game reads Escape through DirectInput `GetDeviceState`, which the mod already hooks; the first presses are hidden from the game, a lone Escape (or two) is replayed to it after `TapGapMs` as one short press, and an Escape that is still held after the gap passes at once. So the game's own Esc menu opens ~0.3 s later than without the gesture; set `KeyTripleTap = None` to get the old timing. In the fallback mode (DirectInput not hooked) the presses cannot be hidden. With the mod menu open, a single Esc closes it.
* All of this is configurable in `[Menu]`: key, hold button or combo (`Back+Start`, `L3+R3`, ...), hold time, text
  size, the reminder, and `FrameGenToggle` (see below).

What it shows (1.7.13 layout): every row has a **plain name** with the technical id small and dim after it (`CPU waits sleep instead of spinning  (W6)`), grouped under the headers **Display & overlay**, **Frame generation**, **Performance fixes (safe)**, **Experimental** (CPU / GPU Proton), **Diagnostics / logging**, **Next launch** and **Save & exit**. The bottom of the panel explains the selected row in two lines: what it does, then whether it changes the picture or only timing, whether it applies now or at the next launch, and the measured effect where known (for example `CPU only: no fps change on the Ally`, `About 0 on the Ally, 2-3 % on PC`). The list scrolls when it does not fit (`rows a-b of N` in the title).
* **Game's own performance display:** Nioh 3 has no retail setting for it. The KTGL fps overlay (`GfxDevice+0x76F0`) is compiled out (`notes\osd-design.md` section 1A), so the menu row says `not in this game build`; the mod's own counter is the one to use.

Details by group (older description):
* **Live fixes, applied at once:** VRR timing, W3 message-pump wait (and its Proton sleep mode), R1 render-wait
  timeout (0 = stock busy poll), L1 frame-limiter sleep, F1 FSR frame-gen presenter sleep, WK job wake gate, Max frame
  latency (game / 1 / 2 / 3; under Proton lowering applies after a swapchain rebuild / restart).
* **Input latency:** IK input kick early (live), TS input -> display (live; shows the median in ms).
* **Experimental CPU power:** W6, B, PP, FPC, AABB, M1, X1, E1 (live when installed; when not installed, toggling sets the
  ini key for the next launch, `* restart`), M2, J1, S1 (next launch). An `Exp/s` stats line shows W6 waits and
  average wait, PP skips, FPC hit rate, M1 refreshes, X1 and E1 skips.
  A fix that isn't installed this session shows `not installed` or `off (Proton)`, with the reason.
* **1.6.0 - OPT-IN:** WG, TG (shows `ticks <= N` while it governs, `REFUSED: L1 off` without L1), DS, CP, SPF, MQ, PT1, DL live;
  LK next launch. Items installed with the switch off are saved as 2 (stay installed).
* **Overlay:** Show FPS counter, frame time, game speed, *Move FPS counter* (corner), *Nudge counter right / down*, text size. `[Overlay] OffsetX / OffsetY` are pixels at 1080p (scaled with the screen height) measured from the chosen corner's edges; with no offset keys the TopLeft counter starts **160 px down** so it sits below the game's health / ki bars (other corners 0). The first nudge in the menu takes over the current values; A resets a value. Save to ini writes the keys. With frame generation running the counter shows `FPS: 60 FG | FG FPS: 120`.
* **Frame generation:** a status row `Frame gen: ON - running (60 -> 120 fps)` (green) / `ON - paused (game menu open | loading | menu/loading/cutscene)` (orange) / `OFF` (grey), the current FG type (read-only), and the game's *Upscaler* setting. See
  [Frame generation](#frame-generation-status-and-switch).
* **Next launch:** WorkerCap, P12, and under Proton also L1/F1 on Proton. These are marked `* restart` when changed.
* **Save to ini** writes everything to `Nioh3EngineFix.ini`. Without saving, changes last until the game exits.
* A description of the selected item, and a live stats line: W3 waits/s, L1 and F1 timer sleeps/s, WK wakes kept
  and skipped per second.

Notes:
* Turning a fix off in the menu restores the stock behaviour of that code path. The patch bytes stay in place, but
  the switch they check is off.
* The game's own XInput calls are used: the mod hooks the game's `XInputGetState` import and chains to the original,
  so the Steam overlay keeps working. The keyboard is read through the game's DirectInput reads. If either hook can't
  be installed, the menu polls the device itself and the log says that the game will also see those presses.
* Hiding F10 from Windows' F10 menu mode needs W3 (on by default). With `W3 = 0`, use the controller or another `Key`.
* Sony pads that the game reads over HID (DualShock/DualSense without Steam Input) are not seen by the menu. Under
  Steam Input (Steam Deck / Ally Game Mode) every pad is an XInput pad, so they work.
* If the overlay stops drawing, the menu closes by itself within 1.5 s and the game's input passes through again.
  Any error disables only the menu and is logged (`MENU: ...` lines).

### Controller display (`[Menu] ControllerDisplay`)
A live picture of the controller, top-left under the FPS counter, drawn the way the Gamepad Viewer skins (gamepadviewer.com)
draw it: the controller body, then for every pressed button its lit sprite, the stick caps moved (and tilted) by the axes,
the triggers lit by how far they are pulled, and the touchpad / PS button when pressed. `ButtonStyle = PlayStation` shows a
DualSense, `ButtonStyle = Xbox` an Xbox pad. `ControllerDisplaySize` (1 = about 280 px wide at 1080p) and
`ControllerDisplayOpacity` change it live; the menu rows are under "Mod menu controls". The picture shows the raw pad
before the menu masks anything. XInput has no touchpad and no PS/guide bit, so those two layers stay dark today.
The sprites are embedded in the `.asi` (see Credits).

### Frame generation status and switch
The startup popup and the menu show:
* the game's **FRAME_GENERATION** setting (OFF, DLSS, FSR or XeSS) and its **upscaler** setting. FSR frame gen doesn't
  need the FSR upscaler; it also runs with DLSS, XeSS or no upscaler.
* whether FG **really runs** (1.7.13, counted per thread): the game/render thread's presents are the real fps, the `AMD FSR Presenter Thread`'s presents are the displayed fps (on the PC the presenter presents into the same swapchain as the game, so the old per-swapchain count saw one chain and `x1.00`). FG runs when displayed > 1.15x real. The log prints `FG state: ...` lines with the reason on every change, and the stats line shows `threads game N/s presenter M/s`.
* a **reason line** when the setting is on but FG doesn't run:
  * `switching - the game is rebuilding its swapchain`: right after a change, for a few seconds.
  * `FSR presenter runs but adds no frames`: the engine pauses FG in menus, cutscenes, the title screen and
    character creation.
  * `no FSR presenter thread: the FG swapchain was not created`: the setting is on, but the game never built its FG
    swapchain. Under Proton this is the known vkd3d-proton problem.
* "Frame generation: off in game settings" means exactly that: nothing blocks it, it is simply off.

The log prints `OSD: game settings: FRAME_GENERATION ..., RESOLUTION_SCALING ...` at the first frame and on every
change. It also prints which thread presents: `"AMD FSR Presenter Thread"` means FSR FG is running.

The **Frame generation** item shows the game's current setting and runtime status. Change it in the
game's Options. **Save to ini** saves only mod choices to `Nioh3EngineFix.ini`.
`[Menu] FrameGenToggle` defaults to `0`; all legacy values are status-only on Windows and Proton.
Legacy `3` retains the independent FGC copy-job null guard under Proton, without settings writes or FG pause control.
Old `.fgmarker` files and game-config backups are ignored; startup and exit never restore older settings.

Recommended in-game settings with VRR:
* Pick the FPS cap (30/60/120) you can *mostly* hold.
* **Required: set "FPS dynamic adjustment" to OFF.** With VRR the game sees your real frame rate, and Dynamic
  FPS reacts to anything below the cap by dropping to the 60 fps mode. Measured: 72.7 fps with it on vs 102.7
  off. VRR already removes the judder Dynamic FPS exists to hide. (The live scripts force it off automatically.)
* Frame generation is fine. VRR acts on the base frame rate.

## Is it working?
Look at `Nioh3EngineFix.log` next to the mod. A good run looks like this:

```
Nioh3.exe at ...: TimeDateStamp 6AA7C106 SizeOfImage 52FB000 -> known build (v2.02)
code is decrypted (...)
resolving sites:   ... OK at RVA ...
W1: applied ... VRR: applied (20 site(s)) ... L1: applied
done: N group(s) applied, 0 skipped.
```

Any line that says `skipped` names the patch and the reason. The mod never patches a site whose bytes it doesn't
recognise, so a skipped patch leaves that part of the game exactly stock.

To uninstall, delete the `.asi`/`.dll`, `.ini` and `.log`. Nothing else was changed.

## Compatibility
* **Nioh3Fix (Lyall) 0.0.6**: none of its hook sites overlap ours. Its Dynamic-FPS hook sits 0x1C bytes before
  one of our VRR sites, and its menu-input hook is at our optional `MenuQuantize` site (so keep that off).
* **OptiScaler / ReShade**: unrelated (graphics API layer).
* The Steam DRM wrapper (SteamStub) is **not** touched. The mod only waits until it has decrypted the game code.

## Known limitations
* The VRR step uses the previous frame's interval, like most engines. A single long hitch frame is followed by
  one long step.
* Per-frame script timers in the 30 cap, and anything above 120 fps, are only partly verified.
* Menu fades are frame-counted and take a little longer below the cap (cosmetic).

## Credits
* Controller display, PlayStation style: the **Gamepad Viewer DualSense skin** by **CrazyGrape** (vector art, 2021), with
  touchpad and PS button support by **Cubio300** (https://gist.github.com/Cubio300/e9ff30dff9a96081086f534b22bf05fd).
  Licensed under Creative Commons Attribution 3.0 United States (https://creativecommons.org/licenses/by/3.0/us/).
  The sprites are packed into one atlas (cropped, not otherwise changed). The DualSense design and the PlayStation logo are
  the intellectual property of Sony.
* Controller display, Xbox style: the **"xbox_star" skin of MyGamepadViewer** by **skyqyzm**
  (https://github.com/skyjty/MyGamepadViewer), MIT License, Copyright (c) 2024 skyqyzm (full text in
  `assets/padviz/xbox/LICENSE`). Rasterised from its SVG layers. The Xbox design and logo are Microsoft's.
  MIT licence text of the Xbox art (it ships inside the `.asi`):

  ```
  MIT License

  Copyright (c) 2024 skyqyzm

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in all
  copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
  SOFTWARE.
  ```
* The layer behaviour (lit sprites, stick travel, trigger opacity) follows gamepadviewer.com and Marcin Wichary's
  Gamepad tester (Apache 2.0). Details and sources: `assets/padviz/CREDITS.txt`.

---

## For developers

```
src/dllmain.cpp       loader thread, config, AOB scan, verification, near page + trampolines, frozen writes,
                      W3 hookPeek and L1Wait (C++)
src/osd.cpp / osd.h   in-game overlay (D3D12, drawn at the bottom-most Present): FPS counter, startup popup
src/menu.cpp / menu.h in-game mod menu (input hooks, items, save to ini, pause reminder); drawn through osd
src/caves.asm         VRR caves (caveFps/Scale/Lim/Tier/Menu) and the caveL1 entry (ml64), port of the Iced caves
src/patches_gen.h     GENERATED from patches.json
patches.json          single source of truth: every site's pattern, offset, original bytes, kind, replacement
tools/derive_patterns.fsx  derives the patterns from the decrypted dump (Iced; rel32/RIP-disp wildcarded)
tools/gen_header.py   patches.json -> src/patches_gen.h   (--check verifies sync)
tests/test_patterns.py  verifies every pattern against the decrypted v2.02 dump (stdlib only)
tests/run_wg_harness.py + wg_harness.cpp  WG stress test: the real WgWake/WgSleep source in a model of the job pools
build.bat / CMakeLists.txt / .github/workflows/build.yml
```

* **Build:** run `build.bat` (MSVC: cl + ml64 + link, `/O2 /MT`), or use CMake with the MSVC generator. The output
  is `bin\Nioh3EngineFix.asi`.
* **Test:** run `py -I tests\test_patterns.py <path\to\Nioh3.dump.exe>`. Each pattern must match exactly once in
  the executable sections and land on the expected RVA with the expected original bytes. The test also checks
  values the mod reads from those bytes: the PeekMessageA IAT slot, the limiter callee, GetDelta60, and GetFps.
  It also checks what the mod menu relies on: the XInputGetState IAT slot, the engine's stop-flag code, and the
  frame-generation pieces (the game's FG setter and its gate, the settings-changed flag, the FG/upscaler config
  descriptors, the config.xml path string).
* **Site kinds** (`patches.json`): `bytes` (fixed replacement), `call` (E8 to a near trampoline, nop-padded),
  `ripload` (`opcode` + rel32 to a near data slot), `jmp` (E9 to a near trampoline - the cave returns by itself or
  jumps back to an absolute address), `vptr` (an aligned 8-byte pointer in .rdata re-pointed at a cave; `vtarget` = the
  original target RVA, no code pattern). Optional `callee` (expected E8 target) and `iat_disp_at` / `iat_name`
  (FF 15 slot and the import it must be) are verified by the test. `deps` lists original code a group relies on beyond
  its site (exact bytes, or an FNV-1a hash for long ranges); the mod checks them before writing that group and the
  test checks them against the dump. The experimental sites were derived and swept for branch targets into the patched
  bytes offline (none), and the MASM caves were compared instruction by instruction with the scripts' Iced caves.
* **How sites are patched:**
  1. Every enabled site is located by pattern and verified: unique match, original bytes, and the expected RVA on
     the known build.
  2. A 4 KB page is allocated within ±2 GB of `Nioh3.exe`. It holds the data the game reads RIP-relatively
     (`ticks120`, `ticks60`, `r1Timeout`) and one `jmp [rip]` trampoline per cave.
  3. Each group is written while every other thread is suspended. A group is only written when no thread is
     stopped strictly inside one of its sites.
* VRR is all-or-nothing: if one of its 20 sites is missing, none of them is written, because a partial install
  would run the game at the wrong speed. R1's two sites are also written together.
* Differences from the scripts:
  * W3 and L1's timer logic are C++ instead of hand-assembled bytes.
  * The timer is created up front. If that fails, L1 is skipped.
  * The optional P7 cave now calls GetDelta60 with proper Win64 stack alignment and home space. The script's
    version lacked both, but it was never applied.
  * Before the first frame, the tier defaults to 60. The scripts read it from the live game. P1 overwrites it on
    the first frame either way.

### Ally barrier trace and the R2a / R2b / R1 barrier rewrites (1.7.9 / 1.7.10)
Background: notes\vkd3d-audit.md (Addendum B has the designs, the 45 engine callers and the safety conditions).
vkd3d-proton maps a `COMMON` before-state to ALL_COMMANDS + MEMORY_READ/WRITE, which on RADV is a full wait-for-idle. The engine
resets the tracked state of every buffer to COMMON at the start of each command list (list stamp), which is the true D3D12 state
there, so each list starts with `COMMON -> X` barriers. 1.7.10 can replace that before-state; all three switches are **off by default**.

**Trace** (`TraceRecords > 0`). Every barrier row of `Nioh3EngineFix.gpuopt-trace.csv` now carries `caller2_rva` (the second return
address inside Nioh3.exe; the first is `SynTransitionResource` 115E9D for every engine barrier) and, new in 1.7.10, `res_dim` and
`res_flags` (D3D12_RESOURCE_DIMENSION and D3D12_RESOURCE_FLAGS of the resource, e.g. `1,0x4` = buffer with ALLOW_UNORDERED_ACCESS;
`GetDesc` is cached per resource pointer). 1.7.9 wrote the header without `caller2_rva`; that is fixed. Cost is paid only while
`TraceRecords > 0`.

**The rewrites** (`[GpuOpt] R2a / R2b / R1`, live switches in the mod menu GPU section, Proton only, dispatcher thread only,
need the hooks: `Barriers = 1` or `2`). The `stats/s gpuopt` line shows `R2a on/off N/frame, R2b ..., R1 ... dropped; kept tex /
uav / unknown / foreign` (candidates that were passed through: texture, UAV buffer while R2b is off, no previous state known, previous
state from another thread or from the same list).
- **R2a**: a `COMMON -> X` on a buffer without ALLOW_UNORDERED_ACCESS gets `StateBefore = last tracked state | NPSR | PSR` (a write
  state such as COPY_DEST becomes `{P -> NPSR|PSR}` + `{NPSR|PSR -> X}`, kept in one batch). Conditions: whole-resource barrier,
  no flags, a previous state recorded by the same thread in an earlier list (list changes are detected per thread); anything else is
  passed through unchanged.
- **R2b (EXPERIMENT, warning)**: the same for buffers with ALLOW_UNORDERED_ACCESS (UAV binds, ExecuteIndirect arguments). It narrows
  the wait for UAV writes that may come from carried-over descriptors and can show stale data or flicker; it may also save nothing
  on RADV. Judge only with screenshots.
- **R1 (EXPERIMENT)**: drops read -> read transitions whose new state is a superset of the old one (only VCB/IB/NPSR/PSR bits). Not
  spec-exact; needs an image compare.

**Ally recipe** (one launch, then A/B live):
1. Launch 1: `[GpuOpt] Barriers = 2`, `TraceRecords = 300000`, `TraceDelaySec = 30` (default); play a match for ~2 min. The log says
   `trace: N records written`; take `Nioh3EngineFix.gpuopt-trace.csv` from the game folder.
2. On the PC: `py -I re\scripts_d3d12\vkd3d_cost.py Nioh3EngineFix.gpuopt-trace.csv` (self-check: `--selftest`). It splits the
   COMMON-origin barriers into R2a (non-UAV buffers) / R2b (UAV buffers) / textures, counts the R1 supersets by resource class and
   caller, and reports render-pass ends. Build-worthiness: R2a if >= ~50/frame, R1 if >= ~50/frame.
3. Same launch or the next one (no trace needed): in the menu switch R2a on and off while watching GPU W / fps (and the `stats/s
   gpuopt` rewrite counters); take screenshots of the same scene with each switch on and off. Then R1 the same way. R2b last, only as
   a curiosity. Save to ini writes `R2a / R2b / R1`.

Tests: `py -I tests\run_r2_harness.py` runs identical work (buffers with R2a read and write cases, a UAV buffer, a texture, a small
render target) on this PC's real D3D12 device with every switch combination and requires bit-identical buffers and render target,
the expected counters, and the trace columns.

### Input display (`[Menu] InputDisplay`)

SF6-style input history: newest row on top; per row the frames held (60 Hz frames, 99 max), the direction arrow (8-way, `N` = neutral)
and the buttons held. Face buttons are the controller display's own lit sprites (same atlas, same Xbox / PlayStation skin from
`ButtonStyle`, same `ControllerDisplayPressColor`); LB/RB/LT/RT are text chips (L1/R1/L2/R2 in PlayStation style). `InputDisplay = 0`
(default) / 1, `InputDisplaySide = Left` (below the FPS counter and the controller picture) / `Right`, `InputDisplayRows = 4 .. 24`.
Rows are on the Gameplay / Overlays page. It only reads the pad feed the controller display uses (raw state, before menu masking);
it writes no game state and no save data. Hidden while the mod menu is open.
