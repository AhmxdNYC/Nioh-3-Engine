# PC2 owner recipe: integrated move menus (#11218)

Updated for #11237: offline source inspection only; companion did not access
or install into the game. Use the requested integrated 4d91269 fresh pair.
Rehashed on PC2 for this recipe; both files match the integration record:
- Training SHA256 `3024f2a9fb0e3dbb385ab859adebe9b3da72e8e029be5d2c5027669a23337df1`
- Rematch SHA256 `2e67c9d9a7079d1a606675bc2a3246f4822ba37940f54b0f830167a091ecae98`
- Root: `C:/Users/User/projects/nioh3-integ-10870/mod/`
- Files: `Nioh3Training/Nioh3Training.asi` and
  `Nioh3BossRematch/bin/Nioh3BossRematch.asi`.
The older cfc73e2 75-second startup result is NOT a live result for this pair.
Do not rebuild or substitute a later artifact during this comparison.

## Reuse and critical distinction

Reuse main-worktree captures/pc2-toggle-10030/runner.ps1 lifecycle and its
T / RM / VP / S / SHOT / POLL scenario operations. Owner alone runs it with
the exact TrainingAsi and RematchAsi paths above and existing PerfAsi. Keep
save/config backups, autoplay and lock discipline. Infinite player health ON,
ActionLog=1, MoveBlock empty, MoveBlockFallback=2, debug freeze OFF.

The old scenario-10454.txt ONLY tests legacy MoveBlock. `T moveblock` clears
that list, but does NOT clear saved per-character menu preferences.
There is NO Rematch pipe command accepting menu item IDs in this pair.
`RM open` opens the NATIVE replay list, not our Training menu. `VP menu set`
controls EngineFix settings, not Training moves. Do not invent `menuaction`.
Actual menu Left/Right invokes SetMoveAllowed(chr,id,on,1), saves immediately
to Nioh3Training.ini, and logs `MOVEMENU chr ... action ... ON/OFF saved`.
Editing INI directly tests loading only, not menu preference storage.

## Exact IDs and input

Root Training moves is item -29, zero-based row 7 when all full-pair exports
are present. Moves page 901 starts at stance item -30, row 0. Init 0 and idle
2 occupy rows 1 and 2 and cannot be disabled. Item = 0x30000 + global catalogue
index. These IDs identify source dispatch; they are NOT pipe commands.

| Boss | Fight command | chr | target action | item | moves-page row |
|---|---|---:|---|---|---:|
| Yoritomo | T fnow 0x7B35 | 16102 | 0x4E7B | 0x30023 | 14 |
| Yoritomo alternative | same | 16102 | 0x4E64 | 0x30022 | 13 |
| Shingen | T fnow 0x15FF | 42450 | 0x4E4E | 0x30007 | 8 |
| Shingen alternative | same | 42450 | 0x4E64 | 0x30002 | 3 |

Yoritomo 4E7B had 7 starts in the earlier legacy baseline; 4E64 is useful
for cross-character preference isolation but may be rare. Never reuse 4E60,
4E62,4E6C,4E74,4EA7 as menu targets: absent from this pair's catalogue.

Use the existing owner's menu-opening method. For standalone Rematch the
default is hold View+Up for 1 second (`VP press BACK+UP 1200`); if EngineFix
hosts it, enter its Boss Rematch row instead. Confirm a screenshot of the
Rematch root before navigating: input/opening behavior depends on installed
EngineFix and menu configuration. Do not count invisible or rejected presses.
From confirmed root selection 0, seven separate `VP press DOWN 120` then
`VP press A 120` enters moves. Each input should be released before the next;
insert `S 1` between scenario presses. Confirm page title/current boss.
From stance row 0, use 14 separate DOWN presses for Yoritomo 4E7B, or eight
for Shingen 4E4E. `VP press LEFT 120` = OFF; RIGHT = ON; A toggles.
Use screenshot plus MOVEMENU saved line as acceptance, not command success.
Back B returns to parent; reopen and confirm selection visually instead of
assuming selection reset. Never blindly issue seven DOWN on a retained row.

## One ON/OFF/ON sequence per boss

1. Fight now, `RMWAIT 180`, then wait for live correct boss and cover release.
   RMWAIT only checks player/loading, NOT Training boss readiness. `T nodeath 1`;
   `T moveblock`; `T idlestat`. New session defaults Idle: verify status 2 and
   player movement usable. Pending 1 is not success; -1 means acquisition failed.
2. In moves menu set all editable rows RIGHT/ON (Yoritomo rows 3..14;
   Shingen 3..22). On stance row LEFT selects CPU. Close menu, `T idlestat` must
   reach 0. After 5 seconds settling: `T movestat`, `S 60`, `T movestat`.
   Capture timestamps and ACTLOG starts for baseline ON1. No attacking required.
3. Menu target LEFT/OFF, verify MOVEMENU saved and exact INI key 0. Close/reopen
   menu and screenshot same OFF row. Close; allow current action to finish
   (at least 5 seconds, extend until its transition ends). `T movestat`,
   `S 60`, `T movestat`: OFF arm. Require other actions to keep starting.
4. Menu target RIGHT/ON, verify saved/key 1. Close, settle, same 60-second
   bracket = ON2. Require target recurrence; no target in ON1 or ON2 makes
   the comparison inconclusive. Extend equally if needed; report actual times.

Report target ACTLOG starts, ALL action starts, delta vetoes and delta rerolls
for each arm. `movestat` counters are process-cumulative, not reset by moveblock.
Its `blocked` value counts legacy IDs ONLY: 0 is expected with menu OFF.
Its mislabeled `select vetoes` total includes select AND commit vetoes.
MOVETOG detail logging stops at counter 200; use counter deltas after saturation.
Evidence is OFF target starts=0 plus positive veto/reroll activity and continued
other actions, bracketed by target starts>0 in ON1 and ON2. Zero attempts cannot
prove a veto. This is behavior validation, not an FPS benchmark or Ki-break proof.

## Persistence, Idle/CPU and retry

After ON2, set the target OFF again. Save a copy of the resulting Training INI
as evidence while the harness is active. Reopen the page: still OFF. `T retry`,
wait for replacement boss and readiness: preference remains OFF and CPU stance
remains CPU (idlestat 0); other permitted actions resume. Then select stance
RIGHT/Idle in menu, close and poll `T idlestat` to 2; observe player movement.
`T retry` again: replacement boss reacquires Idle (2), not just the old actor.
Select LEFT/CPU and verify attacks resume. `T idlehold 0/1` is an exact command
fallback for isolating lifecycle behavior, but does not prove menu dispatch.

For relaunch, preserve the menu-written INI across process restart. The runner's
finally RESTORES it, and passing TrainingAsi COPIES the adjacent INI each run;
therefore two ordinary independent runner invocations do NOT test persistence.
Owner may restart inside the same backup/lock lifecycle, or retain the saved
menu-written INI and deploy that unchanged as launch-2 input before restoring
original config afterward. Do not rewrite keys or send menu setters on launch 2
before checking the same boss's OFF display/key and actual veto behavior in CPU.
New session starts Idle even if previous process ended CPU: expected, not failure.

Cross-boss isolation: Yoritomo 4E64 OFF must not turn Shingen 4E64 OFF (ensure
Shingen was explicitly ON first). Sections are Moves.16102 and Moves.42450.
Restore target ON through menu after evidence, then restore original files/saves.
Final owner verdict includes PC2, exact hashes/settings, both OFF and ON arms,
60-second counts, persistence/retry/Idle outcomes and any inconclusive target.

## Idle OFF / ON / release: exact 30-second arms (#11237)

Run this separately for EACH boss selected by the table above, with god mode
ON (`T nodeath 1`, verify `T hp` reports NoDeath 1), ActionLog=1, all editable
attacks ON, legacy list empty (`T moveblock`), and debug freeze OFF. Restore
all menu targets ON after the persistence tests before starting these arms.
Use the existing runner scenario grammar; T sends the following text to Training.
All polls are bounded; command acceptance alone never means the boss is held.

1. `T idlehold 0`, poll `T idlestat` until 0, close menus and settle at attack
   range. Bracket `S 30` with `T movestat` and `T actors` plus screenshots/video.
   Require normal boss attacks and movement; no attacks makes baseline inconclusive.
2. `T idlehold 1` during an ordinary attack. Poll `T idlestat` each second for
   at most 10 seconds after pending. Expect pending -> optional transition ->
   held/status 2. A current attack may finish. Do NOT start the 30-second ON
   timer while pending. On held, bracket `S 30` as above. Require zero new
   attack starts and zero AI locomotion, visible idle animation and usable player.
   Keep the counted window passive; afterward hit once to check native reactions
   are allowed while AI attacks stay held. Hit displacement is not AI locomotion.
3. `T idlehold 0`, poll status 0; bracket another `S 30`. Expect stop OFF and
   resumed ordinary attacks. Record actual time-to-held and time-to-first attack.
4. Repeat one ON/OFF using menu stance RIGHT/LEFT to prove menu dispatch.

Expected log/counter interpretation:
- `CMD idlehold status 0/1/2/-1`: OFF/pending/held/failed. Pending or -1 is not PASS.
- `IDLEHOLD pending`, optional `transition chr ... -> idle 0x2`, then `held`;
  release produces `IDLEHOLD stop OFF`. There is no dedicated attack-count counter.
- Derive action-transition counts from `ACTLOG boss ... chr ... action ...`,
  separating attacks from idle/status/hit reactions using the catalogue and video.
  ACTLOG is sampled, not an exhaustive native start hook; zero logged transitions
  alone does not prove no attacks. It stops after 2999 emitted lines per process;
  if exhausted, preserve evidence and repeat within a fresh owner lifecycle.
- `movestat`: hooked=1, blocked=0; with all moves ON, delta vetoes=0 and
  delta rerolls=0 in all three idle arms. Idle substitutions do not increment
  those counters. For individual-move OFF testing use the earlier ON/OFF/ON criteria.
- Record boss actor/handle per arm; reject a comparison spanning replacement,
  loading/cover hold, death/HP floor, Ki break, or debug freeze.

## Integrated cleanup and debug-freeze coexistence (#11237)

These expectations supersede the historical OFF-on-retry paragraph in
boss-idle-hold-10371.md. A NEW Training session defaults Idle; retry preserves
chosen stance. Hold -> `T retry`: old ownership stops, replacement acquires
Idle (status 2 after readiness). CPU -> retry: replacement stays CPU (0).
Use the actual mod Return menu (Fight again first, Return second), not a guessed
Training `return` command. Hold -> Return: old ownership cancels before teardown;
then Fight now for either boss: a NEW session defaults Idle, not CPU. Release
with `T idlehold 0` and prove attacks resume. Verify logs identify the replacement
actor; the old pointer alone is insufficient. Never release a cover/loading hold
as a workaround. Positive-Ki eligibility and readiness must precede acquisition;
default readiness is bounded at 60 seconds, then safe-idle acquisition at 10 seconds.

For debug freeze use the existing F9 keyboard hotkey in the foreground game,
with `[Debug] Hotkeys=1`, `FreezeKey=0x78`. No Training freeze pipe command is
implemented in this pair; VP gamepad buttons are not F9. The game-owning agent
uses its existing keyboard automation; companion performs none of these actions.
In CPU stance, F9 ON must pause mid-animation/player with `DBG FREEZE ON` and
time scale 0; F9 OFF must resume. Then acquire idlehold=2, F9 ON/OFF: global
freeze releases but idle hold must remain 2 and boss AI stay idle; player moves
again. Finally idlehold OFF must resume boss AI. Do this outside measured arms.
Return while F9-frozen should log `RETURN: F9 debug freeze released first` and
complete cleanup, avoiding a frozen return load. Explicitly unfreeze before retry
(the recipe does not assume retry auto-releases debug freeze).

Owner report per boss: PC2, exact pair hashes, settings, OFF/ON/release 30-second
attack counts and movement verdicts, veto/reroll deltas, acquisition/release time,
menu persistence/relaunch, Return/retry, and F9 coexistence. No live result is
claimed here. Retain logs/video/INI evidence before normal save/config restoration.
