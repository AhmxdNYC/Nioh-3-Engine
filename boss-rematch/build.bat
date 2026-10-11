@echo off
rem Build Nioh3BossRematch.asi (Release x64, /O2, static CRT). Needs MSVC (cl, link) + Windows SDK. Toolchain search: PATH, the
rem portable MSVC at ..\..\tools\msvc (or the main checkout's), then Visual Studio via vswhere.
setlocal
cd /d "%~dp0"
where cl >nul 2>nul && goto :have
if exist "%~dp0..\..\tools\msvc\setup_x64.bat" ( call "%~dp0..\..\tools\msvc\setup_x64.bat" & goto :have )
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if exist "%VSWHERE%" (
    for /f "usebackq delims=" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do (
        call "%%i\VC\Auxiliary\Build\vcvars64.bat" >nul
    )
)
:have
where cl >nul 2>nul || (echo ERROR: no MSVC toolchain found & exit /b 1)
if not exist build mkdir build
if not exist bin mkdir bin
set CF=/nologo /c /O2 /MT /GS /Gy /Gw /EHsc /std:c++17 /W4 /permissive- /DNDEBUG /DUNICODE /D_UNICODE
for %%f in (main sites hooks bosslog rematch ui remote) do (
    cl %CF% /Fobuild\%%f.obj src\%%f.cpp || exit /b 1
)
link /nologo /DLL /OUT:bin\Nioh3BossRematch.asi /PDB:build\Nioh3BossRematch.pdb /DEBUG:FULL /OPT:REF /OPT:ICF /DYNAMICBASE /NXCOMPAT /MACHINE:X64 ^
   build\main.obj build\sites.obj build\hooks.obj build\bosslog.obj build\rematch.obj build\ui.obj build\remote.obj kernel32.lib user32.lib gdi32.lib || exit /b 1
copy /y Nioh3BossRematch.ini bin\ >nul
copy /y Nioh3BossRematch_bosses.tsv bin\ >nul
echo built %~dp0bin\Nioh3BossRematch.asi
endlocal
