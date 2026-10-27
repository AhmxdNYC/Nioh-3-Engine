
2026-10-17 4:55 am EDT cx-pc2 #10803/#10870: use integ-move-toggle-on-hp0 cfc73e2, worktree nioh3-multiboss-10803. Full base + idle-default + unique menu IDs + cx16 startup fix. pc2 receives exact hashes/75s startup check in #11154. Multi-boss/despawn-all not implemented: native admission/release and late-spawn attribution need owner (#11121). Bounds runner already pushed 01f0a20 by parallel companion; needs retained dump. Consolidate duplicate sessions before further edits.
2026-10-17 4:59 AM EDT | #10870: requested integ/move-toggle-on-hp0 verified and ready; pc2 tests fresh pair in nioh3-integ-10870/mod (hashes in integration note).

#11037: menu collision fix 4f7b7c7 verified again offline (both sliders, idle, 12 moves PASS). Next pc2: live menu validation; cx-pc2 has no game ownership.

2026-10-27 4:08 pm EDT | cx-pc #40254: background HTTPS update checker + three-line hint implemented on pc/final-combined-4 d874f32b; Release EF build PASS, 64 mocked HTTP/version assertions + both menu suites PASS.
Next: final review/preview and report separate branch to pc; site/version.json prepared only, no deployment or game access.

2026-10-27 4:11 pm EDT | cx-pc #40254: update checker completed; Release EF 549f11cd, 70 offline assertions and both menu suites PASS; offline hint preview sent Discord.
Separate branch cx-pc/update-check-40254 from d874f32b; site/version.json prepared NOT deployed. Next pc integrate and publish manifest with a real release; details notes/update-check-40254.md. No game/installed files touched.
