"""Parse dump PE: sections + imports (IAT RVAs) of time-related functions."""
import struct, sys
DUMP = r"C:\Users\Ahmad Hamza\nioh3-engine\re\Nioh3.dump.exe"
img = open(DUMP, "rb").read()
pe = struct.unpack_from("<I", img, 0x3C)[0]
nsec = struct.unpack_from("<H", img, pe+6)[0]
optsz = struct.unpack_from("<H", img, pe+20)[0]
opt = pe+24
base = struct.unpack_from("<Q", img, opt+24)[0]
print("imagebase %x" % base)
for i in range(nsec):
    sh = opt+optsz+i*40
    name = img[sh:sh+8].rstrip(b"\0").decode()
    vs, va = struct.unpack_from("<II", img, sh+8)
    print(f"  {name:8} va={va:08x} vsize={vs:08x}")
dd = opt+112
imp_rva, imp_sz = struct.unpack_from("<II", img, dd+8)
print("import dir", hex(imp_rva), hex(imp_sz))
def cstr(o):
    e = img.index(b"\0", o); return img[o:e].decode(errors="replace")
o = imp_rva
want = sys.argv[1:] 
while True:
    oft, ts, fc, name, ft = struct.unpack_from("<IIIII", img, o)
    if name == 0: break
    dll = cstr(name)
    th = oft or ft
    i = 0
    while True:
        ent = struct.unpack_from("<Q", img, th+i*8)[0]
        if ent == 0: break
        if not ent & (1<<63) and ent < len(img):
            fn = cstr(ent+2)
        else:
            fn = f"ord{ent & 0xffff}"
        iat = ft + i*8
        if not want or any(w.lower() in fn.lower() for w in want):
            print(f"{dll:24} {fn:40} IAT={iat:#x}")
        i += 1
    o += 20
