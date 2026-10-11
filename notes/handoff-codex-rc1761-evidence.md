# RC1761 evidence and whole-mod recipe - 2026-10-11

Read-only verification: `py -I tools/bench/verify_rc1761.py` PASS. Both local ZIPs pass every entry CRC; both archived mod logs identify 1.7.61. All 24 manifest measurement windows have successful acknowledgments/readbacks, adequate duration, advancing mod clocks and ABBAAB order. Recomputed paired FPS means, deltas, SE and 95% CI match results.json to 1e-9. Three adjacent A/B pairs per item, one launch per device; these are fixed-spot mod-clock measurements, not independent whole-mod trials.

| Device | CBMODE 0 -> 6 | XPRE 0 -> 1 |
|---|---|---|
| Ally | 41.372 -> 41.451 FPS; +0.19% +/-0.19% SE (neutral) | 41.336 -> 43.055 FPS; +4.16% +/-0.18% SE |
| pc2 | 38.955 -> 45.381 FPS; +16.50% +/-0.16% SE | 38.976 -> 38.805 FPS; -0.44% +/-0.22% SE (neutral) |

Sources: captures/ally/rc1761/ally_rc1761.zip (`up/manifest.jsonl`, `up/results.json`, `up/mod.log`) and captures/pc2/rc1761/pc2_rc1761.zip (`ab/manifest.jsonl`, `ab/results.json`, `mod_run01.log`). ZIP SHA256 respectively:

- Ally: 0b2163ece9ea4e5d98726f95fc53a4150f5ede84495dd304f8823f12f0731ad7
- pc2: 1f96b1f14bb9b21b93c764c9ffd32ac63d21f51957ce69c481fde5a88dfb78d5

Scheduler logs reset both keys to zero and restore each tested key afterward: XPRE was measured with CBMODE=0. The separate CBMODE=6/XPRE=1 combined smoke proves compatibility only. No additive gain or default-stack Performance gain follows. Metadata generically says ASI 1.7.59+ / config as installed; actual version comes from mod logs. No ASI binary/full hash in these archives: phone1840/1842 remain device attestations for the expected full ASI hash, not independently hashed installed bytes.

Ally archived before/after save hash lists are identical. pc2 rc1761.log records combined 2-ok/0-failed acknowledgments, +1374 frames/30s, return to CBMODE6/XPRE0, graceful close, unchanged saves and restored ASI/INI/config matching backups. Ally ZIP lacks the later rc1761_restore_record.txt: full file restoration, SIGTERM exit and combined details remain phone1842 attestations. No new local game test or restoration was needed. Screenshots remain unaudited and do not prove offline visual equivalence. PresentMon/MangoHud files exist, but their window alignment, display-frame metrics and GPU results were not independently recomputed here; only mod-clock FPS arithmetic is verified.

## Bounded preliminary stock-versus-default recipe

This is a release-readiness benchmark at the existing user setup, not completion of notes/final-benchmark-protocol.md's user-present final sweeps. Do not change graphics, hardware, driver, power or runtime environment. Existing FG-off setups may be measured as-is; a setup using FG must be labeled and cannot establish the native 60-FPS goal.

1. Serial device owner acquires the existing game lock (PC waits for Ally shared account). Capture save/ASI/INI/config hashes and backup restoration sources before any launch. Identify scene, build/hash, graphics/upscaler/FG/cap and shader state using read-only evidence.
2. Define A = stock engine: ASI safely moved aside; existing shader state held identical and recorded. If replacement shaders remain, label this engine-only stock control; do not call it full vanilla. B = pinned 1.7.61 smoke-kit ASI SHA256 4734152ACAFDC13FA417C23EE60C8F6579B023D7F93FE73A763D862446A7AD00 with exact kit INI archived, CBMODE6/XPRE0/FO0, CBN1/CBT1/CBS0/CBF0. No device KEEP substitutions or optional Performance mixed into default B. Same scene and game setup for A/B.
3. Use separate launches in A-B-B-A-A-B order (three adjacent paired trials). Warm gameplay 60s each launch, then two fixed-camera 20s windows at the same scene. Discard interrupted/loading/short windows. Stock has no mod clock: use the same external capture metric and parser for both arms. Keep per-run values as the statistical unit; two windows are not two independent launches.
4. On Windows, archive PresentMon schema and actual process/frame metric; FrameTime is available in this pc2 bundle while older protocol names are absent. On Ally, archive MangoHud schema, units and launch-relative timing origin. Compute paired average FPS and p99 frame time, plus A-to-A drift/range. Label submission/display metrics correctly; no GPU or input-latency claim without aligned evidence. Fixed-spot results do not cover combat/load/stability.
5. Close through menu/graceful device-supported exit, wait for process exit, restore backed-up files and verify all file/save hashes before releasing the lock. Archive capture/conditions/restoration and report invalid runs explicitly. No forced kill or new settings to rescue a run. Stop on a closure/restoration failure and finish restoration before another test.

Whole-mod stock/default results remain absent on pc2 and Ally; PC smoke is also per-toggle. Final graphics sweeps, player combat/load/latency/soak, Rematch/co-op/VRR and offline visual proof remain separate gates. FO5 kit225 stays cancelled/held; FO NO-GO. Installguard remains with its existing writer, untouched by this unit.

Next independent unit: audit external-capture timestamp alignment and frame-metric selection in these existing RC1761 bundles before accepting their sampled GPU/display results; do not relaunch or repeat passed mod-clock checks.
