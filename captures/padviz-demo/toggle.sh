#!/bin/bash
# opens the mod menu (lands on Practice), walks Settings > ini section > Mod menu controls, flips "Controller display", closes the menu.
# Saves a screenshot at the Mod menu controls page (toggle_*.jpg) so the state can be checked.
cd "C:/Users/Ahmad Hamza/nioh3-engine"
FF="$(cat /tmp/ff)"
T(){ for b in $1; do py -I tools/testmatrix/vpad.py press $b 120 >/dev/null; sleep 0.7; done; }
S(){ "$FF" -y -loglevel error -f gdigrab -framerate 5 -i desktop -frames:v 1 -vf scale=1280:720 captures/padviz-demo/$1.jpg; }
py -I tools/testmatrix/vpad.py key F10 150 >/dev/null; sleep 4
T "B"; sleep 1; T "A"; sleep 1.5; S tg_settings
T "DOWN DOWN DOWN DOWN DOWN DOWN DOWN DOWN DOWN"; sleep 1; T "A"; sleep 1.5; S tg_ini
T "DOWN DOWN"; sleep 1; T "A"; sleep 1.5
T "DOWN DOWN DOWN DOWN DOWN DOWN DOWN DOWN DOWN DOWN"; sleep 1; T "A"; sleep 1
S toggle_$(date +%H%M%S)
py -I tools/testmatrix/vpad.py key F10 150 >/dev/null; sleep 1
