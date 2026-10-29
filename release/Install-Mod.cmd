@echo off
py -3 -I "%~dp0install_mod.py" %*
exit /b %errorlevel%
