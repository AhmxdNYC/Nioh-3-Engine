# Player infinite-health entry guard — task 69802 / retest 69771

## Evidence and diagnosis
PC2 retested Training 2de742cd with EF 1f8d71b6 and BR a7857346. Its excerpt (bridge file 5187) contains two player-death episodes and four HP-zero/refill lines. The first death precedes the boss death sequence; the second reload happens without boss death. No SELFKILL guard fired. Move 4E3B is normal boss death animation, not evidence that a live boss selected a suicide attack. The v1 move guard is removed; repeat gap log deduplication remains.

The existing infinite-health interception wraps only the normal-hit call at RVA 329890. Static inspection of the game dump finds eight callers of shared HP subtraction 787268: AFF1D, AFFCE, 329890, 786ED8, E43B9A, 1FF44FF, 21F6540, 21FAECB. The native routine can set the death state at hp+10 and HP zero at hp+20 synchronously. The periodic RecoverFull refill runs too late to undo that death. This coverage gap predates Repeat Last Move. The excerpt does not identify which bypass caller caused the observed grabs; new caller-RVA logging is intended to establish that.

## Change
Install a verified-prologue detour on the common HP-subtraction entry when infinite health is enabled. Each call resolves the current player and compares its live stats+38 pointer to the HP object. Positive player damage becomes zero before native death processing. Native flags, return value, healing, zero damage, boss damage and toggle-OFF behavior pass through. The hook does not revive an already-dead actor or suppress fight results. The original normal-hit hook remains for damage scaling and boss handling.

The trampoline relocates 17 bytes comprising five whole instructions, with no relative branch or RIP-relative addressing. A mismatched prologue fails closed to the existing call-site protection. Logging prints the first 20 blocked shared-entry calls and every hundredth thereafter. This is a coverage fix for the HP-damage function, not a claim that every possible direct state writer is intercepted. It also covers scripted/environmental HP damage while infinite health is ON; the live retest should check loading and normal OFF behavior.

## Validation
- mod/Nioh3Training/build.bat: exit 0.
- run_move_toggle_available.py: exit 0, 15 executable harnesses pass. Existing generator prerequisite and three unavailable extracted boss-action fixtures are skipped; no real action fixture was tested.
- New player_hp_entry_test executes the production detour, installer and trampoline against an executable native-model stub. Four lethal bypass cases preserve HP before the death flag. OFF, boss kills, healing, zero damage, already-dead actors, current-player replacement, absent/unreadable identity, native arguments/return and prologue mismatch are covered.
- Offline PE disassembly confirms the exact 17-byte production prologue and safe instruction boundaries.
- No game process accessed; gameplay efficacy and the triggering caller remain unproven.

## PC2 retest
Use Practice Yoritomo with infinite health ON and Repeat OFF. Confirm INFHP ENTRY 787268 installed, then exercise the grab/combo that previously killed the player. Require no player HP-zero/death, fight loss or reload; capture INFHP ENTRY caller RVAs. Check a genuine boss kill still works and infinite health OFF permits player damage/death. Repeat-log deduplication remains from v1. Lead owns installation and the single-device game retest.

Candidate Training SHA256: 11e796c09bee841e1d275282ec013db7d5d19afecc8f7e2f2e4578125a89c3c7.
Main PC installed snapshot remains EF 0ae82ceb / TR 0e30328a / BR 4c219306 / Cam 7482c03d; this worker did not install the candidate.
