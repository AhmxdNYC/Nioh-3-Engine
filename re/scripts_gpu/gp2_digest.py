#!/usr/bin/env python3
"""gp2_digest.py - turn the mod's GPUProf=2 'GP2:' log lines into a ranked per-PSO table.

  py -I re\\scripts_gpu\\gp2_digest.py LOG [LOG...] [--out PREFIX] [--top 40] [--rebuild] [--last]
  py -I re\\scripts_gpu\\gp2_digest.py --selftest

Reads rows written by LogGp2() in mod\\Nioh3EngineFix\\src\\gpuprof.cpp:
  GP2: frame X ms GPU span (...); covered ...; PSO stamps ON|OFF ...
  GP2:   <rank> <vs=<hash> ps=<hash> | cs=<hash> | (copy / resolve / clear) | ...> | <ms> ms | <draws> | <ivals> | [<marker>]
Windows with PSO stamps OFF (A/B) are skipped. Default: mean over all ON windows (ms/frame; % of mean span).
Writes PREFIX.md and PREFIX.csv (default <first log>.gp2). Hash index cached in D:\\nioh3-re\\gp2_hash_index.json
(--rebuild to refresh). Stdlib only, read-only on game/dump data.
"""
import argparse, csv, glob, json, os, re, struct, sys, zlib

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
CACHE = r'D:\nioh3-re\gp2_hash_index.json'
DUMP = r'D:\nioh3-re\ally-dump\shaderdump_172'
GAMEDIR = r'C:\Program Files (x86)\Steam\steamapps\common\Nioh3\shader_dx12'
G = os.path.join(ROOT, 'gpu-shaders')
HEX32 = re.compile(r'^[0-9a-f]{32}$')


def rd_csv(p):
    try:
        with open(p, newline='', encoding='utf-8', errors='replace') as f:
            return list(csv.DictReader(f))
    except OSError:
        return []


def containers(path):
    """yield (key, container_digest, bytes) for every DXBC container in a .file; key = HASH-part digest for DXIL else container digest"""
    try:
        d = open(path, 'rb').read()
        if d[:8] == b'IDRK0000':   # shader_dx12 container: zlib chunks (gpu-shaders\idrk.py)
            usize = struct.unpack_from('<Q', d, 0x18)[0]
            off, out = 0x38, bytearray()
            while len(out) < usize and off + 4 <= len(d):
                n = struct.unpack_from('<I', d, off)[0]; off += 4
                if n == 0:
                    break
                out += zlib.decompress(d[off:off + n]); off += n
            d = bytes(out)
    except (OSError, zlib.error, struct.error):
        return
    i = 0
    while True:
        i = d.find(b'DXBC', i)
        if i < 0:
            return
        if i + 36 > len(d):
            return
        size, ver, n = struct.unpack_from('<III', d, i + 24)
        if size < 36 or i + size > len(d) or n > 64:
            i += 4
            continue
        cd = d[i + 4:i + 20].hex()
        key = cd
        for c in range(n):
            o = struct.unpack_from('<I', d, i + 32 + 4 * c)[0]
            if o + 28 <= size and d[i + o:i + o + 4] == b'HASH':
                key = d[i + o + 12:i + o + 28].hex()
                break
        yield key, cd, d[i:i + size]
        i += size


def build_index():
    idx = {}   # digest -> dict(stage, loc, name, kind)

    def add(h, **kw):
        if h and HEX32.match(h):
            idx.setdefault(h, {}).update({k: v for k, v in kw.items() if v not in (None, '')})
    # shader_dx12 records (container and HASH digests)
    for r in rd_csv(os.path.join(G, 'dxil_index.csv')):
        loc = 'shader_dx12\\%s record@%s (%s B)' % (r['file'], r['dxbc_off'], r['dxbc_size'])
        for h in (r['hash_digest'], r['container_digest']):
            add(h, stage=r['kind'], loc=loc, name=r['group'], kind='disk', file=r['file'])
    # runtime inventory of the Ally dump (disk / exe / runtime)
    for r in rd_csv(os.path.join(G, 'post', 'out', 'runtime_inventory.csv')):
        src = r['source']
        kind = r['role'] if False else src.split(':')[0]
        name = src.split('/', 1)[1] if '/' in src else r.get('guess') or r.get('role') or ''
        if kind == 'disk':
            m = re.match(r'disk:(0x[0-9a-f]+\.file)', src)
            loc = 'shader_dx12\\' + m.group(1) if m else 'shader_dx12'
        elif kind == 'exe':
            loc = 'exe-embedded'
        elif kind == 'runtime':
            loc = 'runtime-compiled (dump)'
        else:
            continue
        for h in (r['digest'], r['hash']):
            e = idx.get(h)
            if e and e.get('kind') == 'disk' and kind == 'disk':
                e.setdefault('name', name)
                continue
            add(h, stage=r['stage'], loc=loc if not e else e.get('loc', loc), name=name or (e or {}).get('name'),
                kind=kind if not e else e.get('kind', kind), size=r['size'], role=r.get('role'), guess=r.get('guess'))
    # exe-embedded inventory
    for r in rd_csv(os.path.join(G, 'post', 'out', 'inventory.csv')):
        add(r['key'], stage=r['stage'], loc='exe-embedded @rva %s (%s)' % (r['rva'], r['section']), kind='exe', size=r['size'])
        add(r['digest'], stage=r['stage'], loc='exe-embedded @rva %s (%s)' % (r['rva'], r['section']), kind='exe', size=r['size'])
    # Ally dump file names (anything not yet known = runtime-compiled)
    dd = os.path.join(DUMP, 'dump')
    n_dump = 0
    if os.path.isdir(dd):
        for fn in os.listdir(dd):
            h, _, ext = fn.rpartition('.')
            n_dump += 1
            if h not in idx:
                add(h, stage='?', loc='runtime-compiled (dump)', kind='runtime', name='(not in inventory)')
            idx[h].setdefault('dump', ext)
    # psoswap/GP2 key = container digest (DXBC header bytes 4..20, psoswap.cpp ParseBlob), NOT only the HASH part:
    # scan the real bytes so each record is indexed under both digests (game shader_dx12 + Ally dump contents)
    if os.path.isdir(GAMEDIR):
        for fn in sorted(os.listdir(GAMEDIR)):
            if fn.endswith('.file'):
                for n, (k, cd, b) in enumerate(containers(os.path.join(GAMEDIR, fn))):
                    for h in (k, cd):
                        e = idx.get(h)
                        if not e or e.get('kind') != 'disk':
                            add(h, loc='shader_dx12\\%s container #%d (%d B)' % (fn, n, len(b)), kind='disk', file=fn)
    if os.path.isdir(dd):
        for fn in os.listdir(dd):
            for k, cd, b in containers(os.path.join(dd, fn)):
                for h in (k, cd):
                    if h not in idx:
                        add(h, stage='?', loc='runtime-compiled (dump %s)' % fn, kind='runtime')
    # psoswap logs: 'exe|ext <stage> <key> <size> B ...' and any 'PSO shader' lines
    for lg in glob.glob(os.path.join(ROOT, 'captures', 'ally', '*.log')):
        try:
            txt = open(lg, encoding='utf-8', errors='replace').read()
        except OSError:
            continue
        for m in re.finditer(r'\b(exe|ext) (ps|vs|gs|hs|ds|cs|ms|as) ([0-9a-f]{32}) (\d+) B ?(.*)', txt):
            add(m.group(3), stage=m.group(2), size=m.group(4), seen_in_log=os.path.basename(lg))
            if m.group(1) == 'exe':
                idx[m.group(3)].setdefault('loc', 'exe-embedded ' + m.group(5)[:40]); idx[m.group(3)].setdefault('kind', 'exe')
        for m in re.finditer(r'PSO shader[^\n]*?\b(ps|vs|gs|hs|ds|cs|ms|as)\b[ =:]+([0-9a-f]{32})', txt):
            add(m.group(2), stage=m.group(1), seen_in_log=os.path.basename(lg))
    # replacements
    reps = {}   # digest -> label
    labels = {}
    for ps1 in glob.glob(os.path.join(G, 'apply_*.ps1')):
        for m in re.finditer(r'^\s+(\w+)\s+shader_dx12\\(0x[0-9a-f]+\.file)\s+(.*)$', open(ps1, encoding='utf-8', errors='replace').read(), re.M):
            labels[m.group(2)] = '%s: %s' % (m.group(1), m.group(3).strip()[:70])
    for rp in glob.glob(os.path.join(G, 'replacement', '*.file')):
        fn = os.path.basename(rp)
        bk = os.path.join(G, 'backup', fn)
        new = {k: b for k, c, b in containers(rp)}
        old = {k: (c, b) for k, c, b in containers(bk)}
        chg_old = [(k, c, b) for k, (c, b) in old.items() if k not in new or new[k] != b]
        for k, c, b in chg_old:
            for h in (k, c):
                reps[h] = 'ORIGINAL replaced in shader_dx12\\%s (%s)' % (fn, labels.get(fn, 'rewrite'))
        oldb = {k: b for k, (c, b) in old.items()}
        newl = [(k, cd, b) for k, cd, b in containers(rp) if oldb.get(k) != b]
        for i, (k, cd, b) in enumerate(newl):   # pair new<->old by position when counts match
            orig = chg_old[i] if len(chg_old) == len(newl) else None
            oe = idx.get(orig[0], {}) if orig else {}
            for h in (k, cd):
                reps[h] = 'OUR REPLACEMENT in %s (%s)%s' % (fn, labels.get(fn, 'rewrite'), (' <- orig (positional guess) ' + orig[0]) if orig else '')
                add(h, stage=oe.get('stage'), loc='replacement\\%s container #%d (%d B)' % (fn, i, len(b)),
                    kind='replacement', name=oe.get('name') or 'replacement')
    for p in glob.glob(os.path.join(G, 'post', 'out', '*.dxbc')) + glob.glob(os.path.join(G, 'replacement', '*.dxil')) \
            + glob.glob(os.path.join(G, 'replacement', '*.dxbc')):
        h = os.path.basename(p).split('.')[0]
        reps[h] = 'psoswap file %s (ORIGINAL key it replaces)' % os.path.relpath(p, G)
        for k, cd, b in containers(p):
            for h2 in (k, cd):
                reps[h2] = 'OUR REPLACEMENT psoswap file %s' % os.path.relpath(p, G)
                add(h2, loc='replacement\\' + os.path.relpath(p, G), kind='replacement', name='post-v1 (replaces %s)' % h[:8])
    # RGP / notes mentions
    notes = {}
    for nf in glob.glob(os.path.join(ROOT, 'notes', '*.md')):
        try:
            for ln_no, ln in enumerate(open(nf, encoding='utf-8', errors='replace'), 1):
                for m in re.finditer(r'\b[0-9a-f]{32}\b', ln):
                    notes.setdefault(m.group(0), '%s:%d' % (os.path.basename(nf), ln_no))
        except OSError:
            pass
    out = {'built': 'gp2_digest', 'n_dump': n_dump, 'idx': idx, 'reps': reps, 'notes': notes}
    try:
        os.makedirs(os.path.dirname(CACHE), exist_ok=True)
        json.dump(out, open(CACHE, 'w'))
    except OSError as e:
        print('cache not written:', e, file=sys.stderr)
    return out


def load_index(rebuild):
    if not rebuild and os.path.exists(CACHE):
        try:
            return json.load(open(CACHE))
        except (OSError, ValueError):
            pass
    return build_index()


FRAME = re.compile(r'GP2: frame ([\d.]+) ms GPU span.*?covered ([\d.]+) ms.*?async-compute ([\d.]+) ms.*?PSO stamps (ON|OFF)')
ROW = re.compile(r'GP2:\s+(\d+) (.*?) \| +([\d.]+) ms \| +([\d.]+) \| +([\d.]+) \| \[(.*)\]\s*$')
TOK = re.compile(r'\b(ps|vs|gs|hs|ds|cs|ms|as)=([0-9a-f]{32}|f[0-9a-f]{16})')


def parse(paths, last_only):
    wins = []   # dict(span, covered, rows{(desc,marker):(ms,draws,ivals)})
    cur = None
    for p in paths:
        for ln in open(p, encoding='utf-8', errors='replace'):
            if 'GP2:' not in ln:
                continue
            m = FRAME.search(ln)
            if m:
                cur = {'span': float(m.group(1)), 'covered': float(m.group(2)), 'async': float(m.group(3)),
                       'on': m.group(4) == 'ON', 'rows': {}}
                wins.append(cur)
                continue
            m = ROW.search(ln)
            if m and cur is not None:
                cur['rows'][(m.group(2).strip(), m.group(6))] = (float(m.group(3)), float(m.group(4)), float(m.group(5)))
    wins = [w for w in wins if w['on']]
    if last_only and wins:
        wins = wins[-1:]
    return wins


def exe_passes():
    """scripts_gpu/exe_passes.tsv: curated names for exe-embedded / runtime shaders no index names (key = 8-hex digest prefix)"""
    out = {}
    try:
        for ln in open(os.path.join(os.path.dirname(os.path.abspath(__file__)), 'exe_passes.tsv'), encoding='utf-8'):
            f = ln.rstrip('\r\n').split('\t')
            if len(f) >= 2 and re.match(r'^[0-9a-f]{8}$', f[0]):
                out[f[0]] = f[1]
    except OSError:
        pass
    return out


EXE_PASSES = exe_passes()


def resolve(desc, I):
    idx, reps, notes = I['idx'], I['reps'], I['notes']
    stages, locs, names, rep, nt = [], [], [], [], []
    for st, h in TOK.findall(desc):
        stages.append(st)
        if h.startswith('f') and len(h) == 17:
            locs.append('%s fnv-key %s' % (st, h)); continue
        e = idx.get(h)
        if e:
            locs.append('%s %s' % (st, e.get('loc', '?')))
            if e.get('name') and e['name'] != '(not in inventory)':
                names.append('%s' % e['name'])
        else:
            locs.append('%s UNKNOWN (not in index)' % st)
        if h[:8] in EXE_PASSES and not (e and e.get('kind') == 'disk' and e.get('name')):
            names.append(EXE_PASSES[h[:8]])
        if h in reps:
            rep.append('%s: %s' % (st, reps[h]))
        if h in notes:
            nt.append('%s=%s' % (h[:8], notes[h]))
    return stages, locs, names, rep, nt


def digest(paths, out, top, rebuild, last):
    I = load_index(rebuild)
    wins = parse(paths, last)
    if not wins:
        sys.exit('no GP2 windows with PSO stamps ON found')
    n = len(wins)
    span = sum(w['span'] for w in wins) / n
    cov = sum(w['covered'] for w in wins) / n
    acc = {}
    for w in wins:
        for k, (ms, dr, iv) in w['rows'].items():
            a = acc.setdefault(k, [0.0, 0.0, 0.0])
            a[0] += ms; a[1] += dr; a[2] += iv
    rows = sorted(acc.items(), key=lambda kv: -kv[1][0])
    recs = []
    for rank, ((desc, marker), (ms, dr, iv)) in enumerate(rows[:top] if top else rows, 1):
        st, locs, names, rep, nt = resolve(desc, I)
        recs.append({'rank': rank, 'desc': desc, 'stage': '/'.join(st) or '-', 'where': '; '.join(locs) or '-',
                     'name': ' / '.join(dict.fromkeys(names)) or '-', 'pass': marker, 'ms': ms / n,
                     'pct': 100.0 * ms / n / span if span else 0.0, 'draws': dr / n, 'ivals': iv / n,
                     'replacement': '; '.join(rep) or 'no', 'notes': ' '.join(nt)})
    with open(out + '.csv', 'w', newline='', encoding='utf-8') as f:
        w = csv.DictWriter(f, fieldnames=list(recs[0].keys()))
        w.writeheader(); w.writerows(recs)
    tot = sum(r['ms'] for r in recs)
    L = ['# GP2 digest', '', '%d ON window(s); mean GPU span %.2f ms/frame, covered %.2f ms (%.0f%%); table = top %d, %.2f ms (%.0f%% of span)'
         % (n, span, cov, 100 * cov / span if span else 0, len(recs), tot, 100 * tot / span if span else 0), '',
         '| # | ms/frame | % frame | stage | pass/marker | where (shader_dx12 record / exe / runtime) | name | replacement | notes |',
         '|---|---|---|---|---|---|---|---|---|']
    for r in recs:
        L.append('| %d | %.3f | %.1f | %s | %s | %s | %s | %s | %s |' % (r['rank'], r['ms'], r['pct'], r['stage'], r['pass'],
                 r['where'].replace('|', '/'), r['name'].replace('|', '/'), r['replacement'].replace('|', '/'), r['notes']))
    L += ['', 'Per-PSO numbers are GPU-pipelined intervals (ranking, not isolated cost). Hashes:', '']
    for r in recs[:15]:
        L.append('%d. `%s`' % (r['rank'], r['desc']))
    open(out + '.md', 'w', encoding='utf-8').write('\n'.join(L) + '\n')
    print('\n'.join(L[:12 + min(len(recs), 15)]))
    print('\nwrote %s.md / %s.csv' % (out, out))


def selftest():
    import tempfile
    I = load_index(os.environ.get("GP2_REBUILD") == "1")
    idx = I['idx']
    dxil = [h for h, e in idx.items() if e.get('kind') == 'disk' and e.get('stage') == 'ps'][:1]
    exe = [h for h, e in idx.items() if e.get('kind') == 'exe'][:1]
    rt = [h for h, e in idx.items() if e.get('kind') == 'runtime'][:1]
    rp = list(I['reps'])[:1]
    hs = dxil + exe + rt + rp
    assert len(hs) >= 3, hs
    lines = ['[00:00:01.000] GP2: frame 16.50 ms GPU span (wall 17.00 ms); covered 14.00 ms (85 % of span); async-compute 0.50 ms; 900 intervals/frame, 700 draws+dispatches/frame, PSO stamps ON | dropped: cap 0, rec 0, unresolved 0, heap full 0',
             '[00:00:01.000] GP2: top 40 of 300 (PSO shader set, marker) buckets - hash stage(s) | ms/frame | draws+dispatches/frame | intervals/frame | [marker]',
             '[00:00:01.000] GP2:   1 vs=%s ps=%s |   3.210 ms |    40.0 |    41.0 | [GBuffer]' % (hs[0].replace(hs[0], '0' * 32), hs[0]),
             '[00:00:01.000] GP2:   2 cs=%s (async compute) |   2.000 ms |     1.0 |     2.0 | [-]' % hs[-1],
             '[00:00:01.000] GP2:   3 (copy / resolve / clear) |   1.000 ms |     5.0 |     5.0 | [Post]',
             '[00:00:01.000] GP2:   4 cs=%s |   0.500 ms |     1.0 |     1.0 | [DOF]' % hs[1 % len(hs)],
             '[00:00:02.000] GP2: frame 16.00 ms GPU span (wall 17.00 ms); covered 1.00 ms (6 % of span); async-compute 0.00 ms; 1 intervals/frame, 1 draws+dispatches/frame, PSO stamps OFF (A/B overhead window)',
             '[00:00:02.000] GP2:   1 cs=%s |   9.000 ms |     1.0 |     1.0 | [X]' % hs[0]]
    d = tempfile.mkdtemp(prefix='gp2t_')
    lp = os.path.join(d, 'synthetic.log')
    open(lp, 'w').write('\n'.join(lines) + '\n')
    digest([lp], os.path.join(d, 'out'), 40, False, False)
    c = list(csv.DictReader(open(os.path.join(d, 'out.csv'))))
    assert len(c) == 4 and abs(float(c[0]['ms']) - 3.21) < 1e-6 and 'UNKNOWN' not in c[0]['where'].split(';')[-1], c[0]
    assert abs(float(c[0]['pct']) - 100 * 3.21 / 16.5) < 0.01
    assert c[2]['stage'] == '-' and all(r['ms'] != '9.0' for r in c)
    print('SELFTEST OK')


if __name__ == '__main__':
    ap = argparse.ArgumentParser()
    ap.add_argument('logs', nargs='*')
    ap.add_argument('--out'); ap.add_argument('--top', type=int, default=0)
    ap.add_argument('--rebuild', action='store_true'); ap.add_argument('--last', action='store_true')
    ap.add_argument('--selftest', action='store_true')
    a = ap.parse_args()
    if a.selftest:
        selftest()
    elif a.logs:
        digest(a.logs, a.out or a.logs[0] + '.gp2', a.top, a.rebuild, a.last)
    else:
        ap.error('give a log or --selftest')
