r"""exe_dxbc.py <digest_hex> [...] -- extract DXBC containers embedded in re/Nioh3.dump.exe by container digest,
write them to re/scripts_gpu/out/exe_<digest16>.dxbc and print the RVA plus any pointer-table neighbours."""
import struct, sys, os
EXE = r"C:\Users\Ahmad Hamza\nioh3-engine\re\Nioh3.dump.exe"
OUT = r"C:\Users\Ahmad Hamza\nioh3-engine\re\scripts_gpu\out"
BASE = 0x7FF74F410000
b = open(EXE, "rb").read()
for h in sys.argv[1:]:
    i = b.find(b"DXBC" + bytes.fromhex(h))
    if i < 0:
        print(h, "not found"); continue
    size = struct.unpack_from("<I", b, i + 24)[0]
    fn = os.path.join(OUT, "exe_%s.dxbc" % h[:16])
    open(fn, "wb").write(b[i:i + size])
    refs = []
    p = struct.pack("<Q", BASE + i); j = b.find(p)
    while j >= 0 and len(refs) < 5:
        refs.append(hex(j)); j = b.find(p, j + 1)
    print("%s rva=0x%x size=%d ptr-refs=%s -> %s" % (h, i, size, refs, fn))
