# Ibaraki fixes OFF vs ON - result (2026-10-14), user-confirmed success
Scope per user (#4242): pc and ally only; pc2 not required.
| Device | OFF | ON | Change |
|---|---|---|---|
| pc (RX 9070), 4 arms OFF,ON,ON,OFF | 96.3 fps, 1% low 44.6, CPU 7.06 cores | 99.5 fps, 1% low 53.4, CPU 6.34 | +3.3% FPS, +20% 1% low, -10% CPU |
| ally (Linux/Proton) | 33.80 fps, CPU 68.4% | 32.77 fps, CPU 61.3% | -3% FPS (noise ~0.8), -7pp CPU |
Arm-to-arm noise on pc ~4%. Saves restored byte-exact; config.xml identical. Tool: tools/rematch/ibaraki_bench.ps1. Run it in foreground PowerShell.
