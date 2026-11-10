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

## First candidate live gate (November 10, 11:30�11:35 am EST)
Main PC public1.8.12 with Training76a9ec33 substituted,60 FPS cap/FG OFF: R1CBC native observed as slot A chr307259, correct Yamagata name, Active bosses1 versus baseline0. Explicit Stand still disabled AI; Fight normally released it. HP90 applied by native setter, readback12163/13515. Despawn-all retained native; return/load cleared slot to0. Both HP/Ki infinite defaults remained OFF.19/19 backed-up saves/INIs/ASIs restored with zero mismatches; game closed and lock released. Screenshot hp90.png sent to Discord85770; evidence D:/nioh3-re/detection85714-on1.
Final cleanup candidate dcad95e5 adds validated release of explicitly-held native AI when HUD observation disappears. Production-body lifecycle/ownership regression and17 available move/practice runners pass; native entry/phase/persistence checks pass. Final binary live gate remains next, including owned summon compatibility. Missing arena generator and zero real auto-move fixtures limit the broad offline suite; no claim of complete runner coverage.

## Mixed-boss gate found and corrected next
Final-cleanup dcad95e5 re-proved native count1, idle and HP90 controls. Adding an owned Yamagata in slot B exposed a second issue: its HUD gauge replaced the native gauge, so observer dropped still-live native A (A0/B307259/spawned1). This candidate is NOT final. Preserve admitted native identity through HUD changes; require HUD evidence only for first admission. Next candidate must show A+B then remove-owned retains A then return clears all. Test ended11:38am EST;19/19 files restored, game closed.

## Final candidate live gate PASS (November 10, 11:39�11:43 am EST)
TR SHA2567346c4c8645d20398d50a8444f71c12d7deb6f86db4c137044aac4d9f30dadde, source8c702366. Main PC, public1.8.12 BR613b4d2f/EFa1b32e13/CamKia2f3e418,60FPS cap/FG OFF. OFF publicTRa7c35109 count0; ON candidate native Yamagata count1, adding owned summon count2 (nativeA/ownedB both chr307259), menu Remove all deleted only ownedB leaving nativeA count1 with explicit Stand still retained. Explicit Fight normally restored native AI; HP90 readback12163/13515. Return/load cleared A/B to0. Installed candidate hash verified.19/19 save/INI/ASI files restored with zero mismatches; game17356 closed, guard done, lock and marker released. No game crash observed. Functional validation MainPC PASS; PC2/Ally not yet tested, no performance verdict/publication.
Evidence D:/nioh3-re/detection85714-on3: mixed.png/list.png/final-hp.png, logs, hashes and restore.json. Candidate bridge6672. Existing native Reset boss position has no borrowed origin, and summoned HUD removal can hide the native gauge while observer and controls remain valid; follow-up debt recorded separately.

## Board364 native Moves live validation (November10,12:23�12:26pm EST)
Same final TR7346c4c8/public1.8.12 companions, MainPC60FPS cap/FG OFF. Native R1CBC Training move category populated13 attack entries; labels were Anim ? / hexadecimal IDs, NOT verified move names. UI A on first0x4E5D enabled repeat at12:25:44. Actual native actor ACTLOG transitions to0x4E5D at12:25:46,51,55,12:26:00 matched repeat counters1�4, action duration~3.7s and gap1000ms; each cycle logged hit278. Explicit repeat OFF sent before return. Moves screenshot sent Discord.19/19 saved files restored; game8312 closed, lock/marker released. Evidence D:/nioh3-re/moves364-native. Separate Train-here gate still next; no all-options claim.

## Board364 Train-here validation (November10,12:27�12:31pm EST)
MainPC60FPS cap/FG OFF, same candidateTR7346c4c8 and public1.8.12 companions. Entered actual Train here UI and selected Yamagata: Active bosses1, HP54345. Moves page populated (12 visible entries in 1 hit category), labels Anim ? / inferred 1Hit / IDs remain unverified. UI selected0x4E7C. Live DRILL forced counter reached9 by12:30:44 with actual action now0x4E7C; ACTLOG matched native actor00000266C4A20C10. Later repeat hooks matched three further selections, including12:31:08, chr307259. Configured gap1000ms; distance walk-up pauses lengthen intervals, so not a fixed-rate claim. Player practice infinite-health hook intercepted1022 damage. Repeat OFF sent before harness termination.
Both requested paths now show selectable moves and actual selected-action execution. This does NOT validate every move, category, readable move name, or every Training option. Screenshots from both modes sent to Discord. Practice game1816 closed12:31:23;19/19 original save/INI/ASI hashes restored, zero mismatches; guard complete. Evidence D:/nioh3-re/moves364-practice and moves364-native. MainPC functional coverage only; PC2/Ally boards362/363 remain pending; no publication/global resume.

## Native metadata follow-up85900 (November10,12:43pm EST; no game action)
Candidate TR2f439ec13aa3e99454eceed27796c7a0861c86335f202fe7bcf5c18af84d0794 builds on latest release1.8.12 plus detection8c702366. AmTick/AbTick previously scanned only owned spawn actors: native discovery never ran despite the new roster. Added bounded read-only metadata subjects: existing owned indices preserved, admitted/validated native slots appended, duplicate actor pointers excluded. Both collectors and composed-human lookup use the expanded capacity. Does not insert borrowed actors into ownership or change parser, UI labels, files or control defaults. Native observation precedes both metadata ticks on existing game thread; no menu/render-thread discovery added.
Offline: production native lifecycle/metadata selector test PASS (borrowed scan subject, owned scan subject, duplicate suppression, invalid index, mission invalidation and existing ownership/stat tests). Release build PASS (two existing unused-symbol warnings). Autobind real table364 files, union159/fragments212 with0 wrong matches; auto_moves runner has0 extracted real fixtures, so not counted as parser coverage. No game launched/installed/touched per task85900.
Required live gate: cold fresh process with public1.8.12 companions and new TR; native Yamagata R1CBC FIRST (before any summon/Train-here can populate cache), wait up to30s; confirm AUTOMOVES actor discovery/final catalogue and hit-count groups, expected practice reference132 moves/23 known counts, screenshot1-hit rows and selected attack action transition. Then separate cold-process Train-here same boss, verify metadata/groups/execution; mixed native+owned lifecycle1/2/1/0 and no implicit native stats/ownership mutation. Back up saves/INIs/ASIs, guard/controller check, restore hashes. Candidate not live-validated/released; prior7346c4c8 tests do not prove this binary.
