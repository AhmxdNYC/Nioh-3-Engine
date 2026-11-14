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
