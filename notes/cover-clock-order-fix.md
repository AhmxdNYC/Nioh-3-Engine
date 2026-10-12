# Root2 cover clock-order correction

Isolated branch from1ffa569; original/archive unchanged. Engine1.7.71 / addon1.4.16, ABI2 unchanged.

Root2 actual log: begin01:35:16.576; presentation ACK/native post.584; native MissionRequestById.599; phase5 renderer-stale.610. The native attempt continued to spawn override application at01:35:17.153; presentation failure did not cancel it. Root closed and restored the session. This is not a live success.

The real addon harness reproduces the interleaving: cache Tick time, subsequently record a genuine CoverPresented timestamp, then run CoverTick with the cached time. Original code reports stale because cached t<hostAt. CoverTick now samples Ms only after CoverLock ownership, so prior Begin/ACK cannot appear to come from the future. CoverSnapshot also reads its independent atomic pump heartbeat before sampling current time.

The real Engine harness separately reproduced RefreshRematchHost rejecting a legitimate worker publication newer than its caller's cached timestamp. It now samples time after CopySnapshot. CoverCaptures likewise reads its atomic lease/armed fields before sampling time. No tolerance, timestamp clamping or skipped future checks were introduced.

Actual menu regressions pass for both interleavings. Explicit future60s timestamps and stale1500ms boundaries still fail closed, with native request/spawn recovery sentinels retained through cover dismissal. Fixed180s Yamagata cover,1500ms leases,5000ms pre-presentation and10000ms native admission limits remain unchanged. AutoContinueLoad remains a user preference plus the separate already-reviewed scoped tip permission.

Next root native trial must use fresh exact pair/admission hashes, demonstrate no false34ms failure, then observe the real native tip/intro/boss control and owned return. This change cannot turn the preserved root2 failure into an accepted run.

Checks: full pair MSVC build PASS (Engine retains its existing local-shadow warning); actual Engine/addon menus PASS; Engine255/addon94 sites PASS; transition73, passive readers200, presentation37, emitted tip thunk/Windows unwind and20 native anchors PASS. Quick release checker0errors/3 inherited warnings; no repeat of unrelated36-harness full suite for this narrow clock fix.

Binary pins:
- Engine1.7.71 `ce7aa75fa00206490ff9bb3245496d972c294cb7f8f572aeecfc5b430563a597`.
- Addon1.4.16 `a24d1b2b2cf6666346c2d6efe06b5a87cafb971f308eabdd8cc315972f675f97`.

Root must create fresh candidate/pilot admission. Keep scoped-tip test AutoContinueLoad0 transiently and restore current INI after the owned run; this change adds no setting or controller modification.

Independent bounded static review by training_contract: PASS; same-lock/read-before-clock ordering and genuine-future/1500ms negative gates verified. No live acceptance claim.
