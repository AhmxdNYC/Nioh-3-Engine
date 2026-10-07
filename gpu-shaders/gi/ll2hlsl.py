r"""ll2hlsl.py - mechanical decompiler: DXIL (dxc -dumpbin text) -> compilable HLSL with the same interface.

    py -I ll2hlsl.py in.ll out.hlsl

* Interface: resources, samplers, cbuffers (same names, members, offsets, registers), structured-buffer element
  structs, groupshared arrays, static const tables and [numthreads] are rebuilt from the dumpbin header and metadata,
  so dxc re-creates the same binding table (PSV0) and reflection.
* Body: every SSA value becomes a local; every DXIL op becomes the HLSL construct that dxc lowers back to that op
  (mad -> FMad, dot(float3) -> Dot3, floor -> Round_ni, x.Load -> bufferLoad, ...).
* Control flow: Ramsey's "Beyond Relooper" structuring (dominator tree + RPO) into loop / block / if, then lowered to
  HLSL for(;;) / do-while(false) / if with break/continue chosen by continuation analysis; a jump no native
  break/continue can express goes through a small `_j` dispatch variable.

The output is the starting point for hand optimisation; `verify.py` proves (by interpretation of both DXIL programs)
that it computes what the original computes.
"""
import re, struct, sys
import os as _os, sys as _sys
_sys.path.insert(0, _os.path.dirname(_os.path.abspath(__file__)))
import llir

M32 = 0xFFFFFFFF
SW = "xyzw"


# ------------------------------------------------------------------------------------------------ header parsing
class Member:
    def __init__(self, name, ty, count, offset, rowmajor=False, children=None, sname=None):
        self.name, self.ty, self.count, self.offset = name, ty, count, offset
        self.rowmajor, self.children, self.sname = rowmajor, children, sname

    def decl(self, ind="    ", rows_as_vectors=False):
        arr = "[%d]" % self.count if self.count else ""
        if self.children is not None:
            return "%s%s %s%s;" % (ind, self.sname, self.name, arr)
        m = re.match(r"^(float|int|uint)(\d)x(\d)$", self.ty)
        if rows_as_vectors and m and self.rowmajor:
            # structured-buffer element: a row_major RxC matrix is stored exactly like floatC[R]; declared so,
            # m[r] is one 16-byte load like the shipped code (dxc 1.8 splits matrix-row reads into scalars)
            return "%s%s%s %s%s[%s];" % (ind, m.group(1), m.group(3), self.name, arr, m.group(2))
        return "%s%s%s %s%s;" % (ind, "row_major " if self.rowmajor else "", self.ty, self.name, arr)


def parse_struct_lines(lines, i):
    """lines[i] is '{'; returns (members, next index after the closing '} name;')"""
    assert lines[i].strip() == "{", lines[i]
    i += 1
    mem = []
    while True:
        l = lines[i].strip()
        if l.startswith("}"):
            return mem, i
        if not l:
            i += 1; continue
        sm = re.match(r"^struct (\S+)$", l)
        if sm:
            sname = sm.group(1)
            ch, j = parse_struct_lines(lines, i + 1)
            em = re.match(r"^\}\s*(\w+)(\[(\d+)\])?;\s*; Offset:\s*(\d+)", lines[j].strip())
            mem.append(Member(em.group(1), "struct", int(em.group(3)) if em.group(3) else 0, int(em.group(4)),
                              children=ch, sname=clean_sname(sname)))
            i = j + 1
            continue
        mm = re.match(r"^(row_major |column_major )?(\w+) (\w+)(\[(\d+)\])?;\s*; Offset:\s*(\d+)", l)
        if not mm:
            raise ValueError("member? " + l)
        mem.append(Member(mm.group(3), mm.group(2), int(mm.group(5)) if mm.group(5) else 0, int(mm.group(6)),
                          rowmajor=(mm.group(1) or "").strip() == "row_major"))
        i += 1


def clean_sname(s):
    s = s.replace("dx.alignment.legacy.", "")
    if s.startswith("struct."):
        s = s[7:]
    return s


TY_SIZE = {"float": 4, "int": 4, "uint": 4, "bool": 4}


def ty_info(ty):
    """('float', 3) for float3; ('float', (4,4)) for float4x4"""
    m = re.match(r"^(float|int|uint|bool|half|double)(\d)?(x(\d))?$", ty)
    base = m.group(1)
    if m.group(3):
        return base, (int(m.group(2)), int(m.group(4)))
    return base, int(m.group(2) or 1)


def cb_leaf_map(members, base_off=0, path=""):
    """legacy cbuffer layout -> {(reg, comp): (expr, basetype)} for static member reads."""
    out = {}
    for m in members:
        n = m.count or 1
        for e in range(n):
            p = path + m.name + ("[%d]" % e if m.count else "")
            if m.children is not None:
                # struct elements in arrays start on a 16-byte boundary; size: round up
                ssize = struct_size_legacy(m.children)
                off = base_off + m.offset + e * ((ssize + 15) // 16 * 16)
                out.update(cb_leaf_map(m.children, off, p + "."))
                continue
            base, dims = ty_info(m.ty)
            if isinstance(dims, tuple):
                rows, cols = dims
                if not m.rowmajor:
                    raise NotImplementedError("column_major matrix " + m.name)
                stride = 16 * rows
                off = base_off + m.offset + e * stride
                for r in range(rows):
                    for c in range(cols):
                        out[((off + 16 * r) // 16, c)] = ("%s[%d].%s" % (p, r, SW[c]), base)
            else:
                stride = 16 if m.count else 0
                off = base_off + m.offset + e * stride
                reg, comp = off // 16, (off % 16) // 4
                for c in range(dims):
                    out[(reg, comp + c)] = (p + ("." + SW[c] if dims > 1 else ""), base)
    return out


def struct_size_legacy(members):
    end = 0
    for m in members:
        if m.children is not None:
            s = struct_size_legacy(m.children)
            n = m.count or 1
            end = max(end, m.offset + (n - 1) * ((s + 15) // 16 * 16) + s)
        else:
            base, dims = ty_info(m.ty)
            sz = 4 * (dims[0] * dims[1] if isinstance(dims, tuple) else dims)
            n = m.count or 1
            end = max(end, m.offset + (n - 1) * 16 + sz)
    return end


def sb_leaf_map(members, base_off=0, path=""):
    """structured-buffer (tight) layout -> {byte offset: (expr, base, ncomps)} for vector/scalar/row starts."""
    out = {}
    for m in members:
        n = m.count or 1
        if m.children is not None:
            ch = rel_children(m)
            s = sb_struct_size(ch)
            for e in range(n):
                out.update(sb_leaf_map(ch, base_off + m.offset + e * s, path + m.name + ("[%d]" % e if m.count else "") + "."))
            continue
        base, dims = ty_info(m.ty)
        if isinstance(dims, tuple):
            rows, cols = dims
            esz = 4 * rows * cols
            for e in range(n):
                p = path + m.name + ("[%d]" % e if m.count else "")
                for r in range(rows):
                    out[base_off + m.offset + e * esz + 4 * cols * r] = ("%s[%d]" % (p, r), base, cols)
        else:
            esz = 4 * dims
            for e in range(n):
                p = path + m.name + ("[%d]" % e if m.count else "")
                out[base_off + m.offset + e * esz] = (p, base, dims)
    return out


def rel_children(m):
    """dxc's reflection prints nested struct members with ABSOLUTE offsets (vDir at 48 inside sLightInfo at 48);
    return copies with offsets relative to the parent."""
    import copy
    ch = [copy.copy(c) for c in m.children]
    if ch and ch[0].offset >= m.offset and m.offset > 0:
        for c in ch:
            c.offset -= m.offset
    return ch


def sb_struct_size(members):
    end = 0
    for m in members:
        n = m.count or 1
        if m.children is not None:
            end = max(end, m.offset + n * sb_struct_size(rel_children(m)))
        else:
            base, dims = ty_info(m.ty)
            sz = 4 * (dims[0] * dims[1] if isinstance(dims, tuple) else dims)
            end = max(end, m.offset + n * sz)
    return end


def collect_structs(members, acc):
    for m in members:
        if m.children is not None:
            collect_structs(m.children, acc)
            if m.sname not in acc:
                acc[m.sname] = m.children
    return acc


class Iface:
    def __init__(self, text):
        hdr = [l[1:] if l.startswith(";") else l for l in text.splitlines() if l.startswith(";")]
        self.cbuffers = []      # (name, members, size)
        self.sbinfo = {}        # resource name -> (struct name, members, size)
        i = 0
        while i < len(hdr):
            l = hdr[i].strip()
            cm = re.match(r"^cbuffer (\S+)$", l)
            rm = re.match(r"^Resource bind info for (\S+)$", l)
            if cm or rm:
                j = i + 1
                while hdr[j].strip() != "{":
                    j += 1
                j += 1
                while not hdr[j].strip().startswith("struct "):
                    j += 1
                sname = clean_sname(hdr[j].strip().split()[1])
                mem, k = parse_struct_lines(hdr, j + 1)
                sm = re.search(r"Size:\s*(\d+)", hdr[k])
                size = int(sm.group(1))
                if cm:
                    self.cbuffers.append((cm.group(1), mem, size))
                else:
                    self.sbinfo[rm.group(1)] = (sname, mem, size)
                i = k + 1
                continue
            i += 1
        # binding table
        self.res = []   # dict(name, kind, fmt, dim, id, bind)
        intab = False
        for l in hdr:
            s = l.strip()
            if s.startswith("Name ") and "HLSL Bind" in s:
                intab = True; continue
            if intab:
                if s.startswith("---"):
                    continue
                if not s:
                    break
                p = s.split()
                if len(p) < 7:
                    break
                self.res.append(dict(name=p[0], kind=p[1], fmt=p[2], dim=p[3], id=p[4], bind=p[5]))
        # metadata class types: dx.resources lists
        md = {}
        for l in text.splitlines():
            mm = re.match(r"^!(\d+) = (.*)$", l)
            if mm:
                md[mm.group(1)] = mm.group(2)
        self.cls = {}   # (class, rangeid) -> llvm type string
        rm_ = re.search(r"!dx\.resources = !\{!(\d+)\}", text)
        if rm_:
            lists = re.findall(r"(!\d+|null)", md[rm_.group(1)])
            for ci, ref in enumerate(lists):
                if ref == "null":
                    continue
                for ent in re.findall(r"!(\d+)", md[ref[1:]]):
                    e = md[ent]
                    em = re.match(r'^!\{i32 (\d+), (%?"?[^*]+"?)\* undef', e)
                    self.cls[(ci, int(em.group(1)))] = em.group(2).strip('%').strip('"')
        cls_of = {"texture": 0, "UAV": 1, "cbuffer": 2, "sampler": 3}
        self.byreg = {}
        for r in self.res:
            c = cls_of[r["kind"]]
            rid = int(re.sub(r"\D", "", r["id"]))
            reg = int(re.sub(r"\D", "", r["bind"]))
            r["cls"], r["rid"], r["reg"] = c, rid, reg
            r["ltype"] = self.cls.get((c, rid))
            self.byreg[(c, reg)] = r

    def hlsl_type(self, r):
        t = r["ltype"] or ""
        t = t.replace("dx.alignment.legacy.", "")
        if r["cls"] == 3:
            return "SamplerComparisonState" if "Comparison" in t else "SamplerState"
        m = re.match(r"^class\.(\w+)<(.*)>$", t)
        if not m:
            raise ValueError("resource type? %r" % t)
        k, el = m.group(1), m.group(2).strip()
        return "%s<%s>" % (k, conv_el(el))

    def elem_type(self, r):
        t = (r["ltype"] or "").replace("dx.alignment.legacy.", "")
        m = re.match(r"^class\.(\w+)<(.*)>$", t)
        return conv_el(m.group(2).strip())

    def decls(self):
        out = []
        structs = {}
        for name, (sname, mem, size) in self.sbinfo.items():
            collect_structs(mem, structs)
            structs.setdefault(sname, mem)
        for sname, mem in structs.items():
            out.append("struct %s\n{\n%s\n};" % (sname, "\n".join(m.decl(rows_as_vectors=True) for m in mem)))
        for name, mem, size in self.cbuffers:
            body = "\n".join(m.decl() for m in mem)
            if name == "$Globals":
                out.append("// $Globals (b0), %d bytes\n%s" % (size, "\n".join(m.decl("") for m in mem)))
            else:
                r = [x for x in self.res if x["name"] == name][0]
                out.append("cbuffer %s : register(b%d)\n{\n%s\n};" % (name, r["reg"], body))
        for r in self.res:
            if r["cls"] == 2:
                continue
            letter = {0: "t", 1: "u", 3: "s"}[r["cls"]]
            out.append("%s %s : register(%s%d);" % (self.hlsl_type(r), r["name"], letter, r["reg"]))
        return "\n".join(out)


def conv_el(el):
    el = el.strip()
    m = re.match(r"^vector<(.*), (\d)\s*>$", el)
    if m:
        return conv_scalar(m.group(1)) + m.group(2)
    if el in ("float", "int", "unsigned int", "uint", "half", "bool"):
        return conv_scalar(el)
    return el   # struct name


def conv_scalar(s):
    return {"float": "float", "int": "int", "unsigned int": "uint", "half": "half", "bool": "bool"}[s.strip()]


# ------------------------------------------------------------------------------------------------ structuring
class Node:
    __slots__ = ("k", "a", "b", "c", "id")

    def __init__(self, k, a=None, b=None, c=None, id=None):
        self.k, self.a, self.b, self.c, self.id = k, a, b, c, id


def seq(items):
    flat = []
    for it in items:
        if it is None:
            continue
        if it.k == "seq":
            flat.extend(it.a)
        else:
            flat.append(it)
    return Node("seq", flat)


class Decompiler:
    def __init__(self, text, name="main", collapse=()):
        """collapse: [(loop header block, trip count), ...] loops to fully unroll + if-convert (see collapse_loop)."""
        self.text = text
        self.m = llir.parse(text)
        self.ifc = Iface(text)
        self.name = name
        self.ren = {}
        self.spec = None
        self.zero_init = False
        self._prep_values()
        for h, n in collapse:
            self.collapse_loop(h, n)
        m = self.m
        self.order = llir.rpo(m)
        self.rpn = {n: i for i, n in enumerate(self.order)}
        llir.dominators(m)
        self.kids = {n: [] for n in self.order}
        for n in self.order[1:]:
            self.kids[m.idom[n]].append(n)
        self.nid = 0
        self.ret_only = {b.name for b in m.blocks if len(b.instrs) == 1 and b.instrs[0].op == "ret"}

    # ---------------- value typing / naming
    def _prep_values(self):
        m = self.m
        self.vty = {}       # ssa -> hlsl type
        self.virt = {}      # ssa -> ('handle', res) | ('cb', cbinfo, regexpr, isint) | ('ptr', base, idx)
        cbm = {}
        for name, mem, size in self.ifc.cbuffers:
            r = [x for x in self.ifc.res if x["name"] == name][0]
            cbm[r["reg"]] = (name, cb_leaf_map(mem))
        self.cbm = cbm
        self.arrays = {}    # alloca ssa -> (name, ty, count)
        for b in m.blocks:
            for ins in b.instrs:
                d = ins.dest
                if not d:
                    continue
                if ins.op == "call":
                    fn = ins.extra["fn"]
                    if fn.startswith("dx.op.createHandle"):
                        cls, reg = ins.args[1].v, ins.args[3].v
                        self.virt[d] = ("handle", self.ifc.byreg[(cls, reg)])
                        continue
                    if fn.startswith("dx.op.cbufferLoadLegacy"):
                        self.virt[d] = ("cb", ins)
                        continue
                    if ins.ty in ("float", "half"):
                        self.vty[d] = "float"
                    elif ins.ty == "i32":
                        self.vty[d] = "uint"
                    elif ins.ty == "i1":
                        self.vty[d] = "bool"
                    elif ins.ty.startswith("%dx.types.ResRet") or ins.ty.startswith("%dx.types.Dimensions"):
                        self.vty[d] = None   # decided at emission
                    else:
                        raise NotImplementedError(ins.raw)
                elif ins.op == "alloca":
                    self.arrays[d] = ("a" + d, "float" if ins.ty == "float" else "uint", ins.extra["count"])
                elif ins.op == "getelementptr":
                    self.virt[d] = ("ptr", ins)
                elif ins.op == "extractvalue":
                    src = ins.args[0]
                    if src.kind == "reg" and src.v in self.virt and self.virt[src.v][0] == "cb":
                        cbins = self.m.defs[src.v]
                        self.vty[d] = "uint" if cbins.extra["fn"].endswith(".i32") else "float"
                    else:
                        sdef = self.m.defs[src.v]
                        if "i32" in sdef.ty.split(".")[-1] or "Dimensions" in sdef.ty:
                            self.vty[d] = "uint"
                        else:
                            self.vty[d] = "float"
                else:
                    self.vty[d] = {"float": "float", "half": "float", "i32": "uint", "i1": "bool"}.get(ins.ty, ins.ty)
                    if ins.op in ("fcmp", "icmp"):
                        self.vty[d] = "bool"
                    if self.vty[d] not in ("float", "uint", "bool"):
                        raise NotImplementedError("type %r in %s" % (ins.ty, ins.raw))

    # ---------------- loop collapse: full unroll + if-conversion (predication) of a constant-trip loop
    def collapse_loop(self, H, trip):
        """Replace the natural loop headed by block H (no inner loops, one latch, trip count `trip` proven from the
        IR: i = phi [0, pre], [i+1, latch]; latch exits on i+1 == trip / continues on i+1 < trip) by ONE straight-line
        block 'U<H>': every iteration and every block of the body is emitted unconditionally with a boolean
        predicate; phis become selects on edge predicates; memory reads are speculated (structured-buffer and
        local-array indices clamped to 0 when the predicate is false, so no read can leave its resource); stores
        and other side effects stay guarded by their predicate. Exit values are selected from the iteration that
        exited. The result computes exactly what the loop computed (same ops on the same values); what changes is
        that no read waits behind a branch, so the GPU compiler can issue all iterations' reads together."""
        m = self.m
        L = llir.loops(m)
        body = L[H]
        for b in body:
            if b != H and b in L and L[b] <= body:
                raise ValueError("loop %s contains loop %s" % (H, b))
        hb = m.byname[H]
        latch = [p for p in hb.preds if p in body]
        pre = [p for p in hb.preds if p not in body]
        if len(latch) != 1 or len(pre) != 1:
            raise ValueError("loop %s: latch %s pre %s" % (H, latch, pre))
        latch, pre = latch[0], pre[0]
        # ---- prove the trip count: a block X that dominates the latch ends with a branch on
        #      (iv + 1) == trip (exit when true) or (iv + 1) < trip (continue when true), where
        #      iv = phi [0, preheader], [iv + 1, latch] lives in the header. So no iteration >= trip can start.
        dom = llir.dominators(m)
        proof = None
        for X in body:
            if X not in dom[latch]:
                continue
            t = m.byname[X].instrs[-1]
            if t.op != "br" or not t.args or t.args[0].kind != "reg" or t.args[0].v not in m.defs:
                continue
            cond = m.defs[t.args[0].v]
            if cond.op != "icmp" or cond.args[0].kind != "reg" or cond.args[1].kind != "int" or cond.args[1].v != trip:
                continue
            inc = m.defs.get(cond.args[0].v)
            if not inc or inc.op != "add" or inc.args[1].kind != "int" or inc.args[1].v != 1 or inc.args[0].kind != "reg":
                continue
            iv = m.defs.get(inc.args[0].v)
            if not iv or iv.op != "phi" or iv.block != H:
                continue
            a0 = iv.args[iv.extra["from"].index(pre)]
            al = iv.args[iv.extra["from"].index(latch)]
            if not (a0.kind == "int" and a0.v == 0 and al.kind == "reg" and al.v == inc.dest):
                continue
            tt, ff = t.extra["targets"]
            if cond.extra["pred"] == "eq" and tt not in body:
                proof = (X, cond.raw.strip())
            elif cond.extra["pred"] in ("slt", "ult") and ff not in body:
                proof = (X, cond.raw.strip())
        if proof is None:
            raise ValueError("loop %s: no trip-count proof for %d" % (H, trip))
        order = [b for b in llir.rpo(m) if b in body]
        assert order[0] == H
        exits = [(b, t) for b in order for t in m.byname[b].succs if t not in body]
        targets = []
        for _, t in exits:
            if t not in targets:
                targets.append(t)
        if len(targets) > 2:
            raise ValueError("loop %s has %d exit targets" % (H, len(targets)))
        bodydefs = {i.dest: i for b in order for i in m.byname[b].instrs
                    if i.dest and not (i.dest in self.virt and self.virt[i.dest][0] in ("cb", "handle"))}
        outside_uses = set()
        for b in m.blocks:
            if b.name in body:
                continue
            for i in b.instrs:
                for k, o in enumerate(i.args):
                    if o.kind == "reg" and o.v in bodydefs:
                        if i.op == "phi" and i.extra["from"][k] in body:
                            continue
                        if o.v in self.virt:          # a pointer: its index must be the exiting iteration's
                            for x in m.defs[o.v].args[1:]:
                                if x.kind == "reg" and x.v in bodydefs:
                                    outside_uses.add(x.v)
                            continue
                        outside_uses.add(o.v)
        tag = "U%s" % H
        lines = ["// ---- loop %s: %d iterations unrolled and if-converted (ll2hlsl collapse_loop)" % (H, trip)]
        newty = {}

        def nm(d, k):
            return "%sk%d_%s" % (tag, k, d)

        def pv(name):          # declare a bool helper var, return printed name
            newty[name] = "bool"
            return "v" + name

        saved = dict(self.ren)
        exit_preds = []        # (k, b, t, printed pred var, renaming)
        prev = None            # renaming of iteration k-1
        alive = "true"
        iter_exit = []
        for k in range(trip):
            cur = {d: nm(d, k) for d in bodydefs}
            hphi = []
            for i in hb.instrs:
                if i.op != "phi":
                    continue
                self.ren = dict(saved)
                if k == 0:
                    val = self.v(i.args[i.extra["from"].index(pre)])
                else:
                    self.ren.update(prev)
                    val = self.v(i.args[i.extra["from"].index(latch)])
                hphi.append(("v" + cur[i.dest], val))
            self.ren = dict(saved)
            self.ren.update(cur)
            lines.append("// iteration %d" % k)
            for d, val in hphi:
                lines.append("%s = %s;" % (d, val))
            P = {}
            edge = {}
            for b in order:
                blk = m.byname[b]
                if b == H:
                    P[b] = alive
                else:
                    ins_ = [p for p in blk.preds if p in body]
                    ps = [edge[(p, b)] for p in ins_]
                    pvn = pv("%sk%d_p%s" % (tag, k, b))
                    lines.append("%s = %s;" % (pvn, " || ".join(ps) if ps else "false"))
                    P[b] = pvn
                    for i in blk.instrs:
                        if i.op != "phi":
                            continue
                        chain = None
                        for p in reversed(ins_):
                            val = self.v(i.args[i.extra["from"].index(p)])
                            chain = val if chain is None else "(%s ? %s : %s)" % (edge[(p, b)], val, chain)
                        lines.append("v%s = %s;" % (cur[i.dest], chain))
                self.spec = None if P[b] == "true" else P[b]
                for i in blk.instrs:
                    if i.op in ("phi", "br", "switch", "ret", "alloca", "getelementptr"):
                        continue
                    st = self.stmt(i)
                    if st:
                        lines.append(st)
                self.spec = None
                t = blk.instrs[-1]
                if t.op == "br" and not t.args:
                    edge[(b, t.extra["targets"][0])] = P[b]
                elif t.op == "br":
                    c = self.v(t.args[0])
                    tt, ff = t.extra["targets"]
                    edge[(b, tt)] = c if P[b] == "true" else "(%s && %s)" % (P[b], c)
                    edge[(b, ff)] = ("!%s" % c) if P[b] == "true" else "(%s && !%s)" % (P[b], c)
                else:
                    raise ValueError("loop %s: terminator %s" % (H, t.raw))
            for d in bodydefs:
                if self.vty.get(d) is not None:
                    newty[cur[d]] = self.vty[d]
            ex_k = []
            for (b, t) in exits:
                e = pv("%sk%d_x%s_%s" % (tag, k, b, t))
                lines.append("%s = %s;" % (e, edge[(b, t)]))
                exit_preds.append((k, b, t, e, dict(cur)))
                ex_k.append(e)
            ek = pv("%sk%d_exit" % (tag, k))
            lines.append("%s = %s;" % (ek, " || ".join(ex_k) if ex_k else "false"))
            iter_exit.append((ek, dict(cur)))
            if k < trip - 1:
                a = pv("%sk%d_alive" % (tag, k + 1))
                lines.append("%s = %s;" % (a, edge[(latch, H)]))
                alive = a
            prev = cur
        # ---- values used after the loop: from the iteration that exited
        final_ren = {}
        for d in sorted(outside_uses, key=lambda x: int(x) if x.isdigit() else 0):
            chain = None
            for ek, ren in reversed(iter_exit):
                chain = ("v" + ren[d]) if chain is None else "(%s ? v%s : %s)" % (ek, ren[d], chain)
            fname = "%s_out_%s" % (tag, d)
            newty[fname] = self.vty[d]
            lines.append("v%s = %s;" % (fname, chain))
            final_ren[d] = fname
        # ---- exit-target phis: incoming edges from the body -> one incoming edge from the collapsed block
        for t in targets:
            tb = m.byname[t]
            for i in tb.instrs:
                if i.op != "phi":
                    continue
                chain = None
                for (k, b, tt, e, ren) in reversed(exit_preds):
                    if tt != t:
                        continue
                    self.ren = dict(saved)
                    self.ren.update(ren)
                    val = self.v(i.args[i.extra["from"].index(b)])
                    chain = val if chain is None else "(%s ? %s : %s)" % (e, val, chain)
                fname = "%s_phi_%s" % (tag, i.dest)
                newty[fname] = self.vty[i.dest]
                lines.append("v%s = %s;" % (fname, chain))
                keep = [(a, f) for a, f in zip(i.args, i.extra["from"]) if f not in body]
                i.args = [a for a, f in keep] + [llir.Op(i.ty, "reg", fname)]
                i.extra["from"] = [f for a, f in keep] + [tag]
        self.ren = dict(saved)
        self.ren.update(final_ren)
        self.vty.update(newty)
        # ---- splice the collapsed block into the CFG
        U = llir.Block(tag)
        hl = llir.Instr(None, "hlsl", None, [], "; collapsed loop " + H, {"lines": lines})
        if len(targets) == 1:
            term = llir.Instr(None, "br", None, [], "; br " + targets[0], {"targets": [targets[0]]})
        else:
            sel = "%s_to_%s" % (tag, targets[0])
            self.vty[sel] = "bool"
            lines.append("v%s = %s;" % (sel, " || ".join(e for (k, b, t, e, r) in exit_preds if t == targets[0])))
            term = llir.Instr(None, "br", None, [llir.Op("i1", "reg", sel)], "; br sel", {"targets": [targets[0], targets[1]]})
        hl.block = term.block = tag
        U.instrs = [hl, term]
        pt = m.byname[pre].instrs[-1]
        pt.extra["targets"] = [tag if x == H else x for x in pt.extra["targets"]]
        pos = [b.name for b in m.blocks].index(H)
        m.blocks = [b for b in m.blocks if b.name not in body]
        m.blocks.insert(min(pos, len(m.blocks)), U)
        m.byname = {b.name: b for b in m.blocks}
        for b in m.blocks:
            t = b.instrs[-1]
            if t.op == "br":
                b.succs = list(t.extra["targets"])
            elif t.op == "switch":
                b.succs = [t.extra["default"]] + [x[1] for x in t.extra["cases"]]
            else:
                b.succs = []
            b.preds = []
        for b in m.blocks:
            for s_ in b.succs:
                m.byname[s_].preds.append(b.name)
        self.collapsed = getattr(self, "collapsed", []) + [(H, trip, len(body))]

    def v(self, op):
        """operand -> HLSL expression"""
        if op.kind == "reg":
            n = op.v
            if n in self.virt:
                k = self.virt[n]
                if k[0] == "handle":
                    return k[1]["name"]
                if k[0] == "ptr":
                    return self.ptr_expr(k[1])
                raise ValueError("virtual value used directly: %s" % n)
            return "v" + self.ren.get(n, n)
        if op.kind == "int":
            if op.ty == "i1":
                return "true" if op.v & 1 else "false"
            x = op.v & M32
            return ("%du" % x) if x < 0x10000 else ("0x%Xu" % x)
        if op.kind == "float":
            return fconst(op.v)
        if op.kind == "undef":
            return {"float": "0.0f", "i1": "false"}.get(op.ty, "0u")
        if op.kind == "glob":
            return gname(op.v)
        if op.kind == "cexpr":
            g, idx = op.v
            return "%s[%d]" % (gname(g), idx[-1])
        raise ValueError(op)

    def ptr_expr(self, gep):
        base = gep.args[0]
        idx = gep.args[1:]
        if base.kind == "glob":
            return "%s[%s]" % (gname(base.v), self.v(idx[-1]))
        if base.kind == "reg" and base.v in self.arrays:
            return "%s[%s]" % (self.arrays[base.v][0], self.v(idx[-1]))
        raise NotImplementedError(gep.raw)

    # ---------------- statements of one block
    def block_stmts(self, bname):
        out = []
        for ins in self.m.byname[bname].instrs:
            if ins.op == "hlsl":
                out.extend(ins.extra["lines"])
                continue
            if ins.op in ("phi", "br", "ret", "switch", "alloca", "getelementptr"):
                continue
            s = self.stmt(ins)
            if s:
                out.append(s)
        return out

    def stmt(self, ins):
        op, d = ins.op, ins.dest
        a = [self.v(x) if not (x.kind == "reg" and x.v in self.virt and self.virt[x.v][0] == "cb") else None for x in ins.args]
        D = ("v" + self.ren.get(d, d)) if d else None
        if op in ("fadd", "fsub", "fmul", "fdiv"):
            if op == "fsub" and ins.args[0].kind == "float" and ins.args[0].v == 0.0 and struct.pack("<f", ins.args[0].v) == b"\x00\x00\x00\x80":
                return "%s = -%s;" % (D, a[1])
            return "%s = %s %s %s;" % (D, a[0], {"fadd": "+", "fsub": "-", "fmul": "*", "fdiv": "/"}[op], a[1])
        if op in ("add", "sub", "mul", "and", "or", "xor", "shl", "lshr", "ashr", "udiv", "urem", "sdiv", "srem"):
            if ins.ty == "i1":
                return "%s = %s;" % (D, {"and": "%s && %s", "or": "%s || %s", "xor": "%s != %s"}[op] % (a[0], a[1]))
            e = {"add": "%s + %s", "sub": "%s - %s", "mul": "%s * %s", "and": "%s & %s", "or": "%s | %s",
                 "xor": "%s ^ %s", "shl": "%s << %s", "lshr": "%s >> %s", "ashr": "asuint(asint(%s) >> %s)",
                 "udiv": "%s / %s", "urem": "%s %% %s", "sdiv": "asuint(asint(%s) / asint(%s))",
                 "srem": "asuint(asint(%s) %% asint(%s))"}[op] % (a[0], a[1])
            return "%s = %s;" % (D, e)
        if op == "icmp":
            p = ins.extra["pred"]
            x, y = a
            if p[0] == "s":
                x, y = "asint(%s)" % x, "asint(%s)" % y
            o = {"eq": "==", "ne": "!=", "gt": ">", "ge": ">=", "lt": "<", "le": "<="}[p if p in ("eq", "ne") else p[1:]]
            return "%s = %s %s %s;" % (D, x, o, y)
        if op == "fcmp":
            p = ins.extra["pred"]
            x, y = a
            e = {"oeq": "{0} == {1}", "ogt": "{0} > {1}", "oge": "{0} >= {1}", "olt": "{0} < {1}", "ole": "{0} <= {1}",
                 "one": "({0} < {1} || {0} > {1})", "une": "{0} != {1}", "ugt": "!({0} <= {1})", "uge": "!({0} < {1})",
                 "ult": "!({0} >= {1})", "ule": "!({0} > {1})", "ueq": "!({0} < {1} || {0} > {1})",
                 "ord": "({0} == {0} && {1} == {1})", "uno": "({0} != {0} || {1} != {1})",
                 "true": "true", "false": "false"}[p].format(x, y)
            return "%s = %s;" % (D, e)
        if op == "select":
            return "%s = %s ? %s : %s;" % (D, a[0], a[1], a[2])
        if op in ("sitofp", "uitofp", "fptosi", "fptoui", "bitcast", "zext", "sext", "trunc"):
            sty = ins.args[0].ty
            x = a[0]
            if op == "sitofp":
                e = ("(%s ? -1.0f : 0.0f)" % x) if sty == "i1" else "(float)asint(%s)" % x
            elif op == "uitofp":
                e = "(float)%s" % x
            elif op == "fptosi":
                e = "asuint((int)%s)" % x
            elif op == "fptoui":
                e = "(uint)%s" % x
            elif op == "bitcast":
                e = ("asuint(%s)" % x) if sty == "float" else ("asfloat(%s)" % x)
            elif op == "zext":
                e = "(uint)%s" % x
            elif op == "sext":
                e = ("(%s ? 0xFFFFFFFFu : 0u)" % x) if sty == "i1" else x
            else:
                e = "((%s & 1u) != 0)" % x if ins.ty == "i1" else x
            return "%s = %s;" % (D, e)
        if op == "extractvalue":
            src = ins.args[0]
            k = ins.extra["index"][0]
            if src.v in self.virt and self.virt[src.v][0] == "cb":
                cbins = self.virt[src.v][1]
                return "%s = %s;" % (D, self.cb_expr(cbins, k))
            if k == 4:
                raise NotImplementedError("status read " + ins.raw)
            return "%s = v%s.%s;" % (D, self.ren.get(src.v, src.v), SW[k])
        if op == "load":
            if self.spec:      # speculated read of a local / groupshared array: keep the index in range
                gep = self.virt[ins.args[0].v][1]
                e = self.ptr_expr(gep)
                base, idx = e.split("[", 1)
                return "%s = %s[%s ? %s : 0u];" % (D, base, self.spec, idx[:-1])
            return "%s = %s;" % (D, a[0])
        if op == "store":
            if self.spec:
                return "if (%s) { %s = %s; }" % (self.spec, a[1], a[0])
            return "%s = %s;" % (a[1], a[0])
        if op == "call":
            return self.call_stmt(ins, D, a)
        raise NotImplementedError(ins.raw)

    def cb_expr(self, cbins, comp):
        h = cbins.args[1]
        res = self.virt[h.v][1]
        reg = cbins.args[2]
        isint = cbins.extra["fn"].endswith(".i32")
        name, lmap = self.cbm[res["reg"]]
        if reg.kind != "int":
            return self.cb_dyn_expr(res, reg, comp, isint)
        key = (reg.v, comp)
        if key not in lmap:
            raise KeyError("cbuffer %s reg %d comp %d has no member" % (name, reg.v, comp))
        expr, base = lmap[key]
        if isint:
            return expr if base == "uint" else "asuint(%s)" % expr
        return expr if base == "float" else "asfloat(%s)" % expr

    def cb_dyn_expr(self, res, reg, comp, isint):
        """dynamically indexed cbuffer array member: register = base + idx*stride + row, written by dxc as
        add(shl/mul(idx, stride) | idx, base + row). Rebuild `Arr[idx][row].c` / `Arr[idx].c` so dxc emits the same
        register arithmetic."""
        m = self.m
        d = m.defs.get(reg.v)
        X, C = (d.args[0], d.args[1].v) if (d is not None and d.op == "add" and d.args[1].kind == "int") else (reg, 0)
        S, idx = 1, X
        xd = m.defs.get(X.v) if X.kind == "reg" else None
        if xd is not None and xd.op == "shl" and xd.args[1].kind == "int":
            S, idx = 1 << xd.args[1].v, xd.args[0]
        elif xd is not None and xd.op == "mul" and xd.args[1].kind == "int":
            S, idx = xd.args[1].v, xd.args[0]
        cbname = [c for c in self.ifc.cbuffers if [x for x in self.ifc.res if x["name"] == c[0]][0]["reg"] == res["reg"]][0]
        for mem in cbname[1]:
            if not mem.count or mem.children is not None:
                continue
            base, dims = ty_info(mem.ty)
            rb = mem.offset // 16
            if isinstance(dims, tuple):
                es, ncol = dims[0], dims[1]
                if S == es and rb <= C < rb + es:
                    e = "%s[%s][%d].%s" % (mem.name, self.v(idx), C - rb, SW[comp])
                    break
            else:
                c0 = (mem.offset % 16) // 4
                if S == 1 and C == rb and c0 <= comp < c0 + dims:
                    e = "%s[%s]%s" % (mem.name, self.v(idx), ("." + SW[comp - c0]) if dims > 1 else "")
                    break
        else:
            raise NotImplementedError("dynamic cbuffer index pattern: base %d stride %d" % (C, S))
        if isint:
            return e if base == "uint" else "asuint(%s)" % e
        return e if base == "float" else "asfloat(%s)" % e

    def call_stmt(self, ins, D, a):
        fn = ins.extra["fn"]
        oc = ins.extra["opcode"]
        if fn.startswith("dx.op.createHandle") or fn.startswith("dx.op.cbufferLoadLegacy"):
            return None
        if fn.startswith("dx.op.threadId."):
            return "%s = DTid.%s;" % (D, SW[ins.args[1].v])
        if fn.startswith("dx.op.groupId."):
            return "%s = Gid.%s;" % (D, SW[ins.args[1].v])
        if fn.startswith("dx.op.threadIdInGroup."):
            return "%s = GTid.%s;" % (D, SW[ins.args[1].v])
        if fn.startswith("dx.op.flattenedThreadIdInGroup"):
            return "%s = GI;" % D
        if fn.startswith("dx.op.unary."):
            f = {6: "abs", 7: "saturate", 12: "cos", 13: "sin", 14: "tan", 15: "acos", 16: "asin", 17: "atan",
                 21: "exp2", 22: "frac", 23: "log2", 24: "sqrt", 25: "rsqrt", 26: "round", 27: "floor", 28: "ceil",
                 29: "trunc"}[oc]
            return "%s = %s(%s);" % (D, f, a[1])
        if fn.startswith("dx.op.binary."):
            if oc in (35, 36):
                return "%s = %s(%s, %s);" % (D, "max" if oc == 35 else "min", a[1], a[2])
            if oc in (37, 38):
                return "%s = asuint(%s(asint(%s), asint(%s)));" % (D, "max" if oc == 37 else "min", a[1], a[2])
            if oc in (39, 40):
                return "%s = %s(%s, %s);" % (D, "max" if oc == 39 else "min", a[1], a[2])
        if fn.startswith("dx.op.tertiary."):
            if oc == 46:
                return "%s = mad(%s, %s, %s);" % (D, a[1], a[2], a[3])
            if oc == 48:
                return "%s = asuint(mad(asint(%s), asint(%s), asint(%s)));" % (D, a[1], a[2], a[3])
            if oc == 49:
                return "%s = mad(%s, %s, %s);" % (D, a[1], a[2], a[3])
        if fn.startswith("dx.op.dot2."):
            return "%s = dot(float2(%s, %s), float2(%s, %s));" % (D, a[1], a[2], a[3], a[4])
        if fn.startswith("dx.op.dot3."):
            return "%s = dot(float3(%s, %s, %s), float3(%s, %s, %s));" % (D, *a[1:7])
        if fn.startswith("dx.op.dot4."):
            return "%s = dot(float4(%s, %s, %s, %s), float4(%s, %s, %s, %s));" % (D, *a[1:9])
        res = self.virt[ins.args[1].v][1] if len(ins.args) > 1 and ins.args[1].kind == "reg" and ins.args[1].v in self.virt else None
        if fn.startswith("dx.op.getDimensions"):
            ht = self.ifc.hlsl_type(res)
            self.vty[ins.dest] = "uint4"
            k = ht.split("<")[0]
            if k in ("Buffer", "RWBuffer", "StructuredBuffer", "RWStructuredBuffer"):
                return "%s = 0u; %s.GetDimensions(%s.x);" % (D, res["name"], D)
            if k in ("Texture2D", "TextureCube"):
                return "%s = 0u; %s.GetDimensions(%s, %s.x, %s.y, %s.w);" % (D, res["name"], a[2], D, D, D)
            if k in ("Texture2DArray", "Texture3D", "TextureCubeArray"):
                return "%s = 0u; %s.GetDimensions(%s, %s.x, %s.y, %s.z, %s.w);" % (D, res["name"], a[2], D, D, D, D)
            if k in ("RWTexture2D",):
                return "%s = 0u; %s.GetDimensions(%s.x, %s.y);" % (D, res["name"], D, D)
            if k in ("RWTexture2DArray", "RWTexture3D"):
                return "%s = 0u; %s.GetDimensions(%s.x, %s.y, %s.z);" % (D, res["name"], D, D, D)
            raise NotImplementedError(ht)
        if fn.startswith("dx.op.textureLoad."):
            ht = self.ifc.hlsl_type(res)
            k = ht.split("<")[0]
            el = self.ifc.elem_type(res)
            self.vty[ins.dest] = el
            mip, c0, c1, c2 = a[2], a[3], a[4], a[5]
            offs = [x for x in ins.args[6:9]]
            off_used = any(o.kind == "int" and o.v != 0 for o in offs)
            if k == "Texture2D":
                e = "%s.Load(int3(%s, %s, %s)%s)" % (res["name"], c0, c1, mip, (", int2(%s, %s)" % (self.v(offs[0]), self.v(offs[1]))) if off_used else "")
            elif k in ("Texture2DArray", "Texture3D"):
                e = "%s.Load(int4(%s, %s, %s, %s)%s)" % (res["name"], c0, c1, c2, mip, (", int%d(%s)" % (2 if k == "Texture2DArray" else 3, ", ".join(self.v(o) for o in offs[:2 if k == "Texture2DArray" else 3]))) if off_used else "")
            elif k.startswith("RW"):
                e = "%s[uint3(%s, %s, %s)]" % (res["name"], c0, c1, c2) if k != "RWTexture2D" else "%s[uint2(%s, %s)]" % (res["name"], c0, c1)
            else:
                raise NotImplementedError(ht)
            return "%s = %s;" % (D, e)
        if fn.startswith("dx.op.bufferLoad."):
            ht = self.ifc.hlsl_type(res)
            k = ht.split("<")[0]
            if k in ("StructuredBuffer", "RWStructuredBuffer"):
                sname, mem, size = self.ifc.sbinfo[res["name"]]
                lm = sb_leaf_map(mem)
                off = ins.args[3]
                if off.kind != "int":
                    raise NotImplementedError("dynamic structured offset " + ins.raw)
                if off.v not in lm:
                    raise KeyError("struct offset %d" % off.v)
                expr, base, n = lm[off.v]
                self.vty[ins.dest] = base + (str(n) if n > 1 else "")
                ix = a[2] if not self.spec else "(%s ? %s : 0u)" % (self.spec, a[2])
                return "%s = %s[%s].%s;" % (D, res["name"], ix, expr)
            el = self.ifc.elem_type(res)
            self.vty[ins.dest] = el
            return "%s = %s[%s];" % (D, res["name"], a[2])
        if fn.startswith("dx.op.sampleLevel.") or fn.startswith("dx.op.sampleCmpLevelZero."):
            ht = self.ifc.hlsl_type(res)
            k = ht.split("<")[0]
            smp = self.virt[ins.args[2].v][1]["name"]
            self.vty[ins.dest] = "float4"
            nc = {"Texture2D": 2, "Texture2DArray": 3, "Texture3D": 3, "TextureCube": 3, "TextureCubeArray": 4}[k]
            no = {"Texture2D": 2, "Texture2DArray": 2, "Texture3D": 3, "TextureCube": 0, "TextureCubeArray": 0}[k]
            coords = "float%d(%s)" % (nc, ", ".join(a[3:3 + nc]))
            offs = ins.args[7:7 + no]
            off_used = any(not (o.kind == "int" and o.v == 0) and o.kind != "undef" for o in offs)
            ostr = (", int%d(%s)" % (no, ", ".join(self.v(o) for o in offs))) if off_used else ""
            if fn.startswith("dx.op.sampleLevel."):
                return "%s = %s.SampleLevel(%s, %s, %s%s);" % (D, res["name"], smp, coords, a[10], ostr)
            return "%s = %s.SampleCmpLevelZero(%s, %s, %s%s).xxxx;" % (D, res["name"], smp, coords, a[10], ostr)
        if fn.startswith("dx.op.textureStore."):
            ht = self.ifc.hlsl_type(res)
            k = ht.split("<")[0]
            mask = ins.args[9].v
            assert mask == 15, "partial textureStore mask"
            idx = "uint2(%s, %s)" % (a[2], a[3]) if k == "RWTexture2D" else "uint3(%s, %s, %s)" % (a[2], a[3], a[4])
            st = "%s[%s] = float4(%s, %s, %s, %s);" % (res["name"], idx, a[5], a[6], a[7], a[8])
            return ("if (%s) { %s }" % (self.spec, st)) if self.spec else st
        if fn.startswith("dx.op.bufferStore."):
            el = self.ifc.elem_type(res)
            mask = ins.args[8].v
            n = {1: 1, 3: 2, 7: 3, 15: 4}[mask]
            val = a[4] if n == 1 else "%s%d(%s)" % (el.rstrip("1234"), n, ", ".join(a[4:4 + n]))
            st = "%s[%s] = %s;" % (res["name"], a[2], val)
            return ("if (%s) { %s }" % (self.spec, st)) if self.spec else st
        raise NotImplementedError(ins.raw)

    # ---------------- structuring (Ramsey, "Beyond Relooper")
    def is_merge(self, n):
        return sum(1 for p in self.m.byname[n].preds if self.rpn[p] < self.rpn[n]) >= 2

    def is_header(self, n):
        return any(self.rpn[p] >= self.rpn[n] for p in self.m.byname[n].preds)

    def new_id(self):
        self.nid += 1
        return self.nid

    def do_tree(self, x, ctx):
        merges = sorted([c for c in self.kids[x] if self.is_merge(c)], key=lambda n: -self.rpn[n])
        if self.is_header(x):
            lid = self.new_id()
            return Node("loop", self.node_within(x, merges, ctx + [("loop", x, lid)]), id=lid)
        return self.node_within(x, merges, ctx)

    def node_within(self, x, ys, ctx):
        if ys:
            y = ys[0]
            bid = self.new_id()
            inner = self.node_within(x, ys[1:], ctx + [("block", y, bid)])
            return seq([Node("block", inner, id=bid), self.do_tree(y, ctx)])
        items = [Node("code", x)]
        t = self.m.byname[x].instrs[-1]
        if t.op == "br" and not t.args:
            items.append(self.do_branch(x, t.extra["targets"][0], ctx))
        elif t.op == "br":
            c = self.v(t.args[0])
            items.append(Node("if", c, self.do_branch(x, t.extra["targets"][0], ctx + [("if",)]),
                              self.do_branch(x, t.extra["targets"][1], ctx + [("if",)])))
        elif t.op == "ret":
            items.append(Node("ret"))
        else:
            raise NotImplementedError(t.raw)
        return seq(items)

    def copies(self, src, tgt):
        tb = self.m.byname[tgt]
        phis = [i for i in tb.instrs if i.op == "phi"]
        if not phis:
            return None
        pairs = []
        for p in phis:
            k = p.extra["from"].index(src)
            pairs.append(("v" + self.ren.get(p.dest, p.dest), self.v(p.args[k])))
        dests = {d for d, _ in pairs}
        if any(re.search(r"\b%s\b" % re.escape(d), s) for _, s in pairs for d in dests if s != d):
            # parallel copy with interference: go through temporaries
            lines = ["%s t_%s = %s;" % (self.vty[d[1:]], d, s) for d, s in pairs] + ["%s = t_%s;" % (d, d) for d, _ in pairs]
            return Node("copies", ["{ " + " ".join(lines) + " }"], [d for d, _ in pairs])
        return Node("copies", ["%s = %s;" % (d, s) for d, s in pairs if d != s], [d for d, _ in pairs])

    def do_branch(self, src, tgt, ctx):
        cp = self.copies(src, tgt)
        if tgt in self.ret_only:
            return seq([cp, Node("ret")])
        if self.rpn[tgt] <= self.rpn[src]:
            lid = [c for c in ctx if c[0] == "loop" and c[1] == tgt][-1][2]
            return seq([cp, Node("br", lid, "loop")])
        if self.is_merge(tgt):
            bid = [c for c in ctx if c[0] == "block" and c[1] == tgt][-1][2]
            return seq([cp, Node("br", bid, "block")])
        return seq([cp, self.do_tree(tgt, ctx)])

    # ---------------- lowering to HLSL text
    def lower(self, node, kf, kb, kc, ind):
        """-> (lines, pending:set(target ids escaping by native break), flagged:set(target ids flagged))"""
        k = node.k
        if k not in ("seq", "copies", "br"):
            self._last_copies = None
        if k == "seq":
            lines, pend, flg = [], set(), set()
            items = node.a
            for i, it in enumerate(items):
                f = kf if i == len(items) - 1 else ("mid", id(it))
                l, p, g = self.lower(it, f, kb, kc, ind)
                lines += l; pend |= p; flg |= g
            return lines, pend, flg
        if k == "code":
            self._last_copies = None
            return [ind + "// bb %s" % node.a] + [ind + s for s in self.block_stmts(node.a)], set(), set()
        if k == "copies":
            self._last_copies = node.b
            return [ind + s for s in node.a], set(), set()
        if k == "ret":
            return [ind + "return;"], set(), set()
        if k == "if":
            l1, p1, g1 = self.lower(node.b, kf, kb, kc, ind + "    ")
            l2, p2, g2 = self.lower(node.c, kf, kb, kc, ind + "    ")
            if not l1 and not l2:
                return [], p1 | p2, g1 | g2
            if not l1:
                return [ind + "if (!(%s))" % node.a, ind + "{"] + l2 + [ind + "}"], p1 | p2, g1 | g2
            lines = [ind + "if (%s)" % node.a, ind + "{"] + l1 + [ind + "}"]
            if l2:
                lines += [ind + "else", ind + "{"] + l2 + [ind + "}"]
            return lines, p1 | p2, g1 | g2
        if k == "br":
            tp = ("head", node.a) if node.b == "loop" else self.after[node.a]
            if node.b == "loop":
                self._last_copies = None        # header-phi copies are live in the whole loop body: never reset
            r = self.resolve(tp, node.a, kf, kb, kc, ind, first=True)
            self._last_copies = None
            return r
        if k == "loop":
            head = ("head", node.id)
            l, p, g = self.lower(node.a, head, kf, head, ind + "    ")
            pre_ = [ind + "_f%d = false;" % t for t in sorted(p)]
            lines = pre_ + [ind + "[loop] for (;;)", ind + "{"] + self.resets(p, ind + "    ") + l + [ind + "}"]
            d, p2, g2 = self.dispatch(p, kf, kb, kc, ind)
            return lines + d, p2, g | g2
        if k == "block":
            self.after[node.id] = kf
            key = (node.id, kf, kb, kc)
            l, p, g = self.lower(node.a, kf, kb, kc, ind)
            if node.id not in g:
                return l, p, g
            # needs a breakable wrapper: switch(0) { default: } - 'break' leaves it, while 'continue' still
            # reaches the enclosing loop (a do-while(false) would capture it and force a flag)
            l, p, g = self.lower(node.a, kf, kf, kc, ind + "        ")
            g.discard(node.id)
            pre_ = [ind + "_f%d = false;" % t for t in sorted(p)]
            lines = pre_ + [ind + "switch (0u)", ind + "{", ind + "default:", ind + "    {"] +                 self.resets(p, ind + "        ") + l + [ind + "    }", ind + "}"]
            d, p2, g2 = self.dispatch(p, kf, kb, kc, ind)
            return lines + d, p2, g | g2
        raise ValueError(k)

    def resolve(self, tp, tid, kf, kb, kc, ind, first):
        clr = [] if first else [ind + "_f%d = false;" % tid]
        if tp == kf:
            return clr, set(), set()
        if tp == kb:
            return clr + [ind + "break;"], set(), set()
        if tp == kc:
            return clr + [ind + "continue;"], set(), set()
        if kb is None:
            raise RuntimeError("cannot reach jump target %r" % (tp,))
        self.flags.add(tid)
        if first and self._last_copies:
            self.flag_vars.setdefault(tid, set()).update(self._last_copies)
        return ([ind + "_f%d = true;" % tid] if first else []) + [ind + "break;"], {tid}, {tid}

    def resets(self, pend, ind):
        """Phi-destination variables written right before a multi-level (flagged) jump that leaves this construct
        are reset to zero at the construct's start. dxc does not thread the flag test, so without this they look
        possibly-unassigned on the (impossible) normal-exit path and get carried around enclosing loops as undef
        values that pin registers for the whole shader. They are read only at the jump target, and every edge into
        it assigns them right before jumping, so the reset never changes a value that is read."""
        vs = set()
        for t in pend:
            vs |= self.flag_vars.get(t, set())
        out = []
        for d in sorted(vs):
            ty = self.vty.get(d[1:])
            z = {"float": "0.0f", "uint": "0u", "int": "0", "bool": "false"}.get(ty)
            if z:
                out.append(ind + "%s = %s;" % (d, z))
        return out

    def dispatch(self, pend, kf, kb, kc, ind):
        lines, p2, g2 = [], set(), set()
        for t in sorted(pend):
            node_tp = self.target_point[t]
            l, p, g = self.resolve(node_tp, t, kf, kb, kc, ind + "    ", first=False)
            lines += [ind + "if (_f%d)" % t, ind + "{"] + l + [ind + "}"]
            p2 |= p; g2 |= g
        return lines, p2, g2

    def emit(self):
        self.after = {}
        tree = self.do_tree(self.order[0], [])
        # map construct id -> its target point (filled during lowering through self.after / loop heads)
        self.target_point = {}

        def walk(n):
            if n.k == "loop":
                self.target_point[n.id] = ("head", n.id)
                walk(n.a)
            elif n.k == "block":
                walk(n.a)
            elif n.k == "seq":
                for x in n.a:
                    walk(x)
            elif n.k == "if":
                walk(n.b); walk(n.c)
        walk(tree)
        self.flags = set()
        self.flag_vars = {}
        self._last_copies = None

        class TP(dict):
            pass
        # block target points are only known while lowering; make target_point fall back to self.after
        tp = self.target_point
        after = self.after

        class Lazy(dict):
            def __getitem__(s, k):
                return tp[k] if k in tp else after[k]
        self.target_point = Lazy()
        body, pend, flg = self.lower(tree, ("ret",), None, None, "    ")
        assert not pend, pend
        return self.assemble(body)

    def assemble(self, body):
        ifc = self.ifc
        out = ["// decompiled by gpu-shaders/gi/ll2hlsl.py from %s - do not edit by hand, see the *_fast.hlsl" % self.name, ""]
        out.append(ifc.decls())
        for g in self.m.globals.values():
            if g.space == 3:
                out.append("groupshared %s %s[%d];" % ("float" if g.elem_ty == "float" else "uint", gname(g.name), g.count))
            elif g.init is not None:
                vals = ", ".join(fconst(x) if g.elem_ty == "float" else ("%du" % (x & M32)) for x in g.init)
                out.append("static const %s %s[%d] = { %s };" % ("float" if g.elem_ty == "float" else "uint", gname(g.name), g.count, vals))
        nt = self.numthreads()
        out.append("")
        out.append("[numthreads(%d, %d, %d)]" % nt)
        out.append("void main(uint3 DTid : SV_DispatchThreadID, uint3 Gid : SV_GroupID, uint3 GTid : SV_GroupThreadID, uint GI : SV_GroupIndex)")
        out.append("{")
        # one flag per multi-level jump target: set right before the break, tested (and cleared) right after the
        # construct it leaves, false on every other path - a pattern dxc's jump threading folds back into edges
        for t in sorted(self.flags):
            out.append("    bool _f%d = false;" % t)
        for n, (an, ty, cnt) in self.arrays.items():
            out.append("    %s %s[%d];" % (ty, an, cnt))
        byty = {}
        for d, t in self.vty.items():
            byty.setdefault(t, []).append("v" + d)
        for t, names in sorted(byty.items(), key=lambda kv: str(kv[0])):
            if t is None:
                raise ValueError("untyped values: %s" % names[:5])
            init = {"float": "0.0f", "uint": "0u", "int": "0", "bool": "false"}.get(t)
            if t.endswith(("2", "3", "4")):
                init = "(%s)0" % t
            for i in range(0, len(names), 16):
                if self.zero_init and init:
                    # explicit zero instead of "uninitialised": a value the original reads only on paths where it
                    # was assigned must not become an undef phi (keeps a register alive from shader start)
                    out.append("    %s %s;" % (t, ", ".join("%s = %s" % (n, init) for n in names[i:i + 16])))
                else:
                    out.append("    %s %s;" % (t, ", ".join(names[i:i + 16])))
        out += body
        out.append("}")
        return "\n".join(out) + "\n"

    def numthreads(self):
        md = {}
        for l in self.text.splitlines():
            mm = re.match(r"^!(\d+) = (.*)$", l)
            if mm:
                md[mm.group(1)] = mm.group(2)
        ep = re.search(r"!dx\.entryPoints = !\{!(\d+)\}", self.text)
        props = md[ep.group(1)].rstrip("}").split(", ")[-1].lstrip("!")
        tm = re.search(r"i32 4, !(\d+)", md[props])
        return tuple(int(x) for x in re.findall(r"i32 (\d+)", md[tm.group(1)]))


def gname(g):
    m = re.search(r"\?(\w+)@@", g)
    if m:
        return m.group(1)
    return re.sub(r"\W", "_", g)


def fconst(x):
    import numpy as np
    f = np.float32(x)
    if f != f:
        return "asfloat(0x7FC00000u)"
    if f in (np.float32(np.inf), np.float32(-np.inf)):
        return "asfloat(0x%08Xu)" % struct.unpack("<I", struct.pack("<f", float(f)))[0]
    if f == 0 and struct.pack("<f", float(f)) == b"\x00\x00\x00\x80":
        return "-0.0f"
    s = repr(f)                  # numpy: shortest repr that round-trips float32
    s = s.replace("np.float32(", "").rstrip(")")
    if "e" not in s and "." not in s and "inf" not in s:
        s += ".0"
    if s.startswith("."):
        s = "0" + s
    return s + "f"


if __name__ == "__main__":
    import argparse
    ap = argparse.ArgumentParser()
    ap.add_argument("src"); ap.add_argument("dst")
    ap.add_argument("--collapse", default="", help="loop headers to unroll+if-convert: H:N,H:N (N = trip count)")
    ap.add_argument("--title", default=None)
    a = ap.parse_args()
    col = [(x.split(":")[0], int(x.split(":")[1])) for x in a.collapse.split(",") if x]
    d = Decompiler(open(a.src).read(), name=a.src.replace("\\", "/").split("/")[-1], collapse=col)
    text = d.emit()
    if a.title:
        text = "// " + a.title + chr(10) + text
    open(a.dst, "w").write(text)
    print("wrote", a.dst, "collapsed:", getattr(d, "collapsed", []))
