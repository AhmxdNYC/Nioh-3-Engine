r"""dxbc_interp.py -- reference interpreter for SM4/SM5 pixel shaders (DXBC), used to prove a replacement computes
bit-for-bit the same outputs as the shipped shader.

The program is read from D3DDisassemble text with hex literals (exact constant bits). Every register component is a
uint32; float ops run in IEEE float32.
Op models (the same model runs both programs, so what matters is that it is *strict*):
  * add/mul/min/max/compare/frc/round/convert: IEEE float32, minNum/maxNum NaN rules, _sat clamps (NaN -> 0).
  * mad: exact fused multiply-add (one rounding), so it is distinguishable from mul+add.
  * dp2/dp3/dp4: left-to-right sum of rounded products.
  * div/sqrt/rsq/rcp/exp/log/sincos: IEEE result with the lowest mantissa bit XOR'ed with a hash of the operand bits
    ("opaque"): a rewrite that relies on an algebraic identity of these ops (a/b*c vs a*c/b ...) does not match.
  * sample_l / sample: bilinear on the synthetic texture (clamp addressing), result low 2 bits XOR'ed with a hash of the
    exact coordinate and LOD bits: any change of coordinate bits shows. ld: exact texel, out of bounds -> 0.
Stats per pixel: fetches issued and "rounds" (a fetch result consumed while fetches are in flight = one wait; a
wait is also counted at loop back-edges / branches when fetches are pending).
"""
import ctypes, math, re, struct

import numpy as np

F = np.float32
_d3dc = None


def disassemble(blob):
    global _d3dc
    if _d3dc is None:
        _d3dc = ctypes.WinDLL("D3DCompiler_47.dll")
    out = ctypes.c_void_p()
    hr = _d3dc.D3DDisassemble(blob, len(blob), 0x80, None, ctypes.byref(out))  # D3D_DISASM_PRINT_HEX_LITERALS
    if hr != 0:
        raise RuntimeError("D3DDisassemble failed %x" % (hr & 0xFFFFFFFF))
    vt = ctypes.cast(ctypes.cast(out, ctypes.POINTER(ctypes.c_void_p))[0], ctypes.POINTER(ctypes.c_void_p))
    gp = ctypes.WINFUNCTYPE(ctypes.c_void_p, ctypes.c_void_p)(vt[3])
    gs = ctypes.WINFUNCTYPE(ctypes.c_size_t, ctypes.c_void_p)(vt[4])
    t = ctypes.string_at(gp(out), gs(out)).decode("latin1")
    ctypes.WINFUNCTYPE(ctypes.c_ulong, ctypes.c_void_p)(vt[2])(out)
    return t.replace("\x00", "")


def b2f(u):
    return struct.unpack("<f", struct.pack("<I", u & 0xFFFFFFFF))[0]


def f2b(f):
    return int(np.array([f], dtype=np.float32).view(np.uint32)[0])


def mix(*xs):
    h = 0x811C9DC5
    for x in xs:
        h = ((h ^ (x & 0xFFFFFFFF)) * 0x01000193) & 0xFFFFFFFF
        h ^= h >> 15
    return h


def opaque(bits, *ops):
    if (bits & 0x7F800000) == 0x7F800000:  # inf / nan stay
        return bits
    return bits ^ (mix(*ops) & 1)


def fma32(a, b, c):
    """exact float32 fma of float32 inputs (python floats holding f32 values)."""
    if not (math.isfinite(a) and math.isfinite(b) and math.isfinite(c)):
        return F(a * b + c)
    p = a * b  # exact in double
    s = p + c
    bb = s - p
    e = (p - (s - bb)) + (c - bb)  # TwoSum error, s + e == p + c exactly
    r = F(s)
    if e == 0.0 or not math.isfinite(float(r)):
        return r
    rd = float(r)
    if rd == s:
        return r
    nxt = np.nextafter(r, F(math.copysign(math.inf, s - rd)))
    mid = (rd + float(nxt)) / 2.0
    if s == mid:  # tie in double, broken by the error term
        return nxt if (e > 0) == (float(nxt) > rd) else r
    return r


SWZ = {"x": 0, "y": 1, "z": 2, "w": 3, "r": 0, "g": 1, "b": 2, "a": 3}


class Opnd:
    __slots__ = ("kind", "idx", "rel", "swz", "neg", "abs", "lit", "mask")

    def __init__(self):
        self.kind = None; self.idx = 0; self.rel = None; self.swz = [0, 1, 2, 3]; self.neg = False; self.abs = False
        self.lit = None; self.mask = [0, 1, 2, 3]


def split_operands(s):
    out, depth, cur = [], 0, ""
    for ch in s:
        if ch in "([":
            depth += 1
        elif ch in ")]":
            depth -= 1
        if ch == "," and depth == 0:
            out.append(cur.strip()); cur = ""
        else:
            cur += ch
    if cur.strip():
        out.append(cur.strip())
    return out


def parse_lit(tok):
    tok = tok.strip()
    if tok.lower().startswith(("0x", "-0x")):
        v = int(tok, 16)
        return v & 0xFFFFFFFF
    if re.match(r"^-?\d+$", tok):
        return int(tok) & 0xFFFFFFFF
    return f2b(float(tok))


def parse_operand(s):
    o = Opnd()
    s = s.strip()
    if s == "null":
        o.kind = "null"
        return o
    if s.startswith("-"):
        o.neg = True; s = s[1:]
    if s.startswith("|") :
        o.abs = True; s = s.strip("|")
        if "|" in s:
            s = s.replace("|", "")
    if s.startswith("l("):
        vals = [parse_lit(t) for t in s[2:s.rindex(")")].split(",")]
        if len(vals) == 1:
            vals = vals * 4
        o.kind = "l"; o.lit = vals
        return o
    m = re.match(r"^(icb|cb\d+|x\d+|r|v|o|t|s|u|g)(\d*)(?:\[(.*?)\])?(?:\[(.*?)\])?(?:\.([xyzwrgba]+))?$", s)
    if not m:
        raise ValueError("operand %r" % s)
    k, num, i1, i2, sw = m.groups()
    if k.startswith("cb"):
        o.kind = "cb"; o.idx = (int(k[2:]), i1)
    elif k.startswith("x"):
        o.kind = "x"; o.idx = (int(k[1:]), i1)
    elif k == "icb":
        o.kind = "icb"; o.idx = (0, i1)
    else:
        o.kind = k; o.idx = int(num) if num else 0
    if sw:
        comps = [SWZ[c] for c in sw]
        o.mask = comps
        o.swz = comps if len(comps) == 4 else (comps * 4)[:4] if len(comps) == 1 else comps + [comps[-1]] * (4 - len(comps))
    return o


def parse_index(expr):
    """'r1.w + 3' / '5' / 'r1.x' -> (regname, comp, const)"""
    expr = expr.strip()
    m = re.match(r"^(r|x\d+)(\d*)\.([xyzw])(?:\s*\+\s*(\d+))?$", expr)
    if m:
        return ("r", int(m.group(2)), SWZ[m.group(3)], int(m.group(4) or 0))
    return (None, 0, 0, int(expr))


class Ins:
    __slots__ = ("op", "sat", "ops", "aoff", "line", "target", "rdim", "text")


class Program:
    def __init__(self, text):
        self.text = text
        self.ins = []
        self.cb_size = {}
        self.tex_dim = {}
        self.icb = []
        self.itemps = {}
        self.ntemps = 0
        lines = [l.strip() for l in text.splitlines()]
        i = 0
        while i < len(lines):
            l = lines[i]
            i += 1
            if not l or l.startswith("//") or re.match(r"^(ps|vs|cs)_\d_\d$", l):
                continue
            if l.startswith("dcl_immediateConstantBuffer"):
                body = l
                while body.count("{") != body.count("}") and i < len(lines):
                    body += lines[i]; i += 1
                inner = body[body.index("{") + 1: body.rindex("}")]
                for grp in re.findall(r"\{([^{}]*)\}", inner):
                    self.icb.append([parse_lit(t) for t in grp.split(",")])
                continue
            if l.startswith("dcl_constantbuffer"):
                m = re.search(r"CB(\d+)\[(\d+)\]", l)
                self.cb_size[int(m.group(1))] = int(m.group(2))
                continue
            if l.startswith("dcl_resource_texture"):
                m = re.match(r"dcl_resource_(texture\w+) .* t(\d+)$", l)
                self.tex_dim[int(m.group(2))] = m.group(1)
                continue
            if l.startswith("dcl_temps"):
                self.ntemps = int(l.split()[1]); continue
            if l.startswith("dcl_indexableTemp"):
                m = re.match(r"dcl_indexableTemp x(\d+)\[(\d+)\], (\d+)", l)
                self.itemps[int(m.group(1))] = int(m.group(2)); continue
            if l.startswith("dcl_") or l.startswith("{") or l.startswith("}"):
                continue
            ins = Ins()
            ins.text = l
            m = re.match(r"^(\w+)((?:\([^)]*\))*)\s*(.*)$", l)
            op, paren, rest = m.groups()
            ins.aoff = (0, 0, 0)
            ins.rdim = None
            mm = re.match(r"^(\w+?)_aoffimmi$", op)
            pm = re.findall(r"\(([^)]*)\)", paren)
            if op.endswith("_indexable"):
                op = op[: -len("_indexable")]
            if op.endswith("_aoffimmi"):
                op = op[: -len("_aoffimmi")]
                ins.aoff = tuple(int(v) for v in pm[0].split(","))
                pm = pm[1:]
            if pm:
                ins.rdim = pm[0]
            ins.sat = op.endswith("_sat")
            if ins.sat:
                op = op[:-4]
            ins.op = op
            ins.ops = [parse_operand(t) for t in split_operands(rest)] if rest else []
            ins.target = None
            self.ins.append(ins)
        self._link()

    def _link(self):
        stack = []
        for n, ins in enumerate(self.ins):
            if ins.op in ("if_nz", "if_z", "loop", "switch"):
                stack.append(n)
            elif ins.op == "else":
                self.ins[stack[-1]].target = n
                stack[-1] = n
            elif ins.op == "endif":
                self.ins[stack.pop()].target = n
            elif ins.op == "endloop":
                s = stack.pop()
                self.ins[s].target = n
                ins.target = s
        # breaks -> innermost loop's endloop
        loops = []
        for n, ins in enumerate(self.ins):
            if ins.op == "loop":
                loops.append(n)
            elif ins.op == "endloop":
                loops.pop()
            elif ins.op in ("break", "breakc_nz", "breakc_z", "continue", "continuec_nz", "continuec_z"):
                ins.target = loops[-1]


class Tex:
    """float32 texture with W x H (x D) texels of 4 channels (numpy array)."""

    def __init__(self, data, tag):
        self.data = np.ascontiguousarray(data, dtype=np.float32)
        self.tag = tag
        self.bits = self.data.view(np.uint32)

    @property
    def dims(self):
        return self.data.shape[:-1][::-1]  # (w, h[, d])


class Machine:
    LOOP_LIMIT = 200000

    def __init__(self, prog, cbs, texs, sampler_mode=None):
        self.p = prog
        self.cbs = cbs      # {slot: list of [4 x uint32]}
        self.texs = texs    # {slot: Tex}
        self.smode = sampler_mode or {}
        self.trace = None
        self.cur = None

    # ---- operand access
    def rd(self, R, o, n=4):
        k = o.kind
        if k == "l":
            v = list(o.lit)
        elif k == "r":
            v = R["r"][o.idx]
        elif k == "v":
            v = self.inputs[o.idx]
        elif k == "o":
            v = R["o"].setdefault(o.idx, [0, 0, 0, 0])
        elif k == "cb":
            slot, ie = o.idx
            v = self.cbs[slot][self.index(R, ie)] if self.index(R, ie) < len(self.cbs[slot]) else [0, 0, 0, 0]
        elif k == "icb":
            ii = self.index(R, o.idx[1])
            v = self.p.icb[ii] if ii < len(self.p.icb) else [0, 0, 0, 0]
        elif k == "x":
            v = R["x"][o.idx[0]][self.index(R, o.idx[1])]
        else:
            raise ValueError(k)
        v = [v[o.swz[c]] for c in range(4)]
        if o.abs:
            v = [x & 0x7FFFFFFF for x in v]
        if o.neg:
            v = [x ^ 0x80000000 for x in v]
        return v

    def rdi(self, R, o):  # integer read: neg means integer negate
        if o.neg:
            o.neg = False
            try:
                v = self.rd(R, o)
            finally:
                o.neg = True
            return [(-x) & 0xFFFFFFFF for x in v]
        return self.rd(R, o)

    def index(self, R, ie):
        if ie is None:
            return 0
        reg, idx, comp, c = parse_index(ie) if isinstance(ie, str) else ie
        if reg is None:
            return c
        return (R["r"][idx][comp] + c) & 0xFFFFFFFF

    def wr(self, R, o, v, sat=False):
        if o.kind == "null":
            return
        if o.kind == "r":
            dst = R["r"][o.idx]
        elif o.kind == "o":
            dst = R["o"].setdefault(o.idx, [0, 0, 0, 0])
        elif o.kind == "x":
            dst = R["x"][o.idx[0]][self.index(R, o.idx[1])]
        else:
            raise ValueError("dest %s" % o.kind)
        for c in o.mask:
            x = v[c]
            if sat:
                f = b2f(x)
                f = 0.0 if f != f else min(max(f, 0.0), 1.0)
                x = f2b(f)
            dst[c] = x & 0xFFFFFFFF
            if o.kind == "r":
                self.pending_dst.discard((o.idx, c))
        if self.trace is not None:
            self.trace.append((self.cur, [dst[c] for c in o.mask]))

    # ---- fetch helpers
    def fetch_wait(self, R, ops):
        """count a wait if any operand component read is the result of a pending fetch"""
        if not self.pending:
            return
        for o in ops:
            if o.kind == "r":
                for c in set(o.swz if o.mask is None else o.swz):
                    if (o.idx, c) in self.pending:
                        self.rounds += 1
                        self.pending.clear()
                        return

    def texel(self, t, x, y, z=0):
        d = t.data
        if d.ndim == 3:
            h, w = d.shape[:2]
            if 0 <= x < w and 0 <= y < h:
                return [int(b) for b in t.bits[y, x]]
        else:
            dd, h, w = d.shape[:3]
            if 0 <= x < w and 0 <= y < h and 0 <= z < dd:
                return [int(b) for b in t.bits[z, y, x]]
        return [0, 0, 0, 0]

    def sample(self, t, coord, lod, slot):
        d = t.data
        mode = self.smode.get(slot, "clamp")
        if d.ndim == 3:
            h, w = d.shape[:2]
            u, v = b2f(coord[0]), b2f(coord[1])
            if not (math.isfinite(u) and math.isfinite(v)):
                u = 0.0 if not math.isfinite(u) else u
                v = 0.0 if not math.isfinite(v) else v
            fx, fy = u * w - 0.5, v * h - 0.5
            x0, y0 = math.floor(fx), math.floor(fy)
            ax, ay = fx - x0, fy - y0

            def tx(x, y):
                if mode == "wrap":
                    x %= w; y %= h
                else:
                    x = min(max(x, 0), w - 1); y = min(max(y, 0), h - 1)
                return d[y, x].astype(np.float64)
            with np.errstate(all="ignore"):
                c = (tx(x0, y0) * (1 - ax) + tx(x0 + 1, y0) * ax) * (1 - ay) + (tx(x0, y0 + 1) * (1 - ax) + tx(x0 + 1, y0 + 1) * ax) * ay
            key = (coord[0], coord[1], lod)
        else:
            dd, h, w = d.shape[:3]
            u, v, s = (b2f(coord[i]) for i in range(3))
            u, v, s = (0.0 if not math.isfinite(q) else q for q in (u, v, s))
            x = min(max(int(math.floor(u * w)), 0), w - 1)
            y = min(max(int(math.floor(v * h)), 0), h - 1)
            z = min(max(int(math.floor(s * dd)), 0), dd - 1)
            c = d[z, y, x].astype(np.float64)
            key = (coord[0], coord[1], coord[2], lod)
        with np.errstate(all="ignore"):
            out = [f2b(F(q)) for q in c]
        hsh = mix(t.tag, *key)
        return [opaque(out[i] ^ ((hsh >> (2 * i)) & 2), hsh, i) for i in range(4)]

    # ---- run
    def run(self, inputs):
        self.inputs = inputs
        R = {"r": [[0, 0, 0, 0] for _ in range(max(self.p.ntemps, 1))], "o": {},
             "x": {k: [[0, 0, 0, 0] for _ in range(n)] for k, n in self.p.itemps.items()}}
        self.pending = set(); self.pending_dst = self.pending
        self.rounds = 0; self.fetches = 0; self.discard = False
        pc, steps, P = 0, 0, self.p.ins
        cov = set()
        while pc < len(P):
            ins = P[pc]
            cov.add(pc)
            steps += 1
            if steps > self.LOOP_LIMIT:
                return {"hang": True}
            op = ins.op
            nxt = pc + 1
            if op in ("if_nz", "if_z", "breakc_nz", "breakc_z", "retc_nz", "retc_z", "continuec_nz", "continuec_z",
                      "discard_nz", "discard_z"):
                self.fetch_wait(R, ins.ops)
                c = self.rd(R, ins.ops[0])[0] != 0
                if op.endswith("_z"):
                    c = not c
                if self.pending and op.startswith(("if", "break", "continue")):
                    self.rounds += 1; self.pending.clear()
                if op.startswith("if"):
                    if not c:
                        t = ins.target
                        nxt = t + 1
                elif op.startswith("break"):
                    if c:
                        nxt = P[ins.target].target + 1
                elif op.startswith("continue"):
                    if c:
                        nxt = P[ins.target].target
                elif op.startswith("ret"):
                    if c:
                        break
                elif op.startswith("discard"):
                    if c:
                        self.discard = True; break
            elif op == "else":
                nxt = ins.target + 1 if P[ins.target].op == "endif" else ins.target + 1
            elif op in ("endif", "loop"):
                pass
            elif op == "endloop":
                if self.pending:
                    self.rounds += 1; self.pending.clear()
                nxt = ins.target + 1
            elif op == "break":
                nxt = P[ins.target].target + 1
            elif op == "continue":
                nxt = P[ins.target].target
            elif op == "ret":
                break
            else:
                self.cur = ins.text
                self.exec(R, ins)
            pc = nxt
        return {"o": {k: list(v) for k, v in R["o"].items()}, "fetches": self.fetches, "rounds": self.rounds,
                "discard": self.discard, "hang": False, "cov": cov}

    def exec(self, R, ins):
        op, ops = ins.op, ins.ops
        self.fetch_wait(R, ops[1:] if op != "sincos" else ops[2:])
        if op in ("sample_l", "sample", "ld", "sample_b", "sample_d", "resinfo", "gather4"):
            return self.exec_tex(R, ins)
        fop = FLOAT_OPS.get(op)
        if fop:
            srcs = [self.rd(R, o) for o in ops[1:]]
            res = [0] * 4
            for c in ops[0].mask:
                fs = [b2f(s[c]) for s in srcs]
                with np.errstate(all="ignore"):
                    res[c] = fop(fs, [s[c] for s in srcs])
            self.wr(R, ops[0], res, ins.sat)
            return
        iop = INT_OPS.get(op)
        if iop:
            srcs = [self.rdi(R, o) for o in ops[1:]]
            res = [0] * 4
            for c in ops[0].mask:
                res[c] = iop([s[c] for s in srcs]) & 0xFFFFFFFF
            self.wr(R, ops[0], res)
            return
        if op in ("dp2", "dp3", "dp4"):
            n = int(op[2])
            a, b = self.rd(R, ops[1]), self.rd(R, ops[2])
            with np.errstate(all="ignore"):
                acc = F(b2f(a[0])) * F(b2f(b[0]))
                for i in range(1, n):
                    acc = F(acc + F(b2f(a[i])) * F(b2f(b[i])))
            r = f2b(acc)
            self.wr(R, ops[0], [r] * 4, ins.sat)
            return
        if op == "movc":
            c, a, b = (self.rd(R, o) for o in ops[1:])
            self.wr(R, ops[0], [a[i] if c[i] else b[i] for i in range(4)], ins.sat)
            return
        if op == "sincos":
            a = self.rd(R, ops[2])
            s, co = [0] * 4, [0] * 4
            for i in range(4):
                f = b2f(a[i])
                with np.errstate(all="ignore"):
                    s[i] = opaque(f2b(F(math.sin(f)) if math.isfinite(f) else F("nan")), 0x5111, a[i])
                    co[i] = opaque(f2b(F(math.cos(f)) if math.isfinite(f) else F("nan")), 0xC051, a[i])
            if ops[0].kind != "null":
                self.wr(R, ops[0], s)
            if ops[1].kind != "null":
                self.wr(R, ops[1], co)
            return
        raise NotImplementedError(ins.text)

    def exec_tex(self, R, ins):
        op, ops = ins.op, ins.ops
        rs = ops[2]
        t = self.texs.get(rs.idx)
        if op == "resinfo":
            w, h = (t.dims[0], t.dims[1]) if t else (0, 0)
            d = t.dims[2] if t and len(t.dims) > 2 else 1
            v = [f2b(F(w)), f2b(F(h)), f2b(F(d)), f2b(F(1))]
            if "uint" in ins.text.split()[0]:
                v = [w, h, d, 1]
            self.wr(R, ops[0], [v[rs.swz[c]] for c in range(4)])
            return
        self.fetches += 1
        coord = self.rd(R, ops[1])
        if t is None:
            v = [0, 0, 0, 0]
        elif op == "ld":
            ci = [x if x < 0x80000000 else x - 0x100000000 for x in coord]
            if t.data.ndim == 3 and ci[2] != 0:
                v = [0, 0, 0, 0]  # only mip 0 modelled
            elif t.data.ndim == 3:
                v = self.texel(t, ci[0] + ins.aoff[0], ci[1] + ins.aoff[1])
            else:
                v = self.texel(t, ci[0] + ins.aoff[0], ci[1] + ins.aoff[1], ci[2] + ins.aoff[2])
        elif op in ("sample_l", "sample"):
            lod = self.rd(R, ops[4])[0] if op == "sample_l" else 0
            if any(ins.aoff):
                raise NotImplementedError("sample offset")
            v = self.sample(t, coord, lod, ops[3].idx)
        else:
            raise NotImplementedError(ins.text)
        v = [v[rs.swz[c]] for c in range(4)]
        self.wr(R, ops[0], v)
        if ops[0].kind == "r":
            for c in ops[0].mask:
                self.pending.add((ops[0].idx, c))


def _f(fn):
    def g(fs, bs):
        return f2b(fn(*[F(x) for x in fs]))
    return g


def _opq(fn, tag):
    def g(fs, bs):
        return opaque(f2b(fn(*[F(x) for x in fs])), tag, *bs)
    return g


def _cmp(fn):
    def g(fs, bs):
        return 0xFFFFFFFF if fn(*[F(x) for x in fs]) else 0
    return g


def _frc(a):
    return F(a - np.floor(a))


def _ftoi(a):
    if a != a:
        return 0
    if a >= 2147483647.0:
        return 0x7FFFFFFF
    if a <= -2147483648.0:
        return 0x80000000
    return int(math.trunc(float(a))) & 0xFFFFFFFF


def _ftou(a):
    if a != a or a <= 0:
        return 0
    if a >= 4294967295.0:
        return 0xFFFFFFFF
    return int(math.trunc(float(a)))


def _ubfe(w, o, x):
    if w == 0:
        return 0
    if w + o < 32:
        return ((x << (32 - w - o)) & 0xFFFFFFFF) >> (32 - w)
    return x >> o


def _ibfe(w, o, x):
    if w == 0:
        return 0
    if w + o < 32:
        return _sgn((x << (32 - w - o)) & 0xFFFFFFFF) >> (32 - w)
    return _sgn(x) >> o


def _sgn(x):
    return x - 0x100000000 if x & 0x80000000 else x


FLOAT_OPS = {
    "mov": lambda fs, bs: bs[0],
    "add": _f(lambda a, b: F(a + b)),
    "mul": _f(lambda a, b: F(a * b)),
    "mad": lambda fs, bs: f2b(fma32(float(F(fs[0])), float(F(fs[1])), float(F(fs[2])))),
    "div": _opq(lambda a, b: F(a / b), 0xD1),
    "sqrt": _opq(lambda a: F(np.sqrt(a)), 0x5A),
    "rsq": _opq(lambda a: F(F(1) / np.sqrt(a)), 0x75),
    "rcp": _opq(lambda a: F(F(1) / a), 0x7C),
    "exp": _opq(lambda a: F(np.exp2(a)), 0xE1),
    "log": _opq(lambda a: F(np.log2(a)), 0x10),
    "min": _f(lambda a, b: F(np.fmin(a, b))),
    "max": _f(lambda a, b: F(np.fmax(a, b))),
    "frc": _f(_frc),
    "round_ni": _f(lambda a: F(np.floor(a))),
    "round_pi": _f(lambda a: F(np.ceil(a))),
    "round_z": _f(lambda a: F(np.trunc(a))),
    "round_ne": _f(lambda a: F(np.rint(a))),
    "lt": _cmp(lambda a, b: a < b),
    "ge": _cmp(lambda a, b: a >= b),
    "eq": _cmp(lambda a, b: a == b),
    "ne": _cmp(lambda a, b: not (a == b)),
    "ftoi": lambda fs, bs: _ftoi(F(fs[0])),
    "ftou": lambda fs, bs: _ftou(F(fs[0])),
    "itof": lambda fs, bs: f2b(F(_sgn(bs[0]))),
    "utof": lambda fs, bs: f2b(F(bs[0])),
}
INT_OPS = {
    "iadd": lambda s: s[0] + s[1],
    "imul": lambda s: _sgn(s[0]) * _sgn(s[1]),
    "imad": lambda s: _sgn(s[0]) * _sgn(s[1]) + s[2],
    "and": lambda s: s[0] & s[1],
    "or": lambda s: s[0] | s[1],
    "xor": lambda s: s[0] ^ s[1],
    "not": lambda s: ~s[0],
    "ishl": lambda s: s[0] << (s[1] & 31),
    "ushr": lambda s: s[0] >> (s[1] & 31),
    "ishr": lambda s: _sgn(s[0]) >> (s[1] & 31),
    "imax": lambda s: max(_sgn(s[0]), _sgn(s[1])),
    "imin": lambda s: min(_sgn(s[0]), _sgn(s[1])),
    "umax": lambda s: max(s[0], s[1]),
    "umin": lambda s: min(s[0], s[1]),
    "ige": lambda s: -1 if _sgn(s[0]) >= _sgn(s[1]) else 0,
    "ilt": lambda s: -1 if _sgn(s[0]) < _sgn(s[1]) else 0,
    "ieq": lambda s: -1 if s[0] == s[1] else 0,
    "ine": lambda s: -1 if s[0] != s[1] else 0,
    "uge": lambda s: -1 if s[0] >= s[1] else 0,
    "ult": lambda s: -1 if s[0] < s[1] else 0,
    "ineg": lambda s: -s[0],
    "ubfe": lambda s: _ubfe(s[0] & 31, s[1] & 31, s[2]),
    "ibfe": lambda s: _ibfe(s[0] & 31, s[1] & 31, s[2]),
}
