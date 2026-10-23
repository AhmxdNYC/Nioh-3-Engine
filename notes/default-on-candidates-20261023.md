# Default-ON candidates: optimization keys (2026-10-23, read-only analysis)

Human rule: a key that is better on paper, does not change the picture and is not known to harm (fps, 1% low, stability,
other devices) goes ON by default, even without a measured fps win. Groups: **A** = optimized, no visual change (default-on);
**B** = optimized, changes visuals (never default; "full mod" bench arm only); **off** = diagnostic / risky / platform-only /
needs prerequisite / measured harmful.

Sources: mod/Nioh3EngineFix/Nioh3EngineFix.ini comments, notes/settings-audit-plan.md, notes/same-look-package.md,
mod/Nioh3EngineFix/README.md, captures/audit-final-20261023/eval/decisions.json (stage 1, 1 OFF + 1 ON per key),
origin/ally/audit-results:captures/audit-screen/results-ally-{stage2,mce112}.csv (2+2 runs), origin/pc/newopt-20261023 (MCE).
The pc stage-2 EP runs are all invalid (no PresentMon CSV / lost foreground). Evidence column = stage-1 single-run delta
"fps% / 1%low%". Stage-1 avg-fps baseline spread: pc 7.0 %, pc2 30.2 %, ally 24.8 % (1% low spread pc 80 %). So these
numbers are used only to VETO (a loss beyond the device's spread, or a repeated loss); none is claimed as a win.

## Per-key table

| section.key | group | reason (one line) | prerequisites | per-device veto evidence |
|---|---|---|---|---|
| Patches.CBN / CBT | A (already 1) | skip byte-identical CBV re-uploads; same bytes reach GPU | off when CbvMirror != 0 | pc2 +15.9 % (INI), Ally no-harm; **missing in installed INI -> runs OFF** |
| Patches.W6 (=1) | A | present wait sleeps (WaitOnAddress) instead of Sleep(0) spin; frees a core | none (already auto-1 on Proton) | pc2 +6.7/+1.6; Windows ~0 (INI); no veto |
| Patches.GF (=1) | A | fence wait sleeps on SetEventOnCompletion, same wake moment | none (auto-1 on Proton) | pc2 -3.2/+1.4; Windows +0.38 % (INI); no veto |
| Patches.B | A | galloping search, same result as binary search | v2.02 | pc -2.4/+2.1 (in spread); README "no gain" (not harm) |
| Patches.PP | A | page-pool usage sweep every 4th frame (stats <= 3 frames stale; no visual diff reported) | PP_Period 4 | pc README kept +2.9 %; ally s2 +8.8 % (noise) |
| Patches.FPC | A | per-frame material FindParam cache, [App]Game only, expires each frame | none | ally ADVANCE +25.7, s2 +0.1 % (neutral) |
| Patches.AABB | A | SSE OBB->AABB, bit-identical | none | ally +17.9/+50.2 |
| Patches.M2 | A | locked "atomic loads" -> plain aligned loads (same values) | none | pc2 +1.3/-2.4 |
| Patches.J1 | A | same idiom in job-queue polls | none | pc -0.6/-1.9 |
| Patches.S1 | A | PAUSE in end-of-frame busy spin (power/SMT sibling) | none | ally ADVANCE +24.5, s2 +23 % (noise) |
| Patches.IK | A | pad read at start of logic frame: 4-9 ms fresher input, same image, safe online | toggle only while idle | ally -2.3/+24.7 |
| Patches.ED | A | empty effect-update workers not woken (same output) | none | pc2 -6.4/+13.9 (in spread) |
| Patches.WG | A | wake gate, proven no lost job (harness); installed=1 for weeks | WG_* defaults | pc -2.2/+7.5 |
| Patches.DQ | A | 200 us re-check instead of blind 1 ms spin, same CPU use | none | pc2 -1.8/-11.6 (in spread) |
| Patches.P1E7 | A | prefetch only, same results | none | pc +2.4/+15.1 |
| Patches.LK | A | 18 worker locked loads -> plain (same values); installed=1 | none | ally +14.8/-11.2 |
| Patches.MA | A | 25 main-thread locked loads -> plain | none | ally -3.3/0.0 |
| Patches.BPO | A | per-part rotation cache, bit-identical | SSE4.1 (self-checked) | pc2 -6.3/-7.4 (in 30 % spread; recheck in bench) |
| Patches.W1B | A | glyph pad clear with plain store, same bytes | none | ally +2.5/-0.9 |
| Patches.RPA | A | 16-byte moves, bit-exact | none | ally +4.6/-14.7 (in spread) |
| Patches.CP | A | collider prefetch, same results (Ally zip had it installed) | none | ally +8.2/+12.7 |
| Patches.SPF | A | placeable prefetch, same results | none | pc2 -3.0/-2.5 |
| Patches.MQ (=1) | A | SSE bone matrices, bit-identical | SSE4.1 (self-checked); auto-1 on Proton | pc +1.1/+36.7 |
| Patches.PT1 | A | one pool lock per texture, same state | none | pc -2.3/+3.5 |
| Patches.RWb | A | same spin exit, 1/256 of the futile wakes | none | pc2 -0.9/-15.3 (in spread) |
| Patches.PCM | A | pipeline-lookup memo, same pipelines | none | pc2 +14.4/+23.4 |
| Patches.GL1 | A | prefetch in mission task walks, identical results | none | pc2 +5.1/+10.4 |
| Patches.LW | A | loader Sleep(1) -> event wait with same 1 ms bound | none (LoadStats loses 3 Sleep counters) | pc2 +0.3/+4.2 |
| Patches.EP (=1) | A | reuse archive read events (Windows saving tiny but nonzero) | none (auto-1 on Proton) | pc ADVANCE +9.0/+39.2; pc s2 invalid |
| Patches.MCE | A | Memcpy 8 KB-1.5 MB uses game's own AVX loop; identical bytes (73,580 cases) | merge pc/newopt-20261023 first | ally 2+2: -0.9 % fps, +21 % 1%low = neutral |
| Debug.SkipDebugString | A | log only to file, no OutputDebugString (Proton: exception + wineserver call) | none | pc +4.4/+3.8 |
| Patches.E1 | B | invisible effects step N x dt: effect state differs when it comes into view | E1_N 2 | pc -3.7/+1.3 |
| Patches.ESM1 | B | ESM 2048 -> 1024, fog shafts may soften | none | ally -0.6/-17.7 |
| ExtraPerf.XCLD/XFOG/XGI/XSHD/XSHF/XLOD/XTER/XPRT/XAO/XPOST (=1) | B | engine quality knobs below the game's menu (least aggressive level) | ExtraPerf.Enabled 1, Preset 0 | pc XGI low -37.8, XPRT low -28.1, ally XPOST -27.7 (noise; cannot cost work) |
| Patches.MW | off | experimental meshlet wait, "performance unvalidated", no on-paper saving stated | - | pc2 +16.1 (noise); installed=1 |
| Patches.WorkerCap | off | handheld-only; costs parallel width on desktops | handheld | pc2 +4.9 |
| Patches.AS0 / AS | off | KT disabled on non-NVIDIA; corruption / GPU hang possible | - | ally AS0 -19.2/-12.2; pc2 AS -1.3 |
| Patches.L1OnWine / FGOnWine | off | Wine may ignore high-res timer flag (pacing risk) | Proton | ally -7.5 / -15.6, -37.8 low |
| Patches.FGC | off | crash guard, not a perf key; auto via FrameGenToggle=3 | FG on | pc -1.8/-0.6 |
| Patches.P12 | off | more threads, no on-paper win; no gain on 7600X | - | ally screen +36.8 but s2 -5.3 % (contradictory) |
| Patches.M1 | off | rdtsc-window cache of pool capacity = approximation of meshlet request input | - | pc +2.4/-10.8 |
| Patches.X1 | off | may hide pad hot-plug (skips slots marked empty); unverified | - | ally +2.0/-21.3 |
| Patches.TG | off | changes sim step + tap hold time (behaviour change) | VRR, L1 | ally -11.7/-23.4 |
| Patches.DS | off | only matters with TG / at cap | L1 (L1OnWine on Ally) | ally -19.9/-18.0 |
| Patches.DL | off | same result on paper, but worst 1% low of the pc screen; recheck first | installs CP site group | pc +4.2/**-54.0** low |
| Patches.MPCS | off | needs hash-verified mpc shader file | shader package | pc -5.7/-0.4 |
| Patches.TR | off | dropped on PC twice | Windows | pc -7.1/-10.9 (> 7 % spread) + bridge #704 |
| Patches.RTP | off | starves logic threads; game slows when VRR is suspended online | - | pc +1.7/+18.1 |
| Patches.CBS | off | no gain on pc2, worse with sfence (INI) | - | pc +1.2/+6.2 |
| Patches.CBF | off | TEST ONLY | - | ally +14.3 |
| Patches.VSMA | off | inert on PC/pc2; delays streaming requests of skipped tiles; Ally verdict pending | - | pc2 -11.3/0.0 |
| Patches.LPVR / SKYR / FOGT / CLDR | off | no GPU saving (same-look-package); FOGT/SKYR change the picture | - | pc SKYR +6.8; pc2 FOGT +4.3, CLDR -5.3/-25.3 |
| Patches.PJ / PJH / PL | off | need journal / library from earlier sessions; disk files; PJH dropped on PC | prior sessions | pc PJ +0.3/-9.9; pc2 PJH +9.4, PL +21.7 (boot-only, noise) |
| Patches.LC | off | measured worse (loads) | - | pc2 +13.6 (fight noise) |
| Patches.LoadOnlyCpuFixes | off | superseded if W6/GF=1; loads only | - | ally -20.8/-38.4 |
| Patches.SR | off | auto -1 already on under Proton; Windows refuses | Proton | NOT_APPLICABLE |
| Patches.RC / AQ / TSY / SH / PF | off | load-only, no on-paper win on Windows (OS cache / SSD); SH Wine-only and superseded by SR; PF needs trace | - | pc AQ -4.9; ally PF -33.5; ally SH s2 +10 % (noise) |
| Patches.GIMerge | off | loss beyond spread on pc | GpuOpt hooks | pc **-10.3**/-2.9 (> 7 %) |
| VRR.MenuQuantize | off | workaround only; conflicts with Nioh3Fix | - | pc2 -8.2 |
| Tuning.KL_Div | off | earlier flushes = more submissions; no on-paper win | - | pc +0.8/-5.1 |
| Latency.MaxFrameLatency (=2) | off | +1 frame input latency trade | - | pc -2.0/+48.6 |
| Latency.VrrPresent | off | display/tearing behaviour change | Windows | pc2 -2.9/-7.3 |
| GpuOpt.Barriers / DropUndoPairs / AnyCaller / TileBatch | off | hook overhead on the replay thread (pc2 bottleneck); no Windows gain shown | Barriers hooks | ally +5.0; pc DUP -3.3/-9.7; ally TileBatch -11.5 |
| GpuOpt.R2a | off | Proton-only, measured no-op | Proton, Barriers | ally +12.1 |
| GpuOpt.R1 / R2b | off | not spec-exact; flicker/stale data possible; needs image compare | Proton, Barriers | ally R1 ADVANCE, s2 +10.6 % (candidate after image gate) |
| GpuOpt.CbvMirror (+AllSites) | off | non-ReBAR only; CM != 0 disables CBN/CBT | non-ReBAR GPU | pc -2.2 (ReBAR, inert) |
| GpuOpt.SceneBufMirror | off | experiment, non-ReBAR only | non-ReBAR | pc -8.6/-12.1 |
| GpuOpt.FrameOverlap | off | NO-GO (crash risk) | - | pc2 -4.3/-9.1 |
| Memory.LM | off | thread takes engine heap locks every second; load-RAM fix, not a win on paper | - | pc2 +6.8 |

Not in the repo INI (stale, installed only): Patches.RD = 1, Patches.M2S = 1 - out of scope.

## Repo default-on keys vs the INSTALLED ini

Installed: C:\Program Files (x86)\Steam\steamapps\common\Nioh3\Nioh3EngineFix.ini (187 keys vs 289 in repo, older layout).
Repo keys at 1 / -1 that are MISSING there (code default applies):
- **Patches.CBN = 1, Patches.CBT = 1** - code default 0 -> they really run OFF (the only functional loss).
- General.ReBarNotice = 1, Patches.VRROnline = -1, Patches.EP = -1, Patches.SR = -1, ExtraPerf.Enabled = 1,
  Debug.PresentModeLog = 1, Debug.SkipBootNotice = -1 - code defaults equal the repo values, so no behaviour change.
- GpuOpt.CbvMirrorSites / CbvMirrorComputeCopy / SceneBufMirrorQueue = 1 - unused while their parent keys are 0.
Also: the installed INI is a test config, NOT the shipped defaults (B, PP, J1, WG, TG, LK, CP, DL, PT1, MW, PJ, SPR, CpuStats,
GpuOpt.R1, W6/GF/MQ = 1, AutoContinueLoad = 1, VirtualPad = 2). A "pure-perf" or "stock" bench arm must not reuse it as is.

## Ready to apply

Group A (32 incl. CBN/CBT already 1 in the repo; MCE only after merging pc/newopt-20261023):
```
Patches/CBN=1
Patches/CBT=1
Patches/W6=1
Patches/GF=1
Patches/B=1
Patches/PP=1
Patches/FPC=1
Patches/AABB=1
Patches/M2=1
Patches/J1=1
Patches/S1=1
Patches/IK=1
Patches/ED=1
Patches/WG=1
Patches/DQ=1
Patches/P1E7=1
Patches/LK=1
Patches/MA=1
Patches/BPO=1
Patches/W1B=1
Patches/RPA=1
Patches/CP=1
Patches/SPF=1
Patches/MQ=1
Patches/PT1=1
Patches/RWb=1
Patches/PCM=1
Patches/GL1=1
Patches/LW=1
Patches/EP=1
Patches/MCE=1
Debug/SkipDebugString=1
```
Group B (12, full-mod arm only, on top of group A):
```
Patches/E1=1
Patches/ESM1=1
ExtraPerf/XCLD=1
ExtraPerf/XFOG=1
ExtraPerf/XGI=1
ExtraPerf/XSHD=1
ExtraPerf/XSHF=1
ExtraPerf/XLOD=1
ExtraPerf/XTER=1
ExtraPerf/XPRT=1
ExtraPerf/XAO=1
ExtraPerf/XPOST=1
```
