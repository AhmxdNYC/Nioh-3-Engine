# Same-look A/B report: 20261009-1007-pc2sl1-tzfix

## VSMA while moving (stick runs, VSMA live, ABBA per dwell)

| cond | runs | frames | ft p50 | ft p99 | ft p99.9 | ft max | GPUBusy mean | GPUBusy p99 |
|---|---|---|---|---|---|---|---|---|
| VSMA0 | 32 | 6041 | 27.40 | 43.18 | 48.08 | 55.87 | 26.906 | 40.49 |
| VSMA1 | 32 | 6010 | 27.07 | 41.46 | 45.83 | 49.25 | 27.012 | 39.60 |

Paired per dwell (on - off, 16 dwells): frame time p99 -0.345 ms (sd 1.465, se 0.366), GPUBusy mean -0.117 ms (sd 0.981, se 0.245)

Mod lines (first / last):

```
[10:07:43.941] VSMA: VSM waiting-tile amortisation installed, switch off (K 2, front 2 x budget, first-visit draw off)
[10:07:43.997] RCO: update-rate knobs installed: LPVR off (LPVUpdateFrequency = 4), SKYR off (SkylightAmbientUpdateFreq = 4.000), FOGT off (VolumetricFogTemporalFactor = 0.500), CLDR off (VolumetricCloudReprojectionBlendWeightCurrentFrame = 0.500)
[10:07:47.231] RCO: engine values in S at the first frame: LPVUpdateFrequency 1, SkylightAmbientUpdateFreq 1.000, VolumetricFogTemporalFactor 0.000, VolumetricCloudReprojectionBlendWeightCurrentFrame 0.000; offset check: VolumetricFogResolutionX (S+13DC) 160, VolumetricFogResolutionZ (S+13E0) 64, SSAORayNum (S+12F4) 7
[10:08:14.090] stats/s VSMA (switch off): VsmClipmapRender 0.09/frame (9.3/s; stock: off 9.3, throttle param 0.0, other thread 0.0), tile visits 0.0/s; batch tiles drawn 0.0/s (marked 0.0, valid 0.0, front 0.0, phase 0.0, first 0.0) skipped 0.0/s = 0.0 % of the batch tiles | stats/s RCO: frames 206.1/s, LPVR off written 0 refilled 0, SKYR off written 0 refilled 0, FOGT off written 0 refilled 0, CLDR off written 0 refilled 0
...
[10:36:47.942] stats/s VSMA (switch ON): VsmClipmapRender 1.00/frame (40.2/s; stock: off 0.0, throttle param 0.0, other thread 0.0), tile visits 3.2/s; batch tiles drawn 24.9/s (marked 24.9, valid 0.0, front 0.0, phase 0.0, first 0.0) skipped 0.0/s = 0.0 % of the batch tiles | stats/s RCO: frames 214.0/s, LPVR off written 13355 refilled 13355, SKYR off written 13422 refilled 13422, FOGT ON written 13740 refilled 10813, CLDR off written 14402 refilled 14402
[10:37:17.999] stats/s VSMA (switch ON): VsmClipmapRender 1.00/frame (39.6/s; stock: off 7.0, throttle param 0.0, other thread 0.0), tile visits 3.3/s; batch tiles drawn 18.9/s (marked 18.9, valid 0.0, front 0.0, phase 0.0, first 0.0) skipped 0.0/s = 0.0 % of the batch tiles | stats/s RCO: frames 217.8/s, LPVR off written 14412 refilled 14412, SKYR off written 14478 refilled 14478, FOGT off written 14440 refilled 11514, CLDR off written 14402 refilled 14402
[10:37:48.043] stats/s VSMA (switch ON): VsmClipmapRender 1.00/frame (49.1/s; stock: off 8.6, throttle param 0.0, other thread 0.0), tile visits 10.0/s; batch tiles drawn 33.5/s (marked 33.5, valid 0.0, front 0.0, phase 0.0, first 0.0) skipped 0.0/s = 0.0 % of the batch tiles | stats/s RCO: frames 229.8/s, LPVR off written 14412 refilled 14412, SKYR off written 14478 refilled 14478, FOGT off written 14440 refilled 11514, CLDR off written 14402 refilled 14402
```

## Update-rate knobs (static spawn view, BASE brackets each dwell)

| knob | dwells | GPUBusy delta ms (mean, se) | shot mad | % px > 8 | SSIM | BASE-BASE mad | BASE-BASE % | BASE-BASE SSIM |
|---|---|---|---|---|---|---|---|---|
| CLDR | 16 | -0.086 (0.106) | 4.464 | 20.461 | 0.9041 | 4.805 | 22.740 | 0.8993 |
| FOGT | 16 | +0.122 (0.120) | 5.467 | 27.414 | 0.8985 | 4.805 | 22.740 | 0.8993 |
| LPVR | 16 | -0.058 (0.088) | 4.210 | 19.281 | 0.9134 | 4.805 | 22.740 | 0.8993 |
| SKYR | 16 | +0.097 (0.105) | 4.549 | 21.530 | 0.9038 | 4.805 | 22.740 | 0.8993 |

## VSMA arrival shots (same spawn after a load: VSMA on vs off; floor = off vs off, on vs on)

| stage | on-vs-off mad | on-vs-off % px | SSIM | off-vs-off mad | off-vs-off % px | SSIM | on-vs-on mad | on-vs-on % px | SSIM |
|---|---|---|---|---|---|---|---|---|---|
| 0 | 9.067 | 51.809 | 0.6925 | 9.067 | 51.922 | 0.6928 | 2.106 | 6.078 | 0.9701 |
| 1 | 6.505 | 36.254 | 0.8297 | 4.038 | 21.740 | 0.9379 | 6.160 | 31.015 | 0.8346 |
| 2 | 8.123 | 43.993 | 0.9122 | 5.816 | 29.813 | 0.9221 | 7.638 | 44.392 | 0.9225 |
| 3 | 32.155 | 82.501 | 0.2889 | 5.258 | 24.608 | 0.8499 | 31.973 | 82.213 | 0.2904 |

## Arrival windows (static, 3 s): frame time p99 / GPUBusy by VSMA state

- VSMA0: 8 windows, 868 frames, ft p99 38.47 ms, GPUBusy 26.922 ms
- VSMA1: 8 windows, 874 frames, ft p99 36.40 ms, GPUBusy 26.782 ms
