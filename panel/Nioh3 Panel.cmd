@echo off
:: Starts the Nioh 3 engine-fix panel (F10 toggles it over the game). Uses the Microsoft-signed dotnet host.
start "" powershell -NoProfile -WindowStyle Hidden -Command "dotnet fsi '%~dp0Nioh3Panel.fsx' *> '%~dp0panel.log'"
