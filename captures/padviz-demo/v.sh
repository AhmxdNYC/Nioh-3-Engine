#!/bin/bash
# usage: v.sh shotname cmd; cmd args are vpad commands separated by ';'
cd "C:/Users/Ahmad Hamza/nioh3-engine"
FF="$(cat /tmp/ff)"
IFS=';' read -ra C <<< "$1"
for c in "${C[@]}"; do py -I tools/testmatrix/vpad.py $c >/dev/null; sleep 0.3; done
sleep 0.8; [ -n "$2" ] && "$FF" -y -loglevel error -f gdigrab -framerate 5 -i desktop -frames:v 1 -vf scale=1280:720 captures/padviz-demo/$2.jpg
