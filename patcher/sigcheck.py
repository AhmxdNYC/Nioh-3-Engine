"""Verify the memory dump: .text entropy + where each Nioh3Fix AOB lands (RVA, hit count)."""
import math, re, sys
from collections import Counter

DUMP = r"C:\Users\Ahmad Hamza\nioh3-engine\re\Nioh3.dump.exe"
NOTES = r"C:\Users\Ahmad Hamza\nioh3-engine\notes\nioh3fix-signatures.md"
TEXT_LO, TEXT_HI = 0x1000, 0x1000 + 0x038E028B

img = open(DUMP, "rb").read()
sample = img[0x1000000:0x1100000]
c = Counter(sample)
ent = -sum(n / len(sample) * math.log2(n / len(sample)) for n in c.values())
print(f".text entropy {ent:.3f} (decrypted code ~6-6.5)")

def to_regex(pat):
    return re.compile(b"".join(b"." if t.startswith("?") else re.escape(bytes([int(t, 16)]))
                               for t in pat.split()), re.S)

text = img[TEXT_LO:TEXT_HI]
rows = re.findall(r"^\| (\d+) \| ([^|]+) \| `([^`]+)` \|", open(NOTES, encoding="utf-8").read(), re.M)
for num, label, pat in rows:
    hits = [m.start() + TEXT_LO for m in to_regex(pat).finditer(text)]
    shown = ", ".join(f"{h:#x}" for h in hits[:3])
    print(f"#{num:>2} {label.strip()[:42]:<42} hits={len(hits):<3} {shown}")
