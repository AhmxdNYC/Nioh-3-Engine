r"""scalarize.py - MSV (meshlet-VS scalarization) rewrite of the PB2 meshlet vertex shaders, at the DXIL text level.

What the shipped meshlet VS does per vertex (slSetUpMeshletVertex): vid -> meshlet slot (vid / 384, or vid >> 9 on the
direct path) -> a chain of ~7 DEPENDENT buffer loads that only depend on the meshlet slot (payload t24 -> G1M offsets
t17 -> bindless G1M root t1[] -> vertex/index info t20/t21 -> instance offsets t19 -> prim->texture table t27), then 3
per-vertex loads (triangle indices, vertex index, vertex data) and 3 matrix-palette rows (meshlet-uniform again).
The slot is the same for 384 consecutive vertices, so every lane of a wave loads the same addresses, but the compiler
cannot know that: all of it runs as VMEM (one round trip per chain level) with VGPR addresses, and the bindless handle
gets a readfirstlane waterfall per use.

Rewrite (output-identical by construction; every value a lane uses is computed from exactly its own slot):
    P:     ... (unchanged up to the slot phi K)              br label %wf_h
    wf_h:  k = WaveReadLaneFirst(K)                           br i1 X, label A, label B       (X: the uniform cbuffer test)
    A / B: the shipped chain blocks with K replaced by k (wave-uniform -> SMEM / SGPR); their bindless createHandle
           index is now uniform, so its NonUniformResourceIndex flag is cleared (no waterfall)
    wf_m:  the chain's join phis (renamed) [+ optional hoisted meshlet-uniform loads, --hoist]
           eq = (K == k)                                      br i1 eq, label M, label %wf_h
    M:     each original join phi becomes  %x = phi [ %wf_x, %wf_m ]  (LCSSA), so every later use is unchanged
A lane leaves the loop in the iteration whose k equals its own K, i.e. with the chain evaluated on its own slot; the
first active lane always matches, so the loop ends after (number of distinct slots in the wave) iterations (normally 1,
at most 2 for the 384-vertex slots). Read-only SRVs only: re-ordering / repeating loads cannot change a value.

    py -I scalarize.py in.ll out.ll [--hoist]
"""
import re, sys, os
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, '..', 'materials'))
from sfb_rewrite import Fn, DEF, TOK  # noqa: E402

RFL_DECL = 'declare i32 @dx.op.waveReadLaneFirst.i32(i32, i32) #1'
WAVEOPS = 1 << 19      # DXIL ShaderFlags m_bWaveOps
HOISTABLE = re.compile(r'^(add|mul|shl|lshr|and|or|extractvalue|call %dx\.types\.ResRet\.(i32|f32) @dx\.op\.(rawBufferLoad|bufferLoad)\.)')


def _uses(x):
    rhs = x.split(' = ', 1)[1] if DEF.match(x) else x
    return [t for t in TOK.findall(rhs) if t.startswith('%v') or t.startswith('%wf')]


def rewrite(text, hoist=False):
    f = Fn(text)
    B = f.blocks
    idx = {b: i for i, (b, _) in enumerate(B)}
    defs = {}
    for b, ins in B:
        for x in ins:
            m = DEF.match(x)
            if m:
                defs[m.group(1)] = (b, x)
    # per-lane taint: everything that depends on SV_VertexID
    vid = [m.group(1) for b, ins in B for x in ins for m in [DEF.match(x)] if m and '@dx.op.loadInput.i32(i32 4, i32 0, i32 0' in x]
    assert len(vid) == 1, 'SV_VertexID load not found'
    lane = set(vid)
    changed = True
    while changed:
        changed = False
        for b, ins in B:
            for x in ins:
                m = DEF.match(x)
                if m and m.group(1) not in lane and any(u in lane for u in _uses(x)):
                    lane.add(m.group(1)); changed = True
    # K: the phi that carries the meshlet slot (an incoming value is / uses  udiv vid, 384)
    U = [m.group(1) for b, ins in B for x in ins for m in [DEF.match(x)] if m and re.search(r'= udiv i32 %s, 384$' % re.escape(vid[0]), x)]
    assert len(U) == 1, 'slot udiv not found'
    K = P = None
    for b, ins in B:
        for x in ins:
            if ' = phi ' in x:
                for v in re.findall(r'\[ (%[\w.]+), %\w+ \]', x):
                    if v == U[0] or (v in defs and U[0] in _uses(defs[v][1])):
                        K, P = DEF.match(x).group(1), b
                        break
            if K:
                break
        if K:
            break
    assert K, 'slot phi not found'
    succ = {b: re.findall(r'label %(\w+)', ins[-1]) for b, ins in B}
    rets = [b for b, ins in B if ins[-1].strip().startswith('ret')]
    allb = [b for b, _ in B]
    pd = {b: set(allb) for b in allb}
    for r_ in rets:
        pd[r_] = {r_}
    ch = True
    while ch:
        ch = False
        for b in allb:
            if b in rets:
                continue
            n = set.intersection(*[pd[s_] for s_ in succ[b]]) | {b} if succ[b] else {b}
            if n != pd[b]:
                pd[b] = n; ch = True
    cands = pd[P] - {P}
    M = next(c for c in cands if all(c == d or c not in pd[d] for d in cands if d != c) and all(d in pd[c] for d in cands))
    region, st = [], [x for x in succ[P] if x != M]
    while st:
        u = st.pop()
        if u in region:
            continue
        region.append(u)
        st += [x for x in succ[u] if x != M and x not in region]
    region.sort(key=lambda r: idx[r])
    Pins = B[idx[P]][1]
    nph = sum(1 for x in Pins if ' = phi ' in x)
    Prest = Pins[nph:]
    body = Prest + [x for r in region for x in B[idx[r]][1]]
    local = {DEF.match(x).group(1) for x in body if DEF.match(x)}
    ext = {u for x in body for u in _uses(x) if u in lane and u not in local}
    assert ext == {K}, 'region uses other per-lane values: %r' % (ext - {K})
    bad = [x for x in body if re.search(r'@dx\.op\.(storeOutput|bufferStore|rawBufferStore|textureStore|atomic|discard)', x)]
    assert not bad, 'side effects in region'
    assert sum(('BufferLoad' in x or 'bufferLoad' in x) for x in body) >= 8, 'chain too short'
    for r in [P] + region:
        pass
    # preds of M must be in the region (or P)
    for b in allb:
        if M in succ[b]:
            assert b in region or b == P, 'M has a predecessor outside the region'
    # 1. P keeps its phis -> wf_h holds rfl + P's rest
    nonuni = 0

    def fix(x):
        nonlocal nonuni
        x = re.sub(re.escape(K) + r'\b', '%wf_k', x)
        if '@dx.op.createHandle(' in x and x.endswith('i1 true)'):
            x = x[:-len('i1 true)')] + 'i1 false)'; nonuni += 1
        if x.lstrip().startswith('br '):
            x = x.replace('label %' + M + ',', 'label %wf_m,').replace('label %' + M + '\n', 'label %wf_m\n')
            if x.endswith('label %' + M):
                x = x[:-len(M) - 1] + '%wf_m'
        return x
    wfh = ['  %%wf_k = call i32 @dx.op.waveReadLaneFirst.i32(i32 118, i32 %s)' % K] + [fix(x) for x in Prest]
    B[idx[P]][1] = Pins[:nph] + ['  br label %wf_h']
    for r in region:
        B[idx[r]][1] = [fix(x) for x in B[idx[r]][1]]
    # 3. wf_m: M's phis (incoming P -> wf_h) + exit test; M gets LCSSA phis
    Mi = B[idx[M]][1]
    phis = [x for x in Mi if ' = phi ' in x]
    rest = Mi[len(phis):]
    wfm, newM, ren = [], [], {}
    for x in phis:
        d = DEF.match(x).group(1); n = '%wf_' + d[1:]; ren[d] = n
        wfm.append(x.replace(d + ' = ', n + ' = ', 1).replace(', %' + P + ' ]', ', %wf_h ]'))
        ty = re.match(r'^\s*%[\w.]+ = phi (\S+) ', x).group(1)
        newM.append('  %s = phi %s [ %s, %%wf_m ]' % (d, ty, n))
    hoisted = []
    if hoist:
        hoisted = _hoist(B, idx, M, P, lane, set(ren), ren, defs)
        for x in hoisted:
            wfm.append(_rename(x, ren))
    wfm += ['  %%wf_eq = icmp eq i32 %s, %%wf_k' % K, '  br i1 %wf_eq, label %' + M + ', label %wf_h']
    hs = set(hoisted)
    for bi in range(len(B)):
        B[bi][1] = [x for x in B[bi][1] if x not in hs]
    B[idx[M]][1] = newM + [x for x in rest if x not in hs]
    B.insert(idx[P] + 1, ['wf_h', wfh])
    last = max(i for i, (b, _) in enumerate(B) if b in region or b == 'wf_h')
    B.insert(last + 1, ['wf_m', wfm])
    A, Bb = region[0], region[-1]
    out = f.emit()
    # declaration + shader flags
    if '@dx.op.waveReadLaneFirst.i32(i32, i32)' not in out:
        out = out.replace('\n; Function Attrs: nounwind\ndeclare void @dx.op.storeOutput.f32', '\n; Function Attrs: nounwind\n' + RFL_DECL +
                          '\n\n; Function Attrs: nounwind\ndeclare void @dx.op.storeOutput.f32', 1)
        assert RFL_DECL in out, 'declaration anchor not found'
    ep = re.search(r'^!dx\.entryPoints = !\{!(\d+)\}', out, re.M).group(1)
    epl = re.search(r'^!%s = !\{void \(\)\* @main, !"main", (?:!\d+|null), (?:!\d+|null), !(\d+)\}' % ep, out, re.M)
    fl = epl.group(1)
    m = re.search(r'^!%s = !\{i32 0, i64 (\d+)(.*)\}$' % fl, out, re.M)
    assert m, 'shader flags tag not first in entry extra'
    out = out.replace(m.group(0), '!%s = !{i32 0, i64 %d%s}' % (fl, int(m.group(1)) | WAVEOPS, m.group(2)), 1)
    return out, dict(P=P, region=len(region), M=M, K=K, phis=len(phis), nonuniform_cleared=nonuni, hoisted=len(hoisted))


def _rename(x, ren):
    return TOK.sub(lambda t: ren.get(t.group(0), t.group(0)), x)


def _hoist(B, idx, M, P, lane, phiset, ren, defs):
    """meshlet-uniform pure instructions of the block that post-dominates M (the matrix-palette rows): operands must be
    loop-invariant non-lane values, join phis, or other hoisted values."""
    succ = {}
    for b, ins in B:
        succ[b] = re.findall(r'label %(\w+)', ins[-1])
    exits = [b for b, ins in B if ins[-1].strip() == 'ret void']

    def postdom(h):
        seen, st = {M}, [M]
        while st:
            u = st.pop()
            if u in exits and u != h:
                return False
            for v in succ.get(u, []):
                if v != h and v not in seen:
                    seen.add(v); st.append(v)
        return True
    pre = set()
    for b, ins in B[:idx[P] + 1]:
        pre |= {DEF.match(x).group(1) for x in ins if DEF.match(x)}
    out = []
    for b, ins in B[idx[M] + 1:]:
        if not postdom(b):
            continue
        avail = set(phiset)
        for x in ins:
            m = DEF.match(x)
            if not m or ' = phi ' in x:
                continue
            rhs = x.split(' = ', 1)[1].strip()
            us = _uses(x)
            ok = HOISTABLE.match(rhs) and all((u in avail) or (u in pre and u not in lane) or u in out_defs(out) for u in us)
            hdl = re.search(r'%dx\.types\.Handle (%[\w.]+)', rhs)
            if ok and hdl and hdl.group(1) not in pre:
                ok = False
            if ok and any(u in phiset or u in out_defs(out) for u in us):
                out.append(x)
        break
    # keep only loads and what they need/produce (drop arithmetic nobody hoisted uses)
    return out


def out_defs(lst):
    return {DEF.match(x).group(1) for x in lst}


if __name__ == '__main__':
    a = sys.argv[1:]
    t, info = rewrite(open(a[0], encoding='utf-8').read(), '--hoist' in a)
    open(a[1], 'w', encoding='utf-8').write(t)
    print(info)
