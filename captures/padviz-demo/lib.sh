cd "C:/Users/Ahmad Hamza/nioh3-engine"
P(){ py -I tools/testmatrix/vpad.py "$@" >/dev/null; }
# nav: tap pad button n times with short gaps
N(){ local b=$1 n=${2:-1}; for ((i=0;i<n;i++)); do P press $b 90; sleep 0.25; done; }
FF="$(cat /tmp/ff)"
rec_start(){ "$FF" -y -loglevel error -f gdigrab -framerate 30 -i desktop -vf scale=1280:720 -c:v libx264 -preset veryfast -crf 28 -pix_fmt yuv420p "$1" </dev/null & RECPID=$!; sleep 2; }
rec_stop(){ sleep 1; kill -INT $RECPID 2>/dev/null; sleep 2; }
