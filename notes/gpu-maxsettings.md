# MAX settings on the ROG Ally: what each setting really does, what it costs, and what can be saved

Status 2026-10-07. Static work only: `re\Nioh3.dump.exe` (Steam v2.02), the r5 RGP trace
(`captures/rgp/Nioh3_2026-10-06_19-39-58`, RX 9070, 1440p output, **1920×1080 render**, HIGH settings, 10.6 ms GPU),
the round-8 settings sweep (`captures/sweep/report.csv`), the shader files, and the newest Ally data
(`captures/ally/*v4*`, `nioh3_ally_configxml_current_20261007-0418.xml`). The game was not launched and nothing in the
game folder was changed. Validator: `py -I gpu-shaders\maxsettings_spec.py` (**ALL PASSED**).

## 0. Bottom line (honest)

- **The goal (Ally, MAX settings, native 1080p, real 60 fps, same look) is about 3.5–4.5× away on the GPU, and no
  same-look change closes that.**
  - Model of the Ally GPU frame at MAX native 1080p (§2): **~55–80 ms (13–18 fps)**. The budget is 16.7 ms.
  - Every same-look item found here and in round 2, if all of them land, is worth roughly **5–12 ms** on the Ally.
  - The Ally's main thread was ~96 % busy at ~33 fps (ally report), so the CPU can't do 60 at MAX either.
- **One setting dominates MAX: GI = HIGHEST.** It is the only quality step whose cost explodes.
  - HIGHEST sets all four probe-GI gather scales to **1.0** (HIGH: 0.6 / 0.5 / 0.35 / 0.6). The table is at RVA
    0x3BD4610…0x3BD4660 and is read every frame by `GiScaleLookup` 0x187A50.
  - The full-res gather costs about **2.5 ms on the PC vs 0.71 ms at HIGH (+1.8 ms)**, i.e. **+11–14 ms on the Ally.**
  - This is the community's "GI is by far the most expensive setting", now explained.
- **Every other MAX step is cheap or free** (§1):
  - SHADOW HIGHEST changes one per-object flag. It uses the same 4096/2048 maps as HIGH.
  - AO HIGHEST doubles HBAO directions, but the HBAO draws are 0.4 µs each at the test spot (masked).
  - SSR HIGHEST raises the ray steps from ×1.5 to ×2.
  - MOTION_BLUR HIGHEST takes 10 samples instead of 8.
  - LANDSCAPE ULTRA raises the LOD scale from 0.5 to 0.8 and the far distance from 2000 to 4000.
  - MODEL_QUAL, MESHLET, GRASS and VOLUMETRIC are already at their maximum at "HIGH".
  - POSTEFFECT_RESOLUTION does nothing at native resolution.
- **The biggest same-look GPU lever on the Ally isn't a shader at all: it's watts.**
  - The newest Ally run (REN_RAT 50, STANDARD-ish) shows **CPU 15.6 W vs GPU 18 W, GPU clock 1.0–1.1 GHz of 2.7**.
  - Every watt the CPU stops wasting goes straight into GPU clock. That is CPU-agent territory: see the ally report §4.2.
- **Built this round:**
  - **GI1 patch spec + validator.** The ASI implements it: `gpu-shaders/maxsettings_spec.py` →
    `gpu-shaders/maxsettings_patches.json`.
  - **The Ally installer for the staged shader swaps:** `gpu-shaders/ally_shaders.sh`.
  - **No new replacement shader.** §6 explains why: nothing that MAX scales up can be rewritten and proven offline here.

## 1. Settings → engine → passes (Steam v2.02)

Live slot = `0x45BAF30 + 4*idx` (`notes/settings.md`). `ApplyGraphicsSettings` 0x39A39C runs **every frame**
(0x2338B0 → 0x39B570 → 0x39A39C), so it applies changes to these slots and tables on the next frame.
`S` is the per-frame render-settings block that this function fills.

| Setting (max value) | Engine consumer | What it sets, per level | HIGH → MAX difference | Passes affected (r5 PC µs at HIGH, 1080p render) |
|---|---|---|---|---|
| **GI** (HIGHEST) | `GiRenderScalesApply` 0x1872C4 → `GiScaleLookup` 0x187A50 (4 calls per frame), stores via param hashes 0xD8A8960F / 0xEA3FCBD2 / 0x01FCC200 / 0x0F38BF83 | Render scale of the probe-GI gathers. LIGHTEST…HIGHEST: **Indirect** 0.3/0.4/0.5/**0.6**/0.7/**1.0**, **Reflection** 0.3/0.4/0.5/**0.5**/0.5/**1.0**, **IndirectOcn** 0.2/0.25/0.3/**0.35**/0.4/**1.0**, **ReflectionOcn** 0.3/0.4/0.5/**0.6**/0.7/**1.0**. GI = LIGHTEST also skips some work (0x39B570/0x39B808/0x39B948/0x1195DBF/0x123AC50) | Pixels ×2.8 / ×4 / ×8.2 / ×2.8 | RefProbe#121 (diffuse) 338, RefProbe#414 (reflection) 317, water view 53 → **707 at HIGH, ≈2,511 at HIGHEST** (§3). Grids at HIGH match r5 exactly (144×81, 120×68, 84×48, 144×81) |
| **SHADOW_QUAL** (HIGHEST) | `ApplyShadowQuality` 0x18A87C (table 0x3978210, 64 B rows, index clamp 1..5) + `DisplaySettingsWatcher` 0x1350210 (rebuilds RTs on change) | +0 S+0xB98 (18/9/10/8/8); +0xC S+0xB9C/0xBA0 (512/512/1024/1024/1024); +0x10 → setter 0x4653B4 (1024/1024/2048/2048/2048); +0x14 → 0x8FFD28 (2/2/8/8/8); +0x18 → 0x90DF44 (1/0/0/0/0); +0x1C..+0x30 RT hashes (HIGH = HIGHEST); +0x34 S+0xBA8 (512/1024/2048/2048/2048); +0x3C byte → `g_RenderGlobals+0x7598 = (byte==0)` | **Only** +0x3C (HIGHEST clears 0x7598) and +0x38 f32 3000→7000 (no reader found) | 0x7598 gates per-object render flag 0x20 (0x381A8) and a per-frame update skip (0x1E81AE). Most likely more shadow casters or updates, so more SM-* draws (232 µs of SM draws at HIGH). **Unmeasured.** Map sizes and ESM (2048²) are the same as HIGH |
| **AMBIENT_OCCLUSION** (HIGHEST) | `ApplyAmbientOcclusion` 0x39B20C | S+0x12F4 = 2/3/5/**7**/**14** (HIGHEST: 14 if render height ≥ 721, else 7 + S+0x12E8 = 1); S+0x12F8 = 2; S+0x12D8 = 1.75 | HBAO directions ×2 | SSAO node 147 → **HBAO** (0x3de2e18a, object 0x361e1b96; `ParallelStencilLightGroupMask [1,2]`). In r5 the HBAO draws are **0.4–0.5 µs each** (segments 209–214), i.e. masked (per-pixel `sPrmSwi & nSwitchRef`). **≈0 here.** Could matter where the environment enables SSAO on many pixels |
| **SSR** (HIGHEST) | `ApplySetting23` (= ApplySsrQuality) 0x39B0E8 | S+0x107C (ray steps) × ½ / 0.75 / 1 / **1.5** / **2**; resource 0xEF3D849D +0xB8 = 1 (half size?) for LIGHTEST..STANDARD, 0 for HIGH/HIGHEST | Steps ×1.33 | RLR trace + PBRLR mip chain (PBRLR 75 µs) + merge PB2MRLR (117 µs). Est. +25–40 µs PC |
| **VOLUMETRIC** (HIGH = max) | `ApplyVolumetricQuality` 0x39B080 (+ far branch 0x1995C84) | S+0x13DC / S+0x13E0: HIGH ×1.5 capped 160/64; STANDARD cap 120/48; LIGHT ½ capped 80/32; LIGHTEST 0 + S+0x1398 scaled | – (already max) | VFSCAT#235 froxel scatter, 160×96×48 threads, **293 µs async**; VCLOUD 55 µs |
| **LANDSCAPE_QUALITY** (ULTRA) | ApplyGraphicsSettings 0x39A74A (`LandscapeLodScaleTable` 0x3977A58, `LandscapeLodParamTable` 0x3978390), 0x42B368 (byte table 0x3976CA4), 0x31FAA0 (table 0x3977918 = all 1.0), 0x5AB8A0 (LIGHTEST only) | S+0x1978 LOD scale 0.3/0.35/0.4/**0.5**/**0.8**; S+0x1970 far distance 1000/1250/1500/**2000**/**4000**; ULTRA clears terrain-object +0x4A4 | LOD ×1.6, distance ×2 | PB2LS terrain (155 µs; 15–18k-instruction PS, tessellated) + background meshes; **CPU** too. Unmeasured |
| **MOTION_BLUR** (HIGHEST) | 0x39B4A4 | S+0x1898 samples 2/4/6/**8**/**10** | 10 vs 8 samples | MB post at output resolution. Sweep STANDARD→OFF: −0.35 ms GPU (noisy). Est. HIGH→HIGHEST +0.05–0.1 ms PC @1440p |
| MODEL_QUAL (HIGH = max) | only menu/save/set (0x12A2368); consumer not located | – | – | Sweep HIGH→STANDARD −0.39 ms GPU |
| MESHLET_QUALITY (HIGH = max) | `ApplyMeshletLodScales` 0x2353C0 (column +4/+8/+C of the per-stage LOD table) | – | – | meshlet LOD |
| GRASS_DENSITY (HIGH = max) | `GrassPlacementCull` 0x31EF94 (`min(GRASS,4)` → +0xFC) | – | – | grass instances |
| VISUAL_EFFECT (NORMAL = max) | EffectCreate 0x4CE969 | LIMITED refuses "optional" effects | – | G1E particles (394 µs at the shrine) |
| POSTEFFECT_RESOLUTION (HIGH) | 0x2998B4C / 0x2998CE4 / 0x39AF18 | post targets at output vs render size | – | **No effect at native resolution** (output = render) |
| COLOR_PRECISION (HIGH) | SetupPostFxChain 0x39B32C → S+0x2148 = (HIGH) | wider HDR targets | – | Sweep: 0 on PC. Bandwidth-bound Ally: measure |
| ANISOTROPIC_FILTER (16) | 0x5AA0B7 | sampler anisotropy | – | texture filtering in the G-buffer |
| MODEL_DRAW (MANY), MOTION_QUALITY (dead), WIND | CPU side | – | – | CPU (`notes/settings.md`, `notes/effects-animation.md`) |

## 2. The Ally at MAX native 1080p (model) and today's Ally data

**Today** (`captures/ally/*v4-fgoff-futamata*`):
- Config: REN_RAT 50 (960×540 → FSR 3.1), SHADOW / AO / GI / SSR / VOLUMETRIC STANDARD, LANDSCAPE NORMAL,
  POSTEFFECT STANDARD, COLOR MIDDLE, MODEL_QUAL / MESHLET / GRASS HIGH, MODEL_DRAW MANY.
- MangoHud: **presented 20–42 fps (avg 27.5)**, GPU load 68–80 %, **GPU clock 1.0–1.1 GHz**, **GPU 18 W / CPU 15.6 W**.
- The mod's OSD reports "real" 55–61 fps but "presented" 7–36 fps in the same run.
  - That gap belongs to the frame-timing / OSD agents. Settle it before any GPU A/B, because MangoHud's fps is the
    presented rate.

**Scaling** from the r5 trace: Ally ≈ PC × 6–8 for throughput-bound work (×10–12 at the observed 1.0–1.1 GHz).
Render-resolution passes keep the same pixel count (PC r5 also renders 1920×1080). Output-resolution passes ×0.56.

| Family (PC r5 at HIGH) | PC µs | Ally at MAX native 1080p |
|---|---|---|
| Fixed-size: shadow maps, ESM, GHDM, VFSCAT, culling (MPC/MshLod/MCLRC), particles, PFB | ~1,435 | 8.6–11.5 ms |
| Geometry (vertex/primitive side) | ~950 | 3.8–5.7 ms (+ LANDSCAPE ULTRA / MODEL_DRAW MANY, unmeasured) |
| Render-res pixel work (G-buffer, RefProbe at HIGH, WDO, lights, water, TAA filter…) | ~4,040 | 24–32 ms |
| **GI HIGHEST extra** (gathers at 1.0) | **+1,804** | **+10.8–14.4 ms** |
| Other MAX steps (SSR, MB, SHADOW flag, ULTRA) | +~200–500 | +1.2–4 ms |
| Output-res post at 1080p | ~394 | 2.4–3.2 ms |
| AA / upscaler at 100 % | – | 1–2 ms |
| Barrier tails, tiny segments, idle bubbles | ~2,350 | 3.5–6 ms |
| **Total** | | **~55–80 ms (13–18 fps)** |

How the model fits today's data:
- At today's Ally settings (25 % of the pixels, STANDARD) the same model gives ~22–31 ms (32–45 fps).
- That is consistent with the presented 20–42 fps.
- **Conclusion:** 60 fps needs ≤16.7 ms. MAX at native costs ~4× that, and even at 50 % render scale MAX stays at
  ~30–36 ms.
  - The reason is the fixed-size passes plus GI HIGHEST.
  - The settings that actually decide the Ally's frame are **GI**, the render scale, and the fixed-size SHADOW /
    VOLUMETRIC passes.

## 3. GI HIGHEST in detail (the one big MAX cost)

**How the scales are computed.**
- `GiScaleLookup(which)` 0x187A50 builds a 3×6 float table on the stack from .rdata (0x3BD4650, 0x3BD4640, 0x3BD4630,
  0x3BD4620, 0x3BD41B8, 0x3BD41E0).
- It picks a row through the map {0,2,1,0} at 0x3BD4610 and returns `row[g_CfgGi]`. With GI = −1 it returns column 2.
- Its only caller is `GiRenderScalesApply` 0x1872C4, which runs every frame. The four results go to setters for the
  named render params `IndirectRenderScale`, `ReflectionRenderScale`, `IndirectRenderScaleForOcn` and
  `ReflectionRenderScaleForOcn` (strings at 0x4108A88…, registered by 0x365E464).
- The fourth setter only runs when sky byte `[0x45B9E58]+0x500 != 0xC0`.

**What the scales drive.** They are the dispatch size of the per-pixel probe-field gathers
(`RefProbe-CllPtSfbSplsSslsSsmVsm`, `notes/invisible-gpu.md` §1):

| Dispatch | Gather | Cost |
|---|---|---|
| #121 | diffuse GI | 12.4k instructions, **120 VGPRs on gfx1103 → 4 waves64 / SIMD**, 570 typed `buffer_load_format_x` + 216 `buffer_load_b96`, 586 full `s_wait_loadcnt` (latency-serialised probe search: 3 levels × 8 corners, then per-probe SH + octahedral visibility) |
| #414 | reflection | 24.7k instructions, 121 VGPRs, 1 KB LDS |

- Each gather is dispatched twice, once per stencil light group, and each dispatch covers the whole grid.
- The water views ("Ocn") repeat the gathers.

**Cost per GI level** (r5 µs × pixel ratio; upper bound, because early-out waves scale too):

| GI | PC µs | Ally ms (×6–8) |
|---|---|---|
| STANDARD | 589 | 3.5–4.7 |
| HIGH | 707 | 4.2–5.7 |
| VERY_HIGH | 847 | 5.1–6.8 |
| **HIGHEST** | **2,511** | **15–20** |

**Look.**
- HIGHEST removes the engine's own sub-resolution gather and upsample.
- The engine's path is a screen-size RT array (0x58D0A6F2) with a scale/offset in the cbuffer, and it is already used
  at 0.3…0.7.
- Diffuse GI is low-frequency, and a temporal denoiser node exists (`ReflectionProbeDenoiser`).
- The water views' GI is seen only through a distorted water surface.

## 4. Ranked savings at MAX (same look first)

Ally ms = at MAX native 1080p, ×6–8 of PC (more at today's 1.0–1.1 GHz clock). "Look" grades:
- **identical**: same output up to float reassociation;
- **near**: a sub-pixel or sample-count change no one should see at the screen;
- **low / medium**: visible in A/B screenshots if you look for it.

| # | Item | PC ms saved | Ally ms saved | Look | Delivery | Status |
|---|---|---|---|---|---|---|
| 1 | **CPU watts → GPU clock.** The KIDS wake storm, ~15.6 W of CPU in today's run; GPU at 1.0–1.1 GHz of 2.7 | – | **largest: every ~1 W ≈ +4–5 % GPU clock** | identical | CPU agent (ASI) | not mine; noted |
| 2 | **GI1-P1: water views at their HIGH scale under GI HIGHEST** (IndirectOcn 1.0→0.35, ReflectionOcn 1.0→0.6) | 0.25 (water scenes) | **1.5–2.0** | near (water reflection/refraction only) | ASI cave §5.1 | spec built + validated |
| 3 | **GI1-P2: diffuse IndirectRenderScale 1.0→0.75 under HIGHEST** | 0.41 | **2.5–3.3** | low (low-frequency + denoised; still above VERY_HIGH's 0.7) | ASI cave §5.1 | spec built + validated |
| 4 | **GI1-P3: ReflectionRenderScale 1.0→0.75 under HIGHEST** | 0.56 | **3.3–4.4** | medium (glossy/wet reflections) | ASI cave §5.1 | spec built + validated |
| 5 | **RADV wave32 for compute** (`RADV_PERFTEST=cswave32`). RefProbe is divergent (275 `execz` branches) and VGPR-heavy | ? (Ally only) | −10 % … +10 % of compute | identical (any wave size is legal for shaders without `WaveSize`) | launch option | **experiment** §7 |
| 6 | WDO#28 shadow-mask blur latency fix (render res: 367 µs at 1080p) | 0.15–0.25 | **1.0–1.7** at native (0.25–0.4 at 50 %) | identical | file swap | built (round 1), installer for the Ally added |
| 7 | ESM blur, gathers (2048², fixed) | 0–0.13 | 0–0.9 | identical | file swap | built (round 2), installer added |
| 8 | VFSCAT#235 light-loop latency rewrite (42 loads / 42 full waits in the loop; 93 VGPRs, 16 KB LDS) | 0.06–0.12 (async on PC) | 0.4–0.9 | identical | file swap | **not built** (§6) |
| 9 | RefProbe light-group dispatches → one dispatch; the second full-grid dispatch per gather is mostly early-out waves | ≤0.1 at HIGH, ≤0.3 at HIGHEST | 0–2 | identical | ASI (CPU) + shader | investigate: needs per-group pixel counts |
| 10 | Barrier tails / tiny segments (round 2 #3) | 0.5–1.0 | 1–2 | identical | ASI D3D12 hook | design (round 2) |
| 11 | MOTION_BLUR HIGHEST → HIGH (10 → 8 samples) | 0.05–0.1 | 0.2–0.5 | near | menu | settings advice |
| 12 | SSR HIGHEST → HIGH (steps ×2 → ×1.5) | 0.03–0.04 | 0.2–0.3 | near (shorter max ray) | menu | settings advice |
| 13 | AO HIGHEST → HIGH | ~0 here | ~0 | identical here | menu | keep HIGHEST |
| 14 | SHADOW HIGHEST vs HIGH (flag 0x7598 only) | ? | ? | ? | – | measure (§8) |
| 15 | LANDSCAPE ULTRA vs HIGH (LOD 0.8 vs 0.5, 4000 vs 2000) | ? | ? (+ CPU) | low–medium (distant terrain) | menu | measure |
| – | GI HIGHEST → VERY_HIGH (menu) | 1.66 | 10–13 | the real question: visual A/B needed | menu | the yardstick for 2–4 |

If everything that is "identical" or "near" lands (2, 5–12), it is worth ~5–12 ms on the Ally. Adding 3 and 4 gives
~11–20 ms. Neither reaches 16.7 ms at MAX native (§2).

## 5. Engine-side designs (ASI; `mod\Nioh3EngineFix\src` belongs to another agent, nothing edited there)

### 5.1 GI1 — `caveGiScale`: override the GI HIGHEST render scales (live, per frame)

**Site.** `GiScaleLookup` 0x187A50, 170 bytes. It is a leaf (a /GS cookie, no other side effects) that returns xmm0.
- Callers: 0x18745A, 0x187464, 0x187471, 0x18747F, all in 0x1872C4, every frame.
- The AOB `48 83 EC 78 48 8B 05 ?? ?? ?? ?? 48 33 C4 48 89 44 24 60 0F 28 05 ?? ?? ?? ?? 0F 28 0D ?? ?? ?? ?? 48 63 C1`
  is **unique in the image** (validator).
- Verify all 170 bytes with the seven disp32 operands masked (`maxsettings_spec.py` SITES).

**Patch.**
- Write `E9 <rel32>` at 0x187A50 to a near trampoline (`jmp qword ptr [rip+0]` → caveGiScale), using the safe-write
  (thread-IP check) the ASI already has.
- Revert by writing back `48 83 EC 78 48`.
- It is a jump, not a call, so the cave returns straight to the game's caller.

```asm
; caveGiScale - replaces GiScaleLookup 0x187A50. in: ecx = which (0 Indirect, 1 Reflection, 2 IndirectOcn,
; 3 ReflectionOcn); out: xmm0. Clobbers rax rcx rdx only (Win64 volatile; the original clobbers rax rcx too).
PUBLIC caveGiScale, gi_table, gi_pCfgGi
.data
ALIGN 16
gi_table   REAL4 24 dup(0.0)   ; [which][GI 0..5], filled at install (below), then ini overrides
gi_pCfgGi  dq 0                ; = RIP target of the movsxd at 0x187A50+0x80 (0x45BAF88, g_CfgGi)
.code
caveGiScale PROC
    mov     rax, gi_pCfgGi
    movsxd  rax, dword ptr [rax]
    cmp     eax, -1
    jne     @F
    mov     eax, 2                  ; original: GI == -1 -> column 2
@@: movsxd  rcx, ecx
    lea     rdx, gi_table
    imul    rcx, rcx, 24
    add     rdx, rcx
    movss   xmm0, dword ptr [rdx+rax*4]
    ret
caveGiScale ENDP
```

**Filling `gi_table`.**
- Decode the RIP operands inside the verified function (disp32 at +0x16, +0x1D, +0x2C, +0x39, +0x45, +0x53, +0x69,
  +0x83). This gives the six float sources, the row map and g_CfgGi.
- Rebuild the stack table exactly as 0x187A50 does: rows = [s0..s3 + s4[0..1]], [s4[2..3] + s5[0..3]],
  [s6[0..3] + scalar + scalar]; row = map[which].
- `maxsettings_spec.py` does this and proves the cave table equals the original lookup in every cell that isn't
  overridden (GI −1..5, all 4 params, all presets).
- Then apply the ini to column 5 (HIGHEST) only. Optionally column 4 too.

**ini / menu proposal.**
- `[GI] HighestScales = 1.0, 1.0, 1.0, 1.0` (Indirect, Reflection, IndirectOcn, ReflectionOcn), clamped to 0.25…1.0.
- Presets: P1 = `1.0, 1.0, 0.35, 0.6`; P1+P2 = `0.75, 1.0, 0.35, 0.6`; P1+P2+P3 = `0.75, 0.75, 0.35, 0.6`.
- It works as a live menu slider, because it is read every frame. Off = stock table.

**How to verify (RGP).** With GI HIGHEST, at 1920×1080:

| Setting | RefProbe#121 main | #414 main | water #121 | water #414 |
|---|---|---|---|---|
| Stock HIGHEST | 240×135 | 240×135 | 240×135 | 240×135 |
| P1+P2+P3 | 180×102 | 180×102 | 84×48 | 144×81 |

**Risks.**
- Visual only. These are the engine's own scale paths (0.3–0.7 are used by the other levels).
- One frame of temporal-history mismatch when the value changes live.
- Co-op-neutral (local rendering).
- Under GI ≠ HIGHEST the table is unchanged.

### 5.2 RefProbe light-group dispatches (investigate before designing)

- Render-graph `ReflectionProbePass` / `SSAO` / `DeferredIndirectPass` nodes have
  `ParallelStencilLightGroupMask [1, 2]`. Each group dispatches the full grid and early-outs on the stencil test.
- At HIGHEST, the group-1 dispatches would be ~97 µs (#121) and ~229 µs (#414) on the PC at 1.0, if they cost what
  they do at 0.6.
- What we don't know is how much of that is real work (character pixels) and how much is empty waves.
- A capture with per-dispatch wave counts settles it. Only then design a merge, which needs both groups' cbuffers in
  one dispatch, so a shader change plus a node change.

### 5.3 Not worth patching

- MOTION_BLUR HIGHEST (imm 0x0A at 0x39B4DA) and SSR HIGHEST: patching them to their HIGH values is the same as
  picking HIGH in the menu.
- The shadow-HIGHEST flag (0x18A99E): its meaning is unknown, so measure first (§8).

## 6. Shader replacements: why no new one this round

Everything that MAX makes more expensive was checked against what a file swap can do with an offline proof:

| Shader | Verdict |
|---|---|
| **RefProbe#121 / #414** (the GI HIGHEST cost) | Serialised and latency-bound: 586 / 741 full load waits, at 4 waves per SIMD on gfx1103. A batched-load rewrite could plausibly be 1.3–2× faster, which is the biggest same-look lever left (5–9 ms on the Ally at HIGHEST). But they are 12.4k / 24.7k-instruction shaders. There is no DXIL assembler here (`dxc` treats `.ll` as HLSL), so a rewrite means a hand port to HLSL with no way to prove equivalence offline. **Needs a GPU-validated pipeline:** a DXIL interpreter or a "render both and diff" harness on a PC with the game |
| **VFSCAT#235** (VOLUMETRIC HIGH = max; ~1.8–2.3 ms on the Ally, fixed size) | The light loop issues 42 loads, each with its own wait; 93 VGPRs, 16 KB LDS. A software-pipelined port is feasible (3.2k DXIL lines: clustered lights, cube/VSM/parallel shadow compares, LPV, 3D UAV), but it can't be validated offline. Next candidate once there is a GPU diff harness |
| **HBAO** (AO HIGHEST) | Small and easy (a runtime direction/step loop with one sample per step: a WDO28-style fix). But in r5 its draws are 0.4 µs (masked), so it's worth ~0 |
| PB2LS terrain (LANDSCAPE ULTRA), G1E particles, PB2DS lights, culling (MPC/MshLod: bindless waterfalls with true dependency chains) | Too large, scene-specific, or genuinely dependent |
| ESM, WDO#28 (already built) | Unchanged at MAX: same 2048² ESM, render-res mask |

## 7. Ally steps (Bazzite / Proton)

**Staged shader swaps** (GHDM is already installed on the Ally; ESM and WDO#28 are optional A/B):
```
# copy gpu-shaders/ally_shaders.sh and gpu-shaders/replacement/*.file (same folder layout) to the Ally, then:
chmod +x ally_shaders.sh
./ally_shaders.sh status                 # ORIGINAL / REPLACEMENT / UNKNOWN per file
./ally_shaders.sh apply esm              # or wdo28, ghdm, all  (refuses while Nioh3.exe runs)
./ally_shaders.sh revert esm             # exact restore from ~/.local/share/nioh3-shader-backup (sha256-checked)
# NIOH3_DIR=/path/to/steamapps/common/Nioh3 if auto-detection fails; Steam "Verify integrity" also reverts.
```

| File | Original sha256 (v2.02) | Replacement sha256 |
|---|---|---|
| ghdm `0x0104840c.file` | `32151bd9d09f21dd995fa1ccfd23b8fd0bd7d5e02463740641b013db2675bb87` | `129f03a0971e0a073b346224588e2ec0dbc41a4240e1a42efd285693e5742ce9` |
| esm `0x1ca8bb39.file` | `22103ae7d838771f4aa8e9506855c2dc04820feece73fe7dbcacddecfe56dd54` | `3c34641f1fb8d53bcac7111b66b2212f7a9ff1e7378d736626bebdf7f42bc73a` |
| wdo28 `0x1fd07cc0.file` | `197f0c438e05b68cf89d4f3f03e20c0a903bcffa6198a93a43c938e960d28ab3` | `a38f7de7522c4c28d75e458b4a8f6ba9c7015731365a313d2c745a83607536f9` |

Tested here against a scratch copy of the game files: apply, revert, re-apply, the refusal on a modified file, and
byte-exact revert.

**Wave32 experiment** (output-identical, nothing to install):
1. Same save, same spot, fixed TDP. Play one warm-up minute (shader compile).
2. Run 120 s ABAB with MangoHud logging, launch options:
   - A: `%command%`
   - B: `RADV_PERFTEST=cswave32 %command%`
3. Optionally C: `RADV_PERFTEST=cswave32,pswave32 %command%`.
4. Compare presented fps, GPU clock and GPU W.

These flags are documented in Mesa's envvars ("enable wave32 for compute / pixel shaders (GFX10+)"). If a Mesa version
already defaults to wave32, B equals A.

**GI1** (once the ASI has it): A/B GI HIGHEST stock vs P1, then P1+P2, at a spot with water and with glossy surfaces.
Take screenshots plus RGP (expected grids in §5.1).

## 8. Ally RGP captures that would sharpen every number here

Use `MESA_VK_TRACE=rgp MESA_VK_TRACE_TRIGGER=/tmp/rgp_trigger %command%`, then `touch /tmp/rgp_trigger`. Same spot,
fixed TDP, mod on.

1. **MAX native 1080p** (REN_RAT 100, all MAX). Replaces the §2 model with facts: the real ×factor per family.
2. **The same with GI VERY_HIGH, and with GI HIGH.** Measures the GI HIGHEST cost directly (expected +10–14 ms).
3. **SHADOW HIGHEST vs HIGH**, and **LANDSCAPE ULTRA vs HIGH**: the two unmeasured MAX steps (SM-* draw count, PB2LS
   time, CPU frame time).
4. **`RADV_PERFTEST=cswave32`** at the same settings: RefProbe, VFSCAT and culling dispatch times, wave32 vs wave64.
5. Per-dispatch wave counts for RefProbe group 1 vs group 2 (§5.2).
6. The run's GPU clock and power next to each capture (`gpu_metrics`). The ×factor depends on them.

## 9. Files

| File | What |
|---|---|
| `gpu-shaders/maxsettings_spec.py` | offline validator: site bytes, tables re-derived from the code operands, GiScaleLookup emulation, r5 grid cross-check, cost model, preset proofs, AOB uniqueness → `maxsettings_patches.json` |
| `gpu-shaders/maxsettings_patches.json` | GI1 (and MB1, for completeness) patch spec for the ASI agent |
| `gpu-shaders/ally_shaders.sh` | Linux status / apply / revert for the GHDM / ESM / WDO#28 swaps (sha256-checked, refuses while the game runs) |
| `re/symbols/incoming/gpu-maxsettings.tsv` | new symbols (GiScaleLookup, GiRenderScalesApply, the quality tables, setters, flags) |
| `re/scripts_gpu2/out/isa/{329e8168…,2011ba60…,6581fb6f…,192cee1e…}.txt` | gfx1201 ISA of the culling shaders checked in §6 |

## GP2 digest (`re\scripts_gpu\gp2_digest.py`)
`py -I re\scripts_gpu\gp2_digest.py <modlog> [more logs] [--top N] [--last] [--out PREFIX] [--rebuild]` turns the `GP2:` rows of a `GPUProf = 2` log into `PREFIX.md` + `PREFIX.csv` (default `<log>.gp2.*`): rank, ms/frame, % of the mean GPU span, stage, marker (pass), where the shader lives (`shader_dx12\0x*.file record@off`, `exe-embedded @rva`, `runtime-compiled (dump)`), group/pass name, whether a replacement exists, notes.md line that mentions the hash. Mean over all windows with PSO stamps ON (A/B OFF windows skipped); `--last` = newest window only. Hash index (container and DXIL HASH-part digests; from `dxil_index.csv`, `runtime_inventory.csv`, `inventory.csv`, the Ally dump 172 file names, psoswap `exe/ext` log lines, notes) is cached in `D:\nioh3-re\gp2_hash_index.json` (`--rebuild` to refresh). Replacement detection: `gpu-shaders\replacement\*.file` unpacked and diffed per record against `gpu-shaders\backup\*.file` (GI, fog, sweep/culling, post files), plus `post\out\*.dxbc`. Test: `py -I re\scripts_gpu\gp2_digest.py --selftest` (synthetic log; set `GP2_REBUILD=1` to rebuild the index first). Unknown hashes print `UNKNOWN (not in index)` (e.g. a shader built at runtime that the dump missed).
