r"""libslot.py <slot_hex> [...] -- find 'mov R,[X+0x2b0]' (post-effect shader library load) followed within 160 bytes by
'mov Q,[R+slot]'. Heuristic x64 decode of REX.W 8B /r with mod=10 disp32 (no SIB). Prints site and function."""
import sys, struct, bisect, csv, re
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
def movs(lo, hi, disp):
    d = struct.pack("<I", disp); out = []
    i = b.find(d, lo)
    while 0 <= i < hi:
        for k, sib in ((1, None), (2, 0x24)):
            m = b[i - k]; op = b[i - k - 1]; rex = b[i - k - 2]
            if sib is not None and b[i - 1] != 0x24: continue
            if op == 0x8B and (m >> 6) == 2 and ((m & 7) == 4) == (sib is not None) and 0x48 <= rex <= 0x4F:
                reg = ((m >> 3) & 7) | ((rex & 4) << 1); base = (m & 7) | ((rex & 1) << 3)
                out.append((i - k - 2, reg, base))
        i = b.find(d, i + 1)
    return out
TEXT = 0x38e0000
libs = movs(0x1000, TEXT, 0x2b0)
for s in sys.argv[1:]:
    slot = int(s, 16); print("== slot", s)
    seen = set()
    for (a, reg, base) in libs:
        for (a2, r2, b2) in movs(a + 7, a + 600, slot):
            if b2 == reg and a2 not in seen:
                seen.add(a2); print("  lib@0x%x -> slot@0x%x  fn %s" % (a, a2, fn(a2)))
