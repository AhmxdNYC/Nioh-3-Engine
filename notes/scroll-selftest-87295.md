# Scroll SelfTest entry and first-wave result

Task #87295, base origin/main 448b7a7c. Offline only.

ClearedNow refuses mission ID zero before reading the clear database or comparing the current mission, with an explicit no-replay-ID reason. Valid unbeaten mission protection is unchanged. SelfTest routes E0 rows through kReqEmaki -> ExecEmaki, keeping native request/clear-guard/recovery checks. Replay rows retain their clear check and kReqFight route. `SelfTest=arena` now includes scroll rows as well as arena-ready replay rows.

Scroll PASS requires the requested row's accepted scroll, kEIn, readable current scroll mission, player present and not loading/online/guest, waves enabled, permitted native attempt, and the per-request seenFight latch (set by native wave state 3). Stale replay g_fn PASS/FAIL cannot pass a scroll or suppress its request-refusal timeout. PASS waits the existing four seconds, posts native exit, and advances after idle. No wave kills or whole-scroll clear are required or claimed.

Validation: selftest_scroll_offline.py compiles actual ClearedNow and SelfTestTick. PASS for sequential E01/E03/E05/E06, first-wave wait, ten negative evidence/admission gates, rejected request with stale g_fn, timeout/exit, replay behavior, zero-id and active-unbeaten guards. The normal MSVC BR build passed. Existing unbeaten_menu_test.py stops at an Items text assertion; confirmed the identical assertion fails on unchanged base Items, before its compiled tests. No unrelated test changes.

Built BR SHA256: e1f10940c4b3a1f3b7947cae875167b866315185eaae6a4a16f8021711113286
Artifact: Temp/scroll87295-BR.asi in main workspace; uploaded separately for pc review. Not installed, no game or live test dispatch.

Next owner pc: review and coordinate E01/E03/E05/E06 SelfTest sweep. Require first-wave PASS then home per row, ending pass4/fail0/skip0; verify a normal replay row still works. Offline mocks are not a live pass. Machine supplied installed set: public v1.8.12 BR613b4d2f / EFa1b32e13 / TRa7c35109 / CamKia2f3e418.
