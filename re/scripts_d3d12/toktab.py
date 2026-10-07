r"""toktab.py -- dump the KTGL token handler table (3C02220) of re/Nioh3.dump.exe.
usage: py -I toktab.py [tableRva=3C02220] [max=512]
Prints token id, handler RVA, and how many tokens share that handler."""
import struct, sys, collections
EXE = r"C:\Users\Ahmad Hamza\nioh3-engine\re\Nioh3.dump.exe"
BASE = 0x7FF74F410000
b = open(EXE, "rb").read()
tab = int(sys.argv[1], 16) if len(sys.argv) > 1 else 0x3C02220
mx = int(sys.argv[2]) if len(sys.argv) > 2 else 512
rows = []
for k in range(mx):
    q = struct.unpack_from("<Q", b, tab + 8 * k)[0]
    if not (BASE <= q < BASE + 0x3800000):
        break
    rows.append((k, q - BASE))
cnt = collections.Counter(r for _, r in rows)
for k, r in rows:
    print("%3d 0x%02X  %X  %s" % (k, k, r, "(shared x%d)" % cnt[r] if cnt[r] > 1 else ""))
print("entries:", len(rows))
