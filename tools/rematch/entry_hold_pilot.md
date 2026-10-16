# Combined first-Yamagata Summon pilot

Preparation only. The pinned c6d9abe pair combines the cover clock fix and paused-menu Summon intent. Default invocation prints a plan without mutation; no old cover or training acceptance transfers.

## Fresh root admission

Root reviews the runner, library and exact current device state before creating a new receipt. Required fields:

- `approved`: boolean true; `scope`: `ONE_FRESH_YAMAGATA_SUMMON_INTENT_PILOT`.
- `owner_pid`:43708; `owner_start`:2026-10-11T02:34:24.0733347-04:00. Expired identity requires re-review.
- `source`:c6d9abe271e7539ab8354c695ce084c4e2d10c8a; `mod_tree`:670a24d6e444ad2829facd0d23a903dc001e6e95.
- `manifest_sha256`:56ed97e041d1a8ec1ccd3e406c79ebba0cbcb7f3c44f455916556e5487085ca7.
- `engine_sha256`:4f6cccf1365c5659d323fcaedcc00726234b03912875889b1165670cae5eebe8; `addon_sha256`:5efd90ee4792f896e75c229dbefbaf8c79a37fa3f66dc67119d35e941d0992cc.
- `engine_entry_sha256` and `addon_entry_sha256`: freshly reviewed exact CURRENT installed ASIs, 64 lowercase hex characters each. These bind backup/restore to the current entry, not a historical baseline.
- `save_policy`: `CURRENT_ENTRY_WRITABLE_REVERT_AFTER_OWNED_EXIT_V1`.
- `pilot_sha256` and `library_sha256`: exact reviewed tool files; `out_dir`: exact absolute new session directory.
- `nonce`:32 lowercase hex; `review`: concrete root review; `created_utc`/`expires_utc`: nonfuture/current and at most30minutes apart.

Supply the admission file's SHA256 separately. No admission-minting mode exists. This note does not admit a launch while the user is playing.

## Invocation and preservation

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools/rematch/entry_hold_pilot.ps1
# After review, using actual fresh paths and hashes:
powershell -NoProfile -ExecutionPolicy Bypass -File tools/rematch/entry_hold_pilot.ps1 -Run -WritableSaves -OwnerPid 43708 -AdmissionReceipt <NEW-JSON> -AdmissionSha256 <HASH> -OutDir <NEW-SESSION> -SaveRoot <CURRENT-SAVE-ROOT>
```

Archive defaults to `D:\nioh3-re\candidates\combined-training-c6d9abe`; all six manifest artifacts are checked. The pinned C72 lifecycle owns one launch, exact PID/start/exe, device lease, fresh entry backups and durable verified restoration. The pinned opt-in writable adapter locks BACKUP files before releasing its own CURRENT-save read handles. It keeps backups protected until successful cleanup. Existing recovery records are retained on failure.

Transient setup enables EntryHold and existing BossLog/RematchLog diagnostics, disables warp/spawn/arena overrides, and sets VirtualPad0 for physical manual control. Original current-entry saves, INIs and ASIs restore after this dedicated test; this is separate from the user's preserve-progress manual session. No automatic gameplay input or relaunch.

## Actual native stages

1. Root confirms FieldReady after Continue. Runner requires12s continuously ready outside1CBC and freshly verifies current-journey clear. Root selects Practice Yamagata > Start at boss shrine exactly once.
2. Require exact native request1CBC/start67064, load/arrival and new owned attempt. EnterArena approval records shrine control; root manually walks into box66599. The initial phase2 witness must be positive and no older than1500ms.
3. Open hosted Practice without selecting Summon; observe it at least2seconds. HoldMenuReviewed requires visible menu, held boss, actual duration and the same valid phase2/attempt/destination/deadline. Historical boxAt may age while the menu pauses the native query.
4. Select Summon once while paused, then manually close any ordinary pause menu. Observe bounded pending intent, or direct phase3 if the transient was too fast to sample. Pending deadline cannot extend, exceed30s or outlive the hold. Observed loss, expiry, foreign attempt, abandonment or refusal fails without retry. The release wait is bounded35s; phase3 is accepted only with exactly one queue event. A release between log and status reads gets a bounded resample, not a false failure.
5. Require ordered post-Summon standalone Create65092 hook receipt followed by exact1CBC/67064 BossBattleMode, plus root's visible named boss and actual control. The old60s placement ring alone cannot pass. Create is an entry-hook receipt, not proof the native factory returned a usable actor; root observation remains required.
6. Root separately approves NativeReturn and selects Return from practice once. Require ordered native return/replay exit, stable exact original mission/stage and idle/spawn-/gateok, followed by current-entry restoration. No arena-switching or death/retry acceptance is implied.

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools/rematch/entry_hold_pilot.ps1 -ApproveStage HoldMenuReviewed -OwnerPid 43708 -OutDir <SESSION> -Evidence <ACTUAL-OBSERVATION> -VisibleMenu -HeldWithoutBoss -MenuObservedSeconds 2
```

EnterArena also requires ControlResponse; ControlReviewed requires VisibleBoss and ControlResponse. Use actual observations only. Hold cancellation/expiry restores the native predicate and can let the encounter proceed; it is not native-request cancellation.

## Offline validation

`entry_hold_pilot_test.ps1` exercises admission and owner pins, bounded intent and cancellation, release/log interleaving, fresh Create ordering and false placement-ring evidence, approval identity/duration, exact return and plan-only behavior. These are inert fixtures, not native usability evidence. A fresh reviewed in-game trial remains required.
