# Log-only title gate — task 41472

Base: 9f005983 (latest Y/Triangle-close RC); this branch does not contain savefiles.cpp or any save-switching implementation. [SaveFiles] Enabled=0 is explicit in the shipped INI. The new candidate has no admission API and cannot start a helper or change saves, regardless of installed INI settings.

TITLEGATE polls every 100 ms on a dedicated EF worker, only on the known executable build. Every load is guarded; manager, stack begin/end, top/type, state, queue, loading manager/byte, and all four discriminator fields are read twice and must match. No child pointers are dereferenced. Candidate is the requested title/state3/empty-queue/non-press-any predicate, additionally rejecting loading, continuously sampled for >=1000 ms. Invalid reads, a gap >500 ms, clock rollback or object replacement reset stability. Transitions (also state/top changes) log verdict, candidate, menuIdle, stability and raw offsets to Nioh3EngineFix.log. Enabled=0/logOnly=1 is a diagnostic invariant, not a report of the installed INI.

Validation: run `py -I mod/Nioh3EngineFix/tests/run_titlegate_test.py`. 106 checks PASS: all 28 read failures, all 14 recheck mutations, press-any, states 10/9, every state 0..19 other than 3, loading, non-title gameplay, pending queue, unreadable snapshot, stability boundary, gaps, rollback, identity changes. Release build PASS; existing nt-shadow warning only. No game, lock, installed mod or saves touched by cx-pc.

Native acceptance belongs to pc: preserve existing INI and keep SaveFiles Enabled=0, install candidate EF; verify startup marker and sequence press-any (deny), main menu (settling then candidate), Quit (state10 deny), System (state9 deny), New Game, loading and gameplay. New Game has no screenshot-paired state evidence yet: if it shares state3 and non-self pointers the candidate can admit it; do not claim it is verified or enable writes. Logs can miss transitions shorter than the 100ms sampling interval. Two-pass reads are diagnostic sampling, not atomic synchronization.

SHA256: d493ac861f966c606a4711a78b92fba9df0e91a3f9b2ff80874e8f0d6768cf5f.
