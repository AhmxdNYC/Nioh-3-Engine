# MW meshlet completion wait candidate ? 2026-10-13

Status: implemented default OFF, not installed or game-tested. No CPU/FPS benefit claimed.

PC2 acknowledged candidate376 in2973. Adapter377 source hashes and independent
metrics/cleanup review passed. GO2978 authorizes one OFF/ON/OFF series using the
same candidate ASI, three launches maximum,30s measurement and45s settle per arm,
20min total, fresh entry restoration. STARTED/result still pending at admission.
Collector errors and actual CPU/frame coverage must be reviewed before a verdict.

## Behavior

`[Patches] MW = 1` opts in at startup only. Reader3FD5C replaces the existing Sleep(0) call after256 stock polling iterations with one WaitOnAddress round on the actual manager+5F8 byte. The original3FD46 comparison remains authoritative after every return, including timeout/spurious wake. The round requests1ms, which is not a guaranteed wall-time bound. No extra spin phase or simulation/quality change.

Writer981211 executes the original byte-zero only after4A2D0 returns, then WakeByAddressAll on that exact computed address. It makes no further manager dereference. The root job and all its child work remain intact. This differs from W6's Present flag; W1/W2 fix write-combined stores, not this wait.

The paired group validates both unique patterns, original bytes, IAT target, job branch, native register/stack ABI and writer ordering. Missing APIs or failed prechecks retain stock. MW-only installation requires complete thread enumeration/open/suspension/context capture and a bounded allocation-free kernel census rejecting new uncaptured threads. Replacement readback and any original-byte rollback occur before threads resume. An unrecoverable rollback/cache-flush failure is reported unresolved, never falsely as stock restored; helper pointers remain valid. Other patch groups retain their existing installation behavior.

Reader cave passes RSI (manager), preserving RSI/RBX through normal x64 ABI. Writer cave passes RAX (manager); next native instruction zeroes EAX, and RBX remains preserved. NOP padding precedes calls so return addresses are native instruction boundaries.

## Validation and qualification

- `tests/meshletwait_harness.cpp` tests actual Windows WaitOnAddress/WakeByAddressAll: completion before waiting; clear during enrollment; disabled/missing API fallback; timeout returns without clearing; wrong-address and spurious correct-address wake cannot satisfy the native outer loop;50 concurrent completion cycles.
- Same harness injects publication corruption, validates both-site rollback, original mismatch refusal and explicit unresolved cache-flush failure. Pure census classification rejects unknown/invalid thread IDs.
- `tests/test_meshletwait_binding.py` checks exact native reader/writer/prologue/branch/epilogue bytes and cave register bindings.
- Full pattern suite: ALL257 SITES OK,140 dependency ranges match.
- Final Release x64 candidate build PASS. ASI SHA256 `3933e76c10f9b7ea00448dfe7de03119ca4c17d739f65bf43b94e3d98dbd0462`,1755648 bytes. Existing C4456 shadowed-local warning retained; no compile errors. Build output alone does not validate gameplay. Logs: `mod/Nioh3EngineFix/build/mw-build.log`, `mw-native-tests.log`, `mw-binding-tests.log`, `mw-generated-check.log`.

During the first live trial, require `MW: ON`, actual positive wait rounds and wakes. Zero rounds means this branch was not exercised. Capture process CPU, frames/pacing and available GPU/memory metrics together. Compare equivalent gameplay with MW disabled/enabled; wait percentages from earlier sampling are not achievable CPU savings. Preserve all other settings and normal save progress.

MW cumulative wait-round, timeout, wake and error counters are emitted by the existing30s stats logger. No hot-path logs or new measurement thread. Startup-only design means separate launches for OFF/ON; no runtime menu toggle.

API semantics: [Microsoft WaitOnAddress documentation](https://learn.microsoft.com/en-us/windows/win32/api/synchapi/nf-synchapi-waitonaddress) describes value comparison and spurious returns requiring a recheck; the native outer comparison supplies that recheck.

PC2 result2987/evidence378: first MW0 arm stopped before CPU measurement because PresentMon2.x produced no CSV, including after graceful exit0; ON unrun. Independent13files/exact6saves restoration PASS, collectors exited without force/errors, lease free. Candidate efficacy untested. Follow-up: verify PC2 PM version/hash, omit optional track_frame_type for existing FG-OFF configuration (upstream CX-PresentMon2.6.1 reports a matching WaitingForFrameId output-stall fix), preserve CPU capture independently. This is a suspected recorder defect, not established cause or candidate failure. Upstream: https://github.com/CXWorld/CapFrameX/releases .


Main manual launch 2026-10-13 02:41 EDT: exact3933 candidate and MW=1 resolved both sites, but installer exhausted200 attempts and retained stock. No MW ON acceptance. CPU/PDH captured; PresentMon initially showed zero via directory metadata but metadata-handle checks later proved growing CSV (2.1->2.5MB). Do not call this a persistent recorder stall. User requested actual fix; game21116 closed gracefully with progress preserved, capture exited cleanly. Evidence D:/nioh3-re/mw-manual-364c5e2-20261013. Installer reason diagnostics in progress, no safety bypass. PC2 adapter379 reviewed and GO2996 ACK2997; correction2999 requires actual MW publication proof. UO capture optional no-frame-type/independent CPU fix committedbcac660,192 checks PASS.

Activation diagnostic build (main only): ASI5eac03d1010e3f2faf2d72ad413cc6c4f0832c0c8eff4eacd589cb170bde336e. ApplyBatch now summarizes precise rejection counts/TID/error/status/site AFTER resume; gates and publication semantics unchanged. Build PASS, existing C4456 warning. Staged for user-requested restart at D:/nioh3-re/mw-manual-diagnostic-20261013; Steam syncfailed prompt awaited at02:46 EDT. No MW activation claim until runtime proof. Original user mod/save backup remains in first manual root; normal progress never rolled back.

Main diagnostic startup15444 identified repeatable coverage mismatch: snapshot0/open2/owner0/suspend0/context0/RIP0; native census199, unknown199, lastTID8992; no original mismatch/write/rollback. Initial Toolhelp enumeration omitted a thread visible to NtGetNextThread; underlying reason for omission is unproven. Correction underway: MW-only initial native enumeration with pinned owned suspend/context handles and unchanged final census, never ignore omitted thread. User then reported open-world killing only, no boss rematch. Capture375.185s,3244.672 CPU-s (8.648cores across startup+play), all collectors0/errors[], normal saves retained. Game left open pending corrected build. PC2 separately reports3011 oldcandidate paired ON installation succeeded2sites/1attempt, field counters still pending.

Correction built: ASI08557e398c760274253860072107439bcbf498b4ca1fcee4f75baf71910380af replaces MW-only initial Toolhelp/reopen enumeration with NtGetNextThread retained handles (suspend/context/query rights), validates every owner/TID, keeps final frozen native census and rejection/rollback. Other patch groups unchanged. Exact production block extracted into standalone owned-worker harness: capture/self-exclusion/suspend/context/resume PASS; full build PASS. Later read-only main list comparison242/242 showed no divergence, so no permanent hidden-thread or anti-tamper causal claim. Main15444 exited normally after user finished; fresh replacement launch at D:/nioh3-re/mw-manual-native-enum-20261013, runtime acceptance pending.

Important integration correction: initial MW main-based builds wereEngine1.7.61 and omitted user-hosted menus from installed1.7.73; reported and corrected, never silently claimlatestmenus. Combinedport2733104 based34296f3 preserved11menu/cover/osd files and exactaddon9145ba, both actualmenuharnesses andMW50cycles/bindingPASS. ASI4cec2ae50f0f6d50d81e480c615cf46ea0d3bc6e0fccbfbab64ba94901104207 startup23616 onMAIN stillSKIPPED: native snapshotgood, SuspendThread200/error5/TID2264. LaterOpenThread2264 returned87; notproof ofexitduringinstallation. Agent checking positivelysignaled terminatedhandles, no skippingaliveunsuspendablethreads. F7addon1.4.21 andF10engine1.7.73 menuinitializationlogged. Gameclosedgracefully, no savesrestored, capture0/0errors[]. UserrequestedDiscordwebhook onlywhencombinedactuallyready; notsentREADYyet.

MAIN actual activation SUCCESS: combined commit592a888/ASI41553b09a131903f959afee5bd6ad7b5e1b92a217072c507b29a4fe36948984e. Game12332 start2026-10-13T07:11:19.7058307Z. At03:11:23.108 EDT bothMWR/MWWpatched,2sites/1attempt, MW:ON; no failures,2 positivelysignaled terminatedthread observations. F10menu-ready andaddon1.4.21/FightNowON logged. CPUJSONL,PDHGPU/VRAM,andactualPMCSV1,046,234bytes confirmed. Root D:/nioh3-re/mw-manual-hosted-final-20261013, runtime-ready.json. Startupwaitrounds0 before userplay, no gainclaim. Discord/phoneREADY3028 submitted. Steam-only prompt observed/clicked once after exactwindow guard; no game screenshots or gameplay input. User normal progress preserved; game left open recording. Original RA4B4deathretry unchanged.

Completed combined manual session, userreportedtwo bossfights+openworld then requestedclose. Exactgame12332 CloseMainWindow accepted; exitconfirmedafterinitial10s wait, watcherphasegame-exited/lease released.757.531s recorded,80,523framerecords; collectors0/0/errors[]. PrecloseMW totals13,493rounds/6,643timeouts/35,560wakes/errors0. Positiveactivityconfirmed;1ms requestedtimeouts are expected native-loop rechecks, not falsecompletion. Current8savefiles copiedandhashverified under user-exit-saves, no rollback. Logs/INIs/completion.json retained. Rawbosslogs showTakedaShingen03:20:00.783?03:20:33.019 andMinamotonoYoritomo03:22:17.710?03:22:57.429 withclears/returns; alignedanalysispending. Discord/phoneRESULT3031.
