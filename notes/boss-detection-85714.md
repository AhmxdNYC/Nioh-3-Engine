# Boss detection regression 85714

Owner: cxl-pc, scoped task85722; human85730 explicitly authorized the live test.
Unrelated fleet work remains paused.

## Baseline: Main PC, November 10, 2026, 11:21â€“11:24 am EST

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

## First candidate live gate (November 10, 11:30–11:35 am EST)
Main PC public1.8.12 with Training76a9ec33 substituted,60 FPS cap/FG OFF: R1CBC native observed as slot A chr307259, correct Yamagata name, Active bosses1 versus baseline0. Explicit Stand still disabled AI; Fight normally released it. HP90 applied by native setter, readback12163/13515. Despawn-all retained native; return/load cleared slot to0. Both HP/Ki infinite defaults remained OFF.19/19 backed-up saves/INIs/ASIs restored with zero mismatches; game closed and lock released. Screenshot hp90.png sent to Discord85770; evidence D:/nioh3-re/detection85714-on1.
Final cleanup candidate dcad95e5 adds validated release of explicitly-held native AI when HUD observation disappears. Production-body lifecycle/ownership regression and17 available move/practice runners pass; native entry/phase/persistence checks pass. Final binary live gate remains next, including owned summon compatibility. Missing arena generator and zero real auto-move fixtures limit the broad offline suite; no claim of complete runner coverage.

## Mixed-boss gate found and corrected next
Final-cleanup dcad95e5 re-proved native count1, idle and HP90 controls. Adding an owned Yamagata in slot B exposed a second issue: its HUD gauge replaced the native gauge, so observer dropped still-live native A (A0/B307259/spawned1). This candidate is NOT final. Preserve admitted native identity through HUD changes; require HUD evidence only for first admission. Next candidate must show A+B then remove-owned retains A then return clears all. Test ended11:38am EST;19/19 files restored, game closed.

## Final candidate live gate PASS (November 10, 11:39–11:43 am EST)
TR SHA2567346c4c8645d20398d50a8444f71c12d7deb6f86db4c137044aac4d9f30dadde, source8c702366. Main PC, public1.8.12 BR613b4d2f/EFa1b32e13/CamKia2f3e418,60FPS cap/FG OFF. OFF publicTRa7c35109 count0; ON candidate native Yamagata count1, adding owned summon count2 (nativeA/ownedB both chr307259), menu Remove all deleted only ownedB leaving nativeA count1 with explicit Stand still retained. Explicit Fight normally restored native AI; HP90 readback12163/13515. Return/load cleared A/B to0. Installed candidate hash verified.19/19 save/INI/ASI files restored with zero mismatches; game17356 closed, guard done, lock and marker released. No game crash observed. Functional validation MainPC PASS; PC2/Ally not yet tested, no performance verdict/publication.
Evidence D:/nioh3-re/detection85714-on3: mixed.png/list.png/final-hp.png, logs, hashes and restore.json. Candidate bridge6672. Existing native Reset boss position has no borrowed origin, and summoned HUD removal can hide the native gauge while observer and controls remain valid; follow-up debt recorded separately.
