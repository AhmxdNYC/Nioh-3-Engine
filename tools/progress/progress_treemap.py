"""progress_treemap.py - decompilation-progress treemaps (SVG) + badges for the README.

usage: py -I tools\\progress\\progress_treemap.py [--check] [--dump PATH] [--out DIR]
  --check  print the numbers only, write nothing
Writes docs\\progress\\full_game.svg, performance.svg, gpu.svg, badges.md (stdlib only).
Full game: every function start in .pdata of your game dump (chained/fragment entries merged), grouped by purpose
region from data\\regions.tsv (regions.py, docs\\code-map.md); fallback if that file is missing: area inferred from symbols.tsv
names (unnamed code takes the area of the nearest named function within 192 KB, else an address-range block); block area = function bytes; each cell = N functions, colour = share with a real name.
Performance: functions in the Ally SP + PC CPU profiles (self time, perf_coverage.py weighting), block area = time share.
Green = real name, amber = Unknown_*, grey = unnamed.
GPU: the per-PSO GP2 capture in data\\profiles\\gpu (gp2_digest.py).
"""
import bisect, collections, fnmatch, math, os, re, sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import common as C  # noqa: E402
import perf_coverage as pc  # noqa: E402

ROOT = pc.ROOT
OUT = C.OUT

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
# Explicit name-prefix rules, checked BEFORE the keyword list (generic leaves must not inherit a neighbour's area).
PREFIX = [
    ('Math / CRT / containers', r'^(Vec\d|Mtx|Matrix|Quat|Aabb|Obb|TransformedBox|SinCos|Crt|_?(Memcpy|Memset|Memmove)|Sort|IntroSort|BTree|RbTree|Rb[A-Z]|Hash|Deque|Dequeue|LowerBound|SortedEntries|SetAssoc|Fnv|Cnd|Mtx[A-Z]|Xtime|Atan|RollingAvg)'),
    ('Object / scene update', r'^(Obj|Placeable|Stage|BgThread|Bounds|ParamTable|GlobalEntity|Entity)'),
    ('Audio', r'^(Audio|Sound|Voice|ConvReverb|Fft|Reverb|Biquad|Xaudio)'),
]
PREFIX = [(a, re.compile(r)) for a, r in PREFIX]
AREAS = [(a, re.compile(r)) for a, r in AREAS]
AUTO = pc.AUTO
NEAR = 192 * 1024
NEAR_ONE = 64 * 1024  # an unnamed function takes ONE neighbour's area only if it is this close;
                      # farther away both neighbours must agree.


def load_overrides():
    """area_overrides.tsv: <0xRVA | name-glob>	<area>; applied before every other rule."""
    rv, gl = {}, []
    f = C.data('area_overrides.tsv')
    if os.path.exists(f):
        for l in open(f, encoding='utf-8'):
            q = l.rstrip(chr(10)).split(chr(9))
            if len(q) < 2 or q[0].startswith('#') or q[0] == 'key':
                continue
            if q[0].lower().startswith('0x'):
                rv[int(q[0], 16)] = q[1]
            else:
                gl.append((q[0], q[1]))
    return rv, gl


OVR_RVA, OVR_GLOB = load_overrides()


PLAIN = {  # area -> 'technical name / plain name'
    'Third-party (XeSS/FSR/Streamline)': 'XeSS·FSR·Streamline / 3rd-party upscalers',
    'Meshlet': 'Meshlet / geometry streaming', 'Shadow': 'Shadow·ESM / shadows',
    'Effects / Post': 'PostEffect / effects & post', 'UI': 'UiQuad·Glyph / user interface',
    'Jobs / KIDS scheduler': 'KIDS / job system', 'GFX / D3D12 submission': 'SynCtx·D3D12 / GPU submission',
    'Streaming / Resources': 'Stream·RDB / resource streaming', 'Collision / Physics': 'Collision / physics',
    'Animation': 'Anim / animation', 'Audio': 'XAudio2 mixer / audio', 'Mission / Game logic': 'Mission·Schema / game logic',
    'Memory': 'Alloc·Pool / memory', 'Input': 'Input / controls', 'Main loop / timing': 'WinMain·FrameEnd / main loop & timing',
    'Math / CRT / containers': 'Vec·Aabb·CRT / math & container leaves', 'Object / scene update': 'Obj·Placeable·Stage / object & scene update',
}
GPU_PLAIN = {
    'FSR3': 'FFX_FSR3 / upscaler', 'Copies / clears': 'copies/clears / data copies', 'GI / RefProbe': 'RefProbe / lighting (GI)',
    'Fog / volumetrics': 'VFSCAT / fog', 'Sky / clouds': 'VCLOUD / sky & clouds', 'Shadows (WDO/ESM/GHDM)': 'WDO·ESM·GHDM / shadows',
    'Culling (MPC/MshLod)': 'MPC·MshLod / culling', 'Trees / foliage': 'PB2TREE2 / trees', 'Materials / G-buffer (PB2)': 'PB2 / materials',
    'UI / sprites / effect merge': 'sprite quads / UI & effect merge',
    'Post (blur/DOF/bloom/compose)': 'TOONEDGE2·DOF / post & outlines', 'Other': 'other / unattributed',
}


def plain_label(a, m=PLAIN):
    if a in m:
        return m[a]
    mm = re.match(r'unclassified (0x\w+-0x\w+MB)', a)
    return 'text %s / unexplored code' % mm.group(1) if mm else a


def short_desc(d, n=40):
    d = re.split(r'[;(\[]|, | \| ', d.strip())[0].strip().rstrip('.:')
    return d if len(d) <= n else d[:n - 1].rstrip() + '…'


def area_of(name, rva=None):
    if rva in OVR_RVA:
        return OVR_RVA[rva]
    for g, a in OVR_GLOB:
        if fnmatch.fnmatchcase(name, g):
            return a
    for a, rx in PREFIX + AREAS:
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
            r = pc.fold(int(f[0], 16))   # fold to the .pdata function start so rows inside a function/fragment count
            if r not in best or (AUTO.match(best[r]) and not AUTO.match(f[2])):
                best[r] = f[2]
    return best


def load_desc():
    out = {}
    for l in open(pc.SYMS, encoding='utf-8'):
        f = l.rstrip('\n').split('\t')
        if len(f) > 4 and f[0].startswith('0x') and f[1] == 'function' and f[4]:
            out[int(f[0], 16)] = f[4]
    return out


def status(names, r):
    n = names.get(r)
    if not n or AUTO.match(n):
        return 0.0
    return 0.5 if n.startswith('Unknown_') else 1.0


def assign_areas(starts, names):
    anchors = []
    for r, n in names.items():
        a = None if AUTO.match(n) else area_of(n, r)
        if a:
            anchors.append((pc.fold(r), a))
    anchors.sort()
    ast = [r for r, _ in anchors]
    out = {}
    for s in starts:
        n = names.get(s)
        a = area_of(n, s) if n and not AUTO.match(n) else OVR_RVA.get(s)
        if a is None and anchors:
            i = bisect.bisect_left(ast, s)
            c = [anchors[j] for j in (i - 1, i) if 0 <= j < len(anchors)]
            r, aa = min(c, key=lambda x: abs(x[0] - s))
            d = abs(r - s)
            if d <= NEAR_ONE or (d <= NEAR and all(x[1] == aa for x in c)):
                a = aa
        out[s] = a or 'unclassified 0x%X-0x%XMB' % ((s >> 22) * 4, (s >> 22) * 4 + 4)
    return out


REGIONS = C.data('regions.tsv')


def load_regions():
    """regions.tsv (regions.py): sorted [(start, label, confidence)] or None when the file is missing"""
    if not os.path.exists(REGIONS):
        return None
    out = []
    for l in open(REGIONS, encoding='utf-8'):
        f = l.rstrip(chr(10)).split(chr(9))
        if len(f) > 3 and f[0].startswith('0x'):
            out.append((int(f[0], 16), f[2], f[3]))
    return sorted(out) or None


def region_areas(starts, regs):
    """function start -> purpose label of the regions.tsv row holding it"""
    rs = [r[0] for r in regs]
    return {s: regs[max(0, bisect.bisect_right(rs, s) - 1)][1] for s in starts}


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
    if isinstance(v, str):
        return v
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


def tailcol(tail, tw):
    if any(isinstance(c[1], str) for c in tail):
        w = collections.Counter()
        for c in tail:
            w[c[1]] += c[0]
        return w.most_common(1)[0][0]  # colour of the dominant category
    return sum(c[0] * c[1] for c in tail) / tw


def ptext(x, y, s, fs, fill, fw=600, sw=3):
    s = s.replace('&', '&amp;').replace('<', '&lt;')
    return ('<text x="%.1f" y="%.1f" fill="%s" font-size="%s" font-weight="%d" stroke="#0d1117" stroke-width="%s" '
            'stroke-linejoin="round" paint-order="stroke">%s</text>' % (x, y, fill, fs, fw, sw, s))


def wcells(cells, x, y, w, h, total, minfrac=0.0002):
    """cells [(weight, value)] hottest first -> squarified rects; the tail below 0.02 % of total is merged into one cell."""
    keep = [c for c in cells if c[0] >= minfrac * total]
    tail = [c for c in cells if c[0] < minfrac * total]
    if tail:
        tw = sum(c[0] for c in tail)
        keep.append((tw, tailcol(tail, tw), '%d tiny functions' % len(tail)))
    if w < 1 or h < 1 or not keep:
        return []
    s = sum(c[0] for c in keep)
    rects = squarify([(i, c[0] / s * w * h) for i, c in enumerate(keep)], x, y, w, h)
    out = []
    for i, (cx, cy, cw, ch) in rects.items():
        c = keep[i]
        out.append('<rect x="%.1f" y="%.1f" width="%.1f" height="%.1f" fill="%s"%s'
                   % (cx, cy, max(cw - .8, .3), max(ch - .8, .3), colour(c[1]),
                      '><title>%s</title></rect>' % c[2] if len(c) > 2 else '/>'))
        lab = c[3] if len(c) > 3 else None
        if isinstance(lab, tuple) and lab[0] and i < 3 and cw >= 110 and ch >= 30:
            fs = 13
            main, sub = lab
            words, cap = main.split(' '), int((cw - 10) / (fs * 0.58))
            while words and len(' '.join(words)) > cap:
                words.pop()
            main = ' '.join(words)
            if len(main) >= 5:
                out.append(ptext(cx + 5, cy + 16, main, fs, '#fff', 600))
                if sub and ch >= 46 and len(sub) * 11 * 0.58 <= cw - 10:
                    out.append(ptext(cx + 5, cy + 31, sub, 11, '#fff', 600, 2.5))
    return out


SHORT = {'user interface': 'UI', 'animation': 'anim', 'resource streaming': 'streaming', 'main loop & timing': 'main loop',
         'controls': 'input', '3rd-party upscalers': '3rd party', 'geometry streaming': 'meshlet', 'effects & post': 'fx & post',
         'GPU submission': 'GPU', 'sky & clouds': 'sky', 'data copies': 'copies', 'post & outlines': 'post', 'unattributed': 'other',
         'job system': 'jobs', 'game logic': 'logic', 'unexplored code': 'unexplored', 'materials': 'PB2',
         # regions.tsv purpose labels (regions.py)
         'compiled animation rigs': 'anim rigs', 'menu screens & item text': 'menu text', 'object & scene update': 'objects',
         'sky, weather & water': 'sky & water', 'game logic scripting': 'scripting', 'geometry & culling': 'culling',
         'game rules & logic': 'game rules', 'materials & render settings': 'materials', 'co-op gameplay sync': 'co-op',
         'job system & threads': 'jobs', 'sound engine & mixer': 'audio', 'online & matchmaking': 'online',
         'visual effects': 'VFX', 'physics & collision': 'physics', 'post-processing': 'post', 'characters & combat': 'combat',
         'menus & HUD': 'HUD', 'saving & config': 'saves', 'C runtime & std library': 'CRT', 'app, window & OS': 'app/OS',
         'upscalers & frame-gen': 'upscalers', 'resource loading': 'loading', 'strings, text codecs & serializer': 'strings',
         'enemy & NPC AI': 'AI', 'math & containers': 'math', 'shader compile & PSOs': 'shaders', 'video playback': 'video',
         'Steam platform': 'Steam', 'decompression': 'zlib', 'dev & debug tools': 'debug', 'camera control': 'camera',
         'Ogg audio decoding': 'vorbis', 'crypto & checksums': 'crypto', 'photo mode': 'photo', 'no evidence': 'unknown'}


def svg(blocks, title, W=1000, H=620, legend='', minfrac=0.0002):
    """blocks [(label, weight, [cell values], subtitle)]; designed for ~880 px README width"""
    top = 46
    tot = sum(b[1] for b in blocks)
    order = sorted(enumerate(blocks), key=lambda t: -t[1][1])
    rects = squarify([(i, b[1] / tot * W * (H - top)) for i, b in order], 0, top, W, H - top)
    o = ['<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 %d %d" style="width:100%%;height:auto" font-family="Segoe UI,Helvetica,Arial,sans-serif">' % (W, H),
         '<rect width="100%" height="100%" fill="#0d1117"/>',
         '<text x="8" y="19" fill="#e6edf3" font-size="15" font-weight="700">%s</text>' % title, legend]
    for i, (label, wgt, cells, sub) in enumerate(blocks):
        if i not in rects:
            continue
        x, y, w, h = rects[i]
        cw_ = lambda s: len(s) * fs * 0.6
        plain = label.split(' / ')[0 if label.startswith('text ') else -1]
        cands = [(label, 15), (plain, 15), (SHORT.get(plain, plain), 15), (plain, 12), (SHORT.get(plain, plain), 12),
                 (SHORT.get(plain, plain), 10)]
        t, fs = next(((s, f) for s, f in cands if len(s) * f * 0.6 <= w - 12), (None, 15))
        lab = t is not None and w >= 30 and h > 24
        hy = 22 if lab else 0
        if cells and isinstance(cells[0], tuple):
            o += wcells(cells, x + 3, y + 3 + hy, w - 6, h - 6 - hy, tot, minfrac)
        else:
            o += grid(cells, x + 3, y + 3 + hy, w - 6, h - 6 - hy)
        if lab:
            o.append(ptext(x + 6, y + 19, t, fs, '#fff', 700))
            if sub and cw_(t + ' ' + sub) * 0.8 <= w - 12:
                o.append(ptext(x + 6 + cw_(t) + 6, y + 19, sub, 12, '#c9d1d9', 400))
    o.append('</svg>')
    return '\n'.join(o)


def legend(W=1000, items=(('named', 1.0), ('Unknown_* (role unclear)', 0.5), ('not named', 0.0)), x0=0):
    s, x = '', 8
    for t, v in items:
        s += '<rect x="%d" y="28" width="12" height="12" fill="%s"/><text x="%d" y="39" fill="#c9d1d9" font-size="12">%s</text>' % (x, colour(v), x + 17, t)
        x += 32 + len(t) * 6.6
    return s


GPU_GROUPS = [  # (group, regex on 'marker | shader names'); first match wins
    ('FSR3', r'FSR3|FFX_'),
    ('Copies / clears', r'^\(copy|copy / resolve|CopyMRLR|VideoOut|ExeCopy_'),
    ('GI / RefProbe', r'RefProbe|SDFOCC|LPV'),
    ('Fog / volumetrics', r'VFSCAT|VFOG|Fog|Volum'),
    ('Sky / clouds', r'VCLOUD|DrawSky|Sky|Cloud'),
    ('Shadows (WDO/ESM/GHDM)', r'WDO|ESM|GHDM|Shadow|Cascade'),
    ('Culling (MPC/MshLod)', r'MPC|MshLod|MeshletPre|Preculling'),
    ('Trees / foliage', r'TREE|FOLIAGE|GRASS'),
    ('Materials / G-buffer (PB2)', r'PB2|Ultramarine|GBuffer|ExeTess_'),
    ('UI / sprites / effect merge', r'ExeUI_|EffectMerge'),
    ('Post (blur/DOF/bloom/compose)', r'DOF|Bloom|Compose|Post|ExePost_|Blur|TOONEDGE|Tonemap|motion blur'),
]
GPU_GROUPS = [(g, re.compile(r)) for g, r in GPU_GROUPS]
G_GREEN, G_BLUE, G_RED, G_GREY = '#2ea043', '#388bfd', '#e5534b', '#444a54'
DROPPED = re.compile(r'gi:|fog:|toonedge|VFSCAT.*replac', re.I)
KEPT_NAME = re.compile(r'MPC|MshLod|GHDM|ESM|WDO28', re.I)


def gpu_data(minpass=None):
    """-> (span, [(group, ms, colour, title)]) from the Ally GP2 capture (all-original shaders)"""
    import gp2_digest as g
    log = g.LOG
    I = g.load_index(False)
    wins = g.parse([log], False)
    acc = collections.defaultdict(float)
    for w in wins:
        for k, (ms, _, _) in w['rows'].items():
            acc[k] += ms / len(wins)
    span = sum(w['span'] for w in wins) / len(wins)
    rows = []
    for (desc, marker), ms in acc.items():
        st, locs, names, rep, nt = g.resolve(desc, I)
        txt = marker + ' | ' + desc[:12] + ' | ' + ' '.join(names)
        grp = next((n for n, rx in GPU_GROUPS if rx.search(txt)), 'Other')
        located = bool(locs) and not any('UNKNOWN' in l for l in locs)
        copy = desc.startswith('(')
        role = bool(names) or marker != '-' or copy
        rs = ' '.join(rep)
        if copy:
            col = G_GREEN
        elif not (located and role and not (grp == 'Other' and not names)):
            col = G_GREY
        elif DROPPED.search(rs):
            col = G_RED
        elif rep or KEPT_NAME.search(' '.join(names) + marker):
            col = G_BLUE
        else:
            col = G_GREEN
        mk = marker.replace('FFX_FSR3UPSCALER_PASS_', '')
        base = names[0].split('-')[0] if names and not mk.startswith(('FFX_', 'FSR')) and grp != 'FSR3' else ''
        nm = mk if grp == 'FSR3' else (base or (mk if mk != '-' else desc[:10]))
        lbl = (nm.replace('FSR3:', '').replace('FFX_', '').replace('_', ' ').lower(), GPU_PLAIN[grp].split(' / ')[1] if grp in GPU_PLAIN else '')
        title = '%.2f ms: %s %s' % (ms, marker if marker != '-' else '', ' / '.join(dict.fromkeys(names)) or desc[:40])
        rows.append((grp, ms, col, title.replace('&', '&amp;').replace('<', '&lt;'), lbl))
    return span, os.path.basename(log), rows


def pct(x):
    return '%.2f%%' % (100 * x)


def main():
    check = '--check' in sys.argv
    names = load_syms()
    funcs = load_funcs()
    starts = [s for s, _ in funcs]
    regs = load_regions()
    areas = region_areas(starts, regs) if regs else assign_areas(starts, names)   # fallback: name-keyword areas
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
        blocks.append((plain_label(a), gbytes[a], cells, '%d fn, %s named' % (len(vals), pct(named / len(vals)))))
        if check:
            print('  %-36s %6d fn %6.2f MB  named %d' % (a, len(vals), gbytes[a] / 1e6, named))
    print('cells: %d functions each, %d total' % (N, sum(len(b[2]) for b in blocks)))
    what = 'block = purpose region (regions.tsv)' if regs else 'block = game area'
    full = svg(blocks, 'Nioh3.exe: %d functions, %.1f MB of code; %s (area ~ code size), cell = %d functions' % (nfun, byt / 1e6, what, N), legend=legend())

    ally = pc.mean(pc.ally_shares(True))
    pcs = pc.mean(pc.pc_shares(True)[0])
    comb = collections.Counter()
    for k in set(ally) | set(pcs):
        comb[k] = (ally.get(k, 0) + pcs.get(k, 0)) / 2
    tot = sum(comb.values())
    pn = sum(v for k, v in comb.items() if status(names, k) == 1.0) / tot
    pu = sum(v for k, v in comb.items() if status(names, k) == 0.5) / tot
    gpu = C.meta().get('gpu_passes_mapped', 'n/a')   # GPU passes whose engine code path is named (data/progress_meta.tsv)
    print('perf: %d fns, CPU self time named %s, Unknown_ %s (named+Unknown_ %s), unnamed %s; GPU passes mapped %s' % (
        len(comb), pct(pn), pct(pu), pct(pn + pu), pct(1 - pn - pu), gpu))
    pareas = assign_areas(list(comb), names)
    desc = load_desc()
    pg = collections.defaultdict(list)
    for k, v in sorted(comb.items(), key=lambda kv: -kv[1]):
        st = status(names, k)
        fnm = names.get(k, '')
        if st:
            lbl = (short_desc(desc.get(k, ''), 28).lower() if desc.get(k) else '', fnm)
            ttl = '%s: %s (%.3f%%)' % (fnm, desc.get(k, '')[:160], 100 * v / tot)
        else:
            lbl = ('', '')
            ttl = 'FUN_%X not named (%.3f%%)' % (k, 100 * v / tot)
        pg[pareas[k]].append((v, st, ttl.replace('&', '&amp;').replace('<', '&lt;'), lbl))
    pblocks = []
    for a, lst in pg.items():
        t = sum(c[0] for c in lst)
        pblocks.append((plain_label(a), t, lst, '%s of time' % pct(t / tot)))
        if check:
            print('  %-36s %5d fn %6.2f%%' % (a, len(lst), 100 * t / tot))
    perf = svg(pblocks, 'Profiled CPU self time (ROG Ally + PC): %d functions; block area = time share, cell area = function time (tiny ones merged)' % len(comb), legend=legend())

    span, glog, grows = gpu_data()
    gtot = sum(r[1] for r in grows)
    gid = sum(r[1] for r in grows if r[2] != G_GREY) / gtot
    gpa = collections.defaultdict(list)
    for grp, ms, col, ti, lbl in sorted(grows, key=lambda r: -r[1]):
        gpa[grp].append((ms, col, ti, lbl))
    gblocks = [(plain_label(grp, GPU_PLAIN), sum(c[0] for c in lst), lst, '%.2f ms' % sum(c[0] for c in lst)) for grp, lst in gpa.items()]
    print('gpu (%s): top-%d PSO buckets %.2f ms of %.1f ms span; identified %s; blue %s red %s' % (
        glog, len(grows), gtot, span, pct(gid), pct(sum(r[1] for r in grows if r[2] == G_BLUE) / gtot),
        pct(sum(r[1] for r in grows if r[2] == G_RED) / gtot)))
    if check:
        for b in sorted(gblocks, key=lambda b: -b[1]):
            print('  %-40s %6.2f ms' % (b[0], b[1]))
    gleg = legend(items=(('identified', G_GREEN), ('identified + faster replacement (private tests, not shipped)', G_BLUE), ('rewrite tried, dropped', G_RED), ('not identified', G_GREY)), x0=620)
    gpusvg = svg(gblocks, 'GPU time on the ROG Ally (one capture, preset not recorded): top-%d shader buckets, %.1f ms of a %.1f ms frame' % (
        len(grows), gtot, span),
                 legend=gleg, minfrac=0.004)

    def badge(l, v, c):
        return '![%s](https://img.shields.io/badge/%s-%s-%s)' % (
            l, l.replace('-', '--').replace(' ', '%20'), v.replace('-', '--').replace('%', '%25').replace(' ', '%20'), c)
    badges = ' '.join([
        badge('functions named', '%.2f%% (%d/%d)' % (100 * nm / nfun, nm, nfun), 'orange'),
        badge('perf CPU time named', pct(pn), 'brightgreen' if pct(pn) == '100.00%' else 'yellowgreen'),
        badge('perf incl. Unknown_', pct(pn + pu), 'green'),
        badge('GPU time identified', pct(gid), 'green'),
        badge('GPU passes mapped', gpu, 'green')])
    if check:
        print(badges)
        return
    os.makedirs(OUT, exist_ok=True)
    for fn, s in (('full_game.svg', full), ('performance.svg', perf), ('gpu.svg', gpusvg)):
        open(os.path.join(OUT, fn), 'w', encoding='utf-8').write(s)
        print(fn, len(s) // 1024, 'KB')
    open(os.path.join(OUT, 'badges.md'), 'w', encoding='utf-8').write(badges + '\n')


if __name__ == '__main__':
    main()
