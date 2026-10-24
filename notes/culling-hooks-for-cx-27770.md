# Culling hooks and costs for cx-pc

2026-10-23, offline. pc #27770: cx-pc owns the culling-shader build (task 27607), after the 35% scale option (#27751). gx-pc does not build it. Game not opened.

Ally USER (`notes/ally-gpu-budget.md`): 28.7 ms GPU, 34.9 fps, REN_RAT 50, FSR 3.1 to 1080p, volumetrics STANDARD, frame generation off. The culling family is **2.88 ms**. +15% fps needs **3.74 ms** saved. The whole family is under that bar, so a culling shader cannot be the 15% win by itself.

## Do not skip

| site | what it is | cost |
|---|---|---|
| `0x9046AA` in `0x904610` | Main-view MeshletResolveLOD. Calls `0x531948` | The expensive LOD resolve. pc2 dense: MshLod#1 0.56 ms plus MshLod#2 0.45+0.27+0.18. No duplicate |
| `0x8DA16D` in `0x8DA0A8` | Shadow clipmap LOD, same `0x531948`, different kernel and buffer | pc2 dense **0.114 / 0.095 ms**. RX 9070 share of a 0.92 ms front-end is about **0.015 ms**. Not a copy of the main resolve (`notes/meshlet-redundant-lod.md`) |

## Stamps are not the big kernels

S1 MinRun4 rows (`notes/handoff-codex-next-perf-triage.md`). MinRun4 merges different PSOs and bills the longest run.

| record in `0x29ee0902.file` | digest prefix | S1 row |
|---|---|---|
| MPC24 | `304bac05` | 1.015 ms, 9.9 calls. Short argument kernel, not the culling kernel |
| MPC22 | `a09d4223` | 0.352 ms, 12 calls |
| MPC20 | `18b6fee7` | absent from the top 100 |
| MPC33 | `cc40b6f0` | absent from the top 100 |
| MCLRC | `864b4941` | 0.65 ms, MeshletCheckRequestLoad |

Exact prefixes: MPC20 `18b6fee7ead259130f50a9c5803fcfe7`, MPC33 `cc40b6f0522ea9826b04dce64a0bc6b6`, MPC24 `304bac05646e17a2c24d873283d6993c`, MPC22 `a09d4223be2a00bf1c085a9080e73ac7`.

## Already tried

- Same-result MPC#23/#28-31 plus MshLod#3 rewrite (`notes/gpu-culling-rewrite.md`): about **0.055-0.06 ms** on the old RGP. Not shipped. The cnt skip at `0x5301FC` was not proven equal to the cbuffer `.y`.
- MPC20 lane-equal scalarize is unbuilt. RX 9070 r5 attributed **75.4 us**. No Ally number. Do the MinRun1 cost probe before a new shader.
- XLOD (meshlet LOD scale, `ApplyMeshletLodScales` `0x2353C0`, cave EPL1) is already opt-in. Ally XLOD2 was **-0.4%** at about 41 fps (GPU 98%, a different capture). Called none.

## Other addresses, if the build also moves CPU culling

- `MeshletPrecullingRecord` `0x5301FC`
- `CullEntitiesByDrawDistance` `0x1772B4`, `GetDrawDistanceLimit` `0x177274`
- Reuse of last-frame visibility: **0.3-1.5 ms E** (`notes/big-wins-pass-rank.md`). One-frame pops if it rejects more. Not additive with a shader rewrite of the same dispatches.

GPU-pass estimates do not move a CPU-bound pc2 frame.
