# What bounds the level-load wall time (PC LoadStats 1.7.16 + static RE)

Date: 2026-10-08. Static work only; the game was not run. Inputs: the PC LoadStats logs (6 title -> Continue loads,
ASI 1.7.16, BASE / GL1 / LC / LW) and their test-matrix summary, Ally mod logs + SP, the decompile.
Digest: `ls_digest.py` (private dev repo; it also prints tick and gate lines once they exist).
The names are in symbols/symbols.tsv. Builds on research/loading.md.

## 1. PC data (58A window)

| log | window s | frames (ms) | CPU cores | stage ready | managers busy A, last busy | in-flight A / cap | ready-wait | event scene | MB archive + other | inflate CPU-s | PSO CPU-s |
|---|---|---|---|---|---|---|---|---|---|---|---|
| BASE_a | 25.3 | 2857 (8.9) | 8.29 | +0.6 | 8.7 s, +9.9 | 9.6/10 | never | 0 | 1554 + 653 | 5.51 | 0.00 |
| BASE_b | 26.2 | 2925 (9.0) | 8.26 | +0.5 | 8.9 s, +10.3 | 9.6/10 | never | 0 | 1528 + 658 | 5.64 | 0.00 |
| GL1 | 25.8 | 2909 (8.9) | 8.54 | +0.6 | 9.6 s, +11.1 | 9.6/10 | never | 0 | 1544 + 658 | 5.40 | 0.00 |
| LC | 25.4 | 2826 (9.0) | 8.82 | +0.6 | 11.3 s, **+13.0** | 15.0/16 | never | 0 | 1557 + 657 | 5.37 | 0.00 |
| LW | 26.4 / 25.7 | 2918 / 2841 | 8.28 / 8.39 | +0.6 | n/a (LW replaces the hooked Sleeps) | - | never | 0 | 1560 + 655 | 5.6 / 5.75 | 0.00 |

UI bracket around every window (`MENU:` lines): the loading UI signal (D63C0 = 1) opens **2.10 s +-0.02** before 58A is
set and closes **0.59 s +-0.01** after the clear, in all 6 runs. Wall clock from the Continue click (title menu signal
closes, ~06:04:11.4 in BASE_a) to 58A set is ~39 s more; that span includes the harness's fixed Space presses (+25 s,
+30 s) and is not measured by LoadStats.

## 2. Ranked breakdown of the ~25.8 s window

| # | item | seconds | evidence | verdict |
|---|---|---|---|---|
| 1 | **wait after the resource managers drain** (last busy -> clear) | **14.6-16.3 (~60 %)** | managers A/B are empty from +9.9..+11.1 s; LC made them finish 3 s *later* (+13.0) and the window did not move (25.4 s); ready-wait never armed; event scene 0 | **the critical path**, not identified yet: one of the LoadIsLoadEnd preconditions (§3.2) or a sequencer settle state / script wait. Needs the LG probe (§5) |
| 2 | resource-manager work (A/B queues, TrySync pool 3 threads) | 9.9-11.1 span, overlapped | pending avg 277-354, at cap 87-89 %; LC: in-flight 9.6 -> 15.0 but busy 8.7 -> 11.3 s (the same 3 TrySync threads run the steps; a higher cap only queues more) | **off the critical path** by >= 12 s. LC / LW write-off for this route |
| 3 | set -> stage state 3 | 0.5-0.6 | `split [set->stage ready]`, 22-26 MB read | fade/teleport part of the sequencer; fixed |
| 4 | loading-UI fades around the window (user-visible, outside 58A) | 2.10 + 0.59 | constant to +-0.02 s over 6 runs | fixed timers = the look. **Write-off** |
| 5 | character ready-wait (LoadIsLoadEnd 120-frame timeout) | 0 | `g_LoadEndReadyWaitAcc` never > 0 | not a limiter on PC |
| 6 | zlib inflate | 5.4-5.8 CPU-s, max 4 parallel | io line | <= 1.4 s even fully serial; overlapped. ZI stays written off |
| 7 | PSO creation | < 0.005 CPU-s (22-31 gfx + 8 compute runtime) | io line ("max 20-29 ms" is since the previous window, i.e. boot/title: the max is exchanged only at a window end) | **write-off** (#7 of loading.md) |
| 8 | disk | 2.2 GB overlapped archive + 0.65 GB in 584k small sync reads; in-call 1.0 + 2.2 s summed over threads | 111 MB/s average on an NVMe that reads GB/s | **write-off** (#9) |
| 9 | Sleep-paced manager polls | A idle 6.5-6.8k, no-progress 1.4-1.7k, B idle 7.4-8.9k Sleep(1)s | LW removes them: 26.4 / 25.7 s vs BASE 25.3 / 26.2 | **write-off** (0 s) |
| 10 | game-logic prefetch (GL1) | 0 | 25.8 s | write-off for loads |

CPU stays at 8.3-8.8 cores for the whole window and the frame time at 8.9-9.1 ms (VRR cap 120 = 8.33 ms, so the cap
binds only part of the frames); the full scene renders behind the cover, as on the Ally.

**Ally** (no LoadStats in these logs): the title -> Continue window is 118.657 / 118.688 / 118.737 s (L1-L3) and
118.281 s at MAX, constant to +-0.04 s, while the PC does the route in 25.8 +-0.5 s. loading-ally-fixed.md §7 explains
the constant: the Ally's load work ends at about +93 s, and about 25 s of the window is the tip page waiting for a
button press (most likely the test harness pressing at a fixed delay). The Ally's remaining gap is per-request read
latency under Wine; EP and SR brought the load from 96.0 s to 47.3 s and then 38.45 s (docs/BENCHMARKS.md, section (c)).
Ally SP in that window: main thread 70 % in GPU-fence waits, workers 95 % idle, dispatcher 63 % waiting on the ECL
ring.

## 3. Who opens and closes 58A (static)

### 3.1 Owners
StopFlagSet 3B28FC has 53 call sites; at least 23 pass idx 2 (byte G+58Ah): the script command Pause::SetFlag 23A763C,
**FastTravelSeqTick 3B2A04** (set 3B2B8A, clear 3B32AD), **WarpSeqBTick 5643A0** (set 564684, clear 56460F),
FastTravelSeqReset 3B2120 (clear 3B21C4), and ~17 more (1C2xxxx, 223xxxx, 28A8FD4, 2237244, 21C0314, 23B9C28..23BA4EC).
All are dt60 frame-timer state machines ticked from PerFrameUpdater 5651D0.

FastTravelSeqTick (struct `g_FastTravelSeq` [4757470]: +0 state, +4 timer float (dt60 units), +40h popping wait):

| state | does | leaves when |
|---|---|---|
| 2 | sets 58A, UiLoadingScreenStart 6372A0, fade 20 | timer > 0 |
| 3 / 4 | fade | 3: >= 5 units; 4: > 20 units and UiFadeIdle 3B3AD8 -> warps the player, state 5 |
| 5 | | LoadIsLoadEnd 3B3424 and StagePathInitStep(stage+500h) and timer > 2 |
| 6 | | LoadIsLoadEnd, [[45B9E40]+58h] == 0, path init, !UiBlockingWidgetActive 5A377C ([45C84A8]+430h), timer > 2 |
| 7 | popping wait | timer > [+40h] -> UiLoadingScreenEnd 3B1B6C, **clears 58A**, state 8 |

The popping wait is 0 for the native requests (FastTravelRequestShrine 1C10F48, FastTravelRequestTo 1C282CC) and a
scripted value only for Player::RequestFastTravelWaitPopping 23AADB4. WarpSeqBTick (`g_WarpSeqB` [47098A0]): state 1
waits > 25 units + UiFadeIdle, sets 58A; state 2 waits > 10 units + LoadIsLoadEnd, clears 58A.

So on every native owner the window ends on **LoadIsLoadEnd** plus a few frames; the 2-25-unit timers are < 0.5 s.

### 3.2 LoadIsLoadEnd 3B3424 preconditions (evaluated in order, short-circuit; only #9 was measured so far)

| # | call site -> callee | passes when | meaning |
|---|---|---|---|
| G1 | 3B3443 -> StagePathInitStep 3B35B4 (stage+500h) | al = 1 (state 5) | path / navmesh init steps |
| G2 | 3B3460 -> StageAreaListBusy 8B3868 (stage+540h) | al = 0 | an area whose load has not finished |
| G3 | 3B347B -> StageRequestQueueBusy 3B3974 (stage+450h) | al = 0 | stage requests in state 1..3 |
| G4 | 3B3490 -> UiLoadPairReady 3B3934 (only if byte 45C6F2C) | al = 1 | |
| G5 | 3B34A4 -> ObjectListAnyNotReady 13335C ([45B9E60]) | al = 0 | an object with +2C8h == 0 (not set up) |
| G6 | 3B34D0 -> D3958 (only if stage+504h != -1) | rax != 0 | current area present |
| G7 | 3B34F0 -> LoadEndBlockerScan 2BE284 | al = 1 | list [[45B9E40]+48h] (when +2679h) |
| G8 | 3B3506 -> StageAreasObjectsReady 3B3A28 | al = 1 | area objects: +1F0h set, +1ECh = 0, +1C0h = 0 |
| G9 | 3B3559 -> LoadCharacterReady 3B39AC | ready | the 120-unit timeout path (LoadStats acc) |

G9 never armed on PC, so the 15 s after the managers drain sit in G1-G8, in the sequencer's state 6/7, or in a script wait.
G5/G8 (objects set up) are the strongest prior: objects get their models and textures through the streaming queues
(KIDS_MMM_SUB_REQLOAD, TS_MESHLET_REQ, texture tiles), not through managers A/B, and that work keeps running after +10 s.

## 4. Fix designs (all default-off ASI flags; built only after §5 names the gate)

| if the LG probe shows | fix | same look because | expected |
|---|---|---|---|
| G5/G8 hold and SP shows per-frame object setup on the main thread (budget per frame) | **LB**: raise that per-frame budget while 58A is set | the same objects are set up, in fewer frames, behind an opaque cover | up to the G5/G8 share (<= ~10 s PC) |
| G5/G8 hold and the main thread waits (Ally pattern), streaming queues busy | **LS**: raise the streaming request caps (KIDS_MMM_SUB_REQLOAD / TS_MESHLET_REQ in-flight) only while 58A is set | same requests, same order, more in flight | unknown until the queue depths are logged; decompile the queues first |
| G1/G3/G2 hold | managers/path init: decompile the step that holds | - | - |
| no gate holds, state 6 waits on 5A377C or [[45B9E40]+58h] | identify the widget; a minimum-display time is the look | - | **write-off** if it is a timer |
| state 7 (scripted popping wait) or owner = script with no LoadIsLoadEnd calls | fixed by design (hides pop-in) | - | **write-off** |
| window frame-bound (frames constant, wall follows fps) | lift the mod's own VRR cap while 58A is set | the cover is static; loading.md rejected cover frame caps only as the game's own | <= 0.6 s PC (8.9 vs 8.33 ms) |

Written off now: LC, LW, GL1 for loads (0 measured; managers drain >= 12 s early), ZI, PSO pre-creation (< 5 ms CPU
in the window), I/O (111 MB/s), the 2.10 s + 0.59 s UI fades. RWb/GF/W6/LM stay Ally-only (shared power), unchanged.

## 5. Next measurement (spec; one session per device; game not run here)

**New probe LG** (new code in `loadstats.cpp`, `[Profiler] LoadGates = 1`, default 0, needs LoadStats >= 1):
1. Call-site hooks on G1-G9 (the 9 E8 sites in §3.2; same verified-bytes thunk path as the inflate sites; extend
   `ls_sites.py` to check E8 + rel32 -> the listed callee). The thunk calls the original and stores {result, eval#, QPC};
   G1's thunk increments eval#. Per window log: per gate the call count, % of evaluations it was the first failing gate,
   and the time it last flipped to pass (s after the set); "LoadIsLoadEnd last returned 0 at +X s, 1 at +Y s".
2. Polls (no hooks, every 10 ms): `g_FastTravelSeq` [4757470] +0/+4/+40h and `g_WarpSeqB` [47098A0] +0/+4: log each
   state change as `LoadSeq FT 5->6 +12.3 s`; at the 58A set log the owner (FT if FT state is 2-4, B if B state is 1-2,
   else "script/other"). Also log [[45B9E40]+58h] and 5A377C's object ([45C84A8]+430h != 0) on change.
3. `LoadStats = 3`: the 1 s tick also runs outside 58A windows (whenever the frame counter moves), so the ~39 s from
   Continue to the set is covered; add 58A, FT/B state, first failing gate to the tick line.

**PC turn** (ini = the full-mod 1.7.16 test ini + `[Profiler] LoadStats = 3`, `LoadGates = 1`, `MemStats = 1`,
`SP = 1`, `ReportSec = 5`, `TopN = 25`): `loadstats_ab.ps1 BASE_a BASE_b`, with the harness writing a timestamp per
click/key to a log file; then one shrine fast travel and one death respawn in the same
launch (native FT owners). Digest: `ls_digest.py`, `load_sp.py` (SP rows 0-10 s vs 10 s-clear).
**Ally turn**: the same flags on the title -> Continue route x2 + one fast travel. This was run on the Ally; the
results are in loading-ally-fixed.md §7 (no gate holds to the clear: the load work ends at ~+93 s).

Decide: the gate/state that holds for most of the post-drain time picks the row of §4; nothing else gets built first.
