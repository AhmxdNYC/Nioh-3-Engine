r"""flag_dedupe.py - VSM-FLAG-DEDUPE: per-thread "last page" cache for repeated Buffer<uint> page-flag loads (WDO#14).

The sun-shadow resolve (0x1667d3a0.file WDO-PtSrbSsmVsm #14) does, for every PCF tap of the virtual shadow map,
    flag = tVirtualShadowMapFlags.Load(page index of this tap at clip level L)        (and again at level L+1 for the
                                                                                       cross-fade)
Neighbouring taps almost always fall into the same VSM page, so the same dword is fetched again and again. The rewrite
gives every "chain" of loads (same handle, same clip-level term in the index: L, L+1, ...) a cache (valid, idx, val):
    if (valid && idx == cache.idx) flag = cache.val; else flag = Load(idx);     cache = (true, idx, flag)
Same output by construction: an equal index of a read-only SRV (createHandle class 0) returns the equal value. The
load stays in place (its own block, skipped by the whole wave when every lane hits); everything else keeps its place
and order. The cache is carried through the CFG as SSA phis (standard phi placement on iterated dominance frontiers,
dead / trivial phis removed). Loads inside loops (the clip-level search) and the first load of each chain (nothing
cached yet) are left alone.

    py -I flag_dedupe.py in.ll out.ll            -> prints the rewritten sites
    import flag_dedupe; text, sites = flag_dedupe.rewrite(text)
"""
import os, re, sys
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, '..', 'materials'))
import sfb_rewrite as S  # noqa: E402  (Fn: named blocks/values + sequential renumbering on emit)

LOAD = S.LOAD
DEF = S.DEF
TOK = S.TOK
U2F_ANY = re.compile(r'^\s*(%[\w.]+) = uitofp i32 (%[\w.]+) to float$')
ADDC = re.compile(r'^\s*(%[\w.]+) = add (?:nuw )?(?:nsw )?i32 (%[\w.]+), (-?\d+)$')
# instructions the index trace may walk through (pure arithmetic producing the page index)
TRACE_OK = re.compile(r'= (?:fadd|fmul|fsub|fptoui|fptosi|add|mul|shl|or|and|call float @dx\.op\.(?:unary|binary|tertiary)\.f32)')


def _cfg(fn):
    names = [b for b, _ in fn.blocks]
    succ = {b: re.findall(r'label %([\w.]+)', ins[-1]) if ins else [] for b, ins in fn.blocks}
    pred = {b: [] for b in names}
    for b in names:
        for s in succ[b]:
            pred[s].append(b)
    return names, succ, pred


def _dominators(names, pred):
    dom = {b: set(names) for b in names}
    dom[names[0]] = {names[0]}
    ch = True
    while ch:
        ch = False
        for b in names[1:]:
            ps = [dom[p] for p in pred[b]]
            nd = ({b} | set.intersection(*ps)) if ps else {b}
            if nd != dom[b]:
                dom[b] = nd; ch = True
    idom = {}
    for b in names[1:]:
        sd = dom[b] - {b}
        idom[b] = next(c for c in sd if all(c == d or d in dom[c] for d in sd))   # the strict dominator dominated by all others
    return dom, idom


def _loop_blocks(names, succ, pred, dom):
    out = set()
    for b in names:
        for h in succ[b]:
            if h in dom[b]:                         # back edge b -> h: natural loop body
                body, st = {h, b}, [b]
                while st:
                    x = st.pop()
                    for p in pred[x]:
                        if p not in body:
                            body.add(p); st.append(p)
                out |= body
    return out


def _chain_key(defs, idx):
    """the clip-level term of a page index: the (single) uitofp feeding it, as (base value, constant offset)"""
    seen, st, u2f = set(), [idx], set()
    while st:
        v = st.pop()
        if v in seen or v not in defs:
            continue
        seen.add(v)
        x = defs[v]
        m = U2F_ANY.match(x)
        if m:
            u2f.add(m.group(2)); continue
        if not TRACE_OK.search(x) or ' phi ' in x:
            continue
        st.extend(TOK.findall(x.split(' = ', 1)[1]))
    if len(u2f) != 1:
        return None
    lv = u2f.pop()
    m = ADDC.match(defs.get(lv, ''))
    return (m.group(2), int(m.group(3))) if m else (lv, 0)


def rewrite(text, handle_filter=None, offsets=None, bitval=False):
    """offsets: only chains whose clip-level term is base+o for o in offsets (None = all).
    bitval: when every use of a chain's loaded dword is 'and x, 1', cache only that bit (i1: an SGPR lane mask instead
    of a VGPR); a hit then yields zext(bit), whose 'and 1' equals the loaded dword's. Off by default: RGA showed no
    VGPR gain on WDO#14 (gfx1103 48 either way, gfx1201 44 either way)."""
    fn = S.Fn(text)
    srv = S.srv_handles(fn)
    names, succ, pred = _cfg(fn)
    dom, idom = _dominators(names, pred)
    loops = _loop_blocks(names, succ, pred, dom)
    defs = {}
    for b, ins in fn.blocks:
        for x in ins:
            m = DEF.match(x)
            if m:
                defs[m.group(1)] = x
    # ---- candidate loads: SRV Buffer<uint> loads whose only use is "extractvalue 0" in the same block
    cands = []          # (block, load dest, handle, idx, extract dest, chain)
    for b, ins in fn.blocks:
        if b in loops:
            continue
        for i, x in enumerate(ins):
            m = LOAD.match(x)
            if not m or m.group(2) not in srv or (handle_filter and m.group(2) != handle_filter):
                continue
            d, h, idx = m.group(1), m.group(2), m.group(3)
            uses = [(bb, j) for bb, ii in fn.blocks for j, y in enumerate(ii) if re.search(re.escape(d) + r'(?![\w.])', y.split(' = ', 1)[-1])]
            if len(uses) != 1 or uses[0][0] != b:
                continue
            e = S.EXTR0.match(ins[uses[0][1]])
            if not e or uses[0][1] < i:
                continue
            key = _chain_key(defs, idx)
            if key is None or (offsets is not None and key[1] not in offsets):
                continue
            cands.append((b, d, h, idx, e.group(1), (h,) + key))
    chains = sorted({c[5] for c in cands})
    cid = {c: k for k, c in enumerate(chains)}
    bitc = set()
    if bitval:
        body = '\n'.join(y for _, ii in fn.blocks for y in ii)
        for c in chains:
            if all(len(re.findall(re.escape(x[4]) + r'(?![\w.])', body)) == 1 + len(re.findall(r'= and i32 ' + re.escape(x[4]) + r', 1$', body, re.M))
                   for x in cands if x[5] == c):
                bitc.add(cid[c])
    # ---- SSA construction of the per-chain cache (valid:i1, idx:i32, val:i32)
    children = {b: [] for b in names}
    for b, p in idom.items():
        children[p].append(b)
    df = {b: set() for b in names}
    for b in names:
        if len(pred[b]) >= 2:
            for p in pred[b]:
                r = p
                while r != idom.get(b) and r is not None:
                    df[r].add(b)
                    r = idom.get(r)
    VARS = ('valid', 'idx', 'val')
    TY = {'valid': 'i1', 'idx': 'i32', 'val': 'i32'}
    INIT = {'valid': 'false', 'idx': '0', 'val': '0'}
    BINIT = dict(INIT, val='false')
    phis = {}           # (chain, var, block) -> {pred: value}
    for c in chains:
        dblocks = {x[0] for x in cands if x[5] == c} | {names[0]}
        work, has = list(dblocks), set()
        while work:
            b = work.pop()
            for y in df[b]:
                if y not in has:
                    has.add(y)
                    for v in VARS:
                        phis[(cid[c], v, y)] = {}
                    if y not in dblocks:
                        work.append(y)
    pname = lambda k, v, b: '%%dd%d_%s_%s' % (k, v, b)
    by_block = {}
    for x in cands:
        by_block.setdefault(x[0], []).append(x)
    reach = {}          # load dest -> {var: value} (cache seen by that load)
    stacks = {(cid[c], v): [(BINIT if cid[c] in bitc else INIT)[v]] for c in chains for v in VARS}

    def walk(b):
        pushed = []
        for (k, v, y) in phis:
            if y == b:
                stacks[(k, v)].append(pname(k, v, b)); pushed.append((k, v))
        pos = {n: i for i, n in enumerate([n for n, _ in fn.blocks])}
        ins = fn.blocks[pos[b]][1]
        for x in sorted(by_block.get(b, []), key=lambda t: next(i for i, y in enumerate(ins) if y.lstrip().startswith(t[1] + ' '))):
            k = cid[x[5]]
            reach[x[1]] = {v: stacks[(k, v)][-1] for v in VARS}
            for v, val in (('valid', 'true'), ('idx', x[3]), ('val', x[4] + '_bit' if k in bitc else x[4])):
                stacks[(k, v)].append(val); pushed.append((k, v))
        for s in succ[b]:
            for (k, v, y), inc in phis.items():
                if y == s:
                    inc[b] = stacks[(k, v)][-1]
        for ch in children[b]:
            walk(ch)
        for kv in pushed:
            stacks[kv].pop()

    sys.setrecursionlimit(max(10000, 4 * len(names)))
    walk(names[0])
    # ---- simplify: trivial phis (all incomings the same value or the phi itself) -> that value
    alias = {}

    def res(v):
        while v in alias:
            v = alias[v]
        return v
    ch = True
    while ch:
        ch = False
        for key, inc in phis.items():
            me = pname(*key)
            if me in alias:
                continue
            vals = {res(v) for v in inc.values()} - {me}
            if len(vals) == 1:
                alias[me] = vals.pop(); ch = True
    for d in reach:
        reach[d] = {v: res(val) for v, val in reach[d].items()}
    rewrite_set = [x for x in cands if reach[x[1]]['valid'] != 'false']
    # ---- live phis: used by a rewritten load (valid/idx/val), transitively through other phis
    live, st = set(), []
    for x in rewrite_set:
        r = reach[x[1]]
        for v in VARS:
            if v == 'valid' and r[v] == 'true':
                continue
            st.append(r[v])
    while st:
        v = st.pop()
        if v in live or not v.startswith('%dd'):
            continue
        live.add(v)
        k = next(key for key in phis if pname(*key) == v)
        st.extend(res(t) for t in phis[k].values())
    # ---- emit phis (before splitting: the split renames every phi edge that now comes from a join block)
    pos = {n: i for i, n in enumerate([n for n, _ in fn.blocks])}
    for key, inc in phis.items():
        me = pname(*key)
        if me not in live:
            continue
        k, v, b = key
        ins = fn.blocks[pos[b]][1]
        np_ = sum(1 for y in ins if ' = phi ' in y)
        ins.insert(np_, '  %s = phi %s %s' % (me, 'i1' if (v == 'val' and k in bitc) else TY[v], ', '.join('[ %s, %%%s ]' % (res(inc[p]), p) for p in pred[b])))
    # ---- bit chains: define %e_bit = trunc %e (bit 0) right after the extract, where some cache user needs it
    used = set()
    for key in phis:
        if pname(*key) in live:
            used |= {res(v) for v in phis[key].values()}
    for x in rewrite_set:
        used.add(reach[x[1]]['val'])
    for x in cands:
        if cid[x[5]] in bitc and x[4] + '_bit' in used:
            ins = fn.blocks[pos[x[0]]][1]
            q = next(i for i, y in enumerate(ins) if y.lstrip().startswith(x[4] + ' '))
            ins.insert(q + 1, '  %s_bit = trunc i32 %s to i1' % (x[4], x[4]))
    # ---- split every rewritten load: pre (hit test) -> ld (the load) -> join (val phi + rest of the block)
    sites = []
    for n, x in enumerate(rewrite_set):
        b0, d, h, idx, e, c = x
        bi = next(i for i, (bb, ins) in enumerate(fn.blocks) if any(y.lstrip().startswith(d + ' ') for y in ins))
        b, ins = fn.blocks[bi]
        p = next(i for i, y in enumerate(ins) if y.lstrip().startswith(d + ' '))
        q = next(i for i, y in enumerate(ins) if y.lstrip().startswith(e + ' '))
        r = reach[d]
        t = 'dd%d' % n
        pre = ins[:p]
        if r['valid'] == 'true':
            pre += ['  %%%s_h = icmp eq i32 %s, %s' % (t, idx, r['idx'])]
        else:
            pre += ['  %%%s_q = icmp eq i32 %s, %s' % (t, idx, r['idx']), '  %%%s_h = and i1 %s, %%%s_q' % (t, r['valid'], t)]
        hitval = r['val']
        if cid[c] in bitc:
            pre += ['  %%%s_z = zext i1 %s to i32' % (t, r['val'])]; hitval = '%%%s_z' % t
        pre += ['  br i1 %%%s_h, label %%%s_j, label %%%s_ld' % (t, t, t)]
        ld = [ins[p], '  %s_l = extractvalue %%dx.types.ResRet.i32 %s, 0' % (e, d), '  br label %%%s_j' % t]
        join = ['  %s = phi i32 [ %s_l, %%%s_ld ], [ %s, %%%s ]' % (e, e, t, hitval, b)] + [y for i, y in enumerate(ins[p + 1:], p + 1) if i != q]
        fn.blocks[bi:bi + 1] = [[b, pre], [t + '_ld', ld], [t + '_j', join]]
        old, new = '%' + b, '%' + t + '_j'
        pat = re.compile(r'(\[ [^,\]]+, )' + re.escape(old) + r'( \])')
        for bb in fn.blocks:
            skip = 1 if bb[0] == t + '_j' else 0
            bb[1] = bb[1][:skip] + [pat.sub(lambda m_: m_.group(1) + new + m_.group(2), y) if ' = phi ' in y else y for y in bb[1][skip:]]
        sites.append(dict(site=t, block=b0, load=d, chain=cid[c], key='%s+%d' % (c[1], c[2]),
                          cached='valid' if r['valid'] == 'true' else 'phi'))
    names = [b for b, _ in fn.blocks]
    for s in sites:          # block positions (emit keeps the order): pre = the block that branches to the load block
        s['ld_block'] = names.index(s['site'] + '_ld')
        s['pre_block'] = next(i for i, (b, ins) in enumerate(fn.blocks) if ins and ('label %' + s['site'] + '_ld') in ins[-1])
    S._UCACHE.clear()
    return fn.emit(), sites


if __name__ == '__main__':
    t, s = rewrite(open(sys.argv[1], encoding='utf-8').read())
    open(sys.argv[2], 'w', encoding='utf-8').write(t)
    for x in s:
        print(x)
    print(len(s), 'loads guarded')
