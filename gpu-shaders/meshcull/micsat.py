r"""micsat.py - round 4: if-convert the summed-area-table lookups of MIC#0 (e31c389f, MeshletInstanceCulling), at the
DXIL text level. notes/gpu-rewrite-round4.md s3.

Shipped (3 copies, one per culling view): the VSM valid-rect count over the instance's screen rect is read from
tValidRectCountCumulativeMap (t21, Buffer<uint>, read-only) as
    w = T[0]; if (w < 1) skip;  h2 = T[2]; if (!(v < h2)) skip;  h = T[1];  (corner indices i00 i10 i01 from w, h, rect)
    a = c00 ? T[i00] : 0;   b = c10 ? 0 : T[i10];   c = c01 ? 0 : T[i01];   d = T[i11];   hit = (d - (b + c) == -a)
Every load sits behind its own branch, so the GPU waits for each one separately: 7 serial round trips per copy.
Rewrite (same values by construction):
  * T[2] and T[1] are loaded next to T[0] (constant indices; read-only SRV) - the arithmetic stays where it was (it
    divides by w, which is only known to be >= 1 after the first test);
  * the three corner diamonds are folded into their head block: the corner loads become unconditional and each
    phi [T[i], load-block], [0, other] becomes select(c, T[i], 0) (polarity kept). A load the shipped shader would
    not have issued only feeds the arm of a select that is not taken; reads of a read-only SRV have no side effect and
    an out-of-range typed-buffer read cannot fault (D3D12: returns 0; Vulkan robustBufferAccess: some in-buffer value,
    discarded by the select) -> 2 round trips per copy. No float op is touched or moved.

    py -I micsat.py in.ll out.ll
"""
import re, sys, os
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, '..', 'materials'))
from sfb_rewrite import Fn, DEF, TOK  # noqa: E402


def rewrite(text):
    f = Fn(text)
    B = f.blocks
    hm = [m.group(1) for _, ins in B for x in ins for m in [re.match(r'^\s*(%[\w.]+) = call %dx\.types\.Handle @dx\.op\.createHandle\(i32 57, i8 0, i32 \d+, i32 21, i1 false\)$', x)] if m]
    assert len(hm) == 1, hm
    H = hm[0]
    LD = re.compile(r'^\s*(%[\w.]+) = call %dx\.types\.ResRet\.i32 @dx\.op\.bufferLoad\.i32\(i32 68, %dx\.types\.Handle ' + re.escape(H) + r', i32 (%[\w.]+|\d+), i32 undef\)$')
    names = [b for b, _ in B]
    pos = lambda b: names.index(b)          # noqa: E731
    blk = lambda b: B[pos(b)][1]            # noqa: E731

    def succs(b):
        return re.findall(r'label %(\w+)', blk(b)[-1])

    def const_load(b, c):
        for i, x in enumerate(blk(b)):
            m = LD.match(x)
            if m and m.group(2) == str(c):
                return i, m.group(1)
        return None
    copies = []
    for b, ins in B:
        if const_load(b, 0):
            copies.append(b)
    assert len(copies) == 3, copies
    info = []
    for h0 in copies:
        # header chain: h0 (T[0]) -> b1 (T[2]) -> b2 (T[1] + corner index arithmetic)
        b1 = next(s for s in succs(h0) if const_load(s, 2))
        b2 = next(s for s in succs(b1) if const_load(s, 1))
        i0, l0 = const_load(h0, 0)
        e0 = i0 + 1
        assert re.match(r'^\s*%[\w.]+ = extractvalue %dx\.types\.ResRet\.i32 ' + re.escape(l0) + ', 0$', blk(h0)[e0])
        moved = []
        for bb, c in ((b1, 2), (b2, 1)):
            i, l = const_load(bb, c)
            x = blk(bb)[i:i + 2]
            assert re.match(r'^\s*%[\w.]+ = extractvalue %dx\.types\.ResRet\.i32 ' + re.escape(l) + ', 0$', x[1])
            del blk(bb)[i:i + 2]
            moved += x
        blk(h0)[e0 + 1:e0 + 1] = moved
        # fold the corner diamonds into b2
        cur, n = b2, 0
        while True:
            t = blk(cur)[-1]
            m = re.match(r'^\s*br i1 (%[\w.]+), label %(\w+), label %(\w+)$', t)
            if not m:
                break
            c, x, y = m.groups()
            L = J = None
            for a_, b_ in ((x, y), (y, x)):
                li = blk(a_)
                if (li[-1].strip() == 'br label %' + b_ and sum(1 for z in li if LD.match(z)) == 1 and
                        all(not re.search(r'@dx\.op\.(?!bufferLoad)|^\s*store|= phi ', z) for z in li[:-1])):
                    L, J = a_, b_
            if L is None:
                # the last block: one unconditional corner load, then the hit test -> not a diamond
                break
            ph = blk(J)[0]
            pm = re.match(r'^\s*(%[\w.]+) = phi i32 \[ (%[\w.]+), %' + L + r' \], \[ 0, %' + cur + r' \]$', ph)
            assert pm, ph
            assert sum(1 for z in blk(J) if ' = phi ' in z) == 1
            assert set(p for p in names if J in succs(p)) == {L, cur}
            assert [p for p in names if L in succs(p)] == [cur]
            sel = ('  %s = select i1 %s, i32 %s, i32 0' if L == x else '  %s = select i1 %s, i32 0, i32 %s') % (pm.group(1), c, pm.group(2))
            blk(cur)[-1:] = blk(L)[:-1] + [sel] + blk(J)[1:]
            for gone in (L, J):
                del B[pos(gone)]
                names.remove(gone)
            # successors of the folded J now come from cur
            for s in succs(cur):
                blk(s)[:] = [z.replace(', %' + J + ' ]', ', %' + cur + ' ]') if ' = phi ' in z else z for z in blk(s)]
            n += 1
        assert n == 3, (h0, n)
        nl = sum(1 for z in blk(cur) if LD.match(z))
        assert nl == 4, (h0, nl)
        info.append((h0, b2, n))
    return f.emit(), info


if __name__ == '__main__':
    a = sys.argv[1:]
    t, info = rewrite(open(a[0], encoding='utf-8').read())
    open(a[1], 'w', encoding='utf-8', newline='\n').write(t)
    print(info)
