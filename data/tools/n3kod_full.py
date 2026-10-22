"""Dump Nioh 3 .kidsobjdb (KOD_) to readable text using RDBExplorer's name databases.
Usage: py -I n3kod.py <file.kidsobjdb> [type-substring-filter]   > out.txt
"""
import os, re, struct, sys

HERE = os.path.dirname(os.path.abspath(__file__))
DB = os.path.join(HERE, '..', 'RDBExplorer', 'RDBExplorer', 'Databases')


def load_types():
    t = {}
    txt = open(os.path.join(DB, 'kidstypeinfodb.yml'), encoding='utf-8').read()
    for m in re.finditer(r'LocalName: ?(.*)\n\s+FullName: ?(.*)\n\s+Hash: 0x([0-9A-Fa-f]+)', txt):
        t[int(m.group(3), 16)] = m.group(2).strip() or m.group(1).strip()
    extra = os.path.join(HERE, 'extra_types.tsv')
    if os.path.exists(extra):
        for line in open(extra, encoding='utf-8'):
            h, n = line.rstrip('\n').split('\t')[:2]
            t.setdefault(int(h, 16), n)
    return t


def load_props():
    p = {}
    for line in open(os.path.join(DB, 'all_properties.csv'), encoding='utf-8'):
        a = line.strip().split(',')
        if len(a) >= 2:
            try:
                p[int(a[0], 16)] = a[1]
            except ValueError:
                pass
    extra = os.path.join(HERE, 'extra_props.tsv')
    if os.path.exists(extra):
        for line in open(extra, encoding='utf-8'):
            h, n = line.rstrip('\n').split('\t')[:2]
            p.setdefault(int(h, 16), n)
    return p


FMT = {0: ('?', 1), 1: ('B', 1), 2: ('h', 2), 3: ('H', 2), 4: ('i', 4), 5: ('I', 4), 6: ('q', 8), 7: ('Q', 8),
       8: ('f', 4), 10: ('4f', 16), 12: ('2f', 8), 13: ('3f', 12)}


def parse(data):
    assert data[:4] == b'_DOK', data[:4]
    hsize, plat, n, namektid, total = struct.unpack_from('<IIIII', data, 8)
    pos = hsize
    objs = []
    for _ in range(n):
        start = pos
        sig = data[pos:pos + 4]
        if sig == b'IDOK':
            esz, ktid, tk, ncol = struct.unpack_from('<IIII', data, pos + 8)
            pos += 24
            ref = None
        elif sig == b'RDOK':
            esz, ktid, pf, pk, ncol = struct.unpack_from('<IIIII', data, pos + 8)
            pos += 28
            tk = None
            ref = (pf, pk)
        else:
            raise ValueError('bad sig %r at %x' % (sig, pos))
        cols = []
        for c in range(ncol):
            cols.append(struct.unpack_from('<iII', data, pos))
            pos += 12
        vals = []
        for ty, rc, pk in cols:
            f, sz = FMT[ty]
            v = [struct.unpack_from('<' + f, data, pos + i * sz) for i in range(rc)]
            pos += rc * sz
            vals.append((ty, pk, v))
        pos = max(pos, start + esz)
        pos = (pos + 3) & ~3
        objs.append(dict(ktid=ktid, type=tk, ref=ref, cols=vals))
    return namektid, objs


def fmtv(ty, v):
    if ty == 1:
        b = bytes(x[0] for x in v)
        if b and all(32 <= c < 127 or c == 0 for c in b) and b.rstrip(b'\0'):
            return repr(b.rstrip(b'\0').decode())
        return b.hex() if len(b) <= 64 else b[:64].hex() + '...(%d)' % len(b)
    out = []
    for x in v[:5000]:
        if ty == 8:
            out.append('%g' % x[0])
        elif ty in (10, 12, 13):
            out.append('(' + ','.join('%g' % y for y in x) + ')')
        elif ty in (5, 7):
            out.append('0x%x' % x[0] if x[0] > 0xFFFF else str(x[0]))
        else:
            out.append(str(x[0]))
    s = ', '.join(out)
    if len(v) > 5000:
        s += ' ...(%d)' % len(v)
    return ('[' + s + ']') if len(v) != 1 else s


def main():
    path = sys.argv[1]
    filt = sys.argv[2].lower() if len(sys.argv) > 2 else None
    types, props = load_types(), load_props()
    namektid, objs = parse(open(path, 'rb').read())
    print('# %s  objects=%d namektid=%08x' % (os.path.basename(path), len(objs), namektid))
    for o in objs:
        tn = types.get(o['type'], 'type_%08x' % o['type']) if o['type'] is not None else 'REF %08x/%08x' % o['ref']
        if filt and filt not in tn.lower():
            continue
        print('== obj %08x  %s' % (o['ktid'], tn))
        for ty, pk, v in o['cols']:
            print('   %-44s %s' % (props.get(pk, 'p_%08x' % pk), fmtv(ty, v)))


if __name__ == '__main__':
    main()
