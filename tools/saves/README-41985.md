# Named save picker / switch and relaunch (41985)

Use this with the matching EF candidate, replacing the old helper pyz. Defaults remain Enabled=0 RealRoot=0. Start with synthetic/copied roots; pc owns native testing.

Run prepare_test.py as in README-41493.md, using NEW paths. It now creates three named synthetic slots: New Game (B), Yoritomo Practice (A), slot1 (A), with current=C. Merge its generated INI section into a preserved test configuration.

At the verified main menu, open Save files and pick a named row. Use Import now, refresh title when live import is enabled, or Switch after Quit (confirm twice) and quit normally. Switch and relaunch was removed by user request (#43064); no menu action requests automatic restart. Cancel still cancels pending work; later gameplay/menu changes alone do not.

Back up current save creates a fresh timestamped named snapshot after exit, without replacing an existing slot. To name snapshots offline: python saveslot.py --sandbox <workspace> backup "Yoritomo Practice". Names are ASCII letters/numbers, spaces, underscores and hyphens, max48, starting with a letter/number and with no trailing space. Existing slot1..slot8 folders remain supported. Catalog supports64 entries; malformed/incomplete/reparse entries are not listed. Helper hashes/account checks still run before switching.

Receipts distinguish a completed switch from failed/suppressed Steam relaunch. Do not repeat a switch just because Steam did not open; inspect request-TOKEN/receipt.json first. No in-process file swapping is enabled; the new native reload feasibility/plan is notes/save-names-41985.md in the repo.
