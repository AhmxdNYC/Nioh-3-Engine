r"""s2g.py - read / splice the record table of a Nioh 3 "_S2G7000" shader group (payload of an IDRK .file).

Record table at payload+0x98: (u32 offset, u32 size) per record, offsets relative to the blob base (payload+0xB0).
A record is a plain DXBC container (compute groups: GHDM, RefProbe, VFSCAT, ...) or "CXDK2000"|u32 dxbc|u32 tail|
DXBC|tail (graphics groups). Records follow each other in table order; the table stores each record's size.

    recs = records(payload)                     # [(index, offset, size, kind, container digest, HASH digest)]
    new_payload = splice(payload, {index: new_dxbc_bytes})
"""
import struct
import os, sys
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
import idrk


def layout(p):
    total, = struct.unpack_from("<I", p, 0x08)
    tab, = struct.unpack_from("<I", p, 0x98)
    base, blobs = struct.unpack_from("<II", p, 0xB0)
    assert p[:8] == b"_S2G7000" and total == len(p) and base + blobs == len(p), "not a plain S2G payload"
    ents = []
    while tab + 8 * len(ents) + 8 <= base:
        o, s = struct.unpack_from("<II", p, tab + 8 * len(ents))
        if base + o + 4 > len(p) or p[base + o:base + o + 4] not in (b"DXBC", b"CXDK"):
            break
        if p[base + o:base + o + 4] == b"DXBC":
            if struct.unpack_from("<I", p, base + o + 24)[0] != s:     # DXBC container size must match the entry
                break
        elif s < 16:
            break
        ents.append((o, s))
    return tab, base, ents


def records(p):
    tab, base, ents = layout(p)
    out = []
    for i, (o, s) in enumerate(ents):
        kind = "dxbc" if p[base + o:base + o + 4] == b"DXBC" else "cxdk"
        dx = base + o if kind == "dxbc" else base + o + 16
        size, dig, parts = idrk.dxbc_parts(p, dx)
        hp = parts.get("HASH")
        hd = p[hp[0] + 4:hp[0] + 20].hex() if hp else ""
        out.append((i, o, s, kind, dig.hex(), hd))
    return out


def splice(p, new):
    """replace plain-DXBC records {index: dxbc bytes}. Records are stored back to back (no padding) but the table is
    not in storage order, so the blob area is rebuilt in the original storage order with the replaced records
    substituted and every table entry re-pointed. Every other record is moved byte-identically."""
    tab, base, ents = layout(p)
    for i in new:
        assert p[base + ents[i][0]:base + ents[i][0] + 4] == b"DXBC", "only plain DXBC records are supported"
    order = sorted(range(len(ents)), key=lambda i: ents[i][0])
    assert ents[order[0]][0] == 0
    for a, b in zip(order, order[1:]):
        assert ents[a][0] + ents[a][1] == ents[b][0], "records are not contiguous"
    end_old = base + ents[order[-1]][0] + ents[order[-1]][1]
    out = bytearray(p[:base])
    offs = [0] * len(ents)
    sizes = [0] * len(ents)
    for i in order:
        o, s = ents[i]
        offs[i] = len(out) - base
        blob = new[i] if i in new else p[base + o:base + o + s]
        out += blob
        sizes[i] = len(blob)
    out += p[end_old:]
    for i in range(len(ents)):
        struct.pack_into("<II", out, tab + 8 * i, offs[i], sizes[i])
    struct.pack_into("<I", out, 0x08, len(out))
    struct.pack_into("<I", out, 0xB4, len(out) - base)
    out = bytes(out)
    _, _, e2 = layout(out)
    assert len(e2) == len(ents)
    for i, (o, s) in enumerate(e2):
        if i in new:
            assert out[base + o:base + o + s] == new[i]
        else:
            oo, ss = ents[i]
            assert out[base + o:base + o + s] == p[base + oo:base + oo + ss], i
    return out


if __name__ == "__main__":
    b = open(sys.argv[1], "rb").read()
    p = idrk.unpack(b)
    tab, base, ents = layout(p)
    print(idrk.group_name(p), "records", len(ents), "payload", len(p))
    for r in records(p):
        if len(sys.argv) < 3 or r[5] in sys.argv[2:] or r[4] in sys.argv[2:]:
            print(r)
