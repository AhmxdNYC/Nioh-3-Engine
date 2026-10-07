r"""dll_digest_scan.py <digest> [...] -- look for DXBC containers by digest in the game's DLLs (read-only) and the dump.
Prints which binary contains each digest, plus the total count of DXBC containers per binary."""
import sys, os, re
G = r"C:\Program Files (x86)\Steam\steamapps\common\Nioh3"
bins = [os.path.join(G, f) for f in os.listdir(G) if f.lower().endswith((".dll"))]
bins.append(r"C:\Users\Ahmad Hamza\nioh3-engine\re\Nioh3.dump.exe")
want = [bytes.fromhex(h) for h in sys.argv[1:]]
for fn in bins:
    b = open(fn, "rb").read()
    n = b.count(b"DXBC")
    hits = []
    for w in want:
        i = b.find(b"DXBC" + w)
        if i >= 0: hits.append("%s@0x%x" % (w.hex()[:16], i))
    print("%-45s DXBC=%5d %s" % (os.path.basename(fn), n, " ".join(hits)))
