# Menu move and shipped-default audit (#37423, cx-pc, offline)

## Menu (Layout 0, EngineFix)
- Top level order: Boss Matches, Practice, **Gameplay / Overlays**, Settings (renamed from Display Overlays). The separate "Load / Auto" first-screen page is gone.
- Gameplay / Overlays now holds: FPS counter rows, Controller display (on, X, Y, size, opacity, pressed colour), **Controller button style** (Menu.ButtonStyle), **Auto continue load** (the loading-screen auto-click, General.AutoContinueLoad), frame meter, Ki window. Settings no longer lists the auto-click.
- Negative positions: Overlay OffsetX/OffsetY now -1900..1900 / -1060..1060 (ini parse clamp, menu nudge clamp, catalog), Controller display X/Y -100..100 (percent of free screen; negative moves it past the top-left default). The frame meter has no X/Y offset key (only [Practice] FrameMeterY percent, where -1 means "auto above the HUD"), so it was not changed.
- Another agent's #37433 change (status readouts and timing diagnostics also on this page) is in the same tree and the harness passes with it.

## Shipped defaults flipped to ON (ini + catalog)
Source of truth is the benchmark arm `captures/final-full/pc/plan/pc/arm2.ini` (pure-perf arm: pc stock 75.8 -> 98.5 fps, pc2 41.7 -> 46.9, same keys), cross-checked with `notes/default-on-candidates-20261023.md` group A (no visual change, no veto):
Patches B, PP, FPC, AABB, M2, J1, S1, IK, ED, WG, DQ, P1E7, LK, MA, BPO, W1B, RPA, CP, SPF, PT1, RWb, PCM, GL1, LW, and Debug.SkipDebugString (25 keys).
Only the shipped ini and the menu catalog changed. The C++ Config member defaults still read 0 when a key is missing from an existing user ini, so existing installs keep what they have.

## Not flipped (and why)
- RD, M2S, MW: in the 10-13 combined bundle, but not in the benchmarked pure-perf arm; the bundle note says no proven gameplay result, MW is "performance unvalidated". M1/M2 stay off per the bundle (M2 is the *locked atomic loads* key in the pure-perf arm and IS flipped; the bundle's "M2=0" refers to full-M2 safety gaps, check this before shipping).
- W6, GF, MQ, EP: arm2 sets 1, shipped value is -1 (auto: on under Proton, Windows gain ~0). Left at auto.
- MCE: in arm2 but the key does not exist in this tree (pc/newopt-20261023 not merged).
- Installed live ini has J1/WG/LK/MW = 0 today, so it does not equal the benchmark config either.

## Tests
menu_harness + Boss UI harness PASS. settings preservation harness build fails here (its build.bat call, environment). mod/settings/test_catalog.py already fails on keys outside this change (Patches.MW, RD, M2S, Profiler.NodeTimer, Debug.DxgiDiag*).
