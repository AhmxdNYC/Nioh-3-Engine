# tilestats (TS) — integration notes for the ASI owner

New files only (not wired in): `src\tilestats.cpp`, `src\tilestats.h`, `tests\tilestats_harness.cpp`,
`tests\run_tilestats_harness.py`. Background: `notes\gpu-tiles.md`. Harness: `py -I tests/run_tilestats_harness.py`
→ `TILESTATS HARNESS: ALL OK` (real D3D12 device, needs tiled tier 2+).

## 1. build.bat
```
cl /nologo /c /O2 /MT /GS /Gy /Gw /EHsc /std:c++17 /W4 /permissive- /DNDEBUG /DUNICODE /D_UNICODE ^
   /Fobuild\tilestats.obj src\tilestats.cpp || exit /b 1
```
and add `build\tilestats.obj` to the link line. No new libs (d3d12 types only, no import).

## 2. dllmain.cpp (next to memstats::Start, ~line 1920)
```cpp
#include "tilestats.h"
...
{   // tile-mapping / queue-sync census (tilestats.cpp; [Profiler] TileStats = 0, [GpuOpt] TileBatch = 0)
    tilestats::StartInfo ti;
    ti.log = &Log;
    ti.iniPath = iniPath.c_str();
    tilestats::Start(ti);
}
```
Stats thread (~line 1741): `if (sec % 30 == 0) tilestats::LogLine();`

## 3. osd.cpp
- `InstallHooks()`, right AFTER the ExecuteCommandLists `PatchSlot(...)` (qvt is the dummy queue's vtable; all queues
  of the device share it):
  ```cpp
  if (tilestats::WantsHooks()) tilestats::InstallOnQueueVtable(qvt, /*eclViaOsd=*/g_origEcl != nullptr);
  ```
  It patches slots 8 (UpdateTileMappings), 9 (CopyTileMappings), 14 (Signal), 15 (Wait); slot 10 only when the OSD's
  ECL hook failed. Slot 10 stays the OSD's because the OSD checks `_ReturnAddress()` (CallerIsGame) and must stay the
  outermost ECL hook.
- `HkExecuteCommandLists`, first statement (before the CallerIsGame early return):
  ```cpp
  tilestats::OnEcl(q, n, CallerIsGame(_ReturnAddress()));
  ```
  (cheap: a counter; with TileBatch = 1 it first issues a held UpdateTileMappings of that queue — required for
  correctness, so it must run before `g_origEcl` for EVERY caller.)
- Present hook, next to `gp::OnFrame()` (osd.cpp:1050): `tilestats::OnFrame();` (frame counter for the per-frame
  numbers, and issues any held call).
- If the overlay is off (no Present hook), TS gets no hooks: only install TS from InstallHooks.
- Optional menu line: `tilestats::StatusLine(buf, n)`.

## 4. Nioh3EngineFix.ini
```
[Profiler]
; TileStats: D3D12 tile-mapping census - MEASUREMENT ONLY. One "stats tiles:" line every 30 s: UpdateTileMappings
; calls / regions / ranges / tiles per frame by queue type (DIRECT/COMPUTE/COPY), CPU us inside the call (mean/max),
; CopyTileMappings, and ExecuteCommandLists / Signal / Wait per frame per queue type (game calls; other modules apart).
; 2 = also a one-shot trace of 256 calls after 60 s (QPC timestamps, to line up with an RGP/GPUView capture) and an
; estimate of redundant re-maps. 0 = off.
TileStats = 0

[GpuOpt]
; TileBatch (notes\gpu-tiles.md): merges consecutive UpdateTileMappings of the game on the same queue into one call
; when that provably gives the same final mapping (same resource/flags/heap, single-tile regions, no tile in both);
; the merged call is issued before any other operation on that queue. 0 = off, 1 = on, 2 = count only (no change).
TileBatch = 0
```

## 5. Notes / risks
- Thread-safety: per-queue SRW lock for held calls; counters are interlocked; nothing is logged from game threads
  (the trace is buffered and dumped by LogLine).
- TileBatch holds AddRefs on the queue, resource and heap while a call is held; it is issued at the next op on that
  queue or the next Present at the latest. The engine always follows its updates with a Signal on the same queue
  (290900 end, CacheTileBatchFlushAndSignalGfx), so in practice a call is held for microseconds.
- Only calls whose return address is in Nioh3.exe are merged (the ECL thread replays tokens from the exe; the cache
  job calls from the exe). Other modules pass through but still flush held calls first.
- Under vkd3d-proton GetDesc / vtable layout are the same (the OSD already relies on both). Not yet run on the Ally.
- `tests\test_patterns.py` needs no change (no byte patches).
