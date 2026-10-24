# Final 3-arm bench on pc (board #130), 2026-10-24
pc, Ryzen 5 7600X + RX 9070, 1440p XeSS, FG off, Yoritomo R7B35 fight-now, PresentMon 60 s window, one launch per run, order 1,2,3,3,2,1.
Build: 1813 (1.7.73-drs-hrabi, hr keys 0). NOT the 1860 (1.7.61) build pc2/ally use: 1860 never opened a window on pc (2 launches). Arm inis from captures/final-bench/pc-plan (older key set; unknown keys ignored).
| arm | runs avg fps | mean fps | 1% low | hitches >2x median (worst) |
|---|---|---|---|---|
| ARM1 stock-like | 75.61, 76.06 | 75.8 | 58.5 / 62.5 | 1 (34 ms) / 0 |
| ARM2 pure-perf | 102.26, 94.65 | 98.5 | 75.6 / 73.7 | 0 / 0 |
| ARM3 full | 95.52, 102.05 | 98.8 | 74.6 / 72.1 | 0 / 0 |
Verdict: both mod arms +30% over stock-like (75.8 -> ~98.6 fps); FULL is NOT better than pure-perf (98.8 vs 98.5, run spread 7%). Flag: the big options (half-rate, DRS, Ultra35) gave nothing on pc, which is CPU-bound.
Caveats: capture window was 60 s (plan columns for 15/40 s not honoured by the runner), so no time halving beyond one launch per ~4 min. Sensor log (LHM) died after ~7 min, covers runs 1-2 only: CPU pkg 89 W avg / 151 max, Tctl 93.6 C avg (max 97), GPU 160 W avg, 57 C. No per-arm temps. Raw: captures/final-full/pc/ (results.csv, arms.csv, sensors.csv), PresentMon CSVs in captures/audit-screen/run-pc-20261024-033116.
