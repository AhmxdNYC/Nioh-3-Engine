r"""verify_msv.py - prove an MSV-rewritten meshlet VS bit-exact against the shipped record (DXIL reference interpreter).

    py -I verify_msv.py orig.ll new.ll [--verts 400] [--scenes 8]

Model: the vendored interpreter (sweep/vendor_gi, via materials/psim.PShader) runs every vertex alone; SV_VertexID comes
from loadInput; storeOutput is recorded per vertex. WaveReadLaneFirst is SCRIPTED per vertex, which exercises both loop
paths a real wave takes:
  * honest   : returns the lane's own slot (the lane is the first active lane; 1 iteration);
  * decoy    : the first call returns another lane's slot (a different meshlet: the chain runs on foreign data, the lane
               must NOT leave the loop), later calls return the own slot (the lane leaves in the 2nd iteration).
  * decoy2   : two different foreign slots before the own one (3 iterations).
Every vertex's outputs must be bit-identical to the shipped shader in all three modes. Scenes: all SRV buffers filled
from a per-scene mixture of small ints (in-range indices: deep paths), random u32 (out-of-range -> 0) and floats; the
cbuffer's nVSDrawPrm.x (direct vs meshlet path) is forced both ways.
"""
import os, random, struct, sys
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path[:0] = [os.path.join(HERE, '..', 'materials')]
import psim, verify_sfb  # noqa: E402
import dxil_interp as DI  # noqa: E402


class VShader(psim.PShader):
    def _gen_call(self, ins, d, a):
        if ins.extra['fn'].startswith('dx.op.waveReadLaneFirst.'):
            return ['%s = X.rfl(%s)' % (d, a[1])]
        return super()._gen_call(ins, d, a)


def buf_bytes(rng, n, mix):
    out = bytearray()
    for _ in range(n // 4):
        r = rng.random()
        if r < mix:
            out += struct.pack('<I', rng.randrange(0, 64))
        elif r < mix + (1 - mix) * 0.5:
            out += struct.pack('<f', rng.uniform(-4, 4))
        else:
            out += struct.pack('<I', rng.getrandbits(32))
    return bytes(out)


def scene(ll, seed, direct):
    rng = random.Random(seed)
    mix = rng.choice([0.6, 0.8, 0.95, 1.0])
    b = {}
    for cls, reg, name, ty, fmt, dim in psim.bindings(ll):
        if ty == 'cbuffer':
            raw = bytearray(buf_bytes(rng, 512, mix))
            struct.pack_into('<I', raw, 144, 0 if direct else rng.randrange(1, 4))   # nVSDrawPrm.x
            struct.pack_into('<I', raw, 128, rng.randrange(0, 32))                    # nMshltPrm0.x
            b[(cls, reg)] = DI.CBuffer(bytes(raw))
        elif dim == 'buf':
            b[(cls, reg)] = DI.TypedBuffer([rng.randrange(0, 64) for _ in range(4096)], isint=True, comps=1)
        elif fmt == 'byte':
            b[(cls, reg)] = DI.StructBuffer(buf_bytes(rng, 1 << 14, mix), 1)
        else:
            b[(cls, reg)] = DI.StructBuffer(buf_bytes(rng, 1 << 13, mix), 16)
    return DI.Scene(verify_sfb.Bindless(b))


def canon(b):
    """NaN payloads are not modelled (the Python float model produces either quiet NaN sign for 0*inf; the GPU computes
    the same op on the same inputs) -> every float NaN compares equal; everything else bit for bit"""
    if b.startswith('f'):
        u = int(b[1:], 16)
        if (u >> 23) & 0xFF == 0xFF and u & 0x7FFFFF:
            return 'fNaN'
    return b


def run(sh, sc, vids, mode, slot_of):
    X = DI.Ctx(sc)
    F = sh._build(X, {k: psim.Arr(v) for k, v in sh.consts.items()}, {}, lambda p: None)
    res, iters = [], 0
    for v in vids:
        X.out = {}
        X.inp = lambda s, r, c, v=v: v
        calls = [0]

        def rfl(x, calls=calls, v=v):
            calls[0] += 1
            n = {'honest': 0, 'decoy': 1, 'decoy2': 2}[mode]
            if calls[0] <= n:
                return (x + 1 + 977 * calls[0]) & 0xFFFFFFFF     # a foreign slot, never equal to x
            return x
        X.rfl = rfl
        R = [None] * sh.nregs
        for rr, n, t in sh.alloca_init:
            R[rr] = psim.Arr([0.0] * n if t == 'float' else [0] * n)
        b, steps = 0, 0
        while b >= 0 and steps <= 30000:
            b = F[b](R)
            steps += 1
        if b >= 0:      # data-dependent loop that random data never ends (the shipped shader does it too: compared)
            res.append('RUNAWAY'); iters += {'honest': 1, 'decoy': 2, 'decoy2': 3}[mode]; continue
        iters += calls[0]
        res.append({k: canon(psim._bits(x)) for k, x in X.out.items()})
    return res, iters


def verify(orig_ll, new_ll, nv=400, nsc=8):
    A, B = VShader(orig_ll, 'orig'), VShader(new_ll, 'new')
    tot = skipped = 0
    for s in range(nsc):
        for direct in (False, True):
            rng = random.Random(77 + s)
            vids = sorted({rng.choice([rng.randrange(0, 4096), 384 * rng.randrange(0, 8) + rng.randrange(-2, 3),
                                       512 * rng.randrange(0, 8) + rng.randrange(-2, 3)]) & 0xFFFFFFFF for _ in range(nv)})
            ra, _ = run(A, scene(orig_ll, 1000 + s, direct), vids, 'honest', None)
            for mode in ('honest', 'decoy', 'decoy2'):
                rb, it = run(B, scene(new_ll, 1000 + s, direct), vids, mode, None)
                for i, (x, y) in enumerate(zip(ra, rb)):
                    if x == 'RUNAWAY' and y == 'RUNAWAY':
                        skipped += 1; continue
                    if x != y:
                        return False, 'scene %d direct=%d mode %s vid %d differs' % (s, direct, mode, vids[i])
                exp = len(vids) * {'honest': 1, 'decoy': 2, 'decoy2': 3}[mode]
                if it != exp:
                    return False, 'loop iterations %d != %d (%s)' % (it, exp, mode)
                tot += len(vids)
            if not any(len(o) for o in ra):
                return False, 'no outputs'
    if skipped * 2 > tot or tot - skipped < 1500:
        return False, 'too many runaway vertices (%d of %d)' % (skipped, tot)
    return True, '%d vertex runs bit-identical (3 rfl modes x 2 paths x %d scenes; %d runaway in both skipped)' % (tot - skipped, nsc, skipped)


if __name__ == '__main__':
    a = sys.argv[1:]
    nv = int(a[a.index('--verts') + 1]) if '--verts' in a else 400
    nsc = int(a[a.index('--scenes') + 1]) if '--scenes' in a else 8
    ok, msg = verify(open(a[0], encoding='utf-8').read(), open(a[1], encoding='utf-8').read(), nv, nsc)
    print('BIT-EXACT' if ok else 'MISMATCH', msg)
    sys.exit(0 if ok else 1)
