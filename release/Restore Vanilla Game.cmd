@echo off
setlocal EnableExtensions
rem Double-click in the Nioh 3 folder. Turns the mod off. Does not touch saves or ini files.
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
  echo This turns the mod off so Nioh 3 starts as the normal game.
  echo Saves, ini settings, the updater, and the website shortcut stay.
  echo.
  choice /C YN /N /M "Turn the mod off now? Y or N: "
  if errorlevel 2 (
    echo Cancelled. Nothing was changed.
    exit /b 0
  )
)

set "MOVED=0"
call :park dinput8.dll
if errorlevel 1 exit /b 1
call :park Nioh3EngineFix.asi
if errorlevel 1 exit /b 1
call :park Nioh3BossRematch.asi
if errorlevel 1 exit /b 1
call :park Nioh3Training.asi
if errorlevel 1 exit /b 1
call :park Nioh3CamKiProbe.asi
if errorlevel 1 exit /b 1
if "%MOVED%"=="0" (
  echo The mod is already off. Double-click "Put the mod back.cmd" to turn it on.
  if not "%~2"=="--yes" pause
  exit /b 0
)
echo The mod is off. Start Nioh 3 from Steam for the normal game.
echo Saves were not touched. Double-click "Put the mod back.cmd" to turn the mod on again.
if not "%~2"=="--yes" pause
exit /b 0

:park
if not exist "%ROOT%%~1" exit /b 0
if exist "%ROOT%%~1.mod-off" (
  echo Left %~1 in place because %~1.mod-off is already there.
  exit /b 1
)
ren "%ROOT%%~1" "%~1.mod-off"
if errorlevel 1 (
  echo Could not rename %~1. Close the game and try again.
  exit /b 1
)
set "MOVED=1"
exit /b 0
