# Native-first mission arenas ? task70561 / thread70324

Candidate only; no game launch, install, lock or benchmark by cx-pc.

## Base and policy

Branch `cx-pc/native-arena-entry`, based on `pc/bm-any-boss` **190a33b3** (BR **8eae1606** source). Its Training tree is identical to **c481a839**, the supplied TR **0e30328a** source (verified by empty scoped git diff).

Chosen option: every enabled **mission catalogue** Fight-now row takes the real mission request before CrossWorld, FightNowSummon or fade dispatch. No per-mission allowlist or assumption that world-layer dependencies have been fully decoded. The game's native load owns placement/environment/script data. NativeEnter and NativeMissions are retired for this route, including existing user INI values; user INIs are not replaced. No new mission IDs were introduced.

Explicit roster/field `FightNowChr` and summon APIs retain their separate legacy behavior: their caller contract cannot issue a native mission request for an open-field S row. Mission-row preload is resource-only, so a prewarm world switch cannot bypass loading. Yamagata gets the same mission policy as every other catalogue mission.

## Admission, fallback and protection

- BossRematch reports confirmed queue admission to the new Training `Nioh3Training_NativeEntryResult` export. Every early refusal/uncertain request cancels pending auto-entry. **Install both ASIs together**; old BossRematch cannot confirm this new Training watcher.
- Auto-entry requires an observed load, exact running mission, target world, live player, stage5 and successful warp. New entry/Return/leave supersedes old pending work. No warp on unrelated loads; no fallback during a load or unknown game state.
- After the existing three-second settle, warp into the arena. Alive, HP-positive and AI-active native placement ends the watch. An intro/load pauses/restarts the ready-time window; it is no longer incorrectly treated as proof of boss spawn.
- If no ready native boss appears after **30 consecutive ready seconds**, perform one synchronous in-mission arena fallback. Native-first, summon shortcuts, cross-world re-entry and deferred fade recursion are bypassed for that call. Home/keep-gear ownership is not recaptured. Existing ArenaNativeDuplicate handles a later native placement before/while the owned dynamic arena session runs.
- All nine requested fallback rows exist in the shipped Training arena catalogue: D084F D5A7B D63A9 D6B58 D95FA DA30C DB90D DCE5A R9A25. **This confirms route coverage, not a live spawn PASS.** No exception list was added for them.
- The existing unbeaten-mission clear veto is armed **before** the native request, including uncertain outcomes. Its never-arrived timeout cannot drop protection during an owned/uncertain or loading attempt. No code writes story flags or clear bytes. Legacy fade/cross-world failure paths and debug Training fnow no longer send unguarded raw mission requests; use BossRematch Fight now.
- Full native-script story/save isolation remains a live gate: the inherited veto intercepts `Mission::SetMissionClearFlag`, not a newly proven universal persistent-write barrier. Compare saves and exercise a guarded kill on pc/0; offline tests cannot establish every script side effect.

## Ki observation

No new HP/Ki write was added. Native entry logs raw Ki to three decimals once a live, AI-active boss is observed. Relevant existing writers are `boss_vitals.h` BvKiTarget (minimum0.5 for positive max), `boss_vitals_game.h` BvSlotTick (skips broken/zero Ki), and NOFIN/practice drain paths (0.5 floor). Therefore **a true float zero is not explained by those direct writes**, but a displayed integer0 may represent0.5; other native effects/initialization are not excluded. No blanket refill was added: that could erase intentional break state. The hypothesis that native scripting initializes Ki later is unproven. Retest the raw `ENTER ... Ki x.xxx/y.yyy` values and subsequent activation rather than assuming a cause.

## Build and validation

- `mod/Nioh3Training/build.bat`: exit0, final source, Zig Windows build (two pre-existing unused-symbol warnings).
- `mod/Nioh3BossRematch/build.bat`: exit0, final source, portable MSVC setup; existing warnings only.
- `native_entry_test.py`: PASS; executes production EnterTick and route/admission helpers with mocked observations. Covers unadmitted and wrong-mission loads, intro pause, native boss before deadline, missing readiness/player/world/read, failed warp, timeout and one-shot fallback. Integration checks verify native-before-fade and guard-before-request ordering.
- `test_native_attempt.py`: PASS: new4 admission/guard checks,4 standalone returns,15 map ownership checks,96 actual-source transaction checks, plus unreadable-exit-gap regression.
- `run_transition_cover_tests.py`: PASS:42 dojo,48 branch/death/reload,73 cover,200 passive readers,37 presentation checks,20 exact native anchors, emitted-thunk regression.
- `boss_match_persistence_test.py` and `boss_match_timer_test.py`: PASS. Preference persistence/timer tests are not save-file parity tests.
- Existing native-attempt fixture was refreshed for inherited FillStart/Unbeaten dependencies and production admission RAII; test scripts accept `N3_MSVC_SETUP` for the installed portable toolchain. First runs failed on a missing hard-coded toolchain path/stale fixture; corrected runs above pass.

Artifacts (SHA256):

- Training `42a6b6e85fe12f698c0f618c2ad8449652273275cde2cb340c63614426ed0a83`
- BossRematch `3f6015d01c8478a0a733a0d780c5863b9e370b9e523fc501cf552f9d409af368`

## Load-time cost and required live gate

**No measured load-time number from this offline job.** Cost = real native load + up to3 s ready settle to auto-enter; fallback-only rows add30 ready seconds plus existing arena spawn/streaming. Intro/loading resets the fallback window; entry has a five-minute overall bound. Logs report request-to-boss or request-to-fallback milliseconds for pc/0's measured result.

Lead pc/0: install the pair with current human INIs; compare RA0CB content/screenshots and entry time; check Yamagata intro/fight; exercise the nine no-native-boss rows and ensure one fallback/no duplicate; inspect raw initial Ki; verify unbeaten clear veto and unchanged story/clear state after a kill/return. Retain save backup/restore and game ownership with the lead. Public1.8.6 remains unchanged.
