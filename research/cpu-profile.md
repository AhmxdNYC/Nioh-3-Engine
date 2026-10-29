# Nioh 3 CPU profile

## TL;DR (2026-10-06, two 30 s in-game captures at 14:16 and 14:22, gameplay, 60 fps lock)
The process uses **8.4-8.5 cores**. There is no single dominant spin loop. The main thread **[App]Game is CPU-bound (92 % of a
core, 89 % busy)**. The rest is spread over ~110 job/worker threads, and most of them block properly. Concrete,
patchable waste found (ranked by expected win):

| # | what | where | cost | fix | effort |
|---|---|---|---|---|---|
| **W1** | **read-modify-write to WRITE-COMBINED (GPU upload) memory**: `and dword [rbx+rax*4],0` in a zero-fill loop, so every store does an uncached read | Nioh3+61B1C loop 61BAE..61BC2 (main thread) | **11-22 % of the main thread** (#1 hotspot) | in-place 22-byte rewrite to `rep stosd` (below) | trivial, no cave |
| **W2** | same bug in a quad/vertex writer: `and dword [rbx+8],0` (z=0) x4 into a WC vertex buffer | Nioh3+1371D0 (6 callers) | 3-6 % of main thread | in-place: fold z=0 into a `movq` store (below) | trivial, no cave |
| W3 | WinMain message pump never waits: `PeekMessageA` loop + per-iteration call to 47990 | Nioh3+1A6BF18..1A6BF84 (main thread starts at EP 52C1310, SteamStub .bind; the profiler's "38E1290" is past the end of .text) | **0.26-0.62 core**, 18-44 % kernel | `MsgWaitForMultipleObjectsEx(0,0,1,QS_ALLINPUT,MWMO_INPUTAVAILABLE)` on the empty path (cave) | small |
| W4 | frame limiter = Sleep(0)+job-poll busy-wait until the frame deadline | Nioh3+5ACF80 tail -> cold 19DF57E -> 916EBC (KIDSTaskScheduler0) | up to ~0.4-0.7 core **when there is slack** (shrine test: 40 % of Scheduler0 samples in NtDelayExecution); ~0 when CPU-bound | cave at call 19DF583: `Sleep(1)` while more than ~3 ms remain | small |
| W5 | FSR3 FG presenter pacing = pure QPC spin | Nioh3+370F578 (single caller 3777A40) | ~0.4 core with FG on | hybrid sleep-then-spin cave | small |
| W6 | `while(flag) Sleep(0)` on main thread | Nioh3+7D9E90 (4 callers: 3D652B, 7D9B91, 2EE090F, 2EEC4F9) | 2.5 % main thread | Sleep(0) -> WaitOnAddress / event (later built as mod option W6: no measured fps effect in a normal setup, see docs/BENCHMARKS.md) | small |
| W7 | job-queue polling uses `lock cmpxchg [x],0` as an atomic *load* (cache-line ping-pong) | Nioh3+411C0 / 41050 / 40800 (all KIDSTS workers) | ~0.15-0.2 core + contention | needs cave/rewrite | medium |

The "cost" column is a share of thread samples or of a core, not an fps gain. The W1/W2 fixes were later measured
together with other early engine patches (real-time frame step, spin fixes): about +2.5 % fps on PC over only two
on/off pairs (docs/BENCHMARKS.md, section (a)).

These did not matter in these captures: audio (SoundSystemUpdateThread 2.7 %, XAudio2 <1 %), input
(InputUpdateWorker 3-4 %), USB/device polling (DINPUT8 only appears under the pump thread's overlay hook).
Collision (KIDSTS[s9:CollisionUpdate] x9 = ~1 core) is mostly lower_bound binary searches over flat sorted 16-byte
arrays (Nioh3+D8D90, D8740 x2) plus inlined std::map finds in D80B0 and one std::map walk 19CED0; D8B70 has no map. That is
algorithmic, not a spin.

### W1 patch: Nioh3+61BAC (22 bytes, verified with `decode.fsx`)
```
orig 61BAC: 33 C9 41 0F B6 06 41 03 C7 03 C1 FF C1 83 24 83 00 41 3B CF 72 EC
     xor ecx,ecx / L: movzx eax,[r14]; add eax,r15d; add eax,ecx; inc ecx; and dword [rbx+rax*4],0; cmp ecx,r15d; jb L
new  61BAC: 41 0F B6 06 41 03 C7 48 8D 3C 83 44 89 F9 31 C0 F3 AB 0F 1F 40 00
     movzx eax,[r14]; add eax,r15d; lea rdi,[rbx+rax*4]; mov ecx,r15d; xor eax,eax; rep stosd; nop4
```
The memory effect is the same: r15d contiguous dwords are zeroed. rdi/rcx/rax are dead there: edi is reloaded at 61BD9,
the loop is guarded by `test r15d,r15d` at 61B96, and no branch targets land inside 61BAE..61BC1. Probe proof
(`ripprobe.fsx`): at RIP 61BBD, rbx -> `RW+WRITECOMBINE PRIVATE` (4/4 hits).
Call chain: 6224D (in 62000) <- 61EA0 <- 1CE818 <- 253C68/254440.

### W2 patch: Nioh3+1371D0 (4 sites, verified)
A `movss xmm,[mem]` load zeroes lanes 1-3, and `addss/subss` touch only lane 0. So `movq [y-slot],xmm` writes y and z=0 together:
```
13721F: 83 63 08 00 80 7C 24 58 00 F3 0F 11 43 04  ->  80 7C 24 58 00 66 0F D6 43 04 0F 1F 40 00
137269: 83 63 34 00 80 7C 24 58 00 F3 0F 11 4B 30  ->  80 7C 24 58 00 66 0F D6 4B 30 0F 1F 40 00
1372B4: 83 63 60 00 80 7C 24 58 00 F3 0F 11 4B 5C  ->  80 7C 24 58 00 66 0F D6 4B 5C 0F 1F 40 00
1372FE: 83 A3 8C 00 00 00 00 48 8B 6C 24 38 48 8B 74 24 40 F3 0F 11 8B 88 00 00 00
     ->  48 8B 6C 24 38 48 8B 74 24 40 66 0F D6 8B 88 00 00 00 0F 1F 80 00 00 00 00
```
The `cmp byte [rsp+58h],0` stays, so the following `jne`s see the same flags. Cold paths 190A2B4/C0/CC re-enter
at 137208/137251/13729C, outside the patched bytes. Probe: rbx -> `RW+WRITECOMBINE` (3/3 hits).
**How to find more of these:** profile -> hot RIP right after an `and/or/add [mem],imm` -> `ripprobe.fsx` ->
check for WRITECOMBINE.

## Threads (capture 2, 14:22, 30 s, 260 k samples @ 8.7 kHz)
| TID | role | CPU % | kern % | notes |
|---|---|---|---|---|
| 21452 | [App]Game: main/game thread (frame fn 1A74130 <- 27049CC <- 134EBD0) | 92.6 | 17.8 | CPU-bound, 89 % busy; W1/W2/W6 live here |
| 21760 | KIDSTaskScheduler0: frame step 5ACF80 + jobs | 83.0 | 4.9 | 14 % short SRW-condvar waits; frame limiter (W4) |
| 2472 | SynergisticDispatcherTask: D3D12 submission (amdxc64 26 %, D3D12Core 6 %) | 49.4 | 1.4 | 47 % blocked |
| 25244 | WinMain thread (start EP 52C1310; profiler showed 38E1290, past .text): message pump | 43.0 | 29.7 | W3: PeekMessage 60 %, NtYieldExecution, Steam overlay hook |
| 21920,5388,29344,10064,29292,20500 | KIDSTaskScheduler6..1: job workers | 22-32 each | 1-6 | ~70 % blocked, the rest is real job work |
| 9 threads | KIDSTS[s9:CollisionUpdate] | ~96 total | | binary search on flat arrays (D8D90/D8740) + std::map finds inlined in D80B0 |
| 8 threads | KIDSTS[s2:StageUpdate] | ~82 total | | |
| 10 threads | [App]EffectUpdateThread | ~43 total | | |
| 10 threads | KIDSTS[s66:TS_MESHLET_REQ] | ~38 total | | some Sleep(0)/Sleep(1) (0.12 core in NtDelayExecution) |
| 1 each | MissionThread 18, KTGLGraph.Cache.FileLoad(+Poll) 8+9, CustomOccluderRayCast 7, Input 3, Sound 3, ExecuteCommandLists 3.5 | | | |

Totals by family: KIDSTaskScheduler x7 = 246 %, CollisionUpdate 96 %, [App]Game 92 %, StageUpdate 82 %,
(unnamed) x68 = 58 %, Dispatcher 57 %, EffectUpdate 43 %, MESHLET 38 %.
The FSR Presenter thread does not appear in these captures (FG was apparently off this session). In the 13:43 test it was at 43 %.

## Earlier evidence (test capture + static analysis)

Tool: `cpuprof.fsx` (external SuspendThread/GetThreadContext sampler, dotnet fsi, no admin).
`wpr -start CPU` not usable: session is not elevated (kernel logger needs admin) — sampling used instead.
Helper: `dis.fsx <rva>[:lenHex] ...` disassembles from the dump with import names resolved.

## Preliminary — 5 s test capture, 2026-10-06 13:43 (in gameplay, idle at shrine, 60 fps lock, FSR FG)
Process total **647 % = 6.5 cores**.
Caveat: this run used stack *scanning* for callers (stale slots possible; e.g. a Sleep caller was mis-attributed
to Nioh3+48230). Profiler since upgraded to real .pdata unwinding.

| TID | thread name | CPU % | kern % | what it does (from samples) |
|---|---|---|---|---|
| 4072 | [App]Game (main) | 96.5 | 34 | 32 % blocked in WaitForSingleObject (from Nioh3+167D70); QPC polling in Nioh3+1A74130; rest = game logic |
| 8812 | KIDSTaskScheduler0 | 80 | 23 | 40 % of samples in NtDelayExecution (Sleep) via Nioh3+916EBC / Nioh3+41050 |
| 27908 | (unnamed, start shown as Nioh3+38E1290, an address past .text; real start EP 52C1310) | 62 | **44** | **busy message pump**: 83 % NtUserPeekMessage + 13 % NtYieldExecution, under DINPUT8 / gameoverlayrenderer64 hooks; Nioh3 frame Nioh3+1A6BD18 |
| 14728 | AMD FSR Presenter Thread | 43 | 2 | FSR frame-gen pacing |
| 13228 | SynergisticDispatcherTask | 28 | 10 | render submission (amdxc64/D3D12Core), 72 % blocked |
| 14304/4756/4388/27572/25812/13816 | KIDSTaskScheduler1-6 | 15-22 each | low | job workers |
| many | KIDSTS[s9:CollisionUpdate]n, [s2:StageUpdate]n, [s66:TS_MESHLET_REQ]n, [App]EffectUpdateThread ×10 ... | 2-11 each | | ~150 more threads <0.5 % |

### First spin-wait evidence (static disassembly)
- **Sleep(0) spin loops** in Nioh3+1A74130: `1A74216: xor ecx,ecx; call [Sleep]; ...; call 877148; test al,al; je 1A74216`
  and the same pattern at 1A742B6 (spin on 1A6030). Nioh3+1A6BCA4 is a `Sleep(0)` thunk (`xor ecx,ecx; jmp [Sleep]`).
- Thread 27908 = PeekMessage + SwitchToThread/NtYieldExecution loop at ~62 % of a core while idle → should block in
  MsgWaitForMultipleObjects.

## Static findings (dump disassembly, 2026-10-06 ~14:05) — spin-wait hypothesis
Tools: `dis.fsx`, `xref.py <rva>...` (direct call/jmp xrefs), `spinscan.fsx` (Sleep/Yield/QPC/PAUSE census).

**Census** (spinscan output): **319 call sites of `Sleep(0)`**, 41 `Sleep(1)`, 3 `SleepEx(0)`, 2 `SwitchToThread`
thunks (B62081, C5B6ED), 146 WaitForSingleObject, 81 QueryPerformanceCounter, 5 SleepConditionVariableSRW,
97 code fragments containing PAUSE. The engine's default "wait" idiom is `while(!cond) Sleep(0);`.
Sleep(0) only gives the core away if another ready thread of *equal* priority exists; otherwise it returns at once
→ every waiting thread burns a full core in user+kernel (matches the high kern% on KIDSTaskScheduler0).

### S1 — job-system wait = Sleep(0) spin (KIDSTaskScheduler*, all "help while waiting" sites)
```
Nioh3+916EBC  JobWaitStep(sched, bool):
    if (JobTryRunLocal 40680(...)) return;          // pop own queue
    if (JobTrySteal  41050(sched+440,...)) return;  // walk other queues via 411C0
    Sleep(0);                                    // 916F12: xor ecx,ecx ; call [Sleep]
Nioh3+916E60  JobWaitCounter(c, sched):  while (atomic(c+2C) & 1) JobWaitStep(sched,1);
callers of 916EBC: 916E98 (above), 19DF583 (worker loop in 5ACF80, then QPC via 205530), 1A27CF9 (loop, cnt<0x400)
```
- Queue polling (411C0/41050) reads queue heads with **`lock cmpxchg [x],0`** as an atomic *load* → every poll takes
  the cache line exclusive; 7+ spinning workers ping-pong the scheduler's lines between cores (expensive + slows the
  thread that actually owns the work).
- Fix options: (a) after N empty polls, block on an event/semaphore/WaitOnAddress that the job-push path signals
  (needs the push site — scheduler shows ZwSetEvent + NtWaitForAlertByThreadId samples, so a wake path exists);
  (b) cheap patch: replace `Sleep(0)` at 916F12 with `Sleep(1)`/SwitchToThread+PAUSE back-off (risk: up-to-1 ms latency
  on job completion waits inside a 16.7 ms frame — must A/B with PresentMon); (c) replace `lock cmpxchg` loads with plain
  `mov` (x64 loads are already acquire) — code-size-identical patch in 411C0/41050 is not possible byte-for-byte, needs a cave.

### S2 — WinMain thread: pure PeekMessage busy loop (thread 27908, ~62 % core, 44 % kernel)
`Nioh3+1A6BD18` creates "[App]Game" then pumps forever:
```
1A6BF18: PeekMessageA(&msg,0,0,0,PM_REMOVE); if (got) { if WM_QUIT break; Translate; Dispatch; }
         47990(g_45CC560, ...)    // CS enter/leave + cursor/UI-mode check, cheap
         jmp 1A6BF18              // never waits
```
User32 itself throttles empty PeekMessage with NtYieldExecution (the 13 % ZwYieldExecution samples).
Fix: on the empty path (`je 1A6BF54` at 1A6BF35) call `MsgWaitForMultipleObjectsEx(0,NULL,1,QS_ALLINPUT,MWMO_INPUTAVAILABLE)`
(1 ms cap keeps 47990's polling cadence; messages wake it instantly) via a code cave. Expected: ~0.6 core freed, no latency cost.

### S3 — Sleep(0) spin loops on the main thread
`Nioh3+1A74130` (called from 27050D1 and 32471BC; QPC-timed): `1A74216: do { Sleep(0); } while(!877148(...))` and
`1A742B6: do { Sleep(0); } while(!1A6030(...))`. 877148 has 16 callers — a "try-acquire" primitive used with Sleep(0) spin.

### S4 — THE FRAME LIMITER IS A Sleep(0) BUSY-WAIT (KIDSTaskScheduler0, ~80 % core, 23 % kernel)  (scene-dependent: only costs when the frame has slack; see W4)
`Nioh3+5ACF80` is the per-frame step that runs on KIDSTaskScheduler0 (thread 8812 in the test: 40 % of samples in
NtDelayExecution ← SleepEx ← 916EBC ← 19DF588/5ACF80). Its pacing tail:
```
5AD229: div = max(1, [r15+30h]);  freq = [g_45BA780]->+18h (QPC freq);  ticksPerFrame(rdi) = freq / div
        last(r12) = [r15]
5AD292: now = 205530()            // QPC wrapper
        if (now > last && now - last < ticksPerFrame)  -> cold block 19DF57E:
19DF57E:    do { 916EBC(sched, 0);  // run/steal one job, else Sleep(0)
                 now = 205530(); } while (now > last && now - last < ticksPerFrame)
5AD2BB: [r15] = now;  frameDelta = (now-last)/freq ...
```
So every frame the remaining time to the 16.67 ms deadline is burned in a Sleep(0)+queue-poll loop
(same design as Nioh 1's post-Present busy-wait). In a light scene at a 60 fps lock that is most of the frame.
**Patch idea (small, safe):** redirect `call 916EBC` at **19DF583** (5 bytes) to a cave:
```
; in: rsi=sched, rdx=now, r12=last, rdi=ticksPerFrame
remaining = rdi - (rdx - r12)
if (remaining > freq/333 /*~3 ms*/)  Sleep(1)      ; game already calls timeBeginPeriod
else                                 916EBC(rsi,0) ; original fine-grained spin for the last ~3 ms
```
(or a high-resolution waitable timer for `remaining - 1 ms`). Expected: ~0.6-0.7 core freed with identical pacing.

### S5 — FSR3 frame-generation presenter spins on QPC (AMD FSR Presenter Thread, ~43 % core)
Test capture: 38 % of that thread's samples in RtlQueryPerformanceCounter from **Nioh3+370F578**:
```
370F578 FsrFiWaitUntilQpc(t):  do { QueryPerformanceCounter(&now); } while (now < t);   // no pause, no sleep
single caller: 3777A40 (presenter loop in 37778B4): FsrFiWaitUntilQpc(lastPresent + interval) between the
interpolated and the real present.
```
This is FidelityFX FrameInterpolationSwapChain's pacing wait (statically linked). Patch idea: cave at the single
call site 3777A40 → hybrid wait: while (t - now > ~1.5 ms) Sleep(1) / high-res waitable timer; then the original spin.
Expected ~0.3-0.4 core freed; pacing accuracy kept by the final spin. (Only relevant with FSR FG on.)

### Other notes from the test capture
- Workers KIDSTaskScheduler1-6: 87 % of samples blocked in WaitForSingleObject → they do sleep properly; 15-22 % CPU each is
  real job work + wake/steal overhead. The spinning ones are Scheduler0 (frame limiter, S4) and job-wait sites (S1).
- [App]Game showed 96.5 % CPU yet 32 % of samples inside NtWaitForSingleObject in this shrine test. In the 14:16/14:22
  captures it is 89 % busy with only 4 % of samples in ntdll, so the frame there is main-thread bound (W1/W2 dominate).
- WinMain pump thread (S2) is hooked by gameoverlayrenderer64 (Steam overlay) and DINPUT8 — its PeekMessage cost is
  inflated by the overlay hook (EnterCriticalSection inside the hook).


## Top 30 self functions on the main threads (capture 2). Current names for these RVAs are in symbols/symbols.tsv.
"busy" = cycle-busy fraction of the thread in the ms before the sample. It roughly equals the thread-level value and is only low for long blocks.


#### [App]Game (main) - tid 21452, top 30 self (% of this thread's wall samples)

| % | busy | function |
|---|---|---|
| 21.9 | 90.4 | `Nioh3+61B1C` |
| 3.9 | 89.3 | `Nioh3+1E80B0` |
| 3.3 | 91.4 | `Nioh3+1371D0` |
| 2.7 | 92.4 | `Nioh3+1CD690` |
| 2.6 | 87.8 | `ntdll.dll!ZwDelayExecution` |
| 2.2 | 88.0 | `Nioh3+B94250` |
| 2.0 | 88.2 | `Nioh3+2523FC` |
| 1.8 | 88.7 | `Nioh3+4AA34` |
| 1.8 | 88.3 | `Nioh3+B94230` |
| 1.4 | 87.8 | `Nioh3+1E7050` |
| 1.4 | 87.8 | `Nioh3+128EF0` |
| 1.3 | 88.4 | `Nioh3+1D84A0` |
| 1.2 | 88.0 | `Nioh3+1D8720` |
| 1.2 | 88.1 | `Nioh3+39E20` |
| 1.0 | 89.8 | `Nioh3+62000` |
| 1.0 | 87.7 | `Nioh3+4DB6C` |
| 0.9 | 89.5 | `Nioh3+1D7340` |
| 0.8 | 88.5 | `Nioh3+1D8220` |
| 0.8 | 88.4 | `Nioh3+1290B8` |
| 0.8 | 88.9 | `Nioh3+1DAF6C` |
| 0.7 | 86.7 | `Nioh3+419A8` |
| 0.7 | 93.0 | `Nioh3+418F40` |
| 0.7 | 93.2 | `Nioh3+5DF70` |
| 0.7 | 88.0 | `Nioh3+3BC0C` |
| 0.7 | 87.7 | `Nioh3+3C7750(leaf?)` |
| 0.6 | 87.8 | `Nioh3+114C10(leaf?)` |
| 0.6 | 87.0 | `Nioh3+1D7EB0` |
| 0.6 | 89.8 | `Nioh3+1E8040(leaf?)` |
| 0.6 | 88.9 | `Nioh3+1E0B64` |
| 0.5 | 89.4 | `ntdll.dll!ZwReleaseSemaphore` |

#### KIDSTaskScheduler0 - tid 21760, top 30 self (% of this thread's wall samples)

| % | busy | function |
|---|---|---|
| 13.9 | 77.3 | `ntdll.dll!NtWaitForAlertByThreadId` |
| 4.0 | 86.0 | `Nioh3+6EFD0(leaf?)` |
| 3.5 | 85.8 | `Nioh3+71C60` |
| 2.4 | 85.8 | `Nioh3+6EA50` |
| 2.4 | 76.5 | `Nioh3+66D00` |
| 2.1 | 77.6 | `Nioh3+F450C` |
| 1.9 | 80.3 | `Nioh3+411C0` |
| 1.6 | 76.7 | `ntdll.dll!ZwSetEvent` |
| 1.5 | 85.5 | `Nioh3+6F010` |
| 1.3 | 85.5 | `Nioh3+174610(leaf?)` |
| 1.3 | 84.9 | `Nioh3+54360` |
| 1.2 | 68.7 | `Nioh3+889C0` |
| 1.0 | 84.8 | `Nioh3+6F060(leaf?)` |
| 1.0 | 77.9 | `Nioh3+12F22C` |
| 1.0 | 88.2 | `Nioh3+16CFF0` |
| 1.0 | 75.3 | `ntdll.dll!NtAlertThreadByThreadIdEx` |
| 0.9 | 78.1 | `Nioh3+E32D90` |
| 0.8 | 89.7 | `Nioh3+425B0` |
| 0.7 | 78.5 | `Nioh3+605EE4` |
| 0.7 | 84.4 | `Nioh3+16BA70` |
| 0.7 | 76.1 | `Nioh3+12FBDD0` |
| 0.7 | 77.7 | `Nioh3+F33A0` |
| 0.7 | 77.6 | `Nioh3+65214` |
| 0.6 | 87.8 | `Nioh3+1A7BF0` |
| 0.6 | 89.4 | `Nioh3+121FC0` |
| 0.6 | 76.3 | `Nioh3+1FDC1C` |
| 0.6 | 87.8 | `Nioh3+121E70(leaf?)` |
| 0.6 | 69.9 | `Nioh3+13484C` |
| 0.6 | 76.0 | `Nioh3+284ED4` |
| 0.6 | 81.1 | `Nioh3+12B440` |

#### SynergisticDispatcherTask (render submit) - tid 2472, top 30 self (% of this thread's wall samples)

| % | busy | function |
|---|---|---|
| 53.0 | 35.7 | `ntdll.dll!ZwWaitForSingleObject` |
| 2.8 | 53.7 | `amdxc64.dll+196B710` |
| 1.7 | 64.3 | `Nioh3+119820` |
| 1.4 | 72.7 | `Nioh3+B94230` |
| 1.3 | 28.1 | `Nioh3+58C144` |
| 1.2 | 64.4 | `Nioh3+B94250` |
| 1.0 | 65.6 | `amdxc64.dll+46C170` |
| 0.9 | 66.2 | `amdxc64.dll+1B0190` |
| 0.9 | 50.3 | `Nioh3+1192B0` |
| 0.8 | 59.3 | `D3D12Core.dll+23EF60` |
| 0.8 | 59.8 | `D3D12Core.dll+23BF40` |
| 0.7 | 50.0 | `amdxc64.dll+18CDC0` |
| 0.6 | 63.9 | `Nioh3+189BFB0(leaf?)` |
| 0.6 | 51.5 | `amdxc64.dll+474B50` |
| 0.6 | 74.4 | `Nioh3+1164C4` |
| 0.6 | 50.0 | `amdxc64.dll+3B5790` |
| 0.5 | 67.4 | `Nioh3+115B9C` |
| 0.5 | 47.4 | `ntdll.dll!RtlEnterCriticalSection` |
| 0.5 | 69.5 | `Nioh3+114A20` |
| 0.5 | 66.8 | `amdxc64.dll+462890` |
| 0.4 | 51.1 | `amdxc64.dll+208230` |
| 0.4 | 48.0 | `D3D12Core.dll+18A80` |
| 0.4 | 27.0 | `Nioh3+815DB0` |
| 0.4 | 68.4 | `amdxc64.dll+454C90` |
| 0.4 | 68.4 | `Nioh3+118E28` |
| 0.4 | 64.0 | `amdxc64.dll+44E390` |
| 0.4 | 55.8 | `amdxc64.dll+44FF80` |
| 0.4 | 71.5 | `Nioh3+116260` |
| 0.3 | 47.6 | `amdxc64.dll+52ABA0` |
| 0.3 | 59.9 | `amdxc64.dll+206E40` |

## Tooling (private dev repo; dotnet fsi / py, no admin, nothing injected)
- `cpuprof.fsx <sec> <out> [intervalUs=1000] [minCpu%=2] [maxThreads=24] [stackBytes=32768]`
  - Per-thread CPU from GetThreadTimes and QueryThreadCycleTime.
  - Samples the busy threads one at a time at 8+ kHz: Suspend -> GetThreadContext -> 4 KB stack read -> Resume, in try/finally, plus a ProcessExit/Ctrl+C resume guard.
  - Real x64 .pdata unwinding: 98-99 % of samples reach the thread start.
  - Views: self, caller, first-Nioh3-frame and inclusive. Per hot function it also shows static spin flags (PAUSE/lock/Sleep/QPC imports) and tight-loop detection. Exports a `.tsv`.
  - Memory stays small (aggregation is online).
  - Hold time (thread actually stopped): p50 <=10 us, p99 <=15 us, max 592 us. 0.1 % of samples exceed 50 us; the cause is our own sampler thread being preempted on a fully loaded CPU.
  - The first run had a single 32 ms hold, caused by first-call P/Invoke stub JIT. A warm-up now fixes it.
  - "delivery" (suspend -> context) can be long when the target is in a kernel critical region. The target is not stopped during that time.
  - To test on another process: `CPUPROF_PROC=x CPUPROF_PID=<pid>`.
- `dis.fsx <rva>[:lenHex]...`: disassembly from the dump, with import names resolved.
- `decode.fsx <ip> "<hex>"`: verify patch bytes.
- `xref.py <rva>...`: direct call/jmp xrefs.
- `spinscan.fsx`: census of Sleep/Yield/QPC/PAUSE sites.
- `ripprobe.fsx <tid> <lo> <hi>`: catches a thread inside a RIP range and reports the memory type (WRITECOMBINE/NOCACHE) of the addresses its registers point to.
- wpr/ETW: `wpr -start CPU` needs elevation and this session is not admin, so it was not used.

## Caves: `cpu_caves.fsx` (2026-10-06; dry-run checked against the live game only, NOT applied)
Goal: power and boost headroom on this PC and on power-limited handhelds (ROG Ally X, Steam Deck). On an APU every watt
a spinning core burns comes out of the GPU's shared budget.

| id | site | change | expected saving | when it pays |
|---|---|---|---|---|
| W3 | `1A6BF2D` `FF 15 <IAT PeekMessageA>` -> `90 E8 <cave>` | PeekMessageA. If the queue is empty, `MsgWaitForMultipleObjectsEx(0,NULL,1ms,QS_ALLINPUT=0x4FF,MWMO_INPUTAVAILABLE)` | 0.25-0.6 core (pump thread was 26-62 % CPU, 18-44 % kernel) | always |
| R1 | `4A5346` `EB 02 F3 90 33 D2` -> `8B 15 <cave.r1Timeout>`, `4A5357` `75 EF`->`75 ED` | `while (WaitForSingleObject(h, 2ms))` instead of `pause; WaitForSingleObject(h,0)` | up to ~0.3 core of the main thread (shrine test: ~32 % of [App]Game samples in this poll, 34 % kernel); ~0 when CPU-bound | GPU-bound / capped frames, which is the handheld case |
| L1 | `19DF57E` `33 D2 48 8B CE E8 <916EBC>` -> `E8 <cave> nop5` | while more than 1.5 ms remain to the limiter deadline: wait on a high-resolution waitable timer until deadline-1 ms; then the original job/Sleep(0) spin | ~0.3-0.5 core of KIDSTaskScheduler0 when frames have slack (test: 40 % of its samples in Sleep(0)); 0 when CPU-bound | capped fps below what the CPU can do |
| W6 | `7D9E90` `while(flag) Sleep(0)` | **not patched** here (later built as mod option W6; no measured fps effect in a normal setup, see docs/BENCHMARKS.md) | (2.5 % of main thread) | the waits are ~0.4 ms on the bottleneck thread and no waker exists. Anything coarser than Sleep(0) adds main-thread latency |

How the caves work:
- **API resolution:** every API call goes through the game's IAT. Slots come from the dump's import table and are checked live in dry mode.
  MsgWaitForMultipleObjectsEx and CreateWaitableTimerExW are not imported. They are resolved once at runtime with
  GetModuleHandleW and GetProcAddress (both IAT). If resolution or timer creation fails, a fail bit is set and the patch falls back to stock behaviour
  (L1 disables itself).
- **Runtime switches** (cave data, no code writes): `enW3`, `enL1`, and `r1Timeout` (0 = stock-equivalent poll). Counters: W3 waits and L1 timer sleeps.
  `status` prints them per second.
- **Thread safety:** writes happen in `t.Frozen`. While frozen, each thread's RIP is checked against the patched ranges, and the write
  only happens if every RIP is at an instruction start valid in both encodings (retries up to 100 times).
  Return addresses are boundaries in both encodings:
  - W3 returns to 1A6BF33.
  - L1 returns to 19DF583 (the original call starts there; inside the patch that byte is the start of the nop5).
  - Threads in the original PeekMessageA / 916EBC return to 1A6BF33 / 19DF588.
  So a revert while the pump thread is blocked inside MsgWait, or Scheduler0 inside the timer wait, is safe.
- **Overlap:** no overlap with W1/W2 or the ft_vrr sites. The script checks against the ft_vrr site list.

Risks:
- **W3:** the Steam overlay inline-hooks PeekMessage, and it is still called each loop. 47990 (cursor/IME state) now runs at 1 kHz,
  or at the system timer resolution, instead of continuously. Window messages wake the pump at once. Pad input is polled by
  InputUpdateWorker and is unaffected.
- **R1:** the exit condition is identical (eax==0). A signal wakes the wait immediately, so the 2 ms timeout only sets the re-poll cadence.
  WAIT_FAILED loops in both versions.
- **L1:** frame pacing depends on high-resolution timer accuracy (about 0.5 ms on Win10 1803+/Win11); the 1 ms margin covers that.
  Check PresentMon frametime stdev and 1 % lows. Scheduler0 no longer runs jobs during the sleep part. That only happens while
  there is slack, but it could lengthen a frame if workers are saturated. Proton/Wine: if CREATE_WAITABLE_TIMER_HIGH_RESOLUTION is
  refused, L1 turns itself off.
- **Tunables:** W3_MS, R1_MS, L1_MARGIN_US, L1_MIN_US at the top of the script. Margin and min are also live in the cave (qwords).

Commands:
```
dotnet fsi cpu_caves.fsx dry                       # read-only: bytes, IAT slots, cave + site disassembly
$env:FT_ALLOW_WRITE=1; dotnet fsi cpu_caves.fsx apply W3,R1,L1   # ONE apply per game process (one cave)
dotnet fsi cpu_caves.fsx status                    # flags, timer handle, waits/s
dotnet fsi cpu_caves.fsx off L1 ; ... on L1        # A/B without touching code (R1 off = timeout 0)
dotnet fsi cpu_caves.fsx revert W3,R1,L1           # original bytes back (cave stays allocated)
```
