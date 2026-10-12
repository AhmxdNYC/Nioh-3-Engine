# Guarded hosted Fight now cover pilot

New-feature preparation only. Candidate/source/admission pins in `Get-FCPExpected` are deliberately empty: `-Run` refuses until root reviews the frozen build and fills exact pins. Default or `-DryRun` only prints the plan; no game/config/save/lease mutation. No prior C72E PASS is transferred to cover acceptance.

The runner reuses the hash-pinned training library and its exact nine C72E lifecycle function imports; it does not evaluate either old runner's top-level code. Pass `-LifecycleRepo` when using this isolated worktree, because it does not contain all current-main dependencies. Dependency mismatch is a stop requiring review, never an automatic repin.

Example read-only plan:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools/rematch/run_fight_cover_pilot.ps1 -DryRun
```

Root will supply the new frozen Engine/addon SHA256, mod tree, old root-accepted native receipt SHA256 and a separately hashed explicit admission JSON. Admission JSON requires boolean `approved=true`, scope `ONE_FIGHT_COVER_NATIVE_PILOT_NEW_FEATURE_UNTESTED`, `owner_pid`, `owner_start`, `mod_tree`, `engine_sha256`, `addon_sha256`. Only source-tree layout is supported; binary paths are fixed under `mod/<name>/bin/<name>.asi`. Existing installed baseline pins and reviewed full catalogue pins remain mandatory.

Arena data is explicitly the pinned catalogue R1CBC arena337691,10244,206714/yaw-89.2/box66599, with a session-local header-only override file: UTF8 without BOM bytes `# no arena overrides\r\n`, SHA256 `d05722d1edae92d39bdb8a1a1fe6bac59fc92550c38f8593aa04d2087d50b171`. No unpinned installed capture overrides. FightNow/FightNowWarp/FightNowSpawn=1; overlay1; capture/selftest disabled. Engine menu preferences are preserved. Session writes are restored by the existing owned finalizer.

One launch, root Continue confirmation,12s continuously ready field, fresh clear scan; then root selects hosted Yamagata Fight now once while observations already run. The oracle requires a new token, exact tuple, ordered begin/present_ack/native_post/ready events with one native attempt, all readiness bits and nonzero sample<=deadline. Failed/cancelled release is never success. Phase4's sample is historical: root separately confirms actual opaque transition, arena/boss/control response; current basic mission/player/loading/fight status is checked again. Root review does not claim first physical controllable frame timing.

Confirmations use `-ApproveStage FieldReady|ControlReviewed|NativeReturn -OwnerPid ... -OutDir ... -Evidence ...`; ControlReviewed additionally requires all `-VisibleArena -VisibleBoss -ControlResponse`. Receipt nonce binds owner, runner and adopted game PID/start. Return waits for a4s telemetry proxy, then submits native `exit` exactly once; authoritative native gates may refuse. Exact original entry tuple including original mission0, ordered request/exit logs and restored saves/INIs/binaries are required. No forced retry, arbitrary walk, manual native warp, fallback route or automatic second launch.

Failure retains original error and guard/recovery ownership when restoration fails. No new destructive recovery path. Root approval/live review remains outstanding.

Offline fixtures:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools/rematch/test_fight_cover_pilot.ps1 -LifecycleRepo 'C:\Users\Ahmad Hamza\nioh3-engine'
```

Fixtures cover ordering/missingACK/token/attempt/tuple/readiness bits, failure/cancel, replaced logs, strict owner/game review, unset/mismatched admission, exactmission0 return, actual runner finally guard retention and restoration-gated completion. They perform no game actions. An initial wrong return fixture string was corrected to the actual native `replay mission exited (stage now ..., mission state +28h ...)` format; final63 checks pass.
