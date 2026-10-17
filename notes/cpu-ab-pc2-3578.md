# INVALID — superseded by gameplay audit 3639

INVALID: no fresh screenshot/control verification. Retained screenshot shows Steam foreground; exact gameplay state during measured windows unverified. Withdraw CPU/FPS inference.

The raw numerical observations below are retained only as historical telemetry. They do not count as a gameplay benchmark or evidence of CPU benefit.

# PC2 CPU A/B, task 3578

The completed OFF/ON comparison observed a small CPU reduction with effectively unchanged frame rate. Same machine, same Mirage Fortress tour spawn, same build, save and graphics; separate launches because these fixes are startup-only.

| Measured metric | OFF | ON |
|---|---:|---:|
| Process CPU, all 12 logical processors = 100% | 67.2776% | 64.6097% |
| Native CPU time | 485.093750 s | 468.890625 s |
| CPU measurement duration | 60.086104 s | 60.477366 s |
| Present rate, mod clock | 43.0643 FPS | 43.1538 FPS |
| Native clock CSV samples | 201 | 194 |

CPU delta: **-2.6679 percentage points, -3.9656% relative**. Frame-rate delta: +0.21%. Verdict: small CPU saving observed in this pair; no demonstrated frame-rate gain. One pair cannot establish repeatability or a confidence interval; `fastab_report.py` correctly reports `too few pairs` for statistical inference.

RD/M2S/J1 alone changed 0 -> 1. WG/LK/MW stayed 1; M1/full M2 stayed 0. A comparison of both complete runtime INIs verified exactly those three changed keys. ON logs verify RD 2 sites, M2S 2 sites and J1 8 sites. OFF logs contain none of their installation/ON proofs. The shared ASI is bridge424, SHA256 `32649c8a635f76ba6eaf0be282dea3ed333196f2654ccaf63838e4018c33390a`.

Both tours report stage 7292, mission 4019512, region 150 and identical spawn `282540,16054,130103`. Each used 30 seconds requested settle, 5 seconds warm-up and a requested 60-second measurement, within a 142-second spawn dwell. Both windows ended before the tour moved on. The tour-wide FPS summaries span different intervals and are not the measured-window FPS above.

Recorder repair: full PresentMon tracing reproduced the empty-output problem (3-byte BOM-only file). A separate, post-measurement 10-second probe with `--no_track_display --no_track_gpu --no_track_input` produced **371 actual frame rows**, 34,262 bytes. Added `-FrameOnlyPresentMon` for that tested argument set, explicit target PID after a game window exists, and stdout/stderr/command recording. Every real fastab window now also saves the native frame-clock samples, independent of ETW. The two CPU arms used the same original full-tracing settings plus the same native-clock sampler; the successful minimal-tracing probe occurred after the OFF measured window and is not included in the A/B numbers.

Other harness changes: `-FixedArm A/B` supports startup-only boot groups; CPU time is captured and reported; interrupted dwell windows are excluded; paths and lock identity resolve on pc2; tour child avoids duplicate PresentMon, launches the hidden Steam autoplay helper, exposes the obscuring Open Steam window, and allows up to 600 seconds for Steam startup. Failed tours now return failure from fastab.

Validation: native A/B complete; both tour verdicts `done`; report selftest PASS including known CPU delta and single-pair inference; PowerShell parse PASS; full fastab dry run PASS (two ABBA pairs, recovered the synthetic -0.4 ms effect). Final independent hashes match **all 6 current-entry save/config files and both original mod INI/ASI files**. No game remains and device lock `nioh3-pc2` is free. Early prelaunch/Steam timeout attempts contain no measured windows and are excluded.

Evidence:

- OFF: `captures/pc/fastab/20261014-1001-task3578-A`
- ON: `captures/pc/fastab/20261014-1009-task3578-B`
- Combined report and independent checks: `C:/nioh3-re/task3578-cpu-ab/paired/report.md`, `summary.json`, `finalize.py`
- Fresh outer backup: `C:/nioh3-re/task3578-cpu-ab/entry-save` and `entry-mod` (private, not uploaded).
- Additional execution limitations are recorded in `universal-optimizer/TECH_DEBT.md` under Task3578.
