# FO deferred-command lifecycle contract - 2026-10-11

Bounded static analysis of experimental `codex-fo10` at `3b89c04`,
`D:/nioh3-wt/codex-fo10`. **Design contract only; FO remains NO-GO.**
No source repair, new test PASS, build, game launch, deployment or performance
claim. Startup availability is already repaired; do not repeat its passed tests.

## Observed ordering

References below are to `mod/Nioh3EngineFix/src/frameoverlap.cpp` in that worktree.

| Path | Current sequence | Missing guarantee |
|---|---|---|
| Fo98, 643-674 | Queue lock; copy live epoch to fake ctx; original emitter; retain object pointer; unlock | Resource epoch changes before stream publication; no retained epoch/owner or lifetime token |
| Flush98, 676-690 | Unlocked volatile count read; queue lock; append all records; zero count; unlock | Space/reserve validation, publication boundary and synchronization |
| FoPresent, 787-788 | Flush98; wait previous TAIL | Previous TAIL can enqueue after the flush |
| StockPresent, 754-762 | Flush98; PrepareStock -> FlushAll -> Barrier; original present | Late records can miss stock drain/reset |
| FoPresentDirect, 893-898 | PrepareStock; original present | No explicit deferred-queue flush |
| FoNode, 1155-1161 | Flush98; optional Barrier; original node | Barrier does not close producers or flush their new records |
| FoPPOps, 1012 onward | Flush98; conditional TAIL wait/lag path | Queue-empty observation does not authorize resource retirement |
| FlushAll, 237-240 | Barrier; clear lastWasFO | No deferred publication/consumer drain |
| Cache flush / endFrame / teardown, 1143-1153 | FlushAll; original operation | Reset/teardown can proceed with deferred records |
| Restore, 1500-1513 | Installation lock; restore code and emitter slots | No producer admission closure, in-flight hook join or deferred drain |

Static decomp `D:/nioh3-re/decomp/0x2f06ab4.c` shows the original emitter
calling object helpers, comparing ctx+6EB8 with obj+190, writing twelve bytes,
stamping obj+190, and calling a final helper. This queue stores only obj, not that epoch.
The transitive object-lock implementation and other emitter `7C50B4` are not
available in the inspected decomp exports; their behavior remains a proof gate.
Do not infer reentrancy or a retain/release API from the function names.

`TailFn` (542-603) release-stores tailDone after recycle. `Barrier` acquire-waits
that store; it does not consume the deferred queue. Replay completion and queue
publication are distinct events. Nor is tailDone a GPU-completion fence.

`PPOpsLagged` ends its 0x82/0x83 batch by incrementing live ctx+6EB8 (1006-1007).
Fo98 reads that ordinary field while holding its own queue lock; this unit has
not established a shared lock or happens-before edge with the increment.
This is a synchronization proof gap, not a newly reproduced runtime race.

`0x418b94.c` calls FinalKick then snapshots/clears statistics. `0x418c70.c`
publishes the remaining stream segment, waits for dispatcher drain, resets
write/lastKick to base and increments stream generation+3C60. **EndFrame is a
resetting operation, not an append-space allocator.** FO separately switches
halves and increments this generation (865-880). Stream generation, PagePool
use epoch, frame/TAIL ownership and GPU fence must remain distinct identities.

## Required contract before enabling a repaired queue

1. **Admission and capacity.** Before an emitter makes irreversible object-state
   changes, establish a place for every possible due record. A full queue cannot
   call the original with the live ctx, wait for main from replay, or discard the
   record. Growing storage alone needs allocation-failure behavior; a fixed cap
   needs a proved admission bound. Disabling FO after the emitter ran does not
   repair its already-required record. Admission failure must route the session
   safely before this point, or the feature must stay unavailable.
2. **Epoch and order.** Define the linearization point for selecting the use epoch
   and stamping the resource. Synchronize the PagePool epoch advance with this
   selection. Retain enough metadata to prove which retirement batch owns each
   record. Preserve per-resource causal order and dependencies on ordinary main
   commands; queue FIFO alone proves neither. Account for main-thread original
   emitter calls and calls made during a producer's transitive object lock.
3. **Publication.** Queue-lock release/acquire protects payload and count; stats
   take a synchronized snapshot. Remove unlocked volatile reads. Distinguish
   queued, copied into stream, dispatcher-published and replay-consumed states.
   Only main changes the live stream. Queue depth zero is not a lifetime fence.
4. **Space and drain.** Validate write within the active half, then use subtraction
   for available bytes and account for verified engine reserve/final tokens.
   Never assume the existing 64-byte FO guard is the engine's complete reserve
   contract. Detach a bounded prefix under the queue lock, release it before any
   dispatcher/Tail wait, and preserve records arriving during the wait. A drain
   must reconcile prior TAIL ownership, reset-to-base, logical end, kick pointers,
   generation, frame start and substreams before further writes. Calling EndFrame
   under the queue lock causes a circular wait if replay enters Fo98.
5. **Closure.** A flush followed by Barrier is not producer closure. A second
   flush handles the witnessed TAIL arrival only; other producers can still enter.
   Define admission closing and count/join of already-admitted calls without
   preventing those calls from finishing. Prove the engine stops new producers
   for each destructive boundary; do not park replay waiting for main. If that
   cannot be proved, keep reset/restore unavailable while records can be due.
6. **Lifetime and restore.** Prove the object remains live through actual replay
   of its 0x98 token, and resource retirement uses the required engine GPU fence.
   Establish producer quiescence, publish and consume all required records,
   reconcile engine stream state, then unpatch at a safe point. The watchdog's
   current tailDone>=issued test alone is insufficient for restoring emitter
   slots. A watchdog thread cannot call main-only Flush98 to manufacture safety.

These are obligations, not an implemented algorithm. There is no demonstrated
safe full-queue fallback yet. Main 1.7.61 with FO0 remains the usable build.

## Repair sequence and next concrete unit

Next independent unit: extend the isolated actual-code audit with a faithful
epoch-stamping emitter fixture and deterministic schedules for (a) same-object
duplicate suppression while its first record remains queued, (b) a PagePool
epoch advance before publication, and (c) late enqueue across FlushAll/reset
and restore eligibility. Record exact state transitions and fixture limits;
no game or D3D device required. Do not rerun the unchanged eleven passed hazard
checks merely to report another PASS. New cases must exercise changed evidence.

Then trace the two original emitter lock/update paths and the 0x98 consumer /
PagePool retirement path narrowly, identifying an actual ownership/admission
mechanism. Use that evidence to choose one combined queue/space/closure repair,
or reject the queue strategy if an obligation cannot be met. Do not ship a
count-only or flush-after-Barrier-only patch as a safety repair.

Convert existing hazard assertions to safety controls when implementation
changes. Cover due/not-due calls, both emitters, overflow admission, allocation
failure if used, 0/8/12-byte boundaries, reserve, both halves, reset generations,
late replay and non-replay producers, stock/direct present, PagePool advance,
device flush, teardown and watchdog restore. Prove exactly-once replay/order,
no stream canary damage, producer progress, safe lifetime and empty closed epochs.
Actual-code harnesses plus 255 patterns and full release checks precede any
shared build; lock-owned runtime/native validation follows only when justified.

## Checkpoint state

Main parent `88f317b`; experimental parent `3b89c04` unchanged. Phone inbox empty
at entry; no local game running. No game lock was acquired because no test was
launched; installed ASI/INI/config/saves and prior restoration remain untouched.
Remote 1767/1768/1781 still pending at last known bridge cursor1785; no fresh
remote query or result inferred. Controller read-only: enabled/working,
monitor45016/worker22552. Serial ownership until exit, no controller writes,
Claude calls or auto-wake changes. Recheck phone inbox at final checkpoint.
