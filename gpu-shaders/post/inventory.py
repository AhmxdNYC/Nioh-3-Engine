r"""inventory.py -- every DXBC container embedded in re/Nioh3.dump.exe (read only).

For each container: file offset / RVA, size, container digest (the 16 bytes after 'DXBC', what RGP calls the dxbc
digest), the psoswap key (the digest; FNV-1a 64 of the container when the digest is zero, see src/psoswap.cpp), the part list, the program kind
(SM4/5 'SHEX'/'SHDR' or SM6 'DXIL') and stage, plus the r5 RGP time of every PSO that uses it.

Writes gpu-shaders/post/out/inventory.csv and prints the post-chain subset. Usage: py -I inventory.py [--extract]
(--extract also writes every container to out/blobs/<digest16>.dxbc).
"""
import csv, collections, os, struct, sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
EXE = os.path.join(ROOT, "re", "Nioh3.dump.exe")
OUT = os.path.join(ROOT, "gpu-shaders", "post", "out")
PSOMAP = os.path.join(ROOT, "re", "scripts_gpu", "out", "rgp_top300", "Nioh3_2026-10-06_19-39-58_shaders", "pso_map.csv")

STAGES = {0: "ps", 1: "vs", 2: "gs", 3: "hs", 4: "ds", 5: "cs", 0xFFFE: "fx"}


def fnv1a64(b):
    h = 0xcbf29ce484222325
    for x in b:
        h ^= x
        h = (h * 0x100000001b3) & 0xFFFFFFFFFFFFFFFF
    return h


def sections(b):
    pe = struct.unpack_from("<I", b, 0x3C)[0]
    nsec = struct.unpack_from("<H", b, pe + 6)[0]
    opt = struct.unpack_from("<H", b, pe + 20)[0]
    base = pe + 24 + opt
    out = []
    for i in range(nsec):
        o = base + 40 * i
        name = b[o:o + 8].rstrip(b"\0").decode("latin1")
        vsz, va, rsz, rptr = struct.unpack_from("<IIII", b, o + 8)
        out.append((name, va, vsz, rptr, rsz))
    return out


def off2rva(secs, off):
    for name, va, vsz, rptr, rsz in secs:
        if rptr <= off < rptr + rsz:
            return va + off - rptr, name
    return None, "?"


def parse(b, i):
    size = struct.unpack_from("<I", b, i + 24)[0]
    nparts = struct.unpack_from("<I", b, i + 28)[0]
    if not (32 <= size <= 16 << 20) or not (1 <= nparts <= 32) or i + size > len(b):
        return None
    parts = []
    kind, stage, ver = "?", "?", ""
    for p in range(nparts):
        po = struct.unpack_from("<I", b, i + 32 + 4 * p)[0]
        if po + 8 > size:
            return None
        fourcc = b[i + po:i + po + 4].decode("latin1")
        psz = struct.unpack_from("<I", b, i + po + 4)[0]
        parts.append((fourcc, psz))
        if fourcc in ("SHEX", "SHDR"):
            vt = struct.unpack_from("<I", b, i + po + 8)[0]
            kind = "sm5" if fourcc == "SHEX" else "sm4"
            stage = STAGES.get(vt >> 16, "?")
            ver = "%d_%d" % ((vt >> 4) & 0xF, vt & 0xF)
        elif fourcc == "DXIL":
            vt = struct.unpack_from("<I", b, i + po + 8)[0]
            kind = "dxil"
            stage = STAGES.get(vt >> 16, "?")
            ver = "%d_%d" % ((vt >> 4) & 0xF, vt & 0xF)
    return size, parts, kind, stage, ver


def r5_times():
    t = collections.defaultdict(float)
    segs = collections.defaultdict(set)
    if not os.path.exists(PSOMAP):
        return t, segs
    for r in csv.DictReader(open(PSOMAP)):
        sh = r["shaders(stage:dxbc_digest=file/group#index)"]
        for tok in sh.split():
            st, _, rest = tok.partition(":")
            dig = rest.split("=")[0]
            t[(dig, r["api_pso_hash"])] = float(r["seg_dur_us"])
            segs[dig].add((r["seg"], r["api_pso_hash"]))
    return t, segs


def main():
    b = open(EXE, "rb").read()
    secs = sections(b)
    os.makedirs(OUT, exist_ok=True)
    extract = "--extract" in sys.argv
    if extract:
        os.makedirs(os.path.join(OUT, "blobs"), exist_ok=True)
    times, segs = r5_times()
    rows = []
    i = b.find(b"DXBC")
    while i >= 0:
        r = parse(b, i)
        if r:
            size, parts, kind, stage, ver = r
            blob = b[i:i + size]
            dig = blob[4:20].hex()
            rva, sec = off2rva(secs, i)
            used = segs.get(dig, set())
            us = sum(times[(dig, p)] for s, p in used)
            rows.append(dict(offset="0x%x" % i, rva="0x%x" % rva if rva is not None else "", section=sec, size=size,
                             digest=dig, key=(dig if dig.strip("0") else "f%016x" % fnv1a64(blob)), kind=kind, stage=stage, ver=ver,
                             parts=" ".join("%s:%d" % p for p in parts), r5_psos=len({p for s, p in used}),
                             r5_us="%.1f" % us))
            if extract:
                open(os.path.join(OUT, "blobs", dig[:16] + ".dxbc"), "wb").write(blob)
            i = b.find(b"DXBC", i + size)
        else:
            i = b.find(b"DXBC", i + 4)
    with open(os.path.join(OUT, "inventory.csv"), "w", newline="") as f:
        w = csv.DictWriter(f, fieldnames=list(rows[0].keys()))
        w.writeheader()
        w.writerows(rows)
    c = collections.Counter((r["kind"], r["stage"]) for r in rows)
    print(len(rows), "containers;", dict(c))
    for r in sorted(rows, key=lambda r: -float(r["r5_us"])):
        if float(r["r5_us"]) > 0:
            print("%8s %-6s %-4s %s %s %6d B key=%s psos=%d" % (r["r5_us"], r["kind"], r["stage"], r["digest"][:16], r["rva"],
                                                                r["size"], r["key"], r["r5_psos"]))


if __name__ == "__main__":
    main()
