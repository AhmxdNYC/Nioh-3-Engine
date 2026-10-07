"""extract_dxbc.py <shader_dx12 file> <outdir> [index ...]
Unpacks an IDRK shader group and writes DXBC #index (order of appearance, as in pso_map '#N') to
<outdir>/<group>_<N>.dxbc. With no index, lists all DXBCs (index, offset, size, kind, hash digest).
"""
import os, sys
sys.path.insert(0, r"C:\Users\Ahmad Hamza\nioh3-engine\gpu-shaders")
import idrk

src, out = sys.argv[1], sys.argv[2]
want = [int(x) for x in sys.argv[3:]]
p = idrk.unpack(open(src, "rb").read())
g = idrk.group_name(p)
os.makedirs(out, exist_ok=True)
items = list(idrk.iter_dxbc(p))
print(g, len(items), "DXBC")
for i, (off, size, dig, hdig, kind, parts) in enumerate(items):
    if not want:
        print(i, hex(off), size, kind, hdig.hex(), dig.hex())
    elif i in want:
        fn = os.path.join(out, "%s_%d.dxbc" % (g.split("-")[0], i))
        open(fn, "wb").write(p[off:off + size])
        print("wrote", fn, size, kind, hdig.hex())
