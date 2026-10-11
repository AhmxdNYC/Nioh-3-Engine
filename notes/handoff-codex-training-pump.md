# Training shared pump seam - 2026-10-11, Codex

Private experimental branch `codex-training-pump`, based exactly on
`b2cfe097ad93dd89b4753e49f43a2cfa93c8ea75` (rematch-instant 1.4.0).
Source version is 1.4.1; this is groundwork, not an implemented observer.

`main.cpp` now owns the single `InstallPump` call and `SharedPump` callback.
`rematch::Prepare` retains the early disabled gate and legacy setup including
FIGHT installation, catalogue/self-test configuration and optional rematch hooks.
It marks preparation successful only after FIGHT installs. `SetPumpLive` publishes
successful installation through an atomic flag; `Pump` calls the original Tick
only while live. UI startup still depends on successful pump installation. The
existing hooks.cpp thunk, original callee chaining and reentry guard are unchanged.
Preparation logs now say prepared rather than on. No observer-only startup exists
in this unit: a future observer must prepare/register before FIGHT installation,
share this dispatcher even when Rematch is disabled, and never call rematch setup
or Tick for an observer-only run.

Validation: test_pump_lifecycle.py extracts and compiles the actual startup,
dispatch and live-publication bodies under MSVC /W4 /WX. 21 behavioral checks
cover disabled/failed preparation, failed/successful pump installation, a callback
during installation, repeated ticks, disabling live and unprepared activation.
Static wiring checks enforce one install in main, no install in Prepare, the
Rematch disabled gate preceding stale SelfTest reads, and UI's live gate. Prepare
itself is checked structurally, not executed in this controlled harness.
Existing rematch harness: 60 checks PASS, including original pump/chaining;
pattern suite: ALL 94 SITES OK with shifted-image simulation. Release build PASS
without warnings; git diff --check PASS. These are offline gates, not native
runtime compatibility or performance evidence. No training observer, receipt
queue, valid-or-unknown snapshots, epochs or event telemetry yet.

No game/Ghidra launch or runtime restoration needed. Build is isolated; no
installed files, benchmark-pinned builds, defaults/visuals, saves, settings,
hardware, other worktrees or controller/holds changed. No Claude invocation or
wake changes. PC preflight inbox1886 remains NOT READY at 5.4 GB headroom below
6 GB; no retry or user-app change. No new sender=user steering at checkpoint.
Remote default-stack and player gates remain pending; stable EngineFix 1.7.61,
private guard PR1 unmerged and FO NO-GO remain unchanged.

Next bounded unit: implement default-OFF observer prerequisite registration,
validity-aware copied snapshots and epoch/event state using this dispatcher;
finish offline acceptance before considering an authorized runtime test. Separately,
v4 SpawnRestore drops ownership even after failed writes/unreadable records; its
ownership proof/repair remains a mutation gate, not fixed by this pump refactor.
