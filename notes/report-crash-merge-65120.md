# Automatic crash and typed-report integration

Task #65120 merges `pc/auto-crash` d33ce5ac and `pc/report-textbox` 5d7cbb26 on the requested `origin/gxl-pc/release-185` base. Merge commit 3a33fc02 retains both parents.

Both senders are compiled by build.bat and CMake. EngineFix starts crash reporting before the typed-report sender, preserving both startup paths. Crash recording, queued automatic uploads, menu text entry, controller keyboard, local report copies and report submission are retained.

`crashupload::EnsureInstallId(iniPath)` owns identity initialization under an SRW lock. Both senders use its value; only crashupload writes `[Report] InstallId`. reportsend no longer reads or creates `install-id.txt` and no longer has an independent ID generator. If ini persistence fails, the two features still use the same process-local ID.

The deploy root is `site/`, with `api/crash.py`, `api/report.py`, and one `vercel.json`. The report route imports `feedback-api/report_api.py`, explicitly included by the root deployment configuration. The nested duplicate route/config were removed. The report GitHub storage fallback is self-contained because the source branch referenced a `server.py` that neither branch shipped. Its issue/comment path is exercised with a mock, with no GitHub write.

## Validation

- `mod/Nioh3EngineFix/build.bat`: exit 0 using the existing portable MSVC toolchain. EngineFix SHA256 `2d230b38fc8f28ee36021e3f47fdd5483b10294922aeadb65d6369b2deb33836`.
- `tests/run_menu_harness.py`: both actual menu harnesses passed, including the Report box path.
- `tests/run_reportsend_test.py`: passed sender checks plus shared ini identity generation/readback/reuse, linked against the real crashupload implementation.
- `tests/run_crashupload_test.py`: passed the actual C++ crash record test, supplied `site/tests/crash_api_test.py`, and a localhost upload/retry test. Exactly one pending record was accepted and removed. The generated record's ID matches the ini. Production storage credentials are removed in the test process; the server binds only localhost.
- `site/tests/report_merge_test.py`: 3 tests passed: both deploy entry points import, report schema/redaction/local persistence works, and mocked GitHub issue/comment storage works without an external server module.

No game launched, no install changed, no site deployed, no release published. The built candidate is at `D:/nioh3-wt/report-crash-merge/mod/Nioh3EngineFix/bin/Nioh3EngineFix.asi`. The companion's installed public v1.8.4 remains unchanged. pc owns integration/game verification and any subsequent deployment.
