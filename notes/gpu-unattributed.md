# pc2: where the ~4 ms the GP2 per-PSO table does not attribute goes

Status 2026-10-09. Question: pc2 (RX 6650 XT, 1080p HIGH, FSR 65, ASI 1.7.44) Phase A (bridge #125 table, #126 raw zip) shows 19.2 ms
"attributed" of a 23.2 ms mean stamped span. What is the other 4 ms: real GPU idle, unmeasured work, or profiler overhead?
Decoder: `re\scripts_gpu\gpu_unattr.py` (stdlib, `--selftest` passes). Pending test: Phase U of `pc_gpu_session.ps1` (section 4).

## 1. Short answer (from data we already have)

**Most of the 4 ms is not missing at all. The table is the wrong thing to sum.** Over the same 23 windows (draws >= 1000/frame):

| term | ms per stamped frame | what it is |
|---|---|---|
| stamped span S (DIRECT queue, first -> last GP2 stamp) | 23.20 | |
| "attributed" = table top 80 rows | 19.23 | 16.16 DIRECT + **3.07 async compute** |
| S - attributed | **3.97** | = 2.15 + 4.89 - 3.07 (below) |
| uncovered DIRECT time (S - covered) | **2.15** (9.3 % of S) | between lists / before a list's first stamp / unstamped lists |
| PSO tail below the cut | +4.89 | real stamped DIRECT work in small buckets: 1.42 below the log's per-window top 100, 3.47 between digest rank 80 and the last row |
| async rows counted as if serial | -3.07 | VCLOUD 1.6, VFSCAT 1.0, IGP, PFB, BWVM run on the async queue *beside* the DIRECT span |

So the real unexplained part is the **2.15 ms of uncovered DIRECT time** (heavy GPU-bound windows: 2.8-3.5 ms of ~30 ms).
GP2's `covered` (21.05 ms, 91 % of S) contains every DIRECT interval, small or not; per-PSO rows are just a ranked split of it.
(`--min-draws 1000` in ally_budget is a **window** filter (gameplay vs menu/load windows), not a PSO cut. Without it, 34 windows:
S 18.87, attributed 15.58, gap 3.28 = uncovered 1.93 + tail 3.84 - async 2.48. Same picture.)

Of the 2.15 ms uncovered, what PresentMon proves:
- **Real whole-GPU idle inside the frame is small**: PresentMon GPUWait (idle inside the frame's own GPU work, any queue) = **0.27 ms**
  mean (0.25-0.42 in the heavy 30 ms windows). PresentMon GPUBusy is the union of all the process's queues; the DIRECT span lies
  inside the frame's GPU work, so whole-GPU idle inside S is <= ~0.3-0.4 ms.
- Therefore **>= ~1.8 ms (heavy windows >= ~2.5 ms) of the uncovered DIRECT time has the GPU busy with something else**: either
  (a) work on the DIRECT queue that GP2 does not stamp (list heads: barriers / layout transitions / decompressions / cache flushes before
  the first draw; lists with no GP2 stamp), or (b) the DIRECT queue waiting on a fence while the async queue is still running (async
  is 3.5-6.9 ms/frame in the heavy windows; VCLOUD alone 1.6-2.5 ms). Existing data cannot split (a) from (b): that is what GQ does.
- **Idle between frames** (PresentMon FrameTime - GPUTime) is outside S, so it is not part of the 4 ms, but it is real: **0.85-0.95 ms
  per frame in the heavy GPU-bound windows** (~3 %), 0.03-0.08 ms in the light ones. Hypothesis (unproven): the frame-latency wait
  drains the GPU at the frame boundary and the first submit of the next frame comes ~1 ms after the last list ends (more draws -> later
  first submit). GQ does not see this (it is between frame slots); a PresentMon / ECL-timestamp view is needed.

Profiler overhead (same windows): stamped frames are **+0.42 ms** slower than unstamped ones by the mod's own wall clock (mean of 23
windows, range -0.42..+1.36; 308 stamps per stamped frame, ~1.8 % of a stamped frame). PresentMon fold (GPUBusy by frame index mod 8,
11 windows matched) agrees: stamped phase **+0.41 ms** (sem 0.25; the spread between the other phases is 0.65, so not significant per
window). The stamps sit inside lists, so this overhead mostly inflates covered rows, not the gap. **Overhead explains <= ~0.4 ms.**

| user's category | proven now | open (GQ / RGP) |
|---|---|---|
| real GPU idle (starvation, CPU-late, fence waits with nothing else running, present/flip) | <= ~0.3 ms inside S; ~0.9 ms/frame between frames (heavy windows, outside S) | which edges |
| unmeasured work (heads, barriers / flushes / decompressions, unstamped lists, driver work) + DIRECT waits overlapped by async | >= ~1.8 ms of the 2.15 (heavy: >= ~2.5 of ~3) | split heads vs cross-queue waits; name the edges |
| PSO tail (`--top` / GPUProf2_Top cut) | 4.89 ms, real work, already inside covered | - |
| async counted twice | 3.07 ms (makes the table look *more* attributed) | - |
| profiler overhead | ~0.4 ms per stamped frame | ON/OFF A/B in one launch (Phase U) |

Numbers: `py -I re\scripts_gpu\gpu_unattr.py <#126>\tour\logs\run01.log --pm <#126>\tour\presentmon.csv --min-draws 1000 --windows`.

## 2. Things in #125/#126 that are wrong or misleading (fix before trusting them)
- **PresentMon stopped after 5 min 59 s** (rows end 17:58:55 PM time = 00:58:55 log time; the tour ran to 01:07:37). Only windows 0-11
  have PresentMon; the tour report's `pm_gpu` column is empty. Cause unknown (pc2 side). Phase U is ~7 min: check `presentmon.csv`
  covers the whole run.
- `ally_budget.py` `fps(stamped)` takes the **first OSD line after** each GP2 window, i.e. the next 30 s stats period: 120.1 fps next to
  a 25.7 ms window is that artifact. The OSD "rendered" fps at the heavy spawns is ~32 fps.
- The tour's `fps=120.0` / "avg fps 120" is the game **tick** rate (logic), not rendered fps; pc2 renders 30-110 fps on this route.
  The `bound` column says "CPU/wait" for points that PresentMon shows GPU-busy 96 %.
- PresentMon timestamps on pc2 are 7 h behind the log's local clock (17:52 vs 00:52); `gpu_unattr.py` finds the offset itself
  (it scores every 15 min offset by |PresentMon frame time - GP2 wall|).

## 3. How the GP2 numbers are built (gpuprof.cpp, so the decomposition is exact)
- Intervals are a contiguous chain inside each list: each stamp closes the previous interval and opens the next (IvClose). A list's
  coverage runs from its first GP2 stamp (first draw / dispatch / copy / clear / marker in a stamped frame) to its last (Close).
  Everything inside that, barriers and in-list stalls included, is **covered**. `MinRun 4` only merges short PSO runs into the
  neighbour bucket (billed to the longest run): mis-attribution between PSOs, never lost time.
- `covered` = sum of DIRECT intervals; `span` = first -> last DIRECT stamp of the frame slot; `async-compute` = sum of COMPUTE-list
  intervals, kept out of span and covered. Copy-queue lists are not hooked at all (only DIRECT / COMPUTE lists of the game's device).
- So `S - C` = time on the DIRECT timeline outside every list's stamped range: gaps between lists (idle or cross-queue Wait), list
  heads (commands before the first stamp, e.g. ResourceBarrier; draws / dispatches / copies / clears / markers are stamped), and
  lists that got no stamp. GQ (gq-jt-probes.md) splits exactly this, with closure `S - C = gaps + unmeasured + heads + no-cov`.
- GPUProf2_Top is clamped to 100 rows per window, but the tail is still recoverable as `C - sum(DIRECT rows)` (done above).

## 4. Pending test: Phase U on pc2 (one launch, hands-free)
`powershell -NoProfile -ExecutionPolicy Bypass -File tools\testmatrix\pc2_pc_gpu_session.ps1 -Phases U -UserConfig <pc2 config.xml>`
(pc2-bench-1080-120-high = the #125 config). ~8 min. ASI >= 1.7.40 (GQ); pc2 has 1.7.44 / 1.7.45b: no new ASI needed.
- run_tour, list = the single mission **7292** (Mirage Fortress spawn: the heaviest GPU-bound point in #126, 33.8 ms span, 2863
  draws, PresentMon GPUBusy 96 %), `-Points 1` (spawn only), **5 passes x 30 s**: the same spawn view re-loaded five times = five GP
  windows (tour windows follow the dwells, and the GP2 A/B counter counts only those).
- `GPUProf2_SampleEvery = 1`, `GPUProf2_AB = 1`: windows 1/3/5 stamp **every** frame, windows **2/4 stamp nothing** (GPU side GP2
  off; hooks stay installed, ~0.2 us CPU per draw on the recording threads, off the critical path at 30 fps). = ABABA in one launch:
  PresentMon GPUBusy ON - OFF = the GP2 GPU cost per stamped frame, without the frame-phase guesswork of the fold.
- `GQ = 1`, `GQ_Top = 20` (DIRECT queue gap census: gaps / gap edges with markers and ECL call sites / unmeasured submissions /
  list heads / tails / lists without coverage; closure vs GP2 uncovered). SampleEvery 1 also removes the "list reset before the
  sampled frame" artefact of 1-in-8 sampling. Same MinRun 4 / Top 100 as #125.
- Not covered by the mod: **copy queue** (not hooked) and an async-queue gap census (GQ is DIRECT only; GP2 gives async totals per PSO).
  A DIRECT wait on async / copy shows up as a GQ gap whose next list starts at the consumer marker. Extending GQ to COMPUTE / COPY
  queues would be a mod change; not done until the DIRECT census says the waits matter.
- Optional, not hands-free: Radeon GPU Profiler. pc2 has no AMD developer tools yet (user OK pending, bridge #556). An RGP capture
  shows barrier kinds, cache flushes / decompressions, queue timelines incl. copy and the CPU submit times (QueueEvent). Our own
  decoder (`patcher\gpu\sqtt_rdna4.py`) is RDNA3/4 only: an RDNA2 (gfx1032) trace would need the RGP GUI or a gfx10 decoder. RGP
  must be its own launch (the driver instruments every command buffer while connected).

Decode (the session does it; by hand): `py -I re\scripts_gpu\gpu_unattr.py <U capture>\logs\run01.log --pm <U capture>\presentmon.csv --windows`
-> `U_unattr.txt` with, per arm: **IDLE by cause** (PresentMon in-frame / between-frame idle; GQ DIRECT gaps with the short-regular
per-submit class split out, top edges, unmeasured-submission gaps), **UNMEASURED work by category** (list heads, no-coverage lists,
tails, closure residual, PSO tail, async overlap), **PROFILER overhead** (A/B windows: mod wall clock ON vs OFF and PresentMon GPUBusy
ON vs OFF).

What decides what (write the verdict from these, not from the table sum):
- GQ heads >= ~1 ms/frame: the uncovered time is mostly list-start barriers / transitions -> the barrier work (gpuopt Barriers, R2a)
  is the lever; check which lists (edges show the first marker).
- GQ gaps >= ~1 ms/frame with the next list's first marker at an async consumer (DrawSky / VCLOUD, fog): the DIRECT queue waits on
  async compute that is still running -> kick the async work earlier / move the join later (engine async slots, gpu-async-wins.md).
- Gaps mostly short and regular (>= 0.9/frame, mean <= 150 us): per-submit cost class (ECL merge rule in gq-jt-probes.md 5).
- PresentMon OFF windows == ON windows within noise: GP2 overhead is negligible and #125's per-PSO ms can be used as is.
- Between-frame idle ~0.9 ms (outside S) is a separate lead; test it with a frame-latency / submit-timing probe, not with GQ.
