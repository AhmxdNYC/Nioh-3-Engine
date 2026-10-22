import sys
L=[]
def p(b,ms=90,gap=250): L.append(f"press {b} {ms}"); L.append(f"wait {gap}")
def n(b,k=1,gap=220):
    for _ in range(k): p(b,90,gap)
def w(ms): L.append(f"wait {ms}")
def c(x): L.append(x)
clip=sys.argv[1]
if clip=="1":
    w(500)
    for b in "A B X Y LB RB".split(): p(b,450,300)
    p("A+X",500,300)
    c("trigger L 1 500"); w(250); c("trigger R 1 500"); w(250)
    for b in "UP DOWN LEFT RIGHT".split(): p(b,350,200)
    for xy in ("1 0","0 1","-1 0","0 -1"): c(f"stick L {xy} 400")
    c("stick R 1 0 400"); w(800)
    c("key F10 150"); w(1800)
    n("DOWN",3); w(500); n("A"); w(1000); n("A"); w(2200)
    n("DOWN",6); w(1500); n("A"); w(3000); n("B"); w(600)
    n("DOWN",2); w(900); n("A"); w(3000); n("B"); w(800)
    n("B",2); w(800)
    n("DOWN",2); w(900); n("RIGHT",12,300); w(2500)
    n("DOWN",4); w(900); n("A"); w(2000)
    n("B"); w(1500)
print("\n".join(L))
