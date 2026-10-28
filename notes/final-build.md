# Combined 1.7.86 release candidate (task42021)

Branch pc/final-build; base cxl-pc/release-185-40866 36a6f808.
Offline candidate only: not deployed, not installed, not pushed to main.

Included:
- Full ring meter, Small/Medium/Large, proportional Perfect Ki checks, close side legends (ring-side0c7a6f76).
- Y/Triangle close outside edits and matching controller hints (f30449ab).
- Input history/controller art/caps and fonts already in base (dbed70b7,2afb794e,8fab6567).
- Latest chain-aware hit-label generator and TSV from pc/hit-label-fix1c272daf, including measured accuracy table15454ee6 content.
- Older two-action observed overrides removed: newer isolated tests identify4E4E/4E55 as1 hit. Regenerated own-window fallback agrees with3 real action fixtures.
- Updater numeric downgrade refusal, hourly notice, Windows browser/Explorer and Wine URL+folder overlay without external launches.
- Save queue gate3f68800f: [SaveFiles] Enabled=0,RealRoot=0,Probe=0 and all paths blank. Helper ships but is not automatically configured or run.
- Website .url shortcut and complete Training action-records TSV (11,476 rows) included in zip AND installer/updater allowlists.

Excluded / limits:
- Live in-process title-screen save reload, named-slot picker/new relaunch work and live self-correcting hit recorder are unfinished and excluded.
- Hit predictions match24/25 measured Yoritomo moves, not universal proof.0x4E42 remains known gap.
- Save gate is experimental and default OFF; real saves untouched. Existing user INIs are preserved by installer, so previously opted-in settings are not silently reset.
- CamKiProbe is built but stored under diagnostics/ so it is not loaded by normal installation.
- Arena regeneration checker cannot run: tools/gen_summon_rows.py absent; shipped arenas retained. Available move checker explicitly reports this skip.
- Release lint has0 errors and4 warnings: undocumented/default keys, indirect key reads, existing provisional ALL-profile settings, missing MODLOG1.7.86 entry. This package uses base INI, not provisional ALL profile. Not a strict public release certification.

Validation:
-4 ASIs built;124 update assertions, both menu suites,106 title-gate checks,40 save-action checks.
-7 ring suites,16,949,251 production renderer assertions; padviz harness passes.
-74 save-helper tests;19 updater tests;11 hit-label tests;9 chain tests;2 corrected fallback tests.
-14 move native tests including3 real files pass; action records reader validates11,476 rows,11,474 unambiguous aliases and2 conflicts safely rejected.
- Full108-character generator reproduces committed TSV exactly;3,566 total hits by generic rule.
- Installer/package tests and zip hash validation run after packaging; see logs in worktree.

Install for acceptance: close game and use included Install-Mod.cmd/install-mod.sh; installer backs up saves and preserves existing INIs. Test first on copied saves. Save-manager helper requires separately configured Python and paths and remains OFF. No auto-load live swap implemented.

SHA256 artifacts:
- EngineFix: 02e8c98a848b6b64022866824f6e031a22723b3054a2d289c021691049553675 (mod/Nioh3EngineFix/bin/Nioh3EngineFix.asi)
- Training: 24f1804e9a8410017bd8c10b5343cc1dd37695722946f862d265c60d7f134866 (mod/Nioh3Training/Nioh3Training.asi)
- BossRematch: d43ea35bc41877f3360c51bf18ddf7e7429e1253805a707348ce7fe1a17a8931 (mod/Nioh3BossRematch/bin/Nioh3BossRematch.asi)
- CamKiProbe (optional diagnostics): 7de9053dfe36171bc1c68198741356c4334c463a48f0af76367e5fd12c323854 (mod/Nioh3CamKiProbe/bin/Nioh3CamKiProbe.asi)
- Updater: 8a6b57c81b6dd21ce7572e9b51493b288fca4879cad0eccc28d48ed5af6e53ee (updater-dist/Nioh3Updater.exe)
- Action records: e772852f067d318840d4a92450f7b8fb9111826a5cd0fcb2b52697ce3728a502 (mod/Nioh3Training/Nioh3Training_action_records.tsv)
