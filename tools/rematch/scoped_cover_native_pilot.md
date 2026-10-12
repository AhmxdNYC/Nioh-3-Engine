# Scoped-tip Yamagata native pilot

New one-launch driver for source `1ffa5696b746a7806958bef2ab4bfe1a9003fb53`, mod tree `65365b00e9e2d770de334bce33850ec3f3ad2899`, Engine1.7.70/addon1.4.15. Old FightCover pilots and receipts are unchanged. Default/DryRun only reports a plan. No approval issuer exists here.

Root43708 / start `2026-10-11T02:34:24.0733347-04:00` must review and create a NEW receipt, approved boolean true, scope `ONE_SCOPED_COVER_WRITABLE_YAMAGATA_NATIVE_PILOT`. Required fields: owner_pid/start, source, mod_tree, engine_sha256, addon_sha256, gate_receipt_sha256, pilot_sha256, library_sha256, backup_policy_sha256, save_policy=`CURRENT_ENTRY_WRITABLE_REVERT_AFTER_OWNED_EXIT_V1`, exact absolute out_dir,32-lowercase-hex nonce, nonempty review, created_utc/expires_utc (not future, unexpired, at most30minutes). Expected immutable pins are in Get-SCPExpected; calculate current pilot/library hashes after checkpoint. Existing accepted C72 lineage is a prerequisite only, never transferred acceptance for this feature/policy.

Root invocation after independently minting reviewed admission:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools/rematch/scoped_cover_native_pilot.ps1 -Run -WritableSaves -OwnerPid 43708 -OutDir '<NEW EXACT RECEIPT DIRECTORY>' -AdmissionReceipt '<FRESH REVIEWED RECEIPT>' -RequestGateReceipt '<PINNED C72 PREREQUISITE RECEIPT>' -SaveRoot '<VERIFIED CURRENT SAVE ROOT>' -CandidateRoot 'D:\nioh3-wt\cover-publication-fix'
```

Existing Kit/GameDir defaults remain local-root paths. Current original installed ASI hashes are checked before staging; no arbitrary replacement accepts. The writable adapter verifies/locks the current entry backup and publishes its durable policy receipt before releasing current-save handles. Pinned Finish-C72 runs first on every exit; backup handles unlock only after cleaned=true. Failure retains guard/recovery evidence. Cloud/controller policy remains root-owned and unchanged.

Stages: FieldReady, FightReady (before the one manual final confirmation), ControlReviewed (`-VisibleArena -VisibleBoss -ControlResponse`), NativeReturn. Each approval also requires exact OutDir/OwnerPid and nonempty Evidence. Root supplies movement/confirmation/control evidence; no auto-navigation or retry. After NativeReturn confirmation, one native exit command is sent, then the strict existing FightCover return/restoration oracle applies. An original entry already in1CBC is refused before request.

AutoContinueLoad is explicitly0 during the trial. Exact Yamagata has a180s cover budget; observation is bounded200s. Phase4 and ordered begin/ACK/native_post are necessary but root visual/control approval remains required. Existing Engine AutoContinue log with the persistent switch confirmed0 identifies answered scoped tip checks; no setting is enabled. Final success also requires owned return and exact current-entry save/INI/binary restoration. No live acceptance claimed by the35 offline admission/plan/orchestration checks.
