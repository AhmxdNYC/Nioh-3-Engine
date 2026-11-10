# Boss detection regression 85714

Owner: cxl-pc, scoped task85722; human85730 explicitly authorized the live test.
Unrelated fleet work remains paused.

## Baseline: Main PC, November 10, 2026, 11:21–11:24 am EST

Public v1.8.12 installed hashes: EngineFix a1b32e13, BossRematch 613b4d2f,
Training a7c35109. Source baseline54bd7da6. No candidate installed.

Used exclusive virtual pad for unattended input; ordinary game settings retained,
60 FPS cap, frame generation off. Entered native Yamagata Masakage R1CBC using
the BossRematch fight command. Runtime status: infight=1, fnow=pass,
mission=0x1CBC, route=native. Boss health bar visible.

Opened Training: **Active bosses (0 bosses)** despite the live boss.
Training log11:23:47: `CMD slotinfo: A chr 0, B chr 0, spawned 0`.
Screenshot sent directly to Discord with notify.py media.

Evidence: D:/nioh3-re/detection85714/baseline-active.png, copied module logs,
backup-hashes.json and restore.json. Game PID14580 closed after capture;
19/19 saved files (save data, ASIs and INIs) restored with zero hash mismatches.
PC game lock released; test-active marker removed. No candidate/ON result yet.

## Diagnosis and next step

Training source is identical between release31f9426a and54bd7da6; do not claim
v1.8.12 introduced this gap. Native entry accepts a live scripted actor without
registering a spawned slot. SessionActive/SlotChr and practice-stat gates consult
Training session/mod-spawned actors, so they miss this native boss.

Implement native actor observation without assigning mod ownership or applying
default stat writes. Preserve scripted waves and dormant lesson actors, and
invalidate observed slots on death/load/removal. Validate actual menu count and
an explicit reversible setting in game, plus no unintended native despawn or
default HP/Ki mutation. Restore all test changes afterward.
