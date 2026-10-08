# GI HIGHEST: is any of the work provably redundant? (design audit, 2026-10-08)

This is static work only. The game was not launched, and nothing in `mod\` src or in the game folder was changed.

**Inputs**
- **Decompile:** `GiRenderScalesApply` 1872C4 and `ReflectionProbePassRecord` 542C08 (D:\nioh3-re\decomp), plus capstone on `re\Nioh3.dump.exe`.
- **PC trace:** the r5 RGP event table (`captures\rgp\Nioh3_2026-10-06_19-39-58.events.csv`, RX 9070, GI HIGH, 1080p render). Per dispatch it gives the waves, the wave-µs and the µs.
- **Ally data:** the GP2 per-PSO CSV (`captures\ally\gp2_L2_full.csv`, Ally at about STANDARD and 540p).
- **Shaders:** every RefProbe record in `shader_dx12\0xd070067d.file`, disassembled with dxc. They are #7, 9, 11, 13, 14, 15, 25, 26, 29, 101, 117, 121, 412 and 414.

**Rules.** Only same-look changes count. The RADV lesson (STATUS line 79) applies: RGA and offline models do not predict RADV/ACO, and the rp121 rewrite was +0.9 ms *slower* on the Ally. So every Ally number below is **E** (PC r5 × 6–8) until an Ally A/B says otherwise.

## 0. Verdict

- **No duplicate math was found.** Beyond the resolution scale, HIGHEST has:
  - no probe update that runs twice;
  - no full-resolution pass whose result is later reduced the same way;
  - no history that is recomputed;
  - no per-frame rebuild of static GPU data.
- **The exact waste is empty waves.** The gathers dispatch the whole screen grid and then early-out per pixel on a stencil test. At HIGHEST every grid becomes 240×135 = 32,400 waves. Six of the eight gather dispatches per frame are 95–99 % early-out waves:
  - the two "light group 1" main-view dispatches;
  - the four water-view (Ocn) dispatches.

  Projected to HIGHEST, those six dispatches take ~0.65 ms of the ~2.5 ms PC gather time, which is **~4–5 ms on the Ally (E)**. Of that, empty waves are roughly 0.35–0.55 ms PC, or **~2–4 ms Ally (E)**.
- **Only one item can be delivered cheaply and exactly: A, the light-group merge** (§2). It is exact **only if** a runtime probe shows that the two light-group dispatches differ in `gv_mask_param` alone.
  - Value: **~0.7–1.9 ms on the Ally at HIGHEST (E).**
  - Best case: it needs an ASI change only and no shader rebuild, which avoids the RADV codegen risk.
- **The water-view empty waves (B) are exact to remove only through tile compaction or GPU predication.** That is engine surgery: a design only, measure first.
- **Everything else is written off, with the reason stated** (§4).

## 1. Quality table → pass parameters (what HIGHEST actually changes)

**The scale lookup.** `GiRenderScalesApply` 1872C4 runs every frame from `GameUpdateTick` 2338B0. It calls `GiScaleLookup(0..3)` and sets four render params through ParamTable setters:
- Indirect, through 8CCB80, hash D8A8960F;
- Reflection, through 8CCABC, hash EA3FCBD2;
- IndirectOcn, through 89A9D0, hash 01FCC200;
- ReflectionOcn, through 89AA94, hash 0F38BF83. This one is skipped when sky byte `[45B9E58]+500 == C0`.

HIGHEST sets all four to 1.0. HIGH sets 0.6 / 0.5 / 0.35 / 0.6 (`gpu-maxsettings.md` §3).

**Other params the same function sets (not GI-quality dependent).**
- Seven float params and one u32 param from the frame rate (`param_1+0x614` / 60), plus two flags. Names are in `re\symbols\incoming\gi-audit-renames.tsv`. The hashes are not RenderConfig fields, and their names are unknown.
- None of them depends on `g_CfgGi`. **The GI level changes only the four scales.**

**How the scale reaches the shaders.** Inside the gathers it is `gv_info_in.z` (= 1/scale):
- the gather pixel `tid` reads the G-buffer at `(tid+0.5)·gv_info_in.z`;
- the grid is `gv_info_out.xy`.

At 1.0 the gather pixel equals the screen pixel. No sub-pixel offset or jitter is introduced. Neither gather reads `gv_time_param`.

**The RefProbe file's pass family**, identified from bindings and code. Ally ms are from GP2 at ~STANDARD/540p.

| Record (shader hash) | Kind | Role | Ally GP2 ms |
|---|---|---|---|
| #121 (dcbbd6eb / GP2 4b4c3cbe) | CS 8×8 | diffuse gather → `uOutTex1[tid]` (only UAV store) | 0.56 |
| #414 (89cf7520 / 6ae59bd0) | CS 8×8 | reflection gather → `uOutTex1[tid]`; `sky_occ` is **read-only** (9 bufferLoads) | 0.43 |
| #117 / #412 | CS | same interfaces as #121 / #414 (other permutations, probably the water-view ones; RGP PSO 10e5d3ea) | 0.12 / 0.06 |
| #9 (7c978f3f) | PS | **bilateral upsample/composite**: 2×2 taps of t8 (diffuse) and t9 (reflection), weight = normal·depth·bilinear, floor `max(1e-3, w)`, × a vEye term → 2 RTs | 0.28 |
| #11 (76502ffd) | PS | #9 without the vEye term | 0.007 |
| #15 (7d51c698) | PS | **plain bilinear resample** of t8/t9 → 3 RTs `(rgb)`, `(rgb)`, `(a, a, 0)` | 0.11 |
| #7, #13, #14/#25/#26, #29, #101 | PS/CS | probe maintenance: octmap slice copy, SH → `shCoeff`, `sky_occ`, relight (VSM/main light), capture | ≤0.1–0.25, at rates ≤0.13 per frame (event-driven) |

## 2. Candidate A: merge the two stencil light-group dispatches (main view)

**What happens.** The render graph sets `ParallelStencilLightGroupMask [1,2]`. `ReflectionProbePassRecord` therefore dispatches #121 and #414 **twice each over the full grid**, once per group. Every lane tests `(u32(gv_mask_param.y) & stencil.g) == u32(gv_mask_param.x)` and exits on a mismatch. An early-out lane costs one `getDimensions` plus one stencil load, then returns. Every UAV store is after the test.

r5 trace, PC, GI HIGH:

| seg | Dispatch | Grid | Waves | wave-µs | µs/wave | µs |
|---|---|---|---|---|---|---|
| 228 | #121 group 1 | 144×81 | 11,664 | 13,311 | **1.14** | 35.5 |
| 229 | #121 group 2 | 144×81 | 11,664 | 195,742 | 16.8 | 302.1 |
| 230 | #414 group 1 | 120×68 | 8,160 | 10,545 | **1.29** | 57.3 |
| 231 | #414 group 2 | 120×68 | 8,160 | 167,815 | 20.6 | 259.8 |

**Reading the trace.**
- A pure early-out wave costs about 0.47–0.93 µs (the water-view dispatches, §3). A real wave costs about 17–21 µs.
- So **only 1–4 % of the group-1 waves carry any group-1 pixel**, and **35–80 % of the group-1 dispatch time is empty waves**.
- At HIGHEST, group 1 becomes 32,400 waves per gather:
  - #121 group 1 ≈ 99 µs PC;
  - #414 group 1 ≈ 227 µs PC.

**Why it is exact.**
- Group-1 and group-2 pixels are disjoint, given the same mask `y` and different refs `x`.
- Each pixel is written once, at `uOutTex1[tid]`, by exactly one of the two dispatches.
- No dispatch reads what the other writes: `sky_occ` is read-only, and `shCoeff` is never touched.

So one dispatch that accepts both refs produces the same texels **provided every other input of the two dispatches is identical**:
- the cbuffer except `gv_mask_param`;
- the descriptor tables;
- the SSR result.

That condition is **not provable statically**. The per-dispatch cbuffer is written by the shader object the record function calls through a vtable, and the per-group parameters were not traced. It needs probe A0.

**Mechanisms, cheapest first.**

| Step | What | Exact if | Owner |
|---|---|---|---|
| A0 probe | `gpuopt` Dispatch hook: when the bound CS PSO is #121 or #414 (original hashes), log the grid, the root params (descriptor-table GPU handles, CBV VA) and `gv_mask_param` (x,y). Byte-diff the two dispatches' cbuffers: hook `ID3D12Resource::Map` on UPLOAD-heap resources to build a VA→CPU map, then compare the CPU copies at record time. Log every 300th frame, at 3 spots (characters, open sky, water) | – (read-only) | ASI src owner |
| A1 ASI-only | Skip the group-1 Dispatch. Before the group-2 Dispatch, rewrite its `gv_mask_param` in the mapped upload memory to an `(x', y')` that accepts exactly the group-1 ∪ group-2 stencil values. This is safe at record time because the GPU has not consumed the ring slot yet. **No shader change → no RADV codegen risk.** | A0 shows (1) only `gv_mask_param` differs, (2) a single masked equality covers both refs. Example: refs that differ in exactly one bit inside `y` (`0b10` and `0b11` → `y'=0b10`, `x'=0b10`). With refs 1 and 2 under mask 3 this is impossible; then go to A2 | ASI |
| A2 psoswap | #121 and #414 with the test changed to `t == x || t == X_OTHER` (the other group's ref as a literal from A0). When `x == REF_G1`, the wave returns uniformly before any load (32,400 trivial waves ≈ 16 µs on the Ally). Proof: `gi\verify.py` with both groups in the scene; the merged write set and values must be identical to the union of the two original dispatches | A0 condition (1). Build it as a **DXIL-level minimal edit**, or from the pinned non-collapsed round trip (`mkfast` pins, *no* loop collapse) so the ISA stays ≈ shipped. Never reuse rp121_fast / rp414_fast | GPU agent |

**Expected gain at GI HIGHEST.** Group 1 × empty share = 40–80 µs (#121) + 80–160 µs (#414), i.e. **0.12–0.24 ms PC → 0.7–1.9 ms Ally (E)**. Mixed tiles that today run in both dispatches run once, which is a small extra gain.
- At HIGH the same change is worth about a third of that, ≈0.2–0.7 ms Ally (E).
- At the user's current STANDARD/540p it is ≈0.05–0.15 ms.

**Risk.**
- A1: wrong `(x', y')` → GI is missing on some pixels or written twice on others. The A0 log decides this before A1 is built.
- A2: RADV codegen.
- Both: a group-specific parameter that A0 misses because it is constant at the probe spots. Mitigation: probe in 3 scenes and include a cutscene.

**Ally A/B.**
- Setup: GI HIGHEST, REN_RAT 100, a fixed spot with the player plus several enemies, VSync off, GPUProf off.
- ABBA, 4 × 60 s MangoHud. Metric: GPU frame ms.
- Plus one GP2 window per arm for the #121/#414 per-PSO ms. Expected: group-1 time gone, group-2 time +0–5 %.
- Repeat at an open-sky spot.
- Ship only on an Ally win (STATUS rule).

## 3. Candidate B: water-view (Ocn) gathers at full resolution

**What happens.** Each frame has two water-view pairs: segs 283/284 and 304/305. #121 runs at IndirectOcn and #414 at ReflectionOcn. They run on the water-edited G-buffer and are stencil-gated to water pixels. They are **not** split by light group.

| seg | Dispatch | Grid (HIGH) | Waves | µs/wave | µs | At HIGHEST (1.0) |
|---|---|---|---|---|---|---|
| 283 | #121 | 84×48 | 4,032 | 0.93 | 23.9 | ×8.04 → ~192 µs |
| 284 | #414 | 144×81 | 11,664 | 0.61 | 23.1 | ×2.78 → ~64 µs |
| 304 | #121 | 84×48 | 4,032 | 0.64 | 5.6 | ~45 µs |
| 305 | #414 variant (PSO 10e5d3ea) | 144×81 | 11,664 | 0.47 | 10.0 | ~28 µs |

**What it costs.** µs/wave is at the early-out level, so these dispatches are ≥95 % empty waves. Their duration has a floor set by the few real 17–20 µs waves on water pixels.
- At HIGHEST: ~330 µs PC, of which ~230–300 µs is empty overhead.
- **~1.4–2.4 ms on the Ally (E), in scenes where the water view runs.**

**Why there is no cheap exact skip.** The CPU does not know which tiles hold water pixels, and the dispatch grid cannot be offset: `uOutTex1[tid]` has no origin parameter.

Exact options, both engine surgery:
- **B1, GPU predication.**
  - A small, high-occupancy CS counts water-stencil pixels into a buffer.
  - The ASI wraps the 4 water dispatches in `SetPredication(buf, 0, EQUAL_ZERO)`. vkd3d-proton maps this to VK_EXT_conditional_rendering, which RADV supports.
  - This is exact, because with zero water pixels every lane early-outs and nothing is written.
  - It only helps in frames where the water view runs with **no** water on screen. Frames with water lose the counting pass, ≈0.05–0.1 ms on the Ally.
  - First measure how often that happens. Node 153 already sits in a `BeginCheckSkip '!'`, so the engine may already skip it.
- **B2, tile compaction + ExecuteIndirect.**
  - The work: a classify pass, a tile list, and gathers that remap the group id from that list.
  - The list could be read through the bound-but-unused `shCoeff` UAV (u2) of #121, but #414 has no such free slot. That means new descriptor tables built by the ASI, i.e. weeks of work.
  - It would also remove the group-1 and sky waves of §2 in one go, ≈2–4 ms Ally (E) at HIGHEST.
  - Not recommended before A0/A1 and a measurement.

**Measure first (Ally, no code).** One GP2 window at a river spot at GI HIGHEST vs HIGH, then the same at a spot with no water in view. The water dispatches use the same PSOs as the main view, so compare per-PSO totals. If the no-water spot shows the same RefProbe ms as a water spot, the water view runs empty and B1 is worth building.

**Not same-look:** GI1-P1 (Ocn scales 0.35/0.6 under HIGHEST, `gpu-maxsettings.md` §5.1). It is the cheap near-look alternative, out of scope here.

## 4. Write-offs (checked, nothing exact to take)

| Idea from the brief | Finding | Verdict |
|---|---|---|
| Full-res pass whose result is later downsampled/denoised the same way | At 1.0 the composite **#9 is not an identity**. Neighbour taps keep a `max(1e-3, w)` floor (bilinear weight 0 → 1e-3), so the output is a 4-tap blend, and its vEye term is new work. It must run | no |
| …the plain resample #15 at 1.0 | **It is an exact copy at 1.0.** The UV lands on texel centres, filter weights quantize to 0, and it repacks rgb, rgb, (a,a,0) into 3 RTs. But consumers bind its outputs, so removing it means rebinding and swizzle edits in the deferred-shading PSOs. Value ~0.1 ms Ally at 540p (M), ~0.4 ms native (E), **only at HIGHEST** | write-off (engine surgery). Revisit only if B2 is ever built |
| Duplicate probe updates per frame | The probe-maintenance PSOs (#7/13/14/25/26/29/101) run at ≤0.13 per frame in GP2. Radiance update is event-driven (`invisible-gpu.md` §1; env params 0) | no |
| Shared gathers between Indirect and IndirectOcn (and Reflection / ReflectionOcn) | They run on different G-buffers. The main gathers run before the water draws (riverbed); the Ocn gathers run after the G-buffer edit (water surface, segs 291–302). The second water pair (304/305) follows further G-buffer draws (300–302) | inputs differ |
| #121 and #414 share the probe search at HIGHEST (same grid) | #414 searches at the reflection parallax point, 10 units off the surface (`gpu-gi-rewrite.md`); #121 searches at the surface. A fused kernel would need new bindings and more VGPRs (the RADV lesson) | inputs differ + RADV |
| Temporal reprojection recomputing unchanged history | Neither gather reads history or time. Between the gathers and deferred shading the r5 frame has only ≤1.2 µs passes (segs 232–234). `ReflectionProbeDenoiser` has no measurable pass; `RaytracingReflection` 154–156 is inactive | nothing to save |
| Dispatches over empty / culled tiles | Yes: this is A and B. Sky waves inside the group-2 dispatch are the same kind (scene-dependent: ~0 at the r5 shrine, where group 2 averages 16.8 µs/wave) | A / B2 only |
| Per-frame recompute of static data | GPU: none (probe tables are static buffers; cbuffer math is ALU). CPU: `GiRenderScalesApply` does about 20 param writes per frame, which is fps-neutral on the GPU-bound Ally | no |
| Bit-exact parameter tweak | The GI level changes only the four scales (§1). Any other scale value changes the look | no |

## 5. Next steps (in order)

1. **A0 probe** (ASI src owner, logging only; PC is fine for the equality question, the Ally confirms). Output: the refs and the cbuffer diff.
2. Depending on A0:
   - **A1** (ASI-only), if the refs allow a single masked test;
   - else **A2** (minimal-edit psoswap plus `verify.py` union proof);
   - else write A off.
3. The Ally A/B of §2. **One GP2 window at water vs no-water spots** decides whether B1 is worth designing further.

Files:
- this note;
- `re\symbols\incoming\gi-audit-renames.tsv` (10 per-frame param setters from GiRenderScalesApply).

Scratch disassembly of the RefProbe PS/CS records came from `re\scripts_gpu\extract_dxbc.py` plus `dxc -dumpbin` and was not kept.

## 6. Built: A0 probe + A1 merge (ASI 1.7.29, both default off)

Value re-anchored by `ally-gpu-budget.md`: **~0.25-0.5 ms Ally at MAX, ~0.03 ms at the user preset (E)**. The 0.7-1.9 ms above was PC r5 × 6-8 on a menu-diluted table.

**Code.** `mod\Nioh3EngineFix\src\gimerge.{h,cpp}` (new). Integration: `gpuopt_thunks.asm` (new `gpuopt_hook2` mask, `gpuopt_hold` list, a thunk returns without calling the original when `GpuOptSlow` returns 1), `gpuopt.cpp` (`StartInfo.giMerge`, gimerge before gpuopt's batch flush, `OnBarrier` in `HkResourceBarrier`), dllmain (Start after psoswap, stats line), osd (`OnFrame`).
- Hooks: D3D12CreateDevice import (chains psoswap) → device `CreateComputePipelineState` (PSO → shader digest), `CreateGraphicsPipelineState` (pointer reuse), `CreateConstantBufferView` (the CBV a dispatch reads: newest CBV of the target's size created on the recording thread since the last work call), `ID3D12Resource::Map/Unmap` (upload VA → CPU). Command-list calls arrive through gpuopt's thunks. All COM/IAT sites: plain ABI (`hook-register-audit.md`).
- Targets: #121 `4b4c3cbe` (mask @464) and #414 `6ae59bd0` (mask @480) mergeable; seven more gv_mask_param shaders of `shaderdump_172` are probe-only.

**Keys.**
- `[Debug] GIProbe = 1` (A0): every 30 s `stats/s giprobe:` (dispatches + groups per frame per shader, (ref, mask) pairs seen, CBV misses) and `stats/s gimerge:` (pairs ok/bad + why: pso, grid, no-cbv, roots, cbuffer, mask-value, union, barrier/work, same-cbv). Every `GIProbeEvery` (300) frames, up to `GIProbeFrames` (30), one frame of `GIPROBE` lines: grid, groups, gv_mask_param, CBV VA, gv_info_out, cbuffer rows and root params that differ from the previous same-shader dispatch, and `PAIR ok -> x' y'` or the reason. This answers A0: are the two dispatches identical except mask.xy, and is the union a single masked test.
- `[Patches] GIMerge = 2`: observe only (would-merge counts). `GIMerge = 1`: merge after `GIMergeQualify` (60) clean pairs.

**Exactness (A1).** A pair merges only when: same PSO and grid, exactly one root parameter rebound (the CBV table), cbuffers byte-identical except `gv_mask_param.x/.y` (so `.z/.w`, used in the math, are equal), and `MaskUnion` finds `(x', y')` with S(x',y') = S1 ∪ S2 over all u32 stencil values (closed form: containment, or same mask and refs one bit apart). Then group 1 is not recorded and `(x', y')` is written into group 2's own upload-ring cbuffer (read back to verify) before the list executes.
- Group 1 is **held**, not dropped. Any other call on that list (PSO, root signature, heaps, draws, copies, Close, a transition barrier) replays it first with its own root tables, then restores the current ones, plus a NULL UAV barrier when a UAV barrier was held over. Root-table rebinds and UAV barriers keep the hold.
- A failed pair disarms (observe again). If the first dispatch after a merged one still binds the same CBV table without a new CBV (it would read the rewritten mask), the merge is disabled for the session.
- Refs 1 and 2 under mask 3 can never merge (A2 territory); the probe shows which refs the game uses.

**Offline checks.** `py -I tests\run_gimerge_harness.py`: MaskUnion = brute force on all 2^20 5-bit cases; fake-list streams (qualify → merged frame has one dispatch and mask (2,2); cbuffer mismatch → replay `T2=g1 D T2=g2 U D`; transition → replay before it; Close → replay; reuse → disabled; refs 1/2 never arm). gp / r2 harnesses (they link gpuopt) and `test_patterns.py` pass. Not run in the game.

### Ally A/B protocol
1. **A0 (one session, ~10 min).** `[Debug] GIProbe = 1`, `[Patches] GIMerge = 2`, `[Profiler] GPUProf = 2`. GI HIGHEST, REN_RAT 100, gameplay (not menus): a spot with the player and enemies, open sky, a river; 2 min each. Read: `PSOs n target` > 0 and `Map hooked` (else the IAT/device hook missed: stop), the (ref, mask) pairs for rp121/rp414, `pairs ok` vs `why`, any `would-merge ... DISABLED`. Go to step 2 only if rp121/rp414 pairs are mostly `ok` and nothing was disabled. If `why: union` dominates → A2 (psoswap) or write off; `roots`/`cbuffer` → read the GIPROBE rows to see what differs.
2. **Visual.** `GIMerge = 1`, the same spots: screenshot pairs GIMerge 0 vs 1 (same camera, paused). Must be identical (GI is deterministic per frame). Log: `armed`, `merged ~2/f`, `replays` small, `conflicts 0`.
3. **Perf.** GI HIGHEST, REN_RAT 100, VSync off, GPUProf off, fixed spot. ABBA 4 × 60 s MangoHud, GIMerge 0 / 1 (GIProbe 0). Metric: GPU frame ms. Expected −0.25…−0.5 ms (E); then one GP2 window per arm: rp121/rp414 per-PSO ms down by the group-1 share. Repeat once at the user preset (expected ~0, only as a no-regression check). Ship only on an Ally win.
