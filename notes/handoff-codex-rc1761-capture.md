# RC1761 external capture audit - 2026-10-11

`py -I tools/bench/audit_rc1761_capture.py` PASS, read-only against the existing RC1761 ZIPs. This unit does not repeat the prior CRC/mod-clock suite or run a game.

pc2 PresentMon contains one Nioh3.exe PID21868/swapchain0x16F31DCB020. Its single truncated terminal row is excluded; it is outside measurement windows. Among whole-hour offsets -14..+14, only +14 hours puts any rows into all manifest UTC windows: 7,306 samples versus zero unshifted. This reproduces the current report's overlap-based alignment policy (tools/testmatrix/pm_time.py). The offset is inferred, not an independently recorded UTC calibration; its timezone/double-offset explanation is not established by this archive. Device logs are at UTC-7, while CSV wall timestamps are at UTC-14.

All 12 windows contain 581..684 valid GPUBusy samples, exceeding the report's 200-sample gate. Counts/mod-clock frame deltas range 0.991..1.017; maximum adjacent timestamp gaps range 0.024..0.061s. Counts close to the mod clock do not prove exact frame correspondence: LiveCtl acknowledgment intervals and manifest wall intervals differ, with one XPRE A window above unity. These RC1761 windows do not exhibit the older report comment's two-thirds loss; that historical comment is not evidence about this capture.

| pc2 fixed-spot paired A/B | Sampled GPUBusy A -> B (ms) | B-A +/- SE (ms) |
|---|---|---|
| CBMODE0 -> 6 | 19.134546 -> 18.621534 | -0.513012 +/-0.021586 |
| XPRE0 -> 1, CBMODE0 | 19.158201 -> 18.680280 | -0.477921 +/-0.018127 |

Each uses three adjacent pairs in one launch. Independently recomputed means/delta/SE/95% CI/percent match archived results.json to 1e-9. Conditional on inferred alignment, this verifies the already reported sampled GPUBusy arithmetic. It adds no whole-mod, total GPU duration, display FPS, input-latency or new visual verdict. FrameTime, GPUTime, GPUBusy and DisplayedTime are distinct columns. Archived fps/frame_ms/rms/h33 use the mod clock; they are not display metrics. The verifier deliberately does not use GPUTime or DisplayedTime as GPUBusy substitutes.

Ally MangoHud has 5,138 samples with strictly increasing elapsed101282693..514984839231, median increment100139929. Its CSV contains neither a UTC origin nor an explicit elapsed-unit declaration; the spacing is consistent with nanoseconds at roughly10Hz, an inference only. The mod startup stamp is not a capture-origin attestation. No per-window MangoHud result can be accepted without that origin. gpu_load is a percentage field, not GPUBusy ms, and archived gpu_ms remains null. Preserve the no-data verdict.

Needed future capture provenance: record the external tool's timestamp schema/units, actual UTC start anchor and tool version; archive the chosen offset and validation rather than relying solely on maximum-overlap fitting. Stock/default external parsing must work without mod-clock rows, distinguish submission/display metrics, and keep per-launch statistical units. No settings changes are needed to obtain provenance.

No local game/capture process at entry or checkpoint; no lock acquisition, Ghidra, build/deployment, installed-file/save/settings/hardware/controller changes, or Claude invocation/wake. Phone inbox empty at checkpoint. FO remains NO-GO; kit225 held/cancelled. Whole-mod runs and player gates remain external.

Next independent ready unit: audit stock-compatible external frame selection and p99 calculation in the existing benchmark parsers against existing captures, without launches or repeating passed RC1761 checks. Fence/producer-closure static work remains another ready topic; installguard source ownership remains unresolved.
