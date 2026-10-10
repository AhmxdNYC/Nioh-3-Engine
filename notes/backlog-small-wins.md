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
| GHDM | Ally / PC | fastab: +0.5 % Ally USER preset (#1178), +0.4 % PC no FG (#1063); old multi-launch +7.5 % Ally MAX / +11 % PC FG not re-verified | shipped; small win (re-check MAX + FG with fastab) |

Priority list (feelable): PCIe/no-ReBAR fix (CbvMirror v2), display latency (VrrPresent), weak-CPU gains,
stutter (PJH, needs a boss-fight route), cost-vs-looks only for items >= 1 ms.
