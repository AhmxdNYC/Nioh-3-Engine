# CPU bundle J1/WG/LK/MW, Main PC ABBA (#95879, 2026-11-13 7:19-7:36 am EST)

Device: Main PC (RX 9070). Public 1.8.13, EF ba089162 (same DLL both arms), BR 8076cdd6, TR bcc4783e.
Settings: 1440p, FG OFF, FSR REN_RAT 50 (CPU-bound), MaxFps 240, VSync off. Spawn-point move+RB loop, 2 x 60 s per launch.
RD and M2S are not in the 1.8.13 source (keys ignored), so the shipped bundle is J1/WG/LK/MW (all 0 by default).
Tool: tools/testmatrix/pc_cpucut_series.ps1 -KeysA/-KeysB (new), captures/pc-cpubundle-95879/r1.

| Arm | fps (4 windows) | mean | 1% low mean | CPU cores mean |
|---|---|---|---|---|
| A OFF (J1/WG/LK/MW=0) | 100.8 90.3 96.9 92.4 | 95.1 | 61.1 | 8.28 |
| B ON  (J1/WG/LK/MW=1) | 98.6 101.0 97.7 91.6 | 97.2 | 60.2 | 8.25 |

ON vs OFF: fps +2.2 %, 1 % low -1.5 %, CPU -0.4 %. Window-to-window spread is ~10 fps inside one arm, so all three are noise.
The old Oct claim (1 % low +20 %, CPU -10 %) does NOT reproduce on 1.8.13. MW logged "applied (2 sites)".
Verdict: no ship, keep default OFF; per-switch isolation not worth game time on Main PC (bundle total = 0).
