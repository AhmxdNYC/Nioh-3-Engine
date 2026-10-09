r"""verify_lod.py - prove the MshLod#2 fast path (lodfast.py) equivalent to the shipped kernel in the DXIL reference
interpreter (sweep/vendor_gi via materials/psim; the same interpreter as MSV / the culling proofs).

    py -I verify_lod.py orig.ll new.ll [--scenes 40] [--threads 64]
    py -I verify_lod.py --selftest orig.ll            (3 broken rewrites must be caught)

Model (per lane; sound for a rewrite that moves no side effect - lodfast.py asserts that structurally):
  * every thread runs alone, in thread order, on SHARED UAV state (the same order for both programs);
  * EVERY side effect and cross-lane op is an event in one ordered per-thread log, compared between the programs:
    UAV loads (address + value read), UAV stores, atomics (address, op, value; the atomic is applied to the buffer and
    returns the old value), and the shipped wave ops (WaveActiveSum / WavePrefixSum / WaveIsFirstLane /
    WaveReadLaneFirst) with their operands. A wave op's RESULT is an oracle: a deterministic function of (scene,
    thread, ordinal, operands), so both programs see the same cross-lane values iff they issue the same ops with the
    same operands - which is what makes the per-lane model cover any lane mix of a real wave;
  * the NEW guard ops (WaveActiveAllEqual(g) then WaveReadLaneFirst(g)) are SCRIPTED per run, covering every result a
    real wave can produce: fast (all equal, k = own g), slow (not all equal, k = a foreign g), slow-own (not all
    equal, k = own g, i.e. this lane is the first lane). They are not events (the shipped kernel has none);
  * SRV contents are random per scene but structured so the deep paths run (small indices, -1 sentinels, page flags,
    zero cache codes); block coverage of the shipped kernel is reported.
Final UAV contents must be identical too.
"""
import hashlib, os, random, struct, sys
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path[:0] = [os.path.join(HERE, '..', 'materials')]
import psim  # noqa: E402
import dxil_interp as DI  # noqa: E402
from dxil_interp import M32  # noqa: E402

MODES = ('fast', 'slow', 'slowown')


class CShader(DI.Shader):
    def _gen_call(self, ins, d, a):
        fn = ins.extra['fn']
        if fn.startswith('dx.op.waveActiveAllEqual.'):
            return ['%s = X.wae(%s)' % (d, a[1])]
        if fn.startswith('dx.op.waveReadLaneFirst.'):
            return ['%s = X.rfl(%s)' % (d, a[1])]
        if fn.startswith('dx.op.waveActiveOp.'):
            return ['%s = X.wop("sum", %s, %s, %s)' % (d, a[1], a[2], a[3])]
        if fn.startswith('dx.op.wavePrefixOp.'):
            return ['%s = X.wop("prefix", %s, %s, %s)' % (d, a[1], a[2], a[3])]
        if fn.startswith('dx.op.waveIsFirstLane'):
            return ['%s = X.wop("first")' % d]
        if fn.startswith('dx.op.atomicBinOp.'):
            return ['%s = X.atomic(%s, %s, %s, %s, %s, %s)' % (d, a[1], a[2], a[3], a[4], a[5], a[6])]
        return super()._gen_call(ins, d, a)


class LStruct(DI.StructBuffer):
    def load(self, idx, off, isint):
        v = super().load(idx, off, isint)
        self.ev.append(('ld', self.tag, idx, off, v[:4]))
        return v

    def store(self, idx, off, vals, mask, who, isint):
        self.ev.append(('st', self.tag, idx, off, tuple(vals), mask))
        super().store(idx, off, vals, mask, who, isint)


class LTyped(DI.TypedBuffer):
    def load(self, idx, wot):
        v = super().load(idx, wot)
        self.ev.append(('ld', self.tag, idx, v[:4]))
        return v

    def store(self, idx, off, vals, mask, who):
        self.ev.append(('st', self.tag, idx, tuple(vals), mask))
        super().store(idx, off, vals, mask, who)


class Ctx(DI.Ctx):
    def __init__(self, scene, ev, seed, mode):
        super().__init__(scene)
        self.ev, self.seed, self.mode = ev, seed, mode
        self.guard = None
        self.n = 0

    def _orc(self, *k, inc=True):
        h = hashlib.blake2b(repr((self.seed, self.tid, self.n) + k).encode(), digest_size=8).digest()
        self.n += inc
        return struct.unpack('<Q', h)[0]

    def wae(self, g):
        self.guard = (self.mode == 'fast', g)
        return 1 if self.mode == 'fast' else 0

    def rfl(self, x):
        if self.guard is not None:          # the new k = WaveReadLaneFirst(g) right after the guard
            eq, g = self.guard
            self.guard = None
            assert x == g
            return x if eq or self.mode == 'slowown' else (x + 1 + (self._orc('foreign', inc=False) % 5000)) & M32
        r = self._orc('rfl', x)
        self.ev.append(('rfl', x))
        return [x, 0, r & 0xFFFF, r & M32][r >> 62]

    def wop(self, kind, x=None, op=None, sop=None):
        r = self._orc(kind, x, op, sop)
        self.ev.append(('wave', kind, x, op, sop))
        if kind == 'first':
            return r & 1
        if kind == 'sum':
            return [x, x, (x + (r & 63)) & M32, 0 if x == 0 else (x + (r & 7)) & M32][r >> 62]
        return [0, r & 15, r & 255, (r & 31) * 2][r >> 62]

    def atomic(self, h, aop, o0, o1, o2, v):
        self.ev.append(('atomic', h.tag, aop, o0, o1, o2, v))
        assert aop == 0, 'only InterlockedAdd modelled'
        if isinstance(h, DI.TypedBuffer):
            old = h.data[o0][0] if 0 <= o0 < len(h.data) else 0
            if 0 <= o0 < len(h.data):
                h.data[o0][0] = (old + v) & M32
            return old
        a = o0 * h.stride + (o1 or 0)
        if 0 <= a and a + 4 <= len(h.b):
            old = struct.unpack_from('<I', h.b, a)[0]
            struct.pack_into('<I', h.b, a, (old + v) & M32)
            return old
        return 0


def _dw(rng, mix):
    r = rng.random()
    if r < mix[0]:
        return 0
    if r < mix[1]:
        return rng.randrange(1, 16)
    if r < mix[2]:
        return rng.randrange(16, 400)
    if r < mix[3]:
        return M32
    if r < mix[4]:
        return struct.unpack('<I', struct.pack('<f', rng.uniform(-8, 8)))[0]
    return rng.getrandbits(32)


def _bytes(rng, ndw, mix):
    return b''.join(struct.pack('<I', _dw(rng, mix)) for _ in range(ndw))


def scene(ll, seed, ev):
    rng = random.Random(seed)
    a = rng.uniform(0.1, 0.4); b = a + rng.uniform(0.2, 0.4); c = b + rng.uniform(0.0, 0.15)
    d = c + rng.uniform(0.02, 0.15); e = d + rng.uniform(0.0, 0.15)
    mix = (a, b, c, d, e)
    out = {}
    for cls, reg, name, ty, fmt, dim in psim.bindings(ll):
        if ty == 'cbuffer':
            f = lambda x: struct.pack('<f', x)          # noqa: E731
            i = lambda x: struct.pack('<I', x & M32)    # noqa: E731
            raw = (f(rng.uniform(-60, 60)) + f(rng.uniform(-60, 60)) + f(rng.uniform(-60, 60)) + f(1.0) +
                   f(rng.uniform(0.3, 4.0)) + i(rng.randrange(0, 6)) + i(rng.choice([-1, -1, rng.randrange(0, 8)])) +
                   i(rng.randrange(0, 4)) +
                   i(rng.randrange(0, 24)) + i(rng.randrange(0, 24)) + i(rng.randrange(0, 200)) +
                   i(rng.choice([rng.randrange(0, 70), 64, 1000])) +
                   i(rng.choice([-1, rng.randrange(0, 8)])) + i(rng.randrange(0, 8)) + bytes(8) +
                   i(rng.choice([4, 4, rng.randrange(0, 6)])) + i(rng.randrange(1, 9)) + bytes(8))
            for _ in range(8):
                raw += f(rng.choice([rng.uniform(0, 3), rng.uniform(0, 0.2), 1e9])) + bytes(12)
            out[(cls, reg)] = DI.CBuffer(raw)
            continue
        if cls == 0 and reg == 16:          # tG1mOffsets: 48 B rows; rootBufferIdx in the bindless range
            rows = []
            for _ in range(64):
                # byte offsets into the bindless G1M buffers: record aligned (56 B) like the real data; some out of range
                r = [rng.choice([0, 1, 2, 3, rng.randrange(0, 16)])] + [rng.choice([0, 56 * rng.randrange(0, 8), 56 * rng.randrange(0, 64),
                                                                           0x7FFFFFC0 if rng.random() < 0.3 else 0x40000]) for _ in range(11)]
                rows.append(b''.join(struct.pack('<I', x & M32) for x in r))
            res = DI.StructBuffer(b''.join(rows), 48)
        elif cls == 0 and reg == 0:         # tMeshletG1mBuffer[16] (bindless byte buffers): Bindless.get makes them
            continue
        elif cls == 0 and reg == 17:        # tMeshletMatrixPalette float4
            res = DI.StructBuffer(b''.join(struct.pack('<f', rng.uniform(-4, 4)) for _ in range(1024)), 16)
        elif cls == 0 and reg == 18:        # tMeshletInstanceOffsets
            res = DI.StructBuffer(_bytes(rng, 256, mix), 16)
        elif cls == 0 and reg == 19:        # tMeshletResourceOffsets
            res = DI.StructBuffer(b''.join(struct.pack('<I', rng.choice([0, rng.randrange(0, 64), _dw(rng, mix)]) & M32) for _ in range(128)), 8)
        elif cls == 0 and reg == 21:        # tPageFlags
            dens = rng.choice([0.3, 0.7, 0.95, 1.0])
            res = DI.StructBuffer(bytes(sum((1 << k) for k in range(8) if rng.random() < dens) for _ in range(4096)), 1)
        elif cls == 1 and dim == 'buf':     # uArguments / uArgumentForNextLv RWBuffer<uint>
            res = LTyped([rng.randrange(0, 40) for _ in range(64)], isint=True, comps=1)
        elif cls == 1 and reg == 2:         # uPayloads: (g1m << 6 | inst_hi, inst_lo << 18 | cluster)
            pl = []
            g = rng.randrange(0, 8)
            for _ in range(512):
                if rng.random() < 0.3:
                    g = rng.randrange(0, 8)
                p0 = (g << 6) | rng.choice([0, 0, rng.randrange(0, 64)])
                p1 = M32 if rng.random() < 0.1 else ((rng.randrange(0, 16) << 18) | rng.randrange(0, 24))
                pl.append(struct.pack('<II', p0 & M32, p1))
            res = LStruct(b''.join(pl), 8)
        elif cls == 1 and reg in (0, 5):    # uCullCodes / uCacheCodes
            z = rng.choice([0.5, 0.8, 0.95])
            res = LStruct(b''.join(struct.pack('<I', 0 if rng.random() < z else rng.choice([1, 2, 0x70, rng.getrandbits(32)])) for _ in range(1024)), 4)
        elif cls == 1:                      # uPayloadForMRLOD
            res = LStruct(bytes(4096), 8)
        else:
            raise ValueError('unexpected binding %r' % ((cls, reg, name, ty, fmt, dim),))
        if cls == 1:
            res.ev, res.tag = ev, 'u%d' % reg
        out[(cls, reg)] = res
    return DI.Scene(Bindless(out, rng.getrandbits(32)))


class Bindless(dict):
    """t0..t15 (tMeshletG1mBuffer[16]) and beyond: each register its own random byte buffer (deterministic per seed)"""

    def __init__(self, d, seed):
        super().__init__(d)
        self.seed = seed

    def get(self, k, dflt=None):
        if k not in self and k[0] == 0:
            rng = random.Random(self.seed * 131 + k[1])
            # dword data laid out like the G1M cluster-LOD records the kernel indexes (56 B = 14 dwords; the t16 offset
            # fields are multiples of 56): -1 sentinels only where the kernel tests for them (dwords 2, 4-7, 13), the
            # 16-bit meshlet count in dword 1 stays small (it is a loop trip count: cache-code fill / child payloads)
            def dw(i):
                p = i % 14
                if p in (2, 4, 5, 6, 7, 13) and rng.random() < 0.25:
                    return M32
                if p == 1:
                    return (rng.randrange(0, 16) << 16) | rng.randrange(0, 24)
                if p in (8, 9, 10, 12) and rng.random() < 0.8:      # bounding-sphere centre / LOD error floats
                    return struct.unpack('<I', struct.pack('<f', rng.uniform(0, 200) if p == 12 else rng.uniform(-4, 4)))[0]
                return rng.choice([0, 0, rng.randrange(1, 16), rng.randrange(0, 64), (rng.randrange(0, 256) << 16) | rng.randrange(0, 24),
                                   struct.unpack('<I', struct.pack('<f', rng.uniform(-4, 4)))[0], rng.getrandbits(32) & 0xFFFF00FF])
            self[k] = DI.StructBuffer(b''.join(struct.pack('<I', dw(i)) for i in range(2048)), 1)
        return dict.get(self, k, dflt)


def run(sh, ll, seed, mode, nthreads, cov=None):
    ev = []
    sc = scene(ll, seed, ev)
    X = Ctx(sc, ev, seed, mode)
    F = sh._build(X, {k: psim.Arr(v) for k, v in sh.consts.items()}, {}, lambda p: None)
    per = []
    nt = sh.numthreads
    for t in range(nthreads):
        X.tid = (t, 0, 0); X.gid = (t // nt[0], 0, 0); X.gtid = (t % nt[0], 0, 0); X.flat = t % nt[0]; X.who = X.tid
        X.n = 0; X.guard = None
        R = [None] * sh.nregs
        for rr, n, ty in sh.alloca_init:
            R[rr] = psim.Arr([0.0] * n if ty == 'float' else [0] * n)
        s0 = len(ev)
        b, steps = 0, 0
        while b >= 0:
            if cov is not None:
                cov[b] += 1
            b = F[b](R)
            steps += 1
            if steps > 200000:
                raise RuntimeError('runaway thread %d' % t)
        per.append(ev[s0:])
    return per, DI.uav_state(sc)


_W = {}


def _winit(orig_ll, new_ll, nthreads):
    _W.update(A=CShader(orig_ll, 'orig'), B=CShader(new_ll, 'new'), o=orig_ll, n=new_ll, nt=nthreads)


def _scene_job(s):
    A, B, nth = _W['A'], _W['B'], _W['nt']
    cov = [0] * len(A.m.blocks)
    ra, ua = run(A, _W['o'], 9000 + s, 'fast', nth, cov)
    for mode in MODES:
        rb, ub = run(B, _W['n'], 9000 + s, mode, nth)
        for t, (x, y) in enumerate(zip(ra, rb)):
            if x != y:
                k = next((i for i, (p, q) in enumerate(zip(x, y)) if p != q), min(len(x), len(y)))
                return s, 'scene %d mode %s thread %d: event %d differs: %r vs %r' % (
                    s, mode, t, k, x[k] if k < len(x) else None, y[k] if k < len(y) else None), cov, 0
        if ua != ub:
            return s, 'scene %d mode %s: final UAV contents differ' % (s, mode), cov, 0
    return s, None, cov, sum(map(len, ra))


def verify(orig_ll, new_ll, nsc=40, nthreads=64, quiet=False, jobs=None):
    from multiprocessing import Pool
    jobs = jobs or max(1, min(nsc, (os.cpu_count() or 2) - 2))
    nblk = len(CShader(orig_ll, 'orig').m.blocks)
    cov, nev, err = [0] * nblk, 0, None
    with Pool(jobs, _winit, (orig_ll, new_ll, nthreads)) as pool:
        for s, e, c, n in pool.imap_unordered(_scene_job, range(nsc)):
            cov = [a + b for a, b in zip(cov, c)]
            nev += n
            if e and err is None:
                err = e
    if err:
        return False, err, None
    hit = sum(1 for c in cov if c)
    return True, '%d scenes x %d threads x %d guard modes: event logs + UAV state identical (%d events/scene); shipped blocks covered %d/%d' % (
        nsc, nthreads, len(MODES), nev // nsc, hit, nblk), dict(cov=cov)


def selftest(orig_ll, nsc=100):
    import re
    sys.path.insert(0, HERE)
    import lodfast
    good, _ = lodfast.rewrite(orig_ll)
    ok, msg, _ = verify(orig_ll, good, nsc)
    print('good rewrite:', 'PASS' if ok else 'FAIL', msg)
    res = [ok]
    # 1. guard inverted: the fast copy runs for divergent waves (k = a foreign g)
    m = re.search(r'(%\d+) = call i1 @dx\.op\.waveActiveAllEqual', good)
    bad1 = re.sub(r'br i1 %s, label (%%\d+), label (%%\d+)' % re.escape(m.group(1)), lambda t: 'br i1 %s, label %s, label %s' % (m.group(1), t.group(2), t.group(1)), good, count=1)
    # 2. one fast-path row load at the wrong offset (t16 rootOffset 4 -> 8)
    kk = re.search(r'(%\d+) = call i32 @dx\.op\.waveReadLaneFirst\.i32\(i32 118', good).group(1)
    pat = 'i32 %s, i32 4)' % kk
    assert good.count(pat) == 2, good.count(pat)        # the fast copy's t16 / t19 row loads at offset 4
    bad2 = good.replace(pat, 'i32 %s, i32 8)' % kk, 1)
    # 3. one UAV store dropped in the fast copy (the last bufferStore of the module = in the clone)
    k = good.rindex('call void @dx.op.bufferStore.i32')
    e = good.index('\n', k)
    bad3 = good[:good.rindex('\n', 0, k) + 1] + good[e + 1:]
    for name, bad in (('guard inverted', bad1), ('wrong row offset', bad2), ('store dropped', bad3)):
        assert bad != good, name
        ok, msg, _ = verify(orig_ll, bad, nsc)
        print('mutant %-16s %s  (%s)' % (name, 'CAUGHT' if not ok else 'MISSED', msg))
        res.append(not ok)
    return all(res)


if __name__ == '__main__':
    a = sys.argv[1:]
    if a[0] == '--selftest':
        ok = selftest(open(a[1], encoding='utf-8').read())
        print('SELFTEST', 'PASS' if ok else 'FAIL')
        sys.exit(0 if ok else 1)
    nsc = int(a[a.index('--scenes') + 1]) if '--scenes' in a else 40
    nth = int(a[a.index('--threads') + 1]) if '--threads' in a else 64
    ok, msg, _ = verify(open(a[0], encoding='utf-8').read(), open(a[1], encoding='utf-8').read(), nsc, nth)
    print('EQUIVALENT' if ok else 'MISMATCH', msg)
    sys.exit(0 if ok else 1)
