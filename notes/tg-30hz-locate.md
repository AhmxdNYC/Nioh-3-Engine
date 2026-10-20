# TG = the "30 Hz game logic" optimization (locate only, 2026-10-20, #14805/#14780)
- Name: TG, logic tick governor. INI `[Patches] TG` (0 off / 1 on), `[Tuning] TG_Floor=30`, `TG_RatioPct=125`. Source mod/Nioh3EngineFix/src/dllmain.cpp; docs mod/Nioh3EngineFix/README.md (1.6.0 section), INI lines ~237 and ~625.
- In every build since engine-fix 1.6.0, so installed 1.7.73 has the code. Default TG = 0 (opt-in); also live-switchable in the mod menu when VRR is installed.
- Needs VRR + L1 (Proton: L1OnWine=1) + overlay Present hook (mod menu on by default); else logs "TG: refused".
- Numbers: design estimate ticks 44.6 -> ~30 = ~-33% logic work (~1-1.4 cores on Ally), UNTESTED. Measured ABBA fps gain only +2.6% / +3.1% (CPU1/CPU2), notes/cpu-bound-midrange.md. The 26-27% figure was not found as a measurement; likely the estimate.
- Tradeoff: sim step 1/30 s, taps must be held ~33 ms. Offline only.
- CORRECTION: this PC's installed INI (Steam/steamapps/common/Nioh3/Nioh3EngineFix.ini) already has TG=1, L1=1, VRR=1, TG_Floor=30, so it is ON here (default for fresh installs is 0).
