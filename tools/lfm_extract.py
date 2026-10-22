"""Read-only LFMB archive_NN.bin extractor (format found from the files; see notes/game-data.md).
  py -I tools/lfm_extract.py <archive_NN.bin> <entry index> <out file>      extract one entry
  py -I tools/lfm_extract.py <archive_NN.bin>                              list entries
Header: 'LFMB', u32 0, u32 count @+8, entries at +0x20, 32 B each {u64 offset, u64 usize, u64 csize, u64 flag (1 = chunked zlib)}.
Compressed entries are 16 KB zlib chunks, each with a 4-byte header; chunks are located by their zlib magic."""
import struct, sys, zlib


def entries(d):
    assert d[:4] == b'LFMB'
    n = struct.unpack_from('<I', d, 8)[0]
    return [struct.unpack_from('<4Q', d, 0x20 + 32 * i) for i in range(n)]


def blob(d, e):
    off, us, cs, fl = e
    if not fl:
        return d[off:off + us]
    raw = d[off:off + cs]
    out, p = [], 0
    while p < len(raw):
        q = raw.find(b'\x78\x9c', p)
        if q < 0:
            break
        o = zlib.decompressobj()
        out.append(o.decompress(raw[q:]))
        p = len(raw) - len(o.unused_data)
    b = b''.join(out)
    assert len(b) == us, (len(b), us)
    return b


if __name__ == '__main__':
    d = open(sys.argv[1], 'rb').read()
    es = entries(d)
    if len(sys.argv) < 4:
        for i, e in enumerate(es):
            print(i, *e)
    else:
        open(sys.argv[3], 'wb').write(blob(d, es[int(sys.argv[2])]))
