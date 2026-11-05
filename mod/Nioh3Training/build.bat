@echo off
setlocal
cd /d "%~dp0"
py -m ziglang cc -target x86_64-windows-gnu -shared -O2 -fms-extensions -Xclang -fasync-exceptions -Wall src/training.c -o Nioh3Training.asi -lgdi32 -lmsimg32
exit /b %errorlevel%
