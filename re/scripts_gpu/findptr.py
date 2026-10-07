"""findptr.py <rva> [...] -- find absolute 8-byte pointers (image base 0x7FF74F410000) to RVA in the dump;
prints the hit RVA and the surrounding 16 qwords as RVAs (useful for walking vtables)."""
import sys, struct
EXE = r"C:\Users\Ahmad Hamza\nioh3-engine\re\Nioh3.dump.exe"
BASE = 0x7FF74F410000
b = open(EXE, "rb").read()
for a in sys.argv[1:]:
    rva = int(a, 16)
    v = struct.pack("<Q", BASE + rva)
    i = b.find(v); n = 0
    print("== ptr to 0x%x" % rva)
    while i >= 0 and n < 20:
        row = []
        for k in range(-4, 12):
            q = struct.unpack_from("<Q", b, i + 8 * k)[0]
            row.append(("%x" % (q - BASE)) if BASE <= q < BASE + len(b) else "-")
        print("  at 0x%x: [-4..+11] %s" % (i, " ".join(row)))
        i = b.find(v, i + 1); n += 1
