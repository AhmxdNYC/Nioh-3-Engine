# Nioh3EngineFix

A small native mod for **Nioh 3 (Steam, v2.02)**. It fixes CPU and power waste in the engine and makes the game
run at real speed when your frame rate is below the in-game cap. It patches the game in memory only. No game
files are changed, and nothing is left behind when the game exits.

> **Status: built, but not yet load-tested.** The logic is a 1:1 port of memory patches that were applied and
> measured live on v2.02 with external scripts. This DLL has not yet been run inside the game. Read
> `Nioh3EngineFix.log` after your first launch and report what it says.

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

**W6 is confirmed on the ROG Ally** (FG off): main thread 98 % -> 23 % CPU, GPU clock 1064 -> 1450 MHz, +7-12 % fps.
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

### Proton: FG never stays on in config.xml because of the mod (1.1.0)
With `[Menu] FrameGenToggle = 2` under Proton the mod switches FG on for the session only, but the game itself saves it
when the user leaves its own Options/pause menu - and FG on is reported to crash-loop the game there. Now, when the mod
switched FG on and config.xml had `FRAME_GENERATION` OFF at startup, the mod writes a marker file
(`Nioh3EngineFix.fgmarker`) and puts OFF back:
* at exit: hooks on the game's own `ExitProcess` / `TerminateProcess` imports (they run on the exiting thread before
  teardown; `DLL_PROCESS_DETACH` is only the last resort because of the loader lock and already-killed threads);
* at the next start, before the game reads its config, if the marker is still there (crash). This is the path that
  breaks the crash loop.
Both use the existing config.xml writer (value check, `.Nioh3EngineFix.bak` backup, only that value changed) and log
`MENU: Proton ...` lines.

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
* `[Latency] MaxFrameLatency`: 0 = the game's value, 1-16 frames (see Input latency).
* `[General] AllowUnknownBuild = 1` makes the mod try pattern matching on other builds. Each site is still
  verified, and anything unexpected is skipped. Not recommended.

## In-game mod menu
Change the fixes and the overlay while you play, with no restart. The menu opens at any time: in gameplay, in the
game's pause menu, or on the title screen.

| | Keyboard | Controller (ROG Ally / Steam Deck / Xbox pad) |
|---|---|---|
| open / close | **F10** | **hold Start (Menu) for 5 s** (a countdown shows top-right after 1 s) |
| move | Up / Down | D-pad or left stick up / down (hold to repeat) |
| change a value | Left / Right | D-pad left / right |
| toggle / run an action | Enter | A |
| close | Esc, Backspace, F10 | B, or hold Start 5 s again |

* A short Start press still goes to the game, so its pause menu opens as usual. While the mod menu is open the game
  sees a neutral controller and no arrow, Enter, Esc or Backspace keys. This lasts until you let go of the buttons
  after closing, so B doesn't also back out of the game's menu. The game never sees F10.
* While the game's pause menu is open, a reminder shows top-right: `F10 / Hold Start 5s: Mod menu`.
* All of this is configurable in `[Menu]`: key, hold button or combo (`Back+Start`, `L3+R3`, ...), hold time, text
  size, the reminder, and `FrameGenToggle` (see below).

What it shows:
* **Live fixes, applied at once:** VRR timing, W3 message-pump wait (and its Proton sleep mode), R1 render-wait
  timeout (0 = stock busy poll), L1 frame-limiter sleep, F1 FSR frame-gen presenter sleep, WK job wake gate, Max frame
  latency (game / 1 / 2 / 3; under Proton lowering applies after a swapchain rebuild / restart).
* **Input latency:** IK input kick early (live), TS input -> display (live; shows the median in ms).
* **Experimental CPU power:** W6, B, PP, FPC, AABB, M1, X1, E1 (live when installed; when not installed, toggling sets the
  ini key for the next launch, `* restart`), M2, J1, S1 (next launch). An `Exp/s` stats line shows W6 waits and
  average wait, PP skips, FPC hit rate, M1 refreshes, X1 and E1 skips.
  A fix that isn't installed this session shows `not installed` or `off (Proton)`, with the reason.
* **Overlay:** FPS counter, frame time, speed factor, counter position, counter text size.
* **Game setting:** *Frame generation* (the game's own setting, switched live the way its options menu does it), plus
  two status rows: *FG running* (real vs presented fps) and the game's *Upscaler* setting. See
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

### Frame generation status and switch
The startup popup and the menu show:
* the game's **FRAME_GENERATION** setting (OFF, DLSS, FSR or XeSS) and its **upscaler** setting. FSR frame gen doesn't
  need the FSR upscaler; it also runs with DLSS, XeSS or no upscaler.
* whether FG **really runs**: generated frames reach the screen when presented fps > 1.4x the real (engine) fps.
* a **reason line** when the setting is on but FG doesn't run:
  * `switching - the game is rebuilding its swapchain`: right after a change, for a few seconds.
  * `FSR presenter runs but adds no frames`: the engine pauses FG in menus, cutscenes, the title screen and
    character creation.
  * `no FSR presenter thread: the FG swapchain was not created`: the setting is on, but the game never built its FG
    swapchain. Under Proton this is the known vkd3d-proton problem.
* "Frame generation: off in game settings" means exactly that: nothing blocks it, it is simply off.

The log prints `OSD: game settings: FRAME_GENERATION ..., RESOLUTION_SCALING ...` at the first frame and on every
change. It also prints which thread presents: `"AMD FSR Presenter Thread"` means FSR FG is running.

The **Frame generation** item (Left/Right: OFF / FSR / XeSS / DLSS, only the types the game allows here; A: on/off)
calls the game's own setter, the function its Options menu uses, and marks the game's settings as changed. The engine
then rebuilds its swapchain with the new FG mode within a few frames, so the screen blinks once. There is one change
per 2 s. **Save to ini** also writes the new FRAME_GENERATION value into the game's `config.xml`: only that one value,
after copying the file to `config.xml.Nioh3EngineFix.bak`. The game would write the same value itself the next time
you leave its options screen.

`[Menu] FrameGenToggle` controls the switch:
* `0`: status only.
* `1` (default): switch on Windows, status only under Proton / Steam Deck / Bazzite.
* `2`: switch under Proton too.

Turning FG on under Proton is **reported to crash the game**. Afterwards the game won't start until config.xml is
fixed (community reports, vkd3d-proton issue 2845). So with `2` under Proton, FG on is applied for the session only:
the mod never saves it, and a crash is undone by restarting. While FG is on there, don't change other options in the
game's own menu, because the game would then save FG on itself. Background: `notes\frame-generation.md`.

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
