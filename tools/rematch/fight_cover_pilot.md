# Guarded hosted Fight now cover pilot

Private new-feature pilot only. Root reviewed and pinned candidate e9941b4 (Engine1.7.69/addon1.4.13), old native receipt and separate owner-bound admission in `Get-FCPExpected`. `-Run` still requires the exact admission file and live owner identity. Default or `-DryRun` only prints the plan; no game/config/save/lease mutation. No prior C72E PASS is transferred to cover acceptance.

The runner reuses the hash-pinned training library and its exact nine C72E lifecycle function imports; it does not evaluate either old runner's top-level code. Pass `-LifecycleRepo` when using this isolated worktree, because it does not contain all current-main dependencies. Dependency mismatch is a stop requiring review, never an automatic repin.

Example read-only plan:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools/rematch/run_fight_cover_pilot.ps1 -DryRun
```

Root will supply the new frozen Engine/addon SHA256, mod tree, old root-accepted native receipt SHA256 and a separately hashed explicit admission JSON. Admission JSON requires boolean `approved=true`, scope `ONE_FIGHT_COVER_NATIVE_PILOT_NEW_FEATURE_UNTESTED`, `owner_pid`, `owner_start`, `mod_tree`, `engine_sha256`, `addon_sha256`. Only source-tree layout is supported; binary paths are fixed under `mod/<name>/bin/<name>.asi`. Existing installed baseline pins and reviewed full catalogue pins remain mandatory.

Arena data is explicitly the pinned catalogue R1CBC arena337691,10244,206714/yaw-89.2/box66599, with a session-local header-only override file: UTF8 without BOM bytes `# no arena overrides\r\n`, SHA256 `d05722d1edae92d39bdb8a1a1fe6bac59fc92550c38f8593aa04d2087d50b171`. No unpinned installed capture overrides. FightNow/FightNowWarp/FightNowSpawn=1; overlay1; capture/selftest disabled. Engine menu preferences are preserved. Session writes are restored by the existing owned finalizer.

One launch, root Continue confirmation,12s continuously ready field, fresh clear scan; then root selects hosted Yamagata Fight now once while observations already run. The oracle requires a new token, exact tuple, ordered begin/present_ack/native_post/ready events with one native attempt, all readiness bits and nonzero sample<=deadline. Failed/cancelled release is never success. Phase4's sample is historical: root separately confirms actual opaque transition, arena/boss/control response; current basic mission/player/loading/fight status is checked again. Root review does not claim first physical controllable frame timing.

Confirmations use `-ApproveStage FieldReady|FightReady|ControlReviewed|NativeReturn -OwnerPid ... -OutDir ... -Evidence ...`; ControlReviewed additionally requires all `-VisibleArena -VisibleBoss -ControlResponse`. Receipt nonce binds owner, runner and adopted game PID/start. FightReady separates navigation from the180s native observation: navigate to final confirmation without confirming, approve FightReady, wait for the runner's observing message, then confirm once. The original cover/log baseline is captured before navigation; changed cover during navigation fails instead of rebasing. The overall guard deadline still bounds navigation. Return waits for a4s telemetry proxy, then submits native `exit` exactly once; authoritative native gates may refuse. Exact original entry tuple including original mission0, ordered request/exit logs and restored saves/INIs/binaries are required. No forced retry, arbitrary walk, manual native warp, fallback route or automatic second launch.

Root2 (`fight-cover-R1CBC-20261011-root2`) ended before native submission: token0 throughout, manual navigation consumed the former180s window. All8 saves,2INIs and2ASIs independently matched entry afterward; lease free/game absent. New FightReady timing passed75 fixtures and independent peer review. Root reviewed a separate one-run receipt `root-native-admission-3.json`, SHA256 a62e2fcd2064302807317b4d078d0ed57f2a5fb3b7dc4518b45099c543d4ff95, for the same frozen e9941b4 pair. Earlier consumed receipts cannot authorize this new run.

Failure retains original error and guard/recovery ownership when restoration fails. Verified restoration is durably recorded before lease release; partial release retries cannot restore entry saves again. No new destructive recovery path. Root has authorized one exact pilot; live review remains outstanding.

Offline fixtures:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools/rematch/test_fight_cover_pilot.ps1 -LifecycleRepo 'C:\Users\Ahmad Hamza\nioh3-engine'
```

Fixtures cover ordering/missingACK/token/attempt/tuple/readiness bits, failure/cancel, replaced logs, strict owner/game review, unset/mismatched admission, exactmission0 return, actual runner finally guard retention and restoration-gated completion. They perform no game actions. An initial wrong return fixture string was corrected to the actual native `replay mission exited (stage now ..., mission state +28h ...)` format; final63 checks pass.

Fight Cover return oracle correction (root3): the frozen addon retains historical
`spawn=arena` after verified spawn-record restoration. The Practice oracle stays
unchanged. Fight Cover now requires exactly one native return from1CBC/67064,
then exact original-entry replay exit, then `spawn override restored and verified
(replay mission exit): record 67064`. Only a status sampled after that log read
can finish: state0, row-, infight0, warps0, loading0, player1, exact entry tuple and
gateok. Historical arena/applied/start labels are accepted only with this ordered
restoration proof; pending/failed/recovery and unknown labels remain refused.
Replacement logs, duplicate returns/exits/restores, new requests, quarantine,
exceptions, refusals and reapplication cannot become acceptance.

The actual return-loop fixture verifies owned identity check -> log read -> status
read. The retained root3 result remains TRIAL_FAILED; offline evidence validation
does not approve the run or change its raw receipt. A separate root review is
required for any acceptance. The exact retained archive can be checked read-only:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools/rematch/test_fight_cover_pilot.ps1 -ReturnArchive D:/nioh3-re/rematch-pilots/fight-cover-R1CBC-20261011-root3
```

Result:113 checks PASS, including the original oracle false negative, strict
ordered restoration, subsequent idle/identity negatives and unmodified archived
failure receipt. Without the optional archive,108 self-contained checks run.
