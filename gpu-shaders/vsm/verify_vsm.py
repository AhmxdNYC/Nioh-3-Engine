r"""verify_vsm.py - prove the VSM-FLAG-DEDUPE rewrite of WDO#14 bit-exact against the shipped shader (psim model).

    py -I verify_vsm.py orig.ll new.ll [--pixels 400] [--scenes 12]

psim (gpu-shaders/materials/psim.py): every pixel alone; buffer loads / cbuffers REAL; texture samples, sampleCmp and
textureLoad are an oracle hashed from their exact operand bits (equal outputs => every sample got bit-identical
operands). The VSM path only runs with a sane cbuffer, so scenes are built per field of the WDO-PtSrbSsmVsm $Globals:
mP2W ~ identity (world pos = (tc.xy, depth)), vEye near, nArraySize_VirtualSM 1..9 levels, nClipLevelMin/bias small,
mW2S_VirtualSM mapping the pixel into (0,1) at a random scale, clip origins, nValidClipmapFlag masks, page grid W x H
(scMinMipMapVirtualSM), PCF step (scVirtualSM) from far below a page to several pages, cross-fade bytes, bSM on/off;
the page-flag buffer is resident with p in {0.3, 0.7, 0.95} (upper bits random); every other
scene is a stress scene (taps pages apart, p in {0.1, 0.2}) so the cross-fade cache also misses. Every other dword: random float.
Outputs must be bit-identical on every pixel; per guarded load the report shows (hit = load skipped, miss = load run).
"""
import os, random, re, struct, sys
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, '..', 'materials'))
import psim  # noqa: E402
import dxil_interp as DI  # noqa: E402


def cb_layout(ll):
    """{field name: byte offset} of $Globals from the dumpbin header"""
    out = {}
    for m in re.finditer(r'^;\s+(?:row_major )?\w+ (\w+)(?:\[\d+\])?;\s+; Offset:\s+(\d+)', ll, re.M):
        out[m.group(1)] = int(m.group(2))
    return out


def globals_bytes(ll, rng, kernel=None, stress=False, size=2368):
    L = cb_layout(ll)
    w = [struct.pack('<f', rng.uniform(-1, 1)) for _ in range(size // 4)]

    def f(off, v):
        w[off // 4] = struct.pack('<f', v)

    def u(off, v):
        w[off // 4] = struct.pack('<I', v & 0xffffffff)
    if 'mP2W' in L:                                   # world = (tc.x, tc.y, depth) (+ a little shear)
        for r in range(4):
            for c in range(4):
                f(L['mP2W'] + 16 * r + 4 * c, (1.0 if r == c else 0.0) + (rng.uniform(-0.05, 0.05) if r < 3 and c < 3 else 0.0))
    if 'vEye' in L:
        for c in range(3):
            f(L['vEye'] + 4 * c, rng.uniform(-3, 3))
    for k, n in (('bSM', 3),):
        if k in L:
            for i in range(n):
                u(L[k] + 16 * i, rng.random() < 0.6)
        if kernel is not None:   # bSM[2] -> 2x2 PCF, else bSM[1] -> 4x4 PCF, else 1 tap (the three VSM kernels)
            u(L[k] + 32, kernel == 0); u(L[k] + 16, kernel == 1)
    if 'nClipLevelMin' in L:
        u(L['nClipLevelMin'], rng.randrange(0, 3))
    nlev = rng.randrange(1, 10)
    if 'nArraySize_VirtualSM' in L:
        u(L['nArraySize_VirtualSM'], nlev)
    if 'nSubSdw' in L:
        u(L['nSubSdw'], rng.randrange(0, 3))
    if 'nClipmapLevelBias' in L:
        u(L['nClipmapLevelBias'], rng.randrange(0, 4))
    if 'mW2S_VirtualSM' in L:
        s = rng.choice([1e-3, 0.02, 0.1, 0.3])
        for lv in range(16):
            base = L['mW2S_VirtualSM'] + 64 * lv
            for r in range(3):
                for c in range(4):
                    f(base + 16 * r + 4 * c, rng.uniform(-s, s))
            f(base + 48, rng.uniform(0.2, 0.8)); f(base + 52, rng.uniform(0.2, 0.8)); f(base + 56, rng.uniform(0, 1)); f(base + 60, 1.0)
    if 'vClipOriginOffset' in L:
        for lv in range(16):
            f(L['vClipOriginOffset'] + 16 * lv, rng.uniform(0, 1)); f(L['vClipOriginOffset'] + 16 * lv + 4, rng.uniform(0, 1))
    if 'nValidClipmapFlag' in L:
        u(L['nValidClipmapFlag'], rng.choice([0xffff, 0xffff, rng.randrange(0, 1 << 16), 0xffff ^ (1 << rng.randrange(0, 9))]))
    if 'scMinMipMapVirtualSM' in L:
        W, H = (64, 64) if stress else (rng.choice([4, 8, 16, 32, 64]), rng.choice([4, 8, 16, 32, 64]))
        f(L['scMinMipMapVirtualSM'], W); f(L['scMinMipMapVirtualSM'] + 4, H); f(L['scMinMipMapVirtualSM'] + 8, W * H)
    if 'fVirtualShadowDefaultValue' in L:
        f(L['fVirtualShadowDefaultValue'], rng.uniform(0, 1))
    if 'nCrossFadeParam_VirtualSM' in L:
        lo = rng.randrange(0, 256); hi = rng.choice([lo, rng.randrange(0, 256)])
        u(L['nCrossFadeParam_VirtualSM'], lo | (hi << 8) | (rng.randrange(0, 1 << 16) << 16))
    if 'scVirtualSM' in L:
        for c in range(2):
            f(L['scVirtualSM'] + 4 * c, rng.choice([0.05, 0.1] if stress else [1e-4, 1e-3, 1 / 256, 1 / 64, 0.05]))
    return b''.join(w)


def scene_for(ll, rng, kernel=None, stress=False):
    """stress: PCF taps several pages apart and few resident pages (cross-fade cache misses need a run of
    non-resident taps before a resident one)"""
    b = {}
    pres = rng.choice([0.1, 0.2] if stress else [0.3, 0.7, 0.95])
    for cls, reg, name, ty, fmt, dim in psim.bindings(ll):
        if ty == 'cbuffer':
            b[(cls, reg)] = DI.CBuffer(globals_bytes(ll, rng, kernel, stress))
        elif ty == 'sampler':
            b[(cls, reg)] = psim.Res('%s@s%d' % (name, reg))
        elif dim == 'buf':
            data = [(rng.randrange(0, 1 << 31) << 1) & 0xfffffffe | (rng.random() < pres) for _ in range(1 << 17)]
            b[(cls, reg)] = DI.TypedBuffer(data, isint=True, comps=1)
        else:
            b[(cls, reg)] = psim.Res('%s@t%d' % (name, reg))
    return DI.Scene(b)


def pixels_for(ll, rng, n):
    els = psim.inputs(ll)
    px = []
    for i in range(n):
        p = {}
        for sid, (sem, fmt) in enumerate(els):
            for c in range(4):
                if sem == 'SV_Position':
                    v = [rng.uniform(0, 1920), rng.uniform(0, 1080), rng.uniform(0, 1), 1.0][c]
                else:
                    v = rng.uniform(0, 1)
                p[(sid, 0, c)] = DI.rf(v)
        px.append(p)
    return px


def verify(orig_ll, new_ll, npx=400, nsc=12, seed=14):
    A = psim.PShader(orig_ll, 'orig'); B = psim.PShader(new_ll, 'new')
    covB = [0] * len(B.m.blocks)
    for s in range(nsc):
        scA = scene_for(orig_ll, random.Random(seed * 1000 + s), s % 3, s % 6 >= 3)
        scB = scene_for(new_ll, random.Random(seed * 1000 + s), s % 3, s % 6 >= 3)
        px = pixels_for(orig_ll, random.Random(seed * 7777 + s), npx)
        ra, _ = A.run_pixels(scA, px)
        rb, _ = B.run_pixels(scB, px, covB)
        for i, (x, y) in enumerate(zip(ra, rb)):
            if x != y:
                return False, 'scene %d pixel %d differs: %r vs %r' % (s, i, x, y), covB
    return True, 'ok', covB


def site_coverage(sites, covB):
    """[(site, hit = load skipped, miss = load run)]"""
    return [(s['site'], covB[s['pre_block']] - covB[s['ld_block']], covB[s['ld_block']]) for s in sites]


def lint(orig_ll, new_ll, nsites):
    """static checks: same dx.op call multiset (no load added or removed, only moved); every bufferLoad on the flag SRV
    that sits alone in a block is preceded by a pre block whose branch skips it on an 'icmp eq' of its index"""
    import collections
    co = collections.Counter(re.findall(r'@dx\.op\.(\w+)\.', orig_ll.split('define void @main', 1)[1]))
    cn = collections.Counter(re.findall(r'@dx\.op\.(\w+)\.', new_ll.split('define void @main', 1)[1]))
    if co != cn:
        return False, 'dx.op call counts differ: %r' % {k: (co[k], cn[k]) for k in set(co) | set(cn) if co[k] != cn[k]}
    sh = psim.PShader(new_ll, 'lint')
    blocks = {b.name: b for b in sh.m.blocks}
    n = 0
    for b in sh.m.blocks:
        ins = b.instrs
        if ins and ins[-1].op == 'br' and len(ins[-1].extra.get('targets', [])) == 2:
            j, ld = ins[-1].extra['targets']
            L = blocks[ld].instrs
            if len(L) == 3 and L[0].op == 'call' and 'bufferLoad' in L[0].extra['fn'] and L[1].op == 'extractvalue' \
                    and L[2].op == 'br' and L[2].extra['targets'] == [j]:
                cmp = [x for x in ins if x.op == 'icmp' and x.extra.get('pred') == 'eq' and L[0].args[2].v in [a.v for a in x.args]]
                if not cmp:
                    return False, 'site at %s: no eq test of the load index' % b.name
                n += 1
    if n != nsites:
        return False, '%d guarded loads found, %d expected' % (n, nsites)
    return True, '%d guarded loads match the template; dx.op calls %d (same multiset)' % (n, sum(cn.values()))


if __name__ == '__main__':
    import flag_dedupe
    a = sys.argv[1:]
    npx = int(a[a.index('--pixels') + 1]) if '--pixels' in a else 400
    nsc = int(a[a.index('--scenes') + 1]) if '--scenes' in a else 12
    o = open(a[0], encoding='utf-8').read()
    n, sites = flag_dedupe.rewrite(o)
    if open(a[1], encoding='utf-8').read() != n:
        sys.exit('new.ll is not flag_dedupe.rewrite(orig.ll)')
    print('lint:', lint(o, n, len(sites)))
    ok, msg, cov = verify(o, n, npx, nsc)
    print('BIT-EXACT' if ok else 'MISMATCH', msg)
    for x in site_coverage(sites, cov):
        print('  %-5s hit %6d  miss %6d' % x)
