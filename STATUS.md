# STATUS (handoff) - 2026-10-07 ~14:30
Shipped to Ally: ASI 1.5.0 (v9: W6, GF, SP profiler, MemStats, WL, ...), shader pack v2 (ghdm, esm, wdo28, mpc23, toonedge2).
Measured: W6/GF kill main-thread spins (GPU MHz up), fps flat (GPU-bound). GHDM +8.7 % at MAX. MAX native = 16.3-16.8 fps; lightest-50 = 32 fps.
Bugs: live FG ON->OFF under Proton crashes ~2 s later (log captures\ally\nioh3_ally_modlog_v9-FGoff-crash_*.log); fix = capture only the game's own queue (ECL return addr in Nioh3.exe/sl.interposer) + Proton ON->OFF = next launch. NOT DONE.
Killed at the session limit (resume with SMALL fresh agents, Sonnet unless noted):
1. v10 ASI (partial edits in mod\Nioh3EngineFix\src - check `git status`/build first): FG-off crash fix (priority), WG, TG (opt-in), LK/CP/SPF (notes\worker-jobs-opt.md), then GP GPU-pass profiler (timestamp queries at the engine debug markers; patcher\gpu_markers.fsx).
2. Culling kernel rewrite (Opus) - notes\shadows-culling.md; coordinate with gpu-shaders\sweep\ (MPC#23 already in 0x29ee0902).
3. Tiled-resource stall hunt (Opus) - notes\gpu-scheduling.md (1.35 ms device-wide stalls); just started.
4. GI/fog rewrite (Opus) - was packaging; check gpu-shaders\ for partial work.
5. Stutter/p99 (Sonnet) - partial; notes\stutter.md may be missing.
6. Exe post-effects via PSO hook (Opus) - notes\gpu-post-exe.md; was implementing psoswap.
Ally pending: ESM/WDO28 at MAX, shader v2 A/B, fast-travel MemStats test. Never use RGP on the Ally (froze it).

## 6PM PLAN (run as-is after the reset; the hourly heartbeat at :07 triggers it)
Budget: max 3 agents at once; pass `model`; briefs = goal + inputs + deliverable + "read AGENT_BRIEF.md + token-efficiency.md".
Step 0 (main session, cheap): `git -C nioh3-engine status` - see what the killed agents left half-done in mod\Nioh3EngineFix\src and gpu-shaders\. Relaunch the watcher as a background `watch_inbox.py --once`.
Wave 1 (start together):
  A. [sonnet] ASI v10 = FG-off crash fix ONLY first (queue capture limited to the game's ECL callers; Proton ON->OFF = next launch), build + pattern test + zip; then add LK/CP/SPF (notes\worker-jobs-opt.md) + WG; TG opt-in. Finish half-done src edits or revert them (git) if broken. Report -> main posts [BUILD] to the Ally.
  B. [opus] GI/fog shader rewrite: resume from gpu-shaders\ partial files + notes; the biggest same-look GPU lever at MAX (GI = 15-20 ms on the Ally).
  C. [opus] Exe post-effects: psoswap hook (new files) + motion blur/TAA rewrites (8-10 ms on the Ally). Check partial files first.
Wave 2 (as slots free up):
  D. [sonnet] GP GPU-pass profiler in the ASI (timestamp queries at the engine debug markers; patcher\gpu_markers.fsx) - replaces RGP on the Ally.
  E. [opus] Culling-kernel rewrite (merge with sweep MPC#23 in 0x29ee0902).
  F. [opus] Tiled-resource stall hunt (tilestats module + census).
  G. [sonnet] Stutter/p99 census (finish notes\stutter.md from the Ally CSVs).
Ally side: ESM/WDO28 at MAX, shader v2 A/B, then v10 tests (FG off fix, CP/SPF/WG A/Bs), fast-travel MemStats. Digest logs with re\scripts_cpu\sp_aggregate.py before posting.
Done when each item ships to the Ally or is written off in its notes file; update this file after each.

## 18:45 update
Shipped to Ally: ASI 1.7.2 (FG-off fix, GPUProf, ShaderSwap dump, TileStats/SPR probes, WG/CP/SPF installed-off), shader pack v3. MAX shader A/B: GPU W 23.1->20.5 at same fps (VSync 8.33 ms slot quantization) -> metric = GPU ms. HOLD: next shader pack v4 adds GI #121 (0xd070067d, ~2 ms Ally at GI HIGHEST, bit-exact) - send only after the Ally's 1.7.2 digest (one build per session). Running: VFSCAT fog rewrite (opus). Next after digest: rewrite top GPUProf passes + dumped motion blur/DOF/bloom via psoswap.

## 19:35 update
Ally 1.7.2 digest: FG-off fixed; GP labelled = 14.5 of 59 ms (FSR3 native AA 6.9 ms = user's setting, out of scope); tiles ruled out; CPU patches neutral; dump at D:\nioh3-re\ally-dump\shaderdump_172.zip. Sent: ASI 1.7.3 + shader pack v4 (gi). Running: VFSCAT fog (opus), GPUProf v2 per-PSO timing -> ASI 1.7.4 (sonnet, hold until the Ally's 1.7.3 digest), runtime post shaders from the dump (opus).

## 20:00 backlog (user: optimize everything possible)
Running: runtime post shaders (opus), GI reflection #414 (opus), ASI 1.7.5 = CS culling skip + AS0 async slot-0 experiment (sonnet). Held for the Ally: ASI 1.7.4 GPUProf=2 per-PSO timing (sent after the 1.7.3 digest) -> then target the top unlabelled GPU costs (material/G-buffer shaders likely). Next queue: A2 shadow double-draw check via GP markers; load hitches (DiscardVirtualMemory at stage teardown, needs arena RVAs); FSR3 shader micro-opts (AMD code, low odds); particles; PC tiny-draw instancing (hard).

## 21:50 - session restart + REGRESSION
Ally 1.7.5 GP2: all replacements ON = 0.4-0.6 fps SLOWER than all off. Pairs: sync CS 4.82 vs 3.35 ms (likely gi #121/#414), async CS 3.99 vs 3.40 (likely fog). Cause: RGA (Windows AMD compiler) models don't predict vkd3d->dxil-spirv->RADV/ACO. RULE: no shader ships to the default set without an Ally A/B win. AS0 (async slot 0): stable, no gain. GP2 stamps cost ~14 ms (only for attribution). Running: hash mapping + keep/drop table (sonnet). Ally next: 1.7.7 + v6, per-shader A/B w/o GP2. Killed by the restart (resume later): materials/75% (opus, partial gpu-shaders\materials\), vkd3d audit (opus, nothing written).

## 23:00
Done: vkd3d audit (barrier COMMON->full waits est. 0.2-2 ms Ally; needs a trace: ASI 1.7.9 per-caller trace in progress), perf decompile coverage 56.5->96.6 % of game CPU, materials trim (nothing safe; trvs/pbvs ~0), GP2 sampling (ASI 1.7.8, held). Ally: post-reboot session C0..C5 running; then the barrier trace. 'apply all' banned (contains dropped items). Pending for the RADV pre-screen: VCLOUD hoist, SFB dedupe.

## STANDING GOAL (user, 2026-10-07): everything performance-related decompiled
Done = (1) >= 99 % of profiled game CPU self time in functions with REAL names (Unknown_* doesn't count; now 82.0 % real + 14.5 % Unknown_), measured by `py -I re\scripts_re\perf_coverage.py --self`; (2) every engine code path behind each GPU pass in the GP2/RGP tables named (now 59/59) [metric fixed 2026-10-08: the old 63 counted 4 non-functions (command-buffer ids, ESM file offsets, a D3D12Core offset); the 4 real misses are named in incoming/gpu-passes-renames.tsv, pending merge]; (3) every D3D12 call site (barriers, binds, submits, copies, PSO/heap creation) named; (4) each subsystem that costs >= 0.3 % has a short notes entry (role, data structures, hot loops). Keep 1 RE agent on this until done; new profiles (Ally SP/GP2 logs) re-open it.
ORDER (user): decompile in DESCENDING performance impact - CPU by profiled share (Ally first), then the engine code behind the costliest GPU passes by Ally GP2 ms (FSR3, fog, GI, sky, shadows, culling, PB2 submission, foliage), then the systems that decide how much work is done (LOD selection, streaming budgets, culling thresholds, quality-setting -> pass parameter tables, update scheduling).

## PC test rounds (turn-taking with the Ally)
Steam allows one device at a time. On '[GAME-FREE]' from the Ally: launch a Sonnet agent for the PC round (L1 mod defaults: log+overlay+F10 menu screenshot+PresentMon 60 s; L2 W6=1 GF=1; L3 GPUProf=2 sampled + SP + MemStats 90 s -> gp2_digest/sp tables; restore the ini; captures\pc\), then send '[GAME-RELEASED]'. On '[Q] need game': close Nioh3 within 2 min and release. Save backup: D:\nioh3-re\save-backups\.
PC round must take the bridge game lock first: py -m uo_bridge.gamelock acquire nioh3 (UO_AGENT_ID=pc), renew during the run, release at the end. Work-deciders done (no same-look change in the quality tables). CPU perf names 99.5 %; GPU passes mapping agent running.

## QUEUE (refill agents from here; keep 3-4 running, model per job)
0. [main] ASI 1.7.14 built (GL1/LC/LW, default off, untested in game): install on PC after the current test round, then LoadStats A/B per flag (baseline vs GL1 / LC / LW alone, same load route); same on the Ally next session.
1. [opus] Ally barrier-trace follow-up: when the 1.7.10 trace CSV arrives -> vkd3d_cost.py -> build/keep R2a/R1 per counts.
2. [sonnet] PC test round on [GAME-FREE] (see 'PC test rounds').
3. [opus] RADV prescreen follow-up: when the Ally tool works, screen VCLOUD hoist + SFB dedupe + any new rewrite.
4. [sonnet] Live read on PC (when free): meshlet LOD per-category values (see work-deciders addendum).
5. [opus] Next decompile area by impact after the current 4 finish (perf_coverage.py --self gaps, GPU chart grey buckets).
6. [sonnet] README/charts refresh after each merge; device-specific README pass once the Ally hardware IDs arrive.

## FINAL PHASE (user, 2026-10-08): benchmark rounds on PC + Ally X at the END, with the user present
When the queue is done and every kept change is checked: prepare a benchmark protocol (same save/spots, stand + move + combat + a level load, MAX and the user's preset, stock vs mod-all-kept, 3 runs each, GPU W/MHz + fps/1 %/p99 + load seconds; PresentMon on PC, MangoHud on the Ally) and ASK the user to schedule it. PC and Ally run one after the other (Steam one device at a time). Results -> README + Discord.
PC round add L4: [Patches] AS0=1 (async slot 0) - notes\particles.md: on AMD the effect GPU recording runs on the PC main thread (6-7 % of [App]Game); AS0 moves it off (Ally: stable, no fps gain). Ally ask: one COMBAT window per session (effects/anim costs only measured standing). Notes corrections pending (particles.md section 6: ui-mainthread.md, mainthread-logic.md, invisible-gpu.md).
PC live-read item (queue 4) is fully specified in notes\work-deciders.md addendum A4 (meshlet LOD table at [Nioh3+0x45B9E30]+0xA08). MODEL_QUAL = dead setting in v2.02 (confirmed twice). Grass level->instance maths: needs a live watchpoint on placement unit +0xFC.
FINAL PHASE add (user): before the final benchmarks, RE-TEST EVERY experiment on BOTH PC and Ally X - all ASI flags (W6 GF WK WG CP SPF LK ED IK TG RWb AS0 Barriers R2a R2b R1 PP FPC B AABB M1 M2 J1 E1 S1 LM MQ/PT1/RR1 ...) and all shaders incl. the dropped ones (gi #121/#414, fog, toonedge2, trvs/pbvs) + post-v1 + any new ones - one at a time vs all-off, then the best combination. Build a test matrix script so it runs mostly unattended (gameplay-gated, live menu switches where possible, relaunch for next-launch flags). Kept/dropped verdict per device goes into the README.

## PC LIVE SESSION (after the static decompile agents finish)
Ask the Ally to close the game ('[Q] pc needs game' -> it sends [GAME-FREE]), take the game lock (gamelock acquire nioh3), then one Opus/Sonnet agent does all live-only work in one PC session: meshlet LOD table read (work-deciders addendum A4), grass placement +0xFC watchpoint (level -> instance maths), runtime lookup of the 11 unnamed parameter hashes (dump the live name tables / property dictionaries, kidshash.py), page-pool hit/miss/queue-full counters (streaming-anim R1), alpha-0 particle counts (particles.md #3), plus the PC test round (L1-L4) and any live-only decompile gaps. Release the lock + [GAME-RELEASED] at the end.
Loading (notes\loading.md): title->level 118 s is fixed-length (timed scene) - not speedable; other loads 84-132 s: est. 3-10 s/load via RWb/WG/GF during loads, LM (swap), more in-flight requests (10->16), event wake instead of 1 ms poll, faster zlib; LoadStats -> ASI 1.7.11 (sonnet). Ally load test after it ships.
QUEUE add: ASI 1.7.12 (sonnet) = port MQ (bit-exact SSE matrix, 2.75 M inputs 0 diffs, -53 %/call) + PT1 (tile lock per texture) from patcher\mq_live.fsx / pt1_live.fsx (ASI caves in notes\streaming-anim.md s4) + any collision-opt winners - after the collision agent reports; add them to tools\testmatrix\matrix.json. RR1 dropped (not provable).
Game logic (notes\gamelogic.md): MissionThread ~14 % of a core, off the critical path; only GL1 (prefetch in 3 list walks, ~0.05-0.1 W Ally) is clean - needs a live task-count read (add to the PC live session). TG stays the real lever (opt-in, behaviour change).
