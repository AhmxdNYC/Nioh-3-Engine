# 1.8.9 release notes (candidate, not published)

Source: `cx-pc/release-189-prep` = 1.8.8 (`02b671be`) + Proceed-gate fix v3 (`cx-pc/fightnow-gate-80182` `c403678f`) + updater packaging (`cx-pc/updater-pkg-80103` `192cb53d`) + version bump and a log-text fix. EngineFix `MOD_VERSION` is "1.8.9" (only place).

## What players get
- After winning a Fight now (also multi-boss rosters) and standing at the Proceed gate, a new Fight now is accepted instead of "native replay/return still pending". A fight still in progress is refused.
- A Fight now chained from that gate keeps your original home and held saves, so Return to Title then Continue lands at home, not inside the boss mission.
- Return to Title idles Boss Rematch in 20 s even though the game keeps the old mission id.
- The updater shipped in packages is rebuilt from source (75c7251d), not the stale Oct 27 build.

## Packages (D:/nioh3-re/rc189/v1.8.9, bridge #6218-6227)
EF 9bfe7189, BR 8aeb68a2, TR f57937d1 and CamKiProbe 78302ba9 (unchanged source: the released 1.8.8 bytes), Updater 75c7251d, Setup.exe 6c26b07d, Linux pyz 83d2e627, full-install c91b3ad5, combined c972134f.

Offline: rematch harness 357 checks 0 failed; mod menu harness 95 checks pass; transition cover pass; BR python tests 11 of 12 (test_sites.py known failure). In game: v1/v2/v3 gate fixes checked on pc2 (#80139, #80180, #80240). This packaged build has not been run; no fresh-install/update test yet.
