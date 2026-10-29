@echo off
cd /d "%~dp0..\.."
call "tools\msvc\setup_x64.bat" >nul
cl /nologo /O2 /MT /EHsc /std:c++17 /W4 /DUNICODE /D_UNICODE mod\Nioh3EngineFix\tests\savefiles_harness.cpp /Fomod\Nioh3EngineFix\build\savefiles_harness.obj /Femod\Nioh3EngineFix\build\savefiles_harness.exe /link kernel32.lib user32.lib
if errorlevel 1 exit /b 1
mod\Nioh3EngineFix\build\savefiles_harness.exe
