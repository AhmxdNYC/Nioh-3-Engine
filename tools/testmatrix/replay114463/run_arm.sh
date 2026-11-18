#!/bin/bash
cd "C:/Users/Ahmad Hamza/nioh3-engine/Temp/replay114463"
a=$1
if tasklist | grep -qi nioh3; then echo "$(date +%T) game running before $a - stop"; exit 1; fi
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/idle.ps1 > idle-$a.txt 2>&1
(powershell -NoProfile -ExecutionPolicy Bypass -File "C:/Users/Ahmad Hamza/universal-optimizer/tools/steam/steam_autoplay.ps1" -Minutes 12 > autoplay-$a.out 2>&1 &)
echo "$(date +%T) arm $a start"
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/rep.ps1 -OwnerPid 0 -Arm $a > arm$a.out 2>&1
echo "$(date +%T) arm $a done: $(cat D:/nioh3-re/replay-114414/a-$a/result.json 2>/dev/null | tr -d '\r\n ' | cut -c1-300)"
