"""Lossless-efficiency audit of Nioh 3 G1M models (ModelData / StreamingMeshletModelData).

usage: py -I g1m_audit.py <dir_of_.g1m> [instances.tsv ...] > out.tsv     (instances.tsv = stage_models.py output)

G1MG v0045 layout used here (RE'd 2026-10-07, see notes/asset-audit.md):
  0x10004 VB      : n x {u32 0, u32 stride, u32 vcount, u32 flags} + data
  0x10005 layout  : attrs {u16 buf, u16 off, u8 type, u8 semantic, u8 layer, u8 0}
  0x10007 IB      : n x {u32 count, u32 bits (8/16/32), u32 flags} + data   (flags 0x80000000 = meshlet vertex list, u32 global VB index)
  0x10008 submesh : n x 14 u32 (flags, vb, .., ib, .., prim, vbOff, vCount, ibOff, iCount); flags & 0x4000 = meshlet submesh
  0x1000b tris    : u32 count, u32 4, then u32 per triangle = 3 x 10-bit meshlet-local indices
  0x1000c meshlets: u32 count, u32 16, then {u32 triOff, u32 triCnt, u32 vtxOff, u32 vtxCnt}
Meshlet draws (PB2 *Mshlt* VS) are non-indexed: VertexID % 384 / 3 = triangle slot, so each meshlet costs 384 VS lanes.
"""
import os, struct, sys, collections

SLOT = 128          # triangle slots per meshlet in the VS (384 / 3)


def sections(b):
    o = struct.unpack_from('<I', b, 12)[0]
    while o < len(b):
        mg, ver, sz = struct.unpack_from('<4s4sI', b, o)
        if mg == b'GM1G':
            break
        o += sz
    else:
        return {}
    n = struct.unpack_from('<I', b, o + 0x2c)[0]; p = o + 0x30; out = {}
    for _ in range(n):
        t, s, c = struct.unpack_from('<III', b, p); out[t] = (p, s, c); p += s
    return out


def vbs(b, S):
    p, s, c = S[0x10004]; q = p + 12; out = []
    for _ in range(c):
        _, stride, cnt, fl = struct.unpack_from('<4I', b, q); q += 16
        out.append(b[q:q + stride * cnt]); out[-1] = (stride, cnt, out[-1]); q += stride * cnt
    return out


def ibs(b, S):
    p, s, c = S[0x10007]; q = p + 12; out = []
    for _ in range(c):
        cnt, bits, fl = struct.unpack_from('<3I', b, q); q += 12
        w = bits // 8; fmt = {1: 'B', 2: 'H', 4: 'I'}[w]
        out.append((fl, list(struct.unpack_from('<%d%s' % (cnt, fmt), b, q)))); q += (cnt * w + 3) & ~3
    return out


def submeshes(b, S):
    p, s, c = S[0x10008]
    return [struct.unpack_from('<14I', b, p + 12 + 56 * i) for i in range(c)]


def meshlets(b, S):
    p, s, c = S[0x1000c]; n, st = struct.unpack_from('<II', b, p + 12)
    ml = [struct.unpack_from('<4I', b, p + 20 + st * i) for i in range(n)]
    p, s, c = S[0x1000b]; nt, _ = struct.unpack_from('<II', b, p + 12)
    tri = struct.unpack_from('<%dI' % nt, b, p + 20)
    return ml, tri


def acmr(idx, size=16):
    """FIFO post-transform cache (AMD-like), returns misses."""
    fifo = collections.deque(); inq = set(); miss = 0
    for v in idx:
        if v not in inq:
            miss += 1; fifo.append(v); inq.add(v)
            if len(fifo) > size:
                inq.discard(fifo.popleft())
    return miss


def forsyth(idx, cache=32):
    """Tom Forsyth linear-speed vertex cache optimisation (same family as meshopt_optimizeVertexCache)."""
    ntri = len(idx) // 3
    if ntri == 0:
        return idx
    adj = collections.defaultdict(list)
    for t in range(ntri):
        for k in range(3):
            adj[idx[3 * t + k]].append(t)
    live = {v: len(ts) for v, ts in adj.items()}
    pos = {}
    def vscore(v):
        if live[v] == 0:
            return -1.0
        s = 0.0; p = pos.get(v, -1)
        if p >= 0:
            s = 0.75 if p < 3 else (1.0 - (p - 3) / (cache - 3)) ** 1.5
        return s + 2.0 * live[v] ** -0.5
    vs = {v: vscore(v) for v in adj}
    ts = [vs[idx[3 * t]] + vs[idx[3 * t + 1]] + vs[idx[3 * t + 2]] for t in range(ntri)]
    done = [False] * ntri; out = []; lru = []; best = max(range(ntri), key=lambda t: ts[t]); left = ntri
    while left:
        if best < 0:
            best = max((t for t in range(ntri) if not done[t]), key=lambda t: ts[t])
        t = best; done[t] = True; left -= 1; tv = idx[3 * t:3 * t + 3]; out += tv
        for v in tv:
            live[v] -= 1
        lru = list(tv) + [v for v in lru if v not in tv]
        evicted = lru[cache:]; lru = lru[:cache]
        pos = {v: i for i, v in enumerate(lru)}
        touched = set(lru) | set(evicted)
        for v in evicted:
            pos.pop(v, None)
        for v in touched:
            vs[v] = vscore(v)
        best = -1; bs = -1.0
        for v in lru:
            for u in adj[v]:
                if not done[u]:
                    sc = vs[idx[3 * u]] + vs[idx[3 * u + 1]] + vs[idx[3 * u + 2]]; ts[u] = sc
                    if sc > bs:
                        bs, best = sc, u
    return out


def audit(path):
    b = open(path, 'rb').read()
    if b[:4] != b'_M1G':
        return None
    S = sections(b)
    if 0x10004 not in S:
        return None
    V = vbs(b, S); I = ibs(b, S); SM = submeshes(b, S)
    r = dict(file=os.path.basename(path), vb_verts=sum(c for _, c, _ in V), vb_bytes=sum(len(d) for _, _, d in V))
    dup = 0
    for stride, cnt, d in V:
        seen = set()
        for i in range(cnt):
            k = d[i * stride:(i + 1) * stride]
            if k in seen:
                dup += 1
            seen.add(k)
    r['dup_verts'] = dup
    if 0x1000c in S:
        ml, tri = meshlets(b, S)
        tris = sum(m[1] for m in ml); refs = sum(m[3] for m in ml)
        # sanity: local indices fit the meshlet vertex list
        bad = 0
        for to, tc, vo, vc in ml:
            for t in tri[to:to + tc]:
                if max(t & 1023, (t >> 10) & 1023, (t >> 20) & 1023) >= vc:
                    bad += 1
        r.update(kind='meshlet', meshlets=len(ml), tris=tris, mlv_refs=refs, max_tri=max(m[1] for m in ml),
                 max_vtx=max(m[3] for m in ml), fill=tris / (SLOT * len(ml)), bad_idx=bad)
        # lower bound: ceil(tris/128) per meshlet submesh (submesh vCount = meshlet count)
        lb = -(-tris // SLOT)
        r['vs_lanes'] = 3 * SLOT * len(ml); r['vs_lanes_min'] = 3 * SLOT * lb
        # wave64 occupancy: 384 lanes = 6 waves per meshlet; waves past the last triangle only run the header loads
        r['waves_used'] = sum(-(-3 * m[1] // 64) for m in ml); r['waves'] = 6 * len(ml)
        # cluster-LOD levels (0x1000f = level start offsets) and a merge bound per (level, 0x1000e class word)
        lv = [len(ml)]
        if 0x1000f in S:
            p, s_, c = S[0x1000f]; n = struct.unpack_from('<I', b, p + 12)[0]
            lv = list(struct.unpack_from('<%dI' % n, b, p + 16)) + [len(ml)]
        l0 = ml[lv[0]:lv[1]] if len(lv) > 1 else ml
        r['lod0_meshlets'] = len(l0); r['lod0_fill'] = sum(m[1] for m in l0) / (SLOT * max(len(l0), 1))
        r['levels'] = len(lv) - 1
        cls = [0] * len(ml); grp = []
        if 0x1000e in S:
            p, s_, c = S[0x1000e]; n, st = struct.unpack_from('<II', b, p + 12)
            cls = [struct.unpack_from('<I', b, p + 20 + st * i + 4)[0] for i in range(n)]
            grp = [struct.unpack_from('<7I', b, p + 20 + st * i + 4) for i in range(n)]   # class + parent/child refs
        acc = collections.Counter()
        for i, m in enumerate(ml):
            L = max(j for j in range(len(lv) - 1) if lv[j] <= i) if len(lv) > 1 else 0
            acc[(L, cls[i] if i < len(cls) else 0)] += m[1]
        r['min_meshlets'] = sum(-(-t // SLOT) for t in acc.values())
        # group-preserving bound: only merge meshlets with the same level + class + parent/child refs (selected together)
        g = collections.Counter()
        for i, m in enumerate(ml):
            L = max(j for j in range(len(lv) - 1) if lv[j] <= i) if len(lv) > 1 else 0
            g[(L, grp[i] if i < len(grp) else i)] += m[1]
        r['min_meshlets_grp'] = sum(-(-t // SLOT) for t in g.values())
    else:
        tot = miss = miss_opt = 0; ntri = 0
        for s in SM:
            fl, idx = I[s[7]]
            seg = idx[s[12]:s[12] + s[13]]
            if s[9] != 3 or len(seg) < 3:
                continue
            ntri += len(seg) // 3; miss += acmr(seg)
            miss_opt += acmr(forsyth(seg)) if len(seg) <= 300000 else acmr(seg)
            tot += len(seg)
        uniq = len({i for s in SM for i in I[s[7]][1][s[12]:s[12] + s[13]]})
        r.update(kind='indexed', submeshes=len(SM), tris=ntri, acmr=miss / max(ntri, 1), acmr_opt=miss_opt / max(ntri, 1),
                 atvr=miss / max(uniq, 1), uniq_ref_verts=uniq)
    return r


def main():
    d = sys.argv[1]; inst = collections.Counter()
    for f in sys.argv[2:]:
        for ln in open(f, encoding='utf-8'):
            c = ln.split('\t')
            if len(c) > 2 and not ln.startswith('#'):
                inst[c[0]] += int(c[1])
    cols = ['file', 'kind', 'inst', 'tris', 'vb_verts', 'dup_verts', 'meshlets', 'min_meshlets', 'min_meshlets_grp', 'fill', 'max_tri', 'max_vtx',
            'mlv_refs', 'bad_idx', 'waves', 'waves_used', 'levels', 'lod0_meshlets', 'lod0_fill', 'acmr', 'acmr_opt', 'atvr', 'submeshes', 'vb_bytes']
    print('\t'.join(cols))
    for fn in sorted(os.listdir(d)):
        try:
            r = audit(os.path.join(d, fn))
        except Exception as e:
            print('#ERR', fn, e, file=sys.stderr); continue
        if not r:
            continue
        r['inst'] = inst.get(fn[2:10].lower(), 0)
        print('\t'.join(('%.3f' % r[c]) if isinstance(r.get(c), float) else str(r.get(c, '')) for c in cols))


if __name__ == '__main__':
    main()
