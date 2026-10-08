# STATUS (handoff) - rewritten 2026-10-08
Read this section only; everything older is under '## History' (unchanged). Latest commit when written: 7b4bd34 (ASI 1.7.21).

## CURRENT STATE
- Latest ASI = 1.7.23 (1.7.23: ReadProbe / LoadOnlyCpuFixes / EP, default off, not run; earlier: 1.7.21 built from mod\Nioh3EngineFix\src; RematchLog Phase 0 + BossLog + LoadGates probe LG; all new flags default off, none run in game yet except LG on the Ally in 1.7.19 form).
- PC game folder: ASI 1.7.21 + full-mod ini with [GpuOpt] Barriers=0 (Barriers=1 measured -7 % on PC). Last PC round was 1.7.16 (captures\tm\results_pc_1716.md).
- Ally X: 1.7.21 zip delivered. Last Ally run was the 1.7.19 load probe (digest notes\loading-ally-fixed.md section 7).
- Game holder: the Ally was the last holder (Steam allows one device). Verify before acting: `py -m uo_bridge.gamelock status nioh3`; never assume the PC may launch.
- Findings to rely on: Ally load work ends at +93 s (manager A capped at 10 in flight, 957 pending, 640k ReadFile = 68 s in-call vs 3.2 s PC; no timeout; G9 'held' = tip-page artifact). PC 58A window ~25.8 s, last ~15 s unexplained by the managers. Shaders: no shader ships without an Ally A/B win (RGA model does not predict RADV/ACO). B/PP/GF/MQ 1.7.16 live gains are unconfirmed.

## RUNNING / PENDING (everything here needs the game)
1. PC (needs game free): ABBA live retest per notes\pc-keep-candidates.md section 4 (MQ, PP, B, GF; OFF sentinels; one launch; CpuStats=1) + LG load test (fullmod_ini_1717_lg.ini, LoadStats=3 LoadGates=1 MemStats=1 SP=1; 1 shrine fast travel + 1 death respawn; loadstats_ab.ps1). Also one gpuopt trace (Barriers=2 TraceRecords=2000000) with B=0 vs B=1.
2. Ally (needs game): load A/B step 1 - L1 ini vs +GF=1 W6=1 RWb=1, x2 each; metric = last LoadGates eval / manager A last busy. After the read fix is built: read-fix test. Also GF/W6/RWb-during-loads test.
3. Phase 0 boss session: needs THE USER present (play steps in notes\boss-rematch-design.md "How to run Phase 0": BossLog=1 RematchLog=1, offline; Battle Scroll menu, replay a boss, use a Picture Scroll, die once, quit mid-fight, 10 min normal play). Do not start it unattended.
4. User-pending on the Ally: the title-screen tip-page press during load tests; the Phase 0 session; scheduling the final benchmark; Discord posts. Ask via the bridge, do not guess.

## QUEUE (ranked; model per job; keep 3-4 agents running)
1. [ally] Session R1 on ASI 1.7.23 (BUILT 2026-10-08: ReadProbe=1|2 + LoadOnlyCpuFixes=1 + EP=1, all default off; notes\loading-ally-fixed.md 8.4/8.6). Route: same save, online title -> Load Game -> Level 136, press A at the tip page; each launch = 2 loads (#1 cold, #2 warm), 60 s in game between. Base ini = 1.7.19 L1 (LoadStats=3 LoadGates=1 MemStats=1 SP=1) + ReadProbe=1 in EVERY arm. ABBA x 4 launches: B = base, A = base + LoadOnlyCpuFixes=1; first B launch with ReadProbe=2 (send Nioh3EngineFix_reads_*.bin/.txt). Then EP ABBA: A = base + EP=1. Metric = work end (last failing LoadGates eval / A last busy), keep if both A-B pairs beat the B-B spread; check MB read within 1 %, VRAM +60 s within 2 %, screenshot 5 s after the press. Send back: grep 'LoadStats|LoadGates|ReadProbe|LO:|EP:' of each log. Then [opus] the RP-chosen fix (RC / AQ / TSY / SH, 8.3) = session R2; [sonnet] rp_sim.py for the trace.
2. (merged into 1: LO loads-only gating ships with RP.)
3. [opus] PC submit filters: PSO/RT redundancy (SF1 + PM key memo, SF3 OM/RS) per notes\pc-submit-overhead.md; build only if the trace shows >= ~200 redundant calls/frame; run submit_redundancy.py on the one trace run first; B draw-loss check by screenshot pair.
4. [opus] Rematch build after the Phase 0 logs (notes\boss-rematch-design.md; boss-master.md has the data). Blocked on the user session.
5. [sonnet] settings-cost.md: fill measured columns when benchmark data exists; README/charts refresh after merges.
6. FINAL PHASE (end, user present): notes\final-benchmark-protocol.md. First the full re-test matrix (below), then STOCK vs ALL-KEPT, MAX and user preset, 3 runs, interleaved; PC and Ally one after the other; results -> README + Discord. ASK the user to schedule it.

## RULES
- Game-side only: no TDP/boost/governor/env-var/gamescope changes; only how the game uses the hardware.
- While the PC game runs: max 3-4 agents, NO Ghidra/heavy RE.
- Take the lock before launching: `py -m uo_bridge.gamelock acquire nioh3` (UO_AGENT_ID=pc), renew, release at the end; '[Q] need game' = close within 2 min.
- Save backups before each game session (D:\nioh3-re\save-backups\) and back up the ini/asi you replace.
- Bridge = `py -m uo_bridge` (run from the repo root). Read AGENT_BRIEF.md + token-efficiency.md in every agent brief. 'apply all' is banned; one build per session; no shader ships without an Ally A/B win.
- Update this top section after each finished item; keep dated detail in MODLOG.md / notes.

## DROPPED-BUT-KEPT (re-test every one on PC and Ally in the final phase, one at a time vs all-off)
- Experiments: VBC/S1 (not exact), HBAO draw merge (nothing to merge), ZI, RR1 (not provable), OJ/HT/PSO-warmup (load; no timeout), R2a/R2b/R1 (Wine-only no-ops), Barriers=1 (-7 % PC), AS0 (neutral), GL1/LC/LW/WL/DLCP (neutral on PC), W6/PT1/DL (~0 on PC), TG (opt-in, behaviour change), WK WG CP SPF LK ED IK FPC AABB M1 M2 J1 E1 LM.
- Shaders: gi #121 / #414, fog (VFSCAT), toonedge2, trvs/pbvs, post-v1, VCLOUD hoist, SFB dedupe (RADV prescreen pending).
- Also in the matrix: B, PP, GF, MQ, MQ/PT1 and any new flags; tools\testmatrix\matrix.json is the list. Verdict per device goes into the README.

## History
(older sections, unchanged)

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
-3. [ally] (no build; ASI 1.7.19) LOAD A/B for the ~93 s Ally load work (notes\loading-ally-fixed.md §7): the 1.7.19 probe shows no timeout. Work ends at +93 s (manager A at cap 10 until +93.5, 957 pending, 34k no-progress polls; 640k ReadFile = 68 s in-call vs 3.2 s PC); then the tip page waits for the press (G9 'held' = probe artifact). 7.5 cores = idle spin baseline. Step 1: L1 ini vs +GF=1 W6=1 RWb=1, x2 each, metric = last LoadGates eval / A last busy. Step 2 [sonnet, new file]: RP read probe (per-site calls/in-call/thread pool/offset locality, SRW hold at 312F7C/7401F0) + LoadGates tail fix -> decides RC read cache / AQ async archive reads / TS TrySync 3->6. OJ/HT/PSO written off.
-2. [sonnet] (BUILT in ASI 1.7.17; run on the next PC turn) LOAD probe LG + LoadStats=3 (spec: notes\loading-bottleneck.md §5; new code in loadstats.cpp, default off): 9 gate call-site hooks in LoadIsLoadEnd 3B3424 + FastTravelSeq/WarpSeqB state polls + ticks outside 58A. Then PC load test: ini fullmod_ini_1716 + LoadStats=3 LoadGates=1 MemStats=1 SP=1 ReportSec=5; loadstats_ab.ps1 BASE_a BASE_b (+ harness timestamps), 1 shrine fast travel, 1 death respawn; Ally: same flags, title->Continue x2 + 1 fast travel. Why: PC 58A window 25.8 s, resource managers drain at +10..13 s and LC/LW/GL1 = 0 -> the last ~15 s is an unmeasured LoadIsLoadEnd gate / sequencer state; the gate that holds picks the fix row (§4).
-1. [sonnet] Next PC turn: repeat live A/B for B (matrix B = GALLOP 3BC17, NOT barrier batching; batching = 'Barriers', measured -7 %), PP, GF, MQ. The 1.7.16 live numbers are suspect: B/PP gpu_ms moved -16 %/-8 % (a CPU patch cannot), GF/MQ p99 gains are one odd stand1 window, and the PP/W6/MQ counters read 0 in the logs. Full design: notes\pc-keep-candidates.md section 4. Short form: CpuStats=1, F10 toggles read back from 'MENU:' log lines; one launch, per item 30 s stand windows ABBA-ABBA (off on on off off on on off), order MQ, PP, B, GF, OFF sentinel between items; pair rejected unless gpu_ms ON/OFF within 1.5 %, counter nonzero when ON, sentinels within 0.5 %; KEEP = median ON-OFF >= +1 % with the same sign in both halves and median p99 not worse > 2 % (single windows never count; in-launch noise ~0.2 %). Then cross-launch BASE,REC,BASE,REC,BASE,REC (REC=B+PP+MQ+GF+W6): 3/3 positive pairs and mean > max(2 %, 1.5 x paired spread); the 5 % launch noise applies only across launches. Expected combined +2..+4 %. Recommended inis: same note section 5. Results so far: captures\tm\results_pc_1716.md (GL1/LC/LW/AS0/W6/PT1/DL = 0). Dropped-but-kept for the final re-test: VBC (not exact), HBAO merge (nothing to merge), ZI.
esults_pc_1716.md (GL1/LC/LW/AS0/W6/PT1/DL = 0, Barriers -7 %). Dropped-but-kept for the final re-test: VBC (not exact), HBAO merge (nothing to merge), ZI.
-0.5 [sonnet] PC submit overhead (notes\pc-submit-overhead.md): set [GpuOpt] Barriers=0 (and R2a/R2b/R1=0) in the PC ini now (Barriers=1 = -7 %, R2a/R1 are Wine-only no-ops). B is draw-list gallop, NOT barriers; B on drops PC GPUBusy 5.01->4.22 ms = suspected draw loss -> next PC turn: gpuopt trace (Barriers=2 TraceRecords=2000000) with B=0 vs B=1 -> submit_redundancy.py draws/frame + screenshot pair before KEEP; same trace gives exact PSO/root/OM redundancy -> build SF1+PM (PSO skip + key memo) if >= ~200/frame, SF3 (OM/RS) after the trace hash column; SP dispatcher Barriers 2 vs 1 to pin the regression.
0. [main] ASI 1.7.14 built (GL1/LC/LW, default off, untested in game): install on PC after the current test round, then LoadStats A/B per flag (baseline vs GL1 / LC / LW alone, same load route); same on the Ally next session.
1. [opus] Ally barrier-trace follow-up: when the 1.7.10 trace CSV arrives -> vkd3d_cost.py -> build/keep R2a/R1 per counts.
2. [sonnet] PC test round on [GAME-FREE] (see 'PC test rounds').
3. [opus] RADV prescreen follow-up: when the Ally tool works, screen VCLOUD hoist + SFB dedupe + any new rewrite. (HBAO draw merge: written off 2026-10-08, not a candidate. The Ally already issues 1 HBAO draw per frame (nSwitchRef[1..3] permutations), see notes\render-nodes.md.)
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

## PC round 2026-10-08 (ASI 1.7.16) - done, game released
UX check PASS (captures\pc\ux_1716\): top-right reminder always on, Esc x3 opens and closes the mod menu with no game pause menu, rows "Mod menu reminder (top-right)", "Show FPS counter" and FG status present, counter sits below the game HUD. Matrix: captures\tm\results_pc_1716.md. Launch-to-launch BASE spread is ~5 % (144.7/144.5/152.5), so AS0, WL, DLCP, GL1, LC, LW are all NEUTRAL (p99 within 2 %). In-launch live A/B vs BASE-live: B +15.6 %, PP +5.3 %, GF +4.1 % (p99 -12 %), MQ p99 -11 % (fps +0.7 %), Barriers -7.1 %, W6/PT1/DL ~0; the B/PP/GF/MQ gains need a repeat before KEEP (one window each had an odd p99). LoadStats 58A window: BASE 25.3/26.2 s, GL1 25.8, LC 25.4, LW 26.4/25.7 -> no load gain. Harness fixes: $t loop var clobbered $T in run_pc.ps1 batch (fixed), loadstats_ab.ps1 added. verdict_pc.md uses the wrong BASE for the live batch; use results_pc_1716.md.
