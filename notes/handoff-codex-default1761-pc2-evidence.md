# pc2 default1761 archive audit - 2026-10-11

Read-only bridge231 unit, following queue1882 and result1928. Transport downloader verified SHA256 `617b378790b399d70eb14fb1a6e87699c9f45cf0f2d20d29d33fa8941c3c7244` (4841594 bytes); ZIP CRC PASS. Reproduce: `py -I tools/bench/audit_default1761_pc2.py`. Archived scripts were read, never executed.

| Launch | Arm | Window frame counts | Launch FPS | Mean FrameTime ms | Window-mean p99 ms |
| --- | --- | --- | --- | --- | --- |
| L1 | A | 597 / 597 | 30.1273 | 33.1797 | 35.1433 |
| L2 | B | 897 / 897 | 45.1412 | 22.1552 | 23.6219 |
| L3 | B | 899 / 900 | 44.9751 | 22.2358 | 24.0083 |
| L4 | A | 599 / 598 | 30.1079 | 33.2242 | 35.4175 |
| L5 | A | No field/windows | Invalid | Invalid | Invalid |

Independent CSV arithmetic matches the archived report: mean paired FPS difference +14.940526 (+49.6073%), SE0.073337 over two adjacent launch pairs (L1/L2, L4/L3). Mean FrameTime difference -11.006454ms, SE0.018113; window-mean nearest-rank p99 difference -11.465325ms, SE0.056075. Each launch averages two window statistics; individual frames/windows are not independent paired units. A-to-A FPS range0.0194; B-to-B range0.1661. Two pairs are preliminary and SE does not bound systematic uncertainty.

Eight spans are approximately20s and start within0.5s of field+60/+85s. All selected records match the recorded game PID, one swapchain per launch, Hardware Composed: Independent Flip, SyncInterval0 and AllowsTearing1. Metric is PresentMon v2 FrameTime/present cadence, not displayed FPS or input latency. Clock alignment uses the device-reported +14h offset: raw timestamps give zero frames at archived UTC windows, shifted timestamps reproduce counts. This establishes coverage/arithmetic consistency, not independent clock calibration; benchmark interpretation remains conditional on that mapping. Screenshots exist with JPEG headers; endpoint field labels remain classifier/device attestations, not a fresh visual review or equality proof.

All five cells attest equal entry hashes and unchanged config/loader/GHDM/fogpar hashes. A attests ASI absent; B attests exact pinned1.7.61 ASI and default kit INI; both B native logs identify1.7.61. Installed binaries/config/INI/backups are absent from this archive, so these remain device attestations, not independent binary comparisons. A retains GHDM and loader, hence engine-only stock control rather than full vanilla. User settings and slow-link B550 scope remain those of result1928; no universal/combat/load/visual claim.

Important recovery boundary: L1-L4 cells/logs attest graceful CloseMainWindow, equal saves/files and lock release (exit codes blank). L5 cell says `graceful close TIMED OUT - left running`, `NOT RESTORED (game running)`; its log contains no release. Driver stops the sequence and no L6 exists. Result1928 later reports fatal0xC0000005, exact PID20904 killed after failed dialog closure, restored entry hashes and released lock. The archive predates that reported recovery and contains no recovery hash manifest or release receipt. Do not describe archive231 as proving final restoration. The crash dialog image is retained but AMD-module attribution remains the later device report; no root-cause conclusion.

No game/Ghidra/capture session, build/deployment, save/settings/hardware/controller/hold changes or Claude invocation/wake. Existing device-pinned inputs/worktrees untouched. No unchanged missing-pair retry authorized. Follow-up evidence gate: obtain a separate post-crash recovery/hash/release record and independent clock-origin calibration before upgrading this conditional result. Independent source work remains ready: default-OFF training observer on owned codex-training-pump c297a20. Stable1.7.61, guard PR1 unmerged, FO NO-GO retained.
