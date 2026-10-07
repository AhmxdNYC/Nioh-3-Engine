r"""sim.py - DXIL reference interpreter for the shader sweep: compute shaders WITH groupshared memory and barriers.

Built on a snapshot of the GI agent's interpreter (vendor_gi/llir.py + vendor_gi/dxil_interp.py, copied unmodified on
2026-10-07 from gpu-shaders/gi/; sha256 in notes/gpu-shader-sweep.md). What this file adds:

  * GroupMemoryBarrierWithGroupSync: every block is split at dx.op.barrier; the threads of a group run phase by phase
    (each thread runs until it reaches the next barrier, then the next thread ...). A group where some threads reach a
    barrier while others have returned is reported (divergent barrier).
  * LDS race detector: inside one phase, a groupshared slot written by one thread and read or written by another thread
    is reported as a race (the replacement shaders must be race free; the GPU would make such results timing dependent).
  * atomicrmw on groupshared (or/add/max/min/umax/umin/and/xor/xchg) - exempt from the race check, as on the GPU.
  * Everything else (float semantics, resources, robust out-of-bounds behaviour) is the vendored interpreter's.

    sh = BShader(open("x.ll").read())
    X, cov = sh.dispatch(scene, groups=[(0, 0, 0), ...])
"""
import os, re, sys
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "vendor_gi"))
import llir                       # noqa: E402  (vendored)
import dxil_interp as DI          # noqa: E402  (vendored)
from dxil_interp import *         # noqa: F401,F403,E402  (Scene, Texture, TypedBuffer, RWTexture, CBuffer, ...)

_orig_parse_instr = llir.parse_instr


def _parse_instr(line):
    s = line.strip()
    if " atomicrmw " in " " + s.split("  ; ")[0] + " ":
        s = s.split("  ; ")[0]
        dm = re.match(r"^%([\w.]+) = atomicrmw (?:volatile )?(\w+) (.*?) seq_cst$", s)
        if not dm:
            raise ValueError("unsupported atomicrmw: " + line)
        dest, aop, rest = dm.groups()
        p, v = llir.split_top(rest)
        return llir.Instr(dest, "atomicrmw", v.split()[0], [llir.typed(p), llir.typed(v)], line, {"aop": aop})
    return _orig_parse_instr(line)


llir.parse_instr = _parse_instr


class Race(Exception):
    pass


class LdsTracker:
    def __init__(self):
        self.reset()

    def reset(self):
        self.w = {}       # slot -> writer flat id
        self.r = {}       # slot -> set of reader flat ids

    def __call__(self, p, kind, who):
        if p[2] is None:          # private alloca array
            return
        k = (p[2], p[1])
        if kind == 2:             # atomic
            return
        w = self.w.get(k)
        if kind == 0:
            if w is not None and w != who:
                raise Race("LDS %s[%d]: read by thread %d after write by thread %d in the same phase" % (k[0], k[1], who, w))
            self.r.setdefault(k, set()).add(who)
        else:
            if w is not None and w != who:
                raise Race("LDS %s[%d]: written by threads %d and %d in the same phase" % (k[0], k[1], w, who))
            rs = self.r.get(k, ())
            if any(x != who for x in rs):
                raise Race("LDS %s[%d]: written by thread %d after a read by another thread in the same phase" % (k[0], k[1], who))
            self.w[k] = who


AOPS = {"or": "({0} | {1})", "and": "({0} & {1})", "xor": "({0} ^ {1})", "add": "(({0} + {1}) & M32)",
        "sub": "(({0} - {1}) & M32)", "xchg": "{1}", "umax": "max({0}, {1})", "umin": "min({0}, {1})",
        "max": "({0} if s32({0}) >= s32({1}) else {1})", "min": "({0} if s32({0}) <= s32({1}) else {1})"}


def _scalar_lds(text):
    """scalar groupshared globals (`@g = external addrspace(3) global i32`) -> [1 x i32] + constant GEPs."""
    for m in list(re.finditer(r'^(@(?:"[^"]*"|[\w.$]+)) = external addrspace\(3\) global (\w+), align (\d+)$', text, re.M)):
        g, ty = m.group(1), m.group(2)
        text = text.replace(m.group(0), "%s = external addrspace(3) global [1 x %s], align %s" % (g, ty, m.group(3)))
        text = text.replace("%s addrspace(3)* %s" % (ty, g),
                            "%s addrspace(3)* getelementptr ([1 x %s], [1 x %s] addrspace(3)* %s, i32 0, i32 0)" % (ty, ty, ty, g))
    return text


class BShader(DI.Shader):
    def __init__(self, text, name="shader"):
        self.f64 = False
        text = _scalar_lds(text)
        self.m = llir.parse(text)
        self.name = name
        self._split_barriers()
        self._parse_header(text)
        self._compile()

    def _split_barriers(self):
        m = self.m
        newblocks = []
        renamed = {}                       # original block name -> name of its last piece (phi sources)
        for b in m.blocks:
            pieces, cur = [], llir.Block(b.name)
            k = 0
            for ins in b.instrs:
                if ins.op == "call" and ins.extra.get("fn", "").startswith("dx.op.barrier"):
                    k += 1
                    nxt = "%s__b%d" % (b.name, k)
                    cur.instrs.append(llir.Instr(None, "br", None, [], "barrier -> " + nxt, {"targets": [nxt], "barrier": True}))
                    pieces.append(cur)
                    cur = llir.Block(nxt)
                else:
                    cur.instrs.append(ins)
            pieces.append(cur)
            if len(pieces) > 1:
                renamed[b.name] = pieces[-1].name
            newblocks.extend(pieces)
        for b in newblocks:
            for ins in b.instrs:
                if ins.op == "phi":
                    ins.extra["from"] = [renamed.get(x, x) for x in ins.extra["from"]]
        m.blocks = newblocks
        m.byname = {b.name: b for b in newblocks}
        for b in newblocks:
            t = b.instrs[-1]
            b.succs = list(t.extra["targets"]) if t.op == "br" else ([t.extra["default"]] + [x[1] for x in t.extra["cases"]] if t.op == "switch" else [])
            b.preds = []
        for b in newblocks:
            for s in b.succs:
                m.byname[s].preds.append(b.name)

    def _gen(self, ins, val, regs, bidx, blk):
        if ins.op == "br" and ins.extra.get("barrier"):
            return ["return %d" % (-2 - bidx[ins.extra["targets"][0]])]
        if ins.op == "load":
            return ["_p = %s" % val(ins.args[0]), "ldscheck(_p, 0, X.flat)", "R[%d] = _p[0][_p[1]]" % regs[ins.dest]]
        if ins.op == "store":
            return ["_p = %s" % val(ins.args[1]), "ldscheck(_p, 1, X.flat)", "_p[0][_p[1]] = %s" % val(ins.args[0])]
        if ins.op == "atomicrmw":
            d = "R[%d]" % regs[ins.dest]
            e = AOPS[ins.extra["aop"]].format("_o", val(ins.args[1]))
            return ["_p = %s" % val(ins.args[0]), "ldscheck(_p, 2, X.flat)", "_o = _p[0][_p[1]]", "%s = _o" % d,
                    "_p[0][_p[1]] = %s" % e]
        return DI.Shader._gen(self, ins, val, regs, bidx, blk)

    def dispatch(self, scene, groups, numthreads=None, coverage=None, max_steps=4_000_000):
        nt = numthreads or self.numthreads
        X = DI.Ctx(scene)
        cov = coverage if coverage is not None else [0] * len(self.m.blocks)
        for g in groups:
            lds = {n: ([0] * c if t != "float" else [0.0] * c) for n, (c, t) in self.lds_globals.items()}
            tr = LdsTracker()
            F = self._build(X, self.consts, lds, tr)
            th = []
            for tz in range(nt[2]):
                for ty in range(nt[1]):
                    for tx in range(nt[0]):
                        R = [None] * self.nregs
                        for r, n, t in self.alloca_init:
                            R[r] = [0.0] * n if t == "float" else [0] * n
                        th.append([(tx, ty, tz), R, 0])
            while True:
                atb = done = 0
                for t in th:
                    if t[2] == -1:
                        done += 1
                        continue
                    tx, ty, tz = t[0]
                    X.gid = g
                    X.gtid = t[0]
                    X.tid = (g[0] * nt[0] + tx, g[1] * nt[1] + ty, g[2] * nt[2] + tz)
                    X.flat = tx + nt[0] * (ty + nt[1] * tz)
                    X.who = X.tid
                    b, steps, R = t[2], 0, t[1]
                    while b >= 0:
                        cov[b] += 1
                        b = F[b](R)
                        steps += 1
                        if steps > max_steps:
                            raise RuntimeError("runaway thread %r" % (X.tid,))
                    if b == -1:
                        t[2] = -1
                        done += 1
                    else:
                        t[2] = -2 - b
                        atb += 1
                if atb == 0:
                    break
                if done:
                    raise RuntimeError("divergent barrier in group %r: %d threads at a barrier, %d returned" % (g, atb, done))
                tr.reset()
        return X, cov
