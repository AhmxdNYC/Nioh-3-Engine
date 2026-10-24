# Combined Summon native pilot: executable readiness

2026-10-12. No native run or installed-file change performed by this agent.
Root can conduct this test; human gameplay is not a prerequisite. Root must
coordinate the available PC/account with Ally before starting a Steam session.

## Repaired prelaunch blocker and actual checks

The existing1d7267c pilot (original isolated commit2b85cc1) hardcoded training
library hash50192ec2..., but its actual committed library is e7b2bca9... with
CRLF (LF blob d67cd936...). This is not simply LF versus CRLF. No matching
expected hash was found among historical committed versions inspected.
The current entire library was reviewed; its existing00c155c durable lifecycle
pin and exact original-tuple return checks remain intact. Only the stale
outer dependency literal was corrected. The test now verifies all7 actual
dependency files: plan-only previously returned before those checks.

Executed inert checks: entry-hold127 PASS; native-training103 PASS;
writable-save22 PASS (real temporary-file restoration and pinned finalizer).
All6 candidate archive files match their manifest. No need to rebuild ASIs.

Source/archive: c6d9abe271e7539ab8354c695ce084c4e2d10c8a,
modtree670a24d6e444ad2829facd0d23a903dc001e6e95,
`D:/nioh3-re/candidates/combined-training-c6d9abe`.

- Manifest:56ed97e041d1a8ec1ccd3e406c79ebba0cbcb7f3c44f455916556e5487085ca7.
- Engine1.7.72:4f6cccf1365c5659d323fcaedcc00726234b03912875889b1165670cae5eebe8.
- Addon1.4.18:5efd90ee4792f896e75c229dbefbaf8c79a37fa3f66dc67119d35e941d0992cc.
- Corrected runner:f3771a3c6ea95607733b4d97e0b12d591acc53c08d96f0c738f0cd1bac71bc83.
- Entry-hold library:c997f167f9119ddba6e152cc0c264a1c61446e34660c559ffb19523fb8d843f5.
- Training library:e7b2bca9414da70db55e7cdde8181dfba42c0ea4c5672f3fa9a136bc5ab38bb2.

These runner hashes bind this worktree's bytes; copying with changed newlines
needs a newly reviewed hash. Original production candidate retains focused
build/110hook/255+97site/menus/readers/transition/native transaction passes.
No full36-harness release PASS or native acceptance is claimed for this pair.

## Actual root execution

Use the isolated corrected tree `D:/nioh3-wt/training-native-readiness-20261012`.
The owner is pinned PID43708/start2026-10-11T02:34:24.0733347-04:00 and was
present during this check. Verify the exact identity again before admission.
Admission schema remains `tools/rematch/entry_hold_pilot.md`: root records its
actual review, fresh current installed ASI hashes, unique output directory,
nonce, current timestamp/at-most30minute expiry, exact runner/library hashes,
and CURRENT_ENTRY_WRITABLE_REVERT_AFTER_OWNED_EXIT_V1. Do not reuse old admission.

After root creates that reviewed receipt and supplies its actual path as
`$summonAdmission` and matching new output as `$summonOutput`, the launch is:

```powershell
$summonPilot = 'D:/nioh3-wt/training-native-readiness-20261012/tools/rematch/entry_hold_pilot.ps1'
$summonReceiptHash = (Get-FileHash -LiteralPath $summonAdmission -Algorithm SHA256).Hash.ToLowerInvariant()
powershell -NoProfile -ExecutionPolicy Bypass -File $summonPilot -Run -WritableSaves -OwnerPid 43708 -AdmissionReceipt $summonAdmission -AdmissionSha256 $summonReceiptHash -OutDir $summonOutput -SaveRoot "$env:LOCALAPPDATA/KoeiTecmo/NIOH3"
```

This owns one launch, current-entry saves/INIs/ASIs, exact PID/start, and a
verified finalizer. Existing user progress is the fresh entry baseline; never
restore the earlier manual-play backup. Requires18GB headroom and no game.
It enables EntryHold, disables warp/spawn, preserves picture, and sets
VirtualPad0. It does not send menu/gameplay input. Root must operate the UI and
observe current captures/control itself through its available input tools.
No autonomous UI driver is included; do not mint observed approvals merely
because the runner is waiting. Do not silently repin to a different owner.

The actual sequence is:

1. Continue once. Observe field, approve FieldReady. Runner then waits12s
   continuously stable outside1CBC and verifies fresh cleared row1CBC.
2. Root opens Engine menu > Boss Rematch / Practice > Practice Yamagata >
   Start at boss shrine once. Runner verifies1CBC/67064 arrival; root observes
   shrine movement/camera and approves EnterArena with ControlResponse.
3. Root walks into native box66599. The runner requires phase2, same attempt,
   fresh<=1500ms true-box witness, no boss/warp/spawn. Root opens hosted
   Practice without selecting Summon and observes held menu>=2s.
4. Approve HoldMenuReviewed with VisibleMenu, HeldWithoutBoss and actual
   MenuObservedSeconds. Select Summon once, close ordinary pause menu. Only
   stock-true native entrance query releases; bounded intent<=30s, no retry.
5. Require new ordered standalone Create65092 and native1CBC/67064 battle,
   then root sees named boss and tests actual camera/player control. Approve
   ControlReviewed with VisibleBoss+ControlResponse.
6. Approve NativeReturn separately, choose Return from practice once. Require
   exact original mission/stage, native replay exit, idle/spawn-/gateok, then
   owned close and verified current-entry restoration before success.

For each current `awaiting.json`, root uses the same script's approval mode:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File $summonPilot -ApproveStage FieldReady -OwnerPid 43708 -OutDir $summonOutput -Evidence $actualFieldObservation
powershell -NoProfile -ExecutionPolicy Bypass -File $summonPilot -ApproveStage EnterArena -OwnerPid 43708 -OutDir $summonOutput -Evidence $actualShrineControl -ControlResponse
powershell -NoProfile -ExecutionPolicy Bypass -File $summonPilot -ApproveStage HoldMenuReviewed -OwnerPid 43708 -OutDir $summonOutput -Evidence $actualHeldMenu -VisibleMenu -HeldWithoutBoss -MenuObservedSeconds $actualSeconds
powershell -NoProfile -ExecutionPolicy Bypass -File $summonPilot -ApproveStage ControlReviewed -OwnerPid 43708 -OutDir $summonOutput -Evidence $actualBossControl -VisibleBoss -ControlResponse
powershell -NoProfile -ExecutionPolicy Bypass -File $summonPilot -ApproveStage NativeReturn -OwnerPid 43708 -OutDir $summonOutput -Evidence $actualReturnDecision
```

Run each only at its observed stage, not as one batch. Unknown control, expired
hold, missing exact native witness or lost ownership fails this single trial.
Review result.json's status, both root observation bits and all3 restoration
bits; an eventual boss clear alone does not accept the Summon lifecycle.
