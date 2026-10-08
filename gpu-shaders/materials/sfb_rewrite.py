r"""sfb_rewrite.py - generator for the SamplerFeedback MinMip "corner dedupe" rewrite (SFB-DEDUPE) of material shaders.

Every material that streams its textures ('Sfb' in the group name) clamps each texture sample's LOD with
    clamp = max(MinMip[i00], MinMip[i01], MinMip[i10], MinMip[i11])        (4 x Buffer<uint>.Load, uitofp, 3 x FMax)
where i00..i11 are the 4 MinMip tiles under the sample's (u, v) +- margin. Away from a tile border all four indices are
the same tile, so the GPU fetches the same dword four times (4 VMEM instructions per texture per wave).

Rewrite (same output by construction: equal index -> equal value of a read-only SRV):
    v00 = Load(i00)                                   (kept)
    if (i01 != i00 || i10 != i00 || i11 != i00) {     (divergent if; skipped by the whole wave when no lane needs it)
        v01 = Load(i01); v10 = Load(i10); v11 = Load(i11)
    } else { v01 = v10 = v11 = v00 }
Only the three extra loads move into the new block; everything that depends on them follows the join, everything else
keeps its place and order. The rest of the module text is untouched; values are renumbered sequentially at the end.

    py -I sfb_rewrite.py in.ll out.ll          -> prints the number of groups rewritten
    import sfb_rewrite; new_text, n = sfb_rewrite.rewrite(text)
"""
import re, sys

LOAD = re.compile(r'^\s*(%[\w.]+) = call %dx\.types\.ResRet\.i32 @dx\.op\.bufferLoad\.i32\(i32 68, %dx\.types\.Handle (%[\w.]+), i32 (%[\w.]+|-?\d+), i32 undef\)')
EXTR0 = re.compile(r'^\s*(%[\w.]+) = extractvalue %dx\.types\.ResRet\.i32 (%[\w.]+), 0$')
U2F = re.compile(r'^\s*(%[\w.]+) = uitofp i32 (%[\w.]+) to float$')
FMAX = re.compile(r'^\s*(%[\w.]+) = call float @dx\.op\.binary\.f32\(i32 35, float (%[\w.]+), float (%[\w.]+)\)$')
DEF = re.compile(r'^\s*(%[\w.]+) = ')
TOK = re.compile(r'%[\w.]+')
MODE = 'tree'   # 'tree': move loads+uitofp+FMax tree into the if (fewer VALU); 'loads': move only the 3 loads
PURE_CALL = re.compile(r'@dx\.op\.(bufferLoad|cbufferLoadLegacy|binary|unary|tertiary|dot[234]|loadInput|createHandle|sample|textureLoad|'
                       r'derivCoarse|derivFine|getDimensions|quaternary|bitcastI32toF32|bitcastF32toI32|legacyF32ToF16|legacyF16ToF32|makeDouble|splitDouble)')


def strip(l):
    i = l.find('  ;')
    return (l[:i] if i >= 0 else l).rstrip()


class Fn:
    """the @main body as blocks [(label, [instr...])] with every unnamed value/block given a name"""

    def __init__(self, text):
        L = text.split('\n')
        s = next(i for i, l in enumerate(L) if l.startswith('define void @main('))
        e = next(i for i in range(s, len(L)) if L[i] == '}')
        self.head, self.tail, body = L[:s + 1], L[e:], L[s + 1:e]
        blocks, cur = [], ['0', []]
        bnums = {'0'}
        for l in body:
            m = re.match(r'^; <label>:(\d+)', l)
            if m:
                if cur[1] or cur[0] != '0':
                    blocks.append(cur)
                cur = [m.group(1), []]; bnums.add(m.group(1)); continue
            m = re.match(r'^([\w.]+):', l)
            if m and not l.startswith(' '):
                blocks.append(cur); cur = [m.group(1), []]; bnums.add(m.group(1)); continue
            l = strip(l)
            if l.strip():
                cur[1].append(l)
        blocks.append(cur)

        def ren(t):
            n = t.group(0)[1:]
            if n.isdigit():
                return '%b' + n if n in bnums else '%v' + n
            return t.group(0)
        self.blocks = [['b' + b if b.isdigit() else b, [TOK.sub(ren, x) for x in ins]] for b, ins in blocks]

    def emit(self):
        """renumber: named -> sequential unnamed (LLVM implicit numbering: entry block 0, then defs/blocks in order)"""
        num, n = {}, 0
        for bi, (b, ins) in enumerate(self.blocks):
            num['%' + b] = '%' + str(n); n += 1
            for x in ins:
                m = DEF.match(x)
                if m:
                    num[m.group(1)] = '%' + str(n); n += 1
        out = list(self.head)
        for bi, (b, ins) in enumerate(self.blocks):
            if bi:
                out.append('')
                out.append('; <label>:%s' % num['%' + b][1:])
            for x in ins:
                out.append(TOK.sub(lambda t: num.get(t.group(0), t.group(0)), x))
        return '\n'.join(out + self.tail)


_UCACHE = {}


def _rhs(x):
    return x.split(' = ', 1)[1] if DEF.match(x) else x


def users(ins, v):
    """instruction indices in this block that use value v (index cached per block list object)"""
    key = id(ins)
    c = _UCACHE.get(key)
    if c is None or c[0] is not ins:
        m = {}
        for i, x in enumerate(ins):
            for t in set(TOK.findall(_rhs(x))):
                m.setdefault(t, []).append(i)
        c = _UCACHE[key] = (ins, m)
    return c[1].get(v, [])


def find_groups(fn, srv_handles, start=0):
    """yield (block index, [load positions x4]) for 4 loads of one SRV whose (only) extract0 -> uitofp feed a 3-FMax tree"""
    for bi in range(start, len(fn.blocks)):
        b, ins = fn.blocks[bi]
        loads = []
        for i, x in enumerate(ins):
            m = LOAD.match(x)
            if m and m.group(2) in srv_handles:
                loads.append((i, m.group(1), m.group(2), m.group(3)))
        used = set()
        for k in range(len(loads) - 3):
            q = loads[k:k + 4]
            if any(t[0] in used for t in q) or len({t[2] for t in q}) != 1:
                continue
            ok, fl = True, []
            for (i, d, h, idx) in q:
                u = users(ins, d)
                if len(u) != 1 or not EXTR0.match(ins[u[0]]) or len(users(ins, EXTR0.match(ins[u[0]]).group(1))) < 1:
                    ok = False; break
                e = EXTR0.match(ins[u[0]]).group(1)
                ue = users(ins, e)
                if len(ue) != 1 or not U2F.match(ins[ue[0]]):
                    ok = False; break
                fl.append(U2F.match(ins[ue[0]]).group(1))
            if not ok:
                continue
            # the four floats must be combined by exactly three FMax into one value
            leaves, frontier, nodes = set(fl), set(fl), 0
            for i, x in enumerate(ins):
                m = FMAX.match(x)
                if m and m.group(2) in frontier and m.group(3) in frontier:
                    frontier -= {m.group(2), m.group(3)}; frontier.add(m.group(1)); nodes += 1
            if nodes != 3 or len(frontier) != 1:
                continue
            # values only used inside this block (extracts / floats): checked across the whole function
            if any(cross_block_use(fn, bi, v) for v in fl + [t[1] for t in q]):
                continue
            used.update(t[0] for t in q)
            yield bi, [t[0] for t in q]


def cross_block_use(fn, bi, v):
    return any(j != bi and users(ins, v) for j, (b, ins) in enumerate(fn.blocks))


def transform(fn, bi, lpos, k):
    b, ins = fn.blocks[bi]
    L = [LOAD.match(ins[p]) for p in lpos]
    a_idx = [m.group(3) for m in L]
    E = []
    for p, m in zip(lpos, L):
        u = users(ins, m.group(1))[0]
        E.append((u, EXTR0.match(ins[u]).group(1)))
    # uitofp of each extract, then the 3-FMax tree over the four floats
    U = [users(ins, e)[0] for _, e in E]
    fl = [U2F.match(ins[u]).group(1) for u in U]
    frontier, nodes, root = set(fl), [], None
    for i, x in enumerate(ins):
        m = FMAX.match(x)
        if m and m.group(2) in frontier and m.group(3) in frontier:
            frontier -= {m.group(2), m.group(3)}; frontier.add(m.group(1)); nodes.append(i); root = m.group(1)
    inner = set(fl[1:]) | {DEF.match(ins[i]).group(1) for i in nodes[:-1]}
    tree_ok = len(nodes) == 3 and all(set(users(ins, v)) <= set(nodes) for v in inner) and         not any(cross_block_use(fn, bi, v) for v in inner)
    if tree_ok and MODE == 'tree':
        # loads + uitofp + the whole max tree go into the if; the else value is f00 (max of four equal floats)
        M = set(lpos[1:]) | {u for u, _ in E[1:]} | set(U[1:]) | set(nodes)
        exports = [(root, fl[0], 'float')]
    else:
        M = set(lpos[1:]) | {u for u, _ in E[1:]}
        exports = [(e, E[0][1], 'i32') for _, e in E[1:]]
    last = max(M)
    mvals = {DEF.match(ins[i]).group(1) for i in M}
    dep = set()
    for i in range(min(M), last):
        if i in M:
            continue
        x = ins[i]
        rhs = x.split(' = ', 1)[1] if DEF.match(x) else x
        if any(re.search(re.escape(v) + r'(?![\w.])', rhs) for v in mvals):
            dep.add(i)
            if DEF.match(x):
                mvals.add(DEF.match(x).group(1))
    for i in range(min(M), last):
        x = ins[i]
        if (' call ' in x and not PURE_CALL.search(x)) or not DEF.match(x):
            raise ValueError('side effect inside the group range: ' + x)
    pre = [ins[i] for i in range(0, last + 1) if i not in M and i not in dep]
    t = 'sfb%d' % k
    c = ['  %%%s_c1 = icmp ne i32 %s, %s' % (t, a_idx[1], a_idx[0]),
         '  %%%s_c2 = icmp ne i32 %s, %s' % (t, a_idx[2], a_idx[0]),
         '  %%%s_c3 = icmp ne i32 %s, %s' % (t, a_idx[3], a_idx[0]),
         '  %%%s_o1 = or i1 %%%s_c1, %%%s_c2' % (t, t, t),
         '  %%%s_n = or i1 %%%s_o1, %%%s_c3' % (t, t, t),
         '  br i1 %%%s_n, label %%%s_ld, label %%%s_j' % (t, t, t)]
    ld = []
    for i in sorted(M):
        x = ins[i]
        for e, _, _ in exports:
            x = re.sub(re.escape(e) + r'(?![\w.])', e + '_l', x)
        ld.append(x)
    ld.append('  br label %%%s_j' % t)
    phis = ['  %s = phi %s [ %s_l, %%%s_ld ], [ %s, %%%s ]' % (e, ty, e, t, fb, b) for e, fb, ty in exports]
    join = phis + [ins[i] for i in sorted(dep)] + ins[last + 1:]
    fn.blocks[bi:bi + 1] = [[b, pre + c], [t + '_ld', ld], [t + '_j', join]]
    # the old block's terminator now lives in the join block: every phi edge "from b" becomes "from join" (this includes
    # b's own phis when b was a self-loop) - except the join's new phis, whose fallback edge really comes from b
    old, new = '%' + b, '%' + t + '_j'
    pat = re.compile(r'(\[ [^,\]]+, )' + re.escape(old) + r'( \])')
    for bb in fn.blocks:
        skip = len(phis) if bb[0] == t + '_j' else 0
        bb[1] = bb[1][:skip] + [pat.sub(lambda m_: m_.group(1) + new + m_.group(2), x) if ' = phi ' in x else x for x in bb[1][skip:]]


def srv_handles(fn):
    out = set()
    for b, ins in fn.blocks:
        for x in ins:
            m = re.match(r'^\s*(%[\w.]+) = call %dx\.types\.Handle @dx\.op\.createHandle\(i32 57, i8 0,', x)
            if m:
                out.add(m.group(1))
    return out


def rewrite(text):
    fn = Fn(text)
    h = srv_handles(fn)
    k, start = 0, 0
    while True:
        g = next(find_groups(fn, h, start), None)
        if g is None:
            break
        transform(fn, g[0], g[1], k)
        start = g[0] + 2          # the rest of that block is now the join block; earlier blocks have no groups left
        k += 1
    _UCACHE.clear()
    return fn.emit(), k


if __name__ == '__main__':
    t, n = rewrite(open(sys.argv[1], encoding='utf-8').read())
    open(sys.argv[2], 'w', encoding='utf-8').write(t)
    print(n, 'groups rewritten')
