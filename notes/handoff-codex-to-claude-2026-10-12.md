# Codex to Claude handoff: Nioh3, device testing, and phone coordination

Status: **FINAL - both authorized trials finished; recovery verified.**
Prepared at the user's request on October 12, 2026 (America/New_York); finalized
after evidence365/366 on October12 evening (October13 UTC).
This is a catch-up document, not authorization to take over another live owner.

## Read this first

- Read `AGENT_BRIEF.md` and this document before old STATUS updates. Several old
  memory restrictions and the original weekly usage cutoff were superseded by
  explicit user instructions. Do not reinstate them from an old test packet.
- User wants **actual in-game tests**, minimal necessary checks, brief messages,
  and no repeated unchanged test suites or expanding diagnostic frameworks.
- User explicitly removed artificial harness RAM/headroom/swap/PSI launch gates
  and automatic memory-triggered termination. They are telemetry only for the
  currently authorized trials. This is a harness change, not an OS optimization.
- The original stop at 80% weekly Codex usage was reached. User subsequently
  explicitly authorized continuing these gameplay tests past 80%, with efficient
  token use. This is not permission for unlimited unrelated work.
- Keep current save backups, exact owned process identity, device lease, fixed
  runtime and cleanup. Never roll an old automated backup over later human play.
- User prohibited looking at the main-PC game screen during their manual test.
  Main investigation stayed logs-only after that instruction. Device-local visual
  checks are used for remote trials. Ally images stayed local. PC2 evidence366
  includes screenshots in its archive; root hashed their bytes but did not view
  or extract the images. Root reviewed logs and peer observation receipts.
- ACKs are mandatory: instruction ID plus next action/blocker. Sending or IPC
  submission is not acknowledgement of understanding. Retries must not duplicate
  a game run. Discord and phone status forwarding are now automatic.

## Final outcomes of the authorized trials

| Device | Current authorized work | Latest confirmed state |
|---|---|---|
| Main PC | No new manual game launch; preserve user's latest progress | Previous user session closed normally at their request; RA4B4 retry bug remains unresolved |
| PC2 | One native pause/resume, Practice, held-entry Summon trial; instruction2844 | **FINISHED: field pause/resume, Practice/control and healthy held phase2 PASS. Summon/native return UNRUN.** Evidence366 verified; original20min deadline ended trial before submenu completion. Exact13 baseline files/6 saves restored; task16 FREE. No retry |
| Ally | One original-shader GPUProf0 actual field baseline; instruction2843 | **FINISHED: field/control PASS; FPS baseline UNRUN.** Evidence365 verified:61 manifest files,497 memory rows,9/9 noisy admission intervals. Exact restoration and bounded observer PASS; root independently queried task17 FREE. No retry. Source5b9cbf83/entry893122e5 |

PC2's latest local admission is `C:/nioh3-re/pc2-admission-2844-local.json`,
SHA256 `4acaa0e575a0a1d602ec1458f461074db65ead50c5a64a60f812569749fe9776`.
Peer reports a narrow derivative of357: memory wait becomes nonblocking telemetry,
pilot memory-environment refusal removed, transitive pins updated. Original357
and earlier admission362 remain immutable; do not reuse those admissions.

PC2 final evidence366 SHA256:
`a8a3dd4a846224fc44ed400da621862ce93b917e2d069cd6aa6c942ed27f0b98`.
Local archive/review: `D:/nioh3-re/reviews/pc2-final-2844-366.zip` and
`D:/nioh3-re/reviews/pc2-final-2844-366/root-review.json`; remote output
`C:/nioh3-re/pc2-summon-2844-20261012-1945`. Root verified all64 manifest members,
13 restoration pins and6-save inventory against readiness358, and1871 raw
practicehold samples.402 were healthy phase2 from02:59:55.4338688Z through
03:02:40.4989966Z; all broken/diagReady/pending values were zero.

Actual field Pause3/Key3 resume was recorded before one Practice confirmation at
02:57:15.4349571Z. Shrine and250ms movement response were observed. Held native
pause was also observed, but the held Practice submenu review was incomplete;
**no Summon intent or native return was tested**. The original20min deadline
03:02:40.5701103Z ended the run. Serial input/capture/review latency consumed the
budget; this is an incomplete trial, not a native Summon failure. Final Forward2000
was interrupted by movement-window closure and released in finally; it has no
successful full-duration input receipt.

Original game8180/runner3332 exited. Independent recovery at03:04:26.554155Z
verified13 baseline files, exact6 saves and fresh backup, no game/collectors/guard
or recovery hold; root separately queried task16 FREE. Original result.json's
PREPARING/null and pending-lease snapshot remains unchanged; operator-summary.json
and independent-recovery.json clarify the actual outcome and final cleanup.
The runner's entry-hold-admission.json SHA256 is
`f29e456c9a1c662faf947e3e132340fa4d29d99d0d082bc27fc50b8e59a711ab`;
its parsed JSON is identical to the original4acaa0e5 admission above, despite
serialization byte differences. Both are retained in366. No further run is queued.

Ally's inherited generic STOP receipt says gameplay UNRUN even though prior
observations demonstrate field and menu control. Keep that original unchanged and
link the peer's factual clarification; do not discard the actual observations or
silently rewrite the receipt. Nine CPU admission intervals were noisy (last total
about1.98 cores); no25s benchmark window was accepted. No performance gain follows
from this successful functional smoke.

Ally final evidence365 SHA256:
`bcfe4c849eccf489aa45ea568e63e22802603439648922104e507b6760400ef1`.
Local archive/review: `D:/nioh3-re/reviews/ally-gameplay-365.zip` and
`D:/nioh3-re/reviews/ally-gameplay-365/root-review.json`. Peer output:
`/var/home/bazzite/Nioh3EngineFix/backup/gameplay-nonblocking-r1-20261012`.
Runtime is `experimental-11.0-20261008-x86_64`, with exact hashes in entry.json.
Five observed inputs, Level136 current shrine, FG off, original shader, no movement;
45s settle passed. Memory remained telemetry only despite full PSIavg10 reaching1.13
and physical memory reaching3.319 GiB. This proves the former limit was not a
necessary condition for this functional smoke; it does not establish unrestricted
memory safety, quiet timing, or a shader performance gain. Images stayed on Ally;
root reviewed the recorded local observations and raw receipts, not the pictures.

## What changed in the game work

### Rematch cover and the user's manual tests

The incoming handoff `handoff-codex-2026-10-12-live-rematches.md` retains the
earlier seven natural clears and the original R1A5E hidden-death failure. Its
active-process/install state is historical, not current.

- Source `34296f3c4c2f86505387dab054b9aa15a7525ebc` corrected the Yagyu dojo
  cover's assumption that every encounter creates a new actor; that route uses
  an existing teacher. It also added the requested loading progress bar.
  Frozen candidate: `D:/nioh3-re/candidates/cover-yagyu-34296f3`.
- User later asked to launch this build immediately. Engine1.7.73 and addon1.4.21
  were installed with fresh entry backups. Manual mode used VirtualPad0 and
  HostedFightTest0; the user owned input and progress.
- Manual session: `D:/nioh3-re/rematch-pilots/user-rematch-34296f3-20261012`.
  Yagyu R1A5E existing actor5027806 was logged at18:53:35.666 EDT, cover closed
 18:53:36.189, mission clear18:54:25.347, native exit18:55:08.042. This is real
  live evidence, not proof every route or first controllable frame is correct.
- The next RA4B4 Gozuki/Tokugawa fight began, then the user died. Same-mission
  reload left the add-on reporting “fight already on — no warp”; user reported
  an absent boss. This is a distinct **death/retry bug, still unfixed**.
- On request the game closed normally at19:02:47 EDT. No save rollback; final
  human progress was copied to `user-exit-saves`. Logs are in `user-stop-logs`
  and `log-check-1859`; no subsequent main-PC launch is claimed here.

RA4B4 investigation found `fight::g_active`/gate state retained across same-mission
reload; retained `fnowpass` is not fresh evidence of another fight. Native reset
authority is unresolved, so no blind flag clear or forced native reset was added.
Diagnostic source only: `6c1df8663e3202f7357d03c6d7cb14cf9327197d` in
`D:/nioh3-wt/ra4b4-retry-observer-20261012`, note
`notes/ra4b4-retry-observer.md` within that worktree. Default-off `[Debug]RetryTrace` observes anchored
death/actor/loading/gate state. Build and focused checks passed; **not installed,
not a retry fix, not runtime accepted**.

### Offline release-gate repair and main automated tests

The initial full release gate failed two harnesses. That is preserved history,
**not the final offline result**. Follow-up `38b56f0` passed all255 pattern sites
and36 harnesses, zero errors, three existing warnings. Frozen34296f3 game
binaries remained unchanged. Scheduler test observer affinity was corrected;
its85% threshold was retained and the result was89.6%. Replayprof passed unchanged;
the old truncated failure cannot be reconstructed. Checker failures now preserve
stdout and stderr. See `notes/rematch-release-followup-2026-10-12.md` and
`D:/nioh3-re/candidates/cover-yagyu-34296f3/full-release-pass-38b56f0-20261012.json`
(SHA256 `109653f008a2eaa60fc2ae9e61386a88308819aeca73e783ae4542b31b6abdc9`).
Main integrated the harness repair as `1ac5d5b`; this offline pass does not erase
the native RA4B4 retry defect or imply general release readiness.

Earlier automated main-PC root6/root8 trials established named Yagyu and camera
response but not the complete pause/return workflow. Root11 stopped after Continue
when PrintWindow plus PNG serialization exceeded its1500ms bound. Existing PNG
bytes did not prove a timely capture. Eight saves/current backup and four installed
files were independently restored. Evidence:
`D:/nioh3-re/rematch-pilots/yagyu-shrine-diagnostic-20261012-root11/independent-recovery.json`
and `D:/nioh3-re/reviews/main-retest-readiness-20261012/root11-capture-review/REVIEW.txt`.

### Training/Practice/Summon development and PC2 evidence

| Source / trial evidence | What was established | Remaining limit |
|---|---|---|
| Combined training `c6d9abe`, early276/281/287; evidence278/284/289 | Native refusal and harness/navigation failures were preserved; restoration verified | No Summon acceptance |
| Exact-site registration `1143929`; candidate290, trial293/evidence294-295 | Native Practice reached Yamagata shrine1CBC/67064; visible player control | Movement budget expired before held entry |
| Trial298/evidence299 | Native hold later broke with owner-stale | Cause not proven by that trace alone |
| Diagnostic `3918d97`; trial303/evidence304 | First writer was Tick lock contention; exact attempt/arm diagnostics captured | Lock holder not established; no Summon |
| Observer correction `04083fb`; candidate305, trial308/evidence309 | Healthy held phase2 reached; broken0 throughout captured hold | Native pause/resume not proven; Summon withheld |
| Pause/resume runner `71eae77`; package340 | Actual field pause/resume proof required before Practice; held menu/intent/resume checks added | Source/196 runner+126 input checks are not native acceptance |
| Owner-only package357/readiness358 | Current owner13024 binding replaces8880;11 dependencies unchanged | Trial362 blocked before any game launch |
| Trial362/evidence364 | Existing18GB effective/6GB current memory gate waited300s then restored staged files | **Gameplay NOT TESTED**; prompted user to remove the gate |
| Trial2844/evidence366 | Native field pause/resume, Practice shrine/control and healthy held phase2; actual held pause | Fixed20min elapsed before Summon; exact restoration verified |

Trial308 first raw healthy held phase2 was12:44:36.1824485Z, with healthy samples
through12:47:44.3177270Z. Escape opened Equipment with “3 Pause”; F10 showed the
top-level overlay. Neither demonstrated actual native pause. Do not call308 a
Summon pass. Latest functional evidence is the completed2844 trial above.

Trial276's literal `cover busy` during native loading caused a conservative stop;
the runner's foreign-token wording did not prove a foreign token actually existed.
Retain that distinction when diagnosing the older rematch trace.

Important worktrees: `training-site-registration-20261012`,
`training-diagnostics-20261012`, `training-observer-20261012`,
`pc2-summon-20261012`, and `summon-intent`, under `D:/nioh3-wt`.
Do not assume a worktree tip is the installed or admitted frozen candidate.

## Performance work: what passed and what did not

All three **compiler gates** passed on the then-current Ally runtime:

| Track | Evidence | Scope |
|---|---|---|
| PS05ad packet274 |331 plus supplemental332 | Actual AMD15bf, WaveOps/SM65,4 roots/8 PSOs accepted; no draws |
| PB2 vertex247 |334 | Actual15bf,8 roots/16 PSOs accepted; no draws |
| Combined PB2 packet257 |334 | Actual15bf,8 roots/16 PSOs accepted; no draws |

Compiler success is not image equivalence, exercised in-game binding, or FPS gain.
Steam subsequently updated Proton Experimental (`proton`, `version`, `d3d12.dll`,
`d3d12core.dll` changed; report2803/packet361). Retain old results as historical
runtime-specific evidence; do not transfer their equivalence to the new runtime.

Four live performance tracks remain: lodu, PS05ad, PB2 vertex, combined PB2.
Lodu A1/evidence313 was invalid for performance because foreign CPU noise was
1.97 cores; B/A2 were not run. The recorded35.607 FPS is not a gain result.
PS05ad live binding package339 was prepared; vertex alternatives345/349 passed
scratch preparation but the actually exercised permutation remains unresolved.
Do not choose a vertex permutation by guess.

Earlier PC2 AABB evidence267 showed essentially zero measured difference
(~39.015/39.066/39.074 FPS), and a measurement-marker issue invalidated isolation.
No gain claim; do not spend another cycle repeating that negligible comparison.

### Why Ally benchmarking stalled

The harness, not a game patch, imposed increasingly explicit memory and process
noise checks. These repeatedly stopped runs before useful gameplay. The user
corrected this workflow and ordered the memory gates removed; those old checks
must not be treated as requirements for ordinary play.

| Evidence | Actual outcome |
|---|---|
|318 | Native helper output unavailable and cache context belonged to Codex launcher; not exact-game compiler acceptance |
|323 | Captured actual game runtime/cache context; restored; post-Proton environment was not blindly replayed |
|326/329 | Empty command markers first; corrected cmd-only stdout/stderr/exit transport passed329 |
|331/332 | Native PS05ad passed; generated cache file mistakenly pinned. Supplemental genuine-file/closure check resolved hold without rewriting original failure |
|346 | Initial focus race stopped title-only attempt; exact restoration |
|348 | Memory-pressure/swap gate stopped startup; no gameplay |
|350 |60s idle survey; no new swap-out/reclaim/OOM; does not attribute earlier pressure |
|353 | GPUProf0 startup60s completed under unchanged old limits; no gameplay/FPS or causal profiler proof |
|354/355 | Fixed omitted CPU gaps and CSV tail outside noise accounting;27 focused checks on355 |
|356 | Loading stop at PSIavg10=1.08;213 main/291 recovery rows,61 after closure; restored, no field/FPS |
|363 (trial361) | Title/Continue actually exercised; hard stall125548us/~1s triggered kill. Immediate closure confirmation failed; separate recovery restored entry. Observer incomplete, no field/FPS |

Selected evidence roots: `D:/nioh3-re/reviews/ally-profiler-off-353`,
`ally-profiler-field-355`, `ally-field-result-356`; peer363 is the later loading
diagnostic result. Preserve failed original receipts and append supplemental
recovery rather than turning failures into PASS.

## Earlier source and reverse-engineering work to retain

These are indexed here so the live-test narrative does not hide substantial
prior Codex work. Their original notes retain exact proof scope and caveats;
none should be promoted to an installed optimization from this summary alone.

| Work | Source/evidence pointers | Status distinction |
|---|---|---|
| Production1.7.61 integration | `c5183c2` CBV upload, `cba1fbc` extra-perf; `notes/handoff-codex-step1.md` | CBN/CBT enabled; FO and visual presets default-off; consult original measurements |
| Frame overlap experimental path | `24494d5`, `D:/nioh3-wt/codex-fo10`; `handoff-codex-fo98-*`, `handoff-codex-fo-startup.md`, `renderer-producer-callers-2026-10-11.md` | Startup repair/negative controls/lifecycle witnesses; fence identity `7bca1995`; producer closure/resource retirement unresolved: FO NO-GO |
| ApplyBatch/FrozenWrite5 installation safety | `5736bdb`, worktree `codex-psomemo-install-guard`; `handoff-codex-applybatch-failure-audit.md`, `handoff-codex-guard1765-evidence.md` | Failure-policy and full offline checks passed; keep merge/install/native admission distinct |
| CBV recreation and lifetime | `handoff-codex-cbv-*` notes | Modeled repeated-key generation failures are not established native failures; registry/reset/lifetime research remains |
| Compute PSO compare-only core | `6ed4da3`; teardown audit `e433216` |159 checks; always returns stock result, no cache serving or gain claim; one observation boundary is not full lifetime safety |
| Training, cover and menus | Cover clock-order `dd1cc4d`, combined intent `c6d9abe`; `training-arena-switch-contract.md`, `combined-summon-native-readiness-2026-10-12.md` | Transactional restoration/quarantine, read validity, menu/control work; encounter switching is not arbitrary actor transfer |
| Loading/renderer analysis | `14afb92`, worktree `loading-drain-proof`; PB2/MIC/PS05ad and fog proof notes | Static boundaries, not measured loading savings.54,295 indexed generated C functions/5,932 core functions are coverage counts, not complete semantic understanding |

The repo has many isolated worktrees. `git worktree list` identifies their current
tips; frozen package manifests identify what was actually tested. Older
`codex-rendering-progress-2026-10-11.md` and `codex-task-ledger.md` are useful
historical indexes but do not supersede current instructions or latest results.

## Phone, chat relay, dashboard, and automatic notifications

Separate worktrees are intentional; do not consolidate by overwriting them:

| Location | Relevant work |
|---|---|
| `C:/Users/Ahmad Hamza/universal-optimizer-phone-delivery` | Exact-chat relay and automatic status sync |
| `C:/Users/Ahmad Hamza/universal-optimizer-phone-dashboard` | Phone agent graph, tasks/devices, Claude+Codex limits UI |
| `C:/Users/Ahmad Hamza/universal-optimizer` | Existing notifier, credentials loader and shared tools |

- Relay commits `8c54e25`, `503a34f`, `2167666`: direct installed-extension IPC
  into the bound VS Code thread; active steering and same-thread idle-start tested.
  Phone2268/2270 and idle probe passed. No editor typing or competing Codex worker.
- Dashboard commits `660c172`, `ede80ea`, `af8f63c`: operations view, top navigation
  limits dialog for Claude and Codex, mobile fixes, explicit agent delegation graph
  showing computer/task/state/freshness. Codex usage reads authoritative local
  rate-limit events, not token-count estimates.
- PC2 direct active and idle IPC delivery passed2325/2339. Owner restarts require
  binding refresh; latest reported owner13024, exact start in readiness358. Do not
  resurrect11484/8880 or start a second listener. Ally's original listener was
  logging-only; keep-awake is distinct from chat auto-wake. Do not claim packet288
  was installed on Ally without fresh evidence.
- Notification commits `eeaffe4`, `9631f0d` in phone-delivery add
  `tools/bridge-status-sync/bridge_status_sync.py`, README and10 focused tests.
  Scheduled task **UO-Bridge-Status-Sync** runs hidden under the existing user;
  polls bridge every5s, writes phone peer status atomically, sends Discord from a
  separate durable queue. It makes **no model calls** and no game-control calls.
- Runtime state: `D:/nioh3-re/status/bridge-sync/state.sqlite3` and component
  `*-health.json`; phone snapshot `D:/nioh3-re/status/operations.json`.
  Existing notifier loads protected credentials; no URL/token belongs in notes.
  Nine real events were verified delivered to both outputs at deployment.
  At final handoff audit the task was Ready with last exit1 and cursor2844; root
  restarted the same scheduled task. It resumed the durable queue through2876.
  Stop cause is unestablished; check task state and delivery receipts, not merely
  an old health timestamp. This incident is not evidence of continuous delivery.
- Cursor starts after2808 only on first initialization; never delete its database
  to restart. Normal replay is deduplicated. A crash after Discord accepts but
  before local success commit can duplicate one notification; IDs identify it.
- Status is explicitly reported, not verified gameplay. ACK/RESULT do not imply
  success; structured `meta.status` or leading tags control state. One writer owns
  the operations snapshot. For a deliberate manual edit, stop only this sync task,
  edit, restart; do not restart the chat relay or phone app unnecessarily.

## Continuation workflow and evidence lookup

1. Read latest peer reports and final outcome table; inspect only relevant artifacts.
2. Confirm current owner and whether a human or harness owns any live game. Historical
   PID/start/admission values are evidence, never reusable live authority.
3. Use current entry backup; keep manual-play preservation separate from automated
   entry restoration. For a narrow change, inspect its diff and necessary checks,
   then actually test. No broad suite rerun unless a change/failure justifies it.
4. Record actual tested behavior, failure, raw evidence and cleanup. Keep compilation,
   draw/visual acceptance, control/gameplay acceptance and comparative FPS separate.
5. Send a concise normal bridge milestone; Discord/phone forwarding requires no
   additional model turn. Explicitly ACK task instructions before execution.
   Peers should begin messages `[TO-CODEX] RESULT ...` (or STARTED/READY/etc.);
   destination-first is required by the exact-chat wake relay. `RESULT [TO-CODEX]`
   can reach Discord yet fail to wake the lead chat, so do not invert the order.

Bridge commands run from `universal-optimizer-phone-delivery/agent-bridge`:
`py -m uo_bridge get <id> --out <path> --agent codex --json`, and
`py -m uo_bridge send nioh3 '[TO-PC2] ...' --agent codex --json` (or TO-ALLY).
Device locks: main task15/nioh3-pc, PC2 task16/nioh3-pc2, Ally task17/nioh3-ally.
Credentials remain in the configured loader. Never paste them into documentation.

Historical detail is retained in `STATUS.md`, `notes/codex-task-ledger.md`, the
incoming live-rematches handoff, branch-specific notes and immutable bridge
artifacts. Those contain more detail than this catch-up summary, including failed
approaches. No public release, general all-boss acceptance, or new FPS improvement
is claimed by this handoff.

Local audit archive (outside Git):
`D:/nioh3-re/handoffs/codex-to-claude-20261012/bridge-timeline.json` records458
channel reports after2267 through2875, and `artifact-index.json` indexes67
available artifact metadata records from257 onward at this checkpoint. This keeps
the intermediate workflow decisions available without expanding this summary into
hundreds of messages. Credentials/webhook URLs are redacted; game files, saves,
screenshots and binary contents are not copied into this archive. Bridge reports
remain reports, not independent proof. Final artifacts365/366 are indexed.

## Open work for the next session

- Fix RA4B4 death/retry lifecycle using actual native evidence; RetryTrace is only
  a diagnostic candidate, not a fix.
- PC2 pause/resume and Practice/held entry now have live evidence. Complete the
  held Practice submenu, one Summon intent, timely native resume, visible boss/control
  and native return in a separately authorized trial. Reduce operator round trips.
- Ally field/control baseline now has live evidence. Obtain a valid comparative
  timing window, then correct live binding/visual/performance tests for candidates.
  No shader candidate gained live visual/performance acceptance in these last runs.
- Keep prior runtime compiler evidence distinct from the updated Proton runtime.
- The specific34296f3 offline release gate is resolved as described above. Keep
  native release acceptance separate; do not rerun those unchanged harnesses merely
  because the old handoff still lists them as failures.
- Do not wake or invoke Claude automatically. This document is for Claude to read
  when the user brings that agent back.
