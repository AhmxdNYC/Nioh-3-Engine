r"""g1s_find.py <digest_hex> [...]
Scan every G1SFile entry in package\system.rdb and root.rdb (read-only) for DXBC containers whose
container digest (bytes 4..20 after 'DXBC') equals one of the given digests (pso_map 'stage:<digest>').
Prints rdb, ktid, size, offset of the DXBC and any ASCII names found near the start of the entry,
and writes the DXBC to re/scripts_gpu/out/g1s_<digest>.dxbc.
"""
import os, sys, struct
sys.path.insert(0, r"C:\Users\Ahmad Hamza\nioh3-engine\data\tools")
import n3rdb
PKG = r"C:\Program Files (x86)\Steam\steamapps\common\Nioh3\package"
OUT = r"C:\Users\Ahmad Hamza\nioh3-engine\re\scripts_gpu\out"
want = {bytes.fromhex(h): h for h in sys.argv[1:]}
types = n3rdb.load_types()
g1s = [k for k, v in types.items() if v[0] == "G1SFile"]
print("G1SFile type ktids:", [hex(x) for x in g1s])
found = set()
for w in ("system", "root"):
    ents = n3rdb.read_rdb(PKG, w)
    sel = [e for e in ents if e["tktid"] in g1s and e["loc"]]
    print(w, len(sel), "G1S entries")
    for e in sel:
        try:
            d = n3rdb.get_data(PKG, e)
        except Exception as ex:
            continue
        i = d.find(b"DXBC")
        while i >= 0:
            dig = d[i + 4:i + 20]
            if dig in want:
                size = struct.unpack_from("<I", d, i + 24)[0]
                h = want[dig]
                fn = os.path.join(OUT, "g1s_%s.dxbc" % h[:16])
                open(fn, "wb").write(d[i:i + size])
                head = d[:256]
                txt = "".join(chr(c) if 32 <= c < 127 else "." for c in head)
                print("HIT %s ktid=%08x len=%d dxbc@0x%x size=%d -> %s\n    head: %s" % (w, e["ktid"], len(d), i, size, fn, txt[:200]))
                found.add(h)
            i = d.find(b"DXBC", i + 4)
        if len(found) == len(want):
            break
    if len(found) == len(want):
        break
print("missing:", [h for h in want.values() if h not in found])
