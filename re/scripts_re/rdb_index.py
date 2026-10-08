r"""Read-only full index of Nioh 3 RDB (package\root|system.rdb + .rdx + .fdata) and LFM (archive\archive_NN.bin).  Stdlib only.

  py -I re\scripts_re\rdb_index.py [--game DIR] [--out D:\nioh3-re\rdb_index.tsv] [--sniff] [--no-lfm] [--names DIR]

Columns: layer archive offset size csize ktid type_hash type_name name_hash name compression head
  layer=rdb : archive = root|system/container (.fdata or .file); ktid = file KTID; type_hash = type KTID (TypeIDHelper);
              name = rdb_names.csv lookup (RDBExplorer clone in data\RDBExplorer, if present); name_hash = kidshash(name)
              when a name is known (kidshash.py: h=sum c*31^(i+1)); compression = flags bits 20-25 (zlib/zlibx/none) or 'file'.
  layer=lfm : archive = archive_NN.bin; ktid = first 32 bits of the 256-bit path hash (LFM has no KTIDs; the full path
              hash is the 62-hex file name in 'name'); type_hash empty; compression = LFM flag (1 = zlib blocks).
  head      = first 16 bytes (hex) of the decoded payload, only with --sniff (rdb: table-ish types; lfm: all).
Never writes under the game dir.
"""
import argparse, csv, os, re, struct, sys, zlib
from collections import Counter

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from kidshash import kh  # noqa: E402

GAME = r'C:\Program Files (x86)\Steam\steamapps\common\Nioh3'
CLONE = os.path.join(HERE, '..', '..', 'data', 'RDBExplorer', 'RDBExplorer')
SNIFF_TYPES = {'KTIDFileBinary', 'BinaryFile', 'CSVFile', 'ObjectDatabaseFile', 'KSCLFile', 'AssetData', 'StreamAssetDataFile'}
COMP = {0: 'none', 1: 'zlib', 4: 'zlibx'}


def load_types(src):
    t = {}
    p = os.path.join(src, 'Utils', 'TypeIDHelper.cs')
    if os.path.exists(p):
        for m in re.finditer(r'0x([0-9A-Fa-f]+)\s*=>\s*new\("([^"]+)"', open(p, encoding='utf-8').read()):
            t[int(m.group(1), 16)] = m.group(2)
    return t


def load_names(src):
    n = {}
    p = os.path.join(src, 'Databases', 'rdb_names.csv')
    if os.path.exists(p):
        for line in open(p, encoding='utf-8', errors='replace'):
            i = line.find(',')
            if i > 0:
                h = line[:i].strip()
                try:
                    n[int(h[2:] if h.startswith('0x') else h, 16)] = line[i + 1:].strip()
                except ValueError:
                    pass
    return n


def read_rdx(path):
    d = open(path, 'rb').read()
    return {struct.unpack_from('<H', d, i)[0]: struct.unpack_from('<I', d, i + 4)[0] for i in range(0, len(d) - 7, 8)}


def read_rdb(pkg, name):
    rdx = read_rdx(os.path.join(pkg, name + '.rdx'))
    d = open(os.path.join(pkg, name + '.rdb'), 'rb').read()
    assert d[:4] == b'_DRK'
    count = struct.unpack_from('<i', d, 16)[0]
    folder = d[24:32].rstrip(b'\0').decode()
    pos, out = 32, []
    for _ in range(count):
        pos = (pos + 3) & ~3
        ver, esize, dsize, fsize, etype, ktid, tktid, flags = struct.unpack_from('<IqqqIIII', d, pos + 4)
        pos += 48 + max(esize - dsize - 48, 0)
        loc = None
        if dsize == 0x11:
            nf, hi = struct.unpack_from('<HB', d, pos)
            lo, sz, fid = struct.unpack_from('<IIH', d, pos + 6)
            loc = (nf, (hi << 32) | lo, sz, fid)
        elif dsize == 0x0D:
            loc = struct.unpack_from('<HIIH', d, pos)
        pos += dsize
        cont = ''
        if loc:
            cont = '%s%02x/0x%08x.file' % (folder, ktid & 0xFF, ktid) if loc[0] == 0xC01 else '0x%08x.fdata' % rdx.get(loc[3], 0)
        out.append((ktid, tktid, fsize, flags, loc, cont))
    return out


def sniff_rdb(pkg, cont, loc):
    """first 16 decoded bytes of an entry (first zlib chunk only)."""
    try:
        with open(os.path.join(pkg, cont), 'rb') as f:
            f.seek(loc[1] if loc[0] != 0xC01 else 0)
            h = f.read(56)
            if h[:4] != b'IDRK':
                return ''
            allsz, csz, usz, pds, hn, ht, flags, rid, pc = struct.unpack_from('<qqqiiiIIi', h, 8)
            if pc > 0:
                f.read(pc * 12 + pds)
            comp = (flags >> 20) & 0x3F
            if comp in (1, 4):
                if comp == 4:
                    z = struct.unpack('<H', f.read(2))[0]
                    f.read(8)
                else:
                    z = struct.unpack('<I', f.read(4))[0]
                if z in (0, 0xFFFFFFFF):
                    return ''
                return zlib.decompress(f.read(z))[:16].hex()
            return f.read(16).hex()
    except Exception:
        return ''


def lfm_rows(arch, sniff):
    for fn in sorted(os.listdir(arch)):
        m = re.fullmatch(r'archive_([0-9A-Fa-f]{2})\.bin', fn)
        if not m:
            continue
        op = os.path.join(arch, 'lfm_order_%s.bin' % m.group(1))
        names = []
        if os.path.exists(op):
            o = open(op, 'rb').read()
            for i in range(struct.unpack_from('<I', o, 8)[0]):
                ptr = struct.unpack_from('<I', o, 40 + i * 12 + 8)[0]
                names.append(o[ptr:o.index(b'\0', ptr)].decode())
        with open(os.path.join(arch, fn), 'rb') as f:
            n = struct.unpack_from('<q', f.read(32), 8)[0]
            f.seek(32)
            tab = [struct.unpack('<qqqq', f.read(32)) for _ in range(n)]
            for i, (off, usz, sz, isc) in enumerate(tab):
                nm = names[i].strip('/') if i < len(names) else ''
                hx = nm.replace('/', '')
                head = ''
                if sniff:
                    f.seek(off)
                    raw = f.read(min(sz, 4096))
                    if isc == 1:
                        try:
                            raw = zlib.decompressobj().decompress(raw[4:])
                        except zlib.error:
                            raw = b''
                    head = raw[:16].hex()
                yield ['lfm', fn, '%x' % off, usz, sz, hx[:8].lower(), '', '', '', nm, 'zlib' if isc == 1 else 'none', head]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--game', default=GAME)
    ap.add_argument('--out', default=r'D:\nioh3-re\rdb_index.tsv')
    ap.add_argument('--names', default=CLONE)
    ap.add_argument('--sniff', action='store_true')
    ap.add_argument('--no-lfm', action='store_true')
    a = ap.parse_args()
    pkg = os.path.join(a.game, 'package')
    types, names = load_types(a.names), load_names(a.names)
    os.makedirs(os.path.dirname(os.path.abspath(a.out)), exist_ok=True)
    cnt, n = Counter(), 0
    with open(a.out, 'w', encoding='utf-8', newline='') as o:
        w = csv.writer(o, delimiter='\t', lineterminator='\n', quoting=csv.QUOTE_NONE, escapechar='\\')
        w.writerow('layer archive offset size csize ktid type_hash type_name name_hash name compression head'.split())
        for rdb in ('root', 'system'):
            for ktid, tk, fsize, flags, loc, cont in read_rdb(pkg, rdb):
                tn = types.get(tk, 'Unknown')
                nm = names.get(ktid, '')
                head = sniff_rdb(pkg, cont, loc) if a.sniff and loc and tn in SNIFF_TYPES else ''
                comp = 'file' if loc and loc[0] == 0xC01 else COMP.get((flags >> 20) & 0x3F, 'c%d' % ((flags >> 20) & 0x3F))
                w.writerow(['rdb', '%s/%s' % (rdb, cont), '%x' % (loc[1] if loc else 0), fsize, loc[2] if loc else 0,
                            '%08x' % ktid, '%08x' % tk, tn, '%08x' % kh(nm.encode()) if nm else '', nm, comp, head])
                cnt[tn] += 1
                n += 1
        if not a.no_lfm:
            for r in lfm_rows(os.path.join(a.game, 'archive'), a.sniff):
                w.writerow(r)
                cnt['(lfm)'] += 1
                n += 1
    print('%d rows -> %s' % (n, a.out))
    for t, c in cnt.most_common(15):
        print('%8d %s' % (c, t))


if __name__ == '__main__':
    main()
