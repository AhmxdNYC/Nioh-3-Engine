# PC2 optimization scouting (task #4884, 2026-10-15)

Device: pc2 = Ryzen 5 5600X, RX 6650 XT, 1080p, ReBAR on, GPU on the B550 **chipset** slot (PnP instance path still ends `...020A`, unchanged since the 2026-10-10 finding; `cbf_bench` 0.8 GB/s). Read-only: no game launch, no source or save change. Every number below is quoted from existing notes/handoffs (tags M measured, S static, E estimate); nothing was re-measured here. Rules honoured: no quality reduction, no OS/driver tuning.

Baseline to beat: default 1.7.61 = 45.1 fps, CPU-bound (CPUBusy ~22 ms, GPUTime ~19 ms; stock 30.1 fps) [PC2-HANDOFF #1928]. The replay thread (SynergisticDispatcherTask) is ~78 % busy at ~17-20 ms/frame; main thread then spins ~6 ms/frame in WaitDispatcherDrainSpin 418F40 (cpu-bound-midrange.md s0, M). Main-thread-only savings are absorbed by that drain (W1B +0.1 %, RTP +0.1 %), so only work removed from the **dispatcher** moves fps.

## Ranking (expected frame-time gain x risk)

### 1. Cheaper per-draw descriptor work on the replay thread (CopyDescriptors / CBV create / barriers) - gain E 1.5-3 ms/f (+6-12 % fps), risk med
- Evidence (M, RPF1/RPF2 replay-thread profile on pc2): wall 26.3 ms/f = handlers 20.6 + idle 4.9. Draw handler 6A = 11.2 ms/f (54 %). Top API calls: CopyDescriptorsSimple 2.26 ms/f (19.7k x 115 ns), CreateConstantBufferView 1.66 ms/f (4.1k x 409 ns), ResourceBarrier 1.22 ms/f. CBU1 (CBN+CBT, already shipping) took handlers 20.5 -> 17.1 and gave +15.9 %, proving this path scales 1:1 with fps.
- Proposed change: extend CBT-style memo to descriptor copies: coalesce consecutive CopyDescriptorsSimple into one CopyDescriptors with range arrays; skip CBV creation when (resource, offset, size) equals the slot's previous view (same dedupe idea as CBN, ~16 % of tables are already reuse hits).
- Smallest test: new `[Patches]` switch default 0; pc2 same Continue spot, 60 s plain window ABBA x3 with the existing cbu1/def1761 harness; metric = replay handler ms/f + fps (+ image diff for corruption). Dispatcher-heavy, so pc2 is the right device.

### 2. Stage per-draw constant buffers in system memory but fix the GPU side (CBU2 step 2, "UPLOAD heaps") - gain E 3-6 ms CPU/f if GPU cost is hidden, risk med-high
- Evidence (M, RPF2 #1488): forcing UPLOAD heaps to system memory cut 6A own CPU 4.3 -> 1.07 us/draw and CPUBusy 26.4 -> 13.0 ms, but GPUTime 18.8 -> 26.8 ms, net fps -1.7 % (37.5 -> 36.9). The BAR-VRAM write path is the CPU hotspot; sysmem is the GPU hotspot.
- Proposed change: write cbuffers sequentially into a write-combined staging ring, then one bulk copy per list into the BAR block (CBF staging exists in cbu2). Needs the copy to be GPU-side cheap, which depends on the slot: on the chipset slot the copy measured 0.8 GB/s.
- Blocker/prerequisite: the card is still behind the chipset switch (pc2 hardware, not a tuning change). Step 2 was held on 2026-10-11 until the user moves the card to the CPU x16 slot. Smallest test: first just re-run `cbf_bench-cbu2.exe` after the move (copy GB/s), then CBMODE 6 vs stock ABBA. If the slot is not moved, rank this below #3.

### 3. Logic ticks follow render rate (TG) - gain M +2.6..+3.3 % fps (SE 0.03-0.1), risk low-med (behaviour change)
- Evidence (M, three separate ABBA series, cpu-bound-midrange.md): 80 -> ~50 logic ticks/s frees cache/memory bandwidth for the render threads. Already built and opt-in; only needs a decision, because it changes simulation stepping (not graphics quality).
- Smallest test: already done; a gameplay-gate check (boss fight Yoritomo R7B35 with the 3639 gate) that hit timing/animation is unchanged, OFF vs ON, one pair.

### 4. First-use PSO creation hitches (boss kill / first effect) - gain: hitch tails, not average fps; risk low
- Evidence (M): user play session PSO creates 98-167 ms at boss kill, worst create 153 ms, 25 creates >= 1 ms per session (whats-left.md); pc2 stock boss-kill cluster 00:31:10-31:48; stutter-pso.md: 56 of 94 >100 ms frames are the GPU-bound end-of-level-load burst, 24 more are CPU stalls at load start.
- Proposed change: existing PJ journal + prewarm (PJH hand-over) and the three small diagnostics from task #3894 (overlay ring 50 ms wait, lazy replacement-shader file read on the PSO caller, per-line log fflush). Do the 3894 counters first: they are cheap and may show a mod-induced stall.
- Smallest test: Yoritomo fight, 60 s combat, A = current, B = PJ prewarm of that fight's journal; compare count of frames >33/50/100 ms and p99 (not mean fps).

### 5. Renderer-drain wait candidate (RD) and meshlet/job candidates (M2S, J1) - gain: CPU -2.7 pp measured but 0 fps, risk med
- Evidence: task 3578 pair showed process CPU 67.3 -> 64.6 % but fps +0.21 %, **and that run is marked INVALID** (gameplay state unverified). Everything after the drain is dispatcher-bound, so these free cores rather than frame time on pc2.
- Smallest test: only run again on a gameplay-gated window (3639 gate), RD alone 0/1/0; do not combine with M2S/J1.

## Not recommended / dropped
- Main-thread micro-fixes from cpu100-opportunities.md (all < 0.001 % self): fps-neutral on pc2 because the drain absorbs them.
- PCM memo +0.5 %, RTP +0.1 %, W1B +0.1 %: measured, below the 5 % bar.
- Anything that lowers resolution, effects or quality, and driver/OS tuning: excluded by the brief.

## Top pick
Candidate 1 (replay-thread descriptor copy / CBV dedupe) is the best expected return without hardware changes; candidate 2 has the larger ceiling but depends on moving the GPU off the chipset slot.
