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

Every pipe transaction checks the guard's owner/harness PID-start/deadline,
exact adopted game PID-start/executable, one-game count, foreground HWND owner,
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
