# Board task: validate AABB, then incremental BPO at preserved 60 Hz

Owner: pc (game owner); request #10179. Offline plan/review by cx-pc, 2026-10-17. Source reviewed: 10610bc, unchanged in study branch faf8fc1. No new game result, installation or KEEP recommendation.

## Goal and fixed baseline

Measure less CPU work with the same 60 logic ticks/s, simulation time, collision, AI and picture. Device: **PC**, record actual CPU/GPU/driver, OS, executable/ASI hashes and source revision. Repeat separately on other devices; never transfer a PC verdict to Ally/PC2. Historical estimates of AABB 0.50-0.60 and incremental BPO 0.12-0.25 ms/tick are hypotheses, not acceptance targets.

Use one repeatable saved scene, fixed camera and deterministic route/input for all timing arms. Freeze resolution/upscaler/ratio/quality, refresh/VSync, power state, all mods and all other fixes (including LK/J1/WG/MA). Set game render cap to 60, FG OFF, and `[Patches] TG=0`; verify actual logic rate, not just rendered FPS. Never change simulation timestep, update frequency, AI/collision scheduling or TG to manufacture savings. A render cap alone does not prove 60 Hz logic. If the scene/config cannot sustain 60 logic Hz, stop this comparison and have the owner establish a suitable scene/config first.

## Exact arms and INI

These are deltas to the archived device baseline, not a replacement INI. Startup value 0 means uninstalled, 1 installed ON, 2 installed OFF. Live values are only 0/1. **BPO_Mask is startup-only**; restart between masks and verify the fresh boot log.

| Stage | Startup `[Patches]` | Startup `[Tuning]` | OFF arm A, `[Live]` | ON arm B, `[Live]` |
|---|---|---|---|---|
| 1 AABB | AABB=2, BPO=0, TG=0 | irrelevant | AABB=0 | AABB=1 |
| 2 BPO Q | AABB=1, BPO=2, TG=0 | BPO_Mask=1, BPO_TableBits=14 | AABB=1, BPO=0 | AABB=1, BPO=1 |
| 3 BPO Q+V | AABB=1, BPO=2, TG=0 | BPO_Mask=5, BPO_TableBits=14 | AABB=1, BPO=0 | AABB=1, BPO=1 |
| 4 conditional Q+L+V | AABB=1, BPO=2, TG=0 | BPO_Mask=7, BPO_TableBits=14 | AABB=1, BPO=0 | AABB=1, BPO=1 |

Stage 4 is NOT cleared by this review: require the L ordering proof below first. Stage 3 measures Q+V versus BPO OFF; do not call the difference between separate stage medians a controlled V-only result. If needed, follow with separate mask 4 OFF/ON. If AABB fails correctness, stop BPO-on-AABB. If AABB is merely neutral, BPO can still be measured on that documented fixed baseline without declaring AABB a KEEP.

Common timing settings:

```ini
[Debug]
LiveCtl=1
LiveCtlFile=Nioh3EngineFix_live.ini
[Profiler]
JT=0
SP=0
```

Use a fresh control-file generation and unique tag for every request:

```ini
[Live]
Gen=1
Tag=unique_run_stage_quad_position
AABB=0
```

For BPO requests include AABB=1 and BPO=0 or 1. Require matching Gen/Tag, ok=1, fail=0 and every requested value in the ACK. ACK frames only show liveness. ACK alone can reflect an INI preference for a missing cave: require fresh current-PID install logs first, then actual `MENU: Faster bounding boxes (AABB) -> ON/OFF` or `MENU: Faster part bounds (BPO) -> ON/OFF` transitions. Reject `(ini)`, restart, skip/failure or missing installation. No-op repeated states need fresh ACKs but need not emit another MENU line. Use one exclusive control writer.

Boot evidence: `AABB: SSE character AABB cave installed, switch off` for stage 1 (ON thereafter). BPO must log caves installed, 16384 entries, correct quaternion/plain-load/copy yes/no values for the mask, and switch off. Use the same ASI in both arms; never compare ASI-absent against ASI-present. Record semantic INI diffs and no unrelated live changes. Installed-OFF caves have overhead; a KEEP also requires a separate cold-start uninstalled control to rule out a net loss versus stock (same measurement rules).

## Execution and capture

1. Game owner alone backs up saves/config/binaries/control files and absence states, holds the device game lock, starts the autoplay watcher before launch, installs the identified build and launches. cx-pc does none of these. Archive manifest, hashes, scene/route, settings and capture clock calibration. Restore exact entry state and release lock after the test.
2. After installation/live gates, warm scene for 60 s. Use **three ABBA quads** per stage: OFF, ON, ON, OFF; each state gets 5 s settling then **30 s measured**. Reassert and archive state at both window boundaries. Minimum 12 accepted windows/stage; replace an entire contaminated quad, never select only favorable windows. Archive present data for exact PID/swapchain, monotonic start/end timestamps, CPU-time endpoints, raw logic-tick endpoints and all native/ACK logs.
3. Primary CPU metric: delta of kernel+user running time from GetThreadTimes for identified KIDSTaskScheduler0, divided by actual logic tick delta, in ms/tick. Also report whole-process GetProcessTimes ms/tick and CPU ms/s, so work moved to other threads is visible. Do not substitute wall-clock waits, inclusive JT job sums or PresentMon CPU Busy for running CPU. Resolve thread ID/name for each PID; reject thread replacement within a window.
4. Obtain tick endpoints from the validated FrameMgr counter used by `jt.cpp::Tick()` using the owner's read-only counter collector, with JT/SP OFF. Validate counter address/layout against the installed executable; counter read failure/zero/reset is invalid. A capture tool is a prerequisite if no such collector exists: request tooling instead of treating FPS or ACK frames as logic ticks. Record game-time progression too. Accept 59.5-60.5 ticks/s per steady window and OFF/ON mean rates within 0.5%; these are measurement tolerances, not permission to skip updates.
5. Secondary metrics: mean and nearest-rank p99 external FrameTime, fraction >16.667 ms, fraction >17.5 ms (pacing tolerance), GPU Busy and total duration. Require >=29 s usable present capture per 30 s window, no missing/drop/reset indication. At a cap, equal FPS is expected; a CPU saving need not increase FPS. Report misses with the same thresholds in both arms. No uncapped >60-Hz result substitutes for this test.
6. Reject quads for scene/camera/input or thermal/power/config drift, rate failure, missing state evidence or capture gaps. Flag repeated-OFF CPU ms/tick drift >2% or repeated-ON >2% and rerun the quad after stabilizing. Pair A1/B1 and A2/B2 within each quad; report all three quad means and their median, plus OFF/ON raw values. Frames are not independent replicates.
7. Separate attribution pass, outside timing: `[Profiler] JT=1`, `JT_ReportSec=10`, `JT_Thread=KIDSTaskScheduler0`, `SP=1`, `Focus=1`, `ReportSec=10`. Capture OFF and ON for >=30 s after warmup; discard first report. Optional second launch uses `JT_Thread=[App]MissionThread`. Preserve drop-counter deltas, wait p50/p90 and unresolved-waker counts. Q should reduce SinCosApprox 54360 from 71FC1; V should reduce 174610 copy cost; AABB reduces 6EA50 work. Compare profiler OFF/ON overhead separately; do not use profiled timing as the primary result. Never sum inclusive jobs as CPU utilization.
8. Correctness pass in both arms at the same settings: owner runs the same movement through geometry, combat/hit/guard sequence with active enemies, object/part-heavy view, and one reload/transition. Check collision/targeting, enemy response, animation/game-time progression, missing bounds/objects, crashes and hangs. Record counts/actions and observations; a fixed camera benchmark alone cannot certify physics/AI. If human-only observation is required, the owner prepares the real controller, records a needs-player entry and requests it through DECISION. Pending observation means provisional performance only.

## Done and verdict

Deliver one per-device table: settings, stage/mask, OFF vs ON logic Hz, scheduler and process CPU ms/tick, CPU ms/s, mean/p99 frame time, both miss fractions and three quad deltas; attach manifest/raw captures/logs and restoration receipt. Define saving as OFF minus ON; percent = 100*(OFF-ON)/OFF.

Provisional performance PASS: all three quad CPU savings positive and median scheduler saving exceeds max(0.02 ms/tick, measured repeated-arm noise); no whole-process CPU increase beyond that metric's noise; no p99 regression >2% or >1 percentage-point increase in >17.5 ms misses. Otherwise report neutral, regression or inconclusive with numbers, not an FPS-based KEEP. Final KEEP additionally needs correctness observations, installed-binary validation, uninstalled-control confirmation and resolution of relevant risks below. A completed neutral/inconclusive result is still task completion; do not promise the historical estimates. Mask 7 may be explicitly deferred with its ordering dependency; masks 1/5 do not depend on clearing it.

## Offline correctness review (not a new runtime proof)

Reviewed `mod/Nioh3EngineFix/src/caves.asm:605-672,1861-2043`, `dllmain.cpp` configuration/install paths, `notes/bounds-partobbs.md`, and archived `re/scripts_logic/{offline_out,bounds_offline_out}.txt`. Existing arithmetic/liveness and multithread harness evidence is historical; no harness rerun or current-device test is claimed here.

- **AABB: low arithmetic risk, conditional on validated binary and ordinary FP environment.** It preserves ordered signed operations, writes xyz only, and falls back if output overlaps the input range. No timestep or AI/collision update is removed. Wrong bounds would nevertheless affect downstream broad-phase collision/culling. SIMD also evaluates extra lanes: stored xyz equivalence does not establish identical MXCSR exception flags/traps for unusual/nonfinite data. Check normal masked exceptions and lack of downstream flag dependence before calling it universally equivalent. Archived finite/alias/nonfinite tests are evidence, not coverage of every exception environment.
- **BPO Q: low-to-medium, no object-lifetime cache key.** Key is the four half values, not an object pointer, and insertion reconstructs the values actually consumed. Hit uses key plus MXCSR FTZ/RC/DAZ tag; seqlock rejects odd/zero/changed sequence. Miss/collision runs stock; all object bounds updates still occur. Cache hits skip FP operations and thus do not reproduce their exception flags/traps (mask/status bits are not in the tag). Require stock masked-exception assumptions and no dependency on newly raised flags. Concurrent mutation of the part record is not proven behavior-equivalent merely by a correct reconstructed insertion key: hit snapshots four halves together while stock reads separately. Establish placement-data stability/ownership during a call. Sequence wrap is also a theoretical ABA boundary for an extraordinarily delayed reader; historical contention tests are finite, not a proof for arbitrary execution.
- **Q performance detail:** QB skips insertion when key matches even if the MXCSR tag differs. QA safely rejects the wrong-mode entry, so this can create persistent stock recomputation until eviction, not a wrong quaternion. Do not interpret a cross-mode miss as data corruption or assume every repeat is a hit.
- **BPO V: same final copy under the documented non-overlap/private-temporary assumption.** It loads xyz separately, stores 16 bytes with w=0, then loads source w for returned xmm0. Stock reads all source lanes before its stores. Alias or concurrent source mutation can therefore change behavior; six call-site local-buffer/non-overlap proof must still match the installed executable. Do not generalize this cave to arbitrary Vec4CopyW0 callers. No direct physics/AI scheduling change.
- **BPO L: unresolved ordering risk; exclude mask 2/3/6/7 pending proof.** Replacing locked cmpxchg with mov preserves the observed value of an aligned status word under the intended inputs, but removes a full StoreLoad barrier and locked RMW behavior. The shared ObjResTreeAllReady site has nine additional callers, so the scope is wider than this bounds routine. The prior note's assertion that there is no Dekker pattern is not a complete writer/caller publication/lifetime audit. Require all ready-word writers, preceding stores, subsequent reads, ownership/reclamation and interaction with baseline LK/MA to show the fence redundant. A concurrent load/transition smoke test cannot prove this ordering property. Stale readiness/resource publication can affect collision/AI or object lifetime even with bit-identical arithmetic.

Conclusion: no intentional physics/AI reduction found in AABB/Q/V, but equivalence remains conditional on these data/FP assumptions and exact installed sites. Test AABB, Q and Q+V first; do not promote L based only on numerical harness success. No code/default changes are proposed by this task.
