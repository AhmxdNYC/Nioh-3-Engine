"""render_map.py - Phase 0 module map of the Nioh 3 rendering engine (offline, no Ghidra).

usage: py -I re\\scripts_renderer\\render_map.py [--md] [--partition out.tsv] [--sub NAME --list N]

What it does
  1. Functions, direct call edges (E8/E9 rel32 to a .pdata start) and ctor->vtable-slot edges come from
     re\\scripts_re\\regions.py (same scans as docs/code-map.md).
  2. Seeds: every symbols.tsv function whose real name matches a subsystem regex (SUBS below, first match wins), plus the
     hand-picked ENTRY RVAs from the notes.
  3. Closure: multi-source BFS from the seeds over call + vtable edges, depth <= DEPTH, a function goes to the subsystem whose
     seed is nearest (ties: SUBS order). The walk does not enter hubs (in-degree >= HUB), service code (CRT/math/memory/jobs/
     ktul/zlib regions) or non-render regions (gameplay, audio, net, ...): those edges are counted as dependencies.
  4. Functions in a render region (regions.tsv label in RENDER_LABELS) that no walk reached are added by region label.
  5. Per subsystem: functions, code bytes, real names / auto names (RTTI vfunc_N, ctor, Unknown_, *Hot_) / unnamed, functions
     in the D:\\nioh3-re\\decomp export, functions cited in notes (hex RVA token that is a function start), top notes, and
     outgoing dependency edges by region label.
Caveat: indirect calls through runtime-filled tables (render-node Execute via the node vtable, KTGL token handlers) are only
seen when a ctor/lea of the vtable exists; counts are a lower bound for subsystems reached mostly through vtables.
"""
import collections, glob, os, re, sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, '..', '..'))
sys.path.insert(0, os.path.join(ROOT, 're', 'scripts_re'))
import numpy as np  # noqa: E402
import regions as R  # noqa: E402

DEPTH = 4
HUB = 60
# (key, title, name regex). Order = priority when a name matches several.
SUBS = [
    ('PRESENT', 'Frame pacing / present / swapchain',
     r'Present|Swap[Cc]hain|FrameLatency|Reflex|Xell|DynFps|FrameTiming|Vsync|Vrr|PclMarker|SlPcl|LatencyWait'),
    ('POST', 'Post-processing & upscalers (FSR3 / XeSS / DLSS / FG)',
     r'Fsr|Ffx|Xess|Dlss|^Sl[A-Z]|Streamline|PostEffect|Bloom|Tonemap|Taa[A-Z]|Dof[A-Z]|MotionBlur|Exposure|HdrOutput|Rcas|Upscal|FrameGen|Fg[A-Z]'),
    ('UI', 'UI / OSD / screen layout rendering',
     r'^Ui|Ui[A-Z]|Glyph|Font|ScreenLayout|^Scl|Hud[A-Z]|^Osd|Movie|Fade'),
    ('VOLNAT', 'Volumetrics, sky, weather & water',
     r'Fog|Cloud|Sky|Volumetric|Vcloud|Vfscat|Ocean|UltraMarine|Nature|RainDrop|Snow|Water|Weather|Wind'),
    ('LIGHT', 'Lighting, shadows, GI, reflections, AO',
     r'Vsm|Shadow|Cascade|Esm[A-Z]|RefProbe|ReflectionProbe|Probe|^Gi[A-Z]|Gi[A-Z][a-z]|Lpv|Oboro|Light|Ssao|Hbao|Ssr|Rlr|Deferred|Sdf|Denois|Occlusion'),
    ('FX', 'Effects / particles / decals',
     r'Effect|Particle|Vfx|Trail|Decal|G1e'),
    ('SCENE', 'Scene representation & visibility (meshlets, culling, LOD, draw lists)',
     r'Meshlet|Cull|Lod[A-Z]|Lod$|HiZ|Hiz|DrawList|DrawEmit|DrawSubmit|DrawState|SceneRender|Visib|G1m|Skinning|Terrain|StageSlot|ObjEntry'),
    ('PSO', 'PSO / shader / material system',
     r'Pso|Shader|Material|Technique|Pb2|RootSig|Pipeline|Sampler|Dxc|D3DCompile|ShaderConst'),
    ('RESOURCE', 'GPU resources & memory (heaps, rings, residency, streaming, RTs)',
     r'Upload|Heap|Residen|TileMap|Tiles|Texture|Tex[A-Z]|GpuBuf|Vram|Rebar|Unmap|Placed|Committed|RenderTarget|Wc[A-Z]|Streaming|Mip'),
    ('BACKEND', 'D3D12 backend (device, queues, KTGL token stream, dispatcher replay, barriers, fences, descriptors)',
     r'^Syn|Ecl|Dispatcher|D3d|D3D|^Gfx|Ktgl|Queue|Fence|Barrier|Descriptor|Cbv|Srv|Uav|CommandList|CmdList|CmdStream|ExecuteCommand|Device|AsyncSlot|Tok[A-Z0-9]'),
    ('FRAME', 'Frame / render-graph build & pass scheduling',
     r'RenderGraph|RenderNode|RenderFrame|RenderView|RenderParams|Kidsrender|GraphWalk|FrameRenderViews|Render[A-Z]|CompoundRender'),
]
KEYS = [s[0] for s in SUBS]
TITLE = {s[0]: s[1] for s in SUBS}
SRX = [(s[0], re.compile(s[2])) for s in SUBS]
# hand-picked roots from the notes (RVA -> subsystem); they win over name rules
ENTRY = {
    # frame / graph
    0x113204: 'FRAME', 0x10f060: 'FRAME', 0x367bd44: 'FRAME', 0x3451994: 'FRAME', 0x25b337c: 'FRAME', 0x2449040: 'FRAME',
    0x2448fb0: 'FRAME', 0x243e624: 'FRAME', 0x247950: 'FRAME', 0x39a39c: 'FRAME', 0x39b570: 'FRAME', 0x9afed0: 'FRAME',
    0x4a529c: 'FRAME', 0x577e50: 'FRAME', 0x577ebc: 'FRAME',
    # backend
    0x1a7c3e0: 'BACKEND', 0x2eedfa0: 'BACKEND', 0x1a75970: 'BACKEND', 0x2f144b4: 'BACKEND', 0x254110: 'BACKEND',
    0x86f760: 'BACKEND', 0x86f828: 'BACKEND', 0x418f40: 'BACKEND', 0x4f9214: 'BACKEND', 0x1139d4: 'BACKEND',
    0x7f64dc: 'BACKEND', 0x118e28: 'BACKEND', 0x115de8: 'BACKEND', 0x2ee9de4: 'BACKEND', 0x2ee300c: 'BACKEND',
    0x2ee1bc0: 'BACKEND', 0x32fa450: 'BACKEND', 0x2269ec4: 'BACKEND', 0x32fe938: 'BACKEND', 0x25426c: 'BACKEND',
    # resource
    0x1190c0: 'RESOURCE', 0x1e7050: 'RESOURCE', 0x2ee9ee8: 'RESOURCE', 0x2f110a8: 'RESOURCE', 0x2f13350: 'RESOURCE',
    0x61b1c: 'RESOURCE',
    # pso
    0x119820: 'PSO', 0x58c144: 'PSO', 0x3c77f4: 'PSO', 0x1d7340: 'PSO',
    # scene
    0x535188: 'SCENE', 0x1e80b0: 'SCENE', 0x1d2f60: 'SCENE', 0x39e20: 'SCENE', 0x3bc0c: 'SCENE', 0x5301fc: 'SCENE',
    0x53fd3c: 'SCENE', 0x4aa34: 'SCENE', 0x4bbec: 'SCENE', 0x1daf6c: 'SCENE',
    # lighting
    0x465384: 'LIGHT', 0x465414: 'LIGHT', 0x7e92b4: 'LIGHT', 0x1f8340: 'LIGHT', 0x1f7960: 'LIGHT', 0x1872c4: 'LIGHT',
    0x54762c: 'LIGHT',
    # volumetrics / nature
    0x293230: 'VOLNAT', 0x293220: 'VOLNAT', 0x14871c: 'VOLNAT', 0x39b080: 'VOLNAT', 0x852620: 'VOLNAT', 0x850fe0: 'VOLNAT',
    0x108590: 'VOLNAT', 0x6d7b88: 'VOLNAT',
    # fx
    0x3d0ae0: 'FX', 0x365aa98: 'FX',
    # ui
    0x1cd690: 'UI', 0x12aa5c: 'UI', 0x4a2dc8: 'UI',
    # present
    0x3d62e4: 'PRESENT', 0x5da94: 'PRESENT', 0x1a82298: 'PRESENT', 0x2eea344: 'PRESENT', 0x2ee5a38: 'PRESENT',
    0x2eea690: 'PRESENT', 0x226d140: 'PRESENT', 0x5af410: 'PRESENT', 0x5ad5f8: 'PRESENT', 0x6b8e28: 'PRESENT',
    0x370f41c: 'PRESENT', 0x37778b4: 'PRESENT',
}
RENDER_LABELS = {'GFX': 'BACKEND', 'SHADERC': 'PSO', 'MATERIAL': 'PSO', 'POST': 'POST', 'UPSCALE': 'POST',
                 'LIGHT': 'LIGHT', 'SHADOW': 'LIGHT', 'NATURE': 'VOLNAT', 'FX': 'FX', 'MESHLET': 'SCENE', 'SCL': 'UI'}
SERVICE = {'CRT', 'MATH', 'MEM', 'JOBS', 'KTUL', 'ZLIB', 'VORBIS', 'CRYPTO', 'STEAM', 'DEBUG'}
NONRENDER = {'GAME', 'CHAR', 'AI', 'AUDIO', 'NET', 'COOP', 'SAVE', 'TEXT', 'SCRIPT', 'GEN', 'ANIM', 'PHYS', 'CAMERA',
             'INPUT', 'MOVIE', 'PHOTO', 'SCHEMA', 'OBJ', 'RES', 'APP'}
AUTO = re.compile(r'^(FUN_|Unknown_|sub_)|vfunc_\d+$|ctor_or_dtor|::ctor|Hot_|^PauseSpin|_[0-9A-F]{5,7}$')
DECOMP_INDEX = r'D:\nioh3-re\decomp\index.tsv'


def load_regions():
    lab2key = {v: k for k, v in R.LABELS.items()}
    rows = []
    for ln in open(os.path.join(ROOT, 're', 'scripts_re', 'regions.tsv'), encoding='utf-8'):
        if ln.startswith('start'):
            continue
        s, e, lab = ln.split('\t')[:3]
        rows.append((int(s, 16), int(e, 16), lab2key.get(lab, 'UNKNOWN')))
    rows.sort()
    starts = np.array([r[0] for r in rows], dtype=np.int64)
    i = np.searchsorted(starts, R.FST, side='right') - 1
    return np.array([rows[k][2] if k >= 0 and R.FST[j] < rows[k][1] else 'UNKNOWN' for j, k in enumerate(i)], dtype=object)


def load_syms():
    names, conf = {}, {}
    for ln in open(os.path.join(ROOT, 're', 'symbols', 'symbols.tsv'), encoding='utf-8'):
        if ln.startswith('#') or ln.startswith('rva\t'):
            continue
        c = ln.rstrip('\n').split('\t')
        if len(c) < 6 or c[1] != 'function':
            continue
        r = int(c[0], 16)
        names[r] = c[2]
        conf[r] = c[5]
    return names, conf


def load_note_cites():
    """function start -> set(note files) for hex tokens that are exact function starts (or 0x-prefixed inside one)."""
    fset = set(R.FST.tolist())
    cites = collections.defaultdict(set)
    files = glob.glob(os.path.join(ROOT, 'notes', '*.md')) + glob.glob(os.path.join(ROOT, 'notes', 'renderer', '*.md')) \
        + glob.glob(os.path.join(ROOT, 'docs', 'subsystems', '*.md'))
    tok = re.compile(r'(?<![0-9A-Za-z_])(0x)?([0-9A-Fa-f]{4,7})(?![0-9A-Za-z_])')
    for fn in files:
        rel = os.path.relpath(fn, ROOT).replace('\\', '/')
        txt = open(fn, encoding='utf-8', errors='replace').read()
        cand = set()
        for m in tok.finditer(txt):
            h = m.group(2)
            if not m.group(1) and not re.search(r'[A-Fa-f]', h) and len(h) < 5:
                continue
            cand.add(int(h, 16))
        if not cand:
            continue
        arr = np.array(sorted(cand), dtype=np.int64)
        own = R.owner(arr)
        for v, o in zip(arr.tolist(), own.tolist()):
            if o < 0:
                continue
            st = int(R.FST[o])
            if st == v or v - st < 0x4000:
                cites[st].add(rel)
    return cites


def load_decomp():
    s = set()
    if os.path.exists(DECOMP_INDEX):
        for ln in open(DECOMP_INDEX, encoding='utf-8', errors='replace'):
            c = ln.split('\t')
            if len(c) > 6 and c[6] == 'ok':
                s.add(int(c[0], 16))
    return s


def main():
    args = sys.argv[1:]
    NF = R.NF
    lab = load_regions()
    names, conf = load_syms()
    E = R.call_edges()
    slots, tgts = R.data_pointers()
    vts = R.vtables(slots, tgts)
    vt_addr = np.array(sorted(v[0] for v in vts), dtype=np.int64)
    vt_slots = {v[0]: v[1] for v in vts}
    refs = R.riprel_refs(vt_addr)                          # (fidx, vtable rva): ctor/dtor lea of the vtable
    ve = [(int(f), s) for f, vt in refs.tolist() for s in vt_slots.get(int(vt), [])]
    succ = [[] for _ in range(NF)]
    indeg = np.zeros(NF, dtype=np.int64)
    for a, b in E.tolist():
        succ[a].append(b)
        indeg[b] += 1
    for a, b in ve:
        if a != b:
            succ[a].append(b)
    fidx = R.FIDX
    # seeds
    sub = np.full(NF, -1, dtype=np.int64)
    dist = np.full(NF, 99, dtype=np.int64)
    seedset = collections.defaultdict(list)
    for r, nm in names.items():
        i = fidx.get(r)
        if i is None or AUTO.search(nm):
            continue
        if lab[i] in NONRENDER and r not in ENTRY:
            continue
        for k, rx in SRX:
            if rx.search(nm):
                sub[i] = KEYS.index(k); dist[i] = 0; seedset[k].append(i)
                break
    for r, k in ENTRY.items():
        i = fidx.get(r)
        if i is None:
            o = int(R.owner([r])[0])
            if o < 0:
                print('entry outside any function: 0x%x' % r, file=sys.stderr)
                continue
            i = o
        sub[i] = KEYS.index(k); dist[i] = 0
        if i not in seedset[k]:
            seedset[k].append(i)
    # BFS
    dep = collections.defaultdict(collections.Counter)
    frontier = [i for i in range(NF) if dist[i] == 0]
    for d in range(1, DEPTH + 1):
        nxt = []
        frontier.sort(key=lambda i: sub[i])
        for a in frontier:
            for b in succ[a]:
                if dist[b] <= d:
                    continue
                lb = lab[b]
                if lb in SERVICE or lb in NONRENDER or indeg[b] >= HUB:
                    dep[KEYS[sub[a]]][lb if indeg[b] < HUB else 'HUB(' + lb + ')'] += 1
                    continue
                if dist[b] == 99:
                    nxt.append(b)
                dist[b] = d; sub[b] = sub[a]
        frontier = nxt
    # region fill
    filled = 0
    for i in range(NF):
        if sub[i] < 0 and lab[i] in RENDER_LABELS:
            sub[i] = KEYS.index(RENDER_LABELS[lab[i]]); dist[i] = 50; filled += 1
    # cross-subsystem edges (who calls whom inside the render module)
    xs = collections.Counter()
    for a, b in E.tolist():
        if sub[a] >= 0 and sub[b] >= 0 and sub[a] != sub[b]:
            xs[(KEYS[sub[a]], KEYS[sub[b]])] += 1
        elif sub[a] >= 0 and sub[b] < 0:
            dep[KEYS[sub[a]]]['ext:' + lab[b]] += 0  # keep key space small; BFS deps above are the reported ones
    cites = load_note_cites()
    dec = load_decomp()
    stats = {}
    tot = collections.Counter()
    for k in KEYS:
        ids = np.nonzero(sub == KEYS.index(k))[0]
        st = collections.Counter()
        notes = collections.Counter()
        for i in ids.tolist():
            r = int(R.FST[i]); sz = int(R.SIZE[i])
            nm = names.get(r)
            real = nm is not None and not AUTO.search(nm)
            st['n'] += 1; st['bytes'] += sz
            st['real'] += real; st['auto'] += (nm is not None and not real); st['unnamed'] += nm is None
            st['real_b'] += sz * real
            und = real and (conf.get(r) in ('high', 'medium') or r in cites)
            st['und'] += und; st['und_b'] += sz * und
            st['cited'] += r in cites
            st['dec'] += r in dec
            st['region_fill'] += dist[i] == 50
            if dist[i] < 50:
                st['core'] += 1; st['core_b'] += sz; st['core_real'] += real; st['core_und'] += und; st['core_und_b'] += sz * und
            for nf in cites.get(r, ()):
                notes[nf] += 1
        stats[k] = (st, notes, ids)
        tot.update(st)
    if '--partition' in args:
        out = args[args.index('--partition') + 1]
        with open(out, 'w', encoding='utf-8', newline='\n') as f:
            f.write('# generated by re/scripts_renderer/render_map.py: rva\tsubsystem\tdepth(0 seed,50 region-fill)\tsize\tregion\tname\n')
            for i in np.nonzero(sub >= 0)[0].tolist():
                r = int(R.FST[i])
                f.write('0x%x\t%s\t%d\t%d\t%s\t%s\n' % (r, KEYS[sub[i]], dist[i], int(R.SIZE[i]), lab[i], names.get(r, '')))
    if '--sub' in args:
        k = args[args.index('--sub') + 1]
        n = int(args[args.index('--list') + 1]) if '--list' in args else 40
        st, notes, ids = stats[k]
        # roots = seeds with the largest reachable count inside the subsystem
        sk = KEYS.index(k)
        def reach(i):
            seen = {i}; fr = [i]
            for _ in range(DEPTH):
                nx = []
                for a in fr:
                    for b in succ[a]:
                        if b not in seen and sub[b] == sk:
                            seen.add(b); nx.append(b)
                fr = nx
            return len(seen)
        roots = sorted(((reach(i), i) for i in seedset[k]), reverse=True)[:n]
        for c, i in roots:
            r = int(R.FST[i])
            print('0x%x\t%d\t%s\t%s\t%s' % (r, c, names.get(r, ''), conf.get(r, ''), lab[i]))
        return
    md = '--md' in args
    hdr = '| subsystem | funcs | code KB | real names | understood | auto names | unnamed | in decomp export | cited in notes | via region only | top notes |'
    print(hdr); print('|' + '---|' * 11)
    for k in KEYS:
        st, notes, _ = stats[k]
        n = max(st['n'], 1)
        tn = ', '.join('%s (%d)' % (os.path.basename(a), c) for a, c in notes.most_common(4))
        print('| %s | %d | %d | %d (%.0f %%) | %d (%.0f %%; %.0f %% bytes) | %d | %d | %d | %d | %d | %s |' % (
            k, st['n'], st['bytes'] // 1024, st['real'], 100 * st['real'] / n, st['und'], 100 * st['und'] / n,
            100 * st['und_b'] / max(st['bytes'], 1), st['auto'], st['unnamed'], st['dec'], st['cited'], st['region_fill'], tn))
    n = max(tot['n'], 1)
    print('| **total** | %d | %d | %d (%.1f %%) | %d (%.1f %%; %.1f %% bytes) | %d | %d | %d | %d | %d | |' % (
        tot['n'], tot['bytes'] // 1024, tot['real'], 100 * tot['real'] / n, tot['und'], 100 * tot['und'] / n,
        100 * tot['und_b'] / max(tot['bytes'], 1), tot['auto'], tot['unnamed'], tot['dec'], tot['cited'], tot['region_fill']))
    print('\n## core = seeds + call/vtable closure only (no region fill)')
    print('| subsystem | core funcs | core KB | core real names | core understood (funcs; bytes) |')
    print('|---|---|---|---|---|')
    for k in KEYS + ['TOTAL']:
        st = tot if k == 'TOTAL' else stats[k][0]
        n = max(st['core'], 1)
        print('| %s | %d | %d | %d (%.0f %%) | %d (%.0f %%; %.0f %% bytes) |' % (
            k, st['core'], st['core_b'] // 1024, st['core_real'], 100 * st['core_real'] / n, st['core_und'],
            100 * st['core_und'] / n, 100 * st['core_und_b'] / max(st['core_b'], 1)))
    print('\nseeds per subsystem:', {k: len(seedset[k]) for k in KEYS}, 'region-filled:', filled, 'vtable edges:', len(ve))
    print('\n## dependencies (BFS edges stopped at non-render / service / hub code), top 8 per subsystem')
    for k in KEYS:
        print('- %s: %s' % (k, ', '.join('%s %d' % (a, c) for a, c in dep[k].most_common(8) if c)))
    print('\n## cross-subsystem direct calls inside the render module (top 25)')
    for (a, b), c in xs.most_common(25):
        print('- %s -> %s: %d' % (a, b, c))


if __name__ == '__main__':
    main()
