"""xref.py <rvaHex>... : branch xrefs into each target in the Nioh3 dump (raw off == RVA).
Finds CALL/JMP rel32 (E8/E9), Jcc rel32 (0F 8x) anywhere in executable sections, and short JMP/Jcc rel8
(EB/7x) within +-128 bytes (short hits are byte-pattern matches: verify with dis.fsx)."""
import sys, struct, re
img = open(r"C:\Users\Ahmad Hamza\nioh3-engine\re\Nioh3.dump.exe", "rb").read()
pe = struct.unpack_from("<I", img, 0x3C)[0]
nsec = struct.unpack_from("<H", img, pe+6)[0]; optsz = struct.unpack_from("<H", img, pe+20)[0]
secs = []
for i in range(nsec):
    sh = pe+24+optsz+i*40
    vs, va = struct.unpack_from("<II", img, sh+8); ch = struct.unpack_from("<I", img, sh+36)[0]
    if ch & 0x20000000: secs.append((va, vs))
JCC = re.compile(b"\x0f[\x80-\x8f]")
for t in [int(a, 16) for a in sys.argv[1:]]:
    hits = []
    for va, vs in secs:
        sec = img[va:va+vs]
        for op, kind in ((b"\xe8", "call"), (b"\xe9", "jmp")):
            for m in re.finditer(re.escape(op), sec):
                o = va + m.start()
                if o + 5 <= va + vs and o + 5 + struct.unpack_from("<i", img, o+1)[0] == t:
                    hits.append((kind, o))
        for m in JCC.finditer(sec):
            o = va + m.start()
            if o + 6 <= va + vs and o + 6 + struct.unpack_from("<i", img, o+2)[0] == t:
                hits.append(("jcc", o))
        if va <= t < va + vs:
            for o in range(max(va, t - 130), min(va + vs - 1, t + 128)):
                if (img[o] == 0xEB or 0x70 <= img[o] <= 0x7F) and o + 2 + struct.unpack_from("<b", img, o+1)[0] == t:
                    hits.append(("short?", o))
    print(f"{t:X}: {len(hits)} xrefs: " + " ".join(f"{k}@{o:X}" for k, o in sorted(hits, key=lambda x: x[1])[:40]))
