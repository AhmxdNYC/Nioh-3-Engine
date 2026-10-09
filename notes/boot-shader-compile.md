# Boot shader compile ("Compiling shaders N/17,639"): measurements, RE, and PL (ASI 1.7.46)

Date 2026-10-09. Inputs: PC run8b PJ log + PresentMon (`captures\tm\abba2\run8b_P\J\20261009-013703-tour`), pc2 sessions
0020 (stock) / 0036 (mod) and phase A (bridge #124 / #126, `captures\pc2\raw`, `captures\pc2\phaseA`), Ghidra
(`re\decomp.cmd 0x3483e04 0x34835d4 0x1101b90 0x2efe2a8 0x2efe440`), `notes\stutter-pso.md`, `notes\job-system.md`.
The game was not run on the PC for this note.

## 1. What the existing data says

| | PC (RX 9070, run8b J, PJ v1 on) | pc2 0020 stock (1st launch at that config.xml) | pc2 0036 mod (next launch) |
|---|---|---|---|
| boot-list creates | 17,145 (journal: 14,399 in the first 13.2 s) | 17,639 (on-screen counter) | same list |
| boot phase | first present +1.6 s -> creates end ~+17 s (frame blip 117 / 227 ms) | heavy CPU for ~25 s (7.3 / 4.6 / 3.6 / 7.0 / 8.1 cores per 5 s) | heavy CPU ~10 s (3.2 / 5.4 cores), then ~1.4 |
| frames during it | steady 60 fps (CPU 16.6 ms = cap) | 120 fps, GPU ~5 ms | 120 fps |
| title scene visible (GPU jumps) | ~+19 s | ~+24 s after first present | (human-paced session) |

- **Not on the render thread.** Frames stay at the cap through the whole list, so the compile runs on worker threads,
  not on the dispatcher.
- **Multi-threaded.** pc2 shows 5-8 cores busy during the list. The new boot line counts the exact number of threads.
- **The driver cache helps the next boot.** pc2's heavy phase drops from ~25 s (first launch at a new config) to ~10 s.
  This is suggestive, not clean: there is no true cold/warm pair yet. The PC's ~15 s for 17k creates (~0.9 ms per
  create per wall-clock, on a long-warm driver cache) includes PJ v1's per-create serialization and 64 MB of journal
  writes. It is an upper bound for stock.
- There is no measurement of a first launch after a driver update. The pc2 test below adds one by moving the AMD
  cache aside.

## 2. RE: the boot compile loop

- Boot state `Game::Mode::PreloadPSOMode`.
  - Request body: PreloadPsoBootLoad 1101B90. It opens resource 0x17A4 and calls PsoListLoad 3483E04 with list = req+58h,
    job group = `*arg`, ctx = `arg[1]`, stream.
  - When it returns, it sets the done byte req+98h.
- PsoListLoad 3483E04, in order:
  1. Reads the 0x18-byte header.
  2. Loads every referenced shader group **serially** (two loops, progress counters list+30h / +34h).
  3. Resolves the built-ins.
  4. Creates the PSOs:
     - **No job group:** an inline serial loop. Per graphics record: PsoListResolveGfx 34837DC, then PsoPreloadGfx
       2EFE2A8. Then the compute records (2EFE1CC). The progress counters list+38h / +3Ch are lock-incremented: this is
       the on-screen N/17,639.
     - **With a job group:** the records are split into `min(count, W)` PsoListPreloadJob 34835D4 jobs and pushed with
       JobPushToOwner 3FAD8. Then JobGroupWait 3E7CC joins them, with the caller helping. W = `[[[group+8]+440h]+28h]`,
       the worker count of the group owner's KIDS pool. Graphics first, then a full join, then compute the same way.
- Per record:
  - PsoPreloadGfx 2EFE2A8 normalises key byte +3Dh (79E990) and looks the key up under the PSO cache CS. A hit is
    skipped.
  - PsoPreloadGfxCreate 2EFE440 calls CreateGfxPso 9290D8 **outside** the lock, then inserts under the CS, or releases
    the PSO if the insert fails.
- The game has no ID3D12PipelineLibrary, no CachedPSO and no disk cache of its own (stutter-pso.md section 2). Every boot
  makes ~17.6k `CreateGraphicsPipelineState` / `CreatePipelineState` calls, and on a warm boot the driver answers each
  one from its own shader cache.

Answers to the brief:
- **(a) Parallelize?** It already is. It runs on W pool workers plus the waiter, and the create is outside the lock.
  - The remaining serial parts are the shader-group load loop before the creates and the graphics -> compute join.
  - Widening means a bigger pool: pools are sized clamp(logical, 6, 32) times a weight, so a 12-thread CPU loses 3
    threads in KIDS_RESOURCE. That would be a boot-time pool-size patch. The pool behind the boot group is not
    identified statically.
  - Not built: it only pays when the driver compile is CPU-bound (cold cache). The new boot line reports the thread
    count, so the pc2 run shows whether W is small.
- **(b) Persistent ID3D12PipelineLibrary?** Yes, built as **PL** (section 3).
  - It cannot help the first launch after a driver update. D3D12 rejects a library blob from another driver
    (D3D12_ERROR_DRIVER_VERSION_MISMATCH), so that boot is a full recompile either way, and PL then rebuilds the file.
  - It helps warm boots when a library load is cheaper than a driver-cache hit. It also helps when the driver's cache
    was evicted or cleared without a driver change (the AMD cache has a size limit): the library is still valid then.
- **(c) Skip unused PSOs?** No.
  - The boot list is the game's only defence against first-sight compiles. Every skipped PSO becomes a synchronous
    create on the dispatcher later, which is a gameplay hitch (stutter-pso.md section 2).
  - Continuing the list behind the title (return early, finish on a background thread) is possible, because 2EFE440's
    insert is thread-safe. A draw that reaches a PSO before its turn would still hitch, so it was rejected.
- **(d) PJ / PJH?** They split cleanly at the boot gate (quiet PJ_QuietMs + G+58Ah clear).
  - PL owns every engine create **before** the gate (the boot list). PJ journals and PJH hands over only creates
    **after** it.
  - Same vtable hooks (`psojournal.cpp`), same desc key (Prepare / Hash128 of the canonical record). Nothing is hooked
    or stored twice.
  - PJH's prewarm starts after the gate, so it never races a library load.

## 3. PL - boot pipeline library (ASI 1.7.46, `[Patches] PL = 0`, `PL_MaxMB = 1024`)

- **Values.** PL = 1 on. PL = 2 installed + off: it only logs the boot timing line, which is the A/B "off" arm. PL = 0
  off. There is no menu row, because PL acts before the title.
- **Refused on:** Wine/Proton, where vkd3d-proton keeps its own pipeline cache, and when `[ShaderSwap] Enabled`,
  `[Profiler] GPUProf >= 2`, `[Debug] GIProbe` or `[Patches] GIMerge` hook PSO creation, because a library load would
  bypass them. The log says why, and PL falls back to 2.
- **First device reported** (psoswap's D3D12CreateDevice hook), on a background thread:
  1. Log the driver's `D3D12_FEATURE_SHADER_CACHE` flags, adapter ids and UMD driver version.
  2. Open `Nioh3EngineFix_pj\pl_<os>_<gpu>_sm<NN>.bin`.
  3. Validate the 128-byte header: exe stamp/size, device class, adapter ids, UMD version, blob size <= PL_MaxMB.
  4. Map the blob read-only. The pages are file-backed, so there is no commit charge, and they stay mapped for the
     session.
  5. Check its Hash128.
  6. Call `ID3D12Device1::CreatePipelineLibrary`.
  - Any mismatch or D3D12 refusal starts an empty library and marks the file for a rewrite.
- **Engine create before the gate:**
  - `LoadGraphicsPipeline` / `LoadComputePipeline` / `ID3D12PipelineLibrary1::LoadPipeline` is called with the
    engine's own desc, named by the 128-bit desc key. The D3D12 runtime refuses (E_INVALIDARG) a desc that differs
    from the stored one.
  - On a miss, the engine's own call runs and the result is `StorePipeline`d.
  - Any other load error is counted, and the engine's call runs instead.
  - An exception inside the runtime stops all further library calls.
  - Loads of the same name are serialized (64 striped locks), as the D3D12 docs require.
- **At the gate** (own thread, background priority):
  1. Log the boot line.
  2. If anything was stored, `Serialize`, then write `.tmp`, flush, and rename to `.bin`. If `.bin` is mapped, rename
     to `.new` instead; it is promoted at the next launch after a header check.
  3. Release the library.
  - After the gate every create is the engine's own, untouched.
- **Same picture.** A library PSO is the driver's own compiled object for the identical desc, on the identical driver
  and adapter.
- **Memory.**
  - In game: the mapped file pages, which are evictable.
  - During the gate on a launch that grows the library: transient Serialize buffer + library copies.
  - VRAM: unchanged. The same PSOs exist either way.
- **Log lines:**
  - `PJ: PL: driver shader cache flags ...`
  - `PJ: PL: library ...: N boot pipelines, X MB, mapped + verified in Y ms` or `new library (<reason>)`
  - `PJ: boot PSO list: N engine creates on T thread(s), first +a s, last +b s after process start (device +d s, gate +g s) = W s wall; create calls C thread-s (avg, max)`
  - `PJ: PL = 1: L loaded from the library (avg ms), M not in it, S stored, E before the library was ready, errors`
  - `PJ: PL: library written: ...`
  - The boot line is also logged whenever PJ is on.
- **Offline:** `py -I mod\Nioh3EngineFix\tests\run_pj_harness.py` -> **ALL PJ CHECKS OK**. It uses a real WARP device.
  New launches:
  - plrec: stored 3, written.
  - plwarm: 3 loaded with 0 creates reaching a spy below PJ and every object usable; 1 new stored to .new.
  - plwarm2: .new promoted, 4 loaded, nothing written.
  - plbad: blob byte flipped -> checksum refused -> rebuilt.
  - plstale: UMD version changed -> refused.
  - plgarb: garbage blob with a valid checksum -> D3D12 E_INVALIDARG -> rebuilt, no crash.
  - plpj: PJ + PL together. Boot loads go to PL, and the create after the gate is journaled by PJ only.
  - pl2: timing only, no file.
  - `tests\test_patterns.py`: ALL 231 SITES OK.

## 4. pc2 test (queued on the bridge after #473 / #476 / #477 / #502)

Arms (hands-free vpad, `tools\tour\pc2_run_tour.ps1 -Input vpad -NoProfile -Points 1 -DwellSec 4 -List
tools\tour\tourlist_boot.txt`). `-NoProfile` is required, because GPUProf 2 turns PL off. Same config.xml as #441.
- S1: stock, with no ASI and no input: launch, record PresentMon 90 s, quit.
- OFF1: `PL=2`.
- PLc: `PL=1`, with `Nioh3EngineFix_pj\pl_*` deleted first (cold library).
- PLw1: `PL=1`.
- OFF2: `PL=2`.
- PLw2: `PL=1`.
- S2: stock.
- Optional driver-cold pair, restoring the cache after: DC-OFF and DC-PL.
  - Move `%LOCALAPPDATA%\AMD\DxcCache` aside. Run `PL=2`.
  - Move the new one aside again. Run `PL=1`: the library from PLw is still valid.
  - Put the original back.

Read per launch:
- From the boot line:
  - wall time first -> last create (W);
  - title-ready ~ `last`;
  - threads (T);
  - create thread-seconds and average ms.
- From the PL line: loaded / stored / early / errors, library MB, and write time.
- Launch -> field: the first mod-log line time to the `LoadStats #1 io` line. Vpad presses every ~4 s, so this is ±2 s.
- Stock: PresentMon first present -> end of the flat compile-screen run. Calibrate the heuristic on OFF1/OFF2 against
  their `last` time.
- MemStats at the field (WS / private / commit / VRAM), PLw vs OFF.
- A same-spot screenshot PLw vs OFF (shotdiff).

Keep PL = 1 (provisional) only if all of these hold:
- PLw's W is >= 15 % below OFF's, or launch -> field is >= 2 s faster.
- The cold library run (PLc) is no slower than OFF by more than ~1 s.
- Errors are 0.
- Memory at the field is within noise.
- The shotdiff is identical.

If PL loses, the boot line still tells us whether a pool-size patch (2a) is worth it: a low T with a high average ms
when cold.
