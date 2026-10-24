#!/bin/bash
# 60 s clip: Xbox Series body with presses, menu switch to PlayStation (DualSense body), presses again. Records mkv with -t (never killed).
cd "C:/Users/Ahmad Hamza/nioh3-engine"
FF="$(cat /tmp/ff)"
P(){ py -I tools/testmatrix/vpad.py "$@" >/dev/null; }
T(){ for b in $1; do P press $b 120; sleep 0.7; done; }
"$FF" -y -loglevel error -f gdigrab -framerate 30 -i desktop -t 64 -vf scale=1920:1080 -c:v libx264 -preset veryfast -crf 26 -pix_fmt yuv420p captures/padviz-demo/clip2.mkv </dev/null &
sleep 2
demo(){ for b in A B X Y LB RB; do P press $b 500; sleep .3; done; P press A+X 500; sleep .3
  P trigger L 1 600; sleep .2; P trigger R 1 600; sleep .2
  P press UP 400; P press DOWN 400; P press LEFT 400; P press RIGHT 400
  P stick L 1 0 500; P stick L 0 1 500; P stick L -1 0 500; P stick L 0 -1 500; P stick R 1 0 500; P stick R 0 -1 500; sleep .5; }
demo
P key F10 150; sleep 4
T "B"; sleep 1; T "A"; sleep 1.5
T "DOWN DOWN DOWN DOWN DOWN DOWN DOWN DOWN DOWN"; sleep 1; T "A"; sleep 1.5
T "DOWN DOWN"; sleep 1; T "A"; sleep 1.5
T "DOWN DOWN DOWN DOWN DOWN DOWN DOWN DOWN DOWN"; sleep 1; P press LEFT 120; sleep 1.5
P key F10 150; sleep 2
demo
wait
