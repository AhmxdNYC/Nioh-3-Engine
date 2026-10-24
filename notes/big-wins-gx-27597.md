# Big wins: pass reuse, dynamic resolution, variable-rate shading

2026-10-23. Offline. The game was not launched. Bridge #27607 (human #27597). gx-pc owns these three. cx-pc owns culling shaders and CPU waits (Ally USER culling family 2.88 ms is out of this note).

Sources: `notes/ally-gpu-budget.md` (USER span 28.7–29.2 ms, about 34.9 fps, REN_RAT 50, FSR 3.1, frame generation off), `notes/engine-rewrite-options.md`, `notes/gpu-resolution-audit.md`, `notes/same-look-package.md`, `notes/extra-performance-settings.md`, `notes/big-wins-pass-rank.md`. Tags: **M** measured, **E** estimate. An async wall-clock stamp is not frame time.

## The bar

15% of the Ally USER frame is **4.3 ms** off 28.7 ms. On pc2 the same bar is about **4.1 ms** of the ~27 ms target, or **3.5 ms** of the 23.2 ms GP2 stamp (`gpu-resolution-audit.md`). None of pass reuse or variable-rate shading reaches that bar. Dynamic resolution can, and only by drawing fewer pixels than the user already chose.

## Rank

| rank | change | where to hook | expected frame ms | risk | clears 4.3 ms? |
|---|---|---|---|---|---|
| 1 | Let internal resolution fall below the current REN_RAT on slow frames | Existing `DynResolutionController` `0x5AF7F8`. Compare is `0x10F3330` (target ×0.75 / ×0.9). Steps are `0x5AFA40` / `0x5AF950` (not decompiled here). Apply with `SetRenderRatioIndex` `0x1FDE3E8`. Bounds `DYNAMIC_RENDER_RATIO_MIN/MAX`, 25..100, steps of 5 | **E, not measured below 50.** Two-point model from U1 and U5b (below) needs about REN_RAT **35** to save ~4.4 ms on the Ally. One step, 65→60, is only ~1.2–1.5 ms on pc2 | Look change while it is dipped. Opt-in | only if the picture is allowed to soften |
| 2 | Half-rate GI gather (reuse last frame) | RefProbe CS `4b4c3cbe` (#121, 1.26 ms M). Scale path, which is not a rate: `GiRenderScalesApply` `0x1872C4` / `GiScaleLookup` `0x187A50` | **0.5–1 ms E** (half of the 1.77 ms USER GI family is 0.9). At MAX the gathers are ~7.9 ms, and 15% of that 64 ms frame is 9.6 ms, so half-rate still misses | Medium. GI lag, disocclusion | no |
| 3 | Variable-rate shading on G-buffer and deferred pixel shaders | **No engine site.** `Nioh3.dump.exe` has no `ShadingRate` / `RSSetShadingRate` string (searched 2026-10-23). A hook would be a new `RSSetShadingRate` before PB2 and deferred PS draws | **0.3–0.8 ms E** on pc2, **small on the Ally** (`engine-rewrite-options.md` rank 10). Upper bound if every G-buffer pixel ran at 2×2 and the vertex share stayed ~75%: about **1.4 ms E**, still short | FSR3 can amplify the blocks. Needs VRS tier 2. vkd3d fragment shading rate is unproven here | no |
| 4 | Far shadow cascade every 2nd frame | Shadow-map draws (family 1.38 ms M). Not the deferred resolve (2.04 ms), which still lights the frame | **0.1–0.2 ms E** Ally. Halving the whole map family would be ≤0.7 ms and would strobe near casters | Low–medium at cascade distance. High if the near maps are skipped | no |
| 5 | Half-rate clouds | Already done. Dispatch `0x1900E8` → `0x190350`, shader `b76874bb`, target render/8, 16-frame Bayer (`g_iBayerPosNum`) | Real frame cost **≤ ~0.3 ms E**. The 1.52 ms row is async wall time | Further reuse changes the sky for almost no frame time | no |

## 1. Dynamic resolution

The controller already exists. It averages 60 frames of a time history at `+0xA138` and compares that average with the target. Its input is frame time, so it drops resolution even when the CPU is the limit (`settings.md`). A mod would feed it GPU-busy time from our timestamps. Prologues at `0x5AF7F8`, `0x10F3330`, `0x5AFA40`, `0x5AF950`, and `0x1FDE3E8` were checked in `re/Nioh3.dump.exe` this session. The step functions were not decompiled.

`REN_RAT` is an enum, `OptsRatioPercent` = {25, 30, …, 100}. U5a sent 67, which is not in the list, and the game stayed at 50 (**M**). Use 65 or 70, or 35 / 40 for a dip.

Measured Ally arms, same upscaler launch (`ally-gpu-budget.md` §5):

| arm | fps | GPU span |
|---|---|---|
| U1 REN_RAT 50 | 34.2 | 28.76 ms |
| U5b REN_RAT 100 | 18.0 | 54.56 ms |
| U3 XeSS (still REN_RAT 50) | 33.3 | 29.56 ms |
| U6 FSR + frame generation | 49.0–49.3 displayed | not a GPU-time cut of the real frame |

Two-point model (**E**, two measurements, nothing below 50): span ≈ 20.2 + 34.4·r², with r = REN_RAT/100.

| r | predicted span | saved vs 28.76 |
|---|---|---|
| 0.40 | 25.7 ms | 3.1 ms (11%) |
| 0.35 | 24.4 ms | 4.4 ms (15%) |
| 0.25 (engine floor) | 22.3 ms | 6.5 ms (22%) |

`big-wins-pass-rank.md` is right that there is no capture below 50. The table above is that extrapolation, not a third measurement. The fit `2.56 + 4.43·r²` in the budget note is the FSR row only.

On pc2 at REN_RAT 65, most of the frame does not scale (shadow maps, cloud grid, culling, fog grid). The written estimate for 65→60 is **1.2–1.5 ms E**. Clearing ~4 ms there needs a deeper dip than one step, and it still changes the picture.

XeSS is the wrong swap on the Ally 780M: about 1 fps slower than FSR (DP4a). Frame generation is the measured way to ~49 displayed fps. It does not remove 15% of the GPU's real frame.

## 2. Pass reuse

**Clouds.** One pixel of each 4×4 block per frame, plus reprojection. `CLDR` is a history blend weight, not a rate. The same-look package measured it at the noise floor or slower. Do not build another cloud skip.

**GI.** USER family RefProbe / SDFOCC / HBAO is **1.77 ms M**. Half of that is under 1 ms, about 3% of 28.7. LPV has no per-frame GPU pass (`LPVR` dropped). Sky ambient is already time-sliced (`SKYR` dropped). `FOGT` is a blend weight and it changed the picture. XGI level 3 is a spatial scale (×0.35), not reuse: Ally **+2.1% fps M** on the ~41 fps extra-perf scene (bridge #1723). That is a different preset from this 29 ms budget. Do not add the 2.1% onto 28.7 ms. XSHD level 3 (shorter shadow distance) was **+0.1% fps M** on that same Ally run.

**Shadows.** Far-cascade reuse is the small honest hook (rank 8 in `engine-rewrite-options.md`). The deferred resolve stays. VSM tiles that are cleared but never sampled are a redundancy cap (bursts 3–5.7 ms on 1% lows, mean a few tenths). That is not an average-frame 15% win, and it is not a half-rate hook.

## 3. Variable-rate shading

The G-buffer is 7.54 ms, which is 26% of the frame, but the top PB2 pair is about 75% meshlet vertex shader (`ally-gpu-budget.md` §2 and §6). VRS does not cut vertex work. A 2×2 rate on the pixel fraction is the ~1.4 ms upper bound above. The published estimate is smaller. No on/off capture exists. Do not build it for this bar.

## Verdict

Ship nothing from this list as a same-picture 15% patch. The only candidate that can clear 4.3 ms on the Ally is an opt-in dip below REN_RAT 50, aimed at about 35, driven by GPU-busy time. It is unmeasured below 50. The best same-picture item here is half-rate GI, about 1 ms **E**.
