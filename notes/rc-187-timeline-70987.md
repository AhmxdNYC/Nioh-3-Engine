# rc-187 fight timeline integration ? 70987 / reply70960

Replacement candidate branch: cx-pc/rc-187-timeline. The existing shared cx-pc/rc-187 history is not rewritten. Starting content is0ec56a75, reconstructed so the optional native-first merge remains LAST.

Required parent **3d09e4b9** contains save protection190a33b3, timelineeb97e527, shortened VRS label from0ec56a75 and the prior generator repairs. Native3647a5a9 is NOT an ancestor of that parent; it is the final merge's second parent. To drop native entry, use HEAD^1 or revert the final merge with -m1. Yoritomo sidebugs43603969 are absent; approved v2cdae6b9e remains included.

Production diff against0ec56a75 is exactly the requested timeline implementation: BR addon.h/bosslog.cpp/rematch.cpp, EF crashreport.cpp, Training fightlog.h/training.c. No camera production change, no version bump or preference change. Only merge conflicts: handoff append histories (both retained) and identical Training build scripts (kept compiler flags plus action-record copy/error handling). Previous generator repairs retained unchanged in the required parent.

All four build.bat runs exit0. EngineFix build-integrated VRS/HUD tests and Cam probe self-test passed. Additional offline commands all exit0: ExtraPerf (actual code/assembly/dump), SkipFx, item29 hooks, captured sc22 replay, rematch77 checks plus generator synchronization, transition cover/readers/anchors, native admission, pump143 lifecycle checks, Training15 move/HP/vitals harnesses, native entry, arena safety/static+runtime, Boss Match persistence/timer, timeline unit and recorded replay. Replay reproduces4 fights (2 wins/2 quits) with exact durations48803/62772/243505/101903ms and stable identities. Timeline C assertions explicitly enabled with -UNDEBUG.

One inherited pump harness initially failed before exercising code: its expected Rematch.Enable0 was obsolete (both source and ini already1), then it lacked the existing keepgear::Tick dependency. Updated those assertions/stub and added N3_MSVC_SETUP override;143 executable checks now pass. No production behavior was changed to satisfy it. All production source hashes match the built tree after reconstructing the final merge. Missing arena generator and3 raw boss-action fixtures remain explicitly skipped (0 real action-file cases); no false live claim. Existing compiler warnings remain.

Artifacts (full SHA256 / bridge):
- EF bccc59e822fa6b48a1438cd5e47c46b6c2c661708a4d7fcfe7a1eb12eb1679b7 / #5328
- BR 817518bbd7e70c782fb2d9039b2a40eb8430598234d884dbfdb5970e4b027d40 / #5326
- TR 1534df8b35244f96dbd6d7fb0661fc7749f8df130e8749192ba9673c3e28e0bb / #5329
- Cam e636b5cf243aa127ce389cefd692fb96ce4c3e42998380419fbc022869f8240e / #5327 (source unchanged; rebuilt artifact)

Logs/exit records and immutable built artifacts: Temp/timeline70987 in D:/nioh3-wt/rc-187-timeline. Existing timeline live PASS is the supplied pc2 #211 gate; this integrated combination was NOT run in game. Lead owns integration/live/release gates. No installation, launch, game access or publication. Main-PC supplied snapshot unchanged: EF85ebffdc BR4c219306 TR0e30328a Cam7482c03d; public1.8.6.
