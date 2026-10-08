#!/usr/bin/env python3
"""Nioh 3 save differ (stdlib only, offline, read-only).  See notes/save-format.md.

Works on COPIES of SAVEDATA.BIN (or an already-decrypted *.dec, detected by the RNNUSR magic,
which skips the ~27 s/file decrypt).  Body = flat {u32 hash, u32 size, data} stream.

  savediff.py diff   A B [--noise FILE] [--all] [--max-bytes N] [--max-ranges N] [--hash H]
  savediff.py values HASH S1 S2 [S3 ...]      bytes of one record across N saves
  savediff.py noise  A B [-o FILE]            write hashes that changed (run on two saves taken
                                              seconds apart on the SAME timeline -> noise list)
Records are keyed (hash, occurrence#) so repeated hashes diff correctly.
Heuristic noise (hidden unless --all): changed records whose 4/8-byte LE value looks like a unix
time (1.4e9..2.1e9), and the header (time/saveCount/random keys) is never diffed.
"""
import argparse, os, struct, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import savecrypt as sc


def load(path):
    b = open(path, "rb").read()
    return b if b[:6] in (b"RNNUSR", b"RNNSYS") else sc.decrypt(b)


def records(p):
    """dict (hash, occ) -> (pos, bytes), in file order."""
    out, seen = {}, {}
    for pos, h, n in sc.scan(p, limit=10**9):
        k = (h, seen.get(h, 0)); seen[h] = k[1] + 1
        out[k] = (pos, p[pos + 8:pos + 8 + n])
    return out


def looks_time(a, b):
    if len(a) != len(b) or len(a) not in (4, 8):
        return False
    f = lambda d: int.from_bytes(d, "little")
    return all(1.4e9 <= f(x) <= 2.1e9 for x in (a, b))


def ranges(a, b):
    r, i, n = [], 0, min(len(a), len(b))
    while i < n:
        if a[i] != b[i]:
            j = i
            while j < n and a[j] != b[j]:
                j += 1
            r.append((i, j)); i = j
        else:
            i += 1
    return r


def kname(k):
    return "%08x%s" % (k[0], "" if k[1] == 0 else "#%d" % k[1])


def load_noise(f):
    s = set()
    if f:
        for ln in open(f):
            ln = ln.split("#")[0].strip()
            if ln:
                s.add(int(ln, 16))
    return s


def changed_keys(ra, rb):
    return [k for k in rb if k in ra and ra[k][1] != rb[k][1]]


def cmd_diff(a):
    ra, rb = records(load(a.A)), records(load(a.B))
    noise = load_noise(a.noise)
    add = [k for k in rb if k not in ra]
    rem = [k for k in ra if k not in rb]
    ch = changed_keys(ra, rb)
    hidden = 0
    lines = []
    for k in ch:
        if a.hash and k[0] != int(a.hash, 16):
            continue
        da, db = ra[k][1], rb[k][1]
        if not a.all and (k[0] in noise or looks_time(da, db)):
            hidden += 1; continue
        if len(da) != len(db):
            lines.append("CHG %s size %d -> %d  (pos 0x%x)" % (kname(k), len(da), len(db), rb[k][0])); continue
        if len(da) <= a.max_bytes:
            lines.append("CHG %s [%d B] %s -> %s" % (kname(k), len(da), da.hex(), db.hex()))
        else:
            rg = ranges(da, db)
            s = ", ".join("%d-%d" % x for x in rg[:a.max_ranges]) + (" ..." if len(rg) > a.max_ranges else "")
            lines.append("CHG %s [%d B] %d ranges, %d bytes differ: %s" % (kname(k), len(da), len(rg), sum(y - x for x, y in rg), s))
    for k in add:
        if not a.hash or k[0] == int(a.hash, 16):
            d = rb[k][1]
            lines.append("ADD %s [%d B] %s" % (kname(k), len(d), d[:a.max_bytes].hex() + ("..." if len(d) > a.max_bytes else "")))
    for k in rem:
        if not a.hash or k[0] == int(a.hash, 16):
            d = ra[k][1]
            lines.append("DEL %s [%d B] %s" % (kname(k), len(d), d[:a.max_bytes].hex() + ("..." if len(d) > a.max_bytes else "")))
    print("\n".join(lines))
    print("# records A=%d B=%d  added=%d removed=%d changed=%d (hidden as noise: %d)" %
          (len(ra), len(rb), len(add), len(rem), len(ch), hidden))


def cmd_values(a):
    h = int(a.HASH, 16)
    for i, s in enumerate(a.saves):
        r = records(load(s))
        hits = [(k, v) for k, v in r.items() if k[0] == h]
        if not hits:
            print("[%d] %s: (absent)" % (i, os.path.basename(s)))
        for k, (pos, d) in hits:
            print("[%d] %s %s @0x%x [%d B] %s" % (i, os.path.basename(s), kname(k), pos, len(d), d.hex()))


def cmd_noise(a):
    ra, rb = records(load(a.A)), records(load(a.B))
    ks = sorted({k[0] for k in changed_keys(ra, rb)} | {k[0] for k in rb if k not in ra} | {k[0] for k in ra if k not in rb})
    txt = "".join("%08x\n" % h for h in ks)
    if a.o:
        open(a.o, "w").write("# noise hashes (changed between two same-timeline saves)\n" + txt)
    print("%d noisy hashes" % len(ks)); 
    if not a.o: print(txt, end="")


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sp = ap.add_subparsers(dest="cmd", required=True)
    d = sp.add_parser("diff"); d.add_argument("A"); d.add_argument("B")
    d.add_argument("--noise"); d.add_argument("--all", action="store_true"); d.add_argument("--hash")
    d.add_argument("--max-bytes", type=int, default=32); d.add_argument("--max-ranges", type=int, default=8)
    v = sp.add_parser("values"); v.add_argument("HASH"); v.add_argument("saves", nargs="+")
    n = sp.add_parser("noise"); n.add_argument("A"); n.add_argument("B"); n.add_argument("-o")
    a = ap.parse_args()
    {"diff": cmd_diff, "values": cmd_values, "noise": cmd_noise}[a.cmd](a)


if __name__ == "__main__":
    main()
