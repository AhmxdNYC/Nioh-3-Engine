r"""dxil_interp.py - a reference interpreter for compute-shader DXIL (the `dxc -dumpbin` text form).

Used to prove that a rewritten shader computes the same thing as the shipped one: both .ll files run on the same
synthetic inputs (cbuffer, textures, buffers, samplers) and every UAV write is compared.

Semantics (identical for both programs, which is what makes the comparison meaningful):
  * float ops are IEEE float32, correctly rounded, evaluated in IR order (fadd/fsub/fmul/fdiv/sqrt exact;
    FMad = round(round(a*b)+c); DotN = left-to-right fmul/fadd; Rsqrt = round(1/sqrt); Exp/Log = 2^x / log2 rounded).
    The `fast` flags are ignored, so any reassociation dxc did in either program shows up as a (tiny) difference.
  * i32 values are kept unsigned in [0, 2^32); fptosi/fptoui clamp like AMD v_cvt_*_f32 (NaN -> 0).
  * typed/structured buffer and texture reads out of bounds return 0 (D3D12 robust access); samplers are
    deterministic bilinear / point filters with clamp or wrap addressing on an explicit mip (round(lod)).
  * threads of a group run one after another; LDS is shared per group and every slot is checked to be touched by
    one thread only (the shaders here use LDS as per-thread scratch; a cross-thread access is reported).

    sh = Shader(open("a.ll").read());  out = sh.dispatch(scene, groups=[(0,0,0),...])
"""
import array, math, struct
import numpy as np
import os as _os, sys as _sys
_sys.path.insert(0, _os.path.dirname(_os.path.abspath(__file__)))
import llir

M32 = 0xFFFFFFFF
_A = array.array("f", [0.0])


def rf(x):
    _A[0] = x
    return _A[0]


def s32(x):
    return x - 0x100000000 if x & 0x80000000 else x


def f2bits(x):
    return struct.unpack("<I", struct.pack("<f", x))[0]


def bits2f(x):
    return struct.unpack("<f", struct.pack("<I", x & M32))[0]


def fdiv(a, b):
    try:
        return rf(a / b)
    except ZeroDivisionError:
        if a != a or a == 0:
            return math.nan
        return math.copysign(math.inf, a) * math.copysign(1.0, b)
    except OverflowError:
        return math.copysign(math.inf, a) * math.copysign(1.0, b)


def frem(a, b):
    try:
        return rf(math.fmod(a, b))
    except (ValueError, ZeroDivisionError):
        return math.nan


def f2i(x):
    if x != x:
        return 0
    if x >= 2147483647.0:
        return 0x7FFFFFFF
    if x <= -2147483648.0:
        return 0x80000000
    return int(x) & M32


def f2u(x):
    if x != x or x <= 0:
        return 0
    if x >= 4294967295.0:
        return M32
    return int(x)


def ashr(a, b):
    return (s32(a) >> (b & 31)) & M32


def udiv(a, b):
    return M32 if b == 0 else a // b


def urem(a, b):
    return M32 if b == 0 else a % b


def sdiv(a, b):
    a, b = s32(a), s32(b)
    if b == 0:
        return M32
    q = abs(a) // abs(b)
    return (q if (a < 0) == (b < 0) else -q) & M32


def srem(a, b):
    a, b = s32(a), s32(b)
    if b == 0:
        return M32
    r = abs(a) % abs(b)
    return (r if a >= 0 else -r) & M32


def fmax(a, b):
    if a != a:
        return b
    if b != b:
        return a
    return a if a >= b else b


def fmin(a, b):
    if a != a:
        return b
    if b != b:
        return a
    return a if a <= b else b


def sat(x):
    if x != x:
        return 0.0
    return 0.0 if x < 0 else (1.0 if x > 1 else x)


def fsqrt(x):
    if x != x or x < 0:
        return math.nan
    return rf(math.sqrt(x)) if x != math.inf else math.inf


def frsqrt(x):
    if x != x or x < 0:
        return math.nan
    if x == 0:
        return math.inf
    if x == math.inf:
        return 0.0
    return rf(1.0 / math.sqrt(x))


def fexp2(x):
    if x != x:
        return x
    try:
        return rf(2.0 ** x)
    except OverflowError:
        return math.inf


def flog2(x):
    if x != x or x < 0:
        return math.nan
    if x == 0:
        return -math.inf
    if x == math.inf:
        return math.inf
    return rf(math.log2(x))


def ffloor(x):
    return x if (x != x or x in (math.inf, -math.inf)) else float(math.floor(x))


def fceil(x):
    return x if (x != x or x in (math.inf, -math.inf)) else float(math.ceil(x))


def ftrunc(x):
    return x if (x != x or x in (math.inf, -math.inf)) else float(math.trunc(x))


def frne(x):
    return x if (x != x or x in (math.inf, -math.inf)) else float(round(x))


def ffrc(x):
    if x != x or x in (math.inf, -math.inf):
        return math.nan
    return rf(x - math.floor(x))


def ftrig(f, x):
    try:
        return rf(f(x))
    except (ValueError, OverflowError):
        return math.nan


UNARY = {6: "abs({0})", 7: "sat({0})", 12: "ftrig(math.cos,{0})", 13: "ftrig(math.sin,{0})",
         14: "ftrig(math.tan,{0})", 15: "ftrig(math.acos,{0})", 16: "ftrig(math.asin,{0})", 17: "ftrig(math.atan,{0})",
         21: "fexp2({0})", 22: "ffrc({0})", 23: "flog2({0})", 24: "fsqrt({0})", 25: "frsqrt({0})", 26: "frne({0})",
         27: "ffloor({0})", 28: "fceil({0})", 29: "ftrunc({0})", 8: "({0}!={0})", 9: "({0} in (math.inf,-math.inf))",
         10: "({0}=={0} and {0} not in (math.inf,-math.inf))"}
BINARY = {35: "fmax({0},{1})", 36: "fmin({0},{1})", 37: "(({0}) if s32({0})>=s32({1}) else ({1}))",
          38: "(({0}) if s32({0})<=s32({1}) else ({1}))", 39: "max({0},{1})", 40: "min({0},{1})"}


def _mk_f64():
    def fdiv(a, b):
        try:
            return a / b
        except ZeroDivisionError:
            if a != a or a == 0:
                return math.nan
            return math.copysign(math.inf, a) * math.copysign(1.0, b)

    def fsqrt(x):
        return math.nan if (x != x or x < 0) else (math.sqrt(x) if x != math.inf else math.inf)

    def frsqrt(x):
        if x != x or x < 0:
            return math.nan
        if x == 0:
            return math.inf
        return 0.0 if x == math.inf else 1.0 / math.sqrt(x)

    def fexp2(x):
        if x != x:
            return x
        try:
            return 2.0 ** x
        except OverflowError:
            return math.inf

    def flog2(x):
        if x != x or x < 0:
            return math.nan
        if x == 0:
            return -math.inf
        return math.inf if x == math.inf else math.log2(x)

    def ffrc(x):
        if x != x or x in (math.inf, -math.inf):
            return math.nan
        return x - math.floor(x)

    def ftrig(f, x):
        try:
            return f(x)
        except (ValueError, OverflowError):
            return math.nan
    return dict(fdiv=fdiv, fsqrt=fsqrt, frsqrt=frsqrt, fexp2=fexp2, flog2=flog2, ffrc=ffrc, ftrig=ftrig)


F64_HELPERS = _mk_f64()


class LDSViolation(Exception):
    pass


# ----------------------------------------------------------------------------------------------- resources
class Sampler:
    def __init__(self, filt="linear", addr="clamp", cmp=None):
        self.filt, self.addr, self.cmp = filt, addr, cmp   # cmp: 'less','lesseq','greater','greatereq'


class Texture:
    """mips: list of np arrays (layers, H, W, C) float32 / int32 / uint32. dim: '2d', '2darray', '3d'."""

    def __init__(self, mips, dim="2d"):
        self.mips, self.dim = [np.ascontiguousarray(m) for m in mips], dim
        self.isint = self.mips[0].dtype.kind in "iu"
        self.mipl = [m.tolist() for m in self.mips]

    def dims(self, mip):
        if mip >= len(self.mips):
            return (0, 0, 0, len(self.mips))
        L, H, W, _ = self.mips[mip].shape
        if self.dim == "2d":
            return (W, H, 0, len(self.mips))
        return (W, H, L, len(self.mips))

    def texel(self, mip, x, y, z):
        m = self.mips[mip]
        L, H, W, C = m.shape
        if 0 <= x < W and 0 <= y < H and 0 <= z < L:
            v = self.mipl[mip][z][y][x]
            return v + [0] * (4 - C) if C < 4 else v
        return [0, 0, 0, 0] if self.isint else [0.0, 0.0, 0.0, 0.0]

    def load(self, mip, x, y, z, ox, oy, oz):
        if mip >= len(self.mips):
            v = [0, 0, 0, 0] if self.isint else [0.0] * 4
        else:
            v = self.texel(mip, s32(x) + ox, s32(y) + oy, (s32(z) + oz) if self.dim != "2d" else 0)
        if self.isint:
            v = [int(t) & M32 for t in v]
        return (v[0], v[1], v[2], v[3], 0)

    def _wrap(self, i, n, addr):
        if addr == "wrap":
            return i % n
        return min(max(i, 0), n - 1)

    def sample(self, s, u, v, w, lod, ox=0, oy=0, cmp=None):
        mip = 0 if lod != lod else min(max(int(round(lod)), 0), len(self.mips) - 1)
        m = self.mipl[mip]
        L, H, W, C = self.mips[mip].shape
        if self.dim == "2darray":
            z = min(max(int(round(w)), 0), L - 1)
            zs = [(z, 1.0)]
        elif self.dim == "3d":
            if s.filt == "point":
                zs = [(self._wrap(int(math.floor(w * L)), L, s.addr), 1.0)]
            else:
                pz = w * L - 0.5
                z0 = math.floor(pz)
                fz = round((pz - z0) * 256) / 256
                zs = [(self._wrap(z0, L, s.addr), 1 - fz), (self._wrap(z0 + 1, L, s.addr), fz)]
        else:
            zs = [(0, 1.0)]
        if u != u or v != v:
            u = v = 0.0
        acc = [0.0] * 4
        for z, wz in zs:
            if s.filt == "point":
                x = self._wrap(int(math.floor(u * W)) + ox, W, s.addr)
                y = self._wrap(int(math.floor(v * H)) + oy, H, s.addr)
                t = m[z][y][x]
                if cmp is not None:
                    t = [self._cmp(s, cmp, t[0])]
                for c in range(min(C, 4) if cmp is None else 1):
                    acc[c] += wz * t[c]
            else:
                px, py = u * W - 0.5, v * H - 0.5
                x0, y0 = math.floor(px), math.floor(py)
                fx, fy = round((px - x0) * 256) / 256, round((py - y0) * 256) / 256
                x0 += ox; y0 += oy
                xs = (self._wrap(x0, W, s.addr), self._wrap(x0 + 1, W, s.addr))
                ys = (self._wrap(y0, H, s.addr), self._wrap(y0 + 1, H, s.addr))
                ws = ((1 - fx) * (1 - fy), fx * (1 - fy), (1 - fx) * fy, fx * fy)
                ts = (m[z][ys[0]][xs[0]], m[z][ys[0]][xs[1]], m[z][ys[1]][xs[0]], m[z][ys[1]][xs[1]])
                for t, wt in zip(ts, ws):
                    if cmp is not None:
                        acc[0] += wz * wt * self._cmp(s, cmp, t[0])
                    else:
                        for c in range(min(C, 4)):
                            acc[c] += wz * wt * t[c]
        if cmp is not None:
            return (rf(acc[0]), 0.0, 0.0, 0.0, 0)
        return (rf(acc[0]), rf(acc[1]), rf(acc[2]), rf(acc[3]), 0)

    @staticmethod
    def _cmp(s, ref, t):
        f = s.cmp or "lesseq"
        return 1.0 if {"less": ref < t, "lesseq": ref <= t, "greater": ref > t, "greatereq": ref >= t}[f] else 0.0


class TypedBuffer:
    """typed Buffer<T>/RWBuffer<T>: data list of tuples or scalars; isint for int formats."""

    def __init__(self, data, isint=False, comps=1):
        self.isint, self.comps = isint, comps
        self.data = [list(x) if isinstance(x, (tuple, list)) else [x] for x in data]
        self.writes = []

    def dims(self, mip):
        return (len(self.data), 0, 0, 0)

    def load(self, idx, wot):
        if 0 <= idx < len(self.data):
            v = self.data[idx] + [0] * (4 - len(self.data[idx]))
        else:
            v = [0, 0, 0, 0]
        if self.isint:
            v = [int(t) & M32 for t in v]
        else:
            v = [float(t) for t in v]
        return (v[0], v[1], v[2], v[3], 0)

    def store(self, idx, off, vals, mask, who):
        self.writes.append((who, idx, off, tuple(vals), mask))
        if 0 <= idx < len(self.data):
            for c in range(4):
                if mask & (1 << c) and c < len(self.data[idx]):
                    self.data[idx][c] = vals[c]


class StructBuffer:
    """(RW)StructuredBuffer / ByteAddressBuffer: raw bytes; elements read as 4 dwords at idx*stride+off."""

    def __init__(self, nbytes_or_bytes, stride):
        self.b = bytearray(nbytes_or_bytes) if isinstance(nbytes_or_bytes, int) else bytearray(nbytes_or_bytes)
        self.stride = stride
        self.writes = []

    def dims(self, mip):
        return (len(self.b) // max(self.stride, 1), self.stride, 0, 0)

    def load(self, idx, off, isint):
        a = idx * self.stride + (off or 0)
        out = []
        for c in range(4):
            p = a + 4 * c
            if 0 <= p and p + 4 <= len(self.b):
                u = struct.unpack_from("<I", self.b, p)[0]
            else:
                u = 0
            out.append(u if isint else bits2f(u))
        return (out[0], out[1], out[2], out[3], 0)

    def store(self, idx, off, vals, mask, who, isint):
        self.writes.append((who, idx, off, tuple(vals), mask))
        a = idx * self.stride + (off or 0)
        for c in range(4):
            if mask & (1 << c):
                p = a + 4 * c
                if 0 <= p and p + 4 <= len(self.b):
                    struct.pack_into("<I", self.b, p, (vals[c] & M32) if isint else f2bits(vals[c]))


class RWTexture:
    def __init__(self, W, H, L=1, C=4, dim="2darray"):
        self.W, self.H, self.L, self.C, self.dim = W, H, L, C, dim
        self.data = {}
        self.writes = []

    def dims(self, mip):
        return (self.W, self.H, self.L if self.dim != "2d" else 0, 1)

    def store(self, x, y, z, vals, mask, who):
        self.writes.append((who, (x, y, z), tuple(vals), mask))
        k = (x, y, z)
        cur = list(self.data.get(k, (0.0, 0.0, 0.0, 0.0)))
        for c in range(4):
            if mask & (1 << c):
                cur[c] = vals[c]
        self.data[k] = tuple(cur)

    def load(self, mip, x, y, z, ox, oy, oz):
        v = self.data.get((x, y, z), (0.0, 0.0, 0.0, 0.0))
        return (v[0], v[1], v[2], v[3], 0)


class CBuffer:
    def __init__(self, raw: bytes):
        raw = bytes(raw) + bytes((-len(raw)) % 16)
        n = len(raw) // 16
        self.f = [tuple(struct.unpack_from("<4f", raw, 16 * i)) for i in range(n)]
        self.i = [tuple(struct.unpack_from("<4I", raw, 16 * i)) for i in range(n)]

    def get(self, reg, isint):
        t = self.i if isint else self.f
        if reg < len(t):
            return t[reg]
        return (0, 0, 0, 0) if isint else (0.0, 0.0, 0.0, 0.0)


class Scene:
    """bindings: dict (cls, reg) -> resource; cls 0 SRV, 1 UAV, 2 CBV, 3 Sampler."""

    def __init__(self, bindings):
        self.b = bindings


# ----------------------------------------------------------------------------------------------- compiler
class Shader:
    def __init__(self, text, name="shader", f64=False, timing=None):
        """timing=(mem_latency, alu_latency): also compute, per thread, the dataflow critical path where a memory
        read costs mem_latency, any other op alu_latency, and nothing may start before the branch that guards its
        block has resolved (control dependence: what a GPU compiler cannot hoist). (1, 0) = dependent round trips."""
        self.f64 = f64
        self.timing = timing
        self.m = llir.parse(text)
        self.name = name
        self._parse_header(text)
        self._compile()

    def _parse_header(self, text):
        import re
        self.numthreads = None
        md = {}
        for l in text.splitlines():
            mm = re.match(r"^!(\d+) = (.*)$", l)
            if mm:
                md[mm.group(1)] = mm.group(2)
        ep = re.search(r"!dx\.entryPoints = !\{!(\d+)\}", text)
        if ep:
            props = md[ep.group(1)].rstrip("}").split(", ")[-1].lstrip("!")
            tm = re.search(r"i32 4, !(\d+)", md.get(props, ""))
            if tm:
                self.numthreads = tuple(int(x) for x in re.findall(r"i32 (\d+)", md[tm.group(1)]))

    def _compile(self):
        m = self.m
        regs = {}

        def R(name):
            if name not in regs:
                regs[name] = len(regs)
            return regs[name]

        for b in m.blocks:
            for ins in b.instrs:
                if ins.dest:
                    R(ins.dest)
        bidx = {b.name: i for i, b in enumerate(m.blocks)}
        self.bidx = bidx
        consts = {g.name: list(g.init) for g in m.globals.values() if g.init is not None}
        self.lds_globals = {g.name: (g.count, g.elem_ty) for g in m.globals.values() if g.space == 3}

        def val(op):
            if op.kind == "reg":
                return "R[%d]" % regs[op.v]
            if op.kind == "int":
                if op.ty == "i1":
                    return str(op.v & 1)
                return str(op.v & M32)
            if op.kind == "float":
                return repr(rf(op.v))
            if op.kind == "undef":
                return "0.0" if op.ty in ("float", "half") else "0"
            if op.kind == "glob":
                g = m.globals[op.v]
                return ("LDS[%r]" % op.v) if g.space == 3 else ("CONST[%r]" % op.v)
            if op.kind == "cexpr":
                g, idx = op.v
                base = ("LDS[%r]" % g) if m.globals[g].space == 3 else ("CONST[%r]" % g)
                return "(%s, %d, %r)" % (base, idx[-1], g)
            raise ValueError(op)

        src = ["def build(X, CONST, LDS, ldscheck):"]
        allocas = [ins for b in m.blocks for ins in b.instrs if ins.op == "alloca"]
        for bi, b in enumerate(m.blocks):
            src.append("  def bb%d(R%s):" % (bi, ", T, S, M" if self.timing else ""))
            body = []
            for ins in b.instrs:
                if ins.op == "phi":
                    continue
                if self.timing and ins.op in ("br", "switch", "ret"):
                    body.extend(self._tgen(ins, regs))
                body.extend(self._gen(ins, val, regs, bidx, b))
                if self.timing and ins.op not in ("br", "switch", "ret"):
                    body.extend(self._tgen(ins, regs))
            src.extend("    " + l for l in body)
        src.append("  return [%s]" % ", ".join("bb%d" % i for i in range(len(m.blocks))))
        self.nregs = len(regs)
        self.regs = regs
        self.alloca_init = [(regs[a.dest], a.extra["count"], a.ty) for a in allocas]
        code = "\n".join(src)
        ns = dict(globals())
        if self.f64:
            # float64 evaluation: no per-op rounding. Two programs that are the same function up to fast-math
            # reassociation then agree to ~1e-12 relative, while a real semantic difference stays O(1).
            code = code.replace("rf(", "(")
            ns.update(F64_HELPERS)
        self.src = code
        exec(compile(code, "<%s>" % self.name, "exec"), ns)
        self._build = ns["build"]
        self.consts = consts

    def _tgen(self, ins, regs):
        LM, LA = self.timing
        rops = ["T[%d]" % regs[o.v] for o in ins.args if o.kind == "reg"]
        mx = "max(S[0], %s)" % ", ".join(rops) if rops else "S[0]"
        op = ins.op
        if op in ("br", "switch"):
            return ["S[0] = %s" % mx] if rops else []
        if op == "ret":
            return []
        if op in ("extractvalue", "alloca", "getelementptr", "bitcast") or (op == "call" and (
                ins.extra["fn"].startswith("dx.op.createHandle") or ins.extra["fn"].startswith("dx.op.cbufferLoadLegacy")
                or ins.extra["fn"].startswith("dx.op.threadId") or ins.extra["fn"].startswith("dx.op.flattenedThreadId")
                or ins.extra["fn"].startswith("dx.op.groupId") or ins.extra["fn"].startswith("dx.op.threadIdInGroup")
                or ins.extra["fn"].startswith("dx.op.getDimensions"))):
            return ["T[%d] = %s" % (regs[ins.dest], mx)] if ins.dest else []
        if op == "load":
            return ["T[%d] = max(%s, M.get((id(_p[0]), _p[1]), 0)) + %r" % (regs[ins.dest], mx, LA), "S[1] += 1"]
        if op == "store":
            return ["M[(id(_p[0]), _p[1])] = %s" % mx, "S[1] += 1"]
        if op == "call":
            fn = ins.extra["fn"]
            if any(k in fn for k in ("Store", "store")):
                return ["S[2] = max(S[2], %s)" % mx, "S[1] += 1"]
            if any(k in fn for k in ("bufferLoad", "textureLoad", "sample", "BufferLoad")):
                return ["T[%d] = %s + %r" % (regs[ins.dest], mx, LM), "S[1] += 1", "S[3] += 1"]
        if ins.dest:
            return ["T[%d] = %s + %r" % (regs[ins.dest], mx, LA), "S[1] += 1"]
        return []

    def _phis_for_edge(self, src_block, tgt, val, regs):
        tb = self.m.byname[tgt]
        phis = [i for i in tb.instrs if i.op == "phi"]
        if not phis:
            return []
        dsts, srcs = [], []
        tl = []
        for p in phis:
            k = p.extra["from"].index(src_block.name)
            dsts.append("R[%d]" % regs[p.dest]); srcs.append(val(p.args[k]))
            if self.timing:
                a = p.args[k]
                tl.append("T[%d]" % regs[a.v] if a.kind == "reg" else "S[0]")
        out = ["%s = %s" % (", ".join(dsts) + ("," if len(dsts) == 1 else ""), ", ".join(srcs) + ("," if len(srcs) == 1 else ""))]
        if self.timing:
            out.append("%s = %s" % (", ".join("T" + d[1:] for d in dsts) + ("," if len(dsts) == 1 else ""),
                                     ", ".join(tl) + ("," if len(tl) == 1 else "")))
        return out

    def _gen(self, ins, val, regs, bidx, blk):
        op = ins.op
        d = ("R[%d]" % regs[ins.dest]) if ins.dest else None
        a = [val(x) for x in ins.args]
        if op in ("fadd", "fsub", "fmul"):
            return ["%s = rf(%s %s %s)" % (d, a[0], {"fadd": "+", "fsub": "-", "fmul": "*"}[op], a[1])]
        if op == "fdiv":
            return ["%s = fdiv(%s, %s)" % (d, a[0], a[1])]
        if op == "frem":
            return ["%s = frem(%s, %s)" % (d, a[0], a[1])]
        if op in ("add", "sub", "mul", "and", "or", "xor", "shl", "lshr", "ashr", "udiv", "sdiv", "urem", "srem"):
            ty = ins.ty
            if ty == "i1":
                e = {"and": "(%s & %s)", "or": "(%s | %s)", "xor": "(%s ^ %s)", "add": "((%s + %s) & 1)", "sub": "((%s - %s) & 1)"}[op] % (a[0], a[1])
                return ["%s = %s" % (d, e)]
            assert ty == "i32", ins
            e = {"add": "(%s + %s) & M32", "sub": "(%s - %s) & M32", "mul": "(%s * %s) & M32", "and": "%s & %s",
                 "or": "%s | %s", "xor": "%s ^ %s", "shl": "(%s << (%s & 31)) & M32", "lshr": "%s >> (%s & 31)",
                 "ashr": "ashr(%s, %s)", "udiv": "udiv(%s, %s)", "sdiv": "sdiv(%s, %s)", "urem": "urem(%s, %s)",
                 "srem": "srem(%s, %s)"}[op] % (a[0], a[1])
            return ["%s = %s" % (d, e)]
        if op == "icmp":
            p = ins.extra["pred"]
            x, y = a
            if p[0] == "s":
                x, y = "s32(%s)" % x, "s32(%s)" % y
            o = {"eq": "==", "ne": "!=", "gt": ">", "ge": ">=", "lt": "<", "le": "<="}[p if p in ("eq", "ne") else p[1:]]
            return ["%s = (%s %s %s)" % (d, x, o, y)]
        if op == "fcmp":
            p = ins.extra["pred"]
            x, y = a
            e = {"oeq": "({0} == {1})", "ogt": "({0} > {1})", "oge": "({0} >= {1})", "olt": "({0} < {1})",
                 "ole": "({0} <= {1})", "one": "({0} != {1} and {0} == {0} and {1} == {1})",
                 "ord": "({0} == {0} and {1} == {1})", "uno": "({0} != {0} or {1} != {1})",
                 "ueq": "(not ({0} != {1} and {0} == {0} and {1} == {1}))", "ugt": "(not ({0} <= {1}))",
                 "uge": "(not ({0} < {1}))", "ult": "(not ({0} >= {1}))", "ule": "(not ({0} > {1}))",
                 "une": "(not ({0} == {1}))", "true": "True", "false": "False"}[p].format(x, y)
            return ["%s = %s" % (d, e)]
        if op == "select":
            return ["%s = %s if %s else %s" % (d, a[1], a[0], a[2])]
        if op in ("sitofp", "uitofp", "fptosi", "fptoui", "bitcast", "zext", "sext", "trunc"):
            sty = ins.args[0].ty
            if op == "sitofp":
                return ["%s = rf(float(%s))" % (d, ("-int(%s)" % a[0]) if sty == "i1" else "s32(%s)" % a[0])]
            if op == "uitofp":
                return ["%s = rf(float(%s))" % (d, a[0])]
            if op == "fptosi":
                return ["%s = f2i(%s)" % (d, a[0])]
            if op == "fptoui":
                return ["%s = f2u(%s)" % (d, a[0])]
            if op == "bitcast":
                if sty == "float" and ins.ty == "i32":
                    return ["%s = f2bits(%s)" % (d, a[0])]
                if sty == "i32" and ins.ty == "float":
                    return ["%s = bits2f(%s)" % (d, a[0])]
                return ["%s = %s" % (d, a[0])]       # pointer casts
            if op == "zext":
                return ["%s = int(%s)" % (d, a[0])]
            if op == "sext":
                return ["%s = (M32 if %s else 0)" % (d, a[0])] if sty == "i1" else ["%s = %s" % (d, a[0])]
            if op == "trunc":
                return ["%s = %s & %s" % (d, a[0], "1" if ins.ty == "i1" else hex((1 << int(ins.ty[1:])) - 1))]
        if op == "extractvalue":
            return ["%s = %s[%d]" % (d, a[0], ins.extra["index"][0])]
        if op == "alloca":
            return []
        if op == "getelementptr":
            base = ins.args[0]
            idx = a[1:]
            if base.kind == "glob":
                g = base.v
                return ["%s = (%s, %s, %r)" % (d, a[0], idx[-1], g)]
            if len(idx) == 2:   # [N x T]* %alloca, 0, k
                return ["%s = (%s, %s, None)" % (d, a[0], idx[-1])]
            return ["%s = (%s[0], %s[1] + %s, %s[2])" % (d, a[0], a[0], idx[0], a[0])]
        if op == "load":
            p = a[0]
            return ["_p = %s" % p, "ldscheck(_p)", "%s = _p[0][_p[1]]" % d]
        if op == "store":
            return ["_p = %s" % a[1], "ldscheck(_p)", "_p[0][_p[1]] = %s" % a[0]]
        if op == "br":
            if len(ins.args) == 0:
                t = ins.extra["targets"][0]
                return self._phis_for_edge(blk, t, val, regs) + ["return %d" % bidx[t]]
            t, f = ins.extra["targets"]
            out = ["if %s:" % a[0]]
            out += ["  " + l for l in self._phis_for_edge(blk, t, val, regs) + ["return %d" % bidx[t]]]
            out += self._phis_for_edge(blk, f, val, regs) + ["return %d" % bidx[f]]
            return out
        if op == "switch":
            out = ["_s = %s" % a[0]]
            for cv, t in ins.extra["cases"]:
                out += ["if _s == %d:" % (cv & M32)]
                out += ["  " + l for l in self._phis_for_edge(blk, t, val, regs) + ["return %d" % bidx[t]]]
            t = ins.extra["default"]
            return out + self._phis_for_edge(blk, t, val, regs) + ["return %d" % bidx[t]]
        if op == "ret":
            return ["return -1"]
        if op == "call":
            return self._gen_call(ins, d, a)
        raise NotImplementedError(ins.raw)

    def _gen_call(self, ins, d, a):
        fn = ins.extra["fn"]
        oc = ins.extra["opcode"]
        isint = fn.endswith(".i32")
        if fn.startswith("dx.op.createHandle"):
            return ["%s = X.handle(%s, %s)" % (d, a[1], a[3])]
        if fn.startswith("dx.op.cbufferLoadLegacy"):
            return ["%s = %s.get(%s, %s)" % (d, a[1], a[2], isint)]
        if fn.startswith("dx.op.threadId."):
            return ["%s = X.tid[%s]" % (d, a[1])]
        if fn.startswith("dx.op.groupId."):
            return ["%s = X.gid[%s]" % (d, a[1])]
        if fn.startswith("dx.op.threadIdInGroup."):
            return ["%s = X.gtid[%s]" % (d, a[1])]
        if fn.startswith("dx.op.flattenedThreadIdInGroup"):
            return ["%s = X.flat" % d]
        if fn.startswith("dx.op.unary."):
            if isint:
                raise NotImplementedError(ins.raw)
            return ["%s = %s" % (d, UNARY[oc].format(a[1]))]
        if fn.startswith("dx.op.binary."):
            return ["%s = %s" % (d, BINARY[oc].format(a[1], a[2]))]
        if fn.startswith("dx.op.tertiary."):
            if oc == 46:
                return ["%s = rf(rf(%s * %s) + %s)" % (d, a[1], a[2], a[3])]
            if oc == 48:
                return ["%s = (s32(%s) * s32(%s) + %s) & M32" % (d, a[1], a[2], a[3])]
            if oc == 49:
                return ["%s = (%s * %s + %s) & M32" % (d, a[1], a[2], a[3])]
            raise NotImplementedError(ins.raw)
        if fn.startswith("dx.op.dot2."):
            return ["%s = rf(rf(%s * %s) + rf(%s * %s))" % (d, a[1], a[3], a[2], a[4])]
        if fn.startswith("dx.op.dot3."):
            return ["%s = rf(rf(rf(%s * %s) + rf(%s * %s)) + rf(%s * %s))" % (d, a[1], a[4], a[2], a[5], a[3], a[6])]
        if fn.startswith("dx.op.dot4."):
            return ["%s = rf(rf(rf(rf(%s * %s) + rf(%s * %s)) + rf(%s * %s)) + rf(%s * %s))" % (
                d, a[1], a[5], a[2], a[6], a[3], a[7], a[4], a[8])]
        if fn.startswith("dx.op.getDimensions"):
            return ["%s = %s.dims(%s)" % (d, a[1], a[2])]
        if fn.startswith("dx.op.textureLoad."):
            # (h, mip, c0, c1, c2, o0, o1, o2)
            return ["%s = %s.load(%s, %s, %s, %s, %s, %s, %s)" % (d, a[1], a[2], a[3], a[4], a[5],
                                                                   "s32(%s)" % a[6], "s32(%s)" % a[7], "s32(%s)" % a[8])]
        if fn.startswith("dx.op.bufferLoad."):
            return ["%s = X.bload(%s, %s, %s, %s)" % (d, a[1], a[2], a[3], isint)]
        if fn.startswith("dx.op.rawBufferLoad."):
            return ["%s = X.bload(%s, %s, %s, %s)" % (d, a[1], a[2], a[3], isint)]
        if fn.startswith("dx.op.sampleLevel."):
            # (h, s, c0, c1, c2, c3, o0, o1, o2, lod)
            return ["%s = %s.sample(%s, %s, %s, %s, %s, s32(%s), s32(%s))" % (d, a[1], a[2], a[3], a[4], a[5], a[10], a[7], a[8])]
        if fn.startswith("dx.op.sampleCmpLevelZero."):
            # (h, s, c0, c1, c2, c3, o0, o1, o2, cmp)
            return ["%s = %s.sample(%s, %s, %s, %s, 0.0, s32(%s), s32(%s), %s)" % (d, a[1], a[2], a[3], a[4], a[5], a[7], a[8], a[10])]
        if fn.startswith("dx.op.textureStore."):
            # (h, c0, c1, c2, v0..v3, mask)
            return ["%s.store(%s, %s, %s, (%s, %s, %s, %s), %s, X.who)" % (a[1], a[2], a[3], a[4], a[5], a[6], a[7], a[8], a[9])]
        if fn.startswith("dx.op.bufferStore.") or fn.startswith("dx.op.rawBufferStore."):
            return ["X.bstore(%s, %s, %s, (%s, %s, %s, %s), %s, %s)" % (a[1], a[2], a[3], a[4], a[5], a[6], a[7], a[8], isint)]
        if fn.startswith("dx.op.barrier"):
            return ["X.barrier()"]
        raise NotImplementedError(ins.raw)

    # ------------------------------------------------------------------------------------------- execution
    def dispatch(self, scene, groups, numthreads=None, coverage=None, max_steps=2_000_000):
        nt = numthreads or self.numthreads
        X = Ctx(scene)
        cov = coverage if coverage is not None else [0] * len(self.m.blocks)
        for g in groups:
            lds = {n: ([0] * c if t != "float" else [0.0] * c) for n, (c, t) in self.lds_globals.items()}
            owner = {}

            def ldscheck(p, owner=owner):
                if p[2] is not None and p[2] in self.lds_globals:
                    k = (p[2], p[1])
                    o = owner.setdefault(k, X.flat)
                    if o != X.flat:
                        raise LDSViolation("LDS %s[%d] touched by threads %d and %d" % (p[2], p[1], o, X.flat))

            F = self._build(X, self.consts, lds, ldscheck)
            for tz in range(nt[2]):
                for ty in range(nt[1]):
                    for tx in range(nt[0]):
                        X.gid = g
                        X.gtid = (tx, ty, tz)
                        X.tid = (g[0] * nt[0] + tx, g[1] * nt[1] + ty, g[2] * nt[2] + tz)
                        X.flat = tx + nt[0] * (ty + nt[1] * tz)
                        X.who = X.tid
                        R = [None] * self.nregs
                        for r, n, t in self.alloca_init:
                            R[r] = [0.0] * n if t == "float" else [0] * n
                        b, steps = 0, 0
                        if self.timing:
                            T = [0.0] * self.nregs
                            S = [0.0, 0, 0.0, 0]          # control time, dyn. instructions, store time, mem ops
                            M = {}
                            while b >= 0:
                                cov[b] += 1
                                b = F[b](R, T, S, M)
                            X.times.append((X.tid, max(S[0], S[2]), S[1], S[3]))
                            continue
                        while b >= 0:
                            cov[b] += 1
                            b = F[b](R)
                            steps += 1
                            if steps > max_steps:
                                raise RuntimeError("runaway thread %r" % (X.tid,))
        return X, cov


class Ctx:
    def __init__(self, scene):
        self.scene = scene
        self.tid = self.gid = self.gtid = (0, 0, 0)
        self.flat = 0
        self.who = None
        self.times = []

    def handle(self, cls, reg):
        r = self.scene.b.get((cls, reg))
        if r is None:
            raise KeyError("resource class %d register %d not bound in scene" % (cls, reg))
        return r

    def bload(self, h, idx, off, isint):
        if isinstance(h, StructBuffer):
            return h.load(idx, off, isint)
        return h.load(s32(idx) if isinstance(idx, int) else idx, off)

    def bstore(self, h, idx, off, vals, mask, isint):
        if isinstance(h, StructBuffer):
            h.store(idx, off, vals, mask, self.who, isint)
        else:
            h.store(s32(idx), off, vals, mask, self.who)

    def barrier(self):
        raise NotImplementedError("barrier: threads are run sequentially")


def uav_state(scene):
    """canonical snapshot of every UAV: {(cls,reg): (sorted final contents, write log)}"""
    out = {}
    for k, r in scene.b.items():
        if k[0] != 1:
            continue
        if isinstance(r, RWTexture):
            out[k] = (sorted(r.data.items()), list(r.writes))
        elif isinstance(r, TypedBuffer):
            out[k] = ([tuple(x) for x in r.data], list(r.writes))
        elif isinstance(r, StructBuffer):
            out[k] = (bytes(r.b), list(r.writes))
    return out


def ulp_diff(a, b):
    """distance in float32 ulps (NaN == NaN, +0 == -0)."""
    if isinstance(a, int) and isinstance(b, int):
        return 0 if a == b else 1 << 40
    if a != a and b != b:
        return 0
    if a != a or b != b:
        return 1 << 40
    ia, ib = f2bits(a), f2bits(b)
    ia = ia if ia < 0x80000000 else 0x80000000 - ia
    ib = ib if ib < 0x80000000 else 0x80000000 - ib
    return abs(ia - ib)
