"""offdisk_scan.py - look for shader digests anywhere in the game folder (notes/gpu-post-exe.md section 1).

    py -I offdisk_scan.py "<Nioh3 folder>" 17149dcafa8138ed066f5fdd69835ce5,6a305be4...

Searches the raw bytes of every exe/dll in the folder, and the decoded payload of every shader_dx12\\*.file, for each
16-byte digest (container digest or DXIL HASH digest; anywhere, not only after "DXBC"). IDRK files are decoded by
scanning for complete zlib streams, which handles both chunk layouts (flags 0x1000: u32 size per chunk; flags 0x4000:
10-byte chunk headers, the 24 files dxil_index.csv skips).
"""
import os, struct, sys, zlib


def idrk_payload(b):
    usize = struct.unpack_from("<Q", b, 0x18)[0]
    off, out = 0x38, bytearray()
    while len(out) < usize:
        i = b.find(b"\x78\x9c", off)
        if i < 0:
            break
        d = zlib.decompressobj()
        try:
            r = d.decompress(b[i:])
        except zlib.error:
            off = i + 1
            continue
        if not d.eof:
            off = i + 1
            continue
        out += r
        off = len(b) - len(d.unused_data)
    return bytes(out) if len(out) == usize else None


def main():
    game, digests = sys.argv[1], [bytes.fromhex(h) for h in sys.argv[2].split(",")]
    found = set()
    for f in sorted(os.listdir(game)):
        p = os.path.join(game, f)
        if os.path.isfile(p) and f.lower().endswith((".exe", ".dll")):
            b = open(p, "rb").read()
            for w in digests:
                if w in b:
                    print(w.hex()[:8], f, hex(b.find(w)))
                    found.add(w)
    sd = os.path.join(game, "shader_dx12")
    bad = 0
    for f in sorted(os.listdir(sd)):
        b = open(os.path.join(sd, f), "rb").read()
        pl = idrk_payload(b) if b[:8] == b"IDRK0000" else None
        if pl is None:
            bad += 1
            continue
        for w in digests:
            if w in pl:
                print(w.hex()[:8], "shader_dx12/" + f, hex(pl.find(w)))
                found.add(w)
    for w in digests:
        if w not in found:
            print(w.hex()[:8], "NOT FOUND")
    print("undecodable shader files:", bad)


if __name__ == "__main__":
    main()
