# 1.8.23 RC prep: pad-guard + FileLinger merge (TASK #117702), regression-pc/0, offline, no game
Branch regression-pc/rc-1823-padguard-linger-117702. Base pc1/98-dtsmooth b48265e3 (EF 1c8e1245 source, contains rc-1823 5f31438d and 1.8.22 3e6e730f).
Merges: origin/reports/115 bd9166d3 (also brings main's GH #100 SetColorSpace1 loop breaker a9ef9326, which is in the live-tested a8d37067),
then origin/pc2/filelinger-toggle-117317 a7f342a0. 0 conflicts. EF built: f4ca90b9 (MOD_VERSION 1.8.23).

VERDICT: FAIL (FileLinger part). Pad-guard part PASS.
- FAIL 1: fhlinger.cpp uses 6 fixed RVAs through Grva that are not in rvas.json (491B30 493050 4930B0 4930B5 751784 38F4E38).
  gen_rvas --check: 252 entries, 258 used, 6 missing. The build warns. It is safe on unknown builds (knownBuild gate + byte compare),
  but it breaks the every-RVA-in-rvas.json rule. Fix: py tools/gen_rvas.py DUMP on the FileLinger branch.
- FAIL 2: mod/settings/test_catalog.py: uncovered ini keys [Patches] FileLingerMax, FileLingerFree. The same failure is on a7f342a0 alone.
  (The RC base b48265e3 fails this test only on the expected MOD_VERSION 1.8.23 vs release 1.8.22.)
- PASS: no_hardcode_audit (49 = baseline), build_diff_check 0 removed, padmap + padguard harness ALL PASSED, menu_pad_input 188/0,
  nav_repeat_axis 51, menu defaults, settings preservation (64 native cases), BossRematch hook heads 48/48, testhub offline 5/5,
  generate_menu_defaults --check 391 values.
- Not done: live test of the merged EF (offline only, no game).
