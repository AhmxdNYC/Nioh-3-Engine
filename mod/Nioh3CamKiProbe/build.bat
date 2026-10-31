@echo off
rem Read-only probe ASI. Needs MSVC (cl, link). Calls the portable toolchain if cl is not on PATH.
setlocal
cd /d "%~dp0"
where cl >nul 2>nul && goto :have
if exist "%~dp0..\..\tools\msvc\setup_x64.bat" ( call "%~dp0..\..\tools\msvc\setup_x64.bat" & goto :have )
if exist "C:\nioh3-engine\tools\msvc\setup_x64.bat" ( call "C:\nioh3-engine\tools\msvc\setup_x64.bat" & goto :have )
echo ERROR: no MSVC toolchain found
exit /b 1
:have
where cl >nul 2>nul || (echo ERROR: no MSVC toolchain found & exit /b 1)
if not exist build mkdir build
if not exist bin mkdir bin
cl /nologo /c /O2 /MT /EHa /GS /W4 /permissive- /std:c++17 /D_CRT_SECURE_NO_WARNINGS /DNDEBUG /Fobuild\ src\probe.cpp src\gamerva.cpp || exit /b 1
link /nologo /DLL /OUT:bin\Nioh3CamKiProbe.asi /PDB:build\Nioh3CamKiProbe.pdb /DEBUG:FULL /OPT:REF /OPT:ICF /DYNAMICBASE /NXCOMPAT /MACHINE:X64 build\probe.obj build\gamerva.obj kernel32.lib user32.lib || exit /b 1
cl /nologo /O2 /MT /EHa /W4 /permissive- /std:c++17 /DPROBE_TEST /D_CRT_SECURE_NO_WARNINGS /Febuild\probe_test.exe src\probe.cpp kernel32.lib user32.lib || exit /b 1
build\probe_test.exe || exit /b 1
echo built %~dp0bin\Nioh3CamKiProbe.asi
endlocal
