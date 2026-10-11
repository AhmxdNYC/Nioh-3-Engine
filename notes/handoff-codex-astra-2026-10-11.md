# Nioh 3 — handoff to a fresh Astra lead

Updated 2026-10-11, America/New_York. Private project document; never copy this
file or this repository's history into the public repository.

## Start here

Work in `C:\Users\Ahmad Hamza\nioh3-engine`, not the Roblox directory carried
by the old IDE workspace. Read this document, the newest section of `STATUS.md`,
`AGENT_BRIEF.md`, and `C:\Users\Ahmad Hamza\universal-optimizer\docs\token-efficiency.md`.
Then read only the note for the next task. The original
`notes/codex-handoff-prompt.md` is useful background, but its initial merge and
device queues are already completed or superseded; do not repeat them.

Ahmad wants CPU and GPU optimization for ordinary midrange PCs and the Ally,
controller-friendly settings with submenus, rematches that start in the actual
fight, and eventually a playable enemy-spawning/stage-changing training mode.
He authorized multiple agents, bridge coordination with the existing device
Claude chats, graceful game closure for tests, Discord result cards, and Codex
continuation. He explicitly said **recording finished**. His latest request is
a good handoff and a **new chat using `gpt-6-astra`**.

The prior chat's locally recorded model was `gpt-6.1-sol`. Do not describe that
chat as Astra. The installed Codex model cache lists `gpt-6-astra`; the new
session must request it explicitly and verify its actual session metadata.

## What is actually done

| Area | Result and scope | Evidence / checkpoint |
| --- | --- | --- |
| Stable performance release | `cbv-upload` and `extra-perf` integrated as EngineFix **1.7.61**. Initial 255 pattern sites, 36 harnesses, full release check and PC/device component smoke checks passed. Constant upload is default on; quality presets remain opt-in. | `notes/handoff-codex-step1.md` |
| Constant upload A/B | pc2 component run **+16.50% ±0.16% SE**; PC **+0.55%**, Ally **+0.19%**, both neutral. pc2 has an unusually slow chipset GPU link; its gain does not predict ordinary x16 PCs. | Same step-1 note and device receipts |
| Opt-in Performance preset A/B | PC **+3.14%**, Ally **+4.16%**; pc2 neutral. These change quality, and were tested with CBMODE 0. Do not add these numbers to the constant-upload result. | Step-1 note |
| Frame overlap | Experimental availability/startup and deferred-stream hazards researched and partially repaired. Producer closure, ordering and resource lifetime still block release. The artificial 8 ms lag result is not a native-device FPS gain. | `notes/renderer/frame-overlap.md`, `notes/handoff-codex-step2b.md`; **FO stays NO-GO/default OFF** |
| Installer safety / PSO guard | Isolated private guard candidate built and offline-checked; pc2 evidence covers title-menu installation only. Stable baseline retained. | Private draft PR `https://github.com/AhmxdNYC/nioh3-engine/pull/1`; **unmerged** |
| CPU AABB control | Actual-source native control compiled `/W4` and passed. It reproduced misleading ACKs when a cave was absent/skipped, then verified 7 installed/OFF live readbacks without changing other keys. No live-game or FPS result. | `84ea5f5`, `tools/bench/test_cpu_ready_aabb_control.py`, `notes/handoff-codex-cpu-ready-aabb.md` |
| GPU fog candidate | Full fresh input-bound interpretation **PASS**: 2,400 cases, 638,976 writes, zero strict trace/final-resource differences and max ULP 0. Coverage/holdout both hit all 137 blocks. Altered inputs and output mutations rejected; all 19 original hashes unchanged. Still no native GPU/image/FPS acceptance. | `f825870`, full manifest below, `notes/handoff-codex-fogpar-bound-validation.md` |
| Private test hub | 71 entries inventoried; root reran the five allowed offline entries: **5/5 PASS**. Native, release, dump and device lanes are explicit plans, not automatic launches. Map travel already exists as `tools/tour/run_tour.ps1`. | `6f9a786`, `tools/testhub/README.md`, `scratch/testhub-offline-20261011.json` |
| Rematch diagnosis | User's RC72E request really landed at the shrine before the gate/intro placed the player in the fight. The old PASS meant eventual BossBattleMode, not first controllable arena landing. A request-only native arena-start candidate and isolated A/B catalog kit are ready. | `7debfd6`, `notes/handoff-codex-c72e-native-arena.md` |
| Settings/controller candidate | Forward-port keeps EngineFix 1.7.61 core and native rematch/training addon lineage. Catalog proof and both actual-source C++ menu harnesses pass. Private candidate only; not merged/built/deployed/live-tested. | Agent checkpoint below |
| Training groundwork | Private addon 1.4.6, default OFF, copies bounded native observations with validity/generation/loss metadata. Offline source/pattern/build checks passed. **No playable arbitrary spawn, stage switching, revive/despawn or reward-free practice acceptance.** | `0b25b2b`, `notes/handoff-codex-training-events.md`, `notes/training-mode-progress.md` |

This is work completed since taking over, not evidence of a new broad FPS jump.
Default visuals must remain identical; every new performance claim needs its
own controlled A/B, launch-level SE and device/scene scope.

## Runtime, ownership and installed state

- **No local game is running.** Recording PID 18068 closed gracefully after the
  user confirmed completion; recording watcher ended and the shared game lock
  was observed free. No forced termination or rollback of user progress.
- Do **not** restore the old recording-entry backup
  `D:\nioh3-re\save-backups\20261011-153623-user-recording`.
  The user played after it. Make a fresh current-entry backup before any pilot.
- Installed game is still BossRematch **1.4.0** and EngineFix **1.7.32**, not the
  stable repository binary or the settings candidate. Preserve this distinction.
  Game directory: `C:\Program Files (x86)\Steam\steamapps\common\Nioh3`.
- Installed SHA256s: BossRematch
  `02D763E76037365E8814820FC1EF9D3D713710435F63099D04D617E05966D4D1`;
  EngineFix `DD8B5739062E364DFC708BD1F0F89DFC2B97477D66AE1FBFDFB6F50B0D4F2891`.
- `%LOCALAPPDATA%\nioh3-codex\lead-active` is **held**. The enabled continuation
  monitor was observed PID 22568, `worker_pid=null`, with no invented daily caps.
  Keep the hold while an interactive lead, shader proof or source writer owns
  work. Do not release it just to create an overlapping successor.
- The full fog proof **finished successfully** at **16:41 EDT**, 1,010.679 s.
  Old exec session 56226 exited 0; its observed Python PIDs 17256 /49484 are
  absent. Manifest: `scratch/fogpar-bound-full-20261011.json`, SHA256
  `a900ae644883792e7905b7f96f68b7466b039a5ba11d83b00055d71eb5780a08`.
  No shader bytes or output values are stored in this manifest. It remains
  private and is ignored by Git; the committed note records its digest.
- Latest headroom check after game exit passed the existing guard thresholds;
  recheck immediately before a new native launch. Do not override guards.

## Candidate branches and writer handoff

Private main is **84ea5f5** before this handoff checkpoint; completed topics are
pushed. Inspect the new handoff commit rather than assuming this hash is HEAD.
Other important roots:

| Location | Purpose / integration rule |
| --- | --- |
| `C:\Users\Ahmad Hamza\universal-optimizer` | Shared tooling, notifier, bridge, Codex continuation; main 708edb3 at this checkpoint. |
| `C:\Users\Ahmad Hamza\nioh3-public` | Public staged repo, main 76d85ac, untouched this turn. Read the public handoff before any edit. |
| `D:\nioh3-wt\rematch-instant` | Native v4 reference `b2cfe097`; readonly donor, not a settings source replacement. |
| `D:\nioh3-wt\codex-training-pump` | Addon 1.4.6 `0b25b2b`. Its old EngineFix 1.7.58 must not replace stable core. |
| `D:\nioh3-wt\codex-controller-menus` | Earlier UI donor `88af4b9b`; old addon 1.3.4 **must not be deployed**. |
| `D:\nioh3-wt\codex-rematch-native-arena` | Tools-only native-start preparation `f602021`, already cherry-picked into main. |
| `D:\nioh3-wt\c72e-pilot-harness` | Branch `codex/c72e-pilot-harness`, separate guarded pilot tooling; final checkpoint below. |
| `D:\nioh3-wt\codex-settings-menu-v4` | Sole ASI writer's branch `codex-settings-menu-v4`; final checkpoint below. |

Settings worker's final checkpoint: **307 Engine keys** (289 shipped +18 optional),
**24 addon keys**. Supplementary editor covers **289 /19**; **18 /5** are explicit
INI-only gaps for paths/lists, reserved mutation or harness inputs. Defaults,
Engine enums/kOrder/Remote semantics and
native-v4 routes/training source remain unchanged. Existing supported live
controls remain live; supplementary changes are clearly next-launch settings.
Addon settings remain accessible with Rematch OFF. Both real-source menu
harnesses compiled `/W4` and passed without warnings **before final metadata,
version and text edits**; rerunning on final HEAD remains a gate. Final light
catalog/default/protected-source, menu-map and diff checks passed. Candidate versions:
EngineFix **1.7.67**, addon **1.4.7**; remaining gates include patterns, full
release/harness integration, release build, controller/live smoke and exact
artifact review. Nothing from this candidate is installed.

Both writers **checkpointed and stopped**; root pushed both candidate branches:

- Menus: **08705302b865e37da1195c4ba9540604fd88c98a**, preceding alignment
  **e1ce9b632f973f5b708c598363e7ee4b36ad381d**. Clean worktree; 25 changed files.
  Read `D:\nioh3-wt\codex-settings-menu-v4\notes\settings-menu-v4-candidate.md`.
- Pilot harness: **3a61cb83d2eb55a91e71ecc782db1bf10025e82f**. Clean worktree;
  **20 fixture checks PASS**, including actual Windows save sharing/restore,
  and both A/B read-only plans PASS. Installed INI/catalog hashes unchanged.
  Read `D:\nioh3-wt\c72e-pilot-harness\notes\handoff-codex-c72e-pilot-harness.md`.

Neither candidate is merged or deployed. There is no active child source writer.

## Rematch: exact next live experiment

Only RC72E is prepared. Existing native start **335814** is roughly 43 m from
the fight. Shipped native start **334140** is around 4.6 m from its boss:
`207628.53125, 6027.47998, 142677.921875`, yaw `40.61`.
The intro script's fight position independently matches the user's actual run.
This is a strong static candidate, **not a confirmed instant rematch**.

Kit: `D:\nioh3-re\rematch-pilots\C72E-native334140-20261011`.
Main prep tool: `tools/rematch/prepare_c72e_native_pilot.py`.
It changes only the single catalog row and leaves the existing gate armed.
Both arms use **FightNow=1, Warp=0, Spawn=0, ArenaCapture=0**, no unrelated
catalog changes, and an isolated empty Arenas file. Fifteen light checks passed.

Critical gotchas:

- The recorded pre-intro shrine position can be imported as an "arena" from
  the existing Arenas capture file on next launch. Isolate/disable that input.
- Do not set `atFight=true` merely to change labels; it can suppress the native
  gate required to start the fight.
- Existing v4 spawn-restore failure ownership is unsafe for new mutation work.
  Use the native request-only pilot first, not a new spawn-record overwrite.
- EngineFix 1.7.32 `AutoContinueLoad` only dismisses post-load tips; it does not
  choose Continue on the initial title screen. Never blindly press Enter.
- The rematch pipe observes player/mission/retry/loading/position but does not
  expose a proven control-enabled flag. A first-observed position is not the
  first controllable frame. Record visual acceptance and actual combat; old
  eventual BossBattleMode/zero-warp PASS is insufficient.
- Pilot tooling is being designed with fresh save backup, bounded ownership,
  exact restoration and readonly save-file handles. Review actual failure,
  temporary-file and cloud behavior before running it. No new approval checkbox
  is required; the user already authorized testing and cloud-dialog handling.

## Device results: do not overstate them

Last reviewed bridge cursor **1952**. Messages1951/1952 confirmed recording
lock release; no new pc2/Ally evidence. Read only new messages, then update it.

- pc2 whole-default test message **1928** reports stock **30.1176** versus
  stable **45.0582 FPS**, **+49.6073%** (paired delta **14.940526 FPS ±0.073337 FPS SE**), two paired launches out of
  an incomplete six-launch plan. Fifth stock launch crashed in amdxc64; no
  sixth launch. A +14 h timestamp mapping is still conditional, and final
  cleanup/restoration/lock release are attested rather than fully archived.
  Root request **1935** asks for recovery receipt and independent clock anchor;
  **HOLD rerun**. Read `notes/handoff-codex-default1761-pc2-evidence.md`.
- Ally messages **1940 /1944** report default **+2.95% ±0.43% SE**, three launch
  pairs, sampled MangoHUD means rather than per-frame traces. Raw archive still
  pending; frame generation OFF was read from mod OSD rather than verified
  actual menu. Root request **1946** asks for evidence. Final restoration/sync
  were attested. No new Ally launch until the PC shares-account clearance.
- Neither result closes all acceptance gates or predicts ordinary x16 hardware.

The next GPU measurement should use USER GP2 **MinRun1** for exact MPC20/33/22/24
hashes. Old MinRun4 merges PSOs and bills the longest interval: the large rows
actually identify **24 /22**, not unbuilt 20 /33. See
`notes/handoff-codex-next-perf-triage.md`. Do not build speculative MPC20 work
on a wrongly attributed cost. Compute memo serving remains NO-GO until complete
mutation/lifetime invalidation; Bounds/BPO is already shipped.

## Next steps, in order

1. Verify the clean handoff and private candidate refs, then review the guarded
   RC72E native pilot. Fresh backup and headroom, shared lock,
   one owned launch, initial title handling, first landing/combat evidence and
   exact restoration. Confirm to Ahmad/Discord only if first controllable
   arena entry and fight really work; otherwise report the precise failure.
2. Review/integrate the menu candidate with current core/addon lineage. Run
   patterns, `check_release --full`, required harnesses and build before sharing.
   Keep the explicit INI-only coverage gaps and next-launch semantics honest.
3. Consume pc2 recovery/clock and Ally raw/FG evidence. Queue bounded AABB or
   fog/lodu A/B only with exact installed-cave/artifact/image/scene gates.
   Split CPU-bound and GPU-bound evidence; keep normal-link hardware in scope.
4. Continue useful performance work from the triage note. The full fog finite
   proof now permits the next native GPU/image/controlled A/B gate; it is not
   universal bit-equivalence or device acceptance. Training mutations
   wait for native lifecycle/restore contracts; do not spend successive units
   merely adding observers and describing them as a playable training mode.

## Commands and standing rules

```powershell
# Lightweight catalog; no game or heavy native test:
py -I -B tools/testhub/hub.py
py -I -B tools/testhub/hub.py plan all

# Required before any shared build (run in the candidate worktree):
py -I mod/Nioh3EngineFix/tests/test_patterns.py
py -I release/check_release.py --full

# Bridge, from universal-optimizer\agent-bridge:
$env:PYTHONIOENCODING='utf-8'; $env:UO_AGENT_ID='codex'
py -m uo_bridge read --channel nioh3 --since 1952 --limit 8
py -m uo_bridge.gamelock status nioh3

# Existing notifier loads its own credential; never read or print it:
py -I 'C:\Users\Ahmad Hamza\universal-optimizer\tools\notify\notify.py' result --project nioh3 --verdict neutral --title 'Checkpoint' --text 'Measured result and remaining gates' --file notes/handoff-codex-astra-2026-10-11.md --repo 'C:\Users\Ahmad Hamza\nioh3-engine'
```

Game-side changes only. Keep default picture identical; quality controls opt-in.
One game under the lock; PC/Ally share Steam account. No Ghidra while game runs.
No force-kill or guard bypass. Never call Claude CLI/API or change its wake
state. Never persist/print the pasted API key, webhook or bridge token. Native
MSVC setup may use `tools\msvc\setup_x64.bat`; PowerShell scripts may need a
per-process `-ExecutionPolicy Bypass`, never a global policy change.

Before public work, read `notes/handoff-claude-public-repo.md`: no shader
`.file`s, dumps, decompilation, captures, saves, private tooling/handoffs/history;
scrub PDB paths from shared binaries. Commit identity is
`AhmxdNYC <104233015+AhmxdNYC@users.noreply.github.com>`, with
`Co-Authored-By: OpenAI Codex <noreply@openai.com>`. Commit only owned topics,
push verified checkpoints and keep STATUS, ledger and return notes current.

## Final transfer receipts

Full fog result and both private candidate commits are recorded above.
Fresh persistent session **01a12cb5-4711-7a42-8bf9-c53cb49dcbcd** was created;
its actual `turn_context.model` is **gpt-6-astra**, cwd is the private Nioh repo,
approval policy `never`, and latest sandbox `danger-full-access`. It confirmed
actually reading the requested handoff sections, STATUS and AGENT_BRIEF.
Initial read-only startup blocked file access; this was corrected and the
successful full-access read turn exited0. No edits/launches/messages by it yet.

The existing phone transcript pointer now follows this exact fresh session.
The old lead requested the extension's `/local/<thread>` route; visible IDE
navigation could not be verified. The user can always resume the saved chat:

```powershell
codex resume 01a12cb5-4711-7a42-8bf9-c53cb49dcbcd -m gpt-6-astra -C 'C:\Users\Ahmad Hamza\nioh3-engine' -s danger-full-access -a never
```

After committing this checkpoint, the old lead opens an interactive Astra
terminal on the same session with the takeover prompt, then stops writing.
`lead-active` remains reserved for that interactive lead, preventing an older
automatic worker from overlapping. The continuation controller/model defaults
are unchanged; this explicit chat uses Astra. The new lead owns future safe
checkpoint/release decisions. No promise of unlimited background operation or
account availability is made.
