# v4 spawn restore ownership audit - 2026-10-11, Codex

Audited actual `Spawn` / `SpawnWrite` / `SpawnRestore` bodies pinned at
`c297a20` (`codex-training-pump`, experimental add-on 1.4.1), inherited from
rematch-instant v4. No production source repair or add-on build/deployment.
Reproducer: `py -I tools/bench/audit_spawn_restore_ownership.py`.

MSVC /W4 /WX compile and 33 behavioral checks PASS. Five injected failure
witnesses reproduce lost restore bookkeeping, with a success control and three
foreign-record controls. This is hazard reproduction, not a safety verdict.

| Injected condition | Residual record | Bookkeeping / later recovery |
|---|---|---|
| Coordinate write fails | Override coordinates and flags remain | st=0, rec=0; recovered retry writes nothing |
| Flag write fails after coordinate write | Original coordinates, override flags | Same; partially restored record cannot be retried |
| Coordinate read fails | Override coordinates and flags remain | Same; readability recovery does not recover ownership |
| Flag read fails | Override coordinates and flags remain | Same |
| Lookup returns zero temporarily | Override coordinates and flags remain in controlled memory | Same; lookup recovery cannot restore |

`SpawnRestore` clears st/rec/landCheck unconditionally, including after
`SpawnWrite` reports failure. Failed reads and a missing lookup share the
"no longer ours" log path with actually different bytes/identity. Those failures
do not establish native destruction or retirement. The harness preserves a
controlled record across lookup failure; native lifetime/reachability is not
proved. The write-failure log itself includes WRITE FAILED but does not retain
cleanup responsibility.

Success restores both fields and releases state. Changed coordinates, changed
flags, and a different lookup address cause zero writes in controls. That only
proves current comparisons refuse those writes; it does not prove that releasing
all cleanup responsibility is correct for mixed fields or pointer reuse.

Source-only integration implications: `ToIdle` calls restore then publishes idle;
new Fight-now startup calls restore then can arm a fresh override. `SpawnApply`
also has unchecked coordinate rollback on write failure and resnapshots original
bytes on reapplication. Those paths were inspected, not executed by this bounded
harness, and remain separate acceptance obligations.

Next mutation repair must distinguish restored / proven superseded / unresolved,
retain baseline and unresolved cleanup ownership across failed reads/writes,
track per-field partial writes, and block a new mutation while cleanup is
unresolved. Re-resolve and validate current record identity before any retry;
never write a stale saved pointer. Audit same-record game rewrites, record
replacement, partial application/rollback and baseline resnapshot before calling
v4 ownership safe. A read-only observer remains independent of this mutation gate.

No local game/capture/Ghidra at entry or checkpoint; no restoration needed.
No pinned builds, installed files, saves, visuals, settings, hardware, other
worktrees, controller state or holds changed. No Claude invocation/wake changes.
Checkpoint inbox empty, no new user steering. Device default-stack/player results
remain pending; stable EngineFix 1.7.61, guard PR1 unmerged and FO NO-GO retained.
