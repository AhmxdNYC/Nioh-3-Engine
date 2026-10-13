# Fresh Codex handoff - live manual rematches

Historical checkpoint. The later [Codex-to-Claude catch-up](handoff-codex-to-claude-2026-10-12.md)
supersedes current-state claims below, including installed build, game ownership,
the subsequently resolved offline release gate, and user-overridden memory limits.
Retain the original observations below as evidence, not instructions for a new launch.

Transfer checkpoint; final gate follow-up 2026-10-12 04:39 EDT. Recheck the newest
STATUS section and actual HEAD before resuming. Main at the start of this work:
`a643d4344b3c6d5d194cb536427d7adc10122a80`. This note does not supersede
AGENT_BRIEF.md or the project's save, headroom, default-picture, A/B,
secret/public and ownership rules. Unfinished work is explicitly marked below;
this document does not authorize two leads to run together.

## Start here: preserve the user's active game

The user explicitly says their manual rematch testing is finished. This does
not prove the game process has closed: recheck it. Preserve their progress.
If the game remains open, the user owns input. **Do not close the game, send input, roll saves
back, replace ASIs, or toggle controller/wake systems.** Save policy is
`FRESH_BACKUP_PRESERVE_USER_PLAY`; automated-entry restoration is not
authorized over subsequent human progress. Physical VirtualPad=0 and
HostedFightTest=0 were the manual launch settings.

Read-only process verification at 04:18:15 EDT found:

| Process | PID | Exact local start |
|---|---:|---|
| Nioh3 | 43840 | 2026-10-12T03:49:13.4633660-04:00 |
| Lead Codex | 43708 | 2026-10-11T02:34:24.0733347-04:00 |
| Manual lifetime PowerShell | 46568 | 2026-10-12T03:48:40.7857369-04:00 |

Game executable: `C:\Program Files (x86)\Steam\steamapps\common\Nioh3\Nioh3.exe`.
Process presence proves neither current scene nor responsiveness. Lead lease
reported by latest STATUS: `pc-user-rematch-43708`; recheck read-only before
any future authorized lifecycle action. Passive screenshot observer
exec12377 finished normally. Root started one replacement, exec98980, at
04:25; it is bounded to 30 minutes. Verify completion before starting another.
Lifetime monitor exec5868 remains active separately. Neither sends gameplay
input. Each observer initially replays the last log lines; deduplicate events.

Session root: `D:/nioh3-re/rematch-pilots/user-rematch-4c33164-20261012`.
`session.json` binds PID/start, source 4c33164, Engine 1.7.71/addon 1.4.20 and
no automated input. Entry backups, passive-observation and passive-performance
are under this root. Installed-source archive:
`D:/nioh3-re/candidates/hosted-sweep-4c33164`; source tree:
`D:/nioh3-wt/hosted-sweep-dispatch`. Do not install its successors mid-session.

## Actual manual results, including the failure

Seven earlier routes have ready telemetry, native mission clear and verified
spawn restoration in the retained session evidence:

| Row | Catalog boss name | Mission clear (EDT) | Spawn restoration verified |
|---|---|---|---|
| R1CBC | Yamagata Masakage | 03:52:00.814 | 03:52:35.977 |
| R0034 | Enenra & Jakotsu-baba | 03:54:29.085 | 03:55:05.784 |
| R15FF | Takeda Shingen | 03:57:15.418 | 03:58:07.808 |
| R23E5 | Hino-enma | 03:59:30.525 | 04:00:10.931 |
| REA75 | Ibaraki Doji | 04:02:29.483 | 04:03:16.102 |
| R7B35 | Minamoto no Yoritomo | 04:05:47.860 | 04:06:33.742 |
| RA4B4 | Gozuki & Tokugawa Kunimatsu | 04:08:45.087 | 04:09:26.015 |

These are natural manual clears, not an all-boss sweep PASS or continuously
recorded first-controllable-frame proof. No mission-loading elimination or
loading/FPS speedup has been demonstrated. Multiple names on one catalog row
do not prove every actor or wave was separately observed.

One passive 25-second PresentMon sample during user gameplay ended: 2,821
frames, mean 8.858 ms (~112.89 FPS), CPU busy 8.743 ms, GPU busy 8.683 ms,
p99 12.1375 ms. No settings/input change, no controlled A/B. Raw CSV and
summary are in session `passive-performance`; the collector has exited.

**R1A5E is a hidden-death cover BUG, not a pass.** Token/attempt 8 began
04:10:06.407, posted after ACK at .424; native BossBattle started 04:10:25.878
with no matching Create in that logged placement list. Cover stayed opaque
until phase 5 at 04:10:37.719: player died during transition. Later natural
mission-clear at 04:13:21.965 and native exit at 04:13:54.433 to mission 0 /
stage 4019512 do not erase that failure. Exit says nothing was patched/nothing
to restore. No fresh exact-state/control query was taken for this handoff.

The preceding RA4B4 mission exited at 04:09:26, before this request. Failure
to leave the previous mission did not cause this case. The dojo script uses
an existing teacher actor, without the Character::Create witness expected by
the cover. Writer `training_requests` owns the isolated successor
`D:/nioh3-wt/cover-yagyu-progress`, branch `codex-cover-yagyu-progress`.
It also adds the requested loading bar instead of the secondary status text.
Successor source is committed and pushed as
`34296f3c4c2f86505387dab054b9aa15a7525ebc`; modtree
`1abd6bca26af6c5a1091e807889d5d375a8ac689`. Source writer has stopped editing.
Both builds and independent focused review passed: 42 dojo cases, 48 branch,
73 cover, 200 native-reader, 37 Present, thunk, 20 native-anchor checks and
both actual menu harnesses. Original failure reproduced before correction.
Engine 1.7.73 SHA256
`21ce6ee58663dca6b0de2d7f2e3fc4750973bfd852cddd913abde8c622525d85`;
addon 1.4.21 SHA256
`9145baebfa46b05825c1ced1a8a619ff2994cf645b9f587f2f07911f7a58cfad`.
Archive `D:/nioh3-re/candidates/cover-yagyu-34296f3` retains its original
manifest initially marked RUNNING. **The full gate subsequently completed
FAIL: 2 errors, 3 warnings.** PID49876 exited; do not keep waiting for it.
Raw log `D:/nioh3-re/reviews/cover-yagyu-progress-full.txt` and supplemental
archive `full-release-observed-result.json` preserve this result. Errors are
`run_replayprof_harness.py` (rc1, last line "harness") and
`run_thrsched_harness.py` (rc1, empty captured stdout). The latter's source
explicitly refuses to run while Nioh3 is present; the full checker discards
stderr, so this is a likely guard refusal, not a verified direct-run diagnosis.
Replayprof's exact failure remains unknown. Preserve the failure and obtain
full output from these two checks once the game has actually closed; do not
silently waive them or rerun the entire suite without a reason.
**UNFINISHED:** resolve the full-gate errors and native acceptance. Builds and
focused tests do not make this release-ready. Current installed build remains
4c33164. Do not swap binaries over the user's running game.
Do not infer acceptance from a successful build or from the eventual clear.

Primary evidence: live `Nioh3BossRematch.log` beside the game; session
`passive-observation/events.log` and `captures.jsonl`. The earlier four-route
snapshot receipt is `passive-digest-20261012T080011104575Z`, SHA256
`619f3ada9ff78bd99574e80f6e09a11dac75a2ab5c12f727158b2b38c78ed0b9`.
Root's stable failure snapshot is session
`yagyu-cover-death-20261012-041853`, receipt SHA256
`42e8e81a94193ef3556b0d5084ca79da6540a761a92a2a59f408d5e7c889a6ba`.
The user defeated the boss at 04:13:06; initial cover failure remains unchanged.
Root generated and hashed this stable snapshot using shared read-only handles.

## Next work and outstanding external state

1. Finish and review the R1A5E hidden-death fix; root coordinates any later
   install/restart with the user. Preserve raw failure evidence.
2. Training remains unfinished: combined `c6d9abe` Summon implementation is
   unrun. Independent Yamagata arena transfer is not implemented. New static
   checkpoint `a248218` in `D:/nioh3-wt/all-boss-cover-audit` proves destination
   owner inheritance but not exclusion against teardown/concurrent spawn
   producers. Read `notes/training-destination-owner-inheritance.md`; the
   remaining serialization/lifetime gate forbids guessing a factory context.
3. Static loading checkpoint `14afb92` closes graphics error-callback and
   texture-stream boundary evidence; it is not a runtime optimization or
   measured saving. Claude is blocked by the monthly spend limit; do not
   bypass the limit or claim its old process is still doing useful work.
4. PC2 completed AABB 0/1/0 (message 2249/file 267): three 25-second arms
   reported 39.0154 / 39.0655 / 39.0740 FPS, only +0.0532% versus the OFF mean:
   **no gain claim**. A quiet-JSON marker bug invalidates isolation; corrected
   future kit 268 has not run. Independent local archive review recalculated
   971/974/973 frames and the same means. Twelve live witnesses are consistent;
   twelve restoration hashes match retained entry manifests. Remote disk state
   remains receipt-backed. Download under `D:/nioh3-re/deliveries/`:
   `pc2-aabb-a1-b-a2-267.zip`, SHA256
   `b57233d5a006f0966f3dad3a0d8a38f3ffa316546fcceffaac3331cc7df12819`.
   Marker correction `pc2-aabb-marker-fix-268.zip`, SHA256
   `31512cd7870d085c455a4356a1b908b26664ff89c1cf251c550c211732383894`.
   Its narrow diff uses Set-MeasureLock JSON and readback; three-check receipt
   passes, test body not supplied. Do not repeat negligible AABB timing.
5. PC2 task2254/ACK2257 prepares actual boss tests using paired kit265:
   R1DEA if cleared, otherwise R1CBC, then R6E65/RD22D/RB3F8 where available.
   Visible boss/control, native leave, settled original mission before next;
   first uncertainty stops. No R1A5E before the fix. ACK is preparation, not
   evidence of a game launch. PC2 supplied review-ready adapter269 in message
   2262; no launch yet. Download `D:/nioh3-re/deliveries/pc2-hosted-routes-adapter-269.zip`,
   SHA256 `781fac1d45de1b8b3a410bf931f672f376e26b03d013b117e0cf325d57a1fa24`.
   Review its new route/cover/control/return adapter and real transaction tests
   before issuing a fresh exact admission; old runner hardcoded R1CBC.
   Latest fully read bridge cursor is **2262**.
6. Four rendering families await actual native/draw/image/A-B gates: combined
   PB2 pixel, PS05ad pixel, PB2 VS, lodu. MIC needs exact native capture;
   PSa11 is static only; fog proof passed but timing was neutral. No new proven
   FPS gain. Ally task2247 specifies file257 Proton PSO admission and short
   A1/B/A2 under its reviewed adapter; no fresh Ally ACK yet. All opportunities
   have not been exhausted. Do not change default picture or hardware/OS.
7. New agent `phone_delivery` investigates phone routing in universal-optimizer.
   Phone messages2252/2255/2258 reached the bridge; root answered2260. Current
   automatic receipts mean queued, not consumed by the interactive lead.
   Initial source diagnosis: runner queues phone-inbox.json but does not inject
   into an active interactive session; transcript pointer only mirrors output.
   Fix/health tooling is in isolated
   `C:/Users/Ahmad Hamza/universal-optimizer-phone-delivery`, committed as
   `208c7559ebaec7e8474eb0c03638de72b6f1a8e1` (clean, local only, not deployed).
   New `uo_bridge.phone_delivery` provides
   bind/inbox/ack/health with exact transcript/session/PID/start validation,
   separate atomic consumption receipts and a read-only bridge supplement.
   Tests: tools74, bridge72, nexus32, controller18 passed; final helper14 passed.
   Agent stopped. Five owned files; README lines34-36 contain the new-session
   commands and lines49-53 explain deployment limitations. Helper integration
   and exact new-session binding remain to do. No service restart or
   controller-state mutation has been performed.

## New chat and phone focus

The user will create the fresh chat themselves. **No new chat has been created
by this lead.** Start with model `gpt-6-astra`, this repository and this note;
do not fork the entire old context. The old session is
`01a12cb5-4711-7a42-8bf9-c53cb49dcbcd`. Phone currently mirrors that old session.

Before claiming phone focus transferred, read the phone agent's final note and
actual commit in the isolated tree above, integrate/review the bounded helper,
and bind the actual NEW session UUID and transcript using its documented
command. Run its health check and verify exact session identity. Do not invent
the new ID, acknowledge unread messages, or mistake a queue receipt for a lead
reply. User explicitly wants the phone focused on the new chat. The interactive
lead must check its phone inbox at turn/work checkpoints; this change does not
magically inject messages into an already active Codex turn. Wake/controller
enable flags and existing holds remain unchanged.

Old source workers have stopped. Check their actual worktree HEADs and the
existing full-gate output before taking source ownership. Continue the first
concrete task: finish the rematch release check, preserve the user's game, then
plan its next native Yagyu entry/control/return test. Broader work stays active.

Automatic sweep remains separate and unaccepted. Offline opt-in camera/return
driver `b0bdd29` plus helper `4fa6304` produces pending-root-review receipts,
not automatic PASS. Same-mission return limitation remains: TrackReplay can
prevent cleanup when destination equals original mission; retain the skip.
Do not resume automated testing over this manual session.

## First commands (read-only PowerShell)

```powershell
Set-Location -LiteralPath 'C:\Users\Ahmad Hamza\nioh3-engine'
git rev-parse HEAD
Get-Content -LiteralPath '.\AGENT_BRIEF.md'
$status = Get-Content -LiteralPath '.\STATUS.md' -Raw
($status -split '(?m)^## UPDATE ', 3)[1]
Get-Content -LiteralPath 'D:\nioh3-re\rematch-pilots\user-rematch-4c33164-20261012\session.json'
Get-Process -Id 43840,43708,46568 -ErrorAction SilentlyContinue |
  Select-Object Id,ProcessName,StartTime,Path
Get-Content -LiteralPath 'C:\Program Files (x86)\Steam\steamapps\common\Nioh3\Nioh3BossRematch.log' -Tail 40
```

Then read root's finalized successor/failure receipts above. These commands
do not authorize any input, install, close, recovery, or lease mutation.
