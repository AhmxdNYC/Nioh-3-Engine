# Fight-cover camera witness helper

Offline-prepared helper for the lead's existing guarded PC pilot. It does not launch,
focus, approve a stage, return, retry, alter configuration, or keep the player alive.

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools/rematch/run_camera_witness.ps1 -RunDir <owned-pilot-directory> -OwnerPid <lead-pid> -Token <expected-new-cover-token> -Run
```

Arm before that token reaches ready. Without `-Run`, only a plan is printed.
The exact token/mission1CBC/start67064/placement65092, nonzero attempt, eight
ready witness bits and native sample age <=750ms are required. Lower tokens wait;
a newer unexpected token fails. A phase4 snapshot remains historical: short age
plus current basic status permits a brief probe, never establishes visible control.
The lead must still inspect the images and issue the existing manual approval.

Before polling, retained read-only kernel process handles bind the guard's exact
owner/harness/game creation FILETIMEs and the game executable. Every pipe transaction
checks the unchanged guard record/deadline and the retained handles' liveness,
a complete native Toolhelp single-game enumeration, foreground HWND owner,
and named-pipe server PID. No focus is attempted. Before capture and again before
input it checks native/current status. Input connection time is included in the
750ms freshness gate. It sends exactly `stick R 0.35 0 180` to the already-enabled
VirtualPad, then captures the foreground monitor again. The before bitmap stays in memory;
both PNGs are encoded only after the pulse reply and after capture. Neither
encoding nor file writes delay the pre-input freshness check. The original
capture order and all identity/foreground/pipe checks remain unchanged. If capture
or guards alone exceed the freshness limit, input is refused; the bound is never
extended. Other windows/overlays
may still affect a screenshot; images alone do not establish seamless presentation.

Native implementation references: `vpad.cpp:388` WaitTimed cancels its own timed
entry after duration+3000ms if pad reads stall; command handler at481-490 waits for
completion. Timing starts with pad consumption, so a180ms command is not a guaranteed
180ms wall-clock action. The helper waits3800ms for its reply; no global stop or
button hold is sent. OS scheduling/foreground/identity races between checks and
input cannot be eliminated by this helper. Guard state and logs remain authoritative.

Output is a unique camera-witness directory with before/after PNGs and a receipt;
no existing pilot files or approvals are replaced. Failure never retries the pulse.
Receipts distinguish a failed pre-input operation from failure after a pulse attempt;
`pulse_attempted` does not prove delivery, while `pulse_confirmed` requires the
existing exact success-reply pattern. `images_saved` requires both PNG saves.
An encoding failure cannot produce a completed-success receipt or replay input.
Both bitmaps are disposed on every orchestration exit; a failed capture disposes
its local bitmap/graphics before propagating the error. Partial PNGs may remain
with a failure receipt and must not be treated as a completed witness.
A successful command receipt is not a claim that the camera visibly moved or the
player survived. Root review should be immediate; this does not solve boss damage.

Verification: `test_camera_witness.ps1` exercises the actual orchestration with
in-memory disposable fixtures: object identity and capture/encode order, both
check failures, before/after capture failure, ambiguous pulse/invalid reply,
either PNG encoding failure, disposal failure, slow encoding, and slow capture
refused before input. The unchanged exact 750ms boundary and ready policy are
checked; the real adapter is parser/AST checked for capture-only and save-only
callbacks and the unchanged single command. No live pipe, screenshot, input or
runtime speed improvement is claimed by these offline checks.

## Retained identity timing correction (2026-10-12)

Root3 failed before input between 05:58:23.434 and 05:58:24.619 UTC. Its
phase4 sample is terminal: new `cover` reads cannot renew native readiness.
The prior helper repeated Get-Process/start/path/window discovery in every guard.
The revised helper opens retained identity handles before ready polling and uses
nonblocking native liveness/foreground checks plus complete Toolhelp enumeration.
The guard JSON, exact pipe-server PID, current state and pre-write <=750ms sample
checks remain. Output-directory setup also occurs before ready polling. This
removes repeated expensive process-object discovery, but does not prove that
actual capture/transport will always finish within the bound. Slow operations
still refuse. Receipts add check/capture/input tick times and input sample age.

`camera_witness_process.cs` is required alongside the runner/lib. Both retained
handles and the temporary startup Process object dispose on every exit, including
partial construction failure. No access for process mutation is requested.

`test_camera_witness_process.ps1` runs the existing107 checks plus retained
self-process identity positives/negatives, guard record mutations, and the actual
runner Check callback under injected guard latency. The slow path refuses without
input; the fast path allows one simulated pulse; a delayed pulse still refuses.
These are timing-policy fixtures, not live performance measurements.

No automatic pause is added. Root may explicitly pause after viewing/retaining
the successful after-capture, then review and separately request owned return.
An automated post-capture pause would need its own fresh exact ownership and
input admission and could alter the return gate; it is not hidden in this helper.
