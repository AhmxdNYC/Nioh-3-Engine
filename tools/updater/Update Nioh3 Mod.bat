@echo off
setlocal
title Nioh 3 Mod Updater
if not exist "%~dp0Nioh3Updater.exe" (
    echo The updater is missing. Keep Nioh3Updater.exe next to this file.
    pause
    exit /b 1
)
"%~dp0Nioh3Updater.exe" --game-dir "%~dp0." --config "%~dp0Nioh3Updater.ini" --no-pause
set "update_result=%errorlevel%"
if not "%update_result%"=="0" echo The update did not finish. Read the message above before trying again.
pause
exit /b %update_result%
