"""progress_treemap.py - decompilation-progress treemaps (SVG) + badges for the README.

usage: py -I re\\scripts_re\\progress_treemap.py [--check]
  --check  print the numbers only, write nothing
Writes docs\\progress\\full_game.svg, performance.svg, badges.md (stdlib only).
Full game: every function start in .pdata of re\\Nioh3.dump.exe (chained/fragment entries merged), grouped by area
(inferred from symbols.tsv names; unnamed regions take the area of the nearest named function within 192 KB, else an
address-range block); block area = function bytes; each cell = N functions, colour = share with a real name.
Performance: functions in the Ally SP + PC CPU profiles (self time, perf_coverage.py weighting), block area = time share.
Green = real name, amber = Unknown_*, grey = unnamed.
"""
import bisect, collections, math, os, re, sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import perf_coverage as pc  # noqa: E402

ROOT = pc.ROOT
OUT = os.path.join(ROOT, 'docs', 'progress')

AREAS = [  # (area, regex on function name); first match wins
    ('Third-party (XeSS/FSR/Streamline)', r'Xess|Fsr|FidelityFX|Streamline|Nvapi|Dlss|Iat|Reflex'),
    ('Meshlet', r'Meshlet'),
    ('Shadow', r'Shadow|Esm'),
    ('Effects / Post', r'Effect|PostEffect|Particle|Vfx|Bloom|Tonemap|Ssao|Reflection'),
    ('UI', r'^Ui|Ui[A-Z]|Glyph|Quad|Hud|Menu|Font'),
    ('Jobs / KIDS scheduler', r'Job|KIDS|Sched|Worker|Task|Dispatch|Fiber'),
    ('GFX / D3D12 submission', r'^Syn|D3D|Gfx|Draw|Render|Present|Barrier|Pso|Cmd|Upload|Swapchain|Shader|Texture(?!Stream)|Wc|FrameGen'),
    ('Streaming / Resources', r'Stream|Resource|Rdb|Fdata|Load|Bitmap|Kod|Asset|File'),
    ('Collision / Physics', r'Collision|Physics|Raycast|Hit|Havok'),
    ('Animation', r'Anim|Skel|Pose|Bone|Motion'),
    ('Audio', r'Audio|Sound|Wwise|Voice|Sfx'),
    ('Mission / Game logic', r'Mission|Logic|Script|Schema|Enemy|Player|Camera|Quest|Save'),
    ('Memory', r'Alloc|Heap|Pool|Memory|Malloc'),
    ('Input', r'Input|Pad|Mouse'),
    ('Main loop / timing', r'Frame|WinMain|AppLoop|Sleep|Wfso|Qpc|Pause|Vrr|Fps|Timing|Wine'),
]
AREAS = [(a, re.compile(r)) for a, r in AREAS]
AUTO = pc.AUTO
NEAR = 192 * 1024


def area_of(name):
    for a, rx in AREAS:
        if rx.search(name):
            return a
    return None


def load_funcs():
    """-> sorted [(start, bytes)] merged through chained unwind info."""
    sz = collections.Counter()
    for s, e, u in pc.RF:
        sz[pc.primary(s)] += e - s
    return sorted(sz.items())


def load_syms():
    best = {}
    for l in open(pc.SYMS, encoding='utf-8'):
        f = l.rstrip('\n').split('\t')
        if len(f) > 2 and f[0].startswith('0x') and f[1] == 'function' and f[2]:
            best[int(f[0], 16)] = f[2]
    return best


def status(names, r):
    n = names.get(r)
    if not n or AUTO.match(n):
        return 0.0
    return 0.5 if n.startswith('Unknown_') else 1.0


def assign_areas(starts, names):
    anchors = []
    for r, n in names.items():
        a = None if AUTO.match(n) else area_of(n)
        if a:
            anchors.append((pc.fold(r), a))
    anchors.sort()
    ast = [r for r, _ in anchors]
    out = {}
    for s in starts:
        n = names.get(s)
        a = area_of(n) if n and not AUTO.match(n) else None
        if a is None and anchors:
            i = bisect.bisect_left(ast, s)
            c = [anchors[j] for j in (i - 1, i) if 0 <= j < len(anchors)]
            r, aa = min(c, key=lambda x: abs(x[0] - s))
            if abs(r - s) <= NEAR:
                a = aa
        out[s] = a or 'unclassified 0x%X-0x%XMB' % ((s >> 22) * 4, (s >> 22) * 4 + 4)
    return out


def squarify(items, x, y, w, h):
    """items [(key, area)] sorted desc, areas sum to w*h -> {key: (x,y,w,h)}"""
    res = {}
    items = [i for i in items if i[1] > 0]
    while items:
        short = min(w, h)
        row, tot, best, k = [], 0.0, float('inf'), 0
        while k < len(items):
            t2 = tot + items[k][1]
            worst = max(max(short * short * a / (t2 * t2), t2 * t2 / (short * short * a)) for _, a in row + [items[k]])
            if row and worst > best:
                break
            row.append(items[k]); tot = t2; best = worst; k += 1
        items = items[k:]
        if w >= h:
            cw = tot / h; cy = y
            for key, a in row:
                res[key] = (x, cy, cw, a / cw); cy += a / cw
            x += cw; w -= cw
        else:
            ch = tot / w; cx = x
            for key, a in row:
                res[key] = (cx, y, a / ch, ch); cx += a / ch
            y += ch; h -= ch
    return res


def colour(v):
    g, a, n = (46, 160, 67), (212, 160, 23), (68, 74, 84)
    if v >= 0.5:
        t = (v - 0.5) * 2; p, q = a, g
    else:
        t = v * 2; p, q = n, a
    return '#%02x%02x%02x' % tuple(int(p[i] + (q[i] - p[i]) * t) for i in range(3))


def grid(cells, x, y, w, h):
    n = len(cells)
    if n == 0 or w < 1 or h < 1:
        return []
    cols = max(1, min(n, round(math.sqrt(n * w / h))))
    rows = math.ceil(n / cols)
    cw, ch = w / cols, h / rows
    out = []
    for i, v in enumerate(cells):
        cx, cy = x + (i % cols) * cw, y + (i // cols) * ch
        out.append('<rect x="%.1f" y="%.1f" width="%.1f" height="%.1f" fill="%s"/>' % (
            cx, cy, max(cw - .6, .3), max(ch - .6, .3), colour(v)))
    return out


def svg(blocks, title, W=1600, H=900, legend=''):
    """blocks [(label, weight, [cell values], subtitle)]"""
    top = 34
    tot = sum(b[1] for b in blocks)
    order = sorted(enumerate(blocks), key=lambda t: -t[1][1])
    rects = squarify([(i, b[1] / tot * W * (H - top)) for i, b in order], 0, top, W, H - top)
    o = ['<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 %d %d" width="%d" height="%d" font-family="Segoe UI,Helvetica,Arial,sans-serif">' % (W, H, W, H),
         '<rect width="100%" height="100%" fill="#0d1117"/>',
         '<text x="10" y="22" fill="#e6edf3" font-size="15" font-weight="600">%s</text>' % title, legend]
    for i, (label, wgt, cells, sub) in enumerate(blocks):
        if i not in rects:
            continue
        x, y, w, h = rects[i]
        lab = w > 90 and h > 40
        hy = 17 if lab else 0
        o += grid(cells, x + 4, y + 4 + hy, w - 8, h - 8 - hy)
        if lab:
            t = label if len(label) * 7.2 < w - 10 else label[:max(int((w - 10) / 7.2) - 1, 3)] + '..'
            s = sub if len(t + sub) * 6.6 < w - 10 else ''
            o.append('<text x="%.1f" y="%.1f" fill="#e6edf3" font-size="12" font-weight="600">%s<tspan fill="#8b949e" font-weight="400"> %s</tspan></text>' % (
                x + 6, y + 16, t.replace('&', '&amp;'), s))
    o.append('</svg>')
    return '\n'.join(o)


def legend(W=1600):
    s, x = '', W - 330
    for t, v in (('named', 1.0), ('Unknown_* (role unclear)', 0.5), ('not named', 0.0)):
        s += '<rect x="%d" y="10" width="12" height="12" fill="%s"/><text x="%d" y="21" fill="#8b949e" font-size="11">%s</text>' % (x, colour(v), x + 16, t)
        x += 26 + len(t) * 6.2
    return s


def pct(x):
    return '%.2f%%' % (100 * x)


def main():
    check = '--check' in sys.argv
    names = load_syms()
    funcs = load_funcs()
    starts = [s for s, _ in funcs]
    areas = assign_areas(starts, names)
    nfun = len(funcs)
    score = {s: status(names, s) for s in starts}
    nm = sum(1 for v in score.values() if v == 1.0)
    unk = sum(1 for v in score.values() if v == 0.5)
    byt = sum(b for _, b in funcs)
    nbytes = sum(b for s, b in funcs if score[s] == 1.0)
    print('full game: %d functions, %.1f MB; named %d (%s), Unknown_ %d, bytes named %s' % (
        nfun, byt / 1e6, nm, pct(nm / nfun), unk, pct(nbytes / byt)))

    N = max(1, round(nfun / 5000))
    grp = collections.defaultdict(list)
    gbytes = collections.Counter()
    for s, b in funcs:
        grp[areas[s]].append(score[s]); gbytes[areas[s]] += b
    blocks = []
    for a, vals in grp.items():
        cells = [sum(vals[i:i + N]) / len(vals[i:i + N]) for i in range(0, len(vals), N)]
        named = sum(1 for v in vals if v == 1.0)
        blocks.append((a, gbytes[a], cells, '%d fn, %s named' % (len(vals), pct(named / len(vals)))))
        if check:
            print('  %-36s %6d fn %6.2f MB  named %d' % (a, len(vals), gbytes[a] / 1e6, named))
    print('cells: %d functions each, %d total' % (N, sum(len(b[2]) for b in blocks)))
    full = svg(blocks, 'Nioh3.exe: %d functions, %.1f MB of code; block = game area (area ~ code size), cell = %d functions' % (nfun, byt / 1e6, N), legend=legend())

    ally = pc.mean(pc.ally_shares(True))
    pcs = pc.mean(pc.pc_shares(True)[0])
    comb = collections.Counter()
    for k in set(ally) | set(pcs):
        comb[k] = (ally.get(k, 0) + pcs.get(k, 0)) / 2
    tot = sum(comb.values())
    pn = sum(v for k, v in comb.items() if status(names, k) == 1.0) / tot
    pu = sum(v for k, v in comb.items() if status(names, k) == 0.5) / tot
    gm = re.search(r'now (\d+)/(\d+)\)', open(os.path.join(ROOT, 'STATUS.md'), encoding='utf-8').read())
    gpu = '%s/%s' % gm.groups() if gm else 'n/a'
    print('perf: %d fns, CPU self time named %s, Unknown_ %s (named+Unknown_ %s), unnamed %s; GPU passes mapped %s' % (
        len(comb), pct(pn), pct(pu), pct(pn + pu), pct(1 - pn - pu), gpu))
    pareas = assign_areas(list(comb), names)
    pg = collections.defaultdict(list)
    for k, v in sorted(comb.items(), key=lambda kv: -kv[1]):
        pg[pareas[k]].append((v, status(names, k)))
    pblocks = []
    for a, lst in pg.items():
        t = sum(v for v, _ in lst)
        pblocks.append((a, t, [s for _, s in lst], '%s of time' % pct(t / tot)))
        if check:
            print('  %-36s %5d fn %6.2f%%' % (a, len(lst), 100 * t / tot))
    perf = svg(pblocks, 'Profiled CPU self time (ROG Ally + PC): %d functions; block area = time share, one cell per function (hottest first)' % len(comb), legend=legend())

    def badge(l, v, c):
        return '![%s](https://img.shields.io/badge/%s-%s-%s)' % (
            l, l.replace('-', '--').replace(' ', '%20'), v.replace('-', '--').replace('%', '%25').replace(' ', '%20'), c)
    badges = ' '.join([
        badge('functions named', '%.2f%% (%d/%d)' % (100 * nm / nfun, nm, nfun), 'orange'),
        badge('perf CPU time named', pct(pn), 'yellowgreen'),
        badge('perf incl. Unknown_', pct(pn + pu), 'green'),
        badge('GPU passes mapped', gpu, 'green')])
    if check:
        print(badges)
        return
    os.makedirs(OUT, exist_ok=True)
    for fn, s in (('full_game.svg', full), ('performance.svg', perf)):
        open(os.path.join(OUT, fn), 'w', encoding='utf-8').write(s)
        print(fn, len(s) // 1024, 'KB')
    open(os.path.join(OUT, 'badges.md'), 'w', encoding='utf-8').write(badges + '\n')


if __name__ == '__main__':
    main()
