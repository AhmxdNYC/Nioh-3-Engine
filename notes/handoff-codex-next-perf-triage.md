# Next CPU/GPU performance units: bounded triage

2026-10-11, OpenAI Codex; read-only research against main8fcb81b and existing
allygpu notes/artifacts. Only this new note was written. No game/input,
Ghidra/build/proof/capture/install, bridge/Claude/public or ASI source action.
User recording remains reserved. Stable1.7.61/defaultXPRE0 and FO NO-GO remain.
Target ordinary mid-range PCs and Ally, not only pc2's slow-link pathology.
Measure whole-frame cadence, CPU time and GPU time separately at USER settings;
same-picture candidates come first. Any optional quality tradeoff belongs to a
separate explicit opt-in arm and cannot establish equal-picture improvement.

## Newly pinned GPU cost boundary

The archived USER S1 budget is28.7ms GPU span, G-buffer7.54ms, culling2.88ms
at960x540->1080p FSR3.1, volumetrics STANDARD, FG off. Async rows overlap.
Sources: notes/ally-gpu-budget.md:13,57-59;
captures/ally/gpu_budget_1531/S1_gp2_top100.txt.

The large S1 MPC rows do **not** identify the unbuilt MPC20/33 fast path:

| Record in0x29ee0902.file | Exact container digest prefix | S1 coarse row |
|---|---|---|
| MPC24 | 304bac05 | 1.015ms,9.9 work calls/frame |
| MPC22 | a09d4223 | 0.352ms,12.0 work calls/frame |
| MPC20 | 18b6fee7 | absent from top100 |
| MPC33 | cc40b6f0 | absent from top100 |

Mapped by filtering gpu-shaders/dxil_index.csv for this file in record order;
20/33 also match the retained DXBC header digests in
re/scripts_shadow/out/mpc/MPC_20.dxbc and MPC_33.dxbc. Cached hash-index locations
corroborate the records. This was a small read, not a shader rebuild/proof.
MIC0/e31c389f is0.043ms in this table (line90).
Exact probe hashes: MPC20=18b6fee7ead259130f50a9c5803fcfe7,
MPC33=cc40b6f0522ea9826b04dce64a0bc6b6;
MPC24=304bac05646e17a2c24d873283d6993c,
MPC22=a09d4223be2a00bf1c085a9080e73ac7.

Crucially, **MinRun4 can combine different PSOs and bills the interval to its
longest run**, not just copies. Source gpuprof.cpp:253,496-514 explicitly merges
runs before four work commands. MPC24 is a short argument-finalization kernel
(MPC_24.ll:74-186), not the33716-byte culling kernel at MPC20. Thus1.015/0.352ms
cannot be assigned to MPC20/33, nor treated as isolated savings from MPC24/22.
Absence also does not prove zero native cost. MinRun1 is the missing measurement.

## Concrete untried GPU seam, not a ready gain

MPC20/33 guarded G1M-row scalarization remains unbuilt (gpu-rewrite-round4.md:13),
distinct from the failed vertex-shader MSV. Smallest candidate: MPC20 only,
guard immediately after g=(payload.p0>>6)&262143 in block44, MPC_20.ll:250-271.
If all active lanes have equal g, use its first-lane value for read-only t17
G1M-row offsets0/4/16/24/32/36/44 and the derived bindless root descriptor;
otherwise execute the shipped region. Keep float operations, predicates and
each UAV/atomic event unchanged. t19 is indexed by **instance**, not g:
do not copy lodfast.py's t19 G1M-row assumption into this kernel.

Actual support is bounded: r5 rank_r5.txt:27 attributes75.4us/3dispatches to
MPC20 on RX9070; offline ISA4470instructions/73fetches/32fullwaits/71VGPR.
Those are original-PC cost/ISA evidence, not Ally savings. MPC20/33 contain no
existing wave-op calls; a guard needs correct WaveOps feature metadata/admission.
Uniform-wave fraction, actual RADV codegen/occupancy and USER isolated cost are
unknown. Clone/code-size overhead could lose. Therefore **do the pass-cost probe
first; no new shader implementation is justified as a measured Ally win yet**.

If that probe establishes useful cost, the reversible unit is one separately
staged pack replacing record20 only, all38 others byte-identical, original hash
checked and exact revert. It conflicts with the existing mpc item in the same
file: test alone before any deliberate splice. Proof must cover equal/mixed/
partially active waves, early exits, invalid payloads, every original block,
ordered per-thread UAV reads/writes/atomics and final buffers; guard inversion,
wrong row and dropped-store mutations must fail. Fresh packed-to-interpreter
binding and native moving/streaming/culling views remain required.

Old failed/neutral leads stay closed: MSV stretched triangles; full GI/fog/toon
lost on Ally; WDO28 neutral/partial coverage; fog queue relocation neutral;
cloud extra amortization/occlusion trim changes history; RCAS0 still sharpens.
MIC SAT is already built but unproved, not a fresh candidate. No GHDM-sized
same-picture gain is established among the remaining units.

## CPU correction and smallest remaining research seam

BoundsFromPartObbs71C60 is **already BPO1.7.40**, default off, not a new target.
bounds-partobbs.md documents quaternion memo, ready-word loads and forwarding
copy with historical offline equivalence;0.12-0.25ms is an estimate. Its frame
benefit after AABB depends on remaining logic wait. AABB's prepared control
recipe remains first; an ACK alone may represent an INI preference, not a cave.

The unbuilt compute-PSO twin at58C0D8/58C144 has real historical target evidence:
CPU2 attributed4% of dispatcher samples, modelled1.0ms/frame on **slow-link
pc2**, not a normal mid-range target or promised saving. Existing
handoff-codex-compute-cache.md already proves the safety gap: count/cap/root
invalidation can miss same-root/same-count replacement and shader-pointer reuse;
compute capacity is+30, not graphics+34. Serving hits is NO-GO until complete
mutation/lifetime epochs are proved. Smallest next unit is compare-only logging
under the existing lock, preserving stock boolean/all24 output bytes; cover
insert/remove/trim/clear/reinit/teardown, collisions/padding/misses and multiple
caches. No cached native pointer may serve without that lifetime proof.
The original submission/SRV-table-cache proposal likewise lacks mutation,
list/heap reuse and ring-lifetime proof; existing CBU/CBT/graphicsPCM are already
implemented and must not be counted as new optimization work.

## Ranked next tests and exact gates

1. **GPU fogpar:** finish the already prepared input-bound smoke/full proof
   only after recording clearance. Require original/candidate pack identity,
   fresh DXC-body/input binding, full coverage/holdout block coverage and caught
   mutations per handoff-codex-fogpar-bound-validation.md. Then lead-authorized
   Ally USER test: no other replacement, fresh5473bfb7 execution evidence with
   GP2 MinRun1, followed by profiler-off six launches ABBAAB (three paired units); fixed
   save/camera/settings/FG-off and external cadence metric, drift/p99 plus
   foggy/moving-light visual BASE-noise comparison and full restore receipts.
2. **CPU AABB:** execute the pending actual-control reproduction after clearance;
   require pc2 recovery/hash/release receipt and independent clock calibration,
   naturally CPU-limited view, exact installed1.7.61 cave-OFF boot line and fresh
   exact live MENU transitions. Use the isolated prepared recipe, not the old
   harness that changes display/input/other toggles. No new FPS result exists.
3. **GPU lodu:** select replacement/cull4/0x8d9282e6.file (5f7f3b12), not flat
   mshlod (214ac371); never combine them. Existing packed provenance passes;
   fresh d5a1a069 execution, profiler-off paired whole-frame and MinRun1 pass
   timing, mixed-wave/moving/streaming visual comparisons remain missing. pc2
   first, Ally after a credible KEEP; do not include dropped MSV in the old recipe.
4. **Cost/structure probe:** a cleared Ally USER launch with GP2 MinRun1 and
   exact20/33 hashes plus every MPC stage (separate MAX scope if desired);
   this is the next concrete capture after lead/account/recording release,
   with no launch authorized by this note and no claim of a large gain.
   Add a read-only count/view/buffer witness at531948/5301FC. MPC tail removal stays
   blocked until CPU cnt==uploaded nVSDrawPrm.y and correct mpc file are proved.
   This can justify the new MPC20 kernel above; it is not itself a gain.
5. **CPU follow-ups:** BPO on a proven logic-limited configuration after AABB;
   DQ/P1E7 only with installed/live gates and cap/drift controls. Compute compare-
   only prototype is an independent research unit; serving/A-B waits for epochs
   and a dispatcher-critical view. Validate on the normal-link PC as well as pc2;
   never generalize pc2's BAR-write timings to ordinary hardware. Historical
   estimates do not select the bound.

All device work waits for lead/account/recording clearance and entry backup,
current installed binary/config/shader identity and actual restored/hash/lock
receipts. Keep every optimization isolated; static estimates and async-row
reductions are not FPS gains. This note is a bounded next-unit ranking, not
complete performance/visual/runtime acceptance of the mod.
