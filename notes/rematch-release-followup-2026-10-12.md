# Yagyu candidate release-check follow-up (2026-10-12)

Candidate production source remains `34296f3`; no game was launched, installed,
closed, or driven. No user save or configuration was changed. Original full gate
`D:/nioh3-re/reviews/cover-yagyu-progress-full.txt` remains FAIL (2 errors,
3 warnings), and its archived manifest/result were preserved.

With Nioh3 absent, direct `run_replayprof_harness.py` passed unchanged, including
dump and source wiring checks. Its original failure cannot be reconstructed from
the old checker, which retained only its final line; timing sensitivity is a
possibility, not a proven diagnosis.

Direct `run_thrsched_harness.py` ran but failed its raised-thread CPU sum:
39.9% + 42.7% = 82.6%, below the unchanged 85% threshold. Priority application,
worker starvation, and restoration checks passed. The fixture claimed the census
was on a different CPU; production commit `58b8224` deliberately removed that
affinity. A highest-priority census can therefore compete with the five spinners.

The harness now isolates only its own observer/control threads away from its
contention CPU using verified thread-affinity calls. Production source is
unchanged. The direct corrected test passed: three observer/control threads
isolated, raised main 45.0% + dispatcher 44.6% = 89.6%, worker 1.2%; original
thresholds and restoration assertions remain intact. One intermediate test
failed the new isolation guard because its thread handles lacked query access;
that output is retained too. Final handles request set and query access.

The release checker now includes bounded captured stdout/stderr on a failure,
so guard refusals and failed assertions survive. A simulated failing subprocess
verified both streams remain visible and the error count is unchanged.

Evidence under `D:/nioh3-re/reviews/`:

- `cover-yagyu-replayprof-closed-20261012.txt`: PASS, original source.
- `cover-yagyu-thrsched-closed-20261012.txt`: FAIL, original fixture.
- `cover-yagyu-thrsched-isolated-20261012.txt`: intermediate isolation FAIL.
- `cover-yagyu-thrsched-isolated-final-20261012.txt`: corrected fixture PASS.

Engine/addon binaries still match the original archive SHA256 values
`21ce6ee58663dca6b0de2d7f2e3fc4750973bfd852cddd913abde8c622525d85` and
`9145baebfa46b05825c1ced1a8a619ff2994cf645b9f587f2f07911f7a58cfad`.
This is a targeted recheck of the two failed gates, not a new full-suite PASS.
Native Yagyu entry/control/return acceptance and release readiness remain false.
