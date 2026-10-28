@echo off
setlocal EnableExtensions
rem Double-click in the Nioh 3 folder. Turns the mod back on. Does not touch saves or ini files.
rem A test may set NIOH3_RESTORE_SKIP_RUNNING_CHECK=1. A double-click never does.
if not "%~1"=="" (pushd "%~1" || exit /b 3) else (pushd "%~dp0" || exit /b 3)
set "ROOT=%CD%"
popd
set ROOT=%ROOT%\

if not exist "%ROOT%Nioh3.exe" (
  echo Put this file in the Nioh 3 folder, next to Nioh3.exe.
  echo Nothing was changed.
  if not "%~2"=="--yes" pause
  exit /b 3
)

if not "%NIOH3_RESTORE_SKIP_RUNNING_CHECK%"=="1" (
  tasklist /FI "IMAGENAME eq Nioh3.exe" /FO CSV /NH 2>nul | findstr /I /C:"Nioh3.exe" >nul
  if not errorlevel 1 (
    echo Close Nioh 3 first. Nothing was changed. Saves were not touched.
    if not "%~2"=="--yes" pause
    exit /b 2
  )
)

if /I not "%~2"=="--yes" (
  echo This turns the mod back on.
  echo Saves and ini settings stay as they are.
  echo.
  choice /C YN /N /M "Turn the mod on now? Y or N: "
  if errorlevel 2 (
    echo Cancelled. Nothing was changed.
    exit /b 0
  )
)

set "MOVED=0"
call :back dinput8.dll
if errorlevel 1 exit /b 1
call :back Nioh3EngineFix.asi
if errorlevel 1 exit /b 1
call :back Nioh3BossRematch.asi
if errorlevel 1 exit /b 1
call :back Nioh3Training.asi
if errorlevel 1 exit /b 1
call :back Nioh3CamKiProbe.asi
if errorlevel 1 exit /b 1
if "%MOVED%"=="0" (
  echo No parked mod files were found. Nothing was changed.
  if not "%~2"=="--yes" pause
  exit /b 0
)
echo The mod is back. Start Nioh 3 from Steam.
echo Saves were not touched.
if not "%~2"=="--yes" pause
exit /b 0

:back
if not exist "%ROOT%%~1.mod-off" exit /b 0
if exist "%ROOT%%~1" (
  echo Left %~1.mod-off because %~1 is already there.
  exit /b 1
)
ren "%ROOT%%~1.mod-off" "%~1"
if errorlevel 1 (
  echo Could not rename %~1.mod-off. Close the game and try again.
  exit /b 1
)
set "MOVED=1"
exit /b 0
