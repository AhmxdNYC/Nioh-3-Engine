# Gameplay gate, pc2 task 3639

The two Task3578 CPU windows are **INVALID** as gameplay benchmarks. Their CPU/FPS numbers do not establish a gameplay benefit. Raw files remain for diagnosis; both arm manifests, the paired manifest/reports, outer summary and CPU note now carry the correction. Original reports are archived privately under `C:/nioh3-re/task3639-gameplay-gate/invalidated-originals`.

The retained `task3578-cpu-ab/field-A.png` shows the Steam library foreground at approximately 10:08; `steam-B.png` shows Steam's cloud-sync/Play anyway dialog around 10:10. These screenshots are outside the measured spans and do not establish the exact state throughout either CPU window. The old runs had no pre-capture screenshots or character-control proof, so neither counts. Tour dwell/progress and a ticking frame clock alone are insufficient.

## Installed gate

`tools/testmatrix/fastab.ps1` now uses `gameplay_gate.py` before every measured window. It attempts one foreground recovery before the pre-check (the tour intentionally returns focus to Steam), saves screenshots, requires VirtualPad field/player/foreground/armed flags, rejects loading, pause and menu states, verifies advancing frames, and proves character control with a short stick pulse and measured position change. It reverses the pulse before measurement. An agent must **view all PNGs** and record a fresh review bound to the request nonce and screenshot hashes; absent/expired review aborts. This is agent verification, not a request for human permission.

The pre-check occurs before PresentMon starts. During each window the harness checks gameplay state approximately once per second; after it stops recording, a second screenshot review must pass. Each window records gate paths and validity. Failure aborts the run and writes an explicit INVALID event. PresentMon files are scoped to individual windows. The reporter refuses performance claims if any window lacks gameplay evidence or the session contains an aborted capture, including legacy manifests without the field.

To review a pending request only after inspecting its screenshots:

```powershell
py -I tools/testmatrix/gameplay_gate.py review --request PATH/NAME-pre.json --note 'Observed mission/HUD, responding character, no blocking menu/load/dialog'
```

Never pre-authorize a future screenshot or reuse a prior receipt. Review deadlines are 60 seconds; rejection ends the capture attempt and leaves the owning tour to restore the machine. Fresh runs/retries must pass the gate again. Future pc2 benchmarks must use this gate or equivalent verified screenshot/control checks; historical harnesses without it are not authorized benchmark evidence.

## Measured checks

Two real launches used the machine lock, fresh save/config backups, the existing diagnostic ASI and automatic Steam cloud-dialog handling. No performance A/B was attempted.

| Check | Measured result | Verdict |
|---|---|---|
| Title screen behind Steam | player=0, field=0, fg=0; screenshot retained | Rejected |
| Full fastab pre-gate, launch 1 | mission loaded but Steam foreground, fg=0; 0 measured windows | INVALID, recorder never started |
| Direct mission control check | 55.1104 game-coordinate units moved; pad reads 69286 -> 69914; all 3 screenshots visually checked | VALID |
| Full fastab pre-gate, launch 2 | mission loaded but Steam foreground, fg=0; 0 measured windows | INVALID, recorder never started |
| Equipment menu while mission loaded | visible Equipment menu, field=0 and menu_pause=1 despite player=1 | Rejected |
| Game exiting / unavailable pipe | bounded state-command timeout | Rejected |

The single pre-check focus recovery was added after the full-harness rejections; its `AppActivate` operation was exercised successfully against the second live game and produced fg=1. The accepted direct gate and full-harness rejection paths were tested live; no accepted end-to-end timing window is claimed. The second launch's directory suffix `positive` records its intended test, not its outcome.

Offline checks: report selftest including missing/false gameplay evidence and no-window rejection; complete one-pair fastab dry-run recovered the synthetic effect and printed `DryRun OK`; Python compile, PowerShell parsing and whitespace checks. Synthetic dry-run windows are explicitly marked in metadata and are not gameplay evidence.

Evidence root: `C:/nioh3-re/task3639-gameplay-gate`. Full sessions: `captures/pc/fastab/20261014-1044-task3639` and `20261014-1051-task3639-positive`. Screenshots, state/review JSON, console logs and incremental source patch are included in the delivery; saves, configs and game binaries are excluded.

After each launch, all **6 save/config files and 2 original mod files** were restored and hash checked. Final state: no Nioh3 process; `nioh3-pc2` lock free. The child tour releases the shared machine lease, so the outer cleanup's redundant second release may return HTTP 409 after successful restoration.
