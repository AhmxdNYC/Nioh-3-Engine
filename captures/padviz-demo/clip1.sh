source "C:/Users/Ahmad Hamza/nioh3-engine/captures/padviz-demo/lib.sh"
rec_start captures/padviz-demo/clip1.mp4
# Xbox buttons
for b in A B X Y LB RB; do P press $b 450; sleep 0.35; done
P press A+X 500; sleep .3
P trigger L 1 500; sleep .2; P trigger R 1 500; sleep .2
N UP; N DOWN; N LEFT; N RIGHT
P stick L 1 0 400; P stick L 0 1 400; P stick L -1 0 400; P stick L 0 -1 400
P stick R 1 0 400
sleep 1
# Practice menu -> Boss A -> hit groups
P key F10 150; sleep 2
N DOWN 3; sleep .5; N A; sleep 1; N A; sleep 2.5
N DOWN 6; sleep 1.5; N A; sleep 3; N B; sleep .7
N DOWN 2; sleep 1; N A; sleep 3; N B; sleep 1
# back to Practice root
N B 2; sleep 1
N DOWN 2; sleep 1; N RIGHT 12; sleep 2
N DOWN 4; sleep 1; N A; sleep 2
N B; N B; sleep 3
rec_stop
