# guard1765 pc2 archive audit - 2026-10-11

Bounded read-only unit: bridge230 (`pc2_guard1765.zip`), responding to pc2 message1877 / queue1863 / private kit229. No game launch, build, deployment, merge, settings change, controller mutation or hold release.

Archive transport hash verified by the existing bridge downloader; ZIP CRCs PASS. SHA256 `03a11e51e47c7c3595cb4d246b60be6c21bddd440c49ea0d35aede4e9a0435d6`. Reproduce with `py -I tools/bench/audit_guard1765.py`.

| Evidence | PCM0 | PCM2 installation-only |
| --- | --- | --- |
| INI SHA256 | 93ca1930165fb49073e0e4cb291c48164936b2b38041b14ee82486f2766b95b6 | e9077db663f64b5a5d74789fd4afd23450bfeefcb7947b85e291a4c654c86bf1 |
| ProcessID | 23092 | 21544 |
| Parsed CSV records | 7938 | 7944 |
| Complete game-frame records | 7937 | 7943 |
| Incomplete final record excluded | 1 | 1 |
| Graceful menu close recorded | 10:58:05 | 10:59:39 |

The INIs differ only at line332, `PCM = 0` to `PCM = 2`. Both native logs identify1.7.65, install render-config/GI/LOD/particle hooks, AutoContinue stock/OFF, menu and FrameGenSettingWrite, CBU CBN/CBT ON and Present hook. No inspection-abort/not-installed/mismatch/ERROR/WATCHDOG matches. PCM0 has no memo installer/stat line. PCM2 installs the memo hook but both stat samples show switch off with zero lookups/hits; serving never exercised.

Both archived guard logs retain FAIL because the live CSV read saw zero rows. The finished CSVs independently contain thousands of positive finite FrameTime records for the matching game PIDs; OSD logs also show rendering. This supports startup/frame evidence despite the live-check failure. Device-reported 7939/7945 counts are physical CSV line counts including the header and incomplete tail, not complete frame counts. The archive contradicts the zero-frame verdict; it does not independently prove the device's proposed buffering cause. CSV wall-clock offsets remain uncalibrated; no timing/performance inference made.

Both logs report installed ASI SHA256 `997c1c0e92bab3e936c033deeca4e53b8b37dd3a3866fb2e2c31eb1cb0c85537`, matching kit229. The ASI itself is absent: installed binary identity remains a device/log attestation. Guard logs record graceful closure, four entry saves unchanged, entry ASI/INI/log/config hashes equal after restoration and lock release. Exit codes and lock-release responses are blank; full backup/restored binary/hash lists are absent. Restoration and lock release remain attestations, not an independent file comparison.

Both logs contain the known Streamline swapchain GetDevice failure followed by drawing on the captured DIRECT queue. No new crash attribution. Menu screenshots are retained as evidence but no visual-equivalence verdict claimed. Startup installation evidence PASS with the above limitations; no field/combat/load/default-stack/latency or performance evidence and no PCM serving validation. Draft PR1 unmerged and stable main/bin1.7.61 unchanged by this unit; FO remains NO-GO.

Next independent unit: prove or bound native first active CBV layout positive-size dirty-slot invariant using the existing reset/recreation notes. A separate guard integration decision can now consume audited startup evidence without treating it as broader runtime coverage. Do not replace any device-pinned build or shared staging.
