# Full-build Training integration (#10870)

Merged move-toggle d0a5d59 (includes c132195) into full-build 3602481.
Preserves HP-0 1000 ms, Ki-break tracing/learned IDs, F9, gate, sliders,
and recorder. Fixed menu collisions: moves -29, idle -30, sliders -27/-28.

Both ASIs compile with Zig on PC2. Offline PASS: 202 entries (181 editable,
21 protected), startup/idle lifecycle, actual merged menu dispatch (two
sliders, idle and 12 Yoritomo moves), provider, hosted input 95, cover 73,
branch 48, dojo 42, passive reader 200. Provider fixture needed an empty
GateReqTick stub. See boss-move-menu-10183.md for build commands.
Compile tests/move_integration_harness.cpp with the Rematch Zig flags,
omitting -shared, then run the EXE for the new menu regression.

No game access. Startup is NOT proven: owner #10995 reports an exit during
BossBattleModeEnd installation; offline investigation is next. Multi-boss
still needs native ownership and late-arrival cleanup from study #10541.

Offline startup follow-up (#10995): the first local hook tests used host CPU
features and therefore did NOT reproduce the ASI generic-target failure.
Rerun with the exact ASI flag -target x86_64-windows-gnu exits 0xC0000005.
Companion fix f7dbd7e, integrated as 314f9f5, enables cx16 only on PatchAtomic.
Afterward the exact-target suites pass: 225 patch placements, 150 extra layouts
plus 10 unsupported-alignment refusals, exact BossBattleModeEnd thunk execution,
63 hook/core checks, and merged-menu dispatch. Live startup remains unproven.
Use -target x86_64-windows-gnu for every regression executable, matching ASIs.

Current artifacts (idle-default + atomic fix):
Training 03a90704b5cbffbf29d6d68fddb9fe561bec6d4c3a792750596cbc2013ee750b
Rematch 3332ff789f8c86f861b37293d5994108e865cdd599ac48810bf991426c190672
Paths under C:/Users/User/projects/nioh3-multiboss-10803/mod:
Nioh3Training/Nioh3Training.asi; Nioh3BossRematch/bin/Nioh3BossRematch.asi.
Both are owner-test candidates, not installed or live-verified by this companion.

## Independent requested-branch verification (#10870)

Branch `integ/move-toggle-on-hp0` reuses the completed integration merge
b583133 (parents 3602481 and d0a5d59) and subsequent menu/idle/atomic fixes.
Fetched origin/pc/hp0-stagger remains 3602481; both requested source tips
are ancestors. No source changes or writes to pc/hp0-stagger were needed.
Fresh PC2 Windows generic x64 Zig builds both pass (existing unused-variable
warning in Training). Offline rerun: 202 catalogue entries, 181 editable,
21 protected; idle lifecycle; 225+150 patch cases and 10 safe refusals;
merged menu/sliders/12 editable Yoritomo moves; provider 95 checks;
core 63/0 failures; cover/branch/dojo/passive readers 73/48/42/200 PASS.
No game touched; live startup, menu and Ki-break behavior remain owner checks.
Fresh binaries are under C:/Users/User/projects/nioh3-integ-10870/mod/:
- Nioh3Training/Nioh3Training.asi SHA256 3024f2a9fb0e3dbb385ab859adebe9b3da72e8e029be5d2c5027669a23337df1
- Nioh3BossRematch/bin/Nioh3BossRematch.asi SHA256 2e67c9d9a7079d1a606675bc2a3246f4822ba37940f54b0f830167a091ecae98
These are fresh rebuilds; earlier handoff hashes refer to the other worktree.
