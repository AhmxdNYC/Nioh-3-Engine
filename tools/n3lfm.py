#!/usr/bin/env python3
"""n3lfm.py - read Nioh 3 LFM data straight from the game folder (stdlib only, read-only).

The game's logic data (mission records, enemy/placement tables, scripts, text) lives in the LFM ("layered file manager")
namespace: archive/archive_NN.bin bundles (+ lfm_order_NN.bin = each entry's path '/XX/<62 hex>') and loose files
archive/XX/<62 hex> (same namespace). Formats (notes/game-data.md, re-checked 2026-10-22 on the Linux install):
  archive_NN.bin  'LFMB', u32 0, u32 count, u32 0, u64 file size, u64 align; count x 0x20 {u64 offset, u64 usize, u64 csize,
                  u32 compressed, u32 0}. Compressed data = chunks of <= 16 KB output, each {u32 hdr (0x8000 | csize), zlib stream},
                  chunk starts 16-byte aligned.
  lfm_order_NN    'LFMO', u32 1, u32 count, u32, u32 table offset (0x28); count x 12 {u32 0, u32 entry index, u32 string offset}.
Every archive_*.bin present is indexed (future DLC bundles included); a loose file overrides a bundled entry of the same path.

    py -I tools/n3lfm.py list  <game dir>              # path, archive, size
    py -I tools/n3lfm.py get   <game dir> /52/BD29...  out.bin
"""
import os
import struct
import sys
import zlib


def _inflate(a, off, usize, csize):
    out = bytearray()
    p, end = off, off + csize
    while len(out) < usize and p + 4 <= end:
        hdr = struct.unpack_from("<I", a, p)[0]
        if a[p + 4] == 0x78:
            d = zlib.decompressobj()
            out += d.decompress(a[p + 4:end])
            used = (end - (p + 4)) - len(d.unused_data)
            p += 4 + used
        else:  # stored chunk (not seen yet): hdr = raw size
            n = hdr & 0x7FFF or min(0x4000, usize - len(out))
            out += a[p + 4:p + 4 + n]
            p += 4 + n
        p = (p + 15) & ~15
    if len(out) != usize:
        raise ValueError("LFM entry at 0x%X: inflated %d of %d bytes" % (off, len(out), usize))
    return bytes(out)


class Lfm:
    """path ('/52/BD29...', upper-case hex) -> bytes, over every archive_*.bin + loose file of <game>/archive."""

    def __init__(self, game):
        self.root = os.path.join(game, "archive") if os.path.isdir(os.path.join(game, "archive")) else game
        self.index = {}   # path -> (archive file, offset, usize, csize, compressed) or (loose path,)
        self._blobs = {}
        names = sorted(f for f in os.listdir(self.root) if f.startswith("archive_") and f.endswith(".bin"))
        if not names:
            raise FileNotFoundError("no archive_*.bin in %s (pass the Nioh3 game folder)" % self.root)
        for f in names:
            nn = f[len("archive_"):-len(".bin")]
            ap, op = os.path.join(self.root, f), os.path.join(self.root, "lfm_order_%s.bin" % nn)
            if not os.path.exists(op):
                continue
            with open(ap, "rb") as fh:
                head = fh.read(0x20)
                if head[:4] != b"LFMB":
                    continue
                cnt = struct.unpack_from("<I", head, 8)[0]
                tab = fh.read(cnt * 0x20)
            o = open(op, "rb").read()
            if o[:4] != b"LFMO":
                continue
            ocnt, toff = struct.unpack_from("<I", o, 8)[0], struct.unpack_from("<I", o, 0x10)[0]
            for k in range(ocnt):
                _z, idx, so = struct.unpack_from("<III", o, toff + 12 * k)
                if idx >= cnt or so >= len(o):
                    continue
                path = o[so:o.index(b"\0", so)].decode("latin1").upper()
                eo, us, cs, cp = struct.unpack_from("<QQQI", tab, 0x20 * idx)
                self.index[path] = (ap, eo, us, cs, cp)
        for d in os.listdir(self.root):
            dp = os.path.join(self.root, d)
            if len(d) == 2 and os.path.isdir(dp):
                for f in os.listdir(dp):
                    self.index["/%s/%s" % (d.upper(), f.upper())] = (os.path.join(dp, f),)

    def _blob(self, ap):
        b = self._blobs.get(ap)
        if b is None:
            b = self._blobs[ap] = open(ap, "rb").read()
        return b

    def get(self, path):
        e = self.index.get(path.upper())
        if e is None:
            return None
        if len(e) == 1:
            return open(e[0], "rb").read()
        ap, eo, us, cs, cp = e
        a = self._blob(ap)
        return _inflate(a, eo, us, cs) if cp else bytes(a[eo:eo + us])

    def size(self, path):
        e = self.index.get(path.upper())
        return None if e is None else (os.path.getsize(e[0]) if len(e) == 1 else e[2])

    def source(self, path):
        e = self.index.get(path.upper())
        return None if e is None else os.path.basename(e[0]) if len(e) > 1 else "loose"

    def fingerprint(self):
        """size + mtime of every archive_*.bin / lfm_order_*.bin: changes when a patch or DLC replaces the data."""
        h = 0
        for f in sorted(os.listdir(self.root)):
            if (f.startswith("archive_") or f.startswith("lfm_order_")) and f.endswith(".bin"):
                st = os.stat(os.path.join(self.root, f))
                h = zlib.crc32(("%s:%d:%d;" % (f, st.st_size, int(st.st_mtime))).encode(), h)
        return "%08X" % h


def main(argv):
    if len(argv) < 3:
        print(__doc__)
        return 2
    L = Lfm(argv[2])
    if argv[1] == "list":
        for p in sorted(L.index):
            print("%s\t%s\t%s" % (p, L.source(p), L.size(p)))
        return 0
    if argv[1] == "get":
        d = L.get(argv[3])
        if d is None:
            print("not found:", argv[3])
            return 1
        open(argv[4], "wb").write(d)
        print(len(d), "bytes ->", argv[4])
        return 0
    print(__doc__)
    return 2


if __name__ == "__main__":
    sys.exit(main(sys.argv))
