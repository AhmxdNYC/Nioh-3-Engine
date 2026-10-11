# Native-admission quarantine candidate

2026-10-11. Isolated private candidate after `dcadf9d`; not installed or live tested.

The game-thread `NativeAttempt` copies requested mission/start/journey, source mission/retry, row, sequence, selected free slot, before/after queue evidence and return/load observations. Only its atomic phase is read from the UI. Possible admission is published before ById/Emaki can enqueue then throw. Exact mission/mode and any nonzero requested start must match before fight effects arm. A zero requested start accepts the native resolved start and stores it separately. Emaki additionally matches its carrier reference and 12-byte key; its native mode2/default-start behavior follows the existing `0x10dbed0.c` export evidence.

Exception, unreadable postcall queue, mismatch, abort/load timeout and active session read loss/known online-coop transition latch Quarantined. Cleanup, empty queue, a late load, Idle, successful restoration and elapsed time cannot erase it. Every new direct/menu/scroll entry refuses; warp, duel approach, gate and spawn apply/reapply cannot proceed. Existing owned-record cleanup remains separate. There is no acknowledge/reset API: an uncertain attempt requires a fresh process lifetime. Return is marked before its call and can be attempted once; a return exception retains uncertainty rather than granting retry permission.

Matched target arrival needs an observed valid loading frame followed by more than three consecutive seconds of valid/unpaused target mission/retry/player observations. Loading does not require a readable player. Unknown/mismatched/paused observations reset settling. This means operational route entry, not boss identity or arena readiness. Normal existing replay/scroll exit can clear a non-quarantined Active/Returning attempt after point restoration. Unknown/loading/paused exit observations cannot manufacture that completion. Normal operational exit remains a legacy contract, not the strict intended-return destination or native-consumer completion proof needed for automated training repetitions.

Validation: `test_native_attempt.py` compiles complete production direct/menu/scroll entry, cleanup, replay/scroll tracking, pump and fight-mutation bodies, plus production transaction helpers, under MSVC /W4 /WX. External native/memory/UI boundaries are fault stubs. 67 checks cover pre-call refusal, enqueue-then-error, postcopy failure, wrong explicit start/mission/mode, unchanged queue, scroll carrier/key mismatch, abort/timeout then delayed load, forced Idle, all-start blocking, cleanup success/failure, invalid/paused settling, normal default-start/scroll route, one return, return exception and latched session loss/online/co-op. Existing 83 read-gate checks (+7 integration assertions), 23 restoration (+3 admission), 53 snapshot, 143 lifecycle, 60 rematch and 94 sites pass. Private release build passes.

Limits before live pilot: no native cancellation/consumer proof; three-slot admission snapshots do not prove the four-record consumer rotation is empty; see the lead's `training-queue-consumer-proof.md`. The replay-menu route remains legacy and lacks an exact selected-request witness. Raw PlayMode classification retains known online values1/2 and does not establish semantics for other readable modes. Actor uniqueness/readiness, arbitrary arenas, reward suppression and strict saved-return destination validation remain unproved. This checkpoint does not authorize automated repetition or claim usable training. Root retains responsibility for exact installed build, save/guard restoration, pilot route selection and native acceptance gates.

## Independent review fixes (same isolated branch)

Three focused production-body regressions failed against `0e5aac4` while its original 67 checks still passed: manual return from UI Idle remained Returning forever; a GameMain read gap counted toward normal exit time; and failed scroll-map restoration cleared both attempt and map ownership. They now pass.

Standalone return retains the source mission/retry, waits for a valid load, then requires more than five consecutive valid/unpaused/player-present seconds of one stable different mission/retry destination. Only then can ordinary cleanup clear Returning. Destination change or read gap resets this timer. This is operational completion, not saved-return-target proof; a return without an observed load or to the same source mission remains pending. Replay GameMain read failure now also resets the normal exit timer.

Map recovery validates the current map allocation address/length, uses guarded reads and exact original/written bytes, and verifies the original bytes after restoration. Failed read/write/readback keeps ownership; foreign bytes/allocation mismatch latch conflict and cannot be forgotten when the old address/bytes reappear. Redirect snapshots/publishes ownership before its possibly partial write. All native start entries restore or refuse pending map recovery, and normal transaction completion waits for successful map restoration. A partial write whose bytes no longer exactly match either full original or full owned output remains conflicted; no speculative overwrite or public reset is introduced. These address/length checks are not native allocation-generation proof.

Current checks: 67 original transaction checks plus 4 standalone-return, 1 exit-gap and 15 map-recovery checks (87 total); 83 read gates plus 7 integration assertions; 23 restoration plus 3 admission; 53 observer; 143 lifecycle. The corrected pure rematch suite now expects foreign map bytes to be preserved, not overwritten. The last full private release build passed before the final stable-destination observer adjustment; a further full build is deferred at the lead's request during its independent local pilot. Production observer/body extraction still compiles with /W4 /WX. No live candidate deployment or native acceptance evidence.

## Integrated final private1.4.9 checkpoint

Candidate worktree integrates12a8507/6fe593b/2005b8e (donor0e5aac4/b4ade34/8820684).
Independent review reproduced a stale-mission duel-approach warp before fixing
FnVerify's mission guard and retry guards on warp/rewarp effects. DoWarp now
checks mission identity before crediting a fight. Readable unknown PlayMode
values also refused/quarantined: only raw0 has current native solo evidence;
raw1/2 remain online. This closes the earlier numeric-mode admission gap,
without claiming exhaustive native enum/lifetime proof.

Final candidate tests:8 verifier-identity checks;72 transaction checks plus
4 standalone-return,1 read-gap and15 map-recovery cases;86 read-gate cases
plus7 integration assertions;23 spawn-restoration plus3 entry guards;60 pure
rematch;143 lifecycle/startup;94 addon sites including shifted-image check.
Addon1.4.9 full build passes without compiler warnings; SHA256
50484e3b0e39c5621fb7b829122f92380db017fd87d64ed0e2f96a1acb3b9b07.
Core1.7.67 check_release --full passes with0 errors/3 existing metadata warnings,
including255 pattern sites and every listed harness. The initial worktree run
failed19 harnesses because cl/ml64 were absent from PATH; rerunning after the
existing portable MSVC setup fixed the environment and all harnesses passed.

No candidate deployment or live acceptance. The independent old-binary C72E
launch timed out before a game process appeared, with exact entry restoration;
it provides no native arena/transaction evidence. Queue-consumer identity,
actor lifetime/uniqueness, native reload/retry, and actual playable training
remain open. Default picture and public release are unchanged.
