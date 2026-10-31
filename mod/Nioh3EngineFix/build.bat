@echo off
rem Build Nioh3EngineFix.asi (Release x64, /O2, static CRT /MT). Needs MSVC (cl, ml64, link) + Windows SDK.
rem Finds a toolchain in this order: already on PATH (Developer prompt / CI), the portable MSVC at
rem ..\..\tools\msvc\setup_x64.bat (this PC), or Visual Studio / Build Tools via vswhere.
setlocal
cd /d "%~dp0"

where cl >nul 2>nul && goto :have
if exist "%~dp0..\..\tools\msvc\setup_x64.bat" (
    call "%~dp0..\..\tools\msvc\setup_x64.bat"
    goto :have
)
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if exist "%VSWHERE%" (
    for /f "usebackq delims=" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do (
        call "%%i\VC\Auxiliary\Build\vcvars64.bat" >nul
    )
)
:have
where cl >nul 2>nul || (echo ERROR: no MSVC toolchain found & exit /b 1)

rem regenerate the patch table header from patches.json when Python is available (it is checked in anyway)
where py >nul 2>nul && py -I tools\gen_header.py
if errorlevel 1 (echo ERROR: gen_header.py failed & exit /b 1)
rem fixed game RVAs: rvas.json -> src\rvas_gen.h must be current; warn about any raw base + RVA left (tools\check_fixed_rvas.py)
where py >nul 2>nul && py -I tools\gen_rvas.py --check
if errorlevel 1 echo WARNING: rvas.json / rvas_gen.h out of date - run tools\gen_rvas.py with a decrypted dump
if not exist build mkdir build
where py >nul 2>nul && py -I ..\..\tools\check_fixed_rvas.py > build\fixed_rvas.txt 2>&1
if errorlevel 1 echo WARNING: raw fixed game RVAs left - see build\fixed_rvas.txt

if not exist build mkdir build
if not exist bin mkdir bin

ml64 /nologo /c /W3 /Fobuild\caves.obj src\caves.asm || exit /b 1
rem GPU barrier batching (gpuopt.cpp): per-slot command-list thunks
ml64 /nologo /c /W3 /Fobuild\gpuopt_thunks.obj src\gpuopt_thunks.asm || exit /b 1
rem DQ / P1E7 caves (drawprep.cpp; [Patches] DQ = P1E7 = 0; notes\remaining-work.md 2.5)
ml64 /nologo /c /W3 /Fobuild\drawprep_caves.obj src\drawprep.asm || exit /b 1
rem AS async-slot producer caves (asyncslots.cpp; [Patches] AS = 0; notes\gpu-async-wins.md)
ml64 /nologo /c /W3 /Fobuild\asyncslots_caves.obj src\asyncslots.asm || exit /b 1
cl /nologo /c /O2 /MT /GS /Gy /Gw /EHsc /std:c++17 /W4 /permissive- /DNDEBUG /DUNICODE /D_UNICODE ^
   /Fobuild\asyncslots.obj src\asyncslots.cpp || exit /b 1
rem JT job-timing probe caves (jt.cpp; [Profiler] JT = 0; notes\gq-jt-probes.md)
ml64 /nologo /c /W3 /Fobuild\jt_caves.obj src\jt.asm || exit /b 1
rem PM SyncInterval writer probes (pmode.cpp; [Debug] PresentModeLog = 1; notes\vrr-present.md)
ml64 /nologo /c /W3 /Fobuild\pmode_caves.obj src\pmode.asm || exit /b 1
cl /nologo /c /O2 /MT /GS /Gy /Gw /EHsc /std:c++17 /W4 /permissive- /DNDEBUG /DUNICODE /D_UNICODE ^
   /Fobuild\jt.obj src\jt.cpp || exit /b 1
cl /nologo /c /O2 /MT /GS /Gy /Gw /EHsc /std:c++17 /W4 /permissive- /DNDEBUG /DUNICODE /D_UNICODE ^
   /Fobuild\drawprep.obj src\drawprep.cpp || exit /b 1
cl /nologo /c /O2 /MT /GS /Gy /Gw /EHsc /std:c++17 /W4 /permissive- /DNDEBUG /DUNICODE /D_UNICODE ^
   /Fobuild\dllmain.obj src\dllmain.cpp || exit /b 1
cl /nologo /c /O2 /MT /GS /Gy /Gw /EHsc /std:c++17 /W4 /permissive- /DNDEBUG /DUNICODE /D_UNICODE /Fobuild\updatecheck.obj src\updatecheck.cpp || exit /b 1
rem Grva() pattern table for fixed game RVAs (gamerva.h / rvas.json; tools\gen_rvas.py)
cl /nologo /c /O2 /MT /GS /Gy /Gw /EHsc /std:c++17 /W4 /permissive- /DNDEBUG /DUNICODE /D_UNICODE /Fobuild\gamerva.obj src\gamerva.cpp || exit /b 1

rem in-game overlay (d3d12.dll / dxgi.dll are loaded at run time - no import dependency)
cl /nologo /c /O2 /MT /GS /Gy /Gw /EHsc /std:c++17 /W4 /permissive- /DNDEBUG /DUNICODE /D_UNICODE ^
   /Fobuild\osd.obj src\osd.cpp || exit /b 1
rem Opt-in bounded DXGI diagnostics
cl /nologo /c /O2 /MT /GS /Gy /Gw /EHsc /std:c++17 /W4 /permissive- /DNDEBUG /DUNICODE /D_UNICODE ^
   /Fobuild\dxgidiag.obj src\dxgidiag.cpp || exit /b 1
rem HitchLog: one line per game-thread frame gap over HitchLogMs ([Debug] HitchLog = 1, measurement only)
cl /nologo /c /O2 /MT /GS /Gy /Gw /EHsc /std:c++17 /W4 /permissive- /DNDEBUG /DUNICODE /D_UNICODE ^
   /Fobuild\hitchlog.obj src\hitchlog.cpp || exit /b 1
rem mod menu (xinput / dinput8 are loaded at run time too)
cl /nologo /c /O2 /MT /GS /Gy /Gw /EHsc /std:c++17 /W4 /permissive- /DNDEBUG /DUNICODE /D_UNICODE ^
   /Fobuild\menu.obj src\menu.cpp || exit /b 1
cl /nologo /c /O2 /MT /GS /Gy /Gw /EHsc /std:c++17 /W4 /permissive- /DNDEBUG /DUNICODE /D_UNICODE /Fobuild\savefiles.obj src\savefiles.cpp || exit /b 1
rem native save library (savelib.cpp): Saves page operations without Python (works under Wine/Proton)
cl /nologo /c /O2 /MT /GS /Gy /Gw /EHsc /std:c++17 /W4 /permissive- /DNDEBUG /DUNICODE /D_UNICODE /Fobuild\savelib.obj src\savelib.cpp || exit /b 1
rem GPU barrier batching (notes\gpu-barriers.md; [GpuOpt] Barriers = 0 by default)
cl /nologo /c /O2 /MT /GS /Gy /Gw /EHsc /std:c++17 /W4 /permissive- /DNDEBUG /DUNICODE /D_UNICODE ^
   /Fobuild\gpuopt.obj src\gpuopt.cpp || exit /b 1
rem memory line ([Profiler] MemStats = 0 by default)
cl /nologo /c /O2 /MT /GS /Gy /Gw /EHsc /std:c++17 /W4 /permissive- /DNDEBUG /DUNICODE /D_UNICODE ^
   /Fobuild\memstats.obj src\memstats.cpp || exit /b 1
rem CM per-draw constant buffers mirrored into VRAM (upmirror.cpp; [GpuOpt] CbvMirror = 0; notes\root-cause-vram-streaming.md)
cl /nologo /c /O2 /MT /GS /Gy /Gw /EHsc /std:c++17 /W4 /permissive- /DNDEBUG /DUNICODE /D_UNICODE ^
   /Fobuild\upmirror.obj src\upmirror.cpp || exit /b 1
rem GPU pass profiler ([Profiler] GPUProf = 0 by default)
cl /nologo /c /O2 /MT /GS /Gy /Gw /EHsc /std:c++17 /W4 /permissive- /DNDEBUG /DUNICODE /D_UNICODE ^
   /Fobuild\gpuprof.obj src\gpuprof.cpp || exit /b 1
rem sampling profiler ([Profiler] SP = 0 by default; measurement only)
cl /nologo /c /O2 /MT /GS /Gy /Gw /EHsc /std:c++17 /W4 /permissive- /DNDEBUG /DUNICODE /D_UNICODE ^
   /Fobuild\sp.obj src\sp.cpp || exit /b 1
rem PSO bytecode swap / shader dump ([ShaderSwap] Enabled = 0 by default; notes\gpu-post-exe.md)
cl /nologo /c /O2 /MT /GS /Gy /Gw /EHsc /std:c++17 /W4 /permissive- /DNDEBUG /DUNICODE /D_UNICODE ^
   /Fobuild\psoswap.obj src\psoswap.cpp || exit /b 1
rem GI gather light-group probe / merge ([Debug] GIProbe = 0, [Patches] GIMerge = 0; notes\gi-highest-audit.md s6)
cl /nologo /c /O2 /MT /GS /Gy /Gw /EHsc /std:c++17 /W4 /permissive- /DNDEBUG /DUNICODE /D_UNICODE ^
   /Fobuild\gimerge.obj src\gimerge.cpp || exit /b 1
rem tile-mapping census / merge ([Profiler] TileStats = 0, [GpuOpt] TileBatch = 0; notes\gpu-tiles.md)
cl /nologo /c /O2 /MT /GS /Gy /Gw /EHsc /std:c++17 /W4 /permissive- /DNDEBUG /DUNICODE /D_UNICODE ^
   /Fobuild\tilestats.obj src\tilestats.cpp || exit /b 1
rem load-memory sweeper ([Memory] LM = 0 by default; notes\load-memory.md)
cl /nologo /c /O2 /MT /GS /Gy /Gw /EHsc /std:c++17 /W4 /permissive- /DNDEBUG /DUNICODE /D_UNICODE ^
   /Fobuild\loadmem.obj src\loadmem.cpp || exit /b 1
rem per-load measurement ([Profiler] LoadStats = 0 by default; notes\loading.md)
cl /nologo /c /O2 /MT /GS /Gy /Gw /EHsc /std:c++17 /W4 /permissive- /DNDEBUG /DUNICODE /D_UNICODE ^
   /Fobuild\loadstats.obj src\loadstats.cpp || exit /b 1
rem FnProbe: live function probe ([Debug] FnProbe = 0 by default; notes\remaining-work.md 10.1)
cl /nologo /c /O2 /MT /GS /Gy /Gw /EHsc /std:c++17 /W4 /permissive- /DNDEBUG /DUNICODE /D_UNICODE ^
   /Fobuild\fnprobe.obj src\fnprobe.cpp || exit /b 1
rem PadLatency: controller input -> Present / GPU-done latency ([Debug] PadLatency = 0 by default; notes\pad-latency.md)
cl /nologo /c /O2 /MT /GS /Gy /Gw /EHsc /std:c++17 /W4 /permissive- /DNDEBUG /DUNICODE /D_UNICODE ^
   /Fobuild\padlat.obj src\padlat.cpp || exit /b 1
rem Tour: automatic stage / point tour, saves blocked ([Debug] Tour = 0 by default; notes\tour-mode.md)
cl /nologo /c /O2 /MT /GS /Gy /Gw /EHsc /std:c++17 /W4 /permissive- /DNDEBUG /DUNICODE /D_UNICODE ^
   /Fobuild\tour.obj src\tour.cpp || exit /b 1
rem VirtualPad / KeepRunningUnfocused: pipe-driven test input, no OS input ([Debug] VirtualPad = KeepRunningUnfocused = 0; notes\virtual-input.md)
cl /nologo /c /O2 /MT /GS /Gy /Gw /EHsc /std:c++17 /W4 /permissive- /DNDEBUG /DUNICODE /D_UNICODE ^
   /Fobuild\vpad.obj src\vpad.cpp || exit /b 1
rem skip the post-load "press to continue" tip page ([General] AutoContinueLoad = 0 by default; notes\auto-continue-load.md)
cl /nologo /c /O2 /MT /GS /Gy /Gw /EHsc /std:c++17 /W4 /permissive- /DNDEBUG /DUNICODE /D_UNICODE ^
   /Fobuild\autocont.obj src\autocont.cpp || exit /b 1
rem SkipBootNotice ([Debug] SkipBootNotice = -1: on only in Tour / VirtualPad sessions; notes\autosave-notice.md)
cl /nologo /c /O2 /MT /GS /Gy /Gw /EHsc /std:c++17 /W4 /permissive- /DNDEBUG /DUNICODE /D_UNICODE ^
   /Fobuild\bootnotice.obj src\bootnotice.cpp || exit /b 1
rem RP read probe + EP event pool ([Profiler] ReadProbe = 0, [Patches] EP = 0; notes\loading-ally-fixed.md 8)
cl /nologo /c /O2 /MT /GS /Gy /Gw /EHsc /std:c++17 /W4 /permissive- /DNDEBUG /DUNICODE /D_UNICODE ^
   /Fobuild\readprobe.obj src\readprobe.cpp || exit /b 1
rem LO loads-only gating of GF/W6/RWb ([Patches] LoadOnlyCpuFixes = 0; notes\loading-ally-fixed.md 8.5)
cl /nologo /c /O2 /MT /GS /Gy /Gw /EHsc /std:c++17 /W4 /permissive- /DNDEBUG /DUNICODE /D_UNICODE ^
   /Fobuild\loadonly.obj src\loadonly.cpp || exit /b 1
rem PF trace prefetch ([Patches] PF = 0; notes\loading-ally-fixed.md 8.8)
cl /nologo /c /O2 /MT /GS /Gy /Gw /EHsc /std:c++17 /W4 /permissive- /DNDEBUG /DUNICODE /D_UNICODE ^
   /Fobuild\prefetch.obj src\prefetch.cpp || exit /b 1
rem RC read cache / AQ async archive reads / TSY / SH ([Patches] RC = AQ = TSY = SH = 0; notes\loading-ally-fixed.md 8.7)
cl /nologo /c /O2 /MT /GS /Gy /Gw /EHsc /std:c++17 /W4 /permissive- /DNDEBUG /DUNICODE /D_UNICODE ^
   /Fobuild\readfix.obj src\readfix.cpp || exit /b 1
rem PJ PSO journal + title-screen prewarm ([Patches] PJ = 0; notes\stutter.md 3.1)
cl /nologo /c /O2 /MT /GS /Gy /Gw /EHsc /std:c++17 /W4 /permissive- /DNDEBUG /DUNICODE /D_UNICODE ^
   /Fobuild\psojournal.obj src\psojournal.cpp || exit /b 1
rem present mode log + [Latency] VrrPresent (notes\vrr-present.md)
cl /nologo /c /O2 /MT /GS /Gy /Gw /EHsc /std:c++17 /W4 /permissive- /DNDEBUG /DUNICODE /D_UNICODE ^
   /Fobuild\pmode.obj src\pmode.cpp || exit /b 1
rem SkipFx diagnostic skip list for the cost-vs-looks audit (skipfx.cpp; [Debug] SkipFx = 0; notes\cost-vs-looks.md)
cl /nologo /c /O2 /MT /GS /Gy /Gw /EHsc /std:c++17 /W4 /permissive- /DNDEBUG /DUNICODE /D_UNICODE ^
   /Fobuild\skipfx.obj src\skipfx.cpp || exit /b 1
rem LiveCtl one-launch A/B control file (livectl.cpp; [Debug] LiveCtl = 0; notes\fast-testing.md)
cl /nologo /c /O2 /MT /GS /Gy /Gw /EHsc /std:c++17 /W4 /permissive- /DNDEBUG /DUNICODE /D_UNICODE ^
   /Fobuild\livectl.obj src\livectl.cpp || exit /b 1
rem RB Resizable BAR check + one-time notice ([General] ReBarNotice = 1; notes\rebar-fix-v3.md)
cl /nologo /c /O2 /MT /GS /Gy /Gw /EHsc /std:c++17 /W4 /permissive- /DNDEBUG /DUNICODE /D_UNICODE ^
   /Fobuild\rebar.obj src\rebar.cpp || exit /b 1
rem SBM meshlet scene-buffer mirror ([GpuOpt] SceneBufMirror = 0, [Profiler] SceneBufProbe = 0; notes\rebar-fix-v3.md)
cl /nologo /c /O2 /MT /GS /Gy /Gw /EHsc /std:c++17 /W4 /permissive- /DNDEBUG /DUNICODE /D_UNICODE ^
   /Fobuild\scenemirror.obj src\scenemirror.cpp || exit /b 1
rem MLP meshlet probe CPU half + MpcCheck self-check + MPCS caves (mlprobe.cpp / mpccheck.cpp / mpcskip.asm; notes\meshlet-frontend.md)
cl /nologo /c /O2 /MT /GS /Gy /Gw /EHsc /std:c++17 /W4 /permissive- /DNDEBUG /DUNICODE /D_UNICODE ^
   /Fobuild\mlprobe.obj src\mlprobe.cpp || exit /b 1
cl /nologo /c /O2 /MT /GS /Gy /Gw /EHsc /std:c++17 /W4 /permissive- /DNDEBUG /DUNICODE /D_UNICODE ^
   /Fobuild\mpccheck.obj src\mpccheck.cpp || exit /b 1
ml64 /nologo /c /W3 /Fobuild\mpcskip_caves.obj src\mpcskip.asm || exit /b 1
rem TR timer-resolution hold (timerres.cpp; [Patches] TR = 0; notes\game-logic-audit.md)
cl /nologo /c /O2 /MT /GS /Gy /Gw /EHsc /std:c++17 /W4 /permissive- /DNDEBUG /DUNICODE /D_UNICODE ^
   /Fobuild\timerres.obj src\timerres.cpp || exit /b 1
rem TSTATE census + RTP render-thread priority (thrsched.cpp; [Profiler] ThreadState = 0, [Patches] RTP = 0; notes\cpu-bound-midrange.md)
cl /nologo /c /O2 /MT /GS /Gy /Gw /EHsc /std:c++17 /W4 /permissive- /DNDEBUG /DUNICODE /D_UNICODE ^
   /Fobuild\thrsched.obj src\thrsched.cpp || exit /b 1
rem PM graphics-PSO lookup memo (psomemo.cpp; [Patches] PM = 0; notes\cpu-bound-midrange.md)
cl /nologo /c /O2 /MT /GS /Gy /Gw /EHsc /std:c++17 /W4 /permissive- /DNDEBUG /DUNICODE /D_UNICODE ^
   /Fobuild\psomemo.obj src\psomemo.cpp || exit /b 1
rem BKS back-end census + KL kick limit (bkstats.cpp; [Profiler] Backend = 0, [Tuning] KL_Div = 0; notes\renderer\backend.md)
cl /nologo /c /O2 /MT /GS /Gy /Gw /EHsc /std:c++17 /W4 /permissive- /DNDEBUG /DUNICODE /D_UNICODE ^
   /Fobuild\bkstats.obj src\bkstats.cpp || exit /b 1
rem render-node timer (rnt.cpp; [Profiler] NodeTimer = 0, F11 arms; notes\node-timer-33584.md)
cl /nologo /c /O2 /MT /GS /Gy /Gw /EHsc /std:c++17 /W4 /permissive- /DNDEBUG /DUNICODE /D_UNICODE ^
   /Fobuild\rnt.obj src\rnt.cpp || exit /b 1
rem FO frame overlap (frameoverlap.cpp; [GpuOpt] FrameOverlap = 0; notes\renderer\frame-overlap.md)
cl /nologo /c /O2 /MT /GS /Gy /Gw /EHsc /std:c++17 /W4 /permissive- /DNDEBUG /DUNICODE /D_UNICODE ^
   /Fobuild\frameoverlap.obj src\frameoverlap.cpp || exit /b 1
rem RPF replay-thread profiler (replayprof.cpp / .asm; [Profiler] ReplayProf = 0; notes\renderer\replay-profile.md)
cl /nologo /c /O2 /MT /GS /Gy /Gw /EHsc /std:c++17 /W4 /permissive- /DNDEBUG /DUNICODE /D_UNICODE ^
   /Fobuild\replayprof.obj src\replayprof.cpp || exit /b 1
ml64 /nologo /c /W3 /Fobuild\replayprof_thunks.obj src\replayprof.asm || exit /b 1
rem CBU per-draw constant uploads (cbvupload.cpp; [Patches] CBS / CBN / CBT = 0; notes\renderer\cbv-upload.md)
cl /nologo /c /O2 /MT /GS /Gy /Gw /EHsc /std:c++17 /W4 /permissive- /DNDEBUG /DUNICODE /D_UNICODE ^
   /Fobuild\cbvupload.obj src\cbvupload.cpp || exit /b 1
cl /nologo /c /O2 /MT /GS /Gy /Gw /EHsc /std:c++17 /W4 /permissive- /DNDEBUG /DUNICODE /D_UNICODE ^
   /Fobuild\cbvstage.obj src\cbvstage.cpp || exit /b 1
rem same-look package: VSMA + update-rate knobs LPVR / SKYR / FOGT / CLDR (samelook.cpp / .asm; all default off; notes\same-look-package.md)
cl /nologo /c /O2 /MT /GS /Gy /Gw /EHsc /std:c++17 /W4 /permissive- /DNDEBUG /DUNICODE /D_UNICODE ^
   /Fobuild\samelook.obj src\samelook.cpp || exit /b 1
ml64 /nologo /c /W3 /Fobuild\samelook_caves.obj src\samelook.asm || exit /b 1
rem Extra performance (changes the look): XCLD XFOG XGI XSHD XSHF XLOD XTER XPRT XAO XPOST (extraperf.cpp / .asm; [ExtraPerf]; notes\extra-performance-settings.md)
cl /nologo /c /O2 /MT /GS /Gy /Gw /EHsc /std:c++17 /W4 /permissive- /DNDEBUG /DUNICODE /D_UNICODE ^
   /Fobuild\extraperf.obj src\extraperf.cpp || exit /b 1
ml64 /nologo /c /W3 /Fobuild\extraperf_caves.obj src\extraperf.asm || exit /b 1
cl /nologo /c /O2 /MT /GS /Gy /Gw /EHsc /std:c++17 /W4 /permissive- /DNDEBUG /DUNICODE /D_UNICODE ^
   /Fobuild\crashreport.obj src\crashreport.cpp || exit /b 1
link /nologo /DLL /OUT:bin\Nioh3EngineFix.asi /PDB:build\Nioh3EngineFix.pdb /DEBUG:FULL /OPT:REF /OPT:ICF ^
   /DYNAMICBASE /NXCOMPAT /MACHINE:X64 ^
   build\updatecheck.obj build\gamerva.obj build\crashreport.obj build\readprobe.obj build\readfix.obj build\prefetch.obj build\loadonly.obj build\autocont.obj build\bootnotice.obj build\fnprobe.obj build\padlat.obj build\tour.obj build\vpad.obj build\tilestats.obj build\loadstats.obj build\loadmem.obj build\dllmain.obj build\dxgidiag.obj build\osd.obj build\hitchlog.obj build\menu.obj build\savefiles.obj build\savelib.obj build\gpuopt.obj build\gimerge.obj build\skipfx.obj build\livectl.obj build\gpuprof.obj build\sp.obj build\psoswap.obj build\psojournal.obj build\memstats.obj build\upmirror.obj build\rebar.obj build\scenemirror.obj build\drawprep.obj build\drawprep_caves.obj build\jt.obj build\jt_caves.obj build\pmode.obj build\pmode_caves.obj build\asyncslots.obj build\asyncslots_caves.obj build\timerres.obj build\thrsched.obj build\psomemo.obj build\bkstats.obj build\rnt.obj build\frameoverlap.obj build\replayprof.obj build\replayprof_thunks.obj build\cbvupload.obj build\cbvstage.obj build\samelook.obj build\samelook_caves.obj build\extraperf.obj build\extraperf_caves.obj build\mlprobe.obj build\mpccheck.obj build\mpcskip_caves.obj build\caves.obj build\gpuopt_thunks.obj kernel32.lib user32.lib shell32.lib winhttp.lib advapi32.lib bcrypt.lib || exit /b 1
copy /y Nioh3EngineFix.ini bin\ >nul
rem Ship the Python helpers (only used with [SaveFiles] Native = 0; the default native path needs no Python).
py ..\..\tools\saves\build_package.py || exit /b 1
py ..\..\tools\saves\build_live_package.py || exit /b 1
if not exist bin\save_helpers mkdir bin\save_helpers
copy /y ..\..\tools\saves\dist\SaveSlots-after-exit.pyz bin\save_helpers\ >nul || exit /b 1
copy /y ..\..\tools\saves\dist\SaveSlots-live-title.pyz bin\save_helpers\ >nul || exit /b 1
echo built %~dp0bin\Nioh3EngineFix.asi
endlocal
