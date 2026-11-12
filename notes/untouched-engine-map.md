# Untouched-engine map: how much of the Nioh 3 renderer the shipped mod changes (2026-11-12)

Desk study only (no launch, no bridge, no capture). Answers the human's "how much of the rendering engine haven't we touched".
All numbers are reused from existing captures/notes; every one carries its source. E = estimate (basis given).

## 0. Answer

**Default-on shipped changes touch ~4-7 % of the GPU frame's attributed work: PC ~4 %, pc2 ~4 %, Ally ~7 %. So 93-96 % of
the GPU frame runs the game's own code.** The touched parts are two shader swaps: GHDM (Hi-Z depth min/max) and fogpar (VFSCAT fog
scatter). Both give the same picture.
**If you count every shipped knob, including the ones that are off by default** (VRS v3 `VrsCeiling=3`, ExtraPerf X*, GIMerge, AS,
ESM1, VSMA), **76-82 % of the attributed work can be changed by some knob (PC 82 %, pc2 82 %, Ally 76 %). 18-24 % has no knob at
all: FSR3, copies/clears, UI/output, water, unnamed.** Most of those off-by-default knobs change the look (ExtraPerf, VRS). GIMerge
keeps the same picture but lost 10.3 % on the PC (`notes/default-on-candidates-20261023.md` l.76).
- Denominator = the XSYS1 family rows (graphics + async wall time, overlap ignored): PC 5.67 ms, pc2 28.83 ms, Ally 29.34 ms
  (sums of the family tables cited in §1). Excluded: GP2 span time not in any family row (idle gaps, barrier drains, rows below
  the Top-100 cut). That is PC 3.56 ms (42 % of the span; the PC is CPU-bound, so this is mostly idle), pc2 8.45 ms (27 %),
  Ally 3.12 ms (11 %). It cannot be classified, but no shipped GPU change targets it either.
- Arithmetic for "default-on touched": PC GHDM 0.05 E (r5 82+39 us at 1080p render, `notes/gpu-shader-sweep.md` l.69, scaled
  x0.44 to XSYS1's 1280x720) + VFSCAT 0.19 (xsys1_pc_vs_pc2.txt, cs=641524ee row) = 0.24/5.67. pc2 Hi-Z 0.15
  (`notes/render-architecture.md` l.78) + VFSCAT 1.10 = 1.25/28.83. Ally GHDM 0.25 E (r5 x6-9 Ally factor, gpu-shader-sweep l.42,
  x0.25 px at REN 50) + VFSCAT 1.78 = 2.03/29.34.
- Shipping caveat: `release/profiles.json` l.144-157 lists both shaders as `kept`, but the 1.7.6x `release/USER_GUIDE.md` l.95-98
  tells users to copy them in by hand. `.handoff/role-performance.md` l.8 says GHDM has been "on by default since 10-06". If a user
  did not copy the files, their touched share is ~0 %.

## 1. Data used (no recapture)

| device | source | config | GPU span |
|---|---|---|---|
| PC RX 9070 | XSYS1 `captures/xsys/xsys1_pc_vs_pc2_minms001.txt` l.4, l.13-31 | 1440p output, HIGH, REN 65 (1280x720 render, `render-architecture.md` l.236), FSR3, FG off | 8.49 ms + 0.79 async |
| pc2 RX 6650 XT | same file l.2 | 1080p HIGH REN 65; **ReBAR OFF at capture** (`cross-system-compare.md` l.15-17) | 31.60 + 5.76 async |
| Ally Z1E | `captures/xsys/xsys1_pc2_vs_allyS1_unmatched.txt` l.4, l.12-30 (= GP2 S1, `ally-gpu-budget.md` l.13) | USER: LIGHTEST + VOL STANDARD, REN 50, FSR3 | 29.16 + 3.30 async |
| PC detail | RGP r5 `re/scripts_gpu/out/rgp_top300` (summary: 10.716 ms span) via `gpu-shader-sweep.md` §2 | 1080p render HIGH | 10.7 ms |

Caveats: pc2 later turned ReBAR on (GPU 27 -> 19 ms, `cpu-bound-midrange.md` l.3-25), so pc2's meshlet/shadow/copy rows are
inflated by the PCIe anomaly. The human's real PC setup (1440p XeSS) is CPU-bound at ~100-118 fps, so PC GPU savings mostly do not
show up as fps. The Ally rows are unmatched spots (S1 shrine vs the XSYS tour).

## 2. Whole GPU frame by cost (ms; % of that device's GP2 span; async rows overlap graphics)

| # | family / passes | PC ms (%) | pc2 ms (%) | Ally ms (%) | status (shipped) | RE missing to change it (same look) | est. gain | look risk |
|---|---|---|---|---|---|---|---|---|
| 1 | G-buffer geometry: PB2 materials, meshlet VS, PB2LS terrain, TREE2, eyes | 1.75 (20.6) | 6.57 (20.8) | 7.37 (25.3) | **UNTOUCHED** by default; opt-in VRS v3 (off), XLOD/XTER (off) | No same-look lever is left: MSV (meshlet VS) was measured negative (`perf-advice-king-ops` l.13) and the PB2 VS fast path is parked (l.58). The big lever is not RE: turn VRS v3 on by default (#90621) | VRS v3 measured PC +3.7 %, pc2 +14 %, Ally +12 % (`role-performance.md` l.16) | VRS low (look approved #69938); same-look none |
| 2 | Meshlet LOD resolve / MIC / MPC / MCLRC / Hi-Z (GHDM) | 0.58 (6.8) | 6.73 (21.3) | 2.50 (8.6) | **TOUCHED only in GHDM Hi-Z** (~0.05/0.15/0.25 E); MshLod/MIC/MPC UNTOUCHED (MPCS off; mpc/mshlod files dropped, `perf-advice` l.59) | Recorder rewrites (MeshletResolveLODPass node 14 Execute, MeshletPreculling 5301FC, MeshletInstanceCulling 53FD3C); cbuffer source of `nVSDrawPrm.y` not located (`render-architecture.md` l.221-224) | pc2 2-3.5 dense / PC 0.3-0.5 / Ally 1-3 E (`render-architecture.md` l.181) | none if predicates kept |
| 3 | FSR3 upscaler (prepare, luma, ACCUMULATE_SHARPEN e7386840, RCAS) | 0.35 (4.1) | 1.26 (4.0) | 3.18 (10.9) | **UNTOUCHED** (FG/FGC are CPU-side fixes) | The statically linked FFX code is not disassembled; delivery needs a PSO bytecode swap (ShaderSwap exists, off: ini l.937) | Ally R1 fp16 0.2-0.5 + R3 RCAS fusion 0.1-0.2 E (`ally-gpu-budget.md` l.160-161) | R1 near-look; R3 none |
| 4 | Sky VCLOUD (async CS b76874bb + sky PS) | 0.49+0.02 (6.1) | 2.91+0.07 (9.4) | 1.52+0.14 (5.7) | **UNTOUCHED** by default; XCLD, AS opt-in | None needed: already render/8 + 1/16 temporal; its wall time is async overlap (`ally-gpu-budget.md` l.175-180) | <= 0.05-0.1 Ally (same file l.178-180) | - |
| 5 | Deferred lighting / sun-shadow resolve (WDO#14, WDO#28, PB2DS, PB2DUS, PB2MRLR, SDFOCC) | 0.33 (3.9) | 1.21 (3.8) | 2.11 (7.2) | **UNTOUCHED** (wdo14 and wdo28 built bit-exact, staged, never shipped; XSHF opt-in) | Only a GPU A/B is missing (`gpu-shader-sweep.md` l.174-186) | Ally 0.2-0.4 at MAX, ~0.01-0.03 at USER (`ally-gpu-budget.md` l.123) | none |
| 6 | GI / reflection gathers (RefProbe #121/#414, SSR PBRLR, HBAO, GFM) | 0.57 (6.8) | 1.23 (3.9) | 1.88 (6.5) | **UNTOUCHED** by default; GIMerge off (PC -10.3 %), XGI/XAO opt-in | RefProbe has no async-capable context (RP2) (`render-architecture.md` l.163-166). Half-rate GI is closed (flicker) | RP2: PC 0.25-0.6, pc2 0.3-0.5, Ally 1-3 at GI HIGHEST (l.165, l.186) | none (RP2) |
| 7 | Shadow maps (SM skinned casters, VSM tiles/batches, cascades, ESM) | 0.30 (3.6) | 2.01 (6.4) | 1.38 (4.7) | **UNTOUCHED** (ESM1 closed, VSMA off, XSHD opt-in) | VSM batch scheduler 1F8340: tiles past the 8/frame budget are drawn but never marked valid (`engine-rewrite-options.md` l.75) | Ally 0.1-0.4 mean, **3-5.7 ms movement bursts** (1 % lows) | near-identical |
| 8 | Volumetric fog VFSCAT (async) + SSFOG | 0.22+0.03 (2.9) | 1.25+0.10 (4.3) | 1.78+0.22 (6.9) | **TOUCHED: fogpar** (bit-exact; Ally -0.18 ms, `release/profiles.json` l.155) | Remaining: temporal reuse (`engine-rewrite-options.md` l.79) | 0.2-0.5 pc2 E | medium (lag) |
| 9 | Copy / resolve / clear (+ up to 3 absorbed draws each) | 0.23 (2.7) | 1.25 (4.0) | 1.84 (6.3) | **UNTOUCHED** | The real copies are each needed (`ally-gpu-budget.md` l.190-195). The GBufferEdit 4-RT hazard copy is 1.05 ms on the Ally at MAX (l.86) | <= 0.5 (`render-architecture.md` l.185) | none if unused |
| 10 | Effects / particles / decals (G1E, PB2DECAL, ripple, TOONEDGE2) | 0.18 (2.1) | 0.91 (2.9) | 1.77 (6.1) | **UNTOUCHED** (toonedge2 staged not shipped; XPRT opt-in) | Per-effect material graphs; no generic same-look rewrite (`gpu-shader-sweep.md` l.166) | toonedge2 0.15-0.25 Ally E (l.95) | none |
| 11 | Post: bloom, MB, DOF, compose, SSS (exe-embedded) | 0.19 (2.3) | 0.52 (1.6) | 1.54 (5.3) | **UNTOUCHED** (XPOST opt-in) | Needs a PSO bytecode hook (`gpu-shader-sweep.md` l.29-31, l.187-194) | <= 0.2-0.3 (`render-architecture.md` l.189) | none |
| 12 | UI / output (AMBSL, ExeUI, VideoOut gamma copy) | 0.17 (2.0) | 0.67 (2.1) | 0.83 (2.9) | **UNTOUCHED** | AMBSL could be one tiled pass (`cost-vs-looks.md` l.79). The gamma copy can only be skipped in cutscenes | 0.1-0.15 pc2 E | none |
| 13 | Water / ocean (PB2UMAH, CTS, WET, SRIVER + async PFB/IGP) | 0.18 (2.0) | 1.24 (4.0) | 0.30 (1.0) | **UNTOUCHED** | Water scenes only; no clear win (`gpu-shader-sweep.md` l.63) | scene-dependent | - |
| 14 | Unnamed rows (gfx + async) | 0.08 (1.0) | 0.90 (2.8) | 0.98 (3.4) | unknown | Name them with `gp2_digest.py --rebuild` | ? | ? |
| - | Not in a family row (idle, barrier drains, < Top-100) | 3.56 (42) | 8.45 (27) | 3.12 (11) | n/a | PC: frame-start idle = CPU-bound main thread. Ally: 0.55 ms async fog join. Idle-gap fill is BLOCKED (#90154, `role-performance.md` l.35) | - | - |

Touched (default-on) rows: 2 (Hi-Z only) and 8. Rows with an off-by-default shipped knob: 1, 2, 4, 5, 6, 7, 8, 10, 11.
Rows with no knob at all: 3, 9, 12, 13, 14.

## 3. CPU side (short)

| CPU cost | size (source) | status | what is missing |
|---|---|---|---|
| Main thread waits for dispatcher replay (WaitDispatcherDrainSpin 418F40) | ~6 ms/frame on pc2 (`cpu-bound-midrange.md` l.16, l.52) | **UNTOUCHED** (FO frame overlap measured negative, `perf-advice` l.13) | Name the holders of SRW 7F6904 / UAV barrier 1139D4 / ECL ring 86F760 with one deep SP profile (`perf-advice` l.49-51) |
| Draw prep (DrawEmit 1E80B0, meshlet 4AA34/4BBEC, streaming 1E7050) | ~10 ms pc2 (~40 % of [App]Game, l.57); ~5 % at PC Extreme (`role-performance.md` l.19) | partly: CBN/CBT skip identical constant uploads (default on, pc2 +15.6 %, `release/profiles.json` CBN) | multi-core world update / draw-command cut never built (3-4 %) |
| Dispatcher PSO cache B-tree (119820 / 58C144) | 3.1 + 1.0 ms pc2 (l.58) | built (PCM), default **off** (+0.5 %) | - |
| UiQuadBuild 1CD690 (UI vertex read-modify-write) | 2.0 ms pc2 (l.54) | **UNTOUCHED** (W1 only fixes glyph padding) | write-only rebuild of the alpha pass |
| FenceWait Sleep(0) spin / worker wakes / logic over-tick | Ally 57.6 % of [App]Game / 14-16k wakes/s / 2.8 ticks per frame (`cpu-hotspots-ally.md` l.23-25) | **TOUCHED**: GF/W6 auto on Proton, J1/WG on (`optimization-review-85224.txt` l.32); TG opt-in (changes behaviour) | - |
Main PC: the CPU bundle gave 96 -> 99.5 fps and 1 % low 45 -> 53 (`role-performance.md` l.6). The PC is CPU-bound, so GPU rows 1-14 move PC fps
only when the GPU is the limit (FG on, MAX).

## 4. Top 5 untouched targets (ranked by same-look gain on the GPU-bound devices, pc2 and Ally)

1. **Meshlet front-end (row 2 minus GHDM): pc2 1-3.5 ms, Ally 0.7-2 ms, PC 0.2-0.5 ms (E).** RE next: in MeshletPreculling 5301FC,
   follow the buffer-view setter 52F8EC and the compute-context commit vf+C8h to the cbuffer that holds `nVSDrawPrm.y`. That proves
   the 5-dispatch tail skip with the stock shader file. Then decompile the node-14 MeshletResolveLODPass Execute recorder for a
   persistent-thread LOD pass. Capture first: one pc2 GP2 MinRun 1 window with **ReBAR ON** (XSYS1's 6.73 ms was ReBAR off).
   Look risk: none.
2. **FSR3 + exe-embedded post/UI (rows 3, 11, 12): 5.5 ms = 19 % of the Ally frame, no knob today; est. 0.3-0.7 ms Ally (E).**
   RE next: disassemble FsrConfigApply 0x2998b4c and the statically linked FFX ACCUMULATE_SHARPEN (e7386840) in Nioh3.exe to see
   if an fp16 permutation is already in the binary. Extract the exe DXBC by the digests in `gpu-copies-postres.md` §1 and run the
   swap through the existing ShaderSwap path. Look risk: none for RCAS fusion, near-look for fp16 (needs the human's OK).
3. **VSM batch-overdraw cap (row 7): Ally 0.1-0.4 ms mean, 3-5.7 ms bursts while moving; this targets 1 % lows, the biggest
   unexplained cost (`perf-advice` §2).** RE next: decompile the VSM tile batcher 1F8340 (shadows-redundancy S1). Confirm that tiles
   past the 8/frame budget are cleared and drawn but never marked valid. Then capture an Ally movement tour with the GP1
   "Batch Tiles" marker. Look risk: near-identical (only streaming-feedback timing changes).
4. **Engine amortisation knobs (rows 4, 6, 8): LPVUpdateFrequency, SkylightAmbientUpdateFreq, VolumetricFogTemporalFactor,
   CloudReprojectionBlendWeight, all left at "every frame"; 0-0.3 ms per device (E, `engine-rewrite-options.md` l.76).** This is
   the engine-native form of "update bounce light only when needed" (+5-10 % claimed, `role-performance.md` l.17). RE next: decompile
   the readers of RenderConfig +0x1394 / +0x144 / +0x13C0 / +0x7C0 to learn what each value does. The write path already exists
   (ExtraPerf's RCO cave). Look risk: low.
5. **RefProbe GI gathers on async compute (row 6, RP2): pc2 0.3-0.5, PC 0.25-0.6, Ally 1-3 at GI HIGHEST (E).** RE next: study
   AsyncSlotsCreate 32FA450 and how VCLOUD/VFSCAT get slot-aware contexts, then the ReflectionProbePass node-150 recorder, to see if
   #121/#414 can be put on a compute list. Look risk: none. Effort very high. Cheaper runner-up: ship the staged wdo14/wdo28 after
   one Ally GP2 A/B at MAX (row 5, 0.2-0.4 ms at MAX).

Not listed on purpose: Extreme HQ low-res soft passes (closed, #90622) and VRS image approaches (measured dead ends). Turning VRS v3
on by default is the biggest single lever for row 1, but it is a ship decision (#90621), not RE.
