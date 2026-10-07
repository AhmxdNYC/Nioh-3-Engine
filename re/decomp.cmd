@echo off
setlocal EnableDelayedExpansion
:: decomp.cmd - query the analyzed Nioh3 Ghidra project (read-only; nothing is saved). Addresses are RVAs.
::   decomp.cmd 0x5af50a                 decompile the function containing RVA (callers/callees in header)
::   decomp.cmd asm:0x5af50a[:N]         disassembly +-N instructions around RVA (default 24)
::   decomp.cmd x:0x3a9a6e0              references to RVA
::   decomp.cmd c:0x5af50a               callers (call sites) of the containing function
::   decomp.cmd "s:REN_FPS|DYNAMIC_FPS"  strings (Ghidra-defined only; prefer out\strings.tsv) matching a Java regex
:: Several queries in one call share the ~5-10 s startup: decomp.cmd 0x5af50a asm:0x5af50a c:0x5af50a
:: Output goes to stdout; set N3Q_OUT=path to keep the output file instead of a temp one.
:: Ghidra takes an EXCLUSIVE project lock (even read-only), so queries are serialized via a mutex dir.
if "%~1"=="" ( findstr /b "::" "%~f0" & exit /b 1 )
set "RE=%~dp0"
set "GH=C:\Users\Ahmad Hamza\ghidra\ghidra_12.1.4_PUBLIC\support\analyzeHeadless.bat"
if not defined GHIDRA_HEADLESS_MAXMEM set GHIDRA_HEADLESS_MAXMEM=4G
set "OUTF=%N3Q_OUT%"
if not defined OUTF set "OUTF=%TEMP%\n3q_%RANDOM%%RANDOM%.txt"
set "LOGF=%TEMP%\n3q_log_%RANDOM%%RANDOM%.txt"
set /a TRIES=0
:waitlock
mkdir "%RE%ghidra_proj\.n3query.mutex" 2>nul
if errorlevel 1 (
  set /a TRIES+=1
  if !TRIES! GEQ 300 ( echo [decomp] project busy for 10 min - delete "%RE%ghidra_proj\.n3query.mutex" if no java.exe is using the project & exit /b 2 )
  if !TRIES!==1 echo [decomp] waiting for another query to finish... 1>&2
  ping -n 3 127.0.0.1 >nul
  goto waitlock
)
call "%GH%" "%RE%ghidra_proj" Nioh3 -process Nioh3.dump.exe -readOnly -noanalysis -scriptPath "%RE%scripts" -postScript N3Query.java "%OUTF%" %* > "%LOGF%" 2>&1 < nul
rmdir "%RE%ghidra_proj\.n3query.mutex" 2>nul
if exist "%OUTF%" (
  type "%OUTF%"
  if not defined N3Q_OUT del "%OUTF%"
  del "%LOGF%"
) else (
  echo [decomp] query failed, headless log follows: & type "%LOGF%"
  exit /b 3
)
