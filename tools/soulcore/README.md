# Soul-core survey: every core's timing in one run (human #134924)

One command times every owned soul core on the regular player in one launch, then writes one table.

```
powershell -NoProfile -ExecutionPolicy Bypass -File tools\soulcore\run_survey.ps1 -Asi <Nioh3BossRematch.asi> -Seat pc/3 [-Out D:\nioh3-wt\sc-survey-X]
```

- `-Asi`: a Boss Rematch build with the `spirit scequip/scstate/gauge` pipe (pc2/sc-survey-134412 e93268d3 or later, e.g. BR e5b755af from 11-25).
- What it does: lock + marker + save backup, installs only that ASI (Training untouched), VirtualPad on, Continue into the field,
  then `survey_cores.py` equips each core by the shrine path, uses it twice and keeps the second use (first use learns hitboxes, #134293).
  One Discord line per core (#134331). Restores ASI, ini and saves and releases the lock at the end, also on error.
- Output in `-Out`: `cores.jsonl` (one row per core, resumable: rerun with the same `-Out` skips kept cores), `progress.txt`, `launch.txt`, `table.md`.
- `table.py cores.jsonl` alone rebuilds the table: groups flagged (form ends before its last hit, #134324), good, no hit, never threw.
  On the 11-25 survey it gives 64 rows: 2 flagged, 49 good, 6 no hit, 7 never threw (matches notes/soul-core-83.md).

Limits: the pad is blocked while a form is up, so it does not measure movement. The Down-button arm reads the shortcut table at a fixed
game-build offset (see TECH_DEBT). Copies throwing in parallel (pc2/formcopy-134870) would make the same run faster; this command does not need them.
