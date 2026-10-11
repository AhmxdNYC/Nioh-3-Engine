# Spawn restoration failure reproduction

Source: private settings-menu-v4 ab4a5eb, src/rematch.cpp.
`tools/bench/repro_spawn_restore_loss.py` extracts its actual Spawn structure,
SpawnWrite and SpawnRestore into a /W4 /WX C++ harness with injected memory
access failures. No game or save files are accessed.

| Case | Writes attempted | Original bytes restored | Ownership cleared |
| --- | ---: | --- | --- |
| Control | 2 | yes | yes |
| First write fails | 1 | no | yes |
| Second write fails | 2 | no (mixed transform/flags) | yes |
| Read fails | 0 | no | yes |

All three defect cases reproduced. This is a diagnostic success, NOT a
restoration-invariant PASS. Production source remains unchanged and the
candidate is not deployed. This is separate from the fixed C72E external
guard-file deletion defect.

A one-line early return after failed SpawnWrite is insufficient: a partial
restore no longer equals the complete `mine` tuple, so the next attempt's
whole-tuple ownership check will discard it. A read failure also cannot
establish that the game replaced the record. SpawnApply's rollback/reapply
paths can lose ownership independently. New Fight calls SpawnRestore only
after MissionRequestById has admitted a native load; checking there is too
late to prevent the next request.

Next repair: explicit pending-restoration state with per-field ownership,
read-back confirmation, no write to replaced/foreign fields, and admission
blocked before any native request while restoration is unresolved. Cover
first/second write failure, unreadable identity, mixed partial state,
foreign replacement, retry, reapply and new-request rejection in actual
source tests. Keep native epoch/ABA uncertainty explicit; pointer equality
alone does not prove object lifetime. Live pilot remains gated.
