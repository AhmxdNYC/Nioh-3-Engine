r"""dxbc2hlsl.py -- mechanical SM5 pixel shader -> HLSL translation (one statement per DXBC instruction, temps as
uint4, exact constants as hex bits), so a shipped runtime shader can be recompiled with fxc and then edited locally
(e.g. a loop batched) while everything else stays the original's op sequence. The result must still pass
verify_post.py (bit-exact vs the shipped blob); fxc may re-associate, which the interpreter catches.

Usage: py -I dxbc2hlsl.py <digest-prefix> out.hlsl
Supports the ops of the post shaders (float/int ALU, movc, dp2-4, sincos, sample_l, ld(+aoffimmi), resinfo, if/else/
loop/break/ret, icb). Constant buffers must consist of 16-byte-aligned float4 / float4[n] / float4x4 members.
"""
import os, re, sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import build_post as B  # noqa: E402
import dxbc_interp as DI  # noqa: E402

C = "xyzw"


def header(text):
    cbs, res, isg, osg, cur = {}, [], [], [], None
    sect = None
    for l in text.splitlines():
        m = re.match(r"^// cbuffer (\w+)", l)
        if m:
            cur = m.group(1); cbs[cur] = []; continue
        m = re.match(r"^//\s+(row_major\s+)?(\w+)\s+(\w+)(\[(\d+)\])?;\s*// Offset:\s+(\d+) Size:\s+(\d+)", l)
        if m and cur:
            cbs[cur].append((m.group(2), m.group(3), int(m.group(5) or 0), int(m.group(6)), int(m.group(7))))
            continue
        m = re.match(r"^//\s+(\w+)\s+(sampler|texture|cbuffer)\s+(\S+)\s+(\S+)\s+(s|t|cb|u)(\d+)\s+(\d+)", l)
        if m:
            res.append(m.groups()); continue
        if "Input signature" in l:
            sect = isg
        elif "Output signature" in l:
            sect = osg
        m = re.match(r"^//\s+(\w+)\s+(\d+)\s+([xyzw ]{4})\s+(\d+)\s+(\w+)\s+(\w+)", l)
        if m and sect is not None and m.group(1) != "Name":
            sect.append((m.group(1), int(m.group(2)), m.group(3), int(m.group(4)), m.group(5), m.group(6)))
    return cbs, res, isg, osg


class T:
    def __init__(self, text):
        self.p = DI.Program(text)
        self.cbs, self.res, self.isg, self.osg = header(text)
        self.cbreg = {}  # (slot, reg) -> expr
        self.lines = []

    def decls(self):
        out = []
        slots = {}
        for name, typ, fmt, dim, kind, n, cnt in self.res:
            if typ == "cbuffer":
                slots[name] = int(n)
        for cbn, vars_ in self.cbs.items():
            slot = slots[cbn]
            out.append("cbuffer %s : register(b%d)\n{" % (cbn, slot))
            for typ, name, arr, off, size in vars_:
                if off % 16:
                    raise SystemExit("unaligned cbuffer member %s" % name)
                nreg = (size + 15) // 16
                if typ == "float4" and not arr:
                    out.append("    float4 %s;" % name)
                    self.cbreg[(slot, off // 16)] = name
                else:  # float4[n], float4x4 -> float4 array with the same registers
                    if typ not in ("float4", "float4x4", "float4x3", "float3x4"):
                        raise SystemExit("cbuffer member type %s %s" % (typ, name))
                    out.append("    float4 %s[%d];" % (name, nreg))
                    for k in range(nreg):
                        self.cbreg[(slot, off // 16 + k)] = "%s[%d]" % (name, k)
            out.append("};")
        for name, typ, fmt, dim, kind, n, cnt in self.res:
            if typ == "sampler":
                out.append("SamplerState %s : register(s%s);" % (name, n))
                self.cbreg[("s", int(n))] = name
            elif typ == "texture":
                tt = {"2d": "Texture2D", "3d": "Texture3D", "2darray": "Texture2DArray"}[dim]
                ft = {"float4": "float4", "float": "float", "float2": "float2", "float3": "float3"}.get(fmt, "float4")
                out.append("%s<%s> %s : register(t%s);" % (tt, ft, name, n))
                self.cbreg[("t", int(n))] = (name, dim, ft)
        if self.p.icb:
            out.append("static const uint4 icb[%d] = {" % len(self.p.icb))
            out.append(",\n".join("    {0x%08x, 0x%08x, 0x%08x, 0x%08x}" % tuple((v + [0] * 4)[:4]) for v in self.p.icb))
            out.append("};")
        return out

    # ---- operands
    def idx(self, ie):
        reg, i, comp, c = DI.parse_index(ie)
        if reg is None:
            return str(c)
        return "asint(r%d.%s) + %d" % (i, C[comp], c)

    def src(self, o, comps, kind="u"):
        """operand -> expression of type uint<n> (kind u), float<n> (f) or int<n> (i) for the given dest comps."""
        sw = "".join(C[o.swz[c]] for c in comps)
        n = len(comps)
        if o.kind == "l":
            vals = [o.lit[o.swz[c]] for c in comps]
            if kind == "f":
                e = ", ".join("asfloat(0x%08x)" % v for v in vals)
                e = ("float%d(%s)" % (n, e)) if n > 1 else e
            elif kind == "i":
                e = ", ".join("%d" % (v - (1 << 32) if v & 0x80000000 else v) for v in vals)
                e = ("int%d(%s)" % (n, e)) if n > 1 else e
            else:
                e = ", ".join("0x%08xu" % v for v in vals)
                e = ("uint%d(%s)" % (n, e)) if n > 1 else e
        else:
            if o.kind == "r":
                base = "r%d" % o.idx
            elif o.kind == "v":
                base = "v%d" % o.idx
            elif o.kind == "cb":
                slot, ie = o.idx
                reg, i, comp, c = DI.parse_index(ie)
                if reg is not None:
                    raise SystemExit("dynamic cb index")
                base = "asuint(%s)" % self.cbreg[(slot, c)]
            elif o.kind == "icb":
                base = "icb[%s]" % self.idx(o.idx[1])
            else:
                raise SystemExit("operand kind %s" % o.kind)
            e = "%s.%s" % (base, sw)
            if kind == "f":
                e = "asfloat(%s)" % e
            elif kind == "i":
                e = "asint(%s)" % e
        if kind == "f":
            if o.abs:
                e = "abs(%s)" % e
            if o.neg:
                e = "(-%s)" % e
        elif o.neg or o.abs:
            if kind == "u":  # float modifiers on a raw move (movc): apply them as float ops
                f = "asfloat(%s)" % e
                if o.abs:
                    f = "abs(%s)" % f
                if o.neg:
                    f = "(-%s)" % f
                return "asuint(%s)" % f
            if o.abs:
                e = "abs(%s)" % e
            if o.neg:
                e = "(-%s)" % e
        return e

    def dst(self, o):
        m = "".join(C[c] for c in o.mask)
        base = "r%d" % o.idx if o.kind == "r" else "o%d" % o.idx
        return "%s.%s" % (base, m), o.mask

    def emit(self, s, depth):
        self.lines.append("    " * depth + s)

    FBIN = {"add": "(%s + %s)", "mul": "(%s * %s)", "div": "(%s / %s)", "min": "min(%s, %s)", "max": "max(%s, %s)"}
    FUN = {"sqrt": "sqrt(%s)", "rsq": "rsqrt(%s)", "rcp": "rcp(%s)", "exp": "exp2(%s)", "log": "log2(%s)",
           "frc": "frac(%s)", "round_ni": "floor(%s)", "round_pi": "ceil(%s)", "round_z": "trunc(%s)",
           "round_ne": "round(%s)", "mov": "%s"}
    FCMP = {"lt": "<", "ge": ">=", "eq": "==", "ne": "!="}
    IBIN = {"iadd": ("i", "(%s + %s)"), "and": ("u", "(%s & %s)"), "or": ("u", "(%s | %s)"), "xor": ("u", "(%s ^ %s)"),
            "ishl": ("u", "(%s << %s)"), "ushr": ("u", "(%s >> %s)"), "ishr": ("i", "(%s >> %s)"),
            "imax": ("i", "max(%s, %s)"), "imin": ("i", "min(%s, %s)"), "umax": ("u", "max(%s, %s)"),
            "umin": ("u", "min(%s, %s)"), "imul": ("i", "(%s * %s)")}
    ICMP = {"ige": ("i", ">="), "ilt": ("i", "<"), "ieq": ("i", "=="), "ine": ("i", "!="), "uge": ("u", ">="), "ult": ("u", "<")}

    def tr(self):
        d = 1
        for ins in self.p.ins:
            op, ops = ins.op, ins.ops
            if op in ("if_nz", "if_z"):
                e = self.src(ops[0], [0])
                self.emit("if (%s %s 0u)" % (e, "!=" if op == "if_nz" else "=="), d); self.emit("{", d); d += 1
            elif op == "else":
                d -= 1; self.emit("}", d); self.emit("else", d); self.emit("{", d); d += 1
            elif op in ("endif", "endloop"):
                d -= 1; self.emit("}", d)
            elif op == "loop":
                self.emit("[loop] while (true)", d); self.emit("{", d); d += 1
            elif op in ("breakc_nz", "breakc_z"):
                self.emit("if (%s %s 0u) break;" % (self.src(ops[0], [0]), "!=" if op == "breakc_nz" else "=="), d)
            elif op == "break":
                self.emit("break;", d)
            elif op == "ret":
                self.emit("return %s;" % self.retexpr(), d)
            elif op in ("retc_nz", "retc_z"):
                self.emit("if (%s %s 0u) return %s;" % (self.src(ops[0], [0]), "!=" if op == "retc_nz" else "==",
                                                         self.retexpr()), d)
            elif op in ("discard_nz", "discard_z"):
                self.emit("if (%s %s 0u) discard;" % (self.src(ops[0], [0]), "!=" if op == "discard_nz" else "=="), d)
            else:
                self.emit(self.stmt(ins), d)
        return self.lines

    def retexpr(self):
        if len(self.osg) == 1:
            return "asfloat(o0)"
        return "Out(%s)" % ", ".join("asfloat(o%d)" % s[3] for s in self.osg)

    def sat(self, ins, e):
        return "saturate(%s)" % e if ins.sat else e

    def stmt(self, ins):
        op, ops = ins.op, ins.ops
        if op == "sincos":
            a = []
            for k, fn in ((0, "sin"), (1, "cos")):
                if ops[k].kind == "null":
                    continue
                dd, m = self.dst(ops[k])
                a.append("%s = asuint(%s(%s));" % (dd, fn, self.src(ops[2], m, "f")))
            return " ".join(a)
        dd, m = self.dst(ops[0])
        n = len(m)
        if op in self.FBIN:
            e = self.FBIN[op] % (self.src(ops[1], m, "f"), self.src(ops[2], m, "f"))
            return "%s = asuint(%s);" % (dd, self.sat(ins, e))
        if op == "mad":
            e = "mad(%s, %s, %s)" % tuple(self.src(o, m, "f") for o in ops[1:4])
            return "%s = asuint(%s);" % (dd, self.sat(ins, e))
        if op in self.FUN:
            if op == "mov" and not ins.sat and not (ops[1].neg or ops[1].abs):
                return "%s = %s;" % (dd, self.src(ops[1], m, "u"))
            e = self.FUN[op] % self.src(ops[1], m, "f")
            return "%s = asuint(%s);" % (dd, self.sat(ins, e))
        if op in self.FCMP:
            return "%s = (%s %s %s) ? 0xffffffffu : 0u;" % (dd, self.src(ops[1], m, "f"), self.FCMP[op],
                                                         self.src(ops[2], m, "f")) if n == 1 else \
                "%s = select4(%s %s %s, %d);" % (dd, self.src(ops[1], m, "f"), self.FCMP[op], self.src(ops[2], m, "f"), n)
        if op in self.IBIN:
            k, f = self.IBIN[op]
            e = f % (self.src(ops[1], m, k), self.src(ops[2], m, k))
            return "%s = asuint(%s);" % (dd, e) if k == "i" else "%s = %s;" % (dd, e)
        if op in self.ICMP:
            k, c = self.ICMP[op]
            e = "%s %s %s" % (self.src(ops[1], m, k), c, self.src(ops[2], m, k))
            return "%s = (%s) ? 0xffffffffu : 0u;" % (dd, e) if n == 1 else "%s = select4(%s, %d);" % (dd, e, n)
        if op == "ineg":
            return "%s = asuint(-%s);" % (dd, self.src(ops[1], m, "i"))
        if op == "not":
            return "%s = ~%s;" % (dd, self.src(ops[1], m, "u"))
        if op == "ubfe":
            if ops[1].kind != "l" or ops[2].kind != "l":
                raise SystemExit("ubfe with register width/offset")
            w, o_ = ops[1].lit[ops[1].swz[m[0]]] & 31, ops[2].lit[ops[2].swz[m[0]]] & 31
            if any((ops[1].lit[ops[1].swz[c]] & 31, ops[2].lit[ops[2].swz[c]] & 31) != (w, o_) for c in m):
                raise SystemExit("ubfe per-component widths")
            e = "((%s >> %du) & 0x%xu)" % (self.src(ops[3], m), o_, (1 << w) - 1) if w + o_ < 32 else "(%s >> %du)" % (self.src(ops[3], m), o_)
            return "%s = %s;" % (dd, e)
        if op == "movc":
            if n == 1:
                return "%s = (%s != 0u) ? %s : %s;" % (dd, self.src(ops[1], m), self.src(ops[2], m), self.src(ops[3], m))
            return "%s = movc%d(%s, %s, %s);" % (dd, n, self.src(ops[1], m), self.src(ops[2], m), self.src(ops[3], m))
        if op in ("dp2", "dp3", "dp4"):
            k = int(op[2])
            e = "dot(%s, %s)" % (self.src(ops[1], list(range(k)), "f"), self.src(ops[2], list(range(k)), "f"))
            return "%s = asuint((%s)%s);" % (dd, self.sat(ins, e), ".xxxx"[: n + 1] if n > 1 else "")
        if op in ("ftoi", "ftou", "itof", "utof"):
            if op == "ftoi":
                e = "asuint((int%s)%s)" % (n if n > 1 else "", self.src(ops[1], m, "f"))
            elif op == "ftou":
                e = "(uint%s)%s" % (n if n > 1 else "", self.src(ops[1], m, "f"))
            elif op == "itof":
                e = "asuint((float%s)%s)" % (n if n > 1 else "", self.src(ops[1], m, "i"))
            else:
                e = "asuint((float%s)%s)" % (n if n > 1 else "", self.src(ops[1], m, "u"))
            return "%s = %s;" % (dd, e)
        if op in ("sample_l", "ld", "resinfo"):
            name, dim, ft = self.cbreg[("t", ops[2].idx)]
            nc = {"2d": 2, "3d": 3, "2darray": 3}[dim]
            rsw = "".join(C[ops[2].swz[c]] for c in m)
            if op == "sample_l":
                smp = self.cbreg[("s", ops[3].idx)]
                e = "%s.SampleLevel(%s, %s, %s)" % (name, smp, self.src(ops[1], list(range(nc)), "f"),
                                                     self.src(ops[4], [0], "f"))
            elif op == "ld":
                e = "%s.Load(%s%s)" % (name, "int%d(%s)" % (nc + 1, self.src(ops[1], list(range(nc + 1)), "i")),
                                       (", int%d(%s)" % (nc, ", ".join(str(x) for x in ins.aoff[:nc]))) if any(ins.aoff) else "")
            else:
                return "{ float w_, h_, l_; %s.GetDimensions(0, w_, h_, l_); %s = asuint(float4(w_, h_, 1, l_).%s); }" % (
                    name, dd, rsw)
            if ft != "float4":
                e = "float4(%s, 0, 0, 0)" % e if ft == "float" else e
            return "%s = asuint((%s).%s);" % (dd, e, rsw)
        raise SystemExit("op %s not supported: %s" % (op, ins.text))

    def program(self):
        out = ["// mechanical translation of the shipped DXBC (dxbc2hlsl.py); edit only where marked", ""]
        out += self.decls()
        out.append("uint4 movc2(uint2 c, uint2 a, uint2 b) { return uint4(c.x ? a.x : b.x, c.y ? a.y : b.y, 0, 0); }")
        out.append("uint2 movc2_(uint2 c, uint2 a, uint2 b) { return uint2(c.x ? a.x : b.x, c.y ? a.y : b.y); }")
        out = [l for l in out if not l.startswith("uint4 movc2(")]
        out.append("uint3 movc3(uint3 c, uint3 a, uint3 b) { return uint3(c.x ? a.x : b.x, c.y ? a.y : b.y, c.z ? a.z : b.z); }")
        out.append("uint4 movc4(uint4 c, uint4 a, uint4 b) { return uint4(c.x ? a.x : b.x, c.y ? a.y : b.y, c.z ? a.z : b.z, c.w ? a.w : b.w); }")
        out.append("uint2 select4(bool2 c, int n) { return c ? 0xffffffffu : 0u; }")
        out.append("uint3 select4(bool3 c, int n) { return c ? 0xffffffffu : 0u; }")
        out.append("uint4 select4(bool4 c, int n) { return c ? 0xffffffffu : 0u; }")
        # signature
        params, init = [], []
        regs = {}
        for name, idx, mask, reg, sv, fmt in self.isg:
            comps = [C.index(c) for c in mask if c != " "]
            pn = "in_%s%d" % (name.lower(), idx)
            ty = {"float": "float", "uint": "uint", "int": "int"}[fmt]
            params.append("%s%s %s : %s%s" % (ty, len(comps) if len(comps) > 1 else "", pn, name,
                                             idx if not name.startswith("SV_") or idx else ""))
            for k, c in enumerate(comps):
                regs.setdefault(reg, {})[c] = "%s%s" % (pn, ("." + C[k]) if len(comps) > 1 else "")
        for reg, cm in sorted(regs.items()):
            init.append("    uint4 v%d = asuint(float4(%s));" % (reg, ", ".join(cm.get(c, "0") for c in range(4))))
        if len(self.osg) != 1:
            raise SystemExit("multiple outputs not supported")
        out.append("float4 main(%s) : SV_Target" % ", ".join(params))
        out.append("{")
        out += init
        out.append("    uint4 %s;" % ", ".join("r%d = 0" % i for i in range(self.p.ntemps)) if self.p.ntemps else "")
        out.append("    uint4 o0 = 0;")
        body = self.tr()
        body = [l.replace("movc2(", "movc2_(") for l in body]
        out += body
        if not body[-1].strip().startswith("return"):
            out.append("    return asfloat(o0);")
        out.append("}")
        return "\n".join(out) + "\n"


def main():
    text = DI.disassemble(open(B.find_original(sys.argv[1]), "rb").read())
    open(sys.argv[2], "w").write(T(text).program())


if __name__ == "__main__":
    main()
