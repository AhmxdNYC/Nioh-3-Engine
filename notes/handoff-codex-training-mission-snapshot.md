# Training mission snapshots - 2026-10-11, Codex

Private `codex-training-pump`, parent b68377a; add-on 1.4.3. Narrow read-only
continuation, no deployment or public sync. Existing default-OFF registration,
known-build/CORE/FIGHT/PUMP gates and single dispatcher remain prerequisites.

Pump samples the resolved CORE MissionMgr global, manager+8 state, then state+28
mission and state+14 retry using actual SEH-guarded Rd. Fixed hops are checked for
null/low addresses and addition overflow; no game pointer is retained in telemetry.
Each scalar has independent validity. A failed read publishes unknown with a zero
payload; readable zero remains valid. Snapshot() copies cached local POD only and
returns false with an empty output before publication or while disabled.

The cached sample contains publication sequence, GetTickCount64 milliseconds and
an observer mission epoch: first readable identity starts epoch1, subsequent
different readable mission identities increment it. Retry changes do not increment
mission epoch. Missing mission reads keep the last valid identity for comparison;
missionGap marks unknown samples and the first recovery sample. It clears on the
next valid sample. Same-identity recovery cannot prove that no mission transition
occurred during the gap. Zero mission is a readable identity, not proof of a field
session. Native values are sequential reads, not an atomic native-state snapshot.

A local SRW lock protects publication/copy and epoch bookkeeping. Pump uses a
nonblocking exclusive try; contention skips sampling/publication and increments
SnapshotLosses. Sequence counts completed publications, not all pump entries.
Loss totals are independent atomic reads, not part of a coherent snapshot; skipped
samples can hide native transitions. No synchronous logging/native calls/writes,
actor argument traversal, UI or rematch mutation was added. Entry counts retain
their prior independent-atomic limitations.

Validation: actual training.cpp and actual addon.h Rd against allocated memory and
PAGE_NOACCESS pages, MSVC /W4 /WX:20 checks PASS. Includes default-OFF/no site lookup,
zero identities, identity/retry changes, both partial-read directions, null state,
overflow pointers, gap/recovery, contended publication/loss and disabled cached
queries. Actual dispatcher/lifecycle143 checks PASS with production declarations.
Existing add-on60 checks PASS, ALL94 SITES OK including shifted-image simulation,
and isolated release build without warnings PASS. Diff check PASS.

Not a complete observer: load/session generations, coherent copied session/queue
evidence, bounded scalar events/loss, consumer query transport and native runtime
acceptance remain missing. No live actor, attempt identity, readiness, FPS or visual
claim. Next bounded unit: separately valid session/loading scalars and explicit
loss/recovery semantics before adding their generations. v4 restore ownership is
a separate mutation gate. No installed/pinned build, settings/save/hardware,
controller/hold/device-owner/Claude change.
