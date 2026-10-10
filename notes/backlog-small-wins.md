# Backlog: small wins (not prioritised)

User rule (2026-10-09 16:00): gains of about 1 fps or less can't be felt, so they are listed here and NOT prioritised.
Priority goes to anything with >= ~5 % fps potential, or a feelable smoothness / latency / loading improvement.
Small items run only when a machine would otherwise sit idle.

| item | where | measured | status |
|---|---|---|---|
| VCLOUD pfu/pfp cheaper cloud shader | Ally | +1.1 % fps (#803), picture PASS (#829) | keep candidate; pc2 neutral (async) |
| lodu MshLod#2 guarded fast path | Ally / pc2 | Ally 0.0 (#943), picture PASS | neutral, not shipped |
| mpc / mshlod culling shaders | pc2 | -0.01 ms (#906) | neutral, dropped |
| ESM1 ESM 2048->1024 | pc2 | 0.00 (#694) | dropped |
| POSTEFFECT_RESOLUTION STANDARD | pc2 | neutral (#736) | dropped |
| VSMA shadow-tile cap | PC/pc2/Ally | 0 tiles to cap (#853, #870) | parked (only big-camera-move scenes) |
| update-rate knobs LPVR/CLDR/SKYR/FOGT | PC/pc2 | neutral, FOGT visible | dropped |
| SkipDebugString | Ally | OutputDebugStringA 91/s -> 0.1/s (#667) | keep (cleanup, no fps) |
| TR timer resolution | PC | neutral (#704) | keep as safety, no perf |
| barrier batching retest (GPU-bound) | pc2 | <= 0.3 ms est. | backlog |
| MPCS meshlet tail skip | pc2 | est. 0.05-0.1 ms | backlog |
| PL boot pipeline library | PC | seconds at boot, est. | backlog |
| KL_Div (back-end segment hand-off every 16 draws, branch renderer-backend) | pc2 / Ally | pc2 +1.2..+1.3 % (SE 0.02-0.04, twice); Ally KLA1 #1381 inconclusive (scene got heavier mid-run, fps 29-37 swing; KL 4 -5.9 % +-1.5, KL 2 +1.0 % +-3.8) | backlog, default off; Ally no-harm needs a stable spot + more pairs before it could ship |
| FO frame overlap (branch frame-overlap, fo8) | pc2 / PC | pc2 +0.4 % (SE 0.02), PC +0.5 %; stable (17/17 self-test, 47k frames) | NO-GO as a feature (notes/renderer/frame-overlap.md 7.3), kept default off as infrastructure |
| GHDM | Ally / PC | fastab: +0.5 % Ally USER preset (#1178), +0.4 % PC no FG (#1063); old multi-launch +7.5 % Ally MAX / +11 % PC FG not re-verified | shipped; small win (re-check MAX + FG with fastab) |

Priority list (feelable): PCIe/no-ReBAR fix (CbvMirror v2), display latency (VrrPresent), weak-CPU gains,
stutter (PJH, needs a boss-fight route), cost-vs-looks only for items >= 1 ms.
