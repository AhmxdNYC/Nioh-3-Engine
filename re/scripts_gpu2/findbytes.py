r"""findbytes.py <hexbytes> [...] -- find byte patterns in re/Nioh3.dump.exe; prints RVA and containing function."""
import sys, bisect, csv
EXE = r"C:\Users\Ahmad Hamza\nioh3-engine\re\Nioh3.dump.exe"
FT = r"C:\Users\Ahmad Hamza\nioh3-engine\re\out\functions.tsv"
b = open(EXE, "rb").read()
st, en = [], []
with open(FT, newline="") as f:
    rd = csv.reader(f, delimiter="\t"); next(rd)
    for r in rd:
        try: s = int(r[0], 16); n = int(r[1])
        except Exception: continue
        st.append(s); en.append(s + n)
o = sorted(range(len(st)), key=lambda i: st[i]); st = [st[i] for i in o]; en = [en[i] for i in o]
def fn(a):
    k = bisect.bisect_right(st, a) - 1
    return hex(st[k]) if k >= 0 and a < en[k] else "-"
for h in sys.argv[1:]:
    p = bytes.fromhex(h); i = b.find(p); n = 0
    print("==", h)
    while i >= 0 and n < 60:
        print("  0x%x fn %s" % (i, fn(i))); n += 1; i = b.find(p, i + 1)
