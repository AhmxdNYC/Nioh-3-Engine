#!/usr/bin/env python3
"""frame_graph.py - the KIDS render graph as an ordered pass list with the textures each node binds (notes/render-architecture.md).

  py -I re\\scripts_gpu\\frame_graph.py [GRAPH] [--w 1248 --h 702] [--md]   default graph = system 0x102dd706 (main game graph)

Sources: data\\dumps\\kgk_<graph>.kidsrender.txt (node order = execution order of the graph walker 113204; BeginScene/EndScene
nest render-target scopes) and the RT table data\\dumps\\0x93fb3280.kidsobjdb.txt (sizes; Width 0 = render size, -1/-2/-3 =
render / 2 / 4 / 8 (VCBayer = render/8 is code -3, ally-gpu-budget.md section 6)). Field names give the role: *RenderTarget* /
BeginScene targets are writes, the rest are reads (heuristic, marked). PixelFormat codes are KTGL's own enum; the byte sizes in
BPP are inferred from usage (02 R32F-class shadow/ESM/prev-depth, 03 HDR scene, 04 the 2x1 exposure target) and are an estimate.
Traffic = one full read or write of every bound resource per node at the given render size: an upper-bound-ish screen-space
estimate, not a measurement (compression, partial updates and early-outs are ignored).
"""
import argparse, os, re

D = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', 'data', 'dumps')
BPP = {0x01: 4, 0x02: 4, 0x03: 8, 0x04: 16, 0x05: 4, 0x14: 2, 0x2A: 4, 0x41: 4, 0x4C: 1, 0x4E: 4, 0x69: 4, 0x6A: 8,
       0x6B: 4, 0x6C: 5, 0x72: 2, 0x73: 1, 0x00: 4}
SKIP = {'ViewObjectNameHash', 'ViewSDFObjectNameHash', 'UltraMarineObjectNameHash', 'RainDropObjectNameHash', 'ScreenMapObjectNameHash',
        'GBuffersObjectNameHash', 'VariousTexturesObjectNameHash', 'GBufferMaterialIDTableObjectNameHash'}


def fmt_code(v):
    v = v.strip()
    if v.startswith("'") and len(v) == 3:
        return ord(v[1])
    try:
        return int(v, 16)
    except ValueError:
        return None


def load_rts():
    rts, cur = {}, None
    for ln in open(os.path.join(D, '0x93fb3280.kidsobjdb.txt'), encoding='utf-8', errors='replace'):
        m = re.match(r'== obj ([0-9a-f]{8})\s+(\S+)', ln)
        if m:
            cur = rts.setdefault(int(m.group(1), 16), {'kind': m.group(2).split('::')[-1]})
            continue
        m = re.match(r'\s+(Width|Height|Depth|LevelNum|PixelFormat)\s+(\S+)', ln)
        if m and cur is not None:
            k, v = m.groups()
            cur[k] = fmt_code(v) if k == 'PixelFormat' else int(v, 16) if k == 'LevelNum' else int(v)
    return rts


def load_graph(path):
    nodes, cur = [], None
    for ln in open(path, encoding='utf-8', errors='replace'):
        m = re.match(r'== node (\d+) (\S+) id=([0-9a-f]+)', ln)
        if m:
            cur = {'i': int(m.group(1)), 'type': m.group(2).replace('TypeInfo::RenderNode::', ''), 'f': {}}
            nodes.append(cur)
            continue
        m = re.match(r'\s+(\w+)\s+(.*)$', ln)
        if m and cur is not None:
            cur['f'][m.group(1)] = m.group(2).strip()
    return nodes


def hashes(v):
    return [int(x, 16) for x in re.findall(r'0x([0-9a-f]+)', v)]


def size_of(rt, W, H):
    w, h = rt.get('Width', 0), rt.get('Height', rt.get('Width', 0))
    def dim(x, full):
        return full if x == 0 else full // (2 ** (-x)) if x < 0 else x
    w, h = dim(w, W), dim(h, H)
    sl = rt.get('Depth', 1) or 1
    if rt['kind'] in ('Cube', 'CubeArray'):
        h = w; sl = max(sl, 6)
    bpp = BPP.get(rt.get('PixelFormat'), 4)
    return w, h, sl, w * h * sl * bpp


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('graph', nargs='?', default='0x102dd706')
    ap.add_argument('--w', type=int, default=1248); ap.add_argument('--h', type=int, default=702)
    ap.add_argument('--md', action='store_true')
    ap.add_argument('--big', type=float, default=1.0, help='only list resources >= this many MB')
    a = ap.parse_args()
    rts = load_rts()
    nodes = load_graph(os.path.join(D, 'kgk_%s.kidsrender.txt' % a.graph))
    scope = []
    total_rw = 0.0
    out = []
    for n in nodes:
        t, f = n['type'], n['f']
        if t.endswith('BeginScene'):
            tg = [h for h in hashes(f.get('ColorRenderTargetObjectNameHash', '')) if h] + \
                 [h for h in hashes(f.get('DepthRenderTargetObjectNameHash', '')) if h]
            scope.append(tg)
        elif t.endswith('EndScene'):
            if scope:
                scope.pop()
        if t.startswith('RenderBlock::') or t in ('type_00000000',) or t.endswith(('BeginScene', 'EndScene')):
            continue
        reads, writes = [], []
        for k, v in f.items():
            if k in SKIP or 'Shader' in k or 'ObjectNameHash' not in k and 'Target' not in k and 'TextureObject' not in k:
                continue
            for h in hashes(v):
                if h in rts:
                    (writes if ('RenderTarget' in k or k.startswith('Output')) else reads).append((k, h))
        if scope:
            writes += [('scope RT', h) for h in scope[-1] if h in rts]
        mb_r = sum(size_of(rts[h], a.w, a.h)[3] for _, h in dict(reads).items() if True) / 1e6
        seen = set(); rr = []
        for k, h in reads:
            if h not in seen:
                seen.add(h); rr.append((k, h))
        seen = set(); ww = []
        for k, h in writes:
            if h not in seen:
                seen.add(h); ww.append((k, h))
        def desc(lst):
            s = []
            for k, h in lst:
                w, hh, sl, by = size_of(rts[h], a.w, a.h)
                if by / 1e6 >= a.big:
                    s.append('%s %08x %dx%d%s f%02X %.1fMB' % (re.sub(r'(TextureObjectNameHash|ObjectNameHash|RenderTarget)', '', k)[:22], h,
                             w, hh, ('x%d' % sl) if sl > 1 else '', rts[h].get('PixelFormat') or 0, by / 1e6))
            return s
        mb_r = sum(size_of(rts[h], a.w, a.h)[3] for _, h in rr) / 1e6
        mb_w = sum(size_of(rts[h], a.w, a.h)[3] for _, h in ww) / 1e6
        total_rw += mb_r + mb_w
        out.append((n['i'], t, mb_r, mb_w, desc(rr), desc(ww)))
    if a.md:
        print('| node | type | bound read MB | write MB | reads (>= %.1f MB) | writes |' % a.big)
        print('|---|---|---|---|---|---|')
        for i, t, r, w, dr, dw in out:
            print('| %d | %s | %.1f | %.1f | %s | %s |' % (i, t, r, w, '; '.join(dr) or '-', '; '.join(dw) or '-'))
    else:
        for i, t, r, w, dr, dw in out:
            print('%3d %-34s R %7.1f MB  W %6.1f MB  | R: %s | W: %s' % (i, t, r, w, '; '.join(dr) or '-', '; '.join(dw) or '-'))
    print('nodes listed %d; bound-resource traffic upper bound %.0f MB per frame at %dx%d (one full read/write per binding)'
          % (len(out), total_rw, a.w, a.h))


if __name__ == '__main__':
    main()
