r"""llir.py - parser for the DXIL LLVM-IR text that `dxc -dumpbin` prints (LLVM 3.7 dialect, one function @main).

    m = llir.parse(open("x.ll").read())
    m.blocks          -> [Block(name, instrs, succs, preds)]   (entry block is named "0")
    m.globals         -> {name: Global(space, elem_ty, count, init)}
    m.cfg_summary()   -> text: per block: #instr, loads, succs

Only what dxc emits for compute shaders is supported (no vectors, no invoke, no switch-with-ranges ...).
Every instruction keeps its raw text, so the interpreter / decompiler can fall back to it in error messages.
"""
import re

VAL_RE = re.compile(r'(%[\w.]+|@"[^"]*"|@[\w.$]+|0x[0-9A-Fa-f]+|-?\d+\.\d+e[+-]\d+|-?\d+|true|false|undef|null|zeroinitializer)$')


class Instr:
    __slots__ = ("dest", "op", "ty", "args", "raw", "extra", "block", "idx")

    def __init__(self, dest, op, ty, args, raw, extra=None):
        self.dest, self.op, self.ty, self.args, self.raw, self.extra = dest, op, ty, args, raw, extra or {}

    def __repr__(self):
        return self.raw.strip()


class Block:
    def __init__(self, name):
        self.name, self.instrs, self.succs, self.preds = name, [], [], []

    def __repr__(self):
        return "<bb %s %d instr -> %s>" % (self.name, len(self.instrs), self.succs)


class Global:
    def __init__(self, name, space, elem_ty, count, init):
        self.name, self.space, self.elem_ty, self.count, self.init = name, space, elem_ty, count, init


def split_top(s, sep=","):
    """split at top-level separators (outside (), [], {}, quotes)."""
    out, depth, cur, q = [], 0, [], False
    for ch in s:
        if ch == '"':
            q = not q
        elif not q and ch in "([{<":
            depth += 1
        elif not q and ch in ")]}>":
            depth -= 1
        if ch == sep and depth == 0 and not q:
            out.append("".join(cur).strip()); cur = []
        else:
            cur.append(ch)
    if "".join(cur).strip():
        out.append("".join(cur).strip())
    return out


def parse_const_float(tok):
    """LLVM float literal -> python float (exact value of the float32)."""
    import struct
    if tok.startswith("0x"):
        d = struct.unpack("<d", struct.pack("<Q", int(tok, 16)))[0]
        return d
    return float(tok)


class Op:
    """typed operand: kind in {'reg','glob','int','float','bool','undef','cexpr'}"""
    __slots__ = ("ty", "kind", "v")

    def __init__(self, ty, kind, v):
        self.ty, self.kind, self.v = ty, kind, v

    def __repr__(self):
        return "%s:%s" % (self.kind, self.v)


def mk_op(ty, val):
    val = val.strip()
    if val.startswith("%"):
        return Op(ty, "reg", val[1:])
    if val.startswith("@"):
        return Op(ty, "glob", val[1:].strip('"'))
    if val in ("true", "false"):
        return Op(ty, "int", 1 if val == "true" else 0)
    if val == "undef":
        return Op(ty, "undef", None)
    if val.startswith("getelementptr"):
        # constant expression: getelementptr inbounds ([N x T], [N x T] addrspace(3)* @g, i32 0, i32 K)
        inner = val[val.index("(") + 1:val.rindex(")")]
        parts = split_top(inner)
        g = parts[1].split()[-1]
        idx = [int(p.split()[-1]) for p in parts[2:]]
        return Op(ty, "cexpr", (g[1:].strip('"'), idx))
    if ty in ("float", "half", "double"):
        return Op(ty, "float", parse_const_float(val))
    if re.fullmatch(r"-?\d+", val):
        return Op(ty, "int", int(val))
    if ty.startswith("i") and val.startswith("0x"):
        return Op(ty, "int", int(val, 16))
    raise ValueError("operand? %r %r" % (ty, val))


def typed(s):
    """'float %3' / '[9 x float]* %16' / 'i32 7' / 'float addrspace(3)* getelementptr (...)' -> Op"""
    s = s.strip()
    m = re.match(r"^(.*?)\s+(getelementptr.*)$", s)
    if m:
        return mk_op(m.group(1).strip(), m.group(2))
    m = VAL_RE.search(s)
    if not m:
        raise ValueError("typed? %r" % s)
    ty = s[:m.start()].strip()
    return mk_op(ty, m.group(1))


class Module:
    def __init__(self):
        self.blocks, self.globals, self.byname, self.defs = [], {}, {}, {}
        self.header = ""

    def cfg_summary(self):
        out = []
        for b in self.blocks:
            nl = sum(1 for i in b.instrs if i.op == "call" and ("bufferLoad" in i.extra.get("fn", "") or "sample" in i.extra.get("fn", "") or "textureLoad" in i.extra.get("fn", "")))
            out.append("%-6s n=%-4d mem=%-3d -> %s" % (b.name, len(b.instrs), nl, ",".join(b.succs)))
        return "\n".join(out)


def parse(text):
    m = Module()
    lines = text.splitlines()
    i = 0
    hdr = []
    while i < len(lines) and not lines[i].startswith("define "):
        l = lines[i]
        if l.startswith(";"):
            hdr.append(l)
        gm = re.match(r'^@("[^"]*"|[\w.$]+) = (.*)$', l)
        if gm:
            name = gm.group(1).strip('"')
            rest = gm.group(2)
            space = 3 if "addrspace(3)" in rest else 0
            am = re.search(r"\[(\d+) x (\w+)\]", rest)
            cnt, ety = int(am.group(1)), am.group(2)
            init = None
            im = re.search(r"\] \[(.*)\]", rest)
            if im and "constant" in rest:
                init = [parse_const_float(x.split()[-1]) if ety == "float" else int(x.split()[-1]) for x in split_top(im.group(1))]
            m.globals[name] = Global(name, space, ety, cnt, init)
        i += 1
    m.header = "\n".join(hdr)
    assert lines[i].startswith("define void @main()"), lines[i]
    i += 1
    cur = Block("0")
    m.blocks.append(cur)
    while i < len(lines):
        l = lines[i]; i += 1
        if l.startswith("}"):
            break
        if not l.strip():
            continue
        lm = re.match(r"^; <label>:(\d+)", l) or re.match(r"^(\w[\w.]*):", l)
        if lm:
            if cur.instrs or cur.name != "0":
                cur = Block(lm.group(1)); m.blocks.append(cur)
            else:
                cur.name = lm.group(1)
            continue
        if l.lstrip().startswith(";"):
            continue
        ins = parse_instr(l)
        ins.block, ins.idx = cur.name, len(cur.instrs)
        cur.instrs.append(ins)
        if ins.dest:
            m.defs[ins.dest] = ins
    for b in m.blocks:
        m.byname[b.name] = b
        t = b.instrs[-1]
        if t.op == "br":
            b.succs = list(t.extra["targets"])
        elif t.op == "switch":
            b.succs = [t.extra["default"]] + [x[1] for x in t.extra["cases"]]
    for b in m.blocks:
        for s in b.succs:
            m.byname[s].preds.append(b.name)
    return m


def strip_meta(s):
    # drop ', !tbaa !12, !noalias !3' and ', align 4'
    s = re.sub(r",\s*![\w.]+ !\d+", "", s)
    s = re.sub(r",\s*align \d+", "", s)
    return s.split(";")[0].rstrip() if ";" in s and '"' not in s.split(";")[0][-1:] else s.rstrip()


def parse_instr(line):
    raw = line
    s = line.strip()
    # remove trailing comment (dxc prints '  ; CBufferLoadLegacy(...)')
    if "  ; " in s:
        s = s[:s.index("  ; ")]
    s = re.sub(r",\s*![\w.]+ !\d+", "", s)
    s = re.sub(r",\s*align \d+", "", s)
    dest = None
    dm = re.match(r"^%([\w.]+) = (.*)$", s)
    if dm:
        dest, s = dm.group(1), dm.group(2)
    op = s.split()[0]
    rest = s[len(op):].strip()
    if op == "br":
        if rest.startswith("label"):
            return Instr(None, "br", None, [], raw, {"targets": [rest.split("%")[1]]})
        parts = split_top(rest)
        c = typed(parts[0])
        return Instr(None, "br", None, [c], raw, {"targets": [parts[1].split("%")[1], parts[2].split("%")[1]]})
    if op == "ret":
        return Instr(None, "ret", None, [], raw)
    if op == "switch":
        mm = re.match(r"^(\w+) (%[\w.]+), label %(\w+) \[(.*)\]$", rest)
        cases = re.findall(r"\w+ (-?\d+), label %(\w+)", mm.group(4))
        return Instr(None, "switch", None, [typed(mm.group(1) + " " + mm.group(2))], raw,
                     {"default": mm.group(3), "cases": [(int(a), b) for a, b in cases]})
    if op in ("fadd", "fsub", "fmul", "fdiv", "frem", "add", "sub", "mul", "and", "or", "xor", "shl", "lshr", "ashr",
              "udiv", "sdiv", "urem", "srem"):
        toks = rest.split()
        flags = []
        while toks[0] in ("fast", "nnan", "ninf", "nsz", "arcp", "nuw", "nsw", "exact"):
            flags.append(toks.pop(0))
        rest2 = " ".join(toks)
        a, b = split_top(rest2)
        ty = a.split()[0]
        return Instr(dest, op, ty, [typed(a), typed(ty + " " + b)], raw, {"flags": flags})
    if op in ("fcmp", "icmp"):
        toks = rest.split()
        flags = []
        while toks[0] in ("fast", "nnan", "ninf", "nsz", "arcp"):
            flags.append(toks.pop(0))
        pred = toks.pop(0)
        rest2 = " ".join(toks)
        a, b = split_top(rest2)
        ty = a.split()[0]
        return Instr(dest, op, "i1", [typed(a), typed(ty + " " + b)], raw, {"pred": pred, "flags": flags})
    if op == "select":
        c, a, b = split_top(rest)
        return Instr(dest, op, a.split()[0], [typed(c), typed(a), typed(b)], raw)
    if op == "phi":
        ty = rest.split()[0]
        inc = re.findall(r"\[\s*([^,\]]+),\s*%(\w+)\s*\]", rest[len(ty):])
        return Instr(dest, op, ty, [mk_op(ty, v) for v, _ in inc], raw, {"from": [b for _, b in inc]})
    if op in ("sitofp", "uitofp", "fptosi", "fptoui", "bitcast", "zext", "sext", "trunc", "fpext", "fptrunc",
              "ptrtoint", "inttoptr", "addrspacecast"):
        mm = re.match(r"^(.*) to (.*)$", rest)
        return Instr(dest, op, mm.group(2).strip(), [typed(mm.group(1))], raw)
    if op == "extractvalue":
        parts = split_top(rest)
        return Instr(dest, op, None, [typed(parts[0])], raw, {"index": [int(x) for x in parts[1:]]})
    if op == "alloca":
        am = re.match(r"^\[(\d+) x (\w+)\]", rest)
        if am:
            return Instr(dest, op, am.group(2), [], raw, {"count": int(am.group(1))})
        return Instr(dest, op, rest.split(",")[0], [], raw, {"count": 1})
    if op == "getelementptr":
        r = rest
        if r.startswith("inbounds "):
            r = r[9:]
        parts = split_top(r)
        return Instr(dest, op, None, [typed(p) for p in parts[1:]], raw, {"srcty": parts[0]})
    if op == "load":
        parts = split_top(rest)
        return Instr(dest, op, parts[0], [typed(parts[1])], raw)
    if op == "store":
        parts = split_top(rest)
        return Instr(None, op, None, [typed(parts[0]), typed(parts[1])], raw)
    if op == "call":
        mm = re.match(r"^(.*?) @([\w.]+)\((.*)\)\s*(#\d+)?$", rest)
        rty, fn, args = mm.group(1).strip(), mm.group(2), mm.group(3)
        ops = [typed(a) for a in split_top(args)]
        return Instr(dest, op, rty, ops, raw, {"fn": fn, "opcode": ops[0].v if ops and ops[0].kind == "int" else None})
    raise ValueError("unsupported instruction: " + line)


def loops(m):
    """natural loops: {header: set(blocks)} via back edges (target dominates source)."""
    dom = dominators(m)
    res = {}
    for b in m.blocks:
        for s in b.succs:
            if s in dom[b.name]:
                body = {s, b.name}
                stack = [b.name]
                while stack:
                    x = stack.pop()
                    if x == s:
                        continue
                    for p in m.byname[x].preds:
                        if p not in body:
                            body.add(p); stack.append(p)
                res.setdefault(s, set()).update(body)
    return res


def dominators(m):
    names = [b.name for b in m.blocks]
    order = rpo(m)
    idom = {order[0]: order[0]}
    pos = {n: i for i, n in enumerate(order)}

    def inter(a, b):
        while a != b:
            while pos[a] > pos[b]:
                a = idom[a]
            while pos[b] > pos[a]:
                b = idom[b]
        return a
    changed = True
    while changed:
        changed = False
        for n in order[1:]:
            ps = [p for p in m.byname[n].preds if p in idom]
            new = ps[0]
            for p in ps[1:]:
                new = inter(p, new)
            if idom.get(n) != new:
                idom[n] = new; changed = True
    dom = {}
    for n in order:
        s, x = {n}, n
        while idom[x] != x:
            x = idom[x]; s.add(x)
        dom[n] = s
    m.idom = idom
    return dom


def rpo(m):
    seen, out = set(), []

    def dfs(n):
        stack = [(n, iter(m.byname[n].succs))]
        seen.add(n)
        while stack:
            x, it = stack[-1]
            for s in it:
                if s not in seen:
                    seen.add(s); stack.append((s, iter(m.byname[s].succs))); break
            else:
                stack.pop(); out.append(x)
    dfs(m.blocks[0].name)
    return out[::-1]


if __name__ == "__main__":
    import sys
    mod = parse(open(sys.argv[1]).read())
    print(len(mod.blocks), "blocks", sum(len(b.instrs) for b in mod.blocks), "instrs", len(mod.globals), "globals")
    L = loops(mod)
    for h, body in sorted(L.items(), key=lambda kv: int(kv[0]) if kv[0].isdigit() else 0):
        print("loop header", h, "blocks", len(body))
    if "-v" in sys.argv:
        print(mod.cfg_summary())
