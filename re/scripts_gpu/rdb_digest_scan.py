r"""rdb_digest_scan.py <typename[,typename]> <digest_hex> [...]
Like g1s_find.py but scans system.rdb entries of the given RDB type names (e.g. BinaryFile,KTIDFileBinary)
for DXBC containers with the given container digests. Read-only on the game package."""
import os, sys, struct
sys.path.insert(0, r"C:\Users\Ahmad Hamza\nioh3-engine\data\tools")
import n3rdb
PKG = r"C:\Program Files (x86)\Steam\steamapps\common\Nioh3\package"
OUT = r"C:\Users\Ahmad Hamza\nioh3-engine\re\scripts_gpu\out"
tn = set(sys.argv[1].split(","))
want = {bytes.fromhex(h): h for h in sys.argv[2:]}
types = n3rdb.load_types()
tk = [k for k, v in types.items() if v[0] in tn]
found = set()
for w in ("system", "root"):
    ents = [e for e in n3rdb.read_rdb(PKG, w) if e["tktid"] in tk and e["loc"]]
    print(w, len(ents), "entries")
    for e in ents:
        try:
            d = n3rdb.get_data(PKG, e)
        except Exception as ex:
            print("  err", hex(e["ktid"]), ex); continue
        for dig, h in want.items():
            i = d.find(b"DXBC" + dig)
            if i >= 0:
                size = struct.unpack_from("<I", d, i + 24)[0]
                fn = os.path.join(OUT, "sys_%s.dxbc" % h[:16])
                open(fn, "wb").write(d[i:i + size])
                print("HIT %s %s ktid=%08x len=%d dxbc@0x%x size=%d" % (w, types[e["tktid"]][0], e["ktid"], len(d), i, size))
                found.add(h)
print("missing:", [h for h in want.values() if h not in found])
