# Half-rate with counters on pc (board #128), 2026-10-24
pc, RX 9070, 1440p XeSS, FG off, Yoritomo R7B35, 30 s warmup + 60 s capture. Order OFF/ON/ON/OFF per key.
- Artifact 1792 (1.7.61-hrstats): hangs at boot (no window), 2 launches. Same as 1780.
- Artifact 1816 (1.7.61-hrabi, camera fix): boots, 91.7 fps OFF (1 run).
- Artifact 1813 (1.7.73-drs-hrabi) used for the 12-run plan. Only 5 of 12 runs valid; the rest "no game window after 180 s" (flaky launch, logs of those runs are stale).
| run | key | arm | fps | 1% low | reused_calls |
| 007 | HRG | ON | 91.56 | 59.63 | 40299 (active) |
| 008 | HRG | OFF | 99.07 | 78.59 | 0 |
| 009 | HRC | OFF | 93.05 | 63.33 | 0 |
| 010 | HRC | ON | 85.37 | 59.03 | 0 -> NOT activated, window invalid |
| 001 | HRS | OFF | 86.86 | 54.83 | 0 |
HRS ON: no valid run. HRG: one ON vs one OFF, ON slower (-7.6%), not repeated, noise 3-4%+; no gain shown (pc2 saw +3%). HRC: never reused on pc. Verdict: inconclusive, no fps gain demonstrated on pc (CPU-bound). Raw: captures/test127/ (results-pc-hr.csv, logs/, wrapper128d.log).
