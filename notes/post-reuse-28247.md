# Screen-space half-rate prototype

2026-10-23, offline. The game was not launched. Branch `gx-pc/post-reuse`. Keys stay 0. No preset turns them on.

The existing half-rate hook skips one record in two and leaves the last target, because that record clears its own target. A camera jump over 4 m draws again.

| key | site | hooked? | Ally USER estimate |
|---|---|---|---|
| HRR | RealtimeLocalReflectionPass `0x295C44`. The clear is inside this record. Prologue `48 8B C4 48 89 58 18` | yes, when those bytes match | **0.04 ms E**. The SSR row is 0.08 ms, and half of it is 0.04 |
| HRA | ApplySSAOPass `0x3688234` (2135 bytes). Same prologue. This is not SSAONodeExecute `0x15612C`, which draws geometry | yes, same byte check | **0.02-0.08 ms E**. The 0.65 ms SSAO node is not this function |
| HRB | none | no | **0**. Bloom has no record of its own |
| HRP | none | no | **0**. Motion blur is 0.33 ms inside the post family, not a separate record |

Bloom and the rest of post run in PostEffectApply `0x6DA118` (about 28 KB). That function also runs the upscaler. Skipping it would freeze the upscale, so HRB and HRP stay off even if the ini says 1.

Added to the three already shipped (far shadows 0.10-0.20, GI 0.50-1.00, clouds 0.10-0.15), the new pair is about 0.06-0.12 ms. +15% fps at 28.7 ms still needs 3.74 ms. This does not reach it.
