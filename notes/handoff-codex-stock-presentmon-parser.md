# Stock-compatible PresentMon parser audit

Private offline audit, 2026-10-11. This continues `handoff-codex-rc1761-capture.md`; no ZIP CRC, mod-clock pairing or timestamp-alignment suite was rerun. No game, deployment, settings, bridge, Ghidra, hardware, public publication or source edits.

## Repair and metric contract

`tools/testmatrix/tm.py stats <csv>` reads external CSV only; an EngineFix-absent arm needs no mod log. It selects Nioh3.exe and refuses mixed target PID/swapchain streams, missing identity, malformed target rows and invalid/nonpositive application intervals. It removes the first recorded row from every metric under the benchmark boundary/startup policy; this is an explicit policy, not proof that every first-row stall lies outside the recording. Later real positive hitches remain.

`fps`, `app_fps`, `p99`, `low1` use application/NotSet submission rows and FrameTime (v2) or MsBetweenPresents (v1). Missing FrameType does not prove FG is off, and submission FPS alone does not attest application-rendered FPS with untracked FG. DisplayedTime/MsBetweenDisplayChange are separate display-residence metrics; NA/nonpositive display values are excluded from those metrics, not from submission timing. `display_fps`, `display_p99` and display sample counts are separate outputs. CPU busy excludes CPU waiting; GPU busy excludes GPU waiting. GPUTime is reported separately as `gpu_time_ms` and never substitutes for GPUBusy. Both prefixed and unprefixed input/display latency names are supported. P99 is discrete nearest rank `ceil(.99*n)-1`; 1% low is `1000/p99_ms`, not the mean of the slowest 1%.

The metric definitions and optional frame-type instrumentation are documented by [PresentMon v2.6 console documentation](https://github.com/GameTechDev/PresentMon/blob/v2.6.0/README-ConsoleApplication.md#csv-columns). Actual local v2 CSVs use FrameTime/CPUBusy/GPUTime/GPUBusy/DisplayedTime and unprefixed latency headers. Every inspected local capture lacks FrameType; no runtime FG verdict follows from that absence.

## Existing local CSV checks

These are whole-file parser demonstrations including startup/menu activity, not gameplay windows or performance claims. No historical report or capture was rewritten.

| local CSV | rows/accepted application intervals | audited submission FPS | display samples/FPS | p99 / reciprocal low1 | GPUBusy / GPUTime ms |
|---|---:|---:|---:|---:|---:|
| captures/fcap/pc-20261010-0055/presentmon.csv | 4846 / 4845 | 227.06 | 4322 / 195.44 | 7.10 / 140.94 | 1.32 / 3.86 |
| captures/pc2/raw/20261009-0036-session/presentmon.csv | 28055 / 28054 | 37.03 | 28024 / 36.99 | 58.69 / 17.04 | 25.30 / 25.99 |
| captures/pc2/cbu1/presentmon.csv | 47682 / rejected | — | — | — | — |

Each file contains one Nioh3 PID/swapchain. The cbu1 file contains a malformed target row and now fails closed instead of accepting partial metrics. The older parser counted first-row FrameTime=3362.3773 ms in the PC example (196.19 FPS), and FrameTime=3040.0172 ms in the pc2 session example (36.88 FPS). Those different outputs demonstrate selection effects, not improved game performance. CPU and display metrics cannot be interchanged with submission timing.

## Pairing and provenance

`py -I tools/testmatrix/tm.py paired <pinned_results_directory> pc` is read-only and recomputes stats/coverage from original per-window CSVs. It preserves recorded observation approval and requires valid required window counts/durations, matched artifact hashes, exact graphics XML, one OFF FRAME_GENERATION element, consistent submission schema and protocol scope. A generated/unknown FrameType contradicts the OFF control and refuses pairing. Missing frame-type tracking leaves runtime_fg_off_verified=false. Recorded OFF XML is a configuration precondition, not independent runtime attestation.

Fresh launches are the units. Each run number joins one STOCK and one OPTIMIZED launch; duplicate/missing arms, skipped run numbers and inconsistent pins/pictures/protocol fail. Two adjacent pairs with opposite order form one ABBA/BAAB block. Four preliminary launches therefore give one balanced block, not two independent replications and not thousands of independent frame observations. The JSON exposes pair deltas, block mean deltas, unbalanced tail count and actual time-center mismatch; CI95 and significance verdict remain null. Order alone does not establish equal wall-time spacing, scene equivalence or cancellation of nonlinear drift. Full-protocol flags are preserved.

The existing `tm.py table` remains aggregate medians/ranges without pairing; wording now identifies its comparison as descriptive. Archived summary JSON is not silently recalculated by that command. Do not use its run spread as a significance test. `tm.py paired` is the suitable preliminary audit of the four-launch queue1848, once actual captures exist.

## Other parser and driver limits

`cost_vs_looks.py:71` read_pm and `pc_gpu_report.py:87` pm_stats accept timestamp/FrameTime/GPUBusy without stream or finite-value guards. fastab_report.py calls the former; its primary FPS/frame/rms/hitch metrics require the mod clock, while external GPU busy remains sampled secondary data. It cannot serve an EngineFix-absent primary-FPS arm. Their existing RC1761 use was already independently verified to have one identity and valid selected samples; this unit does not change that pipeline or reissue its verdict. Their window-pair confidence interval is not a between-launch whole-mod confidence interval.

`gfxlib.ps1:54` Set-FgMode silently fails to replace a missing FG XML element. Root was notified to add strict OFF readback in its separately owned pinned driver before launch. A parser cannot establish runtime FG OFF from current CSVs without frame-type tracking. The device worker must still provide observed gameplay, stable scene/route and uncontaminated-machine evidence. Combat, LOAD2/hitches and latency remain outside the preliminary SkipCombat queue.

Validation: `py -I -B tools/testmatrix/test_tm_stats.py` passes 13 tests with multiple negative subcases. Controls cover all first-row timing families, nearest-rank boundary, retained later hitch, invalid intervals, mixed PID/swapchain, missing/truncated identity, generated/dropped display rows, GPUTime-only schema, v1/v2 aliases, complete ABBA block, missing/duplicate/mixed/invalid/short/protocol-mismatched arms and an end-to-end CSV-only pair parse that rejects a shortened CSV despite optimistic stored duration. Existing pinned driver fixture suite passes29 checks. Tests use temporary fixtures only.
