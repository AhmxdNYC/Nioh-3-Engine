@echo off
setlocal EnableDelayedExpansion
:: apply.cmd - apply re\symbols\symbols.tsv to the Nioh3 Ghidra project (WRITES + SAVES the project).
::   apply.cmd            apply symbols.tsv (renames, labels, comments, /N3 structs), save the project
::   apply.cmd dryrun     same, but the transaction is rolled back: nothing is saved, report shows what would change
::   apply.cmd [dryrun] path\to\other.tsv   apply a different TSV (same format)
:: Report: re\symbols\last_apply_report.txt   Headless log: re\symbols\last_apply.log
:: Takes the same mutex dir as decomp.cmd (re\ghidra_proj\.n3query.mutex), so it never runs alongside a query.
:: Heap is pinned to 4G (GHIDRA_HEADLESS_MAXMEM) - never raise it, the game and other agents share this machine.
for %%I in ("%~dp0..") do set "RE=%%~fI\"
set "SYM=%~dp0"
set "GH=C:\Users\Ahmad Hamza\ghidra\ghidra_12.1.4_PUBLIC\support\analyzeHeadless.bat"
set GHIDRA_HEADLESS_MAXMEM=4G
set "DRY="
set "TSV=%SYM%symbols.tsv"
:args
if "%~1"=="" goto argsdone
if /i "%~1"=="dryrun" ( set "DRY=dryrun" ) else ( set "TSV=%~f1" )
shift
goto args
:argsdone
if not exist "%TSV%" ( echo [apply] missing %TSV% & exit /b 1 )
:: Saving writes a NEW copy of the program DB (~1.2 GB db.N.gbf) before dropping the old one: need free space first.
:: (2026-10-06: the first run applied everything in memory but the save died with "not enough space on the disk".)
set "FREEGB=0"
for /f %%F in ('powershell -NoProfile -Command "[math]::Floor((Get-Item -LiteralPath '%RE%ghidra_proj').PSDrive.Free/1GB)"') do set "FREEGB=%%F"
if not defined DRY if !FREEGB! LSS 3 ( echo [apply] only !FREEGB! GB free on the project drive - need ^>= 3 GB to save the ~1.2 GB program DB. Free space first. & exit /b 4 )
set "REPF=%SYM%last_apply_report.txt"
set "LOGF=%SYM%last_apply.log"
if exist "%REPF%" del "%REPF%"
set /a TRIES=0
:waitlock
mkdir "%RE%ghidra_proj\.n3query.mutex" 2>nul
if errorlevel 1 (
  set /a TRIES+=1
  if !TRIES! GEQ 1800 ( echo [apply] project busy for 60 min - giving up. Do NOT delete the mutex while another java.exe query runs. & exit /b 2 )
  if !TRIES!==1 echo [apply] waiting for another query to finish... 1>&2
  ping -n 3 127.0.0.1 >nul
  goto waitlock
)
set "RO="
if defined DRY set "RO=-readOnly"
echo [apply] mutex taken, running headless %DRY%... 1>&2
call "%GH%" "%RE%ghidra_proj" Nioh3 -process Nioh3.dump.exe %RO% -noanalysis -scriptPath "%RE%scripts" -postScript N3ApplySymbols.java "%TSV%" "%REPF%" %DRY% > "%LOGF%" 2>&1 < nul
set RC=%ERRORLEVEL%
rmdir "%RE%ghidra_proj\.n3query.mutex" 2>nul
if not exist "%REPF%" (
  echo [apply] script failed rc=%RC%, headless log follows: & type "%LOGF%"
  exit /b 3
)
type "%REPF%"
findstr /c:"Save not allowed" /c:"not enough space" /c:"Error during analysis" "%LOGF%" >nul
if not errorlevel 1 (
  echo [apply] SAVE FAILED - nothing was written to the project. Errors from %LOGF%:
  findstr /c:"ERROR" "%LOGF%"
  exit /b 5
)
if defined DRY ( echo [apply] dry run - rolled back, nothing saved ) else ( echo [apply] done - project saved. Log: %LOGF% )
