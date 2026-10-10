# Extra performance settings (changes the look) - ASI 1.7.59, branch extra-perf

User (2026-10-09 #1140 paused, 2026-10-10 #1429/#1433): "just do them", "all 3 devices should perform better". These are
options the game's own menu does not have, trading picture for fps. They are NOT same-look fixes: each one changes the image,
so all of them ship **off** (level 0 = the game's own value) and the player picks a preset or a row.

## 1. What ships

- Mod menu section **"Extra performance (changes the look)"**: a Preset row (Game / Performance / Potato) and 10 level rows.
  Every row applies live (menu, `vpad menu set XCLD 2`, LiveCtl `XCLD = 2`); Save writes the levels to the ini.
- ini `[ExtraPerf]`: `Enabled = 1` installs the hooks (with every level 0 they pass the game's values through, proven offline),
  `Preset = 0`, `XCLD .. XPOST = 0`. `Enabled = 0` installs nothing. A preset in the ini wins over the per-row keys.
- Every option drives a real engine control - never a draw skip:

| key | option | engine control (site) | levels 1 / 2 / 3 (/ 4) |
|---|---|---|---|
| XCLD | cloud ray-march steps | RenderConfig S+0x410 / S+0x40C VolumetricCloudMax/MinRaySampleNum (int, env 256 / 32) | min(game, 128/24, 64/16, 32/8) |
| XFOG | volumetric fog grid | S+0x13DC / S+0x13E0 VolumetricFogResolutionX / Z (HIGH 160/64) | min(game, 64/24, 48/16); 3 = the game's VOLUMETRIC LIGHTEST (0 / 0, ScatterScale 0 = fog off) |
| XGI | GI / reflection gather resolution | GiScaleLookup 187A50 results at the 4 calls in GiRenderScalesApply (GIS1-4) | x0.75 / 0.5 / 0.35, floor 0.1 |
| XSHD | sun shadow distance | S+0xAF4 / S+0xAF8 ShadowEraseFarEnd / Start (env 500 / 250 m) | x0.6 / 0.35 / 0.2 |
| XSHF | shadow filter taps | S+0xB44 ShadowSampleNum (4) | 2 / 1 |
| XLOD | meshlet LOD scale | the 8 category scales ApplyMeshletLodScales 2353C0 writes at obj+0xA108 (EPL1 233A49) | x1.5 / 2 / 3, cap 10 |
| XTER | terrain tessellation | S+0x1970 / S+0x1978 TerrainTessellationMaxDistance / DistanceAdaptRate | x0.7 / 0.4 / 0.2 |
| XPRT | GPU particle share | g_ViewGpuEffectSimFraction / DrawFraction 4566008 / 4566000 (EPF1/2 4A5B76 / 4A5B7E) | x0.75 / 0.5 / 0.25 / 0 |
| XAO | ambient occlusion off | S+0x12F4 / 0x12F8 SSAORayNum / StepNum = 0, S+0x12D8 SSAOContrast = 0 (the game's AO OFF) | off |
| XPOST | camera effects off | S+0x188C motion blur, S+0x1700 DOF flag; 2: + S+0x30 BloomWeight = 0 | blur + DOF off / + bloom off |

Render-config writes ride on samelook's RCO cave (RCO1 39B7CA, right after ApplyGraphicsSettings, before the renderer vfunc
+4B8h consumes S). Per field: when the engine re-filled the field since our last write, the new engine value is the base;
otherwise the remembered base is used (nothing compounds); level 0 puts the remembered engine value back if nobody re-filled
it. Since 1.7.59-final the log shows every override start / change (`EXTRA: S+410 VolumetricCloudMaxRaySampleNum: engine 256
-> written 32`, max 4 lines per override) and its end (`override off (engine value N)`); the 30 s stats line shows the GI scale
the engine computed and what the cave returned, the two GPU particle fraction globals, and meshlet LOD scale[0] engine -> used.

Code: `src/extraperf.{h,cpp,asm}`, `src/samelook.cpp` (RcoApplyC calls `extraperf::ApplyS`), `dllmain.cpp` (groups RCO / GIS /
EPL / EPF, callee and global checks against v2.02), `menu.cpp` (rows, Save, remote), `patches.json` (GIS1-4, EPL1, EPF1-2 +
deps), `tests/run_extraperf_harness.py` (offline: every field per level, no compounding, restore, the caves with registers /
flags kept, presets, dump checks of every site and engine writer) -> ALL PASSED; `test_patterns.py` ALL 252 SITES OK.

## 2. How it was measured (PC, RX 9070, 2026-10-10)

- `tools/testmatrix/fastab.ps1 -Shots` (one launch, live ABBA through LiveCtl, notes/fast-testing.md), tour mode with saves
  blocked, lock `nioh3-pc`, user ASI / ini / BossRematch files verified identical after every run. Config = the user's session
  config (2560x1440, REN_RAT 75, XeSS -> OptiScaler FSR4, FG off, [VRR] MaxFps 240). 3 pairs x 15 s windows + 5 s warm-up per
  item; fps from the mod's frame clock, GPU busy from PresentMon (sampled).
- Stage 7292 (Mirage Fortress spawn, the heaviest GPU-bound PC spawn; GPU busy 9.9 ms of a 10.0 ms frame = GPU-bound) for
  everything; stage 7356 (open sky) for clouds / shadows / presets in a 4th launch.
- New in fastab: `-Shots` (screenshot after the pair-0 A and B windows, outside the measured spans; PC idle + game in front),
  `-Asi` (a branch build), lock identity `$env:NIOH3_LOCK_ID`. `tools/testmatrix/shotpairs.py` makes the side-by-side pairs
  (`<session>\pairs\<item>.jpg`, left = game, right = level) and a look-change number (mean abs diff / share of pixels changed
  > 12 levels) next to the A-vs-A noise floor of the same session (the spot has drifting mist, sparks and TAA, so the floor is
  ~6 / 20 %). `tools/testmatrix/extraperf_table.py` prints the rows below.
- Sessions: `captures\pc\fastab\20261010-1134-xp1`, `-1205-xp2`, `-1247-xp3`, `<xp4sky>` (main repo captures folder).

## 3. Results (PC)

PC = RX 9070, 2560x1440 REN_RAT 75 XeSS->FSR4, GPU-bound ~9.8 ms. Gain = fps B vs level 0 (paired, ABBA, 3 pairs, +- = SE); look = share of
pixels changed in the A/B screenshot pair vs the same-session A-vs-A noise floor (~3 %, mean abs diff ~2.1).
Sessions: xp5sky (stage 7356 open sky, ASI 1.7.59) and xp4sky / xp6sky (7356); xp3 / xp1-2 = spawn 7292 (no gains there, see log).

| option | level | PC fps gain | GPU ms | look change (shots) |
|---|---|---|---|---|
| XCLD clouds | 3 (32 steps) | -0.2 % +- 0.1 (no gain; 1 and 2 also 0) | +0.02 | none (3 % = noise) |
| XFOG fog grid | 1 (64x24) | +1.8 % +- 0.1 | -0.17 | dimmer glow, 15 % px |
| XFOG | 2 (48x16) | +1.5 % +- 0.1 | -0.14 | slightly less glow / orange, 20 % px |
| XFOG | 3 (fog off) | +2.8 % +- 0.2 | -0.26 | glow gone, 23 % px, mean diff 12.8 (big) |
| XGI GI/reflection res | 3 (x0.35) | +3.6 % +- 0.1 | -0.34 | none (2 % = noise) |
| XAO | 1 (off) | +1.8 % +- 0.2 | -0.19 | none visible (2 %) |
| XPOST | 1 (blur+DOF off) | +1.8 % +- 0.1 | -0.17 | none in a still frame (3 %) |
| XPOST | 2 (+ bloom off) | +6.7 % +- 4 (noisy; GPU -0.29 ms = 2.8 %) | -0.29 | not shot |
| XTER terrain | 2 (x0.4) | +0.6 % +- 0.1 | -0.06 | none (3.7 %) |
| XLOD meshlet LOD | 2 (x2) | +3.8 % +- 2.9 (not significant) | -0.03 | none (3.8 %) |
| XSHD / XSHF / XPRT | 3 / 2 / 4 | 0.0 / -0.1 / +0.1 % | 0 | none (3 %) |
| **Performance** (XPRE 1) | 2,0,2,1,0,1,0,0,0,0 | pending xp7sky (the xp6sky run of the earlier table with XFOG 1: +5.7 % +- 0.6) | pending (xp6: -0.49) | pending (xp6 with XFOG 1: 15 % px) |
| **Potato** (XPRE 2) | 3,2,3,3,2,2,0,0,1,1 | +10.5 % +- 0.1 (102.1 -> 112.8) | -0.93 | 20 % px, mean diff 6.2: coarser fog, volumetric glow kept |

Why the presets look like this (2026-10-10): the first preset table (xp4sky) put XFOG at 1 / 3 and measured +5.0 % / +9.5 % but
changed 16 % / 23 % of the pixels (Potato replaced the blue volumetric glow with a flat brown haze). XFOG is the only option whose look
cost is large, so it stays out of Performance and Potato uses the mild 48x16 grid (fog off remains a manual row). Options with no
measured gain here (XCLD, XSHD, XSHF, XPRT, XTER) stay in the presets only where they are free in the picture (XCLD 2/3, XSHD 1/3 on
cloudless or distant content) because they may pay on other scenes / the Ally; XTER and XPRT are left to the player.
A harness bug hid the presets in xp5sky (XPRE=1/2 followed by XCLD=0 ... lines in the same control file reset the levels): fixed in
fastab.ps1, which now writes only XPRE when it is non-zero. xp6sky / xp7sky are the valid preset runs.
pc2 (RX 6650 XT, 1080p, CPU-bound, bridge #1691 / file #217): fps about 0, but GPU time drops: Performance -0.52 ms (-2.7 %), Potato -1.82 ms
(-9.4 %), XGI -3.1 %, XFOG -3.5 %, XAO -1.6 %, XPOST -1.1 %. Look: Performance no visible change; Potato loses AO / contact darkening.
Harness fix from pc2: the live ack could be read mid-replace ({} -> fps 0); Read-Ack / read_ack now retry up to 25 x 20 ms until frames / qpc_us
are present (fastab.ps1, cpu_live_sched.ps1, extraperf_ab.py). Ally results: see the bridge (XPERF1).
Ally (bridge #1723, GPU-bound 98 %, ~41 fps): Potato +9.57 % +- 0.17 (40.7 -> 44.6, -2.15 ms); Performance +3.48 % +- 0.07 (41.1 -> 42.5, -0.82 ms). Singles: XCLD3 +2.5 %, XGI3 +2.1 %, XPOST1 +2.1 %, XAO1 +2.0 %, XFOG2 +0.1 %, XSHD3 +0.1 %, XLOD2 -0.4 % (none). Look: Performance makes distant haze / sky lighter and flatter (it ran with the older XFOG 1; the shipped table has fog 0); Potato has slightly flatter ground lighting and less contact shadow, no artifacts. Ship fastab_report.py together with cost_vs_looks.py (it imports it).
Caveat: PC is GPU-bound at ~100 fps with a cheap sky scene; heavy-fog / heavy-cloud scenes and the Ally (GPU 98 %) should gain more.

## 4. Verdict

Ship as opt-in, default off. Real PC wins: GI 3.6 %, XAO 1.8 %, XPOST 1.8 %, fog 1.5-2.8 % (the only costly one for the look), Potato
+10.5 %. Clouds / shadow distance / filter / particles / LOD / terrain showed nothing on this scene. Honest: Performance is a few
percent for an invisible cost; Potato about 10 % for visibly coarser fog + no AO + hard shadows. pc2 and Ally results: see the XPERF1 queues.

## 5. pc2 spec (ready to send; sent)

Sent as [QUEUE] on bridge channel nioh3 (2026-10-10 15:47, #1659): XPERF1, one launch, `extraperf_ab.py run ... --items XPRE=0/1,XPRE=0/2,XGI=0/3,XAO=0/1,XFOG=0/2,XCLD=0/3,XSHD=0/3,XLOD=0/2,XPOST=0/1`, ASI bridge #211 (sha256 854e8e083c0f...), scripts #212 / #213, shots per item, upload pc2_xperf.zip.

## 6. Ally spec (ready to send; sent)

Same as pc2, sent as #1660 (device ally, Linux screenshot via --shot-cmd), upload ally_xperf.zip.
