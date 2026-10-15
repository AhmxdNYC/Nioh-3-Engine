# Ally cover publication / loading-tip correction

Private branch `codex-cover-publication-fix`, based on frozen `e9941b4`. Engine 1.7.70 / addon 1.4.15, ABI2 unchanged. No archived or installed binaries changed.

## Reproduced defect

One failed addon `CoverSnapshot` try-lock previously cleared `CoverActive` while retaining phase3/token/reason and refreshing `workerTickMs`. The actual provider harness reproduced this false terminal publication before the fix. This matches Ally evidence251: Engine reported transition finished with a waiting-for-control reason, followed by addon Menu closed/cancellation without proving explicit B/Escape input.

The worker now retains its previous coherent publication and heartbeat on failure. Already-consumed actions remain consumed and generation invalidation remains monotonic. Persistent failures still expire the existing 1500ms lease. ABI validation rejects phase1?3 without CoverActive, so an incoherent fresh snapshot cannot refresh the host.

## Covered loading-tip permission

The user's AutoContinueLoad switch and INI remain unchanged. A separate try-only lease is published only after an accepted, submitted covered Present for coherent Loading phase3. Native tip confirmation checks token/session, current cover state, cancellation, Enabled, worker/presentation freshness and the absolute deadline at the time of use; it cannot extend permission itself. It answers only the existing exact tip-confirm callsite, retaining native load-done, blocked-confirm and queued-system-message checks. No native pause/loading bits change.

The expanded near thunk preserves RCX/RDX/R8?R11 and XMM0?5 around the callback, aligns the stack/shadow space and registers its fixed-stack unwind metadata. Denied permission tail-jumps to the original leaf. Tests execute the emitted bytes for all user-switch/permission combinations, verify register preservation and exercise Windows virtual unwinding at the callback-return PC.

## Scope and verification

Actual provider regression PASS74; actual Engine/addon menu harnesses PASS; ABI invalid-snapshot tests PASS. Transition checks:73 model,200 passive-reader,37 presentation/lease, actual emitted thunk,20 native anchors PASS. Independent bounded static review by training_contract found no blocker. Final check_release --full PASS:0 errors/3 inherited warnings, Engine ALL255 SITES OK and all36 harnesses; addon ALL94 SITES OK. Both binaries built; Engine build retains its existing dllmain local-shadow warning.

Not a live acceptance receipt. Measured Ally Yamagata load102.6s plus secondload5.7s justifies a fixed180s cover deadline for exact tuple1CBC/67064/65092 only; other routes keep90s. No sliding renewal: AwaitPresent5s/admission10s and1500ms worker/presentation freshness stay unchanged. A331s stalled retry is not admitted as normal. The two-load regression retains cover between loads until boss/control readiness; exceeding180s still dismisses presentation without cancelling native ownership. Native ownership/restoration remains independent of cover dismissal. The separate triple-Start incomplete-gesture replay/native pause-menu behavior is not changed by this correction. This branch does not include the experimental entry-hold/training branch or its newer player text.

## Root native trial binding

Keep `[General] AutoContinueLoad=0`; no new option is required. Preserve the existing approved hosted Rematch setup (Rematch Enable/MenuOverlay/FightNow/FightNowWarp/FightNowSpawn enabled, Engine menu and actual pad/keyboard capture hooks available). The pilot's existing pipe/admission/save/controller ownership remains root-managed.

Expected order remains addon `event=begin` -> `event=present_ack` -> `event=native_post` for one token/attempt, then phase3 while loading. Engine's existing `AutoContinue #N: load done - answered the tip page's 'press to continue' check` records answered native tip checks. With the persistent user switch independently confirmed zero, the only new grant source is the finite covered-Loading permission; it does not log a new setting change. Confirm phase4 only after exact boss/current-player readiness, then root's visual/control and owned-return/restoration receipt. Fail/cancel/timeout remain failure evidence, not accepted playability.

Frozen binaries after build:
- Engine1.7.70 SHA256 `f6f9ba6162103f06403a11307af1c01e54487bf6f5d02aa7e12b5742935c13ab`.
- Addon1.4.15 SHA256 `ccb7dc8c6d8b4779e405f42c79939584f63e1ef10b3044406d6e3ed4471136df`.
