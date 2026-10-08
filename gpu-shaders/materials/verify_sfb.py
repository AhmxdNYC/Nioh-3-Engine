r"""verify_sfb.py - prove a rewritten material pixel shader bit-exact against the shipped one (psim.py model).

    py -I verify_sfb.py orig.ll new.ll [--pixels 600] [--scenes 6]

Random scenes: every cbuffer dword random (floats in [-2, 2]); SamplerFeedbackConstants filled with real-looking
controls (MinMip grid W,H in 1..255, wrap/clamp mode bits, MinMip base offsets); every typed buffer random (MinMip values
0..15); attributes random in [-2, 3] with 1/3 of the pixels snapped near MinMip tile borders. Outputs, discard and UAV
writes must be bit-identical on every pixel; the rewrite's new blocks must be both taken and skipped.
"""
import os, random, re, struct, sys
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import psim  # noqa: E402
import dxil_interp as DI  # noqa: E402

GRID = [1, 2, 3, 4, 8, 16, 32, 64, 128, 255]


def cb_bytes(name, rng, size=4096):
    out = bytearray()
    if name == 'SamplerFeedbackConstants':
        for i in range(16):
            out += struct.pack('<I', rng.randrange(0, 2000))                                  # nSFBMMMOffsets
        for i in range(16):
            out += struct.pack('<I', rng.randrange(0, 2000))                                  # nSFBFBMOffsets
        for i in range(16):
            w, h = rng.choice(GRID), rng.choice(GRID)
            out += struct.pack('<I', (h << 24) | (w << 16) | rng.choice([0, 0x4000, 0x8000, 0xc000]) | rng.randrange(0, 0x4000))
        for i in range(16):
            out += struct.pack('<f', rng.uniform(-2, 2))                                      # fSFBMipBias
    mix = rng.random()          # per-scene mixture: some scenes mostly floats, some mostly ints / SFB-like words
    while len(out) < size:
        r = rng.random()
        if r < 0.25 * mix:
            out += struct.pack('<I', rng.randrange(0, 2000))
        elif r < 0.5 * mix:
            out += struct.pack('<I', (rng.choice(GRID) << 24) | (rng.choice(GRID) << 16) | rng.choice([0, 0x4000, 0x8000, 0xc000]) | rng.randrange(0, 0x4000))
        elif r < 0.5 * mix + 0.15:
            out += struct.pack('<I', rng.randrange(0, 8))
        elif r < 0.5 * mix + 0.6:
            out += struct.pack('<f', rng.uniform(0, 1))
        else:
            out += struct.pack('<f', rng.uniform(-2, 2))
    return bytes(out)


def scene_for(ll, rng):
    b = {}
    uavs = []
    for cls, reg, name, ty, fmt, dim in psim.bindings(ll):
        if ty == 'cbuffer':
            b[(cls, reg)] = DI.CBuffer(cb_bytes(name, rng))
        elif ty == 'sampler':
            b[(cls, reg)] = psim.Res('%s@s%d' % (name, reg))
        elif ty == 'UAV':
            u = psim.UAVLog('%s@u%d' % (name, reg)); b[(cls, reg)] = u; uavs.append(u)
        elif dim in ('buf',):
            isint = fmt in ('u32', 'i32')
            n = 1 << 17
            data = [rng.randrange(0, 16) for _ in range(n)] if isint else [tuple(rng.uniform(-2, 2) for _ in range(4)) for _ in range(4096)]
            b[(cls, reg)] = DI.TypedBuffer(data, isint=isint, comps=1 if isint else 4)
        elif dim == 'r/o' and fmt == 'byte':
            # ByteAddressBuffer (bindless texture register info: per-texture SFB words, offsets, sizes): byte-addressed,
            # filled like a cbuffer (mix of SFB-style words, small ints and floats)
            b[(cls, reg)] = DI.StructBuffer(cb_bytes(name, rng, 1 << 16), 1)
        elif dim in ('r/o', 'struct', 'rawbuf', 'r/w'):
            b[(cls, reg)] = DI.StructBuffer(bytes(rng.randrange(256) for _ in range(1 << 14)), 16)
        else:
            b[(cls, reg)] = psim.Res('%s@t%d' % (name, reg))
    return DI.Scene(Bindless(b)), uavs


class Bindless(dict):
    """unbounded (bindless) SRV/sampler ranges: any register not in the table is a distinct oracle resource"""

    def get(self, k, d=None):
        if k not in self and k[0] in (0, 3):
            # register inside an array binding (e.g. t12 x16): same kind as the array's first register, own content
            lo = max((r for c, r in self if c == k[0] and r < k[1]), default=None)
            base = dict.get(self, (k[0], lo)) if lo is not None else None
            rng = random.Random(k[1] * 7919 + k[0])
            if isinstance(base, DI.StructBuffer):
                self[k] = DI.StructBuffer(bytes(rng.randrange(256) for _ in range(len(base.b))), base.stride)
            elif isinstance(base, DI.TypedBuffer):
                self[k] = base
            else:
                self[k] = psim.Res('%s%d' % ('t' if k[0] == 0 else 's', k[1]))
        return dict.get(self, k, d)


def pixels_for(ll, rng, n):
    els = psim.inputs(ll)
    px = []
    for i in range(n):
        p = {}
        for sid, (sem, fmt) in enumerate(els):
            for c in range(4):
                if sem == 'SV_Position':
                    v = [rng.uniform(0, 1920), rng.uniform(0, 1080), rng.uniform(0, 1), 1.0][c]
                elif fmt in ('uint', 'int'):
                    v = rng.randrange(0, 64)
                elif i % 3 == 0:
                    g = rng.choice(GRID)                     # near a MinMip tile border of a random grid
                    v = DI.rf(rng.randrange(-2 * g, 3 * g) / g + rng.choice([-1, 1]) * rng.uniform(0, 0.02))
                else:
                    v = DI.rf(rng.uniform(-2, 3))
                p[(sid, 0, c)] = v
        px.append(p)
    return px


def new_block_indices(sh):
    return [i for i, b in enumerate(sh.m.blocks)]


def verify(orig_ll, new_ll, npx=600, nsc=6, seed=1):
    A = psim.PShader(orig_ll, 'orig'); B = psim.PShader(new_ll, 'new')
    covB = [0] * len(B.m.blocks)
    for s in range(nsc):
        rng = random.Random(seed * 1000 + s)
        scA, uA = scene_for(orig_ll, random.Random(seed * 1000 + s))
        scB, uB = scene_for(new_ll, random.Random(seed * 1000 + s))
        px = pixels_for(orig_ll, rng, npx)
        ra, _ = A.run_pixels(scA, px)
        rb, _ = B.run_pixels(scB, px, covB)
        for i, (x, y) in enumerate(zip(ra, rb)):
            if x != y:
                return False, 'scene %d pixel %d differs: %r vs %r' % (s, i, x, y), covB
        if [u.writes for u in uA] != [u.writes for u in uB]:
            return False, 'scene %d UAV writes differ' % s, covB
    return True, 'ok', covB


def sfb_block_coverage(new_ll, covB):
    """(taken, skipped) counts of every rewritten group: blocks whose body is the 3 extra loads + br to the join"""
    sh = psim.PShader(new_ll, 'cov')
    res = []
    for i, b in enumerate(sh.m.blocks):
        ins = b.instrs
        if len(ins) in (7, 13) and sum(1 for x in ins if x.op == 'call' and 'bufferLoad' in x.extra.get('fn', '')) == 3:
            pred = i - 1
            res.append((covB[i], covB[pred] - covB[i]))
    return res


if __name__ == '__main__':
    a = sys.argv[1:]
    npx = int(a[a.index('--pixels') + 1]) if '--pixels' in a else 600
    nsc = int(a[a.index('--scenes') + 1]) if '--scenes' in a else 6
    o, n = open(a[0], encoding='utf-8').read(), open(a[1], encoding='utf-8').read()
    ok, msg, cov = verify(o, n, npx, nsc)
    print('BIT-EXACT' if ok else 'MISMATCH', msg, 'groups(taken,skipped)=', sfb_block_coverage(n, cov))


def lint(orig_ll, new_ll):
    """structural template check of every rewritten group in new_ll (independent of execution coverage):
    pre block ends 'icmp ne x3 / or / or / br' comparing the 3 moved loads' indices with the kept load's index (same
    SRV handle); the load block holds exactly those 3 loads (+ extracts, uitofp, FMax tree) and branches to the join;
    every join phi's fallback is the kept load's extract (or its uitofp) and comes from the pre block.
    Also: the new module has exactly 3 bufferLoads... per group moved, and the same total number of dx.op calls as the
    original. Returns (ok, message)."""
    import collections
    sh = psim.PShader(new_ll, 'lint')
    blocks = {b.name: b for b in sh.m.blocks}
    defs = {i.dest: i for b in sh.m.blocks for i in b.instrs if i.dest}
    n = 0
    for b in sh.m.blocks:
        ins = b.instrs
        if len(ins) < 6 or ins[-1].op != 'br' or len(ins[-1].extra.get('targets', [])) != 2:
            continue
        c = ins[-6:-1]
        if not (c[0].op == c[1].op == c[2].op == 'icmp' and c[3].op == c[4].op == 'or'):
            continue
        if not ({a.v for a in c[3].args} == {c[0].dest, c[1].dest} and {a.v for a in c[4].args} == {c[3].dest, c[2].dest}
                and ins[-1].args[0].v == c[4].dest):
            return False, 'group at %s: branch condition is not c1|c2|c3' % b.name
        ld, j = ins[-1].extra['targets']
        L = blocks[ld].instrs
        loads = [x for x in L if x.op == 'call' and 'bufferLoad' in x.extra['fn']]
        if len(loads) != 3 or L[-1].op != 'br' or L[-1].extra['targets'] != [j]:
            return False, 'group at %s: load block shape' % b.name
        k0 = c[0].args[1].v
        if any(x.args[1].v != k0 for x in c[:3]) or [x.args[0].v for x in c[:3]] != [x.args[2].v for x in loads]:
            return False, 'group at %s: conditions do not compare the moved loads with one index' % b.name
        first = [x for x in ins if x.op == 'call' and 'bufferLoad' in x.extra['fn'] and x.args[2].v == k0]
        if len(first) != 1 or any(x.args[1].v != first[0].args[1].v for x in loads):
            return False, 'group at %s: kept load / handle mismatch' % b.name
        e0 = [x for x in ins if x.op == 'extractvalue' and x.args[0].v == first[0].dest]
        u0 = [x for x in ins if x.op == 'uitofp' and e0 and x.args[0].v == e0[0].dest]
        ok_fb = {e0[0].dest} | ({u0[0].dest} if u0 else set())
        phis = [x for x in blocks[j].instrs if x.op == 'phi']
        if not phis:
            return False, 'group at %s: no join phi' % b.name
        for p in phis:
            fr = p.extra['from']
            if sorted(fr) != sorted([ld, b.name]) or p.args[fr.index(b.name)].v not in ok_fb:
                return False, 'group at %s: join phi fallback is not the kept load' % b.name
        n += 1
    co = collections.Counter(re.findall(r'@dx\.op\.(\w+)\.', orig_ll.split('define void @main', 1)[1]))
    cn = collections.Counter(re.findall(r'@dx\.op\.(\w+)\.', new_ll.split('define void @main', 1)[1]))
    if co != cn:
        return False, 'dx.op call counts differ: %r' % {k: (co[k], cn[k]) for k in set(co) | set(cn) if co[k] != cn[k]}
    return True, '%d groups match the template' % n
