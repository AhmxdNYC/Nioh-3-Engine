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

if not exist build mkdir build
if not exist bin mkdir bin

ml64 /nologo /c /W3 /Fobuild\caves.obj src\caves.asm || exit /b 1
rem GPU barrier batching (gpuopt.cpp): per-slot command-list thunks
ml64 /nologo /c /W3 /Fobuild\gpuopt_thunks.obj src\gpuopt_thunks.asm || exit /b 1
rem DQ / P1E7 caves (drawprep.cpp; [Patches] DQ = P1E7 = 0; notes\remaining-work.md 2.5)
ml64 /nologo /c /W3 /Fobuild\drawprep_caves.obj src\drawprep.asm || exit /b 1
cl /nologo /c /O2 /MT /GS /Gy /Gw /EHsc /std:c++17 /W4 /permissive- /DNDEBUG /DUNICODE /D_UNICODE ^
   /Fobuild\drawprep.obj src\drawprep.cpp || exit /b 1
cl /nologo /c /O2 /MT /GS /Gy /Gw /EHsc /std:c++17 /W4 /permissive- /DNDEBUG /DUNICODE /D_UNICODE ^
   /Fobuild\dllmain.obj src\dllmain.cpp || exit /b 1
rem in-game overlay (d3d12.dll / dxgi.dll are loaded at run time - no import dependency)
cl /nologo /c /O2 /MT /GS /Gy /Gw /EHsc /std:c++17 /W4 /permissive- /DNDEBUG /DUNICODE /D_UNICODE ^
   /Fobuild\osd.obj src\osd.cpp || exit /b 1
rem mod menu (xinput / dinput8 are loaded at run time too)
cl /nologo /c /O2 /MT /GS /Gy /Gw /EHsc /std:c++17 /W4 /permissive- /DNDEBUG /DUNICODE /D_UNICODE ^
   /Fobuild\menu.obj src\menu.cpp || exit /b 1
rem GPU barrier batching (notes\gpu-barriers.md; [GpuOpt] Barriers = 0 by default)
cl /nologo /c /O2 /MT /GS /Gy /Gw /EHsc /std:c++17 /W4 /permissive- /DNDEBUG /DUNICODE /D_UNICODE ^
   /Fobuild\gpuopt.obj src\gpuopt.cpp || exit /b 1
rem memory line ([Profiler] MemStats = 0 by default)
cl /nologo /c /O2 /MT /GS /Gy /Gw /EHsc /std:c++17 /W4 /permissive- /DNDEBUG /DUNICODE /D_UNICODE ^
   /Fobuild\memstats.obj src\memstats.cpp || exit /b 1
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
rem boss fight logger ([Debug] BossLog = 0 by default; notes\boss-fights.md)
cl /nologo /c /O2 /MT /GS /Gy /Gw /EHsc /std:c++17 /W4 /permissive- /DNDEBUG /DUNICODE /D_UNICODE ^
   /Fobuild\bosslog.obj src\bosslog.cpp || exit /b 1
rem skip the post-load "press to continue" tip page ([General] AutoContinueLoad = 1; notes\auto-continue-load.md)
cl /nologo /c /O2 /MT /GS /Gy /Gw /EHsc /std:c++17 /W4 /permissive- /DNDEBUG /DUNICODE /D_UNICODE ^
   /Fobuild\autocont.obj src\autocont.cpp || exit /b 1
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
link /nologo /DLL /OUT:bin\Nioh3EngineFix.asi /PDB:build\Nioh3EngineFix.pdb /DEBUG:FULL /OPT:REF /OPT:ICF ^
   /DYNAMICBASE /NXCOMPAT /MACHINE:X64 ^
   build\readprobe.obj build\readfix.obj build\prefetch.obj build\loadonly.obj build\autocont.obj build\bosslog.obj build\tilestats.obj build\loadstats.obj build\loadmem.obj build\dllmain.obj build\osd.obj build\menu.obj build\gpuopt.obj build\gimerge.obj build\gpuprof.obj build\sp.obj build\psoswap.obj build\psojournal.obj build\memstats.obj build\drawprep.obj build\drawprep_caves.obj build\caves.obj build\gpuopt_thunks.obj kernel32.lib user32.lib || exit /b 1
copy /y Nioh3EngineFix.ini bin\ >nul
echo built %~dp0bin\Nioh3EngineFix.asi
endlocal
