# PC2 owner recipe: integrated move menus (#11218)

Offline source inspection only; companion did not access the game. Test the
startup-passed cfc73e2 artifacts, NOT a fresh rebuild: Training SHA256
03a90704b5cbffbf29d6d68fddb9fe561bec6d4c3a792750596cbc2013ee750b;
Rematch 3332ff789f8c86f861b37293d5994108e865cdd599ac48810bf991426c190672.
They are in C:/Users/User/projects/nioh3-multiboss-10803/mod/:
Nioh3Training/Nioh3Training.asi and Nioh3BossRematch/bin/Nioh3BossRematch.asi.

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
