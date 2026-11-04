@echo off
rem KeepGear veto harness (site report #7): real hooks.cpp thunk + InstallVeto/ParseNative/SkipNative on a synthetic binding wrapper.
setlocal
cd /d "%~dp0"
where cl >nul 2>nul || call "%~dp0..\..\..\tools\msvc\setup_x64.bat"
if not exist ..\build mkdir ..\build
cl /nologo /O2 /MT /EHsc /std:c++17 /W4 /DUNICODE /D_UNICODE /DBR_SOULCORE=1 /Fe..\build\keepgear_veto_harness.exe /Fo..\build\ keepgear_veto_harness.cpp kernel32.lib || exit /b 1
..\build\keepgear_veto_harness.exe
