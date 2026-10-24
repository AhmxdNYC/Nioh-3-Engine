"""Nioh 3 RDB/RDX/FDATA + LFM (archive_xx.bin) reader - headless Python port of
MrIkso/RDBExplorer core (RDBReader, RDXReader, ArchiveExploler.GetEntryData,
LFMBParser.UnpackBinArchive).  Stdlib only.  READ-ONLY on game files.

Usage (run with `py -I`):
  py -I n3rdb.py list   <pkgdir> [root|system] > out.tsv     # list RDB entries
  py -I n3rdb.py get    <pkgdir> <rdb> <ktid hex> <outfile>   # extract one entry
  py -I n3rdb.py dump   <pkgdir> <rdb> <tsv-filter-file> <outdir>  # extract ktids listed (1st col)
  py -I n3rdb.py lfm    <archivedir> <NN> [outdir]            # list (and extract) archive_NN.bin
"""
import os, re, struct, sys, zlib

HERE = os.path.dirname(os.path.abspath(__file__))
SRC = os.path.join(HERE, '..', 'RDBExplorer', 'RDBExplorer')


def load_types():
    t = {}
    p = os.path.join(SRC, 'Utils', 'TypeIDHelper.cs')
    for m in re.finditer(r'0x([0-9A-Fa-f]+)\s*=>\s*new\("([^"]+)"(?:,\s*"([^"]*)")?\)', open(p, encoding='utf-8').read()):
        t[int(m.group(1), 16)] = (m.group(2), m.group(3) or '')
    return t


def load_names():
    n = {}
    p = os.path.join(SRC, 'Databases', 'rdb_names.csv')
    with open(p, encoding='utf-8', errors='replace') as f:
        for line in f:
            i = line.find(',')
            if i < 0:
                continue
            h = line[:i].strip()
            if h.startswith('0x'):
                h = h[2:]
            try:
                n[int(h, 16)] = line[i + 1:].strip()
            except ValueError:
                pass
    return n


def read_rdx(path):
    d = open(path, 'rb').read()
    out = {}
    for i in range(0, len(d) - 7, 8):
        idx, sub, loc, fid = struct.unpack_from('<HBBI', d, i)
        out[idx] = fid
    return out


def read_rdb(pkgdir, name):
    rdx = read_rdx(os.path.join(pkgdir, name + '.rdx'))
    d = open(os.path.join(pkgdir, name + '.rdb'), 'rb').read()
    assert d[:4] == b'_DRK', 'bad rdb'
    hsize, sysid, count, dbid = struct.unpack_from('<iiiI', d, 8)
    folder = d[24:32].rstrip(b'\0').decode()
    pos = 32
    ents = []
    for _ in range(count):
        pos = (pos + 3) & ~3
        start = pos
        magic = d[pos:pos + 4]
        ver, esize, dsize, fsize, etype, ktid, tktid, flags = struct.unpack_from('<IqqqIIII', d, pos + 4)
        pos += 48
        plen = esize - dsize - 48
        params = d[pos:pos + plen] if plen > 0 else b''
        pos += max(plen, 0)
        loc = None
        if dsize == 0x11:
            nf, hi = struct.unpack_from('<HB', d, pos)
            lo, sz, fid = struct.unpack_from('<IIH', d, pos + 6)
            loc = (nf, (hi << 32) | lo, sz, fid)
        elif dsize == 0x0D:
            nf, off, sz, fid = struct.unpack_from('<HIIH', d, pos)
            loc = (nf, off, sz, fid)
        pos += dsize
        cont = ''
        if loc:
            if loc[0] == 0xC01:
                cont = '%s%02x/0x%08x.file' % (folder, ktid & 0xFF, ktid)
            else:
                cont = '0x%08x.fdata' % rdx.get(loc[3], 0)
        ents.append(dict(start=start, ktid=ktid, tktid=tktid, fsize=fsize, flags=flags, etype=etype,
                         loc=loc, cont=cont, params=params))
    return ents


def read_idrk(f, off):
    f.seek(off)
    h = f.read(56)
    if h[:4] != b'IDRK':
        raise ValueError('no IDRK at %x in %s' % (off, f.name))
    allsz, csz, usz, pds, hn, ht, flags, rid, pc = struct.unpack_from('<qqqiiiIIi', h, 8)
    if pc > 0:
        f.read(pc * 12)
        f.read(pds)
    return usz, csz, flags


def get_data(pkgdir, e):
    path = os.path.join(pkgdir, e['cont'])
    with open(path, 'rb') as f:
        off = e['loc'][1] if e['loc'][0] != 0xC01 else 0
        usz, csz, flags = read_idrk(f, off)
        comp = (flags >> 20) & 0x3F
        if comp in (1, 4):
            out = bytearray()
            while len(out) < usz:
                if comp == 4:
                    z = struct.unpack('<H', f.read(2))[0]
                    f.read(8)
                else:
                    z = struct.unpack('<I', f.read(4))[0]
                if z == 0 or z == 0xFFFFFFFF:
                    break
                out += zlib.decompress(f.read(z))
            return bytes(out)
        return f.read(usz)


def lfm(archdir, nn, outdir=None):
    order = open(os.path.join(archdir, 'lfm_order_%s.bin' % nn), 'rb').read()
    cnt = struct.unpack_from('<I', order, 8)[0]
    names = []
    for i in range(cnt):
        _, idx, ptr = struct.unpack_from('<III', order, 40 + i * 12)
        names.append(order[ptr:order.index(b'\0', ptr)].decode())
    with open(os.path.join(archdir, 'archive_%s.bin' % nn), 'rb') as f:
        h = f.read(32)
        n = struct.unpack_from('<q', h, 8)[0]
        tab = [struct.unpack('<qqqq', f.read(32)) for _ in range(n)]
        for i, (off, usz, sz, isc) in enumerate(tab):
            nm = names[i] if i < len(names) else '?'
            data = b''
            if outdir or True:
                f.seek(off)
                raw = f.read(min(sz, 64 * 1024 * 1024)) if outdir else f.read(min(sz, 4096))
                if isc == 1:
                    data = decompress_assets(raw, full=bool(outdir))
                else:
                    data = raw
            magic = data[:16]
            print('%s\t%d\t%d\t%d\t%d\t%s\t%r' % (nn, i, usz, sz, isc, nm, magic))
            if outdir:
                p = os.path.join(outdir, 'archive_%s' % nn, nm.strip('/').replace('/', os.sep))
                os.makedirs(os.path.dirname(p), exist_ok=True)
                open(p, 'wb').write(data)


def decompress_assets(raw, full=True):
    # blocks: 16-aligned, [u32 usize][78 9C ...]
    offs = []
    p = 0
    while p <= len(raw) - 6:
        us = struct.unpack_from('<I', raw, p)[0]
        if 0 < us <= 0x10000 and raw[p + 4:p + 6] == b'\x78\x9c':
            offs.append(p)
        p += 16
    out = bytearray()
    for i, o in enumerate(offs):
        end = offs[i + 1] if i + 1 < len(offs) else len(raw)
        d = zlib.decompressobj()
        try:
            out += d.decompress(raw[o + 4:end])
        except zlib.error:
            pass
        if not full and len(out) >= 16:
            break
    return bytes(out)


def main():
    cmd = sys.argv[1]
    if cmd == 'list':
        pkg = sys.argv[2]
        which = sys.argv[3:] or ['root', 'system']
        types, names = load_types(), load_names()
        print('rdb\tktid\ttype\tfsize\tcsize\tcontainer\toffset\tflags\tetype\tname')
        for w in which:
            for e in read_rdb(pkg, w):
                tn, ext = types.get(e['tktid'], ('Unknown_0x%08X' % e['tktid'], ''))
                nm = names.get(e['ktid'], '')
                l = e['loc'] or (0, 0, 0, 0)
                print('%s\t%08x\t%s\t%d\t%d\t%s\t%x\t%08x\t%d\t%s' % (w, e['ktid'], tn, e['fsize'], l[2], e['cont'], l[1], e['flags'], e['etype'], nm))
    elif cmd == 'get':
        pkg, w, k, out = sys.argv[2:6]
        k = int(k, 16)
        for e in read_rdb(pkg, w):
            if e['ktid'] == k:
                open(out, 'wb').write(get_data(pkg, e))
                print('ok', len(open(out, 'rb').read()))
                return
        print('not found')
    elif cmd == 'dump':
        pkg, w, lst, outd = sys.argv[2:6]
        want = set()
        for line in open(lst, encoding='utf-8'):
            c = line.split('\t')[0].strip()
            try:
                want.add(int(c, 16))
            except ValueError:
                pass
        types = load_types()
        os.makedirs(outd, exist_ok=True)
        n = 0
        for e in read_rdb(pkg, w):
            if e['ktid'] in want and e['loc']:
                ext = types.get(e['tktid'], ('', ''))[1] or '.bin'
                try:
                    open(os.path.join(outd, '0x%08x%s' % (e['ktid'], ext)), 'wb').write(get_data(pkg, e))
                    n += 1
                except Exception as ex:
                    print('fail %08x %s' % (e['ktid'], ex))
        print('extracted', n)
    elif cmd == 'lfm':
        arch, nn = sys.argv[2:4]
        out = sys.argv[4] if len(sys.argv) > 4 else None
        lfm(arch, nn, out)


if __name__ == '__main__':
    main()
