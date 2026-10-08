"""List G1M chunks (magic, version, size) of a file. usage: py -I g1m_chunks.py file..."""
import sys, struct
for fn in sys.argv[1:]:
    b = open(fn, 'rb').read()
    print('##', fn, len(b), b[:8])
    if b[:4] != b'_M1G': continue
    _, _, total, hdr, _, n = struct.unpack_from('<4s4sIIII', b, 0)
    o = hdr
    while o + 12 <= len(b) and o < total:
        mg, ver, sz = struct.unpack_from('<4s4sI', b, o)
        print(f'  {o:#8x} {mg[::-1].decode("latin1")} {ver[::-1].decode("latin1")} {sz}')
        if sz < 12: break
        o += sz
