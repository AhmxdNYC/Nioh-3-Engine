# Parallel rendering safety audit - 2026-10-11

Root integrated two finite read-only worker audits. No source edit, harness execution, ASI build/deployment, game launch, Ghidra or new performance claim. Main1.7.61/FO0 remains tested; experimental FO remains NO-GO. Latest serial trace15c9e2d identifies concrete allocator submit/reuse targets but leaves owner+6D8 versus context+6C8 fence identity unresolved.

## ApplyBatch failure policy: next isolated source unit

`mod/Nioh3EngineFix/src/dllmain.cpp:1228` uses a snapshot/suspend/context scan before writing instruction replacements. Snapshot failure, failed/incomplete enumeration and OpenThread failure omit participants. Failed SuspendThread and GetThreadContext skip inspection. With no observed instruction-pointer clash, it still reaches byte writes at1275-1283. Root independently reread this production path; the fail-open control flow is verified. No specific crash cause or runtime corruption reproduction is established.

BuildRepl/protection failures and observed in-site instruction pointers stop byte writes. Post-write cache flush, resume and protection restoration results are ignored; readback failure reports false after mutation without rollback. The sole Worker-local caller at1712 does not prove installation precedes concurrent game execution: DllMain creates Worker at3360, which waits for decryption and sleeps200ms at1607.

Minimal bounded repair: require complete enumeration and successful inspection of every relevant live participant; otherwise resume acquired suspensions, close handles, restore changed protections and return failure before any byte write. If a participant exited, require affirmative termination evidence rather than assuming every API failure means exit. Capture failures without allocation/logging in the frozen interval. This does not settle threads created after the snapshot or all post-write cleanup failures.

Meaningful proof must inject snapshot/enumeration/OpenThread/SuspendThread/GetThreadContext failures into the production implementation, assert unchanged bytes at every patch site, zero writes/cache flushes, balanced successful suspend/resume, closed handles, restored protections and failure status. Include pre-fix negative controls, persistent-clash and positive boundary controls. Keep source/build work isolated; patterns/full release checks required before a shared build. No local game test merely to repeat previous passes.

Pinned SafetyHook f44cc070a8340f2f26649553c49533475417304d uses VEH/page traps (`D:/refs/safetyhook/src/os.windows.cpp:271`, `src/inline_hook.cpp:428`), not this suspend loop. Some trap/protection failures there are unchecked too, and its worker test pauses the target. It is comparison material, not a proven drop-in repair.

## Allocator/list lifetime requirements

Pinned references: vkd3d-proton2230755878b01993b4b82d0ac0c0624f3ab4d333 and DirectX-Specs a610933eac2103cd502bacfb5193ae7f02c6fa54. Native list Reset may precede previous GPU completion; allocator Reset requires the GPU no longer references recorded commands. Concurrent methods and multiple recording lists sharing an allocator violate the contract. See `D:/refs/DirectX-Specs/d3d/CPUEfficiency.md:593`,740,813; resource ownership at613.

vkd3d-proton `libs/vkd3d/command.c:2961-2975` returns S_OK without resetting command pools when internal_refs>1; its comment requires fence-timeline signalling for full safety. Successful queue Signal/Wait means enqueueing work, not observed GPU completion. Device removal can return UINT64_MAX from GetCompletedValue; it must not become a false completion verdict.

FO acceptance requires concrete allocator/list/storage-generation identity; exclusive recording/reuse ownership; closed late producers; every relevant closed/pending/submitted use; exact fence identity and signal-after-last-use order; observed completion across DIRECT/COPY/COMPUTE dependencies; and safe retirement/restart across toggles, fallback, failure and wrap. API success, CPU drain, issued counters or an event alone cannot replace these proofs.

Existing `tests/frameoverlap_harness.cpp:210,270,445` uses always-successful AllocReset, issued-counter FakeSubmitFrame and a FakeWaitFence check that the value was issued. Recycling/retirement/ECL lifecycle callbacks are simulated. The real queue forward-fence test does not establish the actual engine ECL allocator lifecycle. Existing passing harnesses remain useful for their tested contracts, with these limits explicit.

Follow the concrete game trace in notes/handoff-codex-fo98-allocator.md; do not overwrite its newer decoded ExecuteCommandLists-then-Signal ordering with the earlier unresolved-vtable observation. Exact fence coverage and producer/retirement closure remain open. Native/Proton runtime versions must be matched before device acceptance.
