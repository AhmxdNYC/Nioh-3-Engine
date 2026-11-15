# FPW: file-loader poll wait (#103680, pc/2, 2026-11-15)

Human #103680: "have an agent work on the second idea" = make the KTGLGraph.Cache file-loader threads wait instead of
busy-checking (the 4-core walk-hitch cause, notes/walk-hitch-102842.md on performance-pc/hitch-102842: FileLoad +
FileLoad.Poll take ~1.2 of 4 cores in hitches while the disk reads < 18 MB/s).

## RE (v2.02 6AA7C106, dump re/Nioh3.dump.exe, byte-verified)
- KtglCacheInit 0x2EF053C creates 4 cache threads via 0x2EF00F8 {fn, stack 0x800, prio, name}; FileLoad.Poll body
  0x116D30 (thread obj ctx+0x788), FileLoad body 0x116F80 (ctx+0x758). Proc 0x1A71FDC: WaitForSingleObject(kick, INFINITE)
  -> fn(ctx) -> SetEvent(done). Kicked every frame from GfxDevicePresent (0x12682C) and by a sync flush 0x2EF0938 that
  blocks on the done events (6 callers) - so latency added inside the loop goes straight into those waits.
- Request ring ctx+0x6A8 (40-byte entries); counters produced 6BC, dispatched 6C0, read-done 6C4, retired 6C8.
- Poll loop (head 0x116D80): AcquireSRWLockShared(ctx+0xCD0), pause-condvar wait, read-done step 0x117330, retire step
  (fence GetCompletedValue via [ctx+0x870]->vtbl+0x40), ReleaseSRWLockShared at **0x116F12**, then
  `ja 0x116D80` while 6BC > 6C4 (0x116F32) or 6C4 > 6C8 (0x116F4C). **No yield at all.**
- FileLoad loop (head 0x116FD0): same lock, dispatch 0x117090, unlock at **0x11702C**, `ja` while 6BC > 6C4. No yield;
  it spins waiting for Poll, so the two spin against each other.
- Both unlocks: `call [0x38E2630]` (ReleaseSRWLockShared), rcx = ctx+0xCD0.

## Patch ([Patches] FileLoadPoll = 1, default 0; src/thrsched.cpp FpwInstall / FpwBackoff)
Each unlock call (6 bytes, one instruction) -> `call thunk; nop`. Thunk (near page, unwind registered): calls the same
IAT slot with the same rcx, then FpwBackoff(lock). Back-off only when the native loop is about to run again AND no ring
counter moved since the previous pass: pause (SpinUs 20) -> SwitchToThread (to YieldUs 200) -> high-resolution timer
wait SleepUs 250 (measured ~0.5-1 ms on this PC) per pass. Any counter change runs the next pass at once. The native
code still makes every decision; the thunk never writes game memory.
Tunables: [Tuning] FileLoadPoll_SpinUs / FileLoadPoll_YieldUs / FileLoadPoll_SleepUs (SleepUs 0 = yield only).
Stats: FLP status line in the 30 s stats log, "FPW: Poll passes N (pause, yield, sleep = ms) | FileLoad ...".

## Offline proof
tests/run_fpw_harness.py ALL PASSED: real v2.02 bytes (tests/fpw_fixture.h) in a fake image; install rewrites both
sites; thunk calls the IAT unlock with rcx; RtlVirtualUnwind walks through the thunk to the caller; no back-off when not
looping or on progress; idle passes pause/yield/sleep; FileLoad's own condition; wrong bytes = not installed, unchanged.

## Not proven (needs the game)
- Live: install line, CPU of the two loader threads, walk hitches, Boss Match load and Continue load times, OFF vs ON,
  alone and with FileLoadPrio + KidstsCap. Risk: the retire step waits on the GPU fence, so sleeps may stretch the sync
  flush / loading screens by up to ~1 ms per stalled wait; if loads get slower, try SleepUs 0 (yield only).
