# CPU hotspots on the ROG Ally: what the SP profiles say, what is waste, and the fixes

Static analysis of the Ally's in-process profiler (SP) data plus the dump. Nothing was run, attached or patched.
Date: 2026-10-07. Scope: the game only (its code via the ASI, its data, its own API calls).

- Data: `captures\ally\nioh3_ally_modlog_v71-MAX-SP-ED-Barriers_20261007-0547.log` (ASI 1.3.1, MAX settings, FG off,
  1080p FSR). 146 SP reports of 30 s each. Reports 0-6 are the title screen and the first minutes in the level (60 fps,
  then loading and the wake storms). **Reports 7-145 (139 x 30 s = 70 min) are steady gameplay**: 16.2 fps rendered,
  **44.6 game ticks/s**, speed 0.97x.
- Context: `captures\ally\nioh3_ally_threads_v6-w6on-stockenv_20261007-0500.txt` (Linux per-thread CPU, W6 on, lighter
  preset) and the v4/v5/v6 mod logs (tick rate vs render rate).
- Tools: `re\scripts_cpu\sp_aggregate.py` (aggregates SP blocks, folds .pdata fragments, names from symbols.tsv) and
  `re\scripts_cpu\cpu_hotspots_check.py` (offline proof of every site, byte and register fact used below: **ALL PASSED**).
- Symbols: `re\symbols\incoming\cpu-hotspots.tsv` (20 rows, dry-run merge OK: 19 added, 1 updated; not merged).

## TL;DR

With GF (ASI 1.4.0) the last big **spin** on the Ally is gone. What remains is mostly **real work done too often**:
the game logic runs 44.6 ticks/s while the GPU shows 16 fps.

| # | sink (steady MAX gameplay) | evidence | waste? | fix | expected CPU saving | status |
|---|---|---|---|---|---|---|
| 1 | [App]Game FenceWait Sleep(0) spin (7D9E1C, plus the ASI loader's Sleep shim inside it) | 57.6 + 1.6 + 6.5 % of [App]Game samples (`[ntdll] <- 1180A4`, `[DINPUT8]/[ntdll]/[kernel32] <- 4E390`) | pure spin | **GF** (SetEventOnCompletion + event wait) | **~0.6 core** (main thread ~97 % -> ~35 % busy) | built (1.4.0), Ally run pending |
| 2 | **Logic over-ticking**: 44.6 game ticks/s for 16.2 rendered fps. 2.8 logic ticks per shown frame; the render thread uses only the newest packet | `game ticks 44.6/s` vs `rendered 16.2 fps`; TS C->C 22.4 ms vs P->P 61.8 ms; v4/v5/v6: 60/s vs 18-36 fps | work whose output is mostly never shown | **TG** tick governor (limiter target follows the rendered rate, floor 30) or, with no code, the game's own **30 FPS cap** | **~1.0-1.4 cores** at floor 30 (~1.5-2 at floor 24) | design ready, zero-code A/B possible today |
| 3 | Job-worker wake/sleep churn | WakeStats: 14-16k wakes/s steady, ~6k wasted; **40-165k/s storms** (pool 1, wake-one 48C6B) in the first 4 min in the level | wasted wakes | **WG** pending-wake gate on WS2/WS3 (+ wrapper at WSs) | ~0.05 core steady, **~0.3-1 core during storms** | design ready |
| 4 | WinMain pump on Proton (W3 = Sleep(1), ~890 loops/s) | Linux: the process's first thread 11 % CPU, 8.4k ctx switches/s | mostly polling | `W3_TimeoutMs = 3` on Proton (ini only) | ~0.07 core | config change |
| 5 | Ultimate ASI Loader IAT shims (Sleep, AcquireSRWLockExclusive, ...) on every call the game makes | `[DINPUT8] <- ...` rows on [App]Game and [App] threads | per-call tax | **IATS**: point the shimmed slots back at kernel32 after loading | ~0.01-0.05 core after GF | design ready |
| 6 | [App]Game 1 ms QPC spin when no new frame packet (D) | 0.36 % of [App]Game now | spin | **DS**: hi-res timer sleep inside the loop | ~0 now; matters only with TG | design ready |
| 7 | ResourceLoad manager idle poll Sleep(1) | not seen in this capture | polling | **RL**: idle Sleep(1) -> Sleep(4), 3-byte in-place | <= 0.02 core | design ready, verify it runs first |
| - | Main-thread draw prep, job-worker logic, dispatcher translation, [App] workers | 31.6 % / 2.15 / 1.83 / 0.08 / 0.5 thread-equivalents of game code | real work | existing: A (atomics), B, PP, FPC, AABB, J1, M1, M2, E1, ED | small, already designed | see their notes |

**Integrate first:** (1) GF as shipped (run it); (2) TG, after the zero-code A/B "in-game FPS cap 30 vs 60" confirms the
power win; (3) WG on the wake-one sites. Plus the ini change W3_TimeoutMs = 3 under Proton.

**The "DirectInput poll under 0x4E390" is not DirectInput** (section 3.2). 4E390 is `ID3D12Resource::GetDesc().Width < n`;
the `DINPUT8.dll` module on the main thread is Ultimate ASI Loader's dinput8.dll proxy, whose kernel32 IAT shim sits in
front of every `Sleep(0)` of FenceWait's spin. These rows move with the fence-wait rows at r = 0.956 over 146 reports and
will disappear with GF. The game's real DirectInput reads run on [App]InputUpdateWorker, once per logic tick, and cost
~0 (`[win32u] <- 435D58` 0.00 %).

---

## 1. How to read SP numbers (and their two traps)

- SP suspends [App]Game every 1 ms and one worker round-robin (~134 samples per worker thread per 30 s, the same for
  every group). **A share is wall time, not CPU.** Blocked threads are sampled too. For a group of N threads, "RIP in
  Nioh3.exe %" x N = thread-equivalents of game code, comparable across groups.
- For RIP outside Nioh3.exe, SP scans the copied stack upward for the **first** value that looks like a return address
  into Nioh3 (call-site validated, no unwinding). **Stale slots win** whenever the system DLL frames leave an old return
  address of a deeper earlier call below the real one. Two cases decide this report:
  - `[ntdll] <- 1180A4` is not IsFenceComplete running. FenceWait 7D9E1C calls 1180A4 (which pushes 1180B6 at
    F-38h) and then Sleep(0) (which pushes 7D9E6E at F-8). Inside Sleep the scan meets the stale 1180B6 first. So
    these rows are the Sleep(0) of FenceWait's `event == NULL` loop, which is what GF patches.
  - `[DINPUT8.dll] / [ntdll.dll] / [kernel32.dll] <- 4E390` (section 3.2).
- Single-thread groups of idle threads show "[ntdll] <- X" with X a random stale function (in [App]: 4A09F0 is a bit
  allocator, 37F550 a float loop; neither calls the system). Each such row of 2.2-2.3 % is one parked [App] thread out of
  44 (1/44 = 2.27 %).
- Suggested SP v2 (for the ASI owner): weight each sample by the thread's `QueryThreadCycleTime` delta since its last
  sample (separates CPU from blocked time, like cpuprof's "busy" column), and unwind game frames with the .pdata unwind
  codes instead of scanning (no stale slots; RtlVirtualUnwind works on the copied stack).

## 2. Steady-state profile (reports 7-145, 70 min)

`py -I re\scripts_cpu\sp_aggregate.py <log> 7 -1 25`

| group | threads | RIP in game | thread-equivalents of game code | where the rest of the wall time is |
|---|---|---|---|---|
| main [App]Game | 1 | 31.6 % | 0.32 | **65.7 % FenceWait Sleep(0) spin** (57.6 `[ntdll] <- 1180A4` + 1.6 unix + 3.6/2.9/0.1 `<- 4E390` loader shim) |
| KIDSTaskScheduler | 8 | 26.9 % | **2.15** | 68.4 % parked in JobWorkerThreadMain (WaitForSingleObject INFINITE, fine); 1.65 % contended std::mutex (MtxLock B611BC, blocked); 1.1 % condvar; 0.73 % XtimeGetTicks (timed condvar waits) |
| KIDSTS | 77 | 2.4 % | **1.83** | 97.4 % parked (fine) |
| SynergisticDispatcher | 1 | 8.4 % | 0.08 (+ ~6 % in vkd3d/winevulkan = real translation) | 69 % ring-full wait (EclRingPush 86F828 -> WFSO INFINITE at 1921A43, fine); 4.6 % SRW shared lock (7F64DC, blocked); 2.5 % its own WFSO (fine) |
| [App] other | 44 | 1.1 % | 0.50 | 53.6 % condvar waits (CndWaitSrw B616A4), single parked threads with stale labels |

So the game-code work is ~4.9 thread-equivalents, and **~4.3 of them are logic-side** (Scheduler0 plus workers plus the
[App] update threads). The Linux capture agrees (KIDSTaskScheduler x26 265 %, StageUpdate 75 %, Collision 72 %,
EffectUpdate x8 36 %, MissionThread 14 %).

Top game functions, main thread (% of [App]Game samples): DrawEmit 1E80B0 2.03 (fragment 1E81AE = its 10.6 KB body),
MemcpyErms B94230 1.16 + Memcpy B94250 0.83, MainHot_418F40 1.13, DrawListWalk 0.77, TextureStreamUpdate 0.70,
MeshletCheckRequestLoad 0.69, TextureStreamTouchSlot 0.61, PagePoolCountUsed 0.59, DrawListGroupLength 0.55,
state commit 1D7340/1D8720/1DAF6C/1D84A0 ~1.6, WcUploadZeroFill 61B1C 0.34 (W1 site).
Scheduler/workers: object update 1A7BF0, 121FC0, 425B0, JobQueuePoll 411C0 (J1), 88780, 204584, BoundsFromPartObbs
71C60 + ObbToAabb 6EA50/6F010/6EFD0 (AABB), CollisionObjectUpdate D80B0, CollisionShapeGetWorldPos D8B70.

## 3. Main thread

### 3.1 FenceWait spin (GF) - the #1 sink until GF ships
`7D9E1C GfxWaitFenceValue`: `if (!IsFenceComplete) { if (event) { SetEventOnCompletion; WFSO } else do Sleep(0) while
(!IsFenceComplete) }`. At MAX the GPU takes ~62 ms per frame, the main thread finishes its ~20 ms of draw prep, then
spins ~40 ms in Sleep(0) (`sched_yield` under Wine) per frame. GF (ASI 1.4.0) replaces the Sleep at 7D9E66 and at the
worker poll 117D9F with an event wait; the loop's own re-check is kept. Expected on the Ally: [App]Game from ~97 % to
~35 % busy, **~0.6 core**, and the 3.6 + 2.9 % loader-shim rows vanish with it (the A/B proof of section 3.2).

### 3.2 0x4E390 is not a DirectInput poll
- `4E390` (called only from 4E2F8 / 62E918, the upload-buffer path under MeshletUploadBuffers 4DB6C):
  `obj->vtbl[0x50](&desc); return desc.Width(+10h) < n` = `ID3D12Resource::GetDesc().Width < n`. It cannot call into
  dinput8, kernel32 or ntdll (checked: `cpu_hotspots_check.py`).
- The rows are stale-slot attributions of FenceWait's Sleep(0): over the 146 reports they track the 1180A4 rows with
  **r = 0.956** and a fixed ratio of 0.112; they are absent at the title screen (W6 waits via WaitOnAddress, no Sleep);
  `[DINPUT8] <- 117D6C`, the other Sleep(0) fence poll, shows the same module.
- `DINPUT8.dll` in the game process is **Ultimate ASI Loader's dinput8.dll proxy** (`WINEDLLOVERRIDES=dinput8=n,b`).
  Its string table holds its kernel32 IAT-hook list (GetStartupInfo*, GetModuleHandle*, GetProcAddress, LoadLibrary*,
  FreeLibrary, CreateEventA/W, GetSystemInfo, InterlockedCompareExchange, **Sleep**, GetSystemTimeAsFileTime,
  GetCurrentProcessId, GetCommandLine*, **AcquireSRWLockExclusive**, the CreateFile/FindFirstFile/GetFileAttributes
  overload hooks). Those shims stay in the game's IAT after loading, so every `Sleep(0)` of the spin runs the shim's
  once-check (kernel32 InitOnce -> ntdll) before the real Sleep. That is the ~11 % overhead inside the spin. See IATS (9).
- The game's real DirectInput use: `DiKeyboardGetState` 435D58 and `DiMouseGetState` 4371DC on [App]InputUpdateWorker,
  once per logic tick (44/s here), plus GetCursorPos / GetAsyncKeyState (notes\input-thread.md). SP: `[win32u] <- 435D58`
  0.00 %, `[DINPUT8] <- B611BC` 0.1 % of the [App] group. Nothing to optimise beyond TG (fewer ticks = fewer polls)
  and X1 (already shipped).

### 3.3 The rest of [App]Game is draw-prep work
- `1E81AE` is the body fragment of DrawEmit 1E80B0 (per draw per sub-pass: state shadow compares, token emission, meshlet
  residency check). Its only waste is the per-draw `lock cmpxchg` atomic load at 1E835B (A3 in
  notes\mainthread-drawprep.md, verified offline, not in the ASI yet).
- `B94230` is Memcpy's `rep movsb` (ERMS) path for large copies: inline token-0x66 blobs and upload copies. Real work
  (KTGL copies data into its command stream, then the dispatcher copies again); not a patch.
- After GF the main thread is ~0.35 core at 16 fps. A (25 atomic loads, ~1-2.5 % of the thread), B, PP, FPC, C/E are the
  remaining draw-prep items; they matter less on the Ally than TG and WG.

## 4. The big one: logic over-ticking (TG)

**Evidence.** The logic (frame step 5ACF80 on KIDSTaskScheduler0 plus its jobs) and the render thread ([App]Game) are
decoupled: the render frame consumes the newest frame packet (3D1F20 checks `+138h/+1E1h`) and older packets are
dropped. The logic never waits for the render side. On the Ally:

| log | rendered fps | game ticks/s | ticks per shown frame |
|---|---|---|---|
| v71 MAX (this report) | 16.2 | 44.6 (TS C->C 22.4 ms, logic CPU-bound: `L1 timer sleeps 0`) | 2.8 |
| v6 IK/threads | 32.8-41.2 | 44.4-47.2 | 1.1-1.4 |
| v5 W6 A/B | 33.2-33.8 | 52.7-59.6 | 1.6-1.8 |
| v4 FG off | 17.9-19.0 | 59.7-60.8 | 3.2 |

So up to 2/3 of all logic ticks (collision, stage update, character, AI, effects, mission scripts, input) produce state
the player never sees. VRR (variable dt) keeps the game speed at 1.0 at any tick rate inside its clamp (tier 60:
24-60). ~4.3 thread-equivalents of the game's code are logic-side; most of it scales with the tick rate.

**Expected saving** (assumes ~85-90 % of logic-side work is per tick): ticks 44.6 -> 30 = -33 % -> **~1.0-1.4 cores**;
-> 24 = -46 % -> ~1.5-2 cores. W6 freed ~0.75 core and gave the GPU +37 % MHz; this is the same order or larger.

**Behaviour.** Not "identical": the simulation step becomes 1/30 s (or 1/24 s) instead of 1/44.6 s. That is exactly what
the game does when it runs at 30 fps (the 30 FPS cap is a native mode: tier 30, dt 1/30), so it stays inside the
engine's designed envelope. What the player sees is unchanged except: input is sampled per tick (a tap must be held
> 33 ms instead of > 22 ms to be seen; normal taps are 50-100 ms), and the newest packet is on average ~6 ms older at
display time (at 16 fps the frame itself is 62 ms).

### 4.1 Zero-code A/B (do this first)
Set the in-game FPS cap to **30** at MAX (VRR tier 30, clamp 20-30, `FPS dynamic adjustment` OFF as always). The
limiter then caps the logic at 30 ticks/s and L1 sleeps the slack (`L1OnWine = 1`, see 4.2; check `L1 timer sleeps` > 0
in the log). Compare with cap 60: ticks/s, CPU W, GPU MHz, fps, `speed`, TS input->display.

### 4.2 TG design for the ASI (automatic, adapts to the GPU rate)
Rides the existing VRR P4 site (`5AD860 8B 87 14 06 00 00`, `call caveLim`); no new code site.

```
; caves.asm: caveLim gets one more clamp (eax and flags only, as today)
caveLim PROC
    mov     eax, dword ptr [rdi+614h]
    cmp     dword ptr [vrr_fpsOut], 0
    je      lR                      ; VRR not armed this frame -> stock
    mov     eax, dword ptr [vrr_capCur]
    cmp     dword ptr [tg_cap], 0       ; 0 = governor off / nothing to govern
    je      lR
    cmp     eax, dword ptr [tg_cap]
    jbe     lR
    mov     eax, dword ptr [tg_cap]     ; limiter target = governor cap
lR: ret
caveLim ENDP
```
Only eax and the flags change, as in today's caveLim (the P4 contract).

C++ governor (osd.cpp Present hook of the game swapchain, or a 250 ms tick):
```
renderedFps = EMA over ~1 s of the game chain's presents (the OSD's "rendered" value; FG frames excluded)
if (!TG || presents stalled > 250 ms || menu/loading (no 3D frames)) tg_cap = 0;          // stock
else {
    floor  = max(TG_MinTicks (30), clampMinOfTier + 1)                                   // tier 60: 24 -> 30
    target = ceil(renderedFps * TG_Ratio (1.25)) + TG_Margin (2)
    target = clamp(target, floor, vrr_capCur)
    if (target > tg_cap) tg_cap = target;                       // raise at once
    else if (target < tg_cap - 2 for 2 s) tg_cap = target;      // lower slowly (hysteresis)
    if (tg_cap >= vrr_capCur) tg_cap = 0;                       // nothing to govern
}
```
- Self-stabilising: if the render rate is limited by the tick rate, the 1.25 ratio lets it grow until the GPU limits.
- ini: `TG` (0 off / 1 on / 2 installed+off), `TG_MinTicks` (24-60, default 30), `TG_Ratio` (1.0-2.0), live menu switch,
  stats `TG cap n, ticks/s, rendered fps`.
- Needs VRR armed (otherwise stock dt would slow the game) and L1 (otherwise the limiter slack is a Sleep(0)/job-steal
  spin on Scheduler0, see W4 in notes\cpu-profile.md). Under Proton L1 needs `L1OnWine = 1`: it was applied in the v71
  run (`L1: applied`, 22-60 timer sleeps/s at the title), but `mod\ally-package\Nioh3EngineFix.ini` says 0 - make the
  zip say 1 before the TG / cap-30 legs, or the saved ticks turn into limiter spin.
- G[614] follows the measured rate (30), as when the game runs at 30; GetFps consumers keep seeing the tier (P8/P9).
- Pair with DS (section 10): when the governor cap is close to the render rate, the render thread more often finds no
  new packet and runs the 1 ms QPC spin.
- Risks: input-sampling granularity (above); physics step 33 ms = the game's own 30 mode; online co-op: same caveat as
  VRR (offline only).

## 5. Worker wake churn (WG)

**Evidence** (WakeStats, `stats wakes/s` lines): steady gameplay SetEvent 14-16k/s, worker sleeps the same, "wasted"
(sleeps without a wake check from 3FE14) ~5-6k/s, mostly the pop chain 40B0A (~10.5k/s). In the first ~4 minutes in the
level: **wake-one 48C6B 25-150k/s, all on pool 1** (`pools id:n 1:101306 ... 1:158569`), "wasted" up to 159k/s. That is
a submitter that pushes small jobs one at a time with a wake-one each, then helps and runs them itself; every woken
worker finds nothing and sleeps again. At ~5-10 us of CPU per wake+sleep pair under Wine, a 100-160k/s storm costs
~0.5-1.5 cores while it lasts.

**Why WK2's objection no longer holds.** notes\worker-power.md dropped a stricter gate because "woken but not yet
running" could not be counted. The ASI now owns both ends: the six SetEvent sites (WS0-WS5) and the sleep site (WSs).
A per-pool `pending` counter (incremented at a real SetEvent, decremented when the worker's WaitForSingleObject
returns) is exactly that count.

**Gate rule.** At the wake-one sites, if a worker of the same pool is already woken and not yet running (pending >=
`WG_MaxPending`, default 1, and its last increment is younger than `WG_WindowUs`, default 2000), do not wake another:
give the target back its "sleeping, wake me" bit and return. The pending worker returns from its wait and then runs
JobWorkerRunUntilIdle (own queue, then steals from every worker of the pool), so a job pushed before the gate decision
is always seen; extra parallelism still arrives through its pop-chain wakes. Never gated: 48DEC (wake-except) and 48E3C
(forward to the one worker a job must run on). The pop chain 40B0A/40D2F is gated only with `WG_PopChain = 1`.

Register context (all proven by `cpu_hotspots_check.py`):

| site (existing) | bytes | target WorkerThread | pool | after the call |
|---|---|---|---|---|
| WS2 48C6B JobWakeOneInPool | `FF 15 D7 96 89 03` | `[rsi]` (slot rsi = workers + i*458h) | `[rsi+440h]` | 48C71 `mov al,dil` (eax ignored) |
| WS3 48D06 JobWakeOne | `FF 15 3C 96 89 03` | `[rbx]` | `[rbx+440h]` | 48D0C `mov al,1` |
| WS0 40B0A pop chain | `FF 15 38 18 8A 03` | `rdx` | `[r15+440h]` (waker's pool = target's pool) | 40B10 (eax ignored) |
| WS1 40D2F task wake | `FF 15 13 16 8A 03` | `r8` | `[rsi + r10*458h + 440h]` | 40D35 (eax ignored) |
| WSs 1A6BC35 worker sleep | `FF 15 05 67 E7 01` | - | `rbx` (callee-saved) | 1A6BC3B `jmp 1A6BC48: xor eax,eax` (result dead) |

The CAS at 48C53/48CEF/40ADF/40D01 has already cleared bit0 of the target before the SetEvent call, so a skip must set it
back. Cave sketch (WS2; WS3 is the same with rbx, WS0/WS1 with their registers):

```
; wg_pending: 64 x {dword count, dword pad, qword tsc} on separate 64-byte lines, index = pool+34h & 63
caveWg2 PROC                         ; replaces call [SetEvent] at 48C6B; rcx = event; uses rax, r10, r11 only
    (WakeStats counting as today)
    mov     rax, [rsi+440h]
    mov     eax, [rax+34h]
    and     eax, 63
    shl     eax, 6
    lea     r10, wg_pending
    add     r10, rax
    cmp     dword ptr [wg_enable], 0
    je      wake
    mov     eax, dword ptr [r10]
    cmp     eax, dword ptr [wg_max]
    jl      wake
    rdtsc                            ; edx:eax - edx is volatile and not an argument of SetEvent
    shl     rdx, 32
    or      rax, rdx
    sub     rax, qword ptr [r10+8]
    cmp     rax, qword ptr [wg_windowTsc]
    ja      wake                     ; stale count (lost decrement) -> behave stock
    mov     rax, [rsi]               ; WorkerThread, still blocked: no SetEvent was issued
    lock or dword ptr [rax+10h], 1   ; restore "sleeping, wake me"
    lock inc qword ptr [wg_skipped]
    mov     eax, 1
    ret
wake:
    lock inc dword ptr [r10]
    rdtsc
    shl     rdx, 32
    or      rax, rdx
    mov     qword ptr [r10+8], rax
    mov     rax, qword ptr [ws_iatSetEvent]
    jmp     qword ptr [rax]
caveWg2 ENDP
```
(The cave keeps rcx, the SetEvent argument, and uses rax, rdx (rdtsc), r10: all volatile across `call [SetEvent]` in the
Win64 ABI, so the game's code after each site cannot rely on them; WS2/WS3 return eax-independent results, see table.)

The sleep site becomes a real wrapper (today it is a tail call):
```
caveWsSleep PROC FRAME               ; replaces call [WaitForSingleObject] at 1A6BC35; rbx = pool
    sub     rsp, 28h
    .allocstack 28h
    .endprolog
    (WakeStats counting as today)
    mov     rax, qword ptr [ws_iatWait]
    call    qword ptr [rax]
    mov     eax, [rbx+34h]           ; the result is dead (1A6BC48 xor eax,eax)
    and     eax, 63
    shl     eax, 6
    lea     r10, wg_pending
    add     r10, rax
dec1:
    mov     eax, dword ptr [r10]
    test    eax, eax
    jle     done                     ; saturate at 0 (wakes from uncounted sites)
    lea     edx, [rax-1]
    lock cmpxchg dword ptr [r10], edx
    jne     dec1
done:
    add     rsp, 28h
    ret
caveWsSleep ENDP
```
Also count (never gate) at WS4/WS5 so the decrements of their wakes have a matching increment.

- Correctness: no lost work (a skip only happens while another worker of that pool is provably about to scan); the
  worst case of a race is one spurious wake (a worker whose bit0 was restored while a stale signal was pending), which
  the stock code also produces. Results of jobs are unchanged; only which thread runs them changes.
- Expected: storms -70-80 % of their wakes (one woken worker per wake latency instead of one per job), steady ~-20-30 %
  of the wasted wakes. **Up to ~1 core during storms, ~0.05 core steady.**
- Measurement is already in the ASI (WakeStats). Add `WG skipped/s` to the 30 s line.
- Risk: fan-out latency. A burst of N submissions now ramps up through the pending worker's pop chain (one wake latency
  per step) instead of N parallel wakes. Watch Scheduler0's C->L time and fps with `WG_MaxPending` 1 vs 2.

## 6. SynergisticDispatcher: blocked, not spinning
- 69 % `[ntdll] <- 4930C8` = CmdListRingSubmitUpTo -> EclRingPush 86F760 -> EclRingPushItem 86F828:
  `WaitForSingleObject(sem, 0)`, and when the ring is full the cold path 1921A30 does `WaitForSingleObject(sem, INFINITE)`.
  That is GPU-bound backpressure with a proper block.
- Its main loop 1A7C3E0 waits with WFSO INFINITE. 7F64DC holds an SRW shared lock (blocked when an exclusive holder runs).
- Its CPU is token replay plus vkd3d-proton / winevulkan translation (`[winevulkan] <- SynFlushGfxStateForDraw` etc.).
  Barrier batching (gpuopt) is the only lever there; nothing to fix for power.

## 7. [App] threads
- 53.6 % of the group is parked in condvar waits (CndWaitSrw B616A4); the 2.2-2.3 % single-thread rows are parked threads
  with stale labels (section 1). RIP in game 1.1 % = 0.5 thread-equivalents.
- EffectUpdate x8 (Linux 36 % total), MissionThread (14 %), the [App]*Thread KtglWorkers: kick/join once per logic tick,
  blocking properly (`KtglEventWait` = SRW + condvar). Real per-tick work: TG reduces it proportionally; ED/E1 already
  trim the effects.
- CustomOccluderRayCastThread: 4 % CPU but 1.35k context switches/s (several kick/join batches per tick). Small.
- The WinMain pump (the process's first thread, unnamed): with W3 on Proton it is `Sleep(1)` per empty queue, ~890
  loops/s, and still 11 % of a core (5.2 % user, 5.9 % sys, 8.4k context switches/s: PeekMessage and the 47990 cursor
  check go to the wineserver). Window messages are not on the gameplay input path (pads: XInput on InputUpdateWorker;
  mouse deltas are accumulated by the WndProc and latched per tick). **`[Tuning] W3_TimeoutMs = 3` under Proton** cuts
  the loop rate to ~300/s: ~-0.07 core, window-message latency +2 ms.

## 8. Sweep of every Sleep(0) / Sleep(n) / yield site (re\out\spinscan.txt + dump + SP)

Classes (all 400 Sleep/SleepEx call sites, 319 of them Sleep(0); `py -I re\scripts_cpu\sleep_classify.py` ->
`re\out\sleepclass.txt`, one row per site):

| class | sites | idiom | gameplay cost in SP (70 min) |
|---|---|---|---|
| spin256 | 89 Sleep(0) (74 with lock cmpxchg) | `do { CAS } while (fail && (++i & 0xFF || Sleep(0)))`: KTGL spinlocks / RW write locks, **no PAUSE** | D8E00 collision 0.08 % of KIDSTS (incl. its real work), 205840 0.06 % of KIDSTaskScheduler, others 0 |
| CAS-or-yield | 120 Sleep(0) with lock cmpxchg nearby | `while (!CAS) Sleep(0)` try-locks | 3D859C-type, 0 in this capture |
| flag / fence / job polls | 110 Sleep(0) | `while (!cond) Sleep(0)` | see table below |
| timed sleeps | 41 Sleep(1), 21 Sleep(variable), 16 Sleep(2..1000), 3 SleepEx(0) | timed polls, mostly loaders, shutdown, debug | RL (below), worker paused-pool loop |
| SwitchToThread | thunk B62080, callers 206BE4, 91418C, 223CD88, 26F5230 | yield loops | 0 samples |

Every site that showed **any** sample in 70 minutes (the main thread is sampled 2.4 M times, every worker group > 0.8 M):

| site (fn) | thread | waits for | blocking primitive nearby | SP steady | status / fix |
|---|---|---|---|---|---|
| 7D9E66 FenceWait | [App]Game | GPU fence | ID3D12Fence::SetEventOnCompletion (the engine's own event path in the same function) | 65.7 % of main | **GF** (1.4.0) |
| 117D9F GfxFencePollWorker | ECL / meshlet workers, main | GPU fence | SetEventOnCompletion (path 190A0E4) | 0.8 % main (early), ~0 workers | **GF** (1.4.0) |
| 7D9EA4 present-flag wait 7D9E90 | [App]Game | Present thread clears `+7728h` | none in the game; W6 adds WakeByAddress at 1A822D9 | 1.3 % main (title) | **W6** (done) |
| 1A6BF2D PeekMessage loop | WinMain pump | window messages | MsgWait (returns at once on Wine) | (not sampled: no [App] name) | **W3** done; tune `W3_TimeoutMs = 3` on Proton |
| 19DF583 -> 916F14 limiter | KIDSTaskScheduler0 | QPC deadline | hi-res waitable timer | 0.01 % (logic CPU-bound) | **L1** (done); pays off with TG |
| 1A74BD0..1A74C48 D spin (pure QPC, no Sleep) + 1A74218 / 1A742B8 / 1A745CF / 1A7462F / 1A747B8 Sleep(0), 1A746D7 Sleep(1) | [App]Game | new frame packet / try-locks 877148, 1A6030 | none | 0.36 % main | **DS** (section 10) for the QPC spin; the Sleep(0) try-lock loops are rare |
| 404B9 JobHelpStep, 2072FB JobWaitSleepBackoff, 916F14 JobWaitStep | any job waiter | job group counter | none (help-while-waiting by design) | <= 0.01 % | leave: a sleep would add latency to every join |
| 3FD5C / 3EB93 KtglSignalUntilAwake | main, workers | target thread acknowledges | SetEvent loop by design | 0.02 % main | leave |
| 1910C04 KtglCmdStreamFlush ring full | [App]Game | dispatcher frees command-ring space (`[ctx+3C04h]`) | none (no waker) | 0.011 % main | leave (rare); if it grows at MAX: GF-style bounded WaitOnAddress(ctx+3C04h, 1 ms) |
| D9C38 (D8E00) collision write lock, 206230 (205840) RW lock | KIDSTS / KIDSTaskScheduler | lock word | SRW would be the fix (not a patch) | <= 0.08 % | optional PAUSE in the 255-try inner loop; not worth 89 patch sites |
| 1A6255 / 1A6305 / 1A63CD (ResourceIsReady 1A6030) | workers, [App] | resource ready flag | none | 0.03 % | leave |
| 1A6BC42 worker paused-pool Sleep(1) | KIDS workers | pool unpaused | none | 0 (Linux: no 1 kHz pollers among KIDSTS) | leave |
| 1FEE68 ResourceLoad idle Sleep(1), 1FF18C Sleep(r13d) with a wake-one per round while loads are in flight | the ResourceLoad job | load requests | none | 0.000 % (not running in this capture) | **RL** (below), only if SP shows it |
| 133980, 11140A, 130374, 2477CA, 42A95 SleepEx(0), 12E8BF, 19285F8, 3D066/3D0EA, 3E920, 1FF371 | various | locks, flags | - | < 0.002 % each | leave |

All other sites had **zero** samples in 70 minutes of play, which bounds each well below 0.001 % of its group. Spin and
poll waste outside GF/W6/W3/L1 is **< 0.5 % of one core** in total in this scenario. The waste on the Ally is the work rate
(TG) and the wake rate (WG), not spinning.

**RL (ResourceLoad idle poll), in place, 3 bytes:** at 1FEE65 `8D 4A 01` (`lea ecx,[rdx+1]`, edx = 0) ->
`6A 04 59` (`push 4; pop rcx`), so the idle Sleep(1) becomes Sleep(4) only when `[mgr+200h] == 0 && [mgr+210h] == 0`.
Pattern `88 91 49 02 00 00 8D 4A 01 FF 15` unique, site = hit + 6; no branch into 1FEE66..1FEE67. New load requests are
noticed <= 4 ms later instead of <= 1 ms. Saving: 750 fewer wakes/s of one worker (<= 0.02 core). Build it only if a
profile shows `<- 1FEE28` rows.

## 9. IATS: undo the ASI loader's per-call shims (optional)
After the loader has loaded every ASI (our init thread runs after that), for each IAT slot of Nioh3.exe in
{Sleep 38E2338, AcquireSRWLockExclusive 38E2618, GetSystemTimeAsFileTime 38E2270, GetCurrentProcessId 38E2498,
CreateEventW 38E25A0, CreateEventA 38E25C8, GetSystemInfo 38E26C0}: if the slot points into the module that exports
`IsUltimateASILoader`, write `GetProcAddress(kernel32, name)` into it (page made writable, then restored; logged). Never
touch the CreateFile / FindFirstFile / GetFileAttributes / LoadLibrary / GetProcAddress slots (the loader's
`update` folder overloads and plugin loading use them) or any slot that points elsewhere (another mod's hook).
Saving after GF: the shim tax on the remaining Sleeps and on every exclusive SRW acquire (124 call sites), estimated
0.01-0.05 core. Risk: low; a loader that needs its Sleep shim after loading does not exist in the UAL source we know of,
but check the loader version's behaviour once (log the old pointers).

## 10. DS: sleep instead of spinning in the 1 ms "no new frame packet" retry (optional, with TG)
The main loop 1A74130 retries the frame function after `while ((QPC - start)/freq < 0.001f) ;` (1A74BD0..1A74C48, no
pause, no readiness check; start = rbx from 1A74BB8). Site: the loop's `call [QueryPerformanceCounter]` at 1A74BDA
(`FF 15 40 D8 E6 01` -> `90 E8 rel32`, call_at 1; pattern `80 7A 12 00 74 10 48 8D 4D A0 FF 15` unique, site = hit + 0Ah;
no branch into the site).
```
caveDs PROC                          ; rcx = &now (LARGE_INTEGER), rbx = loop start (callee-saved, read-only)
    mov     rdx, rbx
    jmp     DsQpc                    ; BOOL DsQpc(LARGE_INTEGER* now, i64 start) - normal Win64 call frame
caveDs ENDP
BOOL DsQpc(LARGE_INTEGER* now, i64 start) {
    QueryPerformanceCounter(now);
    if (!g_ds || !g_l1Timer) return TRUE;                      // stock spin
    i64 left = g_qpf / 1000 - (now->QuadPart - start) - g_qpf * DS_MarginUs / 1000000;   // margin 150 us
    if (left > g_qpf / 5000) { HiResTimerWait(left); QueryPerformanceCounter(now); ++g_dsWaits; }
    return TRUE;
}
```
The loop's own exit test is unchanged (it still requires 1 ms elapsed). Saves the spin (up to 1 ms per retry); the
retry may come up to the timer's overshoot later (Wine: ~0.1-0.5 ms). Only worth it with TG or near the cap on PC.

## 11. Ready for the ASI (all offline-verified, `py -I re\scripts_cpu\cpu_hotspots_check.py` -> ALL PASSED)

| id | sites | kind | new code | switch | default |
|---|---|---|---|---|---|
| GF | 7D9E66, 117D9F | (shipped 1.4.0) | - | live | Ally 1 |
| **TG** | none new (P4 5AD860 / caveLim) | data + 4 instructions in caveLim | governor in the Present hook | live | 0 (Ally zip 2 = installed, off, for the A/B) |
| **WG** | WS0-WS5, WSs (existing) | cave logic | wrapper at WSs, per-pool pending counters | live (`WG`, `WG_MaxPending`, `WG_PopChain`, `WG_WindowUs`) | 0 (Ally zip 2) |
| W3 tune | - | ini | - | - | Ally zip `W3_TimeoutMs = 3` |
| IATS | IAT slots (data) | pointer writes at init | module check | next launch | 0 |
| DS | 1A74BDA (`FF 15` -> `90 E8`) | call | DsQpc (reuses L1's timer) | live | 0 |
| RL | 1FEE65 (`8D 4A 01` -> `6A NN 59`) | bytes | none | next launch / frozen write | 0 |

Existing, built and offline-proven but **not** in the Ally zip yet and worth enabling there (CPU-only, same output):
AABB (Scheduler0 ObbToAabb, 0.35 % + callees in this profile), J1/M2 (atomic loads in job polls), M1, B, PP, FPC.
Not in the ASI at all: A (25 atomic-load sites of notes\mainthread-drawprep.md, including A3 in DrawEmit).

## 12. Ally A/B plan (stock environment, game-side switches only)
Same save, same spot as the v71 capture (MAX, FG off), MangoHud CSV (cpu_power, gpu_power, gpu_core_clock, fps) + the
mod log (SP, `stats`), 3 min per leg, ABA order.

1. **GF** (1.4.0 Ally zip): menu GF off / on / off. Expect: main-thread `[ntdll] <- 1180A4` 58 % -> ~0 **and** all
   `<- 4E390` rows -> ~0 together (proves section 3.2); [App]Game busy ~97 % -> ~35 %; CPU W down, GPU MHz up.
2. **Tick rate (TG proxy)**: in-game FPS cap 60 vs 30 (GF on). Expect: `game ticks` 44.6 -> 30, `speed` ~1.0x,
   KIDSTaskScheduler/KIDSTS RIP-in-game thread-equivalents -30 %, `stats wakes/s` -30 %, CPU W down by ~2-4 W, GPU MHz
   and fps up. Also note TS input->display and how the game feels (dodges, parries).
3. **W3_TimeoutMs** 1 vs 3 (ini, restart): Linux `top -H` or the thread script: the process's first thread 11 % -> ~4 %.
4. **AABB / J1 / M2 / M1** on vs off (existing switches): Scheduler0 and worker shares.
5. Next build: **TG** auto (floor 30) vs the fixed cap-30 leg; **WG** off / on during the first minutes after loading
   into the level (the storm window) and in steady play: `stats wakes/s` (wake-one, wasted, WG skipped), Scheduler0 C->L,
   fps p1.
6. Optional: IATS, DS, RL.

## 13. Files
- `re\scripts_cpu\sp_aggregate.py` - SP log aggregator (any future Ally log).
- `re\scripts_cpu\cpu_hotspots_check.py` - offline proof of the sites and register contexts above.
- `re\scripts_cpu\sleep_classify.py` -> `re\out\sleepclass.txt` - idiom of every Sleep/SleepEx call site.
- `re\symbols\incoming\cpu-hotspots.tsv` - 20 symbol rows (IsFenceComplete, GfxFencePollWorker, ResourceWidthBelow,
  MtxLock/MtxUnlock, XtimeGetTicks, CndWaitSrw, MemcpyErms, the wake-site labels, RL/DS/ring-full labels, ...).
