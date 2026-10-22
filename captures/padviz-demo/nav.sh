#!/bin/bash
# usage: nav.sh SHOT "BTN BTN ..."  slow reliable pad taps (700 ms apart) then a screenshot
cd "C:/Users/Ahmad Hamza/nioh3-engine"
FF="$(cat /tmp/ff)"
for b in $2; do py -I tools/testmatrix/vpad.py press $b 120 >/dev/null; sleep 0.7; done
sleep 0.5; "$FF" -y -loglevel error -f gdigrab -framerate 5 -i desktop -frames:v 1 -vf scale=1280:720 captures/padviz-demo/$1.jpg
