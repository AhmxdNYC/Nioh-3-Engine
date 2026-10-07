"""findimm.py <u32hex> [...]  -- find little-endian u32 occurrences in re/Nioh3.dump.exe (raw offset == RVA).
Prints RVA and, when the hit is inside a function from re/out/functions.tsv, that function's start RVA."""
import sys, struct, bisect, csv
EXE = r"C:\Users\Ahmad Hamza\nioh3-engine\re\Nioh3.dump.exe"
FT = r"C:\Users\Ahmad Hamza\nioh3-engine\re\out\functions.tsv"
b = open(EXE, "rb").read()
starts, ends = [], []
try:
    with open(FT, newline="") as f:
        rd = csv.reader(f, delimiter="\t")
        hdr = next(rd)
        for r in rd:
            try:
                s = int(r[0], 16); n = int(r[1]) if r[1].isdigit() else int(r[1], 16)
            except Exception:
                continue
            starts.append(s); ends.append(s + n)
    o = sorted(range(len(starts)), key=lambda i: starts[i])
    starts = [starts[i] for i in o]; ends = [ends[i] for i in o]
except FileNotFoundError:
    pass
for h in sys.argv[1:]:
    v = struct.pack("<I", int(h, 16))
    i = b.find(v); hits = []
    while i >= 0:
        hits.append(i); i = b.find(v, i + 1)
    print("== %s: %d hits" % (h, len(hits)))
    for x in hits[:40]:
        k = bisect.bisect_right(starts, x) - 1
        fn = ("fn 0x%x" % starts[k]) if k >= 0 and x < ends[k] else ""
        print("  0x%x %s  ctx: %s" % (x, fn, b[x - 4:x + 8].hex()))
