r"""psim.py - pixel-shader extension of the vendored DXIL interpreter (gpu-shaders/sweep/vendor_gi, unmodified), for proving
that a rewritten material pixel shader computes exactly what the shipped one computes.

Model (sound for rewrites that leave every texture-sample / derivative operand unchanged):
  * every pixel runs alone; loadInput reads the pixel's random attributes, storeOutput / discard are recorded;
  * buffer loads, cbuffers and typed-buffer contents are REAL (the SFB MinMip clamp is computed from them);
  * sample / sampleBias / sampleLevel / sampleGrad / sampleCmp / gather / textureLoad and ddx/ddy are an ORACLE: a
    deterministic hash of (opcode, resource, sampler, every operand's exact bits). Two programs agree on an oracle
    result iff they pass bit-identical operands, so equal outputs on all pixels prove the rewrite changed no sampled
    coordinate, offset, LOD, bias or clamp - and, since quad derivatives are functions of the four lanes' operands, it
    also covers the implicit-derivative quad coupling;
  * UAV writes are recorded in order.
"""
import hashlib, os, re, struct, sys
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, '..', 'sweep', 'vendor_gi'))
import dxil_interp as DI  # noqa: E402
from dxil_interp import f2bits, M32  # noqa: E402

ORACLE_OPS = {60: 'sample', 61: 'sampleBias', 62: 'sampleLevel', 63: 'sampleGrad', 64: 'sampleCmp',
              65: 'sampleCmpLevelZero', 66: 'textureLoad', 73: 'gather', 74: 'gatherCmp',
              83: 'ddx_coarse', 84: 'ddy_coarse', 85: 'ddx_fine', 86: 'ddy_fine'}


def _bits(v):
    if isinstance(v, float):
        return 'f%08x' % f2bits(v)
    if isinstance(v, bool):
        return 'b%d' % v
    if isinstance(v, int):
        return 'i%08x' % (v & M32)
    return 'r' + getattr(v, 'name', type(v).__name__)


def oracle(op, *args):
    h = hashlib.blake2b(('%d|' % op + '|'.join(_bits(a) for a in args)).encode(), digest_size=16).digest()
    vals = struct.unpack('<4I', h)
    if op >= 83:
        return DI.rf((vals[0] / 2 ** 32 - 0.5) * 8.0)
    return tuple(DI.rf(v / 2 ** 32) for v in vals) + (0,)


class Res:
    def __init__(self, name):
        self.name = name

    def dims(self, mip):
        return (1024 >> min(int(mip) & 31, 10), 512 >> min(int(mip) & 31, 9), 1, 11)


class UAVLog:
    def __init__(self, name):
        self.name, self.writes = name, []

    def store(self, *a):
        self.writes.append(tuple(_bits(x) if not isinstance(x, tuple) else tuple(_bits(y) for y in x) for x in a[:-1]))

    def bstore(self, *a):
        self.store(*a, None)

    def load(self, *a):
        return (0, 0, 0, 0, 0)

    def dims(self, mip):
        return (1 << 20, 0, 0, 0)


class Arr(list):
    """local array (alloca): an out-of-range dynamic index (undefined in DXIL) reads 0 and drops the write - the same
    deterministic model for both programs, so it can only hide nothing the rewrite touches (it never touches allocas)"""

    def __getitem__(self, i):
        return list.__getitem__(self, i) if isinstance(i, int) and 0 <= i < len(self) else 0

    def __setitem__(self, i, v):
        if isinstance(i, int) and 0 <= i < len(self):
            list.__setitem__(self, i, v)


class PShader(DI.Shader):
    def _gen_call(self, ins, d, a):
        fn = ins.extra['fn']
        oc = ins.extra['opcode']
        if fn.startswith('dx.op.loadInput.'):
            return ['%s = X.inp(%s, %s, %s)' % (d, a[1], a[2], a[3])]
        if fn.startswith('dx.op.storeOutput.'):
            return ['X.out[(%s, %s, %s)] = %s' % (a[1], a[2], a[3], a[4])]
        if fn.startswith('dx.op.discard'):
            return ['X.discard |= bool(%s)' % a[1]]
        if oc in ORACLE_OPS:
            return ['%s = oracle(%d, %s)' % (d, oc, ', '.join(a[1:]))]
        if fn.startswith('dx.op.textureStore.') or fn.startswith('dx.op.bufferStore.'):
            return ['%s.store(%s)' % (a[1], ', '.join(a[2:]) + ', None')]
        if fn.startswith('dx.op.atomicBinOp.'):
            return ['%s = %s.store(%s)' % (d, a[1], ', '.join(a[2:]) + ', None') if d else '%s.store(%s)' % (a[1], ', '.join(a[2:]) + ', None'),
                    '%s = 0' % d if d else 'pass']
        if fn.startswith('dx.op.legacyF16ToF32'):
            return ['%s = h2f(%s)' % (d, a[1])]
        if fn.startswith('dx.op.isHelperLane'):
            return ['%s = 0' % d]
        if fn.startswith('dx.op.unary.') and fn.endswith('.i32'):
            if oc == 31:   # Bfrev
                return ['%s = int(bin(%s & M32)[2:].zfill(32)[::-1], 2)' % (d, a[1])]
            if oc == 32:   # Countbits
                return ['%s = bin(%s & M32).count("1")' % (d, a[1])]
            if oc == 33:   # FirstbitLo
                return ['%s = ((%s & -%s).bit_length() - 1) & M32' % (d, a[1], a[1])]
            if oc == 34:   # FirstbitHi (from MSB)
                return ['%s = (31 - (%s & M32).bit_length() + 1) & M32 if %s else M32' % (d, a[1], a[1])]
        return super()._gen_call(ins, d, a)

    def run_pixels(self, scene, pixels, coverage=None, max_steps=500_000):
        """pixels: list of dict (sig,row,col) -> value. Returns [(outputs, discard)] per pixel."""
        X = DI.Ctx(scene)
        cov = coverage if coverage is not None else [0] * len(self.m.blocks)
        F = self._build(X, {k: Arr(v) for k, v in self.consts.items()}, {}, lambda p: None)   # constant tables: same OOB model
        res = []
        for px in pixels:
            X.out, X.discard = {}, False
            X.inp = lambda s, r, c, px=px: px.get((s, r, c), 0.0)
            R = [None] * self.nregs
            for rr, n, t in self.alloca_init:
                R[rr] = Arr([0.0] * n if t == 'float' else [0] * n)
            b, steps = 0, 0
            while b >= 0:
                cov[b] += 1
                b = F[b](R)
                steps += 1
                if steps > max_steps:
                    raise RuntimeError('runaway pixel')
            res.append(({k: _bits(v) for k, v in X.out.items()}, X.discard))
        return res, cov


def h2f(u):
    """LegacyF16ToF32: low 16 bits as IEEE half -> float (exact)"""
    return struct.unpack('<e', struct.pack('<H', u & 0xFFFF))[0]


DI.oracle = oracle   # the generated code runs in dxil_interp's globals
DI.h2f = h2f


def bindings(ll):
    """[(cls, reg, name, type, fmt, dim)] from the dumpbin 'Resource Bindings' table"""
    out = []
    sec = ll.split('; Resource Bindings:', 1)
    if len(sec) < 2:
        return out
    for l in sec[1].splitlines()[3:]:
        if not l.startswith(';') or not l[1:].strip():
            break
        m = re.match(r'^;\s+(?:(\S+)\s+)?(cbuffer|texture|sampler|UAV)\s+(\S+)\s+(\S+)\s+\S+\s+([a-z]+)(\d+)(?:,space(\d+))?\s*(\d+|unbounded)', l)
        if m:
            cls = {'texture': 0, 'UAV': 1, 'cbuffer': 2, 'sampler': 3}[m.group(2)]
            out.append((cls, int(m.group(6)), m.group(1) or '', m.group(2), m.group(3), m.group(4)))
    return out


def inputs(ll):
    """[(sigId, row, ncols)] for loadInput: input signature element order = sigId"""
    sec = ll.split('; Input signature:', 1)[1].split('; Output signature:', 1)[0]
    out, fmt = [], {}
    for l in sec.splitlines():
        m = re.match(r'^; (\w+)\s+(\d+)\s+([xyzw ]+?)\s+(\d+)\s+(\w+)\s+(\w+)', l)
        if m:
            out.append((m.group(1), m.group(6))); fmt[(m.group(1), m.group(2))] = m.group(6)
    # sigIds follow the module's signature metadata (second 'Input signature' table: elements the PS really declares),
    # which can omit container elements the shader does not read
    parts = ll.split('; Input signature:')
    if len(parts) > 2:
        meta = []
        for l in parts[2].split('; Output signature:', 1)[0].splitlines():
            m = re.match(r'^; (\w+)\s+(\d+)\s+\w+\s*$', l)
            if m and (m.group(1), m.group(2)) in fmt:
                meta.append((m.group(1), fmt[(m.group(1), m.group(2))]))
        if meta:
            out = meta
    return out
