r"""lodfast.py - round 4: guarded wave-uniform fast path for MshLod#2 (63c07c21, the per-cluster LOD kernel), at the DXIL
text level (no HLSL decompile, no float op touched). notes/gpu-rewrite-round4.md s2.

What the shipped kernel does per thread (one LOD cluster payload): g1m = (payload.p0 >> 6) & 0x3ffff, then it reads the
G1M offset row tG1mOffsets[g1m] (t16, 48 B: rootBufferIdx, rootOffset, ...) and tMeshletResourceOffsets[g1m] (t19, 8 B)
in 14 + 6 places along the chain (the driver CSEs the dominated ones; 8 VMEM remain), every one with a VGPR index, and
every bindless tMeshletG1mBuffer[rootBufferIdx] use is a readfirstlane waterfall (6 loops). When all active lanes of
a wave work on clusters of the same mesh (children of one parent are contiguous in the payload list), all of that is
wave-uniform, but the compiler cannot know it.

Rewrite (mode 'dup', the default):
    b26:   ... g = (p0 >> 6) & 0x3ffff                        (unchanged up to g's definition)
           e = WaveActiveAllEqual(g); k = WaveReadLaneFirst(g)
           br e, wf_u, wf_r
    wf_r:  the rest of b26 and every later block: the SHIPPED code, untouched          (divergent waves)
    wf_u:  r16[0..5] = t16[k].{0,4,16,28,36,44}; r19[0..1] = t19[k].{0,4}   (uniform index -> scalar loads)
           + a clone of wf_r and every block reachable from it, in which each t16/t19 load at g is replaced by the row
           value loaded above, g by k, and the bindless createHandle(t0[r16[0]]) loses its NonUniformResourceIndex flag
Equal index into a read-only SRV -> equal value, so each lane computes exactly what the shipped code computes; the
clone keeps every UAV load/store, atomic and wave op with the same operands, in the same order and the same CFG, and
the branch on e is wave-uniform, so all lanes of a wave run the same copy: the wave-level interleaving of side effects
is the shipped one. Mode 'hoist' (no clone): one diamond loads the rows (uniform or per-lane) and every later re-load
uses the joined value (kept for the ISA comparison only).

    py -I lodfast.py in.ll out.ll [--hoist]
"""
import re, sys, os
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, '..', 'materials'))
from sfb_rewrite import Fn, DEF, TOK  # noqa: E402

AEQ_DECL = 'declare i1 @dx.op.waveActiveAllEqual.i32(i32, i32) #2'
RFL_DECL = 'declare i32 @dx.op.waveReadLaneFirst.i32(i32, i32) #2'
SIDE = re.compile(r'@dx\.op\.(bufferStore|rawBufferStore|textureStore|atomic|wave|barrier)|^\s*store ')


def _handle(B, space_reg):
    cls, rid, reg = space_reg
    pat = re.compile(r'^\s*(%[\w.]+) = call %dx\.types\.Handle @dx\.op\.createHandle\(i32 57, ' +
                     r'i8 {}, i32 {}, i32 {}, i1 false\)$'.format(cls, rid, reg))
    h = [m.group(1) for _, ins in B for x in ins for m in [pat.match(x)] if m]
    assert len(h) == 1, 'handle %r: %r' % (space_reg, h)
    return h[0]


def rewrite(text, mode='dup'):
    f = Fn(text)
    B = f.blocks
    h16 = _handle(B, (0, 0, 16))        # tG1mOffsets      StructuredBuffer<S_G1M_OFFSET> (48 B)
    h19 = _handle(B, (0, 4, 19))        # tMeshletResourceOffsets StructuredBuffer<S_RESOURCE_OFFSET> (8 B)
    LD = re.compile(r'^\s*(%[\w.]+) = call %dx\.types\.ResRet\.i32 @dx\.op\.bufferLoad\.i32\(i32 68, %dx\.types\.Handle (' +
                    re.escape(h16) + '|' + re.escape(h19) + r'), i32 (%[\w.]+), i32 (\d+)\)$')
    loads = {}                          # load def -> (handle, offset, index, block)
    for b, ins in B:
        for x in ins:
            m = LD.match(x)
            if m:
                loads[m.group(1)] = (m.group(2), int(m.group(4)), m.group(3), b)
    idxs = {v[2] for v in loads.values()}
    assert len(idxs) == 1, 'row loads use %d different indices' % len(idxs)
    G = idxs.pop()
    # every use of a row load is `extractvalue ..., 0`
    ext = {}                            # extract def -> (handle, offset)
    for b, ins in B:
        for x in ins:
            for t in TOK.findall(x.split(' = ', 1)[1] if DEF.match(x) else x):
                if t in loads:
                    m = re.match(r'^\s*(%[\w.]+) = extractvalue %dx\.types\.ResRet\.i32 ' + re.escape(t) + r', 0$', x)
                    assert m, 'row load used other than as component 0: ' + x
                    ext[m.group(1)] = loads[t][:2]
    rows = sorted({v[:2] for v in loads.values()}, key=lambda r: (r[0] != h16, r[1]))
    # g's block, and the position right after its definition
    gb = next(b for b, ins in B if any(DEF.match(x) and DEF.match(x).group(1) == G for x in ins))
    bi = [b for b, _ in B].index(gb)
    gi = next(i for i, x in enumerate(B[bi][1]) if DEF.match(x) and DEF.match(x).group(1) == G)
    assert not any(' = phi ' in x for x in B[bi][1][gi + 1:]), 'phi after g'
    succ = {b: re.findall(r'label %(\w+)', ins[-1]) for b, ins in B}
    # dominance: every row load is reachable from g's block only through it (g dominates its uses anyway; assert it)
    reach, st = set(), [gb]
    while st:
        u = st.pop()
        for v in succ[u]:
            if v not in reach:
                reach.add(v); st.append(v)
    assert gb not in reach, 'g block is in a loop'
    assert all(v[3] == gb or v[3] in reach for v in loads.values())
    head = B[bi][1][:gi + 1]
    rest = B[bi][1][gi + 1:]
    rn = {}                             # rows in the fast copy: (h, off) -> value name
    nm = {h16: 'r16_', h19: 'r19_'}

    def row_loads(idx, tag):
        out = []
        for h, off in rows:
            v = '%%wf_%s%s%d' % (tag, nm[h], off)
            out += ['  %s_l = call %%dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32 68, %%dx.types.Handle %s, i32 %s, i32 %d)' % (v, h, idx, off),
                    '  %s = extractvalue %%dx.types.ResRet.i32 %s_l, 0' % (v, v)]
            rn[(tag, h, off)] = v
        return out
    wave = ['  %wf_e = call i1 @dx.op.waveActiveAllEqual.i32(i32 115, i32 ' + G + ')',
            '  %wf_k = call i32 @dx.op.waveReadLaneFirst.i32(i32 118, i32 ' + G + ')']
    info = dict(g=G, rows=len(rows), row_loads=len(loads), uses=len(ext))

    def drop_rows(ins, tag, subst):
        out = []
        for x in ins:
            m = DEF.match(x)
            if m and (m.group(1) in loads or m.group(1) in ext):
                continue
            out.append(TOK.sub(lambda t: subst.get(t.group(0), t.group(0)), x))
        return out

    if mode == 'hoist':
        B[bi][1] = head + wave + ['  br i1 %wf_e, label %wf_u, label %wf_d']
        sub = {e: '%%wf_j%s%d' % (nm[hd], off) for e, (hd, off) in ext.items()}
        wu = row_loads('%wf_k', 'u') + ['  br label %wf_j']
        wd = row_loads(G, 'd') + ['  br label %wf_j']
        wj = ['  %%wf_j%s%d = phi i32 [ %s, %%wf_u ], [ %s, %%wf_d ]' % (nm[h], off, rn[('u', h, off)], rn[('d', h, off)]) for h, off in rows]
        wj += drop_rows(rest, 'j', sub)
        for b2 in range(len(B)):
            if b2 != bi:
                B[b2][1] = drop_rows(B[b2][1], 'j', sub)
        for s in succ[gb]:          # phis of gb's successors now come from wf_j
            j = [b for b, _ in B].index(s)
            B[j][1] = [x.replace(', %' + gb + ' ]', ', %wf_j ]') if ' = phi ' in x else x for x in B[j][1]]
        B[bi + 1:bi + 1] = [['wf_u', wu], ['wf_d', wd], ['wf_j', wj]]
        info['cloned_blocks'] = 0
    else:
        clone = [gb] + sorted(reach, key=lambda b: [x for x, _ in B].index(b))   # gb = its rest (wf_r) in the copy
        cmap = {b: 'wf_c_' + b for b in clone}
        cmap[gb] = 'wf_c_r'
        defs = {DEF.match(x).group(1) for b, ins in B if b in reach for x in ins if DEF.match(x)}
        defs |= {DEF.match(x).group(1) for x in rest if DEF.match(x)}
        sub = {d: '%wf_c_' + d[1:] for d in defs}
        sub.update({e: '%%wf_u%s%d' % (nm[hd], off) for e, (hd, off) in ext.items()})
        sub[G] = '%wf_k'
        for b in clone:
            sub['%' + b] = '%' + cmap[b]
        nonuni = [0]

        def cl(ins):
            out = []
            for x in drop_rows(ins, 'u', sub):
                if '@dx.op.createHandle(' in x and x.endswith('i1 true)'):
                    m = re.search(r'i32 57, i8 0, i32 1, i32 (%[\w.]+), i1 true\)$', x)
                    assert m, x
                    out.append(x[:-len('i1 true)')] + 'i1 false)'); nonuni[0] += 1
                else:
                    out.append(x)
            return out
        fast = [['wf_u', row_loads('%wf_k', 'u') + cl(rest)]]
        for b in clone[1:]:
            fast.append([cmap[b], cl(B[[x for x, _ in B].index(b)][1])])
        # a clone phi's incoming block names went through sub; incoming from gb in the original is from wf_r now
        fast = [[n, [x.replace('%wf_c_r ]', '%wf_u ]') if ' = phi ' in x else x for x in ins]] for n, ins in fast]
        B[bi][1] = head + wave + ['  br i1 %wf_e, label %wf_u, label %wf_r']
        B.insert(bi + 1, ['wf_r', rest])
        for s in succ[gb]:
            j = [b for b, _ in B].index(s)
            B[j][1] = [x.replace(', %' + gb + ' ]', ', %wf_r ]') if ' = phi ' in x else x for x in B[j][1]]
        B.extend(fast)
        # the fast copy must use no value of the slow copy except those defined before the branch
        pre = {DEF.match(x).group(1) for b, ins in B[:bi + 1] for x in ins if DEF.match(x)}
        fdefs = {DEF.match(x).group(1) for _, ins in fast for x in ins if DEF.match(x)} | {'%' + n for n, _ in fast}
        for _, ins in fast:
            for x in ins:
                for t in TOK.findall(x.split(' = ', 1)[1] if DEF.match(x) else x):
                    if t.startswith('%v') or t.startswith('%wf_'):
                        assert t in pre or t in fdefs or t in ('%wf_k', '%wf_e'), 'fast copy uses %s: %s' % (t, x)
        info.update(cloned_blocks=len(fast), nonuniform_cleared=nonuni[0])
    out = f.emit()
    if AEQ_DECL.split(' #')[0] not in out:
        assert RFL_DECL in out, 'waveReadLaneFirst declaration anchor'
        out = out.replace(RFL_DECL, RFL_DECL + '\n\n; Function Attrs: nounwind\n' + AEQ_DECL, 1)
    return out, info


if __name__ == '__main__':
    a = sys.argv[1:]
    t, info = rewrite(open(a[0], encoding='utf-8').read(), 'hoist' if '--hoist' in a else 'dup')
    open(a[1], 'w', encoding='utf-8', newline='\n').write(t)
    print(info)
