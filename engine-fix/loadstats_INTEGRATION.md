# loadstats (LoadStats) — integration notes for the ASI owner

New files only (not wired in): `src\loadstats.cpp`, `src\loadstats.h`, `tests\loadstats_harness.cpp`,
`tests\run_loadstats_harness.py`. Background and the fixes it ranks: `notes\loading.md`.
Checks: `py -I re\scripts_load\ls_sites.py` -> `LOADSTATS SITES OK` (bytes of all 25 sites in re\Nioh3.dump.exe);
`py -I tests\run_loadstats_harness.py` -> `LOADSTATS HARNESS: ALL OK`. Compiles clean with the build.bat flags (/W4).

## 1. build.bat
```
rem per-load measurement ([Profiler] LoadStats = 0 by default; notes\loading.md)
cl /nologo /c /O2 /MT /GS /Gy /Gw /EHsc /std:c++17 /W4 /permissive- /DNDEBUG /DUNICODE /D_UNICODE ^
   /Fobuild\loadstats.obj src\loadstats.cpp || exit /b 1
```
and add `build\loadstats.obj` to the link line. No new libs (kernel32 only).

## 2. dllmain.cpp (next to loadmem::Start, ~line 2041)
```cpp
#include "loadstats.h"
...
{  // per-load measurement (loadstats.cpp, [Profiler] LoadStats = 0 by default; notes\loading.md)
    loadstats::StartInfo ls;
    ls.log = &Log;
    ls.iniPath = iniPath.c_str();
    ls.base = reinterpret_cast<uintptr_t>(g_base);
    ls.knownBuild = known;
    loadstats::Start(ls);
}
```
- It must run after the game code is unpacked and after the patch table is applied (it verifies original bytes and
  skips any site another patch already owns; today only SPR's `SPg` at 11970A overlaps). Its own sampler thread logs;
  nothing goes into the 30 s stats loop.
- Optional menu/OSD line: `loadstats::StatusLine(buf, n)` ("LS: idle" / "LS: load 12.3 s, A 9, 412 MB, PSO 300").
- Future patches at these call sites must check for `E8` + a thunk target instead of the original bytes (or be applied
  before LoadStats): 1FEE68, 1FF18C, 1FF371, 258A5A, 4201F5, 420605, 324BD61, 324C409, A099DD, 324C372, 324C4A1,
  11970A, 2EFE47C, 119AE1, 119D86, 58BF92, 58C07A, 2EFE1F2, 311D17, 312DD2, 31305A, 3130D6, 740392, 1FD9CA3, 32562BE.
  The planned RL design (`ResourceLoadIdle_SleepArg` 1FEE65, 3 bytes) does not overlap 1FEE68.

## 3. Nioh3EngineFix.ini
```
[Profiler]
; LoadStats: per-level-load measurement - MEASUREMENT ONLY (notes\loading.md). For every loading window (engine stop flag
; 58A) one summary: seconds, frames, process CPU, stage-ready time, resource-loader occupancy (in-flight vs its cap of 10),
; load-end ready wait (timeout or not), event-scene seconds, MB read, zlib inflate CPU time, PSOs created, plus the same
; numbers split at "stage ready". 2 = also one line per second inside the window. 0 = off (nothing installed).
LoadStats = 0
```

## 4. Log lines (what to send back from the Ally/PC)
```
LoadStats: mode 1, 25/25 sites hooked, 0 differ, 0 skipped
LoadStats #1: 58A set (loader in-flight cap 10, GameMain+4Dh = 0)
LoadStats #1: 118.3 s | frames 2480 (47.7 ms) | CPU 6.90 cores | stage ready +38.2 s | loaders busy 39.6/2.0 s ...
LoadStats #1 io: read 812 MB archive (...) | inflate 4.10 CPU-s, ... | PSO gfx ... | mgr Sleep(1) ...
LoadStats #1 split [set->stage ready] 38.2 s: ...
LoadStats #1 split [stage ready->clear] 80.1 s: ...
```
`grep LoadStats` of the mod log is the whole result; MangoHud / PresentMon stay as before.
