"""fcap_digest.py - digest of FC frame captures (Nioh3EngineFix_fcap_<n>.csv, [Profiler] FrameCap; mod/Nioh3EngineFix/src/fcap.h).

usage: py -I re\\scripts_renderer\\fcap_digest.py <csv or glob> [...] [--device NAME] [--top N]

Per capture (markdown):
  1. Totals: records, presented frames in the window, list recordings, draws / dispatches / ExecuteIndirect / barriers / copies / clears.
  2. Queues: ExecuteCommandLists batches per queue type in submission order, with the recordings they submit, Signal / Wait fence ops
     (the cross-queue sync graph) and UpdateTileMappings calls.
  3. Passes: per marker path (BeginEvent stack, outer > inner, per list recording) the work it records, distinct PSO sets, the
     render-target sets (RTV / DSV handle values) and the barriers inside it, in first-seen order = recording order.
  4. Top PSO shader sets by draws + dispatches; resources by kind (render targets / depth / UAV textures / buffers) with size + format.
  5. Call sites: the Nioh3.exe return addresses (RVA) of the D3D12 calls per method = the dispatcher token handlers that issue them.
A capture spans FrameCap_Frames presents; recordings that started before the window show up as partial (no Reset row).
"""
import collections, csv, glob, os, sys

DRAW = {12: 'draw', 13: 'drawidx', 14: 'dispatch', 59: 'ei', 76: 'mesh'}
COPY = {15, 16, 17, 18, 19, 64}
CLEAR = {47, 48, 49, 50, 51}
QT = {0: 'DIRECT', 1: 'BUNDLE', 2: 'COMPUTE', 3: 'COPY'}
FMT = {2: 'RGBA32F', 10: 'RGBA16F', 11: 'RGBA16', 16: 'RG32F', 20: 'D32S8', 24: 'RGB10A2', 26: 'R11G11B10F', 28: 'RGBA8', 29: 'RGBA8_SRGB',
       34: 'RG16F', 35: 'RG16', 39: 'R32_TYPELESS', 40: 'D32F', 41: 'R32F', 42: 'R32U', 44: 'R24G8', 45: 'D24S8', 49: 'RG8', 54: 'R16F',
       56: 'R16', 57: 'R16U', 61: 'R8', 62: 'R8U', 71: 'BC1', 77: 'BC3', 80: 'BC4', 83: 'BC5', 87: 'BGRA8', 95: 'BC6H', 98: 'BC7', 0: '-'}
DIM = {1: 'buf', 2: 'tex1d', 3: 'tex2d', 4: 'tex3d'}


def h(x):
    return int(x, 16) if x else 0


def load(path):
    rows = []
    with open(path, encoding='utf-8', errors='replace') as f:
        lines = [ln for ln in f if not ln.startswith('#')]
    for r in csv.DictReader(lines):
        rows.append(r)
    return rows


def digest(path, top):
    rows = load(path)
    out = ['## %s' % os.path.basename(path), '']
    rec_of = {}            # list ptr -> current recording id
    recs = []              # recording dicts
    closed = {}            # list ptr -> last closed recording id
    batches = []           # (qtype, queue, [rec ids], ra)
    sync = []
    tot = collections.Counter()
    pso_use = collections.Counter()
    pso_desc = {}
    sites = collections.defaultdict(collections.Counter)
    res = {}
    passes = collections.OrderedDict()
    frames = 0
    cur_batch = None
    for r in rows:
        k = r['kind']
        slot = int(r['slot'] or 0)
        lst = r['list']
        if k == 'present':
            frames += 1
            continue
        if k == 'res':
            res[r['a0']] = (h(r['a1']), h(r['a2']), h(r['a3']), h(r['a4']) & 0xFFFF, h(r['a5']) & 0xFFFFFFFF, h(r['a5']) >> 32)
            continue
        if k in ('signal', 'wait', 'tilemap'):
            sync.append((k, QT.get(int(r['ltype']), r['ltype']), r['a0'][-6:], r['a1'][-6:], h(r['a2']), r['ra']))
            if k == 'tilemap':
                tot['tilemap'] += 1
            continue
        if k == 'ecl':
            idx, n = h(r['a1']), h(r['a2'])
            if idx == 0 or cur_batch is None:
                cur_batch = (QT.get(int(r['ltype']), r['ltype']), r['a0'][-6:], [], r['ra'])
                batches.append(cur_batch)
            cur_batch[2].append(closed.get(lst, -1))
            continue
        # call / barrier on a list
        if slot == 10 or lst not in rec_of:
            recs.append({'list': lst, 'type': QT.get(int(r['ltype']), r['ltype']), 'partial': slot != 10, 'c': collections.Counter(),
                         'stack': [], 'tid': r['tid']})
            rec_of[lst] = len(recs) - 1
        R = recs[rec_of[lst]]
        st = R['stack']
        if k == 'barrier':
            tot['barriers'] += 1
            R['c']['barriers'] += 1
        else:
            name = r['call']
            if r['ra'] != 'ffffffff':
                sites[name][r['ra']] += 1
            if slot == 57:
                st.append(r['info'] or '?')
            elif slot == 58 and st:
                st.pop()
            if slot in DRAW:
                tot[DRAW[slot]] += 1
                R['c'][DRAW[slot]] += 1
                if R.get('pso'):
                    pso_use[R['pso']] += 1
            elif slot in COPY:
                tot['copies'] += 1
                R['c']['copies'] += 1
            elif slot in CLEAR:
                tot['clears'] += 1
                R['c']['clears'] += 1
            elif slot == 25:
                R['pso'] = int(r['x'], 16) or r['a0']
                if r['info']:
                    pso_desc[R['pso']] = r['info']
            elif slot == 9:
                closed[lst] = rec_of[lst]
                tot['lists'] += 1
        path = ' > '.join(st[:3]) if st else '(no marker)'
        P = passes.setdefault((R['type'], path), {'c': collections.Counter(), 'pso': set(), 'rt': collections.Counter()})
        if k == 'barrier':
            P['c']['barriers'] += 1
        elif slot in DRAW:
            P['c'][DRAW[slot]] += 1
            if R.get('pso'):
                P['pso'].add(R['pso'])
        elif slot in COPY:
            P['c']['copies'] += 1
        elif slot in CLEAR:
            P['c']['clears'] += 1
        elif slot == 46:
            P['rt'][r['info']] += 1
    out.append('- records %d, presents in window %d, recordings %d (closed %d), %s' % (
        len(rows), frames, len(recs), tot['lists'], ', '.join('%s %d' % (a, tot[a]) for a in
                                                             ('draw', 'drawidx', 'dispatch', 'ei', 'mesh', 'barriers', 'copies', 'clears', 'tilemap'))))
    out += ['', '### Queues: ExecuteCommandLists in order', '', '| # | queue | queue ptr | lists | recordings (type: draws+disp+EI / barriers) | caller |', '|---|---|---|---|---|---|']
    for i, (qt, q, ids, ra) in enumerate(batches):
        desc = []
        for rid in ids[:6]:
            if rid < 0:
                desc.append('?')
                continue
            c = recs[rid]['c']
            desc.append('r%d:%d/%d' % (rid, c['draw'] + c['drawidx'] + c['dispatch'] + c['ei'] + c['mesh'], c['barriers']))
        out.append('| %d | %s | %s | %d | %s%s | %s |' % (i, qt, q, len(ids), ' '.join(desc), ' ...' if len(ids) > 6 else '', ra))
    out += ['', '### Fence ops / tile mappings (in order)', '', '| op | queue type | queue | fence or resource | value / regions | caller |', '|---|---|---|---|---|---|']
    for s in sync[:120]:
        out.append('| %s | %s | %s | %s | %d | %s |' % s)
    if len(sync) > 120:
        out.append('| ... %d more | | | | | |' % (len(sync) - 120))
    out += ['', '### Passes (marker path per queue type, recording order)', '',
            '| queue | marker path | draws | idx draws | dispatch | EI | barriers | copies | clears | PSO sets | RT sets (top) |', '|---|---|---|---|---|---|---|---|---|---|---|']
    for (qt, p), P in passes.items():
        c = P['c']
        if not sum(c.values()):
            continue
        rt = '; '.join('%s x%d' % (a.replace('rtv ', '').replace(' 0 0 0 0', ''), n) for a, n in P['rt'].most_common(2))
        out.append('| %s | %s | %d | %d | %d | %d | %d | %d | %d | %d | %s |' % (qt, p[:90], c['draw'], c['drawidx'], c['dispatch'], c['ei'],
                                                                         c['barriers'], c['copies'], c['clears'], len(P['pso']), rt[:120]))
    out += ['', '### Top PSO shader sets (draws + dispatches)', '', '| uses | set | shaders |', '|---|---|---|']
    for p, n in pso_use.most_common(top):
        out.append('| %d | %s | %s |' % (n, p, pso_desc.get(p, '')[:140]))
    out += ['', '### Resources touched (barriers / copies / clears / EI)', '']
    kinds = collections.Counter()
    rts = []
    for ptr, (dim, w, hh, mips, fmt, flags) in res.items():
        kind = DIM.get(dim, str(dim)) + ('-RT' if flags & 1 else '') + ('-DS' if flags & 2 else '') + ('-UAV' if flags & 4 else '')
        kinds[kind] += 1
        if flags & 3:
            rts.append((w * hh, '%dx%d %s mips %d %s' % (w, hh, FMT.get(fmt, fmt), mips, kind)))
    out.append('- ' + ', '.join('%s %d' % kv for kv in kinds.most_common()))
    out.append('- render targets / depth (largest 40): ' + '; '.join(d for _, d in sorted(rts, reverse=True)[:40]))
    out += ['', '### Call sites in Nioh3.exe (RVA of the return address) per method, top 4', '']
    for name, ctr in sorted(sites.items(), key=lambda kv: -sum(kv[1].values())):
        out.append('- %s (%d): %s' % (name, sum(ctr.values()), ', '.join('%s x%d' % kv for kv in ctr.most_common(4))))
    return '\n'.join(out)


def main():
    args = sys.argv[1:]
    top = 30
    dev = ''
    files = []
    i = 0
    while i < len(args):
        if args[i] == '--top':
            top = int(args[i + 1]); i += 2; continue
        if args[i] == '--device':
            dev = args[i + 1]; i += 2; continue
        files += sorted(glob.glob(args[i])) or [args[i]]
        i += 1
    print('# FC frame-capture digest%s' % (' (%s)' % dev if dev else ''))
    print()
    for f in files:
        print(digest(f, top))
        print()


if __name__ == '__main__':
    main()
