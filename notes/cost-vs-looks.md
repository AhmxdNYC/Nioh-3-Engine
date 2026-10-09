# Cost vs looks audit (2026-10-09)

User question (#534): *"can't you just identify what is costing so much exactly"*. pc2's GP2 table (bridge file #125, RX 6650 XT,
1080p HIGH, FSR 65, 23.2 ms stamped span) has no single culprit: 80 PSO rows = 19.2 ms (top 10 = 8.75 ms), ~4 ms "unattributed"
(decomposed in `notes/gpu-unattributed.md`: mostly async rows counted as serial + the small-bucket tail; ~2.15 ms uncovered DIRECT
time), 1.73 ms copy/resolve/clear (mostly small draws absorbed by `GPUProf2_MinRun = 4`). A cost table alone cannot say what is
*waste*. This audit measures, for each significant effect, **what it costs (ms of real frame GPU time) and how much it changes the
picture**, plus whether a **cheaper replacement algorithm** (lower-cost method, temporal reuse) could give a nearly identical picture.
High cost + low visual impact = the game's real waste. Whole-frame structure (pass fusion, buffers, memory traffic) is in
`notes/render-architecture.md`; this note does not repeat it.

## 1. Method

**Diagnostic: `[Debug] SkipFx`** (ASI, `src/skipfx.cpp`, default 0, never in a release profile; `tests/run_skipfx_harness.py` ALL OK).
- Rides on gpuopt's command-list thunks with its own mask (`gpuopt_hook3`, so GP2 sampling never switches it off). Tracks the bound
  PSO per list (SetPipelineState / Reset / ClearState) and the engine marker stack per recording thread (Begin/EndEvent, as GP).
- A Draw / DrawIndexed / Dispatch / ExecuteIndirect / DispatchMesh is **not recorded** when the bound PSO's shader set
  (psoswap's map: `vs=<hash> ps=<hash>` / `cs=<hash>`) contains a listed hash fragment (>= 6 hex, `ps=` / `vs=` / `cs=` restricts the
  stage), or an enclosing engine marker matches (`DrawSky`, `Cascade*`; needs `[Profiler] GPUProf_Markers = 1`). The gpuopt barrier
  batch is flushed first, so resource states are exactly as recorded; nothing else on the list changes. SkipFx = 1 also turns on
  psoswap's PSO -> shader map (as GPUProf = 2 does).
- Live: the control file `Nioh3EngineFix_skip.ini` (`[SkipFx] Tag / PSO / Marker / ClearMode / Clear`) is re-read every 100 ms; the
  mod writes `<file>.ack` every 500 ms (gen, tag, skipped draws / dispatches / EI / mesh since the apply, matched sets) and logs
  `SkipFx: apply ...` / `SkipFx: done '<tag>' ... matched sets: <descriptions>` per condition. One launch cycles every group.
- **Limit (stale outputs).** A skipped compute pass writes nothing: its UAVs keep the last frame's content. At a static view a
  screen-space producer (GI gather, fog volume, clouds, AO) can then look *unchanged* although the effect is gone. Such groups carry
  `stale_risk` and their "looks" number is a lower bound; the `*_all` groups also skip the consumer draw (FOG_all = VFSCAT + SSFOG
  composite; SKY_all = every pass inside DrawSky) and give the upper bound. `ClearMode = 1` (per group, off by default) clears a
  skipped draw's own render targets to `Clear` when the skipped draw is the first draw on that binding.
- **Cost = PresentMon GPUBusy** delta (union of all queues), not GP2 wall time: an async pass that overlaps graphics work saves less
  than its GP2 row. That is exactly the number that matters for fps (and it resolves gpu-unattributed.md's "async counted serial").

**Harness: `tools/testmatrix/cost_vs_looks.ps1`** (pc2: `pc2_cost_vs_looks.ps1` after `tools/pc2/make_pc2.ps1`).
- One launch via `run_tour.ps1 -Input vpad` (lock, save backup/restore, ini/ASI install/restore, PresentMon, VirtualPad title ->
  Continue). Route `tools/tour/tourlist_cvl.txt`: 4 stage spawns in 4 areas (7356, 52, 12009, 13587), 1 point each, dwell sized to
  the plan (~35 conditions x 6.2 s + margin, ~4 min). Keys: SkipFx = 1, GPUProf_Markers = 1, GPUProf = 0 (no profiler overhead),
  OSD counters off (clean shots), VRR MaxFps 240.
- At each dwell (tour `progress.txt` "B" line) it cycles `BASE, groups 1..k, BASE, groups k+1..n, BASE`: write the control file,
  wait for the ack, settle 2 s, measure 3 s (UTC window), screenshot (primary screen; the game is focused once per point and only
  when the PC was idle >= 60 s). Stops a point early when the tour moves on ("D" line). Groups: `tools/testmatrix/cvl_groups.json`
  (32, risky ones last: compose, meshlet culling, FSR3; `-NoRisky` drops them, `-Only a,b` picks).
- `-Mode split`: one more launch, no SkipFx, `GPUProf = 2, GPUProf2_MinRun = 1, GQ = 1` -> `split_budget.txt` (every copy / clear /
  resolve and every meshlet kernel its own row; = #473 item 2) + `gq_lines.txt`. Superseded by gpu-unattributed.md's Phase U when
  that has run; kept for a stand-alone session. `-Mode both` = split then cvl.
- `-DryRun`: fake game (progress lines + mod-like acks), fake shots + PresentMon -> the whole path; ends `DryRun OK` (passes).

**Analysis: `tools/testmatrix/cost_vs_looks.py report <session>`** (`selftest` passes) -> `cvl_report.md`, `cvl_table.csv`, `diffs/*_x8.png`.
- ms saved = BASE GPUBusy interpolated to the condition's slot (drift) - condition GPUBusy; noise = BASE spread at that point.
- looks vs the nearest BASE shot: mean abs diff, % pixels changed (> 8/255), SSIM (grey, 8x8 box); BASE-vs-BASE of the same point is
  the noise floor (TAA jitter, wind, idle animation) -> **excess %** and **SSIM drop**.
- present = the ack counted skipped calls at that point (else the row is ignored: effect not in that scene).
- Verdicts: **WASTE** (>= 0.15 ms and > 2x noise, excess < 1 %, SSIM drop <= 0.01) -> optional toggle / cheaper rewrite first;
  **REWRITE** (>= 0.15 ms, excess < 5 %); **KEEP-visible**; **KEEP-cheap**; **COST-REF** (geometry families, culling, FSR, compose, UI,
  whole sky: price tags, not removal candidates); **ABSENT**. Ranked list by ms per unit of excess change.

## 2. Results table (template; the run fills `cvl_report.md` in this shape)

| effect | ms saved | % pixels changed (excess) | SSIM (drop) | what it looks like | verdict | replacement idea | est. ms after | visual risk |
|---|---|---|---|---|---|---|---|---|
| ... | GPUBusy delta +- noise | | | + shot / diff image | keep / candidate optional toggle / candidate cheaper rewrite | | | |

## 3. Pre-filled expectations (static analysis; the run confirms or overturns)

GP2 = pc2 #125 rows (MinRun 4: neighbours absorbed; async rows are wall time, not frame cost). "Exp. saved" is my estimate of the
GPUBusy delta. Sources: gpu-rewrite-round3, gpu-resolution-audit, settings-cost, ally-gpu-budget, gpu-volumetrics-rewrite, invisible-gpu.

| effect | GP2 ms | exp. saved | expected look when skipped | expected verdict | replacement idea | est. ms after | visual risk |
|---|---|---|---|---|---|---|---|
| VCLOUD (async CS b76874bb) | 1.60 | 0.6-1.2 (async overlap) | clouds frozen (stale); sky = small share of most views | **REWRITE / WASTE candidate** | temporal amortisation: march 1/4 of the pixels per frame + reprojection (industry standard for clouds); vc7 latency rewrite queued (#509) | 0.3-0.5 | low (cloud-edge ghosting on fast turns) |
| VFSCAT froxel fog (async CS) | 1.04 | 0.4-0.9 | fog volume frozen (stale) | REWRITE candidate | temporal reprojection, half the slices per frame; or VOLUMETRIC STANDARD (-58 % froxels, menu) | 0.5 | low-medium (lag on flashes) |
| FOG_all (VFSCAT + SSFOG) | 1.17 | 0.6-1.0 | no fog / god rays: big change in fog areas | KEEP-visible | as above | 0.55 | low-medium |
| SSFOG composite | 0.13 | 0.13 | fog gone | KEEP-cheap | half-res + bilateral upsample | 0.06 | low |
| SKY_all (DrawSky marker) | 1.71 | 0.8-1.4 | no sky | COST-REF | cache the non-cloud sky dome per time step | - | low |
| RefProbe GI (cs 4b4c3cbe) | 0.58 | 0.58 | GI stale (static view ~same) | KEEP-visible (true loss = mood lighting) | checkerboard / half-rate gather + temporal reuse | 0.3 | medium (GI lag) |
| SM skinned casters (ps c4656a8d) | 0.92 | 0.9 | characters lose shadows | KEEP-visible | far skinned-caster distance cull / cascade-LOD | 0.5 | low-medium |
| Cascades (marker) | ~0.9 | 0.8 | dynamic shadows frozen | REWRITE candidate | update the far cascade every 2nd frame | 0.6 | low |
| WDO sun-shadow resolve/blur | 0.80 | 0.8 | screen shadows stale/wrong | KEEP-visible | wdo28 (built, same picture); half-res mask blur | 0.5 | low |
| Bloom chain | 0.49 | 0.45 | glow gone near fire/lights | REWRITE candidate | render-res chain (POSTEFFECT STANDARD) / dual-Kawase | 0.2 | low |
| AMBSL (54 draws) | 0.24 | 0.24 | local ambient spots missing | REWRITE candidate | one tiled/clustered pass | 0.1 | none (same math) |
| 'motion blur' rows (vs d8fc9faf) | 0.28 | 0.28 | static view: none (if it is MB) | **WASTE candidate at a static view**; label disputed (settings-cost) | MOTION_BLUR LIGHTEST | 0.1 | low (taste) |
| ESM blur | 0.18 | 0.18 | ~invisible while static | WASTE candidate | ESM1 (built, #473) | 0.06 | low |
| TOONEDGE2 | 0.16 | 0.16 | outlines gone | KEEP-cheap/visible | - | - | - |
| HBAO / GFM | 0.12 / 0.26 | 0.35 | contact shadows softer/stale | KEEP-cheap | GTAO 2-slice temporal | 0.15 | low |
| PBRLR SSR | 0.11 | 0.11 | reflections gone on wet surfaces only | KEEP-cheap | SSR STANDARD | 0.05 | low |
| G1E particles | 0.34 | 0.3 | wisps/embers gone | KEEP-visible | half-res particle buffer | 0.17 | medium |
| Ripple/decal | 0.22 | 0.2 | local decals gone | KEEP-cheap | merge draws | 0.1 | low |
| IGP + PFB async, RP_GFMHiz | 0.35 / 0.15 | ? | unknown - this run identifies them | ? | after the run | ? | ? |
| PB2 / STD / meshlet families, MPC / MIC / MshLod, FSR3, compose, UI | 0.2-1.5 each | ~GP2 | holes / frozen output | COST-REF | MSV (built, same picture), MIC SAT / MshLod#2 (round 4) | - | none for the proven rewrites |

**Expected outcome.** The real waste candidates are the *temporal* ones: VCLOUD and VFSCAT recompute low-frequency volumes from
scratch every frame (~2.6 ms GP2 wall on pc2 together), the shadow cascades re-render dynamic casters every frame, and the bloom /
post chain runs at output resolution. Each has a standard cheaper algorithm with near-identical output (amortised / reprojected
updates, render-res post). GI, WDO and fog composite are expected to be visibly important (keep; same-look rewrites only).

## 4. Status
- Built: SkipFx in the ASI (1.7.46 tree), harness + analysis + DryRun/selftest pass; test_patterns ALL 231 SITES OK; check_release PASS.
- Queued on pc2 (bridge [QUEUE], after its current items): `pc2_cost_vs_looks.ps1 -Mode cvl -UserConfig configs\pc2-bench-1080-120-high.xml`,
  then `py -I tools\testmatrix\cost_vs_looks.py report <session>` -> post `cvl_report.md` as [RESULT]. Results go into section 3.
