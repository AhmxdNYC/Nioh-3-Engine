#!/bin/bash
cd "C:/Users/Ahmad Hamza/nioh3-engine/Temp/padguard116769"
for a in A B; do
  if tasklist | grep -qi nioh3; then echo "$(date +%T) game running before $a - stop"; exit 1; fi
  (powershell -NoProfile -ExecutionPolicy Bypass -File "C:/Users/Ahmad Hamza/universal-optimizer/tools/steam/steam_autoplay.ps1" -Minutes 12 > autoplay-$a.out 2>&1 &)
  echo "$(date +%T) boot $a start"
  powershell -NoProfile -ExecutionPolicy Bypass -File pg.ps1 -Arm $a > boot$a.out 2>&1
  echo "$(date +%T) boot $a done: $(cat D:/nioh3-re/padguard-116769/b-$a/result.json 2>/dev/null | tr -d '\r\n' | cut -c1-900)"
done
