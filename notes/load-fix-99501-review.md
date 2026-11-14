# Review: deepseek/load-99501 @ c41ed36e (pc/2, 2026-11-14 5:35 am EST)

Branch base 5ed3917b. Files: src/loaddefer.{h,cpp}, dllmain.cpp hook-up, ini keys, build.bat. Static review only, nothing run.

## Verdict
- **Probe arm (LoadDeferProbe=1, LoadDefer=0): OK for one Main PC log-only run.** It only wraps the stock
  GfxFencePollWorker call at 0x69A2B4 (timing + stack-walk caller family) and passes the ring release at 0x69A2C4
  straight through (t_rec is always null without the fix). Byte- and call-target-checked before patching.
- **Fix arm (LoadDefer=1): REJECTED, do not run.** Two blockers below; with only the first one fixed it would
  free staging under an in-flight GPU copy.

## Ring lock check (pc/2, decomp)
- 0x7F6200 ring enter = WaitForSingleObject(ring+0x18 semaphore) + slot pick; 0x5C1A4 ring release = atomic bit clear
  in ring+0x2C + ReleaseSemaphore(ring+0x18). Semaphores have no owner thread, so releasing a held slot from the pump
  thread is legal, and holding up to 4 of the slots cannot deadlock the pump. This part of the design is sound.

## Blockers
1. **InImage() on heap pointers -> the fix never turns on.** InImage() means "inside Nioh3.exe's image". It is applied to
   `cmdlist` (FenceThunk gate, PumpThread), the queue object `q` in FencePassed, and `gm` in LoadWindowOpen. All three
   are heap objects (loadstats.cpp:690 only checks gm != 0). Result: every submit takes the stock path and FencePassed
   is always false. Use non-null + ReadMem (SEH) checks instead; keep InImage only for vtable/function pointers.
   Note the PumpThread `!InImage(r->cmdlist)` branch pops the record WITHOUT calling ring release or the staging frees:
   that leaks a ring semaphore slot each time -> after 8, every TextureCopyRegion/map/unmap caller blocks forever
   (game freeze). That branch must finish the stock way (poll + ring release + frees), never drop.
2. **Staging frees are never captured -> use-after-free once #1 is fixed.** RingThunk (inside TextureCopyRegion, before it
   returns) does `t_rec = nullptr` and queues the record. TextureStagingRelease runs AFTER TextureCopyRegion returns
   (note 270), so CaptureOrRelease sees t_rec == null and frees the staging immediately while the GPU copy is still
   queued. Keep a separate thread-local "capture target" that stays set until captureUntilQpc (or until the next
   TextureCopyRegion on that thread), and have the pump wait for that window (kQueueGraceUs 2 ms < kCaptureGraceUs
   3 ms today, so the pump can also finish a record before its capture window closes; the pump must wait for
   captureUntilQpc AND set done under the lock before it frees).

## Should fix
3. Capture by time catches unrelated frees on that thread for 3 ms (deferred, then freed from another thread).
   Capture only pointers that match the copy's source (the staging ptr TextureCopyRegion was given) instead of "any
   free in 3 ms".
4. Cross-thread GpuAllocRelease (kinds 2/3 allocator vtable+0x20) is still unproven thread-safe (note 3.1). Until it
   is, a safer first fix arm is ring-hold only: defer the poll + ring release but let the caller free the staging
   only after the fence (i.e. keep the stock wait for uploads whose staging is freed by the caller). Price both with the probe.
5. kMaxHoldUs is 300 ms in code, 3 s in the note. The stuck path then calls the stock INFINITE poll on the pump thread,
   blocking every later record; fine as a fallback, but log it once with the target/completed values.
6. g_released is counted twice (per alloc and per record). Counter resets in StatusLine race the Interlocked adds (stats only).
7. Two new ini keys with no menu row: check test_catalog / generate_catalog --check (every shipped key must be in the
   menu or the iniOnly list; human #99450 wants settings in the menu). Add both to iniOnly (probe/dev keys) or the menu.

## Next
- deepseek: fix 1, 2, 7 (+3 if cheap), rebuild, RESULT to pc/2 --reply-to 99657.
- lead: Main PC probe-only run of the CURRENT branch build 5b08866f is allowed now (after the menu check #99505):
  LoadDeferProbe=1, LoadDefer=0, LoadStats=1, one Boss Match load; read `stats loaddefer:` (submits, stock wait avg/max,
  caller mix). That prices the ~1 frame per upload claim before more fix work.

## Round 2 review (pc/2, 2026-11-14 ~6:20 am EST): deepseek/load-99501 22043028, EF 983bd8ee - fix arm REJECTED (1 blocker), probe arm unchanged OK
Fixed and checked: heap pointers no longer go through InImage (cmdlist, q, gm); the pump has no drop branch, every
queued record gets poll + ring release + frees; capture and the pump's snapshot are both under g_lock with an owner
thread id, so a capture cannot slip past the snapshot; t_cap survives RingThunk; double g_released gone; kMaxHoldUs 3 s;
both keys in catalog iniOnly. Cross-record capture (a late free of copy A landing in the next record B) is SAFE because the
pump is one FIFO and waits on the head: B finishes only after A's fence and B's fence.

BLOCKER - the 3 ms capture window is a safety bound, not just a grouping hint. The stock caller frees the staging only
after the blocking wait, i.e. after the GPU copy is done. With the wait removed, any staging free that reaches
CaptureOrRelease after captureUntilQpc goes to the stock free while the copy can still be on the GPU (use-after-free).
3 ms is easily missed by a preempted loader thread on a busy 4-core CPU - the exact users this fix is for. Same hole when
the record already has kMaxAllocs (8) frees: the 9th goes to stock in flight. Same hole on the stuck path: done = 1 is set
before the stock poll, so a free that arrives while the poll still waits goes to stock.
Fix:
1. Capture while !done && owner == this thread, with NO deadline (drop the captureUntilQpc test in CaptureOrRelease and in
   the pump's wait; keep kQueueGraceUs). Unrelated frees on that thread are only delayed until the fence, which is safe.
2. Record full (nAllocs == kMaxAllocs) and !done: wait (Sleep(0)/Sleep(1) loop) until r->done, then the stock free. Or raise
   kMaxAllocs and count g_capFull; never stock-free while !done.
3. Stuck path: run the stock poll first, set done/owner and take the snapshot under the lock AFTER the poll returns.
Minor: FenceThunk drops an unqueued old t_rec (DropRec) - if that ever happens with captured frees, they must be freed
(stock, after the old fence) not discarded; log a counter for it.

## Next (round 2)
- deepseek: blocker fixes 1-3 + the DropRec counter, rebuild, RESULT to pc/2 --reply-to 99780. Offline only.
- Probe arm (c41ed36e / EF 5b08866f, LoadDeferProbe=1, LoadDefer=0) stays approved; no LoadDefer=1 run until round 3 passes.
