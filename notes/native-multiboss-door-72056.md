# Native door multi-boss roster — task 72056 / thread 71960

Base 6949d971 (pc/door-rights-71926), plus fallback sequence 54228355 via 201a7bc64.
Branch cxl-pc/multiboss-door. Offline-only as assigned; main pc owns live R0034 gate.

Training arms the generated roster on native entry, observes exact same-mission placement/spawn-key actors, and counts confirmed deaths in an identity bitset. Live-before-dead is required; the applied-death flag or 4.5 seconds at zero HP confirms a death. Actor disappearance alone does not. Native-first reversed catalogue order works. After a nonfinal death it allows 8 seconds for the script's next member. Any live unfinished member keeps fallback off. If none arrives, Training requests the first remaining identity through its resource-loading path at the saved native arena pose, preserving the same report attempt. It never calls StartArena or warps the player between members.

Late-native dedupe on native rows is restricted to identities already dynamically replaced; future script members survive. Dynamic retry resets the identity mask to catalogue-first and uses the existing fallback retry. Loss/quit cancel pending spawns. Final kill closes one report row. BR retains the report owner/token across intermediate boss-mode end, next-phase start and native-clear callbacks. Training also guards the direct ConfirmWin correction path after partial quit/loss.

Door rights and BR mover code unchanged; no story/clear writes or hand-maintained boss list. 23 resolved generated rosters are handled. The 2 ambiguous multi-form rows R0C12/R945F remain entirely script-owned and refuse fallback as before.

Validation:
- generator --check and 5 generated-data tests pass.
- actual production native roster functions pass reverse-native identity order for all 23 rosters, native grace arrival, final-only win, dynamic second member, saved pose, HP0/finisher stability, disappearance, loss/quit, phase-row passthrough and restricted dedupe.
- production fallback helpers pass all 23 rosters and exact-handle/final-only/loss/quit/dedupe checks.
- BR production ownership functions pass repeated native phase, end/clear suppression, final result, single boss, quit and missing export/module scenarios; 9 existing result cases pass.
- native-entry and native-return tests pass (door-right ownership, exact-placement guards and all five exits).
- Windows BR and TR builds exit 0. Actual output file hashes: BR 65d316586303984e2adb58044d4a3e6ca68b22567e3fb20dfa496780fee336a3; TR 99600c9113dbe35d229b70a821010b7b79191c67403a74dca7a2139e54af3ae1.

Next: main pc R0034 native Jakotsu-baba first, kill, verify no win, wait 8 s then Enenra (or native arrival), kill -> exactly one win; retry/loss/quit. Runtime resource/AI/pose, script exits and save invariance remain unproven until that gate. No install/game launch performed by this seat.
