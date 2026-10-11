# GPU cost versus looks: bounded evidence audit

2026-10-11, OpenAI Codex. Read-only review of archived device summaries; no
fresh capture, game/Ghidra/compiler/bridge/source/controller/public operation.
These are documented results and candidate states, not a checked current bridge
queue or new performance gains.

## What we know

The Ally USER budget reports28.7ms GPU span/34.9fps, with27.3ms attributed
(95% coverage), at960x540 ->FSR3.1 ->1080p, volumetrics STANDARD, FG off
(`notes/ally-gpu-budget.md:13`). Family times are G-buffer materials7.54ms,
FSR3.59ms and culling2.88ms (`:57-59`). Lighting, shadows, fog, particles and
post add further work. This is distributed work, not one demonstrated waste
source. Async family rows overlap; their sum and isolated wall times are not
frame savings. GP2 MinRun4 also absorbs neighbouring draws into copy buckets
(`:21-23`); stale/menu-weighted labels and tables need the documented correction.

There is evidence for specific causes, with limits: decal shader/PSO analysis
shows a four-MRT G-buffer read/modify/write pass, and archived timings are
0.547/0.562ms Ally versus0.216ms pc2 (`notes/ally-ripple-decal.md:14-38`).
Its ~23MB/frame traffic and ~80GB/s bandwidth model imply a ~0.3ms floor
(`:40-46`), supporting a bandwidth explanation. That is a model supported by
code and timing, not runtime bandwidth-counter proof. RGA occupancy numbers
are an offline PAL proxy, not the Ally's actual RADV/ACO compile.

## Corrected method and discarded leads

`notes/cost-vs-looks.md:101` supersedes the earlier GPUBusy/SSIM-only method:
own GP2 pass cost and whole-frame delta are separate; excessive downstream
savings are not WASTE; regional changed pixels versus BASE noise replace
whole-frame SSIM; flat/black/white shots are invalid. A skipped producer can
leave stale output, so a static screenshot cannot establish equal images.

- Decal's1.92ms skip delta corrupted the G-buffer (ground black/player white,
  73% changed pixels versus34% BASE); it was not a0.55ms pass optimisation
  (`notes/ally-ripple-decal.md:32-38`). Drop the removal lead.
- Clouds already use render/8 and1/16 Bayer updates/reprojection; more temporal
  amortisation is not a discovered opportunity (`notes/ally-gpu-budget.md:167-180`).
  LPVR/SKYR/FOGT/CLDR were neutral, slower, inert or visibly changed the image;
  the notes drop those knobs (`notes/same-look-package.md:12-16`).
- Full GI/fog/toon rewrites lost on Ally; WDO28 was neutral with partial target
  coverage (`notes/ally-replacement-ab.md:30-35`). These do not justify new gains.
- MSV was dropped after stretched-triangle corruption; its assumed VS wave
  composition escaped offline proof (`notes/gpu-rewrite-round3.md:296-304`).
  MIC SAT is built but unproved/unstaged (`notes/gpu-rewrite-round4.md:11-13`).
  These are not ready image-preserving improvements. Motion-blur labels and
  the cost-vs-looks expectation table likewise remain hypotheses.

## Best next units, estimates and missing evidence

1. **lodu device validation:** guarded MshLod#2 uniform fast path is documented
   built/proved/staged;400scenes×64threads×3guards,67/67blocks, mutation checks
   (`notes/gpu-rewrite-round4.md:11,36-47`). Estimatedpc2 saving0.1-0.25ms,
   uniform-wave fraction unknown (`:61-66`). Missing: active new shader hash,
   per-pass and whole-frame paired timing, moving/mixed-wave image checks and
   BASE noise floor on the actual driver. Offline equivalence is not that run.
2. **fogpar device validation:** smaller bit-exact early-exit rewrite is built
   and staged; estimatedAlly frame saving0.05-0.2ms, not its async-row reduction
   (`notes/ally-gpu-budget.md:204,211-221`). Missing:5473bfb7 execution evidence,
   paired profiler-off timing with clocks/power stable, foggy/moving-light visual
   comparison and actual RADV behaviour. The larger dropped fog rewrite is
   different; no device win for fogpar is established by this audit.
3. **MPC count/view proof probe, then tail-dispatch removal:** log exact counts,
   camera/view and buffers at531948/5301FC before changing dispatch work.
   The proposed tail unit estimates0.2-0.5ms on heavy pc2 scenes and requires
   the correct mpc shader plus proven bounds (`notes/gpu-rewrite-round3.md:229-238`).
   Missing: those native count/view witnesses, chain output/streaming equivalence
   and an actual device A/B. This is a concrete proof unit, not a ready gain.

Answer boundary: we know much of where GPU time goes and some mechanisms behind
it. We do not yet have a validated ranking of visual value per millisecond, nor
a large image-preserving win demonstrated by these remaining GPU candidates.
