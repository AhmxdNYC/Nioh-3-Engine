# Explicit writable current-entry policy

This is an opt-in adapter, not a new launch driver or admission issuer. Default
C72 deny-write behavior and existing receipts/validators remain unchanged.
Root must approve a NEW admission bound to the adapter hash and the distinct
`CURRENT_ENTRY_WRITABLE_REVERT_AFTER_OWNED_EXIT_V1` policy. Prior deny-write
acceptance is not evidence for this mode. Cloud isolation remains the caller's
existing reviewed requirement; the adapter changes no cloud setting.

Integration into a reviewed pilot:

1. Validate all existing lifecycle dependencies and this adapter's reviewed hash.
   Dot-source the pinned C72 library and use the existing pinned lifecycle importer;
   the adapter checks the exact library/finalizer hashes before handoff. Run the
   normal guard, acquire the PC lease, capture the CURRENT entry manifest, verify
   its backup and acquire the caller's normal C72 save handles as before.
2. Before the single launch, call the adapter with those exact owned FileStreams:

```powershell
. "$PSScriptRoot/writable_save_policy.ps1"
$script:writablePolicy=Enable-WritableSavePolicy -OptIn -Root $SaveRoot `
 -Backup $entryDir -Manifest $script:entry -CurrentHandles $script:saveHandles `
 -Receipt (Join-Path $OutDir 'writable-save-policy.json') `
 -HeldLeases $script:held -AdmissionSha256 $AdmissionSha256
$script:result.save_policy=$script:writablePolicy.policy
$script:result.save_policy_receipt=$script:writablePolicy.receipt
```

The function denies an active game, overlapping paths, foreign/duplicate handles,
changed census or reused receipt. It locks backup files against write/deletion,
verifies current/backup hashes and durably publishes a new policy receipt BEFORE
releasing only the caller's current-save handles. The receipt intentionally states
the pre-release boundary, not proof that release or a game session completed.
The adapter does not prove lease-service ownership from a supplied list; the
caller's existing acquisition/guard remains authoritative. It is PC-scoped.

3. Retain `$script:writablePolicy` for the whole session. Keep the existing launch
   identity/headroom/guard/one-launch checks. An adapter error aborts preparation;
   never continue to launch after it. Native saves may now change normally.
4. In the existing finalization path call pinned `Finish-C72` first. Only after
   it reports `$script:cleaned` true call
   `Close-WritableSavePolicy $script:writablePolicy $script:cleaned`.
   On failure retain the policy object, guard, PC lease, backups and receipts.
   Keep existing `Done-Guard -KeepFile:(-not $script:cleaned)` behavior.

The actual finalizer closes the owned game gracefully, refuses restoration while
ANY Nioh3 remains, verifies backup hashes, restores changed/deleted files, removes
new files, then compares exact paths/lengths/hashes. Its durable restoration receipt
precedes lease release; recovery must not replay old saves after ownership release.
Backup locks permit restore reads. A process exit naturally releases locks, so a
restart requires ownership and backup-hash review, never blind restoration.
This adapter never deletes backups or recovery evidence.

Offline checks (temporary fixture files only):

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools/rematch/writable_save_policy_test.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File tools/rematch/test_c72e_receipt_finalizer.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File tools/rematch/test_c72e_abort.ps1
```

The new fixture uses real save mutations and actual pinned `Finish-C72`/restore
code, mocking process, lease and ASI surroundings. It covers opt-in, changed backup,
failed policy publication, backup write denial, game-remains deferral, actual
new-file deletion failure, exact restoration and retained receipts. Existing
receipt-finalizer tests exercise partial writes/real publication failures and
partial lease release; abort tests exercise actual guard retention. None supplies
new native/game acceptance. No real save directory or game is used by the fixtures.
