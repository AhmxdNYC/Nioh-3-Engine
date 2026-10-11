# ApplyBatch production failure audit - 2026-10-11

Completed one bounded offline proof unit, using verbatim ApplyBatch extracted from pinned main8c45cd0. No repair merged or ASI built/deployed. FO remains NO-GO; main1.7.61/FO0/default visuals remain unchanged.

`py -I mod/Nioh3EngineFix/tests/run_applybatch_failure_audit.py` compiled with MSVC /W4 /WX and exited0. Seventeen cases include nine expected pre-fix safety failures: snapshot, first enumeration, partial next enumeration, OpenThread on first/second participant, SuspendThread on first/second participant and GetThreadContext on first/second participant. Every witness returns success, performs two site writes and one cache flush, and changes bytes despite incomplete inspection. Each also has balanced handles/suspensions and restored protections. These are reproduced hazards, not release safety passes.

Positive/refusal controls cover normal bytes plus aligned VPtr writes, encoding failure, second-site protection failure, persistent instruction-pointer clash, pointers exactly at site start/end and outside sites, and empty batches. Two local synthetic sites and two relevant participants are used; self and foreign-process threads are filtered. The actual ApplyBatch body is extracted rather than rewritten in the test. Surrounding BuildRepl, Win32 APIs, logging and memory operations are stubbed. No real scheduler, thread-creation race, API cleanup failure, game crash attribution, post-write rollback or runtime safety is proven. Optional --source supports an unrefactored candidate; the new installguard helper needs its own actual-production-code proof.

## Source ownership collision

D:/nioh3-wt/codex-install-guard was clean at initial read on branch codex-install-guard8c45cd0. During the unit, another writer replaced ApplyBatch with installguard::Write and created src/installguard.h. This lead stopped source edits and moved only its own newly created test files to main under distinct failure-audit names. The lead's interim direct repair was replaced by that writer; it is not committed or claimed complete. Other writer source/helper remain untouched. User ownership clarification is pending; do not edit this repair concurrently or merge/deploy it. No Claude call or auto-wake operation occurred.

## Device reports reconciled

Phone1831 (Ally) reports smoke1768 PASS: ASI1.7.61 SHA4734152A, one launch, user preset/FG off/Futamata. Three paired15s mod-clock windows: CBMODE0/6 +0.19% +/-0.19 (neutral); XPRE0/1 +4.16% +/-0.18. Phone1835 (pc2) reports smoke1767 PASS: kit223, one launch, CPU-bound Continue spot,1080p HIGH/FG off. ABBA x3/15s windows: CBMODE0/6 +16.50% +/-0.16 SE; XPRE0/1 -0.44% +/-0.22 SE (neutral). These are device-reported A/B results, not this lead's independently inspected captures. pc2 retains the existing slow chipset-slot link caveat; no hardware change is proposed.

Both reports say combined30s checks advanced the frame clock, had successful live-control acknowledgments and normal pictures, then restored XPRE0/CBMODE6 and closed gracefully. Saves and installed ASI/INI/config hashes reportedly match backups. Ally HOLD; pc2 idle/lock released. Reports reference ally_rc1761.zip and pc2_rc1761.zip on the bridge; archives were not downloaded/verified here. Animation-floor screenshots do not prove byte-exact visual equivalence. Optional Performance remains optional; defaults unchanged.

Smoke1767/1768 are now reported passed; stock-vs-default whole-mod benchmarks, archive verification and final packaging remain open. FO5 kit225/1781 stays held/cancelled and is not unlocked by these smokes. Rematch/co-op/VRR player gates remain pending.

Checkpoint: no local Nioh3 process observed at entry or exit; no local game/Ghidra, installed ASI/INI/config/save/settings/hardware change, secret output or controller-state write. Inbox unchanged at1831/1835, no sender=user steering. Independent next unit: reconcile the per-device stock-vs-default benchmark recipe with these reports while the installguard source owner completes/checkpoints its proof.
