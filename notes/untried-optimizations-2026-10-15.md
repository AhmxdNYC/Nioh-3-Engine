# Untried or partly tested optimizations, ranked (2026-10-15)

Desk study only. The game was not run and no source was changed.

Docs read: `notes/perf-opportunities-pc-20261015.md`, `whats-left.md`, `remaining-work.md`, `backlog-small-wins.md`, `pc-cpu-next.md`, `pc-keep-candidates.md`, `cpu-next-candidates-2026-10-13.md`, `combined-cpu-trial-2026-10-13.md`, `cpu-stacks-current-2026-10-13.md`, `cpu-bound-midrange.md`, `root-cause-vram-streaming.md`, `rdna2-meshlet-slowdown.md`, `stutter.md`, `stutter-pso.md`, `frame-io-next-20261011.md`, `material-pb2-next-opportunities.md`, `pb2-pixel-uv-hoist.md`, `compute-hash8-specialization.md`, `meshlet-completion-wait-candidate.md`, `optimization-retention-and-ally-playtest.md`, `ibaraki-offon-result-2026-10-14.md`, `.handoff/open.md` and `.handoff/next.md`, `STATUS.md`, the tail of `MODLOG.md`, the mod README, and `universal-optimizer/TECH_DEBT.md` (no optimization items in it).

I did not open every one of the roughly 300 notes. The boss, training and rematch research notes were only sampled, since they are not performance work.

Filters applied, per the user's rules:
- Game-side mods only.
- No OS or driver tuning. ReBAR/SAM and TDP are excluded.
- No graphics-quality reductions.
- A claim needs a measured result in both arms (OFF and ON).
- Items already measured as a loss or neutral are not listed. Examples: Barriers on PC at -7%, GL1/LC/LW, AS0, W6/PT1/DL, PL, B, ESM1, MSV, and the CbvMirror copy designs.

Current baseline, from `ibaraki-offon-result-2026-10-14.md`:
- PC with the CPU bundle: +3.3% fps, 1% low +20% (44.6 to 53.4), CPU -10%.
- Ally: -3% fps, CPU -7 percentage points. Run-to-run noise is about 4% on PC and about 0.8% on Ally.
- The average-to-1%-low gap (99.5 vs 53.4 fps) is the largest unexplained PC cost.

Gain labels: **M** is measured, **E** is an estimate from shares or a model, and **?** is unknown. Every estimate below is an E and is not a promised result.

---

## 1. PJH: ship PSO journal and title prewarm, and test it on a real boss route
- **Source:** `notes/perf-opportunities-pc-20261015.md` #1, `stutter-pso.md` s.1, `stutter.md` 6.1, `remaining-work.md` 2.6. `STATUS.md` l.797 lists "TR/PJH on PC (no hitches on route)", which is a "no evidence on that route" verdict, not a "does not work" verdict.
- **Idea:** `[Patches] PJ/PJH` is built and default 0. It journals the runtime PSOs, prewarms them at the title on one low-priority thread, and hands the results over to the game.
- **Expected gain:**
  - M: prewarm of 15,732 PSOs in 2.7 s, runtime creates 81 to 43 (-46%), and zero creates >= 1 ms in 4 launches. That route had no hitch to cure.
  - E: the boss-kill PSO hitches (102 to 448 ms) and about 25 creates >= 1 ms per session disappear, which lifts the 0.1% and 1% lows. The effect on average fps is about 0.
- **Risk:** low to medium. About 64 MB of journal, disk writes at boot, and dependence on the driver cache. Same picture, since the descriptors and shaders are identical. The worst case is a wasted thread at the title.
- **Smallest test:**
  - Fight the same boss twice from a fresh journal.
  - Arms are PJH=2 (installed, off) and PJH=1, 2 launches each, using `tools/pj_hitch.py`.
  - Metrics are frames > 50 ms and > 100 ms, p99.9, and SPR creates >= 1 ms in the boss window.
  - Pass is >= 50% fewer frames > 100 ms with p99 not worse.
- **Devices:** PC first. Then PC2, which has a lower tolerance for hitches. Ally last, because PSO hitches were not seen there.

## 2. Isolate each reviewed CPU fix with same-binary ABBA (RD, M2S, MW, J1, DQ, P1E7)
- **Source:** `perf-opportunities-pc-20261015.md` #2, `cpu-next-candidates-2026-10-13.md`, `combined-cpu-trial-2026-10-13.md`, `meshlet-completion-wait-candidate.md`, `renderer-drain-wait-candidate-2026-10-13.md`, `meshlet-cpu-audit-20261013.md`, `remaining-work.md` 2.5.
- **Idea:**
  - RD is the renderer-drain wait. Its target, 418F40, is 7% of main-thread self samples.
  - M2S is the seed-read of the meshlet capacity-sum loop. 48E90 is 15.6% self on its worker.
  - MW is the manager+5F8 completion wait.
  - J1 is the 8 job-queue loads.
  - DQ is the 200 us logic-packet retry quantum.
  - P1E7 is the target loop.
  - All are built and default off or merged. Only the bundle has been measured: +3.3% PC, -3% Ally. No single switch has been isolated.
- **Expected gain:** E: each is <= 0.2 ms/frame, or 1 to 3% CPU. M: the bundle is -10% CPU on PC.
- **Risk:** medium for RD, M2S and MW because of cross-thread ordering (reviewed offline only). Low for DQ and P1E7. M1 and full M2 stay excluded.
- **Smallest test:**
  - One switch at a time, ABBA, at a fixed location with `MaxFps 240` so the cap does not compress the result.
  - 30 s settle and a 60 s capture. Noise of about 4% needs at least 2 quads (ABBA-ABBA).
  - Keep a switch if CPU is >= 3% lower, or fps is >= +1% with p99 not worse by more than 2%.
  - The Ally -3% fps in the bundle makes the Ally no-harm pair mandatory before any Ally ship.
- **Devices:** PC, PC2 and Ally. The PC2 and Ally controlled pairs never completed. PC2 also needs its recorder fixed first (`open.md`, first unchecked item).

## 3. Explain the 1% low (pacing census), then target the dominant class
- **Source:** `perf-opportunities-pc-20261015.md` #3, `whats-left.md` s.5, `stutter.md`, `remaining-work.md` 2.7.
- **Idea:**
  - Step 1 is analysis only. Run `re/scripts_stutter/pc_spikes.py` on the existing Ibaraki and Yoritomo PresentMon CSVs. Label the slowest 1% of frames CPU, GPU or WAIT, and bucket them by time against SPR creates, stage loads and streaming.
  - Step 2 depends on step 1: CP, SPF or DL worker-join patches (built, default off), or PJH (item 1).
  - Also build the spike-triggered SP snapshot that `whats-left.md` s.5 lists as missing (a ring of the last 50 ms of samples, dumped when a frame is more than 2x the median). That is about 4 to 6 hours of ASI work.
- **Expected gain:** unknown. The 1% low is 44 to 53 fps against a 96 to 99 fps average, so the headroom is large. M: the CPU bundle already raised the 1% low by 20%.
- **Risk:** none for the analysis. Medium for the worker-join patches.
- **Smallest test:** no game run for step 1. For step 2, one 60 s combat window with SPR and CpuStats on, counting frames > 25 ms with CP/SPF off and on.
- **Devices:** PC first. PC2 and Ally later, using their existing CSVs.

## 4. PC2 and mid-range CPUs: dispatcher critical path (barrier driver wait, SRW lock, ECL ring)
- **Source:** `cpu-bound-midrange.md` s.0 items 1, 3 and 4 (including its correction note), `pc-submit-overhead.md`, `pc-cpu-next.md`.
- **Idea:**
  - On PC2 the main thread waits for the replay thread (418F40 is 23 to 24% of [App]Game, about 6 ms/frame).
  - The dispatcher is 78% busy and about 19% of its time is blocked, in three places:
    - `SynUavBarrier 1139D4`, 7.5%, which blocks in the driver's ResourceBarrier.
    - `7F6904`, 6.1%, under the shared SRW lock of SynSharedLockedUpdate `7F64DC`.
    - `EclRingPush 86F760`, 5.5%, which is submission backpressure.
  - The next target is whichever of these dominates, found with a deep-stack profile. Possible fixes are fewer or narrower barriers, avoiding the exclusive SRW holder, and a larger or earlier ECL ring flush. All are same-picture changes.
- **Expected gain:**
  - M: PCM, the memo in front of the PSO B-tree, gave only +0.5%. TG gave +2.6 to +3.3% (see item 14).
  - E: the blocked share is about 19% of 20 ms of dispatcher work per frame, but it is only reachable if it is on the critical path. The prior note says "the only route to about 50 fps".
- **Risk:** medium to high. It touches submission ordering and locks. Barrier removal on PC already measured -7%, so the pattern is "fewer calls cost CPU in our hook".
- **Smallest test:**
  - One CPU profile (SP with a `Dispatcher` focus) at the PC2 spawn, then ABBA on the single worst site.
  - Start with the SRW holder identification, which is measurement only.
- **Devices:** PC2 first (CPU-bound at 38 fps). Then PC (dispatcher 0.5 cores) and Ally.

## 5. Cut the 37 s first load of the boss rematch (phase split, then cheapest cut)
- **Source:** `.handoff/open.md` (user #4114 and #4115), `perf-opportunities-pc-20261015.md` #4, `rematch-fast-restart-2026-10-13.md`, `boss-only-load-pc2-3271.md`. The `ff2ccfd` commit already added Preload on menu draw and the in-place `Nioh3Training_FightNow` swap, but neither has a measured first-load result.
- **Idea:**
  - Split the 37 s into menu to request, native mission load, and request to control.
  - Then apply the cheapest cut: warm the arena resources from the menu, and run the PJH prewarm ahead of the load.
  - No readiness checks are skipped. A native duel variant (19 single-enemy missions) is the lowest-risk deeper route.
- **Expected gain:** >= 5 s median is the pass bar. E: 3 to 10 s. M: retries already take 3 to 6 s.
- **Risk:** medium. Skipping flags is unsafe, so only warm caches and prefetch.
- **Smallest test:** `tools/rematch/fightnow_test.ps1`, cold first load against a first load after a menu-time Preload, 3 runs each.
- **Devices:** PC, PC2 and Ally. The user wants it tested on all 3.

## 6. Logic-thread critical path: AABB on PC, then BP and the JT probe
- **Source:** `whats-left.md` s.1 and s.2 rows 4 to 7, `pc-cpu-next.md`, `remaining-work.md` 2.9.
- **Idea:**
  - AABB (the ObbToAabb SIMD patch, 0.6 to 0.8 ms of Scheduler0) is built and bit-exact offline. It was never A/B'd on PC.
  - BP is a SIMD cave for BoundsFromPartObbs 71C60, Vec4CopyW0 and SinCosApprox. It is not built.
  - JT is a probe on JobInvokeTimed 1113B4 / JobNodeExecute 111184 that finds which job finishes last before the 1.44 ms Scheduler0 join wait. The workers are only about 24% busy, so the wait is a tail. JT then enables JG (a reorder or a smaller split grain).
  - UI skip is a memo for UiNodeUpdateWorldMatrix 66D00.
- **Expected gain:**
  - E: AABB about 0.3 to 0.6 ms. BP 0.25 to 0.35 ms. JG 0.2 to 0.6 ms, only if JT shows imbalance. UI skip 0.06 to 0.1 ms.
  - [App]Game waits 0.6 to 1.05 ms/frame on the logic packet, so this is worth about 3 to 8% only in the CPU-bound abba2 scene.
- **Risk:** low for AABB (bit-exact). Medium for JG (behaviour-adjacent). JT is measurement only.
- **Smallest test:** AABB alone, ABBA-ABBA at the abba2 scene with `MaxFps 240` and a per-thread CPU readout. Keep if the Scheduler0 share drops with fps >= 0 and p99 within 2%.
- **Devices:** PC (the logic chain is the critical path there), then PC2.

## 7. Ally CPU bundle and wake gating (PP, MA, J1, WG, LK), including watts
- **Source:** `whats-left.md` s.0 facts 1, 2 and 5 and rows 3 and 13, `cpu-next-candidates-2026-10-13.md` (J1 Ally OFF/ON/OFF, bridge messages 3075 and 3085, never ACKed), `ibaraki-offon-result-2026-10-14.md`.
- **Idea:**
  - On the Ally, main-thread game code is 62% of the frame, which caps it at about 57 fps once the GPU gets under about 18 ms.
  - PP, MA and J1 are not in the Ally profile. Their measured shares are PP 1.27% and MA 3.5% of Ally main (Ally USER, `whats-left.md`). Wakes run at 23.5k/s.
- **Expected gain:**
  - E: about 1.3 ms of Ally main per frame, a 0.3 to 0.6 W CPU drop, and a higher GPU clock.
  - M: the Ibaraki bundle gave -7 percentage points of CPU on the Ally but -3% fps, which was beyond noise. So the ON arm needs a no-harm check, and the harmful switch needs to be found.
- **Risk:** medium. The Ally -3% fps is unexplained.
- **Smallest test:**
  - ABBA on the Ally at USER with GF on and a fixed spot. Use MangoHud CPU power, GPU power, GPU clock and fps, plus the SP main share.
  - Do bundle minus one switch at a time to find the one that costs fps.
  - Keep if CPU W drops with fps not lower.
- **Devices:** Ally. PC2 later.

## 8. Ally barrier and gap session (R2a, R1, GQ gap census)
- **Source:** `whats-left.md` s.0 facts 3 and 4, rows 1 and 2, s.6 item 1, `vkd3d-audit.md` B4 to B6.
- **Idea:**
  - R2a, R2b and R1 (the vkd3d barrier fixes) have never run on the Ally. All 20 Ally logs print `TraceRecords=0 R2a=0 R2b=0 R1=0`.
  - The GFX queue is uncovered for 1.4 to 2.3 ms per frame (mean 1.67 ms, 5.7%).
  - The plan is a GQ gap census in `gpuprof.cpp` (3 to 4 hours of ASI work), then one launch with `Barriers=2 TraceRecords=200000 GPUProf=2 GPUProf2_MinRun=1`, then an ABBA.
- **Expected gain:** E: 0.2 to 0.8 ms (0.25 to 1 fps). It only moves fps because the Ally is GPU-bound. M: Barriers=0 is already the PC default (-7% otherwise), so this applies to the Ally only.
- **Risk:** low for the census. Medium for R2a and R1 (barrier rewrites). Same picture. RADV already ORs consecutive barriers, so the gain may be about 0.
- **Smallest test:** one Ally session. Keep R2 or R1 only at >= +0.1 fps in both ABBA pairs, with `shotdiff` passing.
- **Devices:** Ally only.

## 9. PB2 meshlet VS: descriptor-uniform fast path (GPU, G-buffer)
- **Source:** `material-pb2-next-opportunities.md` (candidate 1), `pb2-pixel-uv-hoist.md`, `remaining-work.md` s.3 and 3.4. The dedicated tool for the Ally rebuild is still missing.
- **Idea:** both original PB2 meshlet VS bodies use waterfall loops (readfirstlane, lane-equality, descriptor load, repeat branch) for divergent root descriptor handles. A bounded region would test full-index equality across the active lanes and use a uniform first-lane index in the all-equal case. The mixed branch stays as the original. Per-lane offsets, vertex indices and float ops are unchanged.
- **Expected gain:**
  - E: 0.3 to 1.0 ms on the Ally for G-buffer materials (7.5 ms at USER, mostly meshlet-VS-heavy).
  - The UV-hoist companion saves 12 of 18 static ops in a pixel shader and has no gain estimate.
  - Prior RGA predictions have been wrong for vkd3d, RADV and ACO (all replacements ran 0.4 to 0.6 fps slower on the Ally). So this is a measured-only claim.
- **Risk:** medium. The uniform fraction, extra branch cost and RADV codegen are unknown. The note says it is a research proposal, not a patch. The proof contract needs 1 record changed and the signature preserved.
- **Smallest test:** swap the single VS record, run it through `ShaderSwap`, take a GP2 per-PSO row before and after, then an ABBA with the profiler off. Reject unchanged codegen.
- **Devices:** Ally first, because it is GPU-bound. PC2 second (RDNA2). The PC is CPU-bound, so it is not a useful device for this.

## 10. PC2: the compute-constant CBVs and the VB/IB dynamic streams (no ReBAR)
- **Source:** `rdna2-meshlet-slowdown.md` s.0 item 3, `root-cause-vram-streaming.md` s.0 items 2, 3 and 5 and s.7.
- **Idea:**
  - CbvMirror only ever redirected the per-draw graphics site 118F89. The compute dispatch constants come from `495B2E <- 49545C <- 58BEE3` (10.8 M CBVs per run) and `117F20 <- 119CB4 <- 119BDC`, and these stayed in system memory in every arm. The meshlet front-end kernels (MIC, MshLod#1, MPC) were never tested with their constants in VRAM.
  - Also unbuilt: a VB/IB mirror for the dynamic streams (particles and UI, 23% of VB binds in system memory).
  - The design must be a compute-shader copy or a bytes-actually-read copy. `CopyBufferRegion` over PCIe on RDNA2 was +10.7 ms in measurement, so a copy-queue design is excluded. The shipped CBU and CBN/CBT work (+15.9% on the PC2 slow link) may already cover part of this, so check before building.
- **Expected gain:** M: the per-draw CBV rows already improved by about 4 ms in the arm C pass rows (graphics only). E: the compute rows (MIC, MshLod#1 and MPC at about 3 ms) have an unproven explanation. VB/IB is worth 0.2 to 0.5 ms.
- **Risk:** high. Copy cost on RDNA2 has already killed two designs.
- **Smallest test:** a probe first. Log callers of the system-memory UAVs and the two compute CBV sites with the VS probe, then match them to the MIC and MshLod#1 rows. Only build if that shows a stable per-frame byte count under about 1 MB.
- **Devices:** PC2 only. The Ally (RDNA3) matched run would show whether MIC and MshLod#1 blow up per dispatch there too, which would mean the cause is the architecture and not PCIe.

## 11. Meshlet readback pair preparation (one submit and wait for the 2 same-job readbacks)
- **Source:** `frame-io-next-20261011.md`; the paired-close idea in the same note was rejected after checking the table at 3BE29D0, and the audit tooling is `tools/bench/audit_frame_io.py`.
- **Idea:** the chain 5B3AC to 4D874 to 62E680 to 62E9B4 to 62EA08 to 1A807F8 to 5C0D4 pays a separate synchronous copy round trip for each of the 2 mapped readbacks (set+8 and set+10). Record both copies, then submit and wait once. Feedback age does not change.
- **Expected gain:** ? (no estimate given in the note). A plausible bound is one GPU round trip per frame on the meshlet worker, which matters most where the GPU is behind (PC2 and Ally).
- **Risk:** medium. Partial-map cleanup is meaningful (the first map must succeed before the second is attempted). Allocator-ring ordering is involved.
- **Smallest test:** offline harness first (the audit script exists). Then default-off ASI, counting round trips and the meshlet-worker wait share, then ABBA.
- **Devices:** PC2 (slowest PCIe), then Ally and PC.

## 12. Load-time memory and respawn paths (LM sweeper, AutoContinueLoad on respawn)
- **Source:** `remaining-work.md` 4.3 and 4.4, `load-memory.md`, `memory-leaks.md` s.5.
- **Idea:**
  - LM is the load-memory sweeper. It is built, bit-exact in the model check, default off, and never run in game.
  - AutoContinueLoad is verified only on title to Continue. Death-respawn and fast-travel are unverified.
  - Also unrun: the 2-hour long-session leak check (`remaining-work.md` 6.2, via `tools/tour/run_tour.ps1 -Passes 2`).
- **Expected gain:** E: 3 to 6 s when a swap burst occurs (LM). The AutoContinueLoad check is a correctness check.
- **Risk:** low to medium. LM zeroes memory ranges, so a leak or crash would be the failure signal.
- **Smallest test:** one fast-travel and one death load with LM on and off, watching `MemStats` and load seconds (`LoadStats=3`). The AutoContinueLoad check takes 15 minutes.
- **Devices:** PC and Ally (the Ally has the swap issue). PC2 is relevant at 16 GB or less.

## 13. End-of-load GPU burst, and the load-start CPU stall
- **Source:** `perf-opportunities-pc-20261015.md` #5, `stutter-pso.md` s.1.
- **Idea:** 56 of the 94 frames over 100 ms in the mod run were the GPU-bound end-of-load burst (3 to 6 s of 100 to 130 ms at 100% GPU). The mod had about twice stock's GPU work per presented frame there, with an unknown cause. 24 more were a CPU stall at load start. Run GPUProf=2 across the load end, mod against stock, to name the pass.
- **Expected gain:** ? It shortens loads and improves the 0.1% low only. It does not improve gameplay fps.
- **Risk:** low for profiling. Medium for any pass change.
- **Smallest test:** two loads with GPUProf=2 (stock ini and mod ini), comparing per-pass times in the last 10 s of the load.
- **Devices:** PC first (where the data came from). PC2 and Ally later.

## 14. TG logic tick governor (measured gain, but it is opt-in and changes behaviour)
- **Source:** `cpu-bound-midrange.md` s.0 item 1, `README.md` TG section, `whats-left.md` s.0 fact 5.
- **Idea:** logic ticks follow the render rate. Ally logic over-ticks at 48.3/s against 34.8 rendered (1.39x), and PC2 ran 80 to 120 ticks/s against 32 to 38 rendered.
- **Expected gain:** M: +2.6% and +3.1% on PC2 (SE 0.1), then +3.3% (SE 0.03) in a third run. These are three confirmations, all on PC2 only.
- **Risk:** medium. It changes simulation timing, which is a gameplay behaviour change and not a graphics change. It is already opt-in and has never been run on PC or Ally with both arms. Boss fights and online play are the concern. Needs a user decision.
- **Smallest test:** the existing PC2 ABBA, repeated on PC and Ally, plus a check that game speed and boss timing are unchanged (a frame-count timer on a scripted attack).
- **Devices:** PC2 (done), then Ally and PC.

## 15. Small same-picture wins that are built but only partly measured
- **Source:** `backlog-small-wins.md`, `optimization-retention-and-ally-playtest.md` (the user's 2026-10-11 rule: keep small optimizations if correctness checks pass), `compute-hash8-specialization.md`, `whats-left.md` rows 10 and 11.
- **Items:**
  - lodu: 0.087 to 0.084 ms in a small GP2 sample only, and no profiler-off fps pair was run.
  - fogpar: FPS-neutral in the sampled result.
  - KL_Div: PC2 +1.2 to 1.3%, and the Ally was inconclusive.
  - VCLOUD cheaper shader: Ally +1.1%.
  - compute-hash8 (3 callers, CRC replaced by a single CRC32 qword): offline-proven only.
  - GI merge (GIMerge): about 0.03 ms at USER.
  - The probes D2 (StageSlotTableUpdate rewrites) and MC (memcpy size histogram, PC 0.47 ms, Ally 3.9% main).
- **Expected gain:** <= 1 fps each, so the user rule says they are not prioritised. Together they could reach 1 to 2%.
- **Risk:** low. Each is bit-exact or picture-checked offline.
- **Smallest test:** one profiler-off ABBA per item when a device would otherwise sit idle. KL_Div on the Ally needs a stable spot and more pairs.
- **Devices:** Ally for the GPU items, PC2 for KL_Div, PC for hash8, D2 and MC.

---

## Notes on what is not listed
- **Looks-changing options** (ExtraPerf presets, FSR3 R1 fp16, ESM 2048 to 1024, XFOG and similar) fail the no-quality-reduction rule.
- **OS-side options** (ReBAR/SAM, TDP, page file) fail the no-OS-tuning rule.
- **Closed no-gain items:** Barriers on PC, PL, B, GL1, LC (made the Ally load worse), PF (neutral), M1 and full M2 (correctness gaps), VBC/S1 (not exact), RR1 (not provable), MSV (corrupts the picture), and the FO frame overlap (no-go).
- **Compute PSO serving cache:** stopped at NO-GO until an owner identity is found (`compute-cache-bounded-stop-next-gpu.md`).

## What is missing from the evidence
- No PC data exists for traversal, new-effect combat or post-load hitches (`stutter.md`, `remaining-work.md` 2.7). Item 3 fills part of this.
- The PC2 and Ally controlled CPU pairs are the main gap for any both-arm claim. The PC2 recorder fix (`open.md`) is a prerequisite.
