"""idrk.py - read/write Nioh 3 shader_dx12 *.file containers and index the DXIL inside.

Container (all little-endian):
  0x00 char[8]  "IDRK0000"
  0x08 u64      file size
  0x10 u64      compressed payload size (file size - 0x38)
  0x18 u64      uncompressed size
  0x20 u32      0
  0x24 u32      resource hash (== file name 0x????????)
  0x28 u32      type hash (0x7bcd279f for shader groups)
  0x2c u32      0x00100000 (max chunk size? flags)
  0x30 u64      0
  0x38 ...      chunks: u32 zsize, zlib stream (zsize bytes) ... (decompress until uncompressed size)
Payload: "_S2G7000" shader group: header + technique name + table + several DXBC (DXIL) containers.

    py idrk.py index <shader_dx12 dir> <out.csv>     # every DXBC: file, offset, size, HASH digest, kind
    py idrk.py find  <shader_dx12 dir> <hex digest>  # locate a DXIL by its HASH-part digest
    py idrk.py unpack <file> <out.bin>
"""
import csv, os, struct, sys, zlib, hashlib

MAGIC = b"IDRK0000"


def unpack(b: bytes) -> bytes:
    assert b[:8] == MAGIC, "not IDRK"
    fsize, csize, usize = struct.unpack_from("<QQQ", b, 8)
    off, out = 0x38, bytearray()
    while len(out) < usize and off + 4 <= len(b):
        n = struct.unpack_from("<I", b, off)[0]; off += 4
        if n == 0:
            break
        out += zlib.decompress(b[off:off + n]); off += n
    assert len(out) == usize, (len(out), usize)
    return bytes(out)


def header(b: bytes):
    return struct.unpack_from("<8sQQQIIIIQ", b, 0)


def chunk_layout(b):
    """-> (list of chunk sizes, has_terminator) of an IDRK file."""
    off, sizes = 0x38, []
    while off + 4 <= len(b):
        n = struct.unpack_from("<I", b, off)[0]; off += 4
        if n == 0:
            return sizes, True
        sizes.append(n); off += n
    return sizes, False


def pack(payload: bytes, like: bytes, chunk=0x4000) -> bytes:
    """Re-compress `payload` into an IDRK container with the original's header fields
    (resource hash, type hash, flags); zlib level 6 ("78 9c" like the originals)."""
    h = header(like)
    _, term = chunk_layout(like)
    body = bytearray()
    for i in range(0, len(payload), chunk):
        z = zlib.compress(payload[i:i + chunk], 6)
        body += struct.pack("<I", len(z)) + z
    if term:
        body += bytes(4)
    return struct.pack("<8sQQQIIIIQ", MAGIC, 0x38 + len(body), len(body), len(payload),
                       h[4], h[5], h[6], h[7], h[8]) + bytes(body)


def dxbc_parts(blob: bytes, off: int):
    """Parse a DXBC container at off -> (size, {fourcc: (part_off, part_size)})."""
    magic, digest, ver, size, n = struct.unpack_from("<4s16sIII", blob, off)
    parts = {}
    for i in range(n):
        po = struct.unpack_from("<I", blob, off + 32 + 4 * i)[0]
        fcc, psz = struct.unpack_from("<4sI", blob, off + po)
        parts[fcc.decode("latin1")] = (off + po + 8, psz)
    return size, digest, parts


def iter_dxbc(payload: bytes):
    i = payload.find(b"DXBC")
    while i >= 0:
        try:
            size, digest, parts = dxbc_parts(payload, i)
            if 0 < size <= len(payload) - i:
                hp = parts.get("HASH")
                hdig = payload[hp[0] + 4:hp[0] + 20] if hp else b""
                # shader kind from DXIL program header (part DXIL: u32 version: kind in bits 16..31)
                dp = parts.get("DXIL")
                kind = None
                if dp:
                    pv = struct.unpack_from("<I", payload, dp[0])[0]
                    kind = ["ps", "vs", "gs", "hs", "ds", "cs", "lib", "raygen", "isect", "anyhit", "closesthit", "miss", "callable", "ms", "as", "?"][(pv >> 16) & 0xF] if ((pv >> 16) & 0xF) < 16 else pv
                yield i, size, digest, hdig, kind, sorted(parts)
                i = payload.find(b"DXBC", i + size)
                continue
        except struct.error:
            pass
        i = payload.find(b"DXBC", i + 4)


def group_name(payload: bytes) -> str:
    return payload[16:48].split(b"\0")[0].decode("latin1", "replace")


def main():
    cmd = sys.argv[1]
    if cmd == "unpack":
        open(sys.argv[3], "wb").write(unpack(open(sys.argv[2], "rb").read()))
    elif cmd in ("index", "find"):
        d = sys.argv[2]
        want = bytes.fromhex(sys.argv[3]) if cmd == "find" else None
        rows = []
        for n in sorted(os.listdir(d)):
            b = open(os.path.join(d, n), "rb").read()
            try:
                p = unpack(b)
            except Exception as e:
                print("ERR", n, e); continue
            gname = group_name(p)
            for off, size, dig, hdig, kind, parts in iter_dxbc(p):
                if want is not None:
                    if want in p[off:off + size] or hdig == want or dig == want:
                        print("HIT", n, gname, hex(off), size, kind, hdig.hex())
                else:
                    rows.append([n, gname, off, size, kind, hdig.hex(), dig.hex()])
        if cmd == "index":
            with open(sys.argv[3], "w", newline="") as f:
                w = csv.writer(f); w.writerow(["file", "group", "dxbc_off", "dxbc_size", "kind", "hash_digest", "container_digest"])
                w.writerows(rows)
            print(len(rows), "DXBC containers indexed")


if __name__ == "__main__":
    main()
