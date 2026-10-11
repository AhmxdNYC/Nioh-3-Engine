# Native Yamagata pilot tooling (offline preparation)

`tools/rematch/run_training_native_pilot.ps1` defaults to a read-only JSON plan.
`-DryRun` is identical: it does not inspect the game/save tree, acquire a lease,
query the pipe, install anything or launch. This tool has **not** exercised a live
training route. The current C72E attempt did not pass its request/landing gate;
the training launch remains gated.

The one supported scenario is cleared-current-journey `R1CBC` (mission `0x1CBC`,
Yamagata placement `65092`), native shrine `67064`. Root manually walks through
native box `66599`; the script does not warp, spawn enemies, walk or fight.
It is a native mission/rematch rehearsal, not arbitrary enemy training or a
reward-free practice mode.

## Before a future root-authorized launch

The tool requires a C72E `result.json` with
`LANDING_PROXY_AND_BOSS_OBSERVED` or
`ROOT_CONFIRMED_NATIVE_ENTRY_FIRST_CONTROL_TIME_UNKNOWN`, all three restoration booleans true,
`root_control_confirmed` and `native_return_confirmed` literally boolean true, and
sibling `entry-manifest.json` recording the exact same Engine 1.7.67/addon 1.4.9
runtime hashes. This is deliberately stricter than queue acceptance alone.
Unknown first-control timing does not invalidate a usable native practice route;
near-target/before-gate proxies cannot substitute for actual control evidence.
Root must separately review the C72E visual/control evidence. A JSON receipt
is operational evidence, not a signed authorization or proof of control.

`-Run -OwnerPid <root PID> -RequestGateReceipt <result.json>` is the explicit
launch mode. Never use it while another game is running. Exact baseline ASIs,
candidate ASIs and catalog are pinned. The normal 18 GB launch headroom guard,
PC lease, owner identity, deadline, fresh save backup/locks and transactional
binary restoration remain in use. No changes to Steam Cloud or controller
configuration are made by this tool.

## Manual stage protocol

The runner publishes `awaiting.json` with a nonce and root/runner PID-start
identities. In a separate root-controlled shell, invoke this same script with
`-ApproveStage <stage> -OwnerPid <root PID> -OutDir <session> -Evidence <observations>`.
This writes an atomic local confirmation only; it sends no game input. All four
approvals are required in order:

1. `ShrineRequest`: root has manually continued and confirms field/control.
   Runner requires 12 seconds of continuously observed idle/player/loading=0
   `gate=ok` within 180 seconds after approval (read failures or observation gaps
   reset the window), current-journey clear status 2, the exact
   catalog tuple, then sends one `shrine R1CBC`. It requires the exact queue
   witness (mission/start/mode 2), an observed load and shrine landing proxy.
2. `EnterArena`: root confirms a visible, controllable shrine landing, then
   manually walks through box 66599. Runner waits for Yamagata placement,
   `infight=1` and a fresh native BossBattleMode event.
3. `NativeRetry`: root confirms visible Yamagata/basic combat and authorizes
   **one** manual native death/retry. Runner sends no retry or input command.
   It waits for a new observed load and the same shrine proxy.
4. `NativeReturn`: root confirms that the observed load was the one native
   death/retry and that shrine control returned. Runner rechecks shrine state,
   sends one `exit`, then waits for mission departure/idle `gate=ok`.

No second request, retry loop or relaunch is provided. The guard deadline bounds
manual waits; shrine/battle/departure observations each have a 180-second bound.
An unobserved short load is an incomplete test, not grounds to infer success.
Final success is `MANUAL_NATIVE_ROUTE_COMPLETED_REVIEW_REQUIRED`; root still
reviews screenshots and confirmations. Position/status does not prove control,
and active `gate=native replay/return still pending` masks fresh NativeGate
diagnostics in this addon. Native mutation wrappers remain authoritative.

## Recovery coupling and verification

The new helper AST-imports only nine function definitions from the exact
SHA256-pinned C72E runner. Its top-level route is never evaluated. This is
intentional short-term coupling: any C72E change fails closed until reviewed and
repinned. It reuses the proven finalizer/abort override instead of copying the
recovery logic; failed close/restoration retains recovery ownership and records.
The supporting pilot/save, binary, guard and session libraries also have exact
SHA256 pins, checked before they are dot-sourced (including in plan mode).
Any change requires review and repinning; a passing plan checks these local
dependency bytes but does not establish live readiness.

Scan generation is captured from the row **before** posting `scan`; a completion
that races ahead of the scan reply is accepted only when the later row generation
is strictly newer. Fresh queue/battle evidence requires the complete pre-request
decoded log prefix to remain character-for-character identical, not merely the same length.

`powershell -NoProfile -ExecutionPolicy Bypass -File tools/rematch/test_training_native_pilot.ps1`
checks the actual pure validators, pinned lifecycle import, absent-path plan,
bad queue/start/mode witnesses, stale/foreign/missing-evidence approvals,
wrong-build/failed-restoration receipts, command allowlist, actual-helper scan
interleaving, replaced log prefixes and altered dependency files. These are offline
refusal checks, not a live route or universal recovery proof. Existing C72E
recovery tests cover the reused finalizer separately.
