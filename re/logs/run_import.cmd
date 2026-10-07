@echo off
set GHIDRA_HEADLESS_MAXMEM=18G
"C:\Users\Ahmad Hamza\ghidra\ghidra_12.1.4_PUBLIC\support\analyzeHeadless.bat" "C:\Users\Ahmad Hamza\nioh3-engine\re\ghidra_proj" Nioh3 -import "C:\Users\Ahmad Hamza\nioh3-engine\re\Nioh3.dump.exe" -scriptPath "C:\Users\Ahmad Hamza\nioh3-engine\re\scripts" -preScript N3PreAnalysis.java -log "C:\Users\Ahmad Hamza\nioh3-engine\re\logs\import.log" -scriptlog "C:\Users\Ahmad Hamza\nioh3-engine\re\logs\import_script.log" > "C:\Users\Ahmad Hamza\nioh3-engine\re\logs\import_stdout.txt" 2>&1
echo EXIT %ERRORLEVEL% >> "C:\Users\Ahmad Hamza\nioh3-engine\re\logs\import_stdout.txt"
