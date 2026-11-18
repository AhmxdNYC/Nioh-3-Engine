# setsec.py INI SECTION KEY VALUE: set KEY inside [SECTION] only (insert after the header if missing). UTF-8, no BOM.
import sys, re
ini, sec, key, val = sys.argv[1:5]
L = open(ini, encoding="utf-8-sig").read().split("\n")
cur = None; hdr = None; done = False
for i, l in enumerate(L):
    m = re.match(r"\s*\[([^\]]+)\]", l)
    if m: cur = m.group(1); hdr = i if cur == sec and hdr is None else hdr; continue
    if cur == sec and re.match(r"\s*%s\s*=" % re.escape(key), l, re.I):
        L[i] = "%s = %s" % (key, val) + ("\r" if l.endswith("\r") else ""); done = True; break
if not done:
    if hdr is None: L += ["[%s]\r" % sec]; hdr = len(L) - 1
    L.insert(hdr + 1, "%s = %s\r" % (key, val))
open(ini, "w", encoding="utf-8", newline="").write("\n".join(L))
print("[%s] %s = %s (%s)" % (sec, key, val, "replaced" if done else "inserted"))
