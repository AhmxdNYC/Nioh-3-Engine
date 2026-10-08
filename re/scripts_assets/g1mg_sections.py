"""List G1MG subsections (type, size, count). usage: py -I g1mg_sections.py file..."""
import sys, struct
def g1mg(b):
    hdr = struct.unpack_from('<I', b, 12)[0]; o = hdr
    while o < len(b):
        mg, ver, sz = struct.unpack_from('<4s4sI', b, o)
        if mg[::-1] == b'G1MG': return o, sz
        o += sz
def sections(b):
    o, sz = g1mg(b); end = o + sz
    n = struct.unpack_from('<I', b, o + 0x2c)[0]; p = o + 0x30
    out = []
    for i in range(n):
        t, s, c = struct.unpack_from('<III', b, p); out.append((t, p, s, c)); p += s
        if p > end: break
    return out
if __name__ == '__main__':
    for fn in sys.argv[1:]:
        b = open(fn, 'rb').read(); print('##', fn)
        for t, p, s, c in sections(b): print(f'  {t:#x} @{p:#x} size={s} count={c}')
