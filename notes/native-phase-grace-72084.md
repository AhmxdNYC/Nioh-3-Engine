# Native next-phase grace and entry labels — #72084

Extends cxl-pc/multiboss-door 00f4d210 for pc2 evidence: R0C12 restarted its native boss phase 1.3 seconds after a first win had already been logged.

All native mission attempts now hold a win for 8 seconds after phase end. Every BR BossBattleMode begin/end notifies Training before ownership guards. A new phase in the same running mission cancels the held win and keeps the same report attempt; loss/quit cancel the hold. After the window, Training requires no live named actor and (when available) a completed generated roster before ending once with win. Missing resolved members continue through the existing native-wait/dynamic-fill path. Dynamic retry resets phase ownership along with the roster. Nonnative single-boss behavior is unchanged.

The generator now emits an observation-only union of every same-mission candidate for the catalogue names, including ambiguous variants on R0C12/R945F. It does not select those variants to spawn or add manual boss lists. Exact wrapper placement/spawn-key checks protect the live check. BR validates both current mission and Training token before preserving an old report owner, so a completed prior attempt cannot suppress a new attempt on the same row.

Entry logs use the received native fight-start event: an acting boss reports native boss live with request/warp timing; a teacher with no fight-start retains script-dormant. Door behavior/movers and mission progress writes remain unchanged.

Offline validation: production phase tests reproduce the 1.3 s next-phase start on both unresolved rows with exactly one history start and one delayed final win; 8 s timing, live-boss veto, loss/quit, resolved incomplete and nonnative passthrough pass. Entry tests execute both active and genuinely dormant label branches. 23 native reverse-order rosters, 23 fallback sequences, 6 data tests, BR phase/token ownership, 9 existing outcome cases and native return/ownership gates pass. Both add-ons rebuilt; actual output hashes BR e3822ef1c4b4b7fe0cd647f7277557a1244c0144ae98f890a7dcb136552e7fec, TR 0e0bea32f941a3644d97ff1e5f3ddc367899aa8adfaa7db50ea8874e291cacf7.

Live gate remains pc's task: R0034 complete sequence plus R0C12/R945F native phase transitions; verify one final history entry, no duplicate spawned phase, active labels and loss/quit. This seat did no game/install.
