r"""s2g.py - read and re-pack Nioh 3 shader_dx12 "_S2G7000" shader groups record by record.

A group payload (idrk.unpack) holds a record table at u32 [0x98] (entries: u32 offset, u32 size, relative to the blob
base u32 [0xB0]; u32 [0xB4] = blob bytes, u32 [0x08] = payload size). A record is either a bare DXBC container
(compute-only groups such as GHDM) or "CXDK2000" | u32 dxbcSize | u32 tailSize | DXBC | tail (tail = interface
metadata: cbuffer member names/offsets, signature and sampler names; it does not depend on the DXBC bytes). Records are
contiguous in table order. Record k is the k-th DXBC in the payload, i.e. the "#k" of pso_map.csv / dxil_index.csv.

    g = Group(open(path, "rb").read())
    g.dxbc(k)                    -> bytes of record k's DXBC container
    new = g.replace({k: dxbc})   -> new payload (other records byte-identical, tails kept)
    g.verify(new, {k: dxbc})     -> asserts every table entry / untouched record
"""
import os, struct, sys
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", ".."))
import idrk  # noqa: E402


class Group:
    def __init__(self, file_bytes):
        self.file = file_bytes
        self.p = idrk.unpack(file_bytes)
        p = self.p
        assert p[:8] == b"_S2G7000", "not an S2G group"
        self.name = idrk.group_name(p)
        total, = struct.unpack_from("<I", p, 0x08)
        self.tab, = struct.unpack_from("<I", p, 0x98)
        self.base, self.blobs = struct.unpack_from("<II", p, 0xB0)
        assert total == len(p) and self.base + self.blobs == len(p), "S2G header sizes"
        n = len(list(idrk.iter_dxbc(p)))
        self.ents = []
        end = 0
        for k in range(n):
            o, s = struct.unpack_from("<II", p, self.tab + 8 * k)
            assert o == end, "records are not contiguous (k=%d)" % k
            end = o + s
            if p[self.base + o:self.base + o + 4] == b"DXBC":
                end = (end + 7) & ~7             # bare DXBC records are padded to 8 (GHDM layout)
            self.ents.append((o, s))
        assert end == self.blobs or (end + 7) & ~7 == self.blobs, (end, self.blobs)
        self.cxdk = p[self.base:self.base + 8] == b"CXDK2000"

    def rec(self, k):
        o, s = self.ents[k]
        return self.p[self.base + o:self.base + o + s]

    def dxbc(self, k):
        r = self.rec(k)
        if r[:8] == b"CXDK2000":
            dsz, tsz = struct.unpack_from("<II", r, 8)
            return r[16:16 + dsz]
        assert r[:4] == b"DXBC"
        size, = struct.unpack_from("<I", r, 24)
        return r[:size]

    def build_rec(self, k, dxbc):
        r = self.rec(k)
        if r[:8] == b"CXDK2000":
            dsz, tsz = struct.unpack_from("<II", r, 8)
            tail = r[16 + dsz:16 + dsz + tsz]
            out = bytearray(b"CXDK2000" + struct.pack("<II", len(dxbc), tsz) + dxbc + tail)
            while len(out) % 4:
                out += b"\0"
            return bytes(out)
        out = bytearray(dxbc)
        while len(out) % 8:
            out += b"\0"
        return bytes(out)

    def replace(self, new):
        p = bytearray(self.p[:self.base])
        offs, sizes = [], []
        for k in range(len(self.ents)):
            r = self.build_rec(k, new[k]) if k in new else self.rec(k)
            offs.append(len(p) - self.base)
            sizes.append(len(r))
            p += r
        for k in range(len(self.ents)):
            sz = sizes[k]
            if k in new and self.rec(k)[:4] == b"DXBC":
                sz = len(new[k])                 # bare DXBC: the table keeps the exact container size
            struct.pack_into("<II", p, self.tab + 8 * k, offs[k], sz)
        struct.pack_into("<I", p, 0x08, len(p))
        struct.pack_into("<I", p, 0xB4, len(p) - self.base)
        return bytes(p)

    def verify(self, newp, new):
        g2 = Group(idrk.pack(newp, self.file))
        assert g2.p == newp
        assert len(g2.ents) == len(self.ents)
        for k in range(len(self.ents)):
            if k in new:
                assert g2.dxbc(k) == new[k], k
                r1, r2 = self.rec(k), g2.rec(k)
                if r1[:8] == b"CXDK2000":
                    d1, t1 = struct.unpack_from("<II", r1, 8)
                    d2, t2 = struct.unpack_from("<II", r2, 8)
                    assert t1 == t2 and r1[16 + d1:16 + d1 + t1] == r2[16 + d2:16 + d2 + t2], "tail changed"
            else:
                assert g2.rec(k) == self.rec(k), "record %d not byte-identical" % k
        # everything before the blobs except the table and the two size fields is unchanged
        a, b = bytearray(self.p[:self.base]), bytearray(newp[:self.base])
        for buf in (a, b):
            struct.pack_into("<I", buf, 0x08, 0)
            struct.pack_into("<I", buf, 0xB4, 0)
            buf[self.tab:self.tab + 8 * len(self.ents)] = bytes(8 * len(self.ents))
        assert a == b, "header / key area changed"
        return g2
