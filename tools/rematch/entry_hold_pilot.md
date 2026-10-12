# One native Yamagata entry-hold pilot

Preparation only. Exact archived dbd6752 pair; no old cover/practice acceptance transfers. Default invocation prints PLAN without filesystem mutation. No admission-minting mode exists.

## Root review and admission

Only current root PID 43708/start 2026-10-11T02:34:24.0733347-04:00 is accepted. Root must review this runner/library and create a NEW admission JSON outside existing pilot receipts with:
- approved: boolean true; scope: ONE_FRESH_YAMAGATA_ENTRY_HOLD_PILOT
- owner_pid and owner_start above
- source dbd6752b0b54c51e037a2999976ce94a348ab676
- mod_tree 4f4b3c045565244917fb1b96496a0a0596b3cf0c
- manifest_sha256 f9847291b9a3fc9e1a212604b36fdcbe94e873bb3ae8c4fa7f590772b140a6bf
- engine_sha256 ac46084cd7169cbd3ef9ba28e26bd92707ad275bcce86421c7f02e3f58846856
- addon_sha256 63a7d68142912dbfa7d4059a91a9bd2955b576db6727150b6b50b9e6e626a803
- pilot_sha256/library_sha256: freshly reviewed exact tool-file hashes
- out_dir: exact absolute NEW session directory; nonce: 32 lowercase hex characters
- review: concrete root review statement; created_utc/expires_utc: nonfuture/current, <=30-minute admission validity.

The receipt SHA256 is supplied separately by root. Do not copy an old admission or claim this note authorizes launch. PID expiry requires root re-review rather than bypass.

## Invocation

Plan:
powershell -NoProfile -ExecutionPolicy Bypass -File tools/rematch/entry_hold_pilot.ps1

AFTER review and fresh receipt, root supplies actual paths:
powershell -NoProfile -ExecutionPolicy Bypass -File tools/rematch/entry_hold_pilot.ps1 -Run -OwnerPid 43708 -AdmissionReceipt <NEW-REVIEWED-JSON> -AdmissionSha256 <EXACT-SHA256> -OutDir <NEW-ABSOLUTE-SESSION> -SaveRoot <CURRENT-LOCAL-SAVE-ROOT>

Use spaces between parameter names and values (OwnerPid 43708). The archive defaults to D:\nioh3-re\candidates\training-entry-hold-dbd6752; every manifest artifact is verified. Installed baseline ASIs must dynamically match dd8b573... and02d763e... before staging. No fallback install on mismatch. The exact reviewed C72 function importer/finalizer retains backups, process identity, headroom, save locks and durable restore-before-lease-release ordering. Guard stays retained on cleanup failure. Config is transient: EntryHold1, hosted menu, warp/spawn0, no arena overrides. Original current-entry saves/INIs/binaries restore after the one session.

## Manual stages

1. FieldReady: root confirms Continue completed; runner waits 12s continuously ready outside1CBC, then performs a fresh cleared-journey scan. Root submits ONE hosted Practice Yamagata start while observation is active.
2. EnterArena: exact native queue/start and new hold attempt observed; root confirms shrine control, then walks manually.
3. WaitingAtBox phase2: current exact67064/1CBC, no fight, live box-positive witness <=1500ms. Root opens hosted Practice menu WITHOUT selecting Summon. HoldMenuReviewed requires explicit visible_menu and held_without_boss true; runner checks the same attempt, valid destination, unbroken phase2 and deadline throughout this review and afterward; an already-observed positive box witness may age while the menu pauses gameplay.
4. After review, the runner waits at most30s for a NEW native positive box witness (boxAt strictly newer than the post-review sample and age<=1500ms). Root closes/unpauses the menu while staying at the entrance if necessary; no synthetic refresh is used. Only after renewal does the runner invite ONE manual Summon selection. Reopen and select promptly: frozen native Release still refuses a witness older than1500ms, so an arbitrarily delayed paused Summon is unsupported. If renewal is unavailable, the pilot fails bounded. It requires phase3, same attempt and exactly one menu Summon request, followed by fresh exact native BossBattle event. It never fabricates a predicate result, factory call or flags.
5. ControlReviewed requires visible_boss/control_response true; NativeReturn is separate root permission. Root uses owned Return from practice once. Exact original mission/stage and ordered native return + idle/spawn-/gateok are required; Practice oracle remains strict. Log is read before final status.
6. No automatic retry, request cancellation inference, death/retry test or arena switching. Hold expiration/abandonment fails observation and cleans up; it can allow the native encounter to proceed. Root must not treat cancellation as native-request cancellation.

Approvals use the existing awaiting.json nonce/process tuple, plus -Evidence. Example:
powershell -NoProfile -ExecutionPolicy Bypass -File tools/rematch/entry_hold_pilot.ps1 -ApproveStage HoldMenuReviewed -OwnerPid 43708 -OutDir <SESSION> -Evidence <ROOT-OBSERVATION> -VisibleMenu -HeldWithoutBoss

EnterArena also requires -ControlResponse; ControlReviewed requires -VisibleBoss -ControlResponse. Use actual observations only. Timing/status is not physical control or menu-visibility proof.

## Offline checks / limitations

entry_hold_pilot_test.ps1 includes policy/actual-observer/plan checks: admission lineage/owner/expiry, unknown/duplicate hold fields, stale/foreign/abandoned/wrong-destination hold, premature/duplicate/foreign requests, log replacement, approval bits/identity, actual release/return observer branches, read-before-status ordering and default-plan behavior. Tests use inert fixtures; no game, pipe input or launch occurred. Native stage usability remains untested. Delayed paused-menu preservation and actual renewal-loop fixtures cover changed attempt, unknown/expired state and early Summon. Root must review before minting admission.
