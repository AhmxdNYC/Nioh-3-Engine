# Big wins: pass reuse, dynamic resolution, variable-rate shading

2026-10-23. Offline. The game was not launched. Bridge #27607 (human #27597). gx-pc owns these three. cx-pc owns culling shaders and CPU waits (Ally USER culling family 2.88 ms is out of this note).

Sources: `notes/ally-gpu-budget.md` (USER span 28.7–29.2 ms, about 34.9 fps, REN_RAT 50, FSR 3.1, frame generation off), `notes/engine-rewrite-options.md`, `notes/gpu-resolution-audit.md`, `notes/same-look-package.md`, `notes/extra-performance-settings.md`, `notes/big-wins-pass-rank.md`. Tags: **M** measured, **E** estimate. An async wall-clock stamp is not frame time.

## The bar

+15% fps at 28.7 ms needs **3.74 ms** saved. **4.3 ms** is 15% less frame time, about +18% fps (cxl-pc #27724). On pc2, 15% of frame time is about **4.1 ms** of the ~27 ms target, or **3.5 ms** of the 23.2 ms GP2 stamp (`gpu-resolution-audit.md`). None of pass reuse or variable-rate shading reaches that bar. Dynamic resolution can, and only by drawing fewer pixels than the user already chose.

## Rank

| rank | change | where to hook | expected frame ms | risk | clears 3.74 ms (+15% fps)? |
|---|---|---|---|---|---|
| 1 | Let internal resolution fall below the current REN_RAT on slow frames | Not the 60-frame average. `DynResolutionController` `0x5AF7F8` calls `0x5AFA40`, which keeps a 3-sample minimum (`0x5AFAB0`, ring `+0x2B8`). `0x10F3330` is a later path and is not proof that replacing the 60-frame average moves the scale (cx-pc `58ffdeea`). The current policy floor is 50, so on Ally USER it saves nothing. A static REN_RAT 35 arm is the test (review A4) | **E, not measured below 50.** Same two-point model as cx-pc: about REN_RAT **35** saves ~4.4 ms on the Ally. One step, 65→60, is only ~1.2–1.5 ms on pc2 | Look change while it is dipped. Opt-in. Do not run it on top of cx-pc's fixed preset | only if the picture is allowed to soften |
| 2 | Half-rate GI gather (reuse last frame) | RefProbe CS `4b4c3cbe` (#121, 1.26 ms M). Scale path, which is not a rate: `GiRenderScalesApply` `0x1872C4` / `GiScaleLookup` `0x187A50` | **0.5–1 ms E** (half of the 1.77 ms USER GI family is 0.9). At MAX the gathers are ~7.9 ms, and 15% of that 64 ms frame is 9.6 ms, so half-rate still misses | Medium. GI lag, disocclusion | no |
| 3 | Variable-rate shading on G-buffer and deferred pixel shaders | **No call in the game exe.** `Nioh3.dump.exe` has no `ShadingRate` string (searched 2026-10-23). cxl-pc's helper (`cxl-pc/vrs-27602`) is unwired: the review counted 0 draws treated. The capture layer only names `RSSetShadingRate` | Review ceiling **0.5–1.0 ms**. Older note: **0.3–0.8 ms E** on pc2, small on the Ally. Loose upper bound if the whole pixel share of the 7.54 ms G-buffer ran at 2×2: about **1.4 ms E** | FSR3 can amplify the blocks. Per-draw 2×2 needs Tier 1. A shading-rate image needs Tier 2. Query the device; do not assume support from the GPU name | no |
| 4 | Far shadow cascade every 2nd frame | Shadow-map draws (family 1.38 ms M). Not the deferred resolve (2.04 ms), which still lights the frame | **0.1–0.2 ms E** Ally. Halving the whole map family would be ≤0.7 ms and would strobe near casters | Low–medium at cascade distance. High if the near maps are skipped | no |
| 5 | Half-rate clouds | Already done. Dispatch `0x1900E8` → `0x190350`, shader `b76874bb`, target render/8, 16-frame Bayer (`g_iBayerPosNum`) | Real frame cost **≤ ~0.3 ms E**. The 1.52 ms row is async wall time | Further reuse changes the sky for almost no frame time | no |

## 1. Dynamic resolution

The controller already exists, and it is two paths. cx-pc read the dump on `58ffdeea`: `0x5AF7F8` loads three timing fields at `+0xA12C/+0xA130/+0xA134` and calls `0x5AFA40`. That path keeps a three-sample minimum in `0x5AFAB0` (ring `+0x2B8`). The 60-frame average at `+0xA138` feeds `0x10F3330` only after that, and that compare is not proof the average is what moves the scale. Do not feed GPU-busy time in until those three producers are known. Frame time is still the wrong signal when the CPU is the limit (`settings.md`). This session only checked that the prologues at `0x5AF7F8`, `0x10F3330`, `0x5AFA40`, `0x5AF950`, and `0x1FDE3E8` are real functions.

The temporal policy's own floor is 50 and its ceiling is 75 (`big-wins-review-27604.md`). Ally USER is already 50, so that policy proposes no drop and saves 0 ms. A floor of 50 cannot beat a frame that is already rendered at 50. The experiment that matches the 15% bar is a static REN_RAT 35 (review arm A4), which is allowed to look worse. It stays off while cx-pc's fixed preset is on.

`REN_RAT` is an enum, `OptsRatioPercent` = {25, 30, …, 100}. U5a sent 67, which is not in the list, and the game stayed at 50 (**M**). Use 65 or 70, or 35 / 40 for a dip.

Measured Ally arms, same upscaler launch (`ally-gpu-budget.md` §5):

| arm | fps | GPU span |
|---|---|---|
| U1 REN_RAT 50 | 34.2 | 28.76 ms |
| U5b REN_RAT 100 | 18.0 | 54.56 ms |
| U3 XeSS (still REN_RAT 50) | 33.3 | 29.56 ms |
| U6 FSR + frame generation | 49.0–49.3 displayed | not a GPU-time cut of the real frame |

Two-point model (**E**, two measurements, nothing below 50): span ≈ 20.2 + 34.4·r², with r = REN_RAT/100. cx-pc's note gets the same 4.39 ms at 35% from these two arms.

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

The G-buffer is 7.54 ms, which is 26% of the frame, but the top PB2 pair is about 75% meshlet vertex shader (`ally-gpu-budget.md` §2 and §6). VRS does not cut vertex work. A 2×2 rate on the pixel fraction is the ~1.4 ms upper bound above. The published estimate is smaller. Per-draw 2×2 needs VRS tier 1. Tier 2 is only for a shading-rate image (cxl-pc #27724). No on/off capture exists. Do not build it for this bar.

## Verdict

Ship nothing from this list as a same-picture 15% patch. The only candidate that can clear 3.74 ms on the Ally is a static dip to about REN_RAT 35. The live policy does not do that today, and the 4.4 ms is the same unmeasured two-point model cx-pc already published. The best same-picture item here is half-rate GI, about 1 ms **E**.

pc #27770: the static 35% scale is cx-pc's build (#27751), before their culling shaders. Dynamic resolution stays with gx-pc. It is opt-in and default off. The stock floor is 50, so it saves 0 ms on Ally USER until a dip below 50 is explicitly enabled. It stays off while cx-pc's fixed preset is on. Hook points remain `0x5AF7F8`, `0x5AFA40`, and `0x5AFAB0`. No dynamic-resolution patch was built in this pass. Culling addresses for cx-pc are in `notes/culling-hooks-for-cx-27770.md`.
