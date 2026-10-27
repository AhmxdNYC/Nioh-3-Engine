@echo off
call "C:/Users/Ahmad Hamza/nioh3-final3-39541/tools/msvc/setup_x64.bat" >nul
set TEMP=D:\uo-release-185-40866\temp
set TMP=%TEMP%
set ZIG_GLOBAL_CACHE_DIR=D:\uo-release-185-40866\zig-cache
set NIOH3_DATA_ROOT=C:\Users\Ahmad Hamza\nioh3-engine
call mod\Nioh3EngineFix\build.bat
if errorlevel 1 exit /b 1
cd /d D:\uo-release-185-40866
call mod\Nioh3BossRematch\build.bat
if errorlevel 1 exit /b 1
cd /d D:\uo-release-185-40866
call mod\Nioh3CamKiProbe\build.bat
if errorlevel 1 exit /b 1
cd /d D:\uo-release-185-40866\mod\Nioh3Training
py -m ziglang cc -target x86_64-windows-gnu -shared -O2 -fms-extensions -Xclang -fasync-exceptions -Wall src/training.c -o Nioh3Training.asi -lgdi32 -lmsimg32
if errorlevel 1 exit /b 1
cd /d D:\uo-release-185-40866
py mod/Nioh3Training/tests/run_frame_meter_draw.py
if errorlevel 1 exit /b 1
py mod/Nioh3EngineFix/tests/run_menu_harness.py
if errorlevel 1 exit /b 1
py mod/Nioh3EngineFix/tests/run_updatecheck.py
if errorlevel 1 exit /b 1
py tools/test_hitcount_40826.py
if errorlevel 1 exit /b 1
py -I tools/gen_boss_move_facts.py --check
if errorlevel 1 exit /b 1
py -m unittest discover -s tools/updater -p test_updater.py
if errorlevel 1 exit /b 1
echo ALL RELEASE CHECKS PASSED
