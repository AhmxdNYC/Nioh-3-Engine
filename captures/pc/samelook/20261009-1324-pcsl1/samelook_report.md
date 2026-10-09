# Same-look A/B report: 20261009-1324-pcsl1

## VSMA while moving (stick runs, VSMA live, ABBA per dwell)

| cond | runs | frames | ft p50 | ft p99 | ft p99.9 | ft max | GPUBusy mean | GPUBusy p99 |
|---|---|---|---|---|---|---|---|---|
| VSMA0 | 32 | 27798 | 6.27 | 8.73 | 10.02 | 11.38 | 5.373 | 10.68 |
| VSMA1 | 32 | 27307 | 6.33 | 8.68 | 9.85 | 13.91 | 5.392 | 10.72 |

Paired per dwell (on - off, 16 dwells): frame time p99 -0.034 ms (sd 0.215, se 0.054), GPUBusy mean +0.012 ms (sd 0.030, se 0.007)

Mod lines (first / last):

```
[13:25:21.888] VSMA: VSM waiting-tile amortisation installed, switch off (K 2, front 2 x budget, first-visit draw off)
[13:25:21.970] RCO: update-rate knobs installed: LPVR off (LPVUpdateFrequency = 4), SKYR off (SkylightAmbientUpdateFreq = 4.000), FOGT off (VolumetricFogTemporalFactor = 0.500), CLDR off (VolumetricCloudReprojectionBlendWeightCurrentFrame = 0.500)
[13:25:24.794] RCO: engine values in S at the first frame: LPVUpdateFrequency 1, SkylightAmbientUpdateFreq 1.000, VolumetricFogTemporalFactor 0.000, VolumetricCloudReprojectionBlendWeightCurrentFrame 0.000; offset check: VolumetricFogResolutionX (S+13DC) 160, VolumetricFogResolutionZ (S+13E0) 64, SSAORayNum (S+12F4) 7
[13:25:52.147] stats/s VSMA (switch off): VsmClipmapRender 0.22/frame (36.9/s; stock: off 36.9, throttle param 0.0, other thread 0.0), tile visits 0.0/s; batch tiles drawn 0.0/s (marked 0.0, valid 0.0, front 0.0, phase 0.0, first 0.0) skipped 0.0/s = 0.0 % of the batch tiles | stats/s RCO: frames 211.5/s, LPVR off written 0 refilled 0, SKYR off written 0 refilled 0, FOGT off written 0 refilled 0, CLDR off written 0 refilled 0
...
[13:53:58.684] stats/s VSMA (switch ON): VsmClipmapRender 0.50/frame (78.1/s; stock: off 0.0, throttle param 0.0, other thread 0.0), tile visits 3.1/s; batch tiles drawn 23.6/s (marked 23.6, valid 0.0, front 0.0, phase 0.0, first 0.0) skipped 0.0/s = 0.0 % of the batch tiles | stats/s RCO: frames 206.3/s, LPVR off written 13194 refilled 13194, SKYR off written 12957 refilled 12957, FOGT ON written 13656 refilled 10695, CLDR off written 14247 refilled 14247
[13:54:28.806] stats/s VSMA (switch ON): VsmClipmapRender 0.50/frame (77.6/s; stock: off 15.2, throttle param 0.0, other thread 0.0), tile visits 4.9/s; batch tiles drawn 22.2/s (marked 22.2, valid 0.0, front 0.0, phase 0.0, first 0.0) skipped 0.0/s = 0.0 % of the batch tiles | stats/s RCO: frames 201.4/s, LPVR off written 14232 refilled 14232, SKYR off written 13960 refilled 13960, FOGT off written 14094 refilled 11134, CLDR off written 14247 refilled 14247
[13:54:58.895] stats/s VSMA (switch ON): VsmClipmapRender 0.50/frame (88.1/s; stock: off 17.2, throttle param 0.0, other thread 0.0), tile visits 15.9/s; batch tiles drawn 43.8/s (marked 43.8, valid 0.0, front 0.0, phase 0.0, first 0.0) skipped 0.0/s = 0.0 % of the batch tiles | stats/s RCO: frames 199.4/s, LPVR off written 14232 refilled 14232, SKYR off written 13960 refilled 13960, FOGT off written 14094 refilled 11134, CLDR off written 14247 refilled 14247
```

## Update-rate knobs (static spawn view, BASE brackets each dwell)

| knob | dwells | GPUBusy delta ms (mean, se) | shot mad | % px > 8 | SSIM | BASE-BASE mad | BASE-BASE % | BASE-BASE SSIM |
|---|---|---|---|---|---|---|---|---|
| CLDR | 16 | +0.036 (0.025) | 3.802 | 14.887 | 0.9186 | 4.097 | 16.372 | 0.9055 |
| FOGT | 16 | +0.023 (0.005) | 5.116 | 23.073 | 0.8992 | 4.097 | 16.372 | 0.9055 |
| LPVR | 16 | +0.010 (0.006) | 4.008 | 16.152 | 0.9140 | 4.097 | 16.372 | 0.9055 |
| SKYR | 16 | +0.016 (0.004) | 4.476 | 18.638 | 0.8935 | 4.097 | 16.372 | 0.9055 |

## VSMA arrival shots (same spawn after a load: VSMA on vs off; floor = off vs off, on vs on)

| stage | on-vs-off mad | on-vs-off % px | SSIM | off-vs-off mad | off-vs-off % px | SSIM | on-vs-on mad | on-vs-on % px | SSIM |
|---|---|---|---|---|---|---|---|---|---|
| 0 | 9.078 | 49.733 | 0.7022 | 9.127 | 50.104 | 0.7016 | 3.138 | 12.595 | 0.9105 |
| 1 | 6.022 | 30.302 | 0.8364 | 5.760 | 28.731 | 0.8518 | 5.902 | 27.804 | 0.8218 |
| 2 | 4.687 | 21.355 | 0.9626 | 5.419 | 27.890 | 0.9623 | 4.187 | 16.148 | 0.9818 |
| 3 | 33.056 | 83.213 | 0.2933 | 6.221 | 25.632 | 0.7896 | 34.755 | 84.507 | 0.3030 |

## Arrival windows (static, 3 s): frame time p99 / GPUBusy by VSMA state

- VSMA0: 8 windows, 3808 frames, ft p99 8.33 ms, GPUBusy 5.283 ms
- VSMA1: 8 windows, 3865 frames, ft p99 8.25 ms, GPUBusy 5.263 ms
