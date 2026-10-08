r"""build_sfb.py - SFB-DEDUPE generator: rewrite every SamplerFeedback-MinMip pixel shader record of the selected
shader_dx12 files, prove each one, and stage the repacked files (game folder is only read).

    py -I gpu-shaders\materials\build_sfb.py [--scene | --all | --files 0x...file,...] [--jobs 8] [--pixels 200 --scenes 6]

--scene (default): the files whose pixel shaders ran in the r5 RGP frame (materials, trees, shadow-map alpha tests...).
Per pixel-shader record:
  1. dxc -dumpbin -> sfb_rewrite.rewrite() (records without the pattern are left byte-identical);
  2. dxcom: text -> bitcode -> container; ISG1/OSG1/PSV0/SFI0 must equal the original's; original STAT/ILDN kept;
     validated + signed by dxil.dll; dxc -dumpbin of the result must equal the rewritten module text;
  3. verify_sfb.lint(): every group matches the template; dx.op call counts unchanged;
  4. verify_sfb.verify(): psim interpreter, original vs rewrite, random scenes: outputs / discard / UAV writes bit-identical.
Any failure keeps that record original. Files: gpu-shaders\replacement\<file> (git-ignored), manifest
gpu-shaders\materials\sfb_manifest.tsv (ally_shaders.sh / apply_sfb.ps1), log gpu-shaders\materials\out\sfb_build.csv.
"""
import csv, hashlib, os, re, struct, subprocess, sys, tempfile, time
from multiprocessing import Pool
HERE = os.path.dirname(os.path.abspath(__file__))
G = os.path.abspath(os.path.join(HERE, '..'))
sys.path.insert(0, HERE); sys.path.insert(0, G)
import idrk  # noqa: E402
SD = r"C:\Program Files (x86)\Steam\steamapps\common\Nioh3\shader_dx12"
REPL = os.path.join(G, 'replacement')
ROOT = os.path.abspath(os.path.join(G, '..'))
ARGS = {'pixels': 200, 'scenes': 6}


class Group:
    """S2G group with CXDK or bare-DXBC records; keeps the original padding rule (bare: as found, CXDK: 4)."""

    def __init__(self, fb):
        self.file, self.p = fb, idrk.unpack(fb)
        p = self.p
        assert p[:8] == b'_S2G7000'
        self.tab, = struct.unpack_from('<I', p, 0x98)
        self.base, self.blobs = struct.unpack_from('<II', p, 0xB0)
        n = len(list(idrk.iter_dxbc(p)))
        self.ents = [struct.unpack_from('<II', p, self.tab + 8 * k) for k in range(n)]
        end = 0
        self.bare_pad = 1
        for o, s in self.ents:
            if o != end:
                assert o == (end + 7) & ~7, 'unexpected record gap'
                self.bare_pad = 8
            end = o + s
        assert (end + self.bare_pad - 1) & ~(self.bare_pad - 1) == self.blobs or end == self.blobs

    def rec(self, k):
        o, s = self.ents[k]
        return self.p[self.base + o:self.base + o + s]

    def dxbc(self, k):
        r = self.rec(k)
        if r[:8] == b'CXDK2000':
            d, t = struct.unpack_from('<II', r, 8)
            return r[16:16 + d]
        return r[:struct.unpack_from('<I', r, 24)[0]]

    def build_rec(self, k, dx):
        r = self.rec(k)
        if r[:8] == b'CXDK2000':
            d, t = struct.unpack_from('<II', r, 8)
            out = bytearray(b'CXDK2000' + struct.pack('<II', len(dx), t) + dx + r[16 + d:16 + d + t])
            while len(out) % 4:
                out += b'\0'
            return bytes(out), len(out)
        out = bytearray(dx)
        while len(out) % self.bare_pad:
            out += b'\0'
        return bytes(out), len(dx)

    def replace(self, new):
        p = bytearray(self.p[:self.base])
        tab = []
        for k in range(len(self.ents)):
            if k in new:
                r, sz = self.build_rec(k, new[k])
            else:
                r = self.rec(k); sz = len(r)
                while len(r) % (4 if r[:8] == b'CXDK2000' else self.bare_pad):
                    r += b'\0'
            tab.append((len(p) - self.base, sz)); p += r
        for k, (o, s) in enumerate(tab):
            struct.pack_into('<II', p, self.tab + 8 * k, o, s)
        struct.pack_into('<I', p, 0x08, len(p)); struct.pack_into('<I', p, 0xB4, len(p) - self.base)
        return bytes(p)


def container(orig, new):
    """original part order; DXIL/HASH from the new container, everything else from the original (must be equal
    where it describes the interface), then validate + sign."""
    import dxcom
    po, pn = dxcom.parts(orig), dxcom.parts(new)
    for k in ('ISG1', 'OSG1', 'PSV0', 'SFI0'):
        if po.get(k) != pn.get(k):
            raise ValueError('interface part %s differs' % k)
    n = struct.unpack_from('<I', orig, 28)[0]
    order = [orig[struct.unpack_from('<I', orig, 32 + 4 * i)[0]:][:4].decode('latin1') for i in range(n)]
    parts = [(k, pn[k] if k in ('DXIL', 'HASH') else po[k]) for k in order]
    hdr = 32 + 4 * len(parts)
    offs, body = [], b''
    for k, v in parts:
        offs.append(hdr + len(body)); body += k.encode('latin1') + struct.pack('<I', len(v)) + v
    c = b'DXBC' + bytes(16) + struct.pack('<III', 1, hdr + len(body), len(parts)) + b''.join(struct.pack('<I', o) for o in offs) + body
    return dxcom.validate_sign(c)


_GCACHE = {}


def _group(fname):
    g = _GCACHE.get(fname)
    if g is None:
        if len(_GCACHE) > 4:
            _GCACHE.clear()
        g = _GCACHE[fname] = Group(open(os.path.join(SD, fname), 'rb').read())
    return g


def do_record(task):
    """one pixel-shader record -> (file, k, row, new container or None)"""
    import dxcom, sfb_rewrite, verify_sfb
    fname, k = task
    t0 = time.time()
    dx = _group(fname).dxbc(k)
    tmp = tempfile.mkdtemp(prefix='sfb_')
    try:
        fp = os.path.join(tmp, 'r.dxbc'); open(fp, 'wb').write(dx)
        ll = dxcom.dumpbin(fp)
        row = dict(file=fname, rec=k, groups=0, status='no-pattern', inst_lines=ll.count(chr(10)))
        c = None
        try:
            nt, n = sfb_rewrite.rewrite(ll)
            row['groups'] = n
            if n:
                c = container(dx, dxcom.ll_to_container(nt))
                fq = os.path.join(tmp, 'n.dxbc'); open(fq, 'wb').write(c)
                if dxcom.body(dxcom.dumpbin(fq)) != dxcom.body(nt):
                    raise ValueError('assembled module != rewritten text')
                ok, msg = verify_sfb.lint(ll, nt)
                if not ok:
                    raise ValueError('lint: ' + msg)
                ok, msg, cov = verify_sfb.verify(ll, nt, ARGS['pixels'], ARGS['scenes'])
                if not ok:
                    raise ValueError('interp: ' + msg)
                cv = verify_sfb.sfb_block_coverage(nt, cov)
                row.update(status='proven', both=sum(1 for x, y in cv if x and y), taken_only=sum(1 for x, y in cv if x and not y),
                           skipped_only=sum(1 for x, y in cv if y and not x), never=sum(1 for x, y in cv if not x and not y),
                           old_size=len(dx), new_size=len(c), new_hash=dxcom.parts(c)['HASH'][4:20].hex())
        except Exception as e:   # noqa: BLE001 - any failure keeps the record original
            row['status'] = 'FAILED: ' + str(e)[:300].replace(chr(10), ' ')
            c = None
        row['secs'] = round(time.time() - t0, 2)
        return fname, k, row, c
    finally:
        import shutil
        shutil.rmtree(tmp, ignore_errors=True)


def repack(fname, new):
    fb = open(os.path.join(SD, fname), 'rb').read()
    g = Group(fb)
    p2 = g.replace(new)
    g2 = Group(idrk.pack(p2, fb))
    assert g2.p == p2 and len(g2.ents) == len(g.ents)
    for k in range(len(g.ents)):
        assert (g2.dxbc(k) == new[k]) if k in new else (g2.rec(k) == g.rec(k)), k
    a, b = bytearray(g.p[:g.base]), bytearray(p2[:g.base])
    for buf in (a, b):
        struct.pack_into('<I', buf, 0x08, 0); struct.pack_into('<I', buf, 0xB4, 0)
        buf[g.tab:g.tab + 8 * len(g.ents)] = bytes(8 * len(g.ents))
    assert a == b, 'header changed'
    out = idrk.pack(p2, fb)
    os.makedirs(REPL, exist_ok=True)
    open(os.path.join(REPL, fname), 'wb').write(out)
    return hashlib.sha256(fb).hexdigest(), hashlib.sha256(out).hexdigest()


def ps_records(files):
    want = set(files)
    rows = [r for r in csv.DictReader(open(os.path.join(G, 'dxil_index.csv'))) if r['file'] in want]
    by = {}
    for r in rows:
        by.setdefault(r['file'], []).append(r)
    tasks = []
    for f, rs in by.items():
        for k, r in enumerate(rs):        # dxil_index lists a file's containers in payload order = record index
            if r['kind'] == 'ps':
                tasks.append((f, k))
    return tasks


def scene_files():
    pm = os.path.join(ROOT, 're', 'scripts_gpu', 'out', 'rgp_top300', 'Nioh3_2026-10-06_19-39-58_shaders', 'pso_map.csv')
    fs = set()
    for r in csv.DictReader(open(pm)):
        for m in re.finditer(r'pixel:[0-9a-f]+=(0x[0-9a-f]+\.file)/([^#]+)#', r['shaders(stage:dxbc_digest=file/group#index)']):
            if 'Sfb' in m.group(2):
                fs.add(m.group(1))
    return sorted(fs)


def all_files():
    return sorted({r['file'] for r in csv.DictReader(open(os.path.join(G, 'dxil_index.csv'))) if r['kind'] == 'ps' and 'Sfb' in r['group']})


if __name__ == '__main__':
    a = sys.argv[1:]
    files = all_files() if '--all' in a else a[a.index('--files') + 1].split(',') if '--files' in a else scene_files()
    jobs = int(a[a.index('--jobs') + 1]) if '--jobs' in a else max(1, (os.cpu_count() or 2) - 2)
    for k in ARGS:
        if '--' + k in a:
            ARGS[k] = int(a[a.index('--' + k) + 1])
    tasks = ps_records(files)
    print('%d files, %d pixel-shader records, %d jobs' % (len(files), len(tasks), jobs), flush=True)
    recs, new = [], {}
    t0 = time.time()
    with Pool(jobs) as pool:
        for i, (fname, k, row, c) in enumerate(pool.imap_unordered(do_record, tasks, chunksize=4)):
            recs.append(row)
            if c:
                new.setdefault(fname, {})[k] = c
            if (i + 1) % 50 == 0 or i + 1 == len(tasks):
                print('%5d/%d records  %.0fs  proven=%d failed=%d' % (i + 1, len(tasks), time.time() - t0,
                      sum(1 for x in recs if x['status'] == 'proven'), sum(1 for x in recs if x['status'].startswith('FAILED'))), flush=True)
    res = []
    for fname in sorted(set(files)):
        ho = hn = ''
        if fname in new:
            ho, hn = repack(fname, new[fname])
        res.append((fname, ho, hn, sorted([x for x in recs if x['file'] == fname], key=lambda x: x['rec']), 0))
    res.sort()
    os.makedirs(os.path.join(HERE, 'out'), exist_ok=True)
    tag = 'all' if '--all' in a else 'scene' if '--files' not in a else 'files'
    keys = ['file', 'rec', 'groups', 'status', 'both', 'taken_only', 'skipped_only', 'never', 'old_size', 'new_size', 'new_hash', 'inst_lines', 'secs']
    with open(os.path.join(HERE, 'out', 'sfb_build_%s.csv' % tag), 'w', newline='') as f:
        w = csv.DictWriter(f, keys, extrasaction='ignore'); w.writeheader()
        for r in res:
            for row in r[3]:
                w.writerow(row)
    with open(os.path.join(HERE, 'sfb_manifest_%s.tsv' % tag), 'w', newline='\n') as f:
        for fname, ho, hn, rows, _ in res:
            if hn:
                f.write('sfb %s %s %s\n' % (fname, ho, hn))
    allrows = [x for r in res for x in r[3]]
    print('records: %d ps, %d with pattern, %d proven, %d failed; files staged: %d' % (
        len(allrows), sum(1 for x in allrows if x['groups']), sum(1 for x in allrows if x['status'] == 'proven'),
        sum(1 for x in allrows if x['status'].startswith('FAILED')), sum(1 for r in res if r[2])))
