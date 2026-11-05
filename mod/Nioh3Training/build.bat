@echo off
rem Windows equivalent of build.sh: same Zig/Clang target and SEH flags.
setlocal
cd /d "%~dp0"
py -m ziglang cc -target x86_64-windows-gnu -shared -O2 -fms-extensions -Xclang -fasync-exceptions -Wall src/training.c -o Nioh3Training.asi -lgdi32 -lmsimg32
if errorlevel 1 exit /b 1
copy /y "..\..\data\boss-action-record.tsv" "Nioh3Training_action_records.tsv" >nul
if errorlevel 1 exit /b 1
echo built %~dp0Nioh3Training.asi
exit /b 0
