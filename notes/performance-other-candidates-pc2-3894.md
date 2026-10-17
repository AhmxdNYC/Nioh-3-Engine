# Three additional performance candidates (PC2, Task 3894)

2026-10-14. Verdict: **three source-backed hypotheses worth small diagnostic A/B tests; zero new measured gains**. These are possible mod-induced stalls, not newly proven stock-engine bottlenecks. No source patch, installation, game launch, save access or benchmark was performed for this explicitly read-only task. The prerequisite Task3639 gate work is complete (notes/gameplay-gate-pc2-3639.md).

## Ranking

| Rank | Candidate | Concrete source fact | Why test it | Runtime evidence required |
|---|---|---|---|---|
| 1 | Overlay resource reuse blocks the Present caller | Four-slot overlay ring; an unfinished slot causes a wait with a 50 ms timeout before native Present | Direct path to an avoidable CPU-side stall during GPU backlog; very small diagnostic change | Nonzero slot waits aligned to gameplay hitches, with reduced end-to-end tails when bypassed safely |
| 2 | Replacement shader files are read synchronously at first use | An indexed, unloaded replacement is opened, read and parsed on the PSO creation caller | A first-effect hitch may contain mod file-I/O time even when the driver's shader cache is warm | Replacement reads actually occur during the fight and contribute material caller time |
| 3 | Every mod log line flushes the file stream on its calling thread | Common Log calls fputs then fflush; shader observation calls it inside PSO preparation | Logging can amplify first-use bursts and serialize callers; distinct from the already-fixed debugger-output overhead | Nontrivial file-write/stream-lock time on the critical thread, removed by buffering the same records |

Ranking reflects how directly the mechanism can delay presentation and how cheaply it can be falsified, not an estimated FPS uplift. No saving in milliseconds is inferred from the 50 ms timeout. If a mechanism never fires in the chosen fight, classify that test NOT EXERCISED rather than calling the candidate a win or a universal nonissue.

## Scope and duplicate screen

Reviewed the named references: worker-threads.md, pc-cpu-next.md, plus pc-supplied bridge files 440 and 441, saved as cpu-next-candidates-2026-10-13.md and meshlet-cpu-audit-20261013.md (download SHA verification passed). Screened the 263 Markdown files in notes/ by mechanism terms and read the relevant surrounding notes: stutter/stutter-pso, boot-shader-compile, osd-design, gpu-post-runtime, backlog-small-wins, perf-coverage, whats-left, loading-bottleneck and memory-leaks.

Excluded existing RD drain waits, M1/M2/M2S/J1/MA, worker wake/power patches, AABB/bounds, page-pool scans, draw preparation, CBV/ReBAR, generic PSO journaling/prewarm, shader rewrites and known loading-gate investigations. In particular, the newer meshlet audit supersedes the old worker note's immutable-list assumption; it is not a new candidate here.

The related notes mention OSD queue overhead and an overlay-off bisect, replacement shaders, and SkipDebugString. The three **specific mechanisms** ranked here (50 ms slot reuse wait, lazy replacement-file I/O, per-line CRT flush) were not found as separately investigated candidates. This is a local-corpus duplicate screen, not a claim about every unpublished fleet document.

Source basis: local HEAD cad8a5588a5c6b87a97546bd1aa7c177a6371466; fetched origin/main 556deac8a2d65a0e9b58607c9ed23793fb239e14. The three cited source files match origin/main after newline normalization. Pc supplied the two newer notes from its reported commit 4736f43; no source from that commit was supplied. Revalidate these paths against the exact candidate source/binary before implementing a test.

## Common minimum test contract (applies separately to every candidate)

Use the existing fightnow/enter_game and capture tools with the completed gameplay gate; do not build another launcher. Use Yoritomo R7B35 for **both arms of each candidate**, the same fresh baseline save, equipment, boss phase/start, scripted combat/camera route, configuration, resolution, FG state, cap and warm-up policy. Measure **at least 60 uninterrupted seconds of actual combat per arm**; idle mission time, title, loading, menus, popups, death transitions and cutscenes do not count. Restart the affected arm if the fight cannot provide the full window.

Before each arm: acquire nioh3-pc2, back up current saves/config/mod files, start the installed hidden Steam autoplay wrapper before launching, confirm fullscreen and foreground, inspect fresh screenshots, and prove character control with the existing gate. Retain start/end gameplay screenshots for A and B. Log foreground HWND/PID and fullscreen/display-state evidence before and throughout each window; any focus/fullscreen loss invalidates the whole arm. Existing approximately 1 Hz polling alone must not be described as proof that no shorter loss occurred; supplement with foreground event tracking where available. Do not use an OSD visibility check as the focus proof.

Use the same recorder in both arms: per-frame times, aligned process CPU, GPU busy/clock/VRAM when available, and per-candidate bounded in-memory timing counters. Report p50/p95/p99, counts above 33.3/50/100 ms, actual duration, frames and mechanism count/time. Clearly label unavailable sensors. Separate load/request-to-control timings from combat metrics. Keep gate checks and instrumentation identical in both arms; avoid logging each probe event through the logger being measured. Restore and hash-check the backed-up files; release only this run's lock.

One valid 60 s A + 60 s B pair is the smallest diagnostic screen, not shipping evidence. A positive result needs a reverse-order pair and repeatability above observed same-arm drift before a performance verdict. A hitch correlation alone does not establish causality. Do not reuse the invalid Task3578 CPU numbers.

## 1. Overlay ring wait before native Present

Evidence in mod/Nioh3EngineFix/src/osd.cpp:

- Line 79 defines kRing = 4. DrawFrame chooses c.frame % kRing at line 959.
- Lines 960-965 wait when the chosen slot's fence is unfinished: SetEventOnCompletion followed by WaitForSingleObject(c.ev, 50), then skip on failure/timeout. The comment says to avoid stalling, but the wait is already on the caller's path.
- OnPresent invokes DrawFrame at line 1153. HkPresent calls Hooked before g_origPresent at lines 1194-1197; Present1 has the same order at 1208-1211.

**Hypothesis:** occasional GPU lag holds up native Present for overlay allocator/upload reuse. The GPU might already be the limiter, so removing this wait can improve caller time without improving displayed frames; measure both. Four slots may already prevent this branch in ordinary fights.

**Smallest in-game comparison:** one diagnostic binary with a narrowly scoped switch, A = existing wait, B = immediately skip this overlay draw when its slot is unfinished. Keep the Present hook, menu/input services, frame counter, telemetry and all game rendering active in both. Keep the OSD visible in both arms so A exercises the path. Run the same Yoritomo fight for >=60 s per arm with the common focus/fullscreen/gameplay checks.

Record slot-busy count, requested fence/completed fence, cumulative/max wait, skips and original-Present duration in a fixed memory buffer. Never reset or write an unfinished allocator/upload slot in B. A transient missing overlay counter is an explicit diagnostic tradeoff; missing game content is not allowed. Screenshot comparison should inspect the game area independently of the overlay.

**Decision:** promote only if A has actual slot waits and B removes aligned frame/latency spikes without new failures or gameplay differences. If no slot waits occur, stop this route's investigation. Keep it distinct from disabling the entire overlay, which could also remove other Present-hook services.

## 2. First-use replacement-file I/O in PSO creation

Evidence in mod/Nioh3EngineFix/src/psoswap.cpp:

- Repl.state starts at 0 (line 244). GetReplIn checks the indexed map under g_lock, then releases the lock before loading (lines 266-290).
- ReadWholeFile performs CreateFileW/GetFileSizeEx/ReadFile synchronously at lines 252-263. The caller parses/validates the result and publishes state at lines 290-308. Successful and unusable results are cached: this is **not an every-frame file read**, nor a missing-file directory scan.
- Consider -> GetRepl (lines 609-620) runs in the PSO planning path; HkGfx -> DoGfx is at lines 919-922, with compute and stream equivalents nearby.

**Hypothesis:** the first fight effect referencing an as-yet-unloaded replacement pays storage/parse time on the PSO caller. Repeated requests after state=1 do not. Some/all replacements may already load during boot, making gameplay benefit zero. Two callers can also both observe state=0 and perform a read before only one publishes; count duplicate reads rather than assuming they occur.

**Smallest in-game comparison:** same diagnostic binary and identical replacement-file hashes; A = existing lazy loading, B = preload and validate the same indexed files into the same map during title idle, before entering the fight. B must preserve stage/key/runtime rejection checks and native fallback; do not disable replacements or change shader bytecode. Fresh process for each arm; warm driver cache consistently beforehand and do not delete it. Capture the first >=60 s of the same scripted Yoritomo combat after control verification in both arms, with focus/fullscreen evidence throughout. Keep PJ/PJH/ShaderHot and all other switches fixed.

Record first-use key/thread/QPC, read and parse time, bytes, duplicate loads and PSO driver-call time separately. Capture entry/loading time as a separate metric so moving work earlier is visible. Include a route action that uses a replacement first seen in the fight; if all files have loaded before control, mark NOT EXERCISED and do not manufacture a gameplay result by counting loading frames.

**Decision:** promote if A actually reads during gameplay, B eliminates that synchronous work, matching driver-call behavior and unchanged replacements accompany lower hitches, and extra startup time/RAM are acceptable. Warm OS file cache can make the effect negligible; report it honestly rather than asserting disk latency from a file-read call alone.

## 3. Synchronous log output on performance-sensitive callers

Evidence in mod/Nioh3EngineFix/src/dllmain.cpp: Log formats/timestamps each record at lines 394-403 and calls fputs plus fflush at lines 407-409. SkipDebugString only controls OutputDebugStringA (lines 404-406); it does not remove file output. In psoswap.cpp, LOG forwards to the common callback at line 96; Observe logs first-seen executable shaders at lines 493-520, before replacement lookup in Consider. GetReplIn also logs loaded replacements at lines 309-313.

**Hypothesis:** first-use bursts add synchronous stream writes and possible CRT stream-lock contention on a PSO creation caller. A background-only stats write is not automatically a critical-path cost. fflush drains CRT buffering; it is **not** FlushFileBuffers and does not prove a physical disk flush or a disk-sized stall.

**Smallest in-game comparison:** diagnostic binary with A = current logger and B = retain the exact same formatted records in bounded preallocated memory during the measured window, then write them after recording stops. This isolates stream I/O; leave formatting, log levels, SkipDebugString, shader settings and all other probes identical. Do not disable diagnostic families or simply redirect to a null sink. Run the same Yoritomo fight for >=60 s per arm, with the common focus/fullscreen/gameplay proof. Keep gameplay-gate evidence on the existing independent harness output path.

Record logger call count by thread, bytes, timing of fputs/fflush, and bounded-buffer occupancy/drop count without recursively logging those measurements. Buffer overflow invalidates the diagnostic comparison; all records must be retained. Prefer a route exercising the same first effects in fresh processes; avoid artificial log spam. File-I/O/CPU-stack tracing can distinguish stream locks/writes from driver compile time if already available.

**Decision:** promote only if output cost is present on a critical caller and removing just that cost improves frame tails repeatably with zero dropped records. If cost is confined to the background logger or below noise, defer. Any eventual production buffering design must preserve bounded memory, record ordering and error/exit flushing; the diagnostic memory buffer is not a shipping implementation.

## Deliverable status

Read-only hunt complete: 3 ranked candidates, 3 concrete paired gameplay test designs, 0 runtime claims. Pc owns selection and scoped implementation/testing. This worker has no game lease or running test and does not change ownership of the independent boss-load/CPU tasks.
