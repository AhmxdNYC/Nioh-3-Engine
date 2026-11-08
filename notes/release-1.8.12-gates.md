# 1.8.12 candidate live gates (#83600)

Candidate source: cx-pc/release-1812-prep 790ef099. EF a1b32e13 (#6557), BR 44ca2f23 (#6558), TR a7c35109, CamKi a2f3e418, Updater e20d4632.
Setup d18ba1df (#6559), Linux pyz 905a01c3 (#6560), full b370c4e7 (#6561), combined c90b0918 (#6562), checksums #6563.
All 36 affected offline runners and testhub 5/5 pass. No game tested by integration worker.

Each device owner holds its game lock, checks human activity including controller, backs up saves, INIs and binaries, installs exact candidate bytes and restores everything afterward. Offline Steam is expected. Send progress in chunks at most five minutes. Do not run a second gate if another owner already has this candidate.
PC: combined/full candidate install and comprehensive features.
pc2: actual Setup.exe install with existing INIs preserved, then features.
Ally: actual Linux pyz install/loader setup, then features.

Required live evidence:
1. Version 1.8.12 in log/menu, Continue reaches playable world, no new crash.
2. Favorites: in ordinary list mark a non-first boss Right; row moves into favorites group and cursor remains on that same boss. Left unmarks and cursor follows it back. Repeat in Training/Scrolls list. Verify favorite persistence after restart. Do not change catalogue or save data.
3. Player tab: Home -> Player, LB/RB among Boss Matches/Training/Player; confirm selected game mode is unchanged by visiting Player and remembered Player subpage resumes.
4. Page reset: change an ordinary EF display setting, reset that page with A, verify shipped default immediately and after menu close/restart; an unrelated page's setting must remain unchanged. In Player or Soul tuning change a safe value then reset, verify shipped default, current mode and learned/equipped core identities retained. Restore original user settings after test. Do not use global reset or delete saves.
5. Soul queue: at least 5 valid alternating LEFT/DOWN second presses during old seal ghost exit; each second seal throws, no swallowed press, no premature old-seal despawn. Logs should identify queued/fired/spawn events. Exclude invalid first throws/hit reactions with reasons.
6. Fight now one known valid row (e.g. R1CBC), boss bar appears, no crash. Report both install/version and feature counts separately; do not call a boot-only test PASS.
7. Capture actual Player tab and favorite reorder/cursor screenshots/video. Send via universal notify.py media and upload one representative image plus logs through bridge for integration evidence. Visual fixture previews alone are not live proof.

Report task-thread RESULT to cx-pc with hashes, counts, gate verdict, evidence IDs, and save/INI/binary restoration. Any failed/missing feature is PARTIAL/FAIL, not overall PASS.
