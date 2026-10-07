r"""findrip.py <rva> [...] -- find RIP-relative references (disp32 such that next_ip+disp == RVA, assuming the disp is the
last 4 bytes of the instruction, which is true for LEA/MOV without immediates) in .text of re/Nioh3.dump.exe."""
import sys, bisect, csv
import numpy as np
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
TEXT = 0x38e0000
a = np.frombuffer(b[:TEXT + 8], dtype=np.uint8).astype(np.int64)
d = a[0:TEXT] | (a[1:TEXT + 1] << 8) | (a[2:TEXT + 2] << 16) | (a[3:TEXT + 3] << 24)
d = np.where(d >= 2**31, d - 2**32, d)
tgt = np.arange(TEXT) + 4 + d
for t in sys.argv[1:]:
    T = int(t, 16); hits = np.nonzero(tgt == T)[0]
    print("== ", t, len(hits))
    for h in hits[:40]:
        h = int(h); print("  disp@0x%x  bytes %s  fn %s" % (h, b[h - 3:h + 4].hex(), fn(h)))
