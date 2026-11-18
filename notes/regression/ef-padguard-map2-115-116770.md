# Regression review: reports/115 bd9166d3 (EF a8d37067, bridge #8446), PadListGuard 2nd pad-map guard (GH #115/#118)
2026-11-18, regression-pc/0, offline only, no game. TASK #116770.

VERDICT: PASS (offline). Not live-verified.

- Diff: padguard.cpp, padguard_cave.h, padmap_harness.cpp, rvas.json, rvas_gen.h (5 files, +274/-56).
- #96 guard unchanged: same kMap1 layout (0x800..0x960), same BuildMapMiss code (default L = kMap1), same Watch rate limit
  (condition rewritten by De Morgan, equivalent). MapHits() now sums both maps.
- One behavior change for #96: on v2.02 (knownBuild) the miss path must now equal kAtMissOrig exactly. Stricter, and the dump matches.
- Map2 block 0xA00..0xB70 has no overlap (static_assert). RUNTIME_FUNCTION table at 0x700 has room for 21 entries; 13 sites + 2 maps = 15.
- Callers checked in the dump: 433B16, 433E79, 2978718 use only [v+0..7]. The cold chunk 19A9265 jumps back into at2's frame.
  A caller not in the table, or a different rsp, still reaches the original throw (stock behavior).
- No hardcoding: no_hardcode_audit PASS (49 findings = baseline). gen_rvas --check: 252/252, 0 missing. EF RAW RVAs 8 -> 8;
  Grva uses 354 -> 363 (+9 = the new rvas.json entries).
- build_diff_check: PASS, 0 removed.
- Tests: run_padmap_harness ALL PASSED (at2 7/7 calls, 3 jumps into the miss path, cave2 105 B, block isolation).
  run_padguard_harness ALL PASSED. menu_pad_input 188/0 failed. nav_repeat_axis 51 PASS. testhub offline 5/5.
- Gap (existed before #96): padguard_harness's fake image never installs either map guard. It logs "#96/#115 not installed"
  in the default case, so the new Start() install loop has no end-to-end offline test. Live proof needed: the EF log line
  "pad map guard installed at +431ED5 (#115 ...)" next to the +4316F9 line.
- Not done: I did not rebuild EF or compare it to a8d37067.
