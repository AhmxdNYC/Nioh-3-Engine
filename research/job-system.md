# KIDS job system: push / wake / wait, and what it costs

Static analysis only (dump disassembly, Ghidra decompiles, the r5 / r6 / r7 / r9 / ingame sampling profiles). Nothing was
attached, read live, written or applied. Date: 2026-10-07.
Scripts and raw outputs stay in the private dev repo. The live script is `jobs_live.fsx`; its `offline` run prints
**OFFLINE CHECKS: ALL PASSED**. The symbols named here are in symbols/symbols.tsv.

## TL;DR

1. **The desktop main thread's kernel time is not job dispatch.** In all 12 captures, [App]Game has no samples in
   ZwSetEvent, NtAlertThreadByThreadId, NtWaitForAlertByThreadId or a job-related ReleaseSemaphore. It has no samples in
   the job code either (403A0 / 3E7CC / 40800 / 411C0). A job push (3FAD8 / 417A0) **never** makes a syscall: the wake
   happens on the *pop* side. So no job-system patch can raise desktop fps. §3 lists what the 17.6 % actually is.
2. **The wake policy is "wake one on the first pop, unconditionally".** Each woken worker wakes one more on its own first
   pop, so wakes run in a serial cascade, and every cascade ends with at least one wasted wake. A one-job group (push one
   job, then pop it yourself) always wakes a worker that finds nothing. **WK** (built) gates the wake on "is anything
   still queued in this pool".
3. **There is no spin before blocking.** A worker scans the other queues once and then sleeps on its event. Candidate (b)
   is therefore moot in both directions (§2b).
4. **WorkerCap (WC, built)** is a 2-byte boot-time patch in SizeWorkerPools. On a 16-thread APU it cuts the 14 weighted
   thread pools from **127 to 63 threads** (cap 8), 78 (cap 10) or 99 (cap 12). This was proven by running the real
   function bytes copied from the dump.
5. **(d) the 404B9 Sleep(0)** has no safe "better wait": nothing ever signals the group counter. It is 0 % of the main
   thread and ≤1.4 % of KIDSTaskScheduler0. Not built.

| rank | id | what | desktop main thread | desktop process | handheld (Ally, Proton) | risk | status |
|---|---|---|---|---|---|---|---|
| 1 | **WC** | WorkerCap: base clamp 32 → cap at 1FE300B / 1FE3011 (boot) | 0 (do **not** use on the 7600X) | – | **−64 threads at cap 8** (127→63 in the weighted pools). KIDSTaskScheduler 13→6, Collision 12→6, EffectDisp 12→6. Expect a large drop in KIDS context switches; power is to be measured. | low–medium: less parallel width if the APU becomes CPU-bound; boot race like P12 | built, offline-proven |
| 2 | **WK** | wake gate at 40A4C: skip SetEvent when no queue in the pool has work | 0 | removes the wasted share of ~0.06 core of SetEvent + the woken side; est. 0.02–0.08 core | fewer futile wake/sleep pairs (ntsync); `status` measures the skip ratio live | low: on the skip path the state equals the stock flag==0 path (proven) | built, offline-proven |
| – | J1 | plain loads instead of `lock cmpxchg [m],0` in the poll paths | 0 | 0.05–0.1 core | same, and fewer lines bouncing between cores | low | already built: `workers_live.fsx` (worker-jobs-opt.md) |
| – | (b) | spin before blocking | 0 | negative | negative (power) | – | rejected (§2b) |
| – | (d) / Y1 | 404B9 Sleep(0) → other wait | 0 | ≤0.01–0.03 core | ~0 | medium | not built (§2d) |
| – | D1 | coalesce the render-ring ReleaseSemaphore (main thread) | ≤0.5–0.9 % of the main thread | – | – | medium: producer and consumer both change | documented (§3) |
| – | WC2 | cap TS_MESHLET_REQ (`dwNumberOfProcessors-2`, 14 threads on the Ally) | 0 | – | −4…−8 threads | medium (streaming throughput) | documented (§2c) |

---

## 1. The paths (exact RVAs)

### Data structures
- **JobPool** (one per pool):
  - `+10h` worker array, stride 0x458;
  - `+28h` / `+2Ch` worker count (the second is read atomically);
  - `+30h` idle-worker count;
  - `+34h` pool id;
  - `+48h` state (`&50000000h == 50000000h` means paused).
- **JobWorker** (0x458 bytes):
  - `+0` WorkerThread*;
  - `+418h` 128 priority lists (lock-free stacks, self-relative index + ABA tag);
  - `+428h` deferred list;
  - `+440h` pool;
  - `+448h` → 4-dword priority bitmap;
  - `+450h` "solo pop" counter;
  - `+454h` no-sleep flag.
- **WorkerThread**:
  - `+8` auto-reset event;
  - `+10h` bit0 = "sleeping, wake me";
  - `+1Ch` pending SetThreadPriority request;
  - `+28h` handle.
- **JobGroup** (counter):
  - `+8` owner worker;
  - `+34h` pending = jobs + 1 for the waiter.

### Push: never wakes
- **JobPushToOwner 3FAD8** (212 callers) and **JobPushLocal 417A0** (16 callers): CAS-push onto `w+418h[prio]`, then
  CAS-set the bit in `w+448h`. That is user mode only, with no syscall and no wake check.
- The main thread's pushes therefore cost no kernel time.
- **JobForwardToWorker 48DF8** is the exception: it hands a job whose worker mask (`job+22h`) excludes the current worker
  to a specific worker, and SetEvents that worker.

### Wake: on pop (JobWorkerPoll 40800)
After a successful pop, at 40A01:
```
idle = pool+30h ; flag = *wakeFlag (param_3)
if (idle == count-1) { if (++w[450h] % 30 == 0) goto wake; }   // I am the only awake worker: every 30th pop
else w[450h] = 0;
if (flag) goto wake;  else skip;
wake (40A4C): for i = me-1 downto (ring): t = workers[i].Thread;
              if (t && t+10h bit0) { CAS-clear bit0; SetEvent(t+8) @40B0A; break; }
40B10: *wakeFlag = 0
```
- At most **one** SetEvent per pop, and only to a worker that announced it is sleeping (no redundant SetEvent).
- The flag starts at 1 in every *session*: once per JobGroupWait call (3E7CC `[rsp+48h]=1`), per JobWaitSleepBackoff
  207200 call, and per JobWorkerRunUntilIdle 3FE14 call (the first pop after each wake).
- Result: **per job, cascaded.** The waiter's first pop wakes W1. W1 steals a job, and its first pop wakes W2, and so on,
  each step one wake latency apart.
- The cascade has no idea how much work is left. It stops at the first woken worker that finds nothing, and that wake was
  wasted.
- Second wake site: **40D2F**. For job type 616FA0 with `pool+34h != 0`, the same wake-one scan runs, followed by
  JobWaitSleepBackoff.

### Worker sleep: JobWorkerThreadMain 1A6BB84
```
loop { if (!JobWorkerRunUntilIdle 3FE14(w)) exit;   // steal 1 job (411C0 over w+1..), pop/run with wakeFlag=1, last steal scan
       lock inc pool+30h; if paused: Sleep(1) loop;
       CAS thread+10h |= 1  (1A6BC15);  WaitForSingleObject(thread+8, INFINITE) (1A6BC35);
       lock dec pool+30h }
```
- **No spin, no pause, no SwitchToThread.** There is one scan of the other queues, then the worker blocks.
- Syscalls on this path: WaitForSingleObject (NtWaitForSingleObject; ntsync under Proton) on the sleeper's side, and
  SetEvent (NtSetEvent) on the waker's side.

### Wait (join): JobGroupWait 3E7CC → JobHelpStep 403A0
```
while (g+34h > 1) JobHelpStep(owner, g, &flag, i < 0x4000 ? mode : 2);
JobHelpStep: 40800 pop/run own queue; re-push deferred; steal 1 (411C0 over other workers);
             nothing found && !(w+454h & 1) -> Sleep(0) @404B9 (xor ecx,ecx @404B7)
```
- This is a help-while-waiting spin. Each empty poll is one Sleep(0) syscall.
- On Windows, Sleep(0) yields only to a ready thread of equal priority. Under Wine it becomes NtYieldExecution, i.e.
  `sched_yield`.
- The same idiom appears in two other places:
  - JobWaitSleepBackoff 207200 (Sleep(0) at 2072FB, 90 callers);
  - JobWaitStep 916EBC (916F14, the L1 limiter path).
- KTGL "signal until awake" loops 3EB38 / 3FD10 call SetEvent repeatedly, with a Sleep(0) every 256 tries.

### Profile numbers (desktop, r5 unless stated)
- ZwSetEvent: Scheduler0 1.5 %, workers 0.2–0.6 % each, TS_MESHLET_REQ up to 1.8–2.6 %. Total 441 samples, about
  **0.056 core** process-wide.
- NtAlertThreadByThreadIdEx: about 1 % of Scheduler0 (KtglEventSet condvar wakes, frame handshakes).
- [App]Game: none of these.
- ZwDelayExecution (Sleep(0)): [App]Game 5.9 %, Scheduler0 1.4 %, TS_MESHLET_REQ9 9–12 % (117D6C fence poll + 404B9).

---

## 2. Candidates

### (a) WK: wake gate (built)
- **Site.** 40A4C `49 8B 8F 40 04 00 00` (`mov rcx,[r15+440h]`, the first instruction of the wake block) becomes
  `E8 <cave> 66 90`.
- **Cave** (Iced; data at +0, code at +C0h):
  1. `pushfq`, then save rax, rdx, r8 and r9.
  2. If the switch is off, take the keep path.
  3. For i in `0 .. [pool+28h]-1`: if `[w_i+448h] != 0` and the OR of its 4 bitmap dwords is non-zero, take the keep path.
  4. **Skip path:** count it; set the return address += 0BFh (40A51 → 40B10, which stores 0 to the flag exactly as the
     stock "no sleeper found" path does); set the saved r8 to 0; restore everything; `ret`.
  5. **Keep path:** count it; restore everything; execute the original `mov rcx,[r15+440h]`; `ret` (to the nop2, then
     40A53).
- **Why it is safe.**
  - The keep path is the original instruction, with every other register and the flags restored.
  - On the skip path, the state at 40B1B equals the stock `je 40B1B` path at 40A46 except:
    - rax: it holds &flag, the same as the stock wake path through 40B10;
    - the flags.
  - The walker proves **rax and the flags dead at 40B1B** (`[ok]`).
  - r8 is forced to 0, its value on the stock path. Without that, the walker found r8 reaching the job call
    `call rdx` @40E51 on some paths.
  - esi == 0 at 40A4C was proven: it is zeroed at 40A12 with no write before the site. So `mov [rax],sil` at 40B10 stores
    0 to the flag.
  - The branch-target sweep of the executable sections found nothing inside 40A4D..40A52.
- **Native harness.** 20,000 synthetic pools of 1–32 workers, with null bitmap pointers and sparse bits, the switch off in
  1/8 of cases. **0 mismatches.** Checked on every case:
  - the path taken;
  - keep: rcx = pool; r8, rdx and r9 preserved; CF preserved;
  - skip: rcx, rdx and r9 preserved; r8 = 0;
  - the counters are exact.
- **Semantics.** The only change is that a wake is not issued when no queue in the pool has a set priority bit at that
  moment. In stock, that wake reaches a worker that scans all queues and goes back to sleep.
- **Liveness.**
  - A job pushed right after the check is still run by its waiter: JobGroupWait helps until the counter drops, and
    steals from every queue.
  - Affinity jobs are forwarded with their own SetEvent (48DF8).
  - Shutdown and pause use separate paths.
  - The stock wake already happens only on pops and never on push, so the gate adds no new lost-wakeup window.
- **Cost.** About 5 loads per worker (7–13 workers), on the wake path only: the first pop per session, and every 30th
  solo pop. After J1 these are shared, read-mostly lines.
- **Expected effect.**
  - Desktop: up to the wasted share of the 0.056 core of SetEvent, plus the woken side (wake, 3FE14 scan, wait), which
    costs about the same again. Roughly **0.02–0.08 core**.
  - Ally: proportionally more, because the pools are larger and the cascades longer.
  - `status` prints wake attempts/s, kept and skipped live, so the real ratio is a 10-second measurement.
- **ASI entry:** `{"id": "WK", "group": "WK", "rva": "0x40A4C", "len": 7, "orig": "49 8B 8F 40 04 00 00", "kind": "call", "target": "caveWakeGate", "call_at": 0, "pattern": "45 84 C0 0F 84 ?? ?? ?? ?? 49 8B 8F 40 04 00 00 33 C0 F0 0F B1 71 2C", "offset": 9}`.
  The pattern is unique, and `call_at 0` + `pad 2` matches BuildRepl's "call" kind. MASM port for `caves.asm`
  (same register contract; the trampoline keeps the return address at [rsp]):
  ```
  caveWakeGate PROC
      pushfq
      push rax
      push rdx
      push r8
      push r9
      cmp  dword ptr [wk_enable], 0
      je   keep
      mov  rax, [r15+440h]
      mov  r8d, dword ptr [rax+28h]
      mov  rax, [rax+10h]
      test r8d, r8d
      jle  keepc
  scan: mov  rdx, [rax+448h]
      test rdx, rdx
      je   next
      mov  r9d, dword ptr [rdx]
      or   r9d, dword ptr [rdx+4]
      or   r9d, dword ptr [rdx+8]
      or   r9d, dword ptr [rdx+0Ch]
      jne  keepc
  next: add  rax, 458h
      dec  r8d
      jne  scan
      inc  dword ptr [wk_skipped]
      add  qword ptr [rsp+28h], 0BFh      ; 40A51 -> 40B10
      mov  qword ptr [rsp+8], 0           ; saved r8 := 0
      pop  r9
      pop  r8
      pop  rdx
      pop  rax
      popfq
      ret
  keepc: inc dword ptr [wk_kept]
  keep: pop  r9
      pop  r8
      pop  rdx
      pop  rax
      popfq
      mov  rcx, [r15+440h]
      ret
  caveWakeGate ENDP
  ```
  Put `wk_kept` and `wk_skipped` on separate cache lines (statistics only; plain inc).

### (b) Bounded spin before blocking: rejected
- There is no spin to bound. The worker blocks after a single steal scan.
- Adding a spin would not avoid wake syscalls. The waker only targets workers with `thread+10h` bit0 set, so a spinning
  worker is never chosen, and a *different* sleeper gets woken.
- The spinner would only compete for the same jobs. It costs CPU on the desktop and power on the handheld.
- The "shorter spin for handhelds" variant has nothing to shorten.

### (c) WC: WorkerCap (built, boot-time)
- **Formula.** SizeWorkerPools 1FE2FD4 runs once from CreateGameWindow (call 298DA7D), after DetectCpuTopology:
  ```
  base = clamp(LogicalCores, 6, 32) -> PoolTable+740h ; divisor = 13 (16 if CpuInfo::IsZen1) at +741h
  pool i, weight>=0: count = clamp(trunc(PoolBaseForIndex(base,i) / divisor * weight), 1, 32)
  raw <= 6 : pool 6 = 4, pool 11 = 2 ; raw 7..8 : pool 6 = 6, pool 11 = 3
  raw <= 12: pools 3 and 5 lose 3 (min 3)   <- P12 site 1FE30D5
  ```
  Here `raw` is the unclamped count in r11d. PoolBaseForIndex 1FDFEB4 returns min(base,16) for pools
  3, 6, 7, 10, 11, 13, 14, 48, 55, 56, returns 12 for pools 5, 8, 9, 16, and returns base otherwise.
- **Patch.** Two bytes:
  - `1FE300B 41 83 FB 20 → 41 83 FB cap`;
  - `1FE3011 B0 20 → B0 cap`.
  - Write B first: the in-between state then behaves as stock for ≤32 CPUs.
  - Because r11d (raw) still drives the special cases, a 16-thread APU gets no ≤12 penalty.
  - The only other readers of +740h are 2098C90 and 228D45A (`> 16` → extra init), which never fire for a cap ≤16.
- **Proof.** The real function bytes are copied from the dump. The mutex calls are stubbed, PoolBaseForIndex is copied,
  and the code runs in-process for logical counts 1–64 × Zen1 on/off × caps {6, 8, 10, 12, 16, 24, 32} × P12 on/off.
  - The results match an independent model: **1,920 runs, 0 mismatches**.
  - Cap 32 is byte-identical to stock.
  - The original code at 12 logical reproduces the dump's live table exactly.

| pool (threads) | Ally stock | cap 12 | cap 10 | cap 8 | Deck stock (8) | 7600X | 7600X+P12 |
|---|---|---|---|---|---|---|---|
| 3 KIDSTaskScheduler | 13 | 10 | 8 | 6 | 3 | 7 | 10 |
| 5 ResourceLoad | 7 | 7 | 6 | 4 | 3 | 4 | 7 |
| 6 StageUpdate | 11 | 8 | 6 | 5 | 6 | 8 | 8 |
| 7 CharacterUpdate | 11 | 8 | 6 | 5 | 5 | 8 | 8 |
| 8 RequestLoad | 8 | 8 | 6 | 5 | 5 | 8 | 8 |
| 10 CollisionUpdate | 12 | 9 | 7 | 6 | 6 | 9 | 9 |
| 11 StgGdtUpd | 6 | 4 | 3 | 3 | 3 | 4 | 4 |
| 13/14 UpdStgEdt/UpdSclEdt | 11/11 | 8/8 | 6/6 | 5/5 | 5/5 | 8/8 | 8/8 |
| 48 ProceduralUpdate | 13 | 10 | 8 | 6 | 6 | 10 | 10 |
| 56 EffectUpdateDisp | 12 | 9 | 7 | 6 | 6 | 9 | 9 |
| 9/16/55 TrySync/MiscAsync/UpdStageEffect | 3/3/6 | 3/3/4 | 3/3/3 | 2/2/3 | 2/2/3 | 3/3/4 | 3/3/4 |
| **sum of the 14 weighted thread pools** | **127** | 99 | 78 | **63** | 60 | 93 | 99 |

- **Not covered.** Fixed-count pools (BackupThread 8, KIDS_MMM_SUB_REQLOAD 32, KTGLPhysSceneWorker 4, PhysUpdate 2) and
  **TS_MESHLET_REQ**.
- **WC2 (TS_MESHLET_REQ).** Its count comes from KidsSchedulerParamsInit 3243BC0:
  `dwNumberOfProcessors-2`, so 14 threads on the Ally.
  - It could be capped by rewriting 3243BEC..3243C10 (37 bytes: `mov eax,[rsp+40h]; add eax,-2; cmp eax,1; jb →1;
    cmp eax,cap; jle; mov eax,cap; jmp 3243C11`).
  - Not built: 3243BC0 has 18 callers, and meshlet streaming throughput is at stake.
- **Recommendation.**
  - Ally / Z1 (16 threads): start at **cap 10**, then try cap 8.
  - cap 12 = the sizing of a 12-thread desktop with P12.
  - Never on the 7600X: there, cap 8 triggers the ≤12 penalty, so KIDSTaskScheduler drops to 3.
  - Steam Deck (8 threads): no effect.
- **Expected effect.**
  - Shorter wake cascades.
  - EffectUpdateDisp kick + join drops from 2×12 to 2×6 condvar wakes per frame.
  - Every steal scan touches N−1 queues instead of 12, so a scan costs about half as much.
  - Fewer runnable threads competing on 8 cores.
  - The KIDS-family voluntary context switches (~9,900/s) should fall substantially. Estimated 25–45 %, not
    measured; the `counters` mode measures it.
  - Power: estimated at a few hundred mW to ~1 W back to the GPU at 25 W. It has to be measured at a fixed fps cap.
- **Risk.**
  - Less parallel width if a scene becomes CPU-bound on the APU. Check the p99 frametime in heavy combat.
  - Boot timing: the bytes must land before 1FE2FD4 runs, which is the same race P12 already wins.
- **ASI entries:**
  ```
  {"id": "WCa", "group": "WC", "rva": "0x1FE300B", "len": 4, "orig": "41 83 FB 20", "kind": "bytes", "new": "41 83 FB 08", "pattern": "41 83 FB 20 7E 04 B0 20 EB 0A 44 3B DD 41 0F B6 C3 0F 4C C5 88 87 40 07 00 00", "offset": 0}
  {"id": "WCb", "group": "WC", "rva": "0x1FE3011", "len": 2, "orig": "B0 20", "kind": "bytes", "new": "B0 08", "pattern": "41 83 FB 20 7E 04 B0 20 EB 0A 44 3B DD 41 0F B6 C3 0F 4C C5 88 87 40 07 00 00", "offset": 6}
  ```
  - The pattern is unique.
  - WC must be handled like P12 in the decrypt-wait loop (`tryP12` in dllmain.cpp ApplyBatch): write WCb, then WCa, the
    moment both show their original bytes.
  - The imm8 comes from an ini key `WorkerCap` (6..32; 0 = off). The default should be off, and the patch should be
    skipped when LogicalCores ≤ cap.

### (d) 404B9 Sleep(0): not built
- **Data.** [App]Game never runs it. KIDSTaskScheduler0 spends 1.4 % in ZwDelayExecution in r5 and 0 % in
  ingame2 / r7, and that figure includes the limiter's 916F14.
- **No safe blocking replacement.** A blocking wait needs a waker on the group counter. Today the last job's
  `lock dec [g+34h]` signals nobody. Blocking would mean patching every completion site to wake a registered waiter,
  which is not a byte patch.
- **Pause-burst variant (Y1).** Replace the Sleep(0) with N×`pause`, keeping a periodic Sleep(0). This only turns kernel
  time into user-mode spin with the same CPU. It also drops the yield that lets a ready worker run on an oversubscribed
  machine (the Ally runs ~140 threads on 16 logical CPUs). Not worth the risk.

---

## 3. What the main thread's 17.6 % kernel time is (desktop)

Each row is a syscall stub's share of [App]Game samples. A user-mode sampler (SuspendThread / GetThreadContext) charges a
syscall's kernel time to its ntdll stub.

| capture (config) | kern % (GetThreadTimes) | ZwDelayExecution (7D9E90 W6) | ZwReleaseSemaphore | RtlQPC (user) | rest of ntdll |
|---|---|---|---|---|---|
| ingame / ingame2 (60 lock) | 18.0 / 17.8 | 2.7 / 2.6 | – / 0.5 | – | ~1.2 |
| r5 (main-240, all fixes) | **17.6** | **5.9** | <0.6 | – | ~1.5 |
| r6-A1..B2 (default 120) | 3.7–7.8 | 1.1–1.9 | 0–0.8 | 5–9 | ~1 |
| r7-A1..B2 (default 120) | 4.8–7.5 | 1.2–3.6 | 0–0.9 | 0.9–7.8 | ~1 |
| r9-all (default 120 + caves) | 20.8 | 5.2 | 0.6 | 8.8 | ~1.7 |

- **ZwDelayExecution** comes from `SpinWaitFlagSleep0` 7D9E90 (W6): `while ([ctx+7728h]) Sleep(0)`.
  - The main thread waits for SynergisticDispatcherTask to drain (WaitSubmitThread 7D9B6C).
  - This is the largest syscall item, at 1–6 %. It is the render backend, not the job system.
  - A pause-burst hybrid (check the flag every ~20 ns for a few µs, then Sleep(0)) would cut this kernel time and slightly
    shorten the reaction. The CPU time stays the same, and an fps change below 1 % is expected.
- **ZwReleaseSemaphore** (0.5–0.9 %) comes from DispatcherRingPush 25426C / KtglCmdStreamFlush 254110: one release per
  render-ring item.
  - **D1** would release only when the consumer is about to sleep. The consumer (SynergisticDispatcherLoop 1A7C3E0)
    waits once per item, so it would have to drain the ring per wake.
  - That needs changes on both sides; documented only.
- **win32u** (D3DKMT) is 0.1–0.3 %.
- **Unexplained: about 10 of the 17.6 points** (and ~14 in r9) never appear at a syscall stub. A sampler of this kind
  attributes such kernel time to the interrupted user instruction. The candidates are:
  - page faults (demand-zero / soft faults show up at the faulting RIP);
  - the sampler's own suspend / get-context APCs (estimated 0.2–0.6 %).

  The share varies 4–21 % with config at a constant Sleep(0) share, which points at the workload (preset / draw volume),
  not at synchronisation.
- **Next measurement (no attach, no game handle):** `dotnet fsi jobs_live.fsx counters 10`. It prints process page
  faults/s, plus per-thread kern %, user % and context switches/s, from NtQuerySystemInformation. High faults/s would
  confirm page faults. The main thread's cs/s shows how often Sleep(0) actually yields.

## 4. Handheld (Ally, Proton) reading
This section uses Ally profile numbers reported from a separate capture; they are not re-derived here.
- ~140 KIDS threads on 8C/16T. The model gives 127 in the weighted pools, plus TS_MESHLET_REQ 14 and the fixed pools.
- KIDSTaskScheduler group at 305 % CPU, ~9,900 voluntary cs/s, 84 % of samples in ntsync waits.
- These fit the structure above:
  - every job-group join wakes a cascade of up to N−1 workers, and N is 13 on the APU against 7 on the desktop;
  - EffectUpdateDisp's 12 workers are kicked and joined every frame;
  - every steal scan walks up to 12 other queues.

WC attacks N (all three effects), WK attacks the futile tail of each cascade, and J1 the scan cost. The order to try:
**J1 + WK (live, reversible) → WC cap 10 → cap 8**. Compare cs/s with `counters`, and package power at a fixed fps cap.

## 5. Test commands
```
dotnet fsi jobs_live.fsx offline                        # dump-only verification (ALL PASSED)
# --- desktop or Ally, game running, in a real level ---
dotnet fsi jobs_live.fsx counters 10                    # read-only baseline: faults/s, per-thread kern/user/cs/s, KIDS family totals
dotnet fsi jobs_live.fsx dry                            # read-only: WK/WC/P12 state + live pool table (base, counts)
$env:FT_ALLOW_WRITE=1
dotnet fsi workers_live.fsx apply J1
dotnet fsi jobs_live.fsx apply WK
dotnet fsi jobs_live.fsx status                         # wake attempts/s, kept vs skipped
dotnet fsi jobs_live.fsx off WK ; dotnet fsi jobs_live.fsx counters 10 ; dotnet fsi jobs_live.fsx on WK ; dotnet fsi jobs_live.fsx counters 10
dotnet fsi jobs_live.fsx revert WK
# --- WorkerCap (handheld only): start BEFORE launching the game ---
$env:FT_ALLOW_WRITE=1; dotnet fsi jobs_live.fsx boot 10        # or: boot 8 ; add "p12" only on <=12-thread CPUs
# launch the game, then:
dotnet fsi jobs_live.fsx status                         # base(+740h) must equal the cap; pool counts as in the §2c table
dotnet fsi jobs_live.fsx counters 10                    # compare KIDS family cs/s and CPU against a stock launch
```
Pass criteria:
- PresentMon average fps and p99 unchanged (desktop), or package power lower at the same capped fps (Ally).
- WK: the skipped/s count is non-zero, with no stalls.
- WC: no frametime spikes in heavy combat; streaming time-to-sharp after a fast travel unchanged.

## 6. Files (private dev repo, except symbols)
- `jobs_live.fsx`: all modes (offline / dry / apply / on / off / revert / status / boot / counters).
- Disassembly of 1A6BB84, 3FE14, 40800, 403A0, 3E7CC, 207200, 1FE2FD4, 1FDFEB4, 7D9E90.
- symbols/symbols.tsv holds the 50 symbols from this note (functions, labels, structs JobPool / JobWorker /
  WorkerThread / JobGroup / Job). PoolTable record 3 (+54h) is KIDSTaskScheduler and record 5 (+84h) is ResourceLoad.
