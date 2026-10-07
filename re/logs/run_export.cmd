@echo off
set GHIDRA_HEADLESS_MAXMEM=8G
"C:\Users\Ahmad Hamza\ghidra\ghidra_12.1.4_PUBLIC\support\analyzeHeadless.bat" "C:\Users\Ahmad Hamza\nioh3-engine\re\ghidra_proj" Nioh3 -process Nioh3.dump.exe -readOnly -noanalysis -scriptPath "C:\Users\Ahmad Hamza\nioh3-engine\re\scripts" -postScript N3Export.java "C:\Users\Ahmad Hamza\nioh3-engine\re\out" > "C:\Users\Ahmad Hamza\nioh3-engine\re\logs\export_stdout.txt" 2>&1
