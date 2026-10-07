r"""qdump.py <rva> [n=16] -- dump n qwords at RVA of re/Nioh3.dump.exe, printing image pointers as RVAs."""
import struct, sys
EXE = r"C:\Users\Ahmad Hamza\nioh3-engine\re\Nioh3.dump.exe"
BASE = 0x7FF74F410000
b = open(EXE, "rb").read()
rva = int(sys.argv[1], 16); n = int(sys.argv[2]) if len(sys.argv) > 2 else 16
for k in range(n):
    q = struct.unpack_from("<Q", b, rva + 8 * k)[0]
    s = ("rva 0x%x" % (q - BASE)) if BASE <= q < BASE + len(b) else hex(q)
    print("+0x%03x  %s" % (8 * k, s))
