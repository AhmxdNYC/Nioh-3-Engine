r"""dedupe_loads.py - LOAD-DEDUPE: remove read-only loads that repeat an earlier, dominating load with identical operands.

A dx.op rawBufferLoad / bufferLoad / textureLoad on an SRV handle (createHandle class 0: read-only for the whole draw)
returns the same value for the same operands, so a second identical call whose block is dominated by the first's is
pure redundancy. DXC cannot CSE them when a storeOutput (a side-effecting call) sits in between - the shipped vertex
shaders re-load their instance matrices after writing outputs. The rewrite replaces every use of the repeat with the
first load's value and deletes the repeat: fewer instructions and loads; nothing else moves. Modules with UAVs are left
alone (no aliasing question at all).

    py -I dedupe_loads.py in.ll out.ll [--max-dist N]     (N: only repeats within N instructions of the first, same block)
    import dedupe_loads; text, n = dedupe_loads.rewrite(text, max_dist=None)
"""
import os, re, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import sfb_rewrite as S

LD = re.compile(r'^\s*(%[\w.]+) = (call %dx\.types\.ResRet\.\w+ @dx\.op\.(?:rawBufferLoad|bufferLoad|textureLoad)\.\w+\(i32 \d+, '
                r'%dx\.types\.Handle (%[\w.]+),.*)$')


def rewrite(text, max_dist=None):
    if re.search(r'createHandle\(i32 57, i8 1,', text) or re.search(r'@dx\.op\.(bufferStore|rawBufferStore|textureStore|atomic)', text):
        return text, 0
    fn = S.Fn(text)
    srv = S.srv_handles(fn)
    names = [b for b, _ in fn.blocks]
    succ = {b: re.findall(r'label %([\w.]+)', ins[-1]) if ins else [] for b, ins in fn.blocks}
    pred = {b: [p for p in names if b in succ[p]] for b in names}
    dom = {b: set(names) for b in names}; dom[names[0]] = {names[0]}
    ch = True
    while ch:
        ch = False
        for b in names[1:]:
            ps = [dom[p] for p in pred[b]]
            nd = ({b} | set.intersection(*ps)) if ps else {b}
            if nd != dom[b]:
                dom[b] = nd; ch = True
    seen, repl = {}, {}
    for bi, (b, ins) in enumerate(fn.blocks):
        for i, x in enumerate(ins):
            m = LD.match(x)
            if not m or m.group(3) not in srv:
                continue
            key = m.group(2)
            hit = None
            for (b0, i0, d0) in seen.get(key, []):
                if b0 in dom[b] and (max_dist is None or (b0 == b and i - i0 <= max_dist)):
                    hit = d0; break
            if hit:
                repl[m.group(1)] = hit
            else:
                seen.setdefault(key, []).append((b, i, m.group(1)))
    if not repl:
        return text, 0
    pat = re.compile('|'.join(re.escape(k) + r'(?![\w.])' for k in repl))
    for blk in fn.blocks:
        blk[1] = [pat.sub(lambda t: repl[t.group(0)], x) for x in blk[1] if not (S.DEF.match(x) and S.DEF.match(x).group(1) in repl)]
    return fn.emit(), len(repl)


if __name__ == '__main__':
    md = int(sys.argv[sys.argv.index('--max-dist') + 1]) if '--max-dist' in sys.argv else None
    t, n = rewrite(open(sys.argv[1], encoding='utf-8').read(), md)
    open(sys.argv[2], 'w', encoding='utf-8').write(t)
    print(n, 'repeated loads removed')
