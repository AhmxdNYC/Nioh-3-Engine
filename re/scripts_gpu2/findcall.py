r"""findcall.py <rva> [...] -- find E8/E9 rel32 call/jmp sites targeting RVA in re/Nioh3.dump.exe (brute force, .text only)."""
import sys, bisect, csv, struct
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
TEXT_END = 0x38e0000
a = np.frombuffer(b[:TEXT_END + 4], dtype=np.uint8)
rel = np.frombuffer(b[:TEXT_END + 4], dtype=np.uint8)
idx = np.arange(TEXT_END - 5)
r32 = (a[1:TEXT_END - 4].astype(np.int64) | (a[2:TEXT_END - 3].astype(np.int64) << 8) | (a[3:TEXT_END - 2].astype(np.int64) << 16) | (a[4:TEXT_END - 1].astype(np.int64) << 24))
r32 = np.where(r32 >= 2**31, r32 - 2**32, r32)
tgt = np.arange(len(r32)) + 5 + r32
op = a[:len(r32)]
for t in sys.argv[1:]:
    T = int(t, 16)
    hits = np.nonzero((tgt == T) & ((op == 0xE8) | (op == 0xE9)))[0]
    print("== target", t, len(hits))
    for h in hits[:80]:
        print("  0x%x %s fn %s" % (h, "call" if b[h] == 0xE8 else "jmp ", fn(int(h))))
