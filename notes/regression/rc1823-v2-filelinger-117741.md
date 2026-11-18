# 1.8.23 RC re-merge with FileLinger fix (TASK #117741), regression-pc/0, offline, no game
Branch regression-pc/rc-1823-v2-117741 = f1753dcb (#117719 merge: b48265e3 + reports/115 bd9166d3 + FileLinger a7f342a0)
+ merge origin/pc2/filelinger-toggle-117317 bcc5d2a2 (rvas.json +6, rvas_gen.h, catalog FileLingerMax/FileLingerFree). 0 conflicts.
EF candidate 96f42d9e = bridge #8485 (MOD_VERSION 1.8.23). Builds are not byte-reproducible: rebuilds differ in 13 PE stamp bytes (440fca91, 6ba24622, df96bb2c same source); run_settings_preservation.py rebuilds bin/. Build warnings: only the 2 old C4456 + the old raw-RVA note; the rvas.json warning is gone.

VERDICT: PASS (offline). Both #117719 fails cleared.
- gen_rvas --check: 258 entries, 258 used, 0 missing. check_fixed_rvas: EngineFix 0 Grva-not-in-rvas.
- mod/settings/test_catalog.py: ini-key coverage passes; it stops only on the expected MOD_VERSION 1.8.23 vs release 1.8.22 (same as base).
- no_hardcode_audit 49 = baseline PASS. build_diff_check vs b48265e3 and vs f1753dcb: 0 removed PASS.
- padmap + padguard harness ALL PASSED, test_padguard 13 sites OK, menu_pad_input 188/0, nav_repeat_axis 51 PASS,
  menu defaults PASS, generate_menu_defaults --check 391, settings preservation 64 native cases PASS,
  BossRematch hook heads 48/48, testhub OK.
- Not done: live test of EF 96f42d9e (no game from regression-pc). Not installed, not published.
