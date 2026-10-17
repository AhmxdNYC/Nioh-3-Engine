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

Offline startup follow-up (#10995): atomic_patch_harness.cpp exercises the actual patcher on private executable memory: 150 patch layouts pass, 10 unsupported straddling heads refuse unchanged; exact BossBattleModeEnd head/alignment installs and executes. Existing rematch_harness also passes 63 checks. Compiler warning alone does not reproduce the crash. Owner subsequently reported live run contamination (#11035); obtain exclusive startup evidence before choosing a fix.
