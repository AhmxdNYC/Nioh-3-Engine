# GQ + JT: two diagnostics for the GFX-queue gaps and the logic join (ASI 1.7.41, branch `diag`)

Both are **measurement only** and **off by default**. Nothing ran in the game yet; everything below was proven offline (section 7).
They answer whats-left.md section 6 plans 1 and 2 (candidates #2 and #4):
- **GQ**: where the Ally GFX queue's 1.4-2.3 ms/frame outside every GP2 interval goes (whats-left fact 4).
- **JT**: what KIDSTaskScheduler0 waits for in its 1.44 ms/frame `NtWaitForAlertByThreadId` "join" on the PC (whats-left section 1).

## 1. Keys (`[Profiler]`)

| key | default | meaning |
|---|---|---|
| `GQ` | 0 | 1 = graphics-queue gap census. **Needs `GPUProf = 2`** (it reuses GP2's query heap, sampling and stamps; with GPUProf < 2 it logs `GQ needs GPUProf = 2 - GQ off`). Windows and Proton (vkd3d timestamps) alike. |
| `GQ_Top` | 5 | rows in the edge table and in the largest-gaps table (1-20) |
| `JT` | 0 | 1 = job-timing / logic-join probe (v2.02 only; group `JT`, 5 code sites written only when 1). `[Patches] JT` is read as a fallback, so the abba2 harness's `-Arms BASE,JT,BASE` works with an old base ini. |
| `JT_Thread` | KIDSTaskScheduler0 | thread description of the watched (waiting) thread |
| `JT_Top` | 6 | wait sites per window (the job table gets 2x) |
| `JT_ReportSec` | = `ReportSec` (30) | window length (5-600 s) |

**Cost when off.** GQ = 0: one global test per `ResourceBarrier` and per `ExecuteCommandLists`; nothing else runs. JT = 0: nothing is
patched. **Cost when on.** GQ adds 2 timestamps per DIRECT list in sampled frames (~50/frame next to GP2's ~600) plus a hook on
ExecuteBundle / DiscardResource / WriteBufferImmediate / BeginRenderPass / Barrier in sampled frames. JT adds 2 QPC reads + one 40-byte
ring record per job (~50-80 ns; the start line prints the QPC cost), a stack walk per watched-thread CV wait and per matching notify.

## 2. How GQ works (gpuprof.cpp)
- A DIRECT list **Reset in a sampled frame** is armed. Its **head** stamp is written before its first GPU command, whichever comes
  first: ResourceBarrier (gpuopt's HkResourceBarrier, before any deferral), BeginEvent, a GP2 draw / dispatch / copy / clear, or the 5 extra
  slots above. Its **end** stamp is written at Close (also when Close falls in an unsampled frame, so an armed list is always complete).
- The record lands in the frame slot of the head stamp; the overlay's `ExecuteCommandLists` hook (osd.cpp, every caller) adds the
  **submission number** (running count of lists on DIRECT queues), the **ECL call site** (return RVA + the next game frame) and the
  position in the ECL.
- Per frame slot (read 6 frames later, like GP2): lists sorted by GPU start; walking the union of `[head, end]` gives every idle stretch
  between consecutive lists. A gap whose two lists have non-consecutive submission numbers had **unmeasured** lists in between (not armed,
  e.g. Reset before the sampled frame began) and is reported separately, never in GAPS.
- **Heads** = head stamp -> first GP2 stamp on the list (list-start barriers / state before the first draw / marker: the COMMON->X
  transitions of R2 land here). **Tails** = last GP2 stamp -> end stamp. Lists with no GP2 stamp are "without GP2 coverage".
- Closure: GP2's `span - covered` should equal GAPS + unmeasured + HEADS + no-coverage (the harness proves it to 0.000 ms).

## 3. How JT works (jt.cpp, jt.asm)
| site | RVA | original | replacement | what the cave does |
|---|---|---|---|---|
| JTc | 40E4E | `48 8B CB FF D2` mov rcx,rbx; call rdx (JobWorkerPoll's job call) | `E8 tramp` | `mov rcx,rbx`, QPC, `call rdx` with the caller's rdx/r8/r9, QPC, returns rax to 40E53 (`mov rsi,rax`); rbx untouched; FRAME unwind info |
| JTw | B616B7 | `FF 15 ..` call [SleepConditionVariableSRW] (STL cv win7 `wait`) | `90 E8 tramp` | other threads: tail-jump through the IAT slot; watched thread: record start, CV, 3 game frames, logic tick; real wait; record end |
| JTx | B616F7 | the same in `wait_for` | `90 E8 tramp` | same (timeouts counted) |
| JTn1 | B61674 | `48 FF 25 ..` jmp [WakeConditionVariable] (`notify_one`) | `E9 tramp 66 90` | rcx != the CV the watched thread sleeps on: tail-jump; else record waker thread, its stack and the job it runs |
| JTn2 | B6165C | `48 FF 25 ..` jmp [WakeAllConditionVariable] (`notify_all`) | `E9 tramp 66 90` | same |

- The four STL functions are slots 0-3 of the condition_variable win7 vtable at 3942338 (the r6 profile's wait is `B616A4 <-
  RtlSleepConditionVariableSRW`, 15.5 % of Scheduler0). Deps (FNV): 111184, 1113B4, 204584 head, 1753B0 head.
- Per job: function RVA (= `[[job]+8]`) and an **inner** RVA: JobNodeExecute 111184 -> vfunc +8 of `[[job+48h]+28h]` (the callback
  JobInvokeTimed calls); JobParallelForSplit 204584 -> body `[job+50h]`, range `[+74h]-[+70h]`, grain `[+78h]`; ObjRangeJobSplitInvoke
  1753B0 -> range `[+4Ch]-[+48h]`, grain `[+50h]`. A split job is a **leaf** when range <= grain (both functions split while grain < range).
- Analysis thread (every 200 ms, waits analysed 60 ms after they end): the **releaser** = the last notify of that CV inside the wait;
  the **critical job** = the job the releaser was running at the notify (or, if it was not in a job, how long ago its last job ended);
  the **longest job** overlapping the wait on any other thread; the other threads' job time inside the wait (busy %) and the share of
  the wait with no job running anywhere. Logic ticks = engine counter G+610h (osd.cpp).
- Times are inclusive: a job that helps while waiting (JobGroupWait) contains the jobs it ran.
- Not covered: jobs run outside JobWorkerPoll's generic call (the 206FA0 resource-task path and the 6B46F8 meshlet inline loop), waits
  that are not STL condition variables (the JobGroupWait Sleep(0) help-spin is visible as Scheduler0 jobs instead).

## 4. Log format

GQ (after the GP2 block of each GP window; per frame = per sampled frame):
```
GQ: frame: <L> DIRECT lists/frame (<P> % with ECL info) over <F> sampled frames; span <S> ms, lists busy <B> ms, GAPS <G> ms/frame
    (<n> gaps/frame; per frame p50 <a> p90 <b> max <c> ms), gaps around unmeasured submissions <U> ms (<u>/frame, excluded), list HEADS
    <H> ms (start -> first GP2 stamp), tails <T> ms, lists without GP2 coverage <C> ms | GP2 uncovered (span - covered) <X> ms (gaps +
    heads + no-cov = <Y>) | armed <k>, no head <k>, dropped: rec cap <k>, ECL table <k>, late <k>, bad ts <k>
GQ: top <N> of <M> gap edges [previous list's last marker -> next list's first marker] | ms/frame | gaps/frame | mean us | max us | ECL ...
GQ:   1 [<marker> -> <marker>] | <ms> ms | <gaps/frame> | <mean us> | <max us> | <ECL return RVA> < <next game frame RVA>
GQ: largest <N> single gaps: us | list k-1 -> k of n (GPU order in the frame) | [last marker] -> [first marker] | submissions | ECL ...
GQ:   1 <us> us | <k-1> -> <k> of <n> | [<marker>] -> [<marker>] | <sub> -> <sub>[ (UNMEASURED submissions between)] | <ra> -> <ra> < <ra1>
```
RVA `ext` = the call came from outside Nioh3.exe (the overlay / Streamline), `-` = none. Markers come from the engine's own pass names.

JT (every JT_ReportSec):
```
JT: window <s> s | watched <name> (tid <t>) | logic ticks <n> (<r>/s) | CV waits <n> (<w>/tick), blocked <B> ms/tick (<%> of the window);
    blocked per tick p50 <a> p90 <b> max <c> ms; timed out <n>, no waker seen <n> | jobs <n> (<r>/s, <ms> ms/tick summed) on <k> threads |
    dropped: jobs <n>, waits <n>, wakes <n>; unbalanced <n>
JT: wait sites by blocked time [return RVAs of the first 3 game frames above the STL wait, innermost first] | ...
JT:   1 [<rva> < <rva> < <rva>] | <ms/tick> | <waits/tick> | <p50> / <p90> / <max> us | <thread> <share> %, ... | [<waker rva> < ...] <share> %
JT:      critical job (...): 1) <fn>/<inner> <share> % of the waits, job p50 <us> p90 <us> us, started before the wait in <%> (p50 <us>
         us before), else p50 <us> us after it began; 2) ...; releaser not in a job <%> (its last job ended p50 <us> us before the wake)
JT:      longest job overlapping the wait: 1) <fn>/<inner> <share> % (p50 <us> us); ... | other job threads busy <%> of the wait
         (<k> threads ran jobs), no job running on any other thread <%> of the wait
JT: jobs by total time (all threads; ...) | ms/tick | jobs/tick | p50 / p90 / max us | threads | split jobs: leaves/tick, grain, mean leaf range
JT:   1 <fn>/<inner> | <ms> | <n> | <p50> / <p90> / <max> | <threads> | leaves <n>/tick, grain <g>, leaf range <r>
```
CRT / STL frames (B5E000-BF0000) are skipped in the stacks. All RVAs are return addresses or function starts; name them with
`py -I tools\gqjt_digest.py <log>` (symbols.tsv, containing function).

## 5. Decisions (`tools\gqjt_digest.py` prints them)
**Build R2 / merge lists (GQ, Ally USER).** First check closure (uncovered == gaps + heads + unmeasured + no-cov within 0.15 ms + 10 %).
- **Merge lists (vkd3d-audit design 3, ECL merge)**: BUILD only if the *per-frame submission gaps* - edges with >= 0.9 gaps/frame and a
  mean <= 150 us (the ~38 us/boundary serialising-semaphore cost class of gpu-scheduling B2) - sum to **>= 0.3 ms/frame**. The edge
  rows name the list pairs to merge first. Large, irregular gaps (mean >> 150 us, < 1/frame) are CPU-late submissions: merging does
  nothing; fix the producer (dispatcher / main thread).
- **R2a / R1**: list HEADS >= **0.2 ms/frame** = list-start transitions are worth the ABBA (R2a / R1 = 1, KEEP at >= +0.1 fps in both
  pairs). Heads < 0.2: decide on the trace's mid-list counts alone (vkd3d-audit B6: COMMON->X on buffers >= 50/frame, supersets >= 50).

**Reorder / split jobs (JT, PC abba2).** Take the top wait site.
- Released by a non-job thread ([App]Game, SynergisticDispatcher, an effect / input worker): **not a job join** (back-pressure or an
  effect / input join); JG cannot help; the waker site names the producer.
- One critical job >= 50 % of the waits, started **after** the wait began in most waits, start delay p50 >= 200 us: **REORDER** - kick it
  first / earlier (JG reorder); expected saving <= its start delay.
- One critical job >= 50 %, already running when the wait began (>= 50 %), p50 >= 0.5 ms, other job threads < 50 % busy: **SPLIT** -
  ParallelFor / range job (204584 / 1753B0): smaller grain at the fan-out (JG: job+78h / +50h); any other job: split / start earlier (RE).
- Other job threads >= 80 % busy during the wait: **THROUGHPUT** - no reorder / split gain; cut the work (AABB, BP).
- ParallelFor rows: leaf p90/p50 >= 4 with few leaves per thread = uneven split; also points at JG.

## 6. Test lines
Install `mod\Nioh3EngineFix\bin\Nioh3EngineFix.asi` from this branch (1.7.41) and its ini keys (copy the `GQ` / `JT` block of the
1.7.41 ini into the game's `[Profiler]`, or rely on the `[Patches] JT` fallback).

**PC, JT in the abba2 scene (with Block A of pc-cpu-next 5, same turn):**
```
powershell -File tools\testmatrix\abba2_pc.ps1 -Arms BASE,JT,BASE -Live none -MaxFps 240 -Input vpad -RunName jt_abba2
py -I tools\gqjt_digest.py captures\tm\abba2\jt_abba2\<the JT launch's log>.log --skip 1
```
The BASE/JT/BASE fps also bounds the probe's cost (expect < 1 %). If AABB is KEEP, repeat with AABB = 1 in the base ini: the join should
shrink by the AABB time only if the critical job is the object update.

**PC, GQ + JT over many scenes (tour, GP2 already on there, windows per dwell):** in the installed ini `[Profiler]`: `GQ = 1`, `JT = 1`, then
```
powershell -File tools\tour\run_tour.ps1 -Input vpad
py -I tools\gqjt_digest.py "C:\Program Files (x86)\Steam\steamapps\common\Nioh3\Nioh3EngineFix.log"
```
(PC is CPU-bound: expect GQ gaps there to be mostly CPU-late, i.e. no merge; the PC numbers are a plausibility check, the Ally decides.)

**Ally session (USER preset, GF on, same spot as S1; this is whats-left 6 plan 1):** ini, one launch, 3 x 30 s windows after 60 s:
```
[GpuOpt]   Barriers = 2   TraceRecords = 200000
[Profiler] GPUProf = 2   GPUProf2_SampleEvery = 8   GPUProf2_Top = 100   GPUProf2_MinRun = 1   GQ = 1   GQ_Top = 8
```
Digest: `py -I tools\gqjt_digest.py <ally log> --skip 1` (merge / R2 verdicts) + `vkd3d_cost.py` by caller2_rva for B6 as planned. Then, only
if a verdict says BUILD/ABBA, the ABBA pairs with GPUProf = 0. Optional second Ally launch: `JT = 1` only (GPUProf = 0), 3 windows: is the
Ally logic thread joined on jobs (CPU fixes) or on the render side.

## 7. Offline proof (this branch)
- `tests\test_patterns.py`: **ALL 266 SITES OK** (5 JT sites: unique patterns, IAT names incl. the two `jmp [IAT]` sites, no branch into
  40E4E..40E53, the call returns to `mov rsi,rax`, vtable 3942338 slots 0-3, 4 deps), kLastTarget = caveJtWakeAll.
- `tests\run_gp_harness.py`: modes 1-4 ALL PASSED; new mode 4 = GQ on a real D3D12 device, sampled 1 in 2, a list submitted after the
  queue drained + 3 ms: GQ finds the gap as the top edge [LightPass -> ListC], 2 lists/frame with ECL numbers, heads 0.001 ms, closure
  GP2 uncovered 14.997 = gaps + heads + no-cov 14.997.
- `tests\run_jt_harness.py` (new; real jt.cpp + jt.asm): register contract of caveJtJob (rcx = job, rbx kept, rax returned), 60/60 waits,
  60 ticks, every waker found, critical job = the 7 ms JobNode with its callback RVA, longest overlap = the same, ParallelFor leaf / grain
  read from job+50h/+70h/+74h/+78h, nothing dropped: ALL PASSED.
- `run_r2_harness`, `run_padlat_harness`, `run_tour_harness`, `run_vpad_harness`, `run_wg_harness`: ALL PASSED. `build.bat`: OK.
  `tools\gqjt_digest.py --selftest`: OK.
