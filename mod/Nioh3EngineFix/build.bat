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
link /nologo /DLL /OUT:bin\Nioh3EngineFix.asi /PDB:build\Nioh3EngineFix.pdb /DEBUG:FULL /OPT:REF /OPT:ICF ^
   /DYNAMICBASE /NXCOMPAT /MACHINE:X64 ^
   build\dllmain.obj build\osd.obj build\menu.obj build\gpuopt.obj build\gpuprof.obj build\sp.obj build\psoswap.obj build\memstats.obj build\caves.obj build\gpuopt_thunks.obj kernel32.lib user32.lib || exit /b 1
copy /y Nioh3EngineFix.ini bin\ >nul
echo built %~dp0bin\Nioh3EngineFix.asi
endlocal
