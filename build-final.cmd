@echo off
call "C:/Users/Ahmad Hamza/nioh3-final3-39541/tools/msvc/setup_x64.bat" >nul
set TEMP=D:\nioh3-final-build-42021\temp
set TMP=%TEMP%
set ZIG_GLOBAL_CACHE_DIR=D:\uo-release-185-40866\zig-cache
set NIOH3_DATA_ROOT=C:\Users\Ahmad Hamza\nioh3-engine
set N3_EXTRACT=C:\Users\Ahmad Hamza\nioh3-engine\data\extract
py mod/Nioh3EngineFix/tests/run_updatecheck.py
if errorlevel 1 exit /b 1
py mod/Nioh3EngineFix/tests/run_menu_harness.py
if errorlevel 1 exit /b 1
py mod/Nioh3EngineFix/tests/run_titlegate_test.py
if errorlevel 1 exit /b 1
cl /nologo /O2 /MT /EHsc /std:c++17 /W4 /DUNICODE /D_UNICODE mod\Nioh3EngineFix\tests\savefiles_harness.cpp /Fotemp\savefiles.obj /Fetemp\savefiles.exe /link kernel32.lib user32.lib
if errorlevel 1 exit /b 1
temp\savefiles.exe
if errorlevel 1 exit /b 1
call mod\Nioh3EngineFix\build.bat
if errorlevel 1 exit /b 1
cd /d D:\nioh3-final-build-42021
call mod\Nioh3BossRematch\build.bat
if errorlevel 1 exit /b 1
cd /d D:\nioh3-final-build-42021
call mod\Nioh3CamKiProbe\build.bat
if errorlevel 1 exit /b 1
cd /d D:\nioh3-final-build-42021
py -m ziglang cc -target x86_64-windows-gnu -shared -O2 -fms-extensions -Xclang -fasync-exceptions -Wall mod/Nioh3Training/src/training.c -o mod/Nioh3Training/Nioh3Training.asi -lgdi32 -lmsimg32
if errorlevel 1 exit /b 1
echo FINAL BUILD PASS
