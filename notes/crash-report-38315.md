# Crash report writer (bridge #38361, human #38315/#38339)
Module: mod/Nioh3EngineFix/src/crashreport.{h,cpp}, started from dllmain next to bootnotice. Branch ally/crash-report-38315 (off pc/final-38312).
Status: CODE ONLY, NOT COMPILED (ally has no MSVC) and NOT run in game. pc builds, installs and forces the crashes.

Output (beside the .asi, never a save folder; no save data is read or written): Nioh3EngineFix-crash-<date>-<time>.txt + Nioh3EngineFix-crash-latest.txt (+ .dmp if CrashDump=1).
Report: reason, uptime, pid/tid, exe base + known-build flag, sha256+size of Nioh3EngineFix/Training/BossRematch .asi, exception code/address/module+offset (+ AV read/write address), registers, unwound stack as module+offset, SetState() key/values (last mod state), last 48 log lines, loaded module list.
Paths: SetUnhandledExceptionFilter (returns CONTINUE_SEARCH so stock crash handling still runs), SIGABRT, std::terminate, CRT invalid-parameter, purecall, and a 1 s popup watcher (any visible #32770 dialog of the process = engine Fatal-error / crash box; one report per distinct title+text). The filter only hands off to a helper thread with its own stack, so stack overflow works; waits max 8 s.
Not hooked: engine fatal handler 18956D0 itself (no detour; popup watcher covers it). SetState() is available but no callers added yet (only "crashreport", "last_popup", "crash_test"); next: publish boss-match/menu page state from the mod.

INI [Debug]: CrashReport=1, CrashDump=0, CrashTest=0..6, CrashTestDelaySec=30.
Test plan for pc (one at a time, set CrashTest, launch, wait delay s, expect Nioh3EngineFix-crash-latest.txt):
1 access violation (code C0000005, write of 0x0) | 2 stack overflow (C00000FD) | 3 abort() reason "abort() / SIGABRT" | 4 fatal popup reason "error popup shown: Nioh 3 | Fatal error..." | 5 terminate | 6 invalid CRT param.
Check each report has the stack, the 3 shas and the log tail. Reset CrashTest=0 afterwards.
Risks to check on first build: compile errors (untested), MSVC /MT abort behaviour for CrashTest 3, popup watcher false positives (any #32770 dialog, e.g. OptiScaler) — it only writes a file, harmless.
