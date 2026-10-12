# Main-PC Yagyu single-route native gate

Preparation only: no launch, install, save/config mutation or lease action.
Owner43708 remains `2026-10-11T02:34:24.0733347-04:00`; no Nioh3 was observed.
User2361/2352 permits concurrent devices through Steam Play anyway; local
`nioh3-pc` ownership remains required. No shared-account exclusion is needed.

Concrete preparation directory:
`D:/nioh3-re/reviews/yagyu-native-readiness-20261012` contains readiness.json,
frozen-manifest.json and **unapproved** admission-draft.json. Eight current save
files were hashed; this is an inventory, not the fresh prelaunch backup.
New intended output is
`D:/nioh3-re/rematch-pilots/yagyu-34296f3-20261012-root1` (currently absent).

Candidate is isolated at `D:/nioh3-wt/yagyu-native-34296f3`, detached exact source
`34296f3c4c2f86505387dab054b9aa15a7525ebc`, modtree
`1abd6bca26af6c5a1091e807889d5d375a8ac689`. Only archived binaries were copied
into this checkout's ignored bin folders; installed game was not changed.
Original cover-yagyu-progress now contains harness commits, so do not use its
HEAD with the old34296f3 frozen manifest.

| Pair | Engine | Addon |
|---|---|---|
| Candidate1.7.73/1.4.21 | `21ce6ee58663dca6b0de2d7f2e3fc4750973bfd852cddd913abde8c622525d85` | `9145baebfa46b05825c1ced1a8a619ff2994cf645b9f587f2f07911f7a58cfad` |
| Actual installed4c33164 entry | `ce7aa75fa00206490ff9bb3245496d972c294cb7f8f572aeecfc5b430563a597` | `5348c108bf87e7782c14feadafc0c9708bc90c891ab585cc80646ba19eb2ef4e` |

Current Engine INI hash is
`fe94fe798c3ad1e0bf90099f6d9ced85521fb869365b2848cec7a534a7c506b1`;
addon INI `a5351fe17015e06818eb4072ecc754d11f6d0f0bb8a396ea3ae1e01dc01acd43`.
Physical VirtualPad0/HostedFightTest0 remain installed. Full offline gate is
PASS0errors/3existing warnings/255sites/36harnesses; immutable receipt is
`D:/nioh3-re/candidates/cover-yagyu-34296f3/full-release-pass-38b56f0-20261012.json`.

The old scoped R1CBC root4 pilot is TRIAL_FAILED (pipe timeout) with restoration
verified; never resume its admission/output. The subsequent manual session
user-rematch-4c33164-20261012 records USER_EXITED08:38:15Z. Preserve all progress
from that session. The historical R1A5E hidden-death failure remains a failure.

Use existing generic `all_boss_cover_pilot.ps1` in
`D:/nioh3-wt/all-boss-cover-audit/tools/rematch`, singleton `Rows @('R1A5E')`,
manual root review. DryRun and all eight lifecycle dependency pins passed.
It resolves exact tuple mission0x1A5E/start5021112/placement5027806, atFight=true,
canonical actor5027806 and90s cover budget. Do not use the scoped Yamagata driver,
PS7dc tooling, or AutoCameraReturn (only R1CBC/R1DEA are supported there).

Before minting approval, root rechecks exact owner/start, no game/competing local
runner/recovery hold, current eight saves and baseline hashes, absent output,
and `nioh3-pc` availability. The existing driver acquires the PC lease itself;
do not hold a conflicting root lease. It checks effective commit headroom>=18GB
and current>=6GB without changing memory settings. Fresh backup is taken and
protected after lease acquisition, before any ASI/INI staging. Its restore uses
only that NEW current entry, including latest user progress. Never copy the
old manual or root4 save snapshots over the current saves. Do not allow human
progress during this bounded automated-entry trial; manual play needs a distinct
preserve-user-play session instead of its revert finalizer.

Root issues a new explicit approved admission from the draft only after review,
refreshing nonce and created/expires timestamps (<=30min), saving as admission.json.
The frozen manifest SHA256 is
`56177f867c01d4fc0f3366511eb09c414e5b9790843c287dc3c342f7ba2fdd7c`.
Then this single invocation is applicable (not executed in preparation):

```powershell
$driver='D:/nioh3-wt/all-boss-cover-audit/tools/rematch/all_boss_cover_pilot.ps1'
$prep='D:/nioh3-re/reviews/yagyu-native-readiness-20261012'
$out='D:/nioh3-re/rematch-pilots/yagyu-34296f3-20261012-root1'
& $driver -Run -WritableSaves -Rows @('R1A5E') -Catalog 'D:/nioh3-re/candidates/cover-yagyu-34296f3/Nioh3BossRematch_bosses.tsv' -FrozenManifest "$prep/frozen-manifest.json" -AdmissionReceipt "$prep/admission.json" -CandidateRoot 'D:/nioh3-wt/yagyu-native-34296f3' -LifecycleRepo 'C:/Users/Ahmad Hamza/nioh3-engine' -SaveRoot 'C:/Users/Ahmad Hamza/AppData/Local/KoeiTecmo/NIOH3' -OutDir $out -OwnerPid 43708 -MaxMinutes 20 -ReviewSeconds 90
```

Use a process-local PowerShell `-ExecutionPolicy Bypass` wrapper as in existing
pilot instructions; change no persistent execution policy. Root supplies actual
Continue/field review, then the exact fresh awaiting stage approval:

```powershell
& $driver -OutDir $out -OwnerPid 43708 -ApproveStage FieldReady -Evidence '<actual current field screenshot>'
& $driver -OutDir $out -OwnerPid 43708 -ApproveStage ControlReviewed -VisibleArena -VisibleBoss -ControlResponse -Evidence '<actual Yagyu and before/after control images plus same-token existing_actor witness>'
```

Field must be offline, alive, continuously settled12s and a known mission OTHER
than0x1A5E. Same-mission/uncleared skip is not a successful Yagyu test; the generic
sweep can exit0 with explicit skips, so root must inspect the actual R1A5E result.
One hostfight only: exact serial/session/row -> new cover token -> drawn ACK ->
native post -> same-attempt existing_actor event with actor5027806 -> ready.
Capture the loading activity bar and first visible arena/control response; a
phase4 sample alone is historical and never proves visible control. The generic
oracle checks exact actor/token/order but does not require existing_actor by
name: root must inspect that additional fix-specific witness explicitly.

After actual named Yagyu/arena and observed control, approve only that fresh
nonce/process-bound ControlReviewed request. Driver then settles the native
return gate and submits one native exit. Require original mission/stage return,
spawn restoration if a lease was actually owned (atFight can own none), live
idle/player/loading0 state, and final current-entry save/INI/ASI restoration.
Do not approve from old screenshots, skip/failure status or eventual death/clear.
First uncertainty stops; no retry. Retain guard on cleanup failure and check
actual lease release separately from its pre-release restoration receipt.

Native acceptance remains false until this evidence is reviewed. Root must be
available for the initial field and bounded control review; this is not an
unattended capture/approval path.

## Root2 after the operator startup timeout

Root1 ended SWEEP_STOPPED / "Root review timed out" with zero dispatched rows.
This is not a Yagyu candidate failure or acceptance. Root reported that a late
owned `press A 120` finally started Continue after physical Enter had not done so.
Independent readback matched all eight current save files to BOTH original
readiness and root1 entry manifest, both INIs to entry backups and baseline pins,
and both installed ASIs to baseline pins. Owner/start unchanged; game absent.
Root reports task15 free; this static helper did not query or mutate the lease.

Corrected preparation is `D:/nioh3-re/reviews/yagyu-native-root2-20261012`:
same frozen manifest, separate root1-independent-restoration.json and fresh
root2 admission.json (SHA256
`4343ff2bbf08a3bf567687edc94b8fe0e989d6af7f6abf82ad7531cf74acfcba`,
expires2026-10-12T10:13:20Z). Root requested this fresh single run; it has not been
launched by the preparing agent. Use the prior launch command with this prep
directory and output `D:/nioh3-re/rematch-pilots/yagyu-34296f3-20261012-root2`.
No driver/source/hash changes and no route broadening.

Narrow input helper:
`D:/nioh3-wt/cover-yagyu-progress/tools/rematch/yagyu_owned_input.ps1`, SHA256
`29b436f6760ee58d07141814d29e4b763336f4975a50232db8b5f7b8851b0f61`.
It accepts only root2, binds the current awaiting.json hash/nonce/stage, retains
exact owner/runner/game process handles and creation times, requires the exact
game executable and foreground, verifies each named-pipe server PID, and limits
each pipe operation. It never focuses, arms/disarms, launches, approves a stage,
returns, repeats input, or changes INIs. Every attempt retains an input receipt.

Run from a separate process-local PowerShell ExecutionPolicy Bypass shell:

```powershell
$inputHelper='D:/nioh3-wt/cover-yagyu-progress/tools/rematch/yagyu_owned_input.ps1'
$out='D:/nioh3-re/rematch-pilots/yagyu-34296f3-20261012-root2'
& $inputHelper -RunDir $out -Action State -Run
# Only after actually seeing the title/Continue selection needing confirmation:
& $inputHelper -RunDir $out -Action PressA -Evidence '<current selected-Continue screenshot>' -Run
# At fresh R1A5E ControlReviewed, after visible named boss and exact token/attempt:
& $inputHelper -RunDir $out -Action Camera -Token <actual-token> -Attempt <actual-attempt> -Evidence '<visible boss and before image>' -Run
```

PressA sends exactly120ms and refuses already-loading/field states; Camera sends
exactly180ms right-stick0.35 after exact current encounter and live pad-state
checks. Root captures/compares actual before/after images and then separately
approves ControlReviewed; pulse acknowledgement is not visual acceptance.
A timeout or uncertain acknowledgement never authorizes an automatic repeat.
Historical cover telemetry is identity evidence; the helper separately rechecks
live field/loading/pause state and limits check-to-pulse age750ms. Root remains
responsible for actual visual life/control evidence and fix-specific existing_actor.

Offline helper checks passed: PowerShell parse, exact/missing/duplicate telemetry
field parsing, mutation-free plan mode and old-root1 rejection. No pipe or native
input was invoked in validation. Current scene observations and prompt operator
work remain required during the five-minute FieldReady window.

## Root2 final outcome: restored, main PC idle, no retry

Root2 ended SWEEP_STOPPED / "Root review timed out; no automatic approval";
its rows array is empty. Guard completed05:57:22EDT. Independent post-run
readback matched all8 saves to both root2 prelaunch and the original latest-user-
progress inventory, both INIs to entry backups/baseline pins, and both ASIs to
baseline pins. Exact game5468/runner45716 are absent. A separate read-only bridge
query returned nioh3-pc task15, holderNone, no reservation and empty queue.
Do not interpret result.json's pre-release PENDING marker as a still-held lease.

Receipt:
`D:/nioh3-re/reviews/yagyu-native-root2-20261012/root2-independent-restoration.json`,
SHA256 `e566261884ce109195cf45d06eea7bac161377d38abd06bcd36f8861c44892d1`.
Native Yagyu remains UNTESTED; this was another operator startup/review timeout,
not a dispatched route or candidate failure. No retry/admission refresh was
performed after root2. Main PC is idle; next native attempt is pending a prepared
operator boot/control sequence, not running in the background.

Retained input receipts show title/Continue `press A120` acknowledgements at
09:53:37Z and09:54:29Z. By09:55:04Z, state was loading1/player1/field0/menu_pause1/
paused0/armed1/foreground1. Root reported loading.png/field.png visibly showing
the load-tip Space Continue prompt. Generic PressA correctly refused loading1
at09:56:04Z. The subsequent narrow LoadContinue extension (commit18eedd0,
helper SHA256 `69eaba281e6204f294a741d86e4cf67393d4fd9f9e08a308c523ca710aa4b8e1`)
allows only the exact observed tuple at FieldReady/ENTRY, checks it twice, and
sends one A120. Ten actual guard tests passed, but no LoadContinue execution
receipt exists: successful dismissal is still unproven. Retain this distinction.

The next candidate remains unchanged34296f3/Engine1.7.73/addon1.4.21. Before
another deliberately authorized attempt, prepare all boot steps and avoid a
context-compaction/operator handoff during the five-minute FieldReady window:

1. Bind a NEW output/admission and fresh current-entry backup. Rebind the helper
   to that exact new output and repin its reviewed hash before launch; root2's
   helper/output/admission must remain consumed historical evidence. Recheck
   owner/start, lease availability, ASIs/INIs/saves, and no game/runner.
2. Root handles the actual Steam Play anyway prompt if displayed. Once the
   exact owned game is foreground, observe the title prompt, query State and
   send one PressA120 only if needed. Then observe selected Continue and send
   one separate PressA120. These are observed discrete actions, never a blind
   sequence or repeated button loop.
3. During the initial load, use bounded state observations. On the visibly
   confirmed load-tip prompt AND exact loading1/player1/field0/menu_pause1/
   paused0 tuple, perform one LoadContinue120. Capture the acknowledgement and
   actual next screen. If A does not dismiss this native prompt, stop for
   review; do not improvise a repeated A/Space sequence or loosen predicates.
4. Observe the actual field and current loading0/player1/offline gate. Only
   then approve the fresh FieldReady request; the driver independently requires
   a known mission other than1A5E and12s settle, fresh clear scan, and one hosted
   request. Do not use the queued/load-tip screen as FieldReady proof.
5. Attend ready transition/control review immediately. Require existing_actor
   with exact new token/attempt/actor5027806, actual visible named boss and
   before/after control evidence, then one native return and verified current-
   entry restoration. Use the same root-reviewed Camera action only after its
   exact-session rebind; no automatic approval.

This is an actionable next-run plan, not authorization to retry now or proof
that the entire boot sequence has completed successfully under this helper.

## Root3 explicitly requested and prepared after root2 restoration

Root subsequently requested one corrected fresh root3, with bounded600s
FieldReady while retaining20min global lifetime and90s gameplay review.
Preparation is isolated in
`D:/nioh3-re/reviews/yagyu-native-root3-20261012`; candidate/installed game
source was not changed. All8 current saves/2INIs/2ASIs still matched preserved
user progress/baseline, exact owner/start remained live and no game was observed.

Admission SHA256
`6b5302d56f2124470439a8be595b528b46778eaece9b66d26bbb84d69c19321d`,
expires10:28:46Z. New output:
`D:/nioh3-re/rematch-pilots/yagyu-34296f3-20261012-root3`.
Frozen manifest and candidate34296f3 pair remain identical. Support driver
`support/all_boss_cover_pilot.ps1` differs from its reviewed original only by
FieldReady300->600; SHA256
`08ebb76ec8779a2496ab60924c9336dddd3dae2e45b1c79e1c84ca096944c706`.
Its library/oracle are byte-identical copies. Input helper differs from the
LoadContinue root2 helper only by its exact root3 session binding, SHA256
`26987f6c8fd8f5f04d586c35641d39a56e16c779dfdf08df7150c64e2a88229f`.

Root's individually observed launch/Steam/titleA/ContinueA/load-tipLoadContinue/
field approval/camera/control approval commands are in RUNBOOK.ps1.txt beside
the admission. Do not execute the runbook as a blind macro. Root must attend
actual screens, capture evidence and handle uncertain states before proceeding.
The isolated helper retains exact awaiting owner/runner/game identities, exact
named-pipe server PID, foreground and no-repeat behavior. It never focuses,
launches, changes settings, approves a stage or skips return/restoration gates.

Offline checks passed for PowerShell parse, all dependency pins, actual exact
admission, actual timeout expression600s/90s, DryRun20min/singleton/no-retry,
ten real load-tip guards, duplicate/missing telemetry, mutation-free plan and
old-session rejection. Check scripts are retained in the prep directory.
No root3 game launch or input occurred in preparation; native acceptance is
still false. Root may execute the explicitly requested corrected attempt after
fresh launch-time checks; do not describe it as running before it actually starts.

## Root3 and root4: reveal observed; control and return still incomplete

Root3 token1/attempt1 observed existing_actor5027806 at06:15:32.207,
phase4 at32.738, and Engine transition-cover closed at32.741.
`R1A5E-before-control.png` shows the named Yagyu, dojo, and live player
2207/2421 taking214damage. Death/reload began06:15:53.043. This is positive
native reveal evidence, not control-response or native-return acceptance.
Root3 restoration receipt:
`D:/nioh3-re/reviews/yagyu-native-root3-20261012/root3-independent-review.json`,
SHA256 `9f454a6d68c0f9dd6ca5e6115fa512a39eff55cfe6fd0cee4d9ea399ef9f59a7`.

Root4 again revealed the correct actor and arena. Its one camera180ms pulse
was acknowledged44rounds. The before image already shows330/2421HP, and the
after image shows the death transition. Therefore the image pair does not
prove ordinary player camera control. Neither root control approval nor
native return occurred; result was FAILED_NO_ADVANCE / SWEEP_STOPPED.
The historical cover sample still reports playerAlive1 after visible death;
never use that retained sample as current health evidence.

Root4 exact8saves matched its fresh entry backup and original user progress;
2INIs/2ASIs matched entry and baseline. Game16624 and runner47276 were absent,
lease task15 had no holder/reservation/queue. Receipt:
`D:/nioh3-re/reviews/yagyu-native-root4-20261012/root4-independent-restoration.json`,
SHA256 `95d897a4fdfa9d9eeb374af9d7b72cfe6aad8537f5ea9c1fd1cc3010c986f027`.
Both final logs were also preserved under that review directory's volatile-final.

Root4 local timing (2026-10-12, UTC-4):

| Event | Time | Implication |
|---|---|---|
| Engine closes cover |06:34:55.324|Native reveal began|
| Driver observes phase4 |06:34:56.055|731ms observation delay|
| Driver desktop Shot completes |06:34:58.289|Capture itself994ms; this is CopyFromScreen, not dotnetfsi|
| ControlReviewed file created |06:34:58.332|3008ms after reveal|
| Redirected child stderr created |06:34:58.672|Watcher dispatch began|
| Child receipt body starts |06:35:01.067|2395ms child startup/compile/identity overhead|
| Before PNG write |06:35:01.534–01.888|Player nearly dead already|
| After PNG write |06:35:02.436–02.547|Death transition visible|
| Native death/reload gate |06:35:08.311|Later than actual visible death; not a survival deadline|

The watcher's reported failure was a separate PowerShell exit-code bug.
Offline exact redirected Start-Process/WaitForExit/Refresh reproduction gave
ExitCode=null on3/3 successful children. Retaining child.Handle immediately
made all3 report0. Unredirected controls also returned0. A null code must be
UNKNOWN, never conflated with a known nonzero exit. Evidence is retained in
`child-exit-redirect-offline-probe.json` beside the root4 receipt.

New source tools are preparation only, not a fifth admission:
`yagyu_camera_watcher.ps1` preloads identity/capture/input types before marking
itself ready, then invokes `yagyu_camera_witness.ps1` in the same PowerShell
process at the exact admitted ControlReviewed nonce/hash. It retains the
admitted output/owner, all3process-start identities, foreground, exact pipe
server, token1/attempt1, current route, existing-actor witness, and750ms fresh
state bound. It permits only one camera180ms and records capture/input times.
The total witness budget is4s and each PrintWindow is bounded1500ms. It never
approves, returns, focuses, pauses, retries, or changes game settings.
`yagyu_child_process.ps1` contains the handle-pinned, explicit-UNKNOWN fallback
for callers requiring a subprocess; the preloaded watcher does not use it.

Offline tests passed parsing, admission scope/expiry/output refusal, absent
awaited-file transition versus malformed data, exact binding rejection,
invalid capture handle, preloading, redirected child success/nonzero/timeout.
Idle-host cold preload about0.4s versus warm about8ms is diagnostic only, not a live
latency guarantee. Tests and evidence: `yagyu_camera_watcher_test.ps1` and
root4 review `preloaded-offline/result.json`. No game access/input/capture was
performed by these tests. No retained root4 script or receipt was overwritten.

Before another native attempt, review a driver change that publishes the
ControlReviewed binding immediately after current phase4 verification, before
its redundant desktop screenshot. The preloaded witness should then own the
before/after captures. Measure the actual phase4-to-input interval explicitly.
Even with this improvement, manual root image review plus the driver's return
settling can outlast this boss's attack sequence. An ordinary native pause
could provide review time only if separately reviewed and authorized, with
observed pause/unpause state and fresh identity checks; no pause command is
implemented or authorized here. Alternatively a separately reviewed narrow
capture-and-return protocol would require explicit root authorization.
Do not launch a fifth trial under unchanged timing assumptions. Native
acceptance remains false; root3/root4 reveal evidence remains useful.
