r"""prefetch.py - software-pipeline the two VCLOUD ray-march loops (exact): the mask + base samples of step i+1 are
issued at the top of step i, so their texture latency overlaps step i's work instead of starting the next iteration.

The march position is data-independent: step i is p_i = mad(dir, step, p_{i-1}) (the loop carries p in v508..v510)
and the mask/base sample coordinates depend only on p_i and loop invariants. So step i+1's coordinates are computed
at the top of step i by a renamed copy of exactly the statements that feed the two samples (same expressions, p_i
replaced by q = mad(dir, step, p_i), which is the value step i+1 computes for itself), the two results are carried
(pfM/pfB) and the loop body reads them instead of sampling. A prologue before the loop does the same for step 0. The
only extra work is one unused prefetch per ray (the last iteration). Every other statement is untouched.

    py -I prefetch.py IN.hlsl OUT.hlsl
"""
import re, sys

STMT = re.compile(r"^(\s*)(v\d+) = (.*);$")
LOOP = "[loop] for (;;)"


def match_brace(L, i):
    d = 0
    for j in range(i, len(L)):
        s = L[j].strip()
        d += s.count("{") - s.count("}")
        if d == 0:
            return j
    raise ValueError(i)


def main():
    src, dst = sys.argv[1], sys.argv[2]
    L = open(src, encoding="utf-8").read().split("\n")
    types = {}
    for l in L[:700]:
        m = re.match(r"^\s+(bool|float|uint|int|float4|float2|float3) (v\d+(?:, v\d+)*);$", l)
        if m:
            for v in m.group(2).split(", "):
                types[v] = m.group(1)
    loops = [i for i, l in enumerate(L) if LOOP in l]
    edits = []                                  # (line index, 'replace'|'insert_before'|'insert_after', text)
    for li in loops:
        ob = li + 1
        assert L[ob].strip() == "{"
        cb = match_brace(L, ob)
        body = list(range(ob + 1, cb))
        # position statements: the first three `vP = mad(dir, step, vCarry);`
        pos = []
        for i in body:
            m = STMT.match(L[i])
            if m and m.group(3).startswith("mad("):
                pos.append((i, m.group(2), m.group(3)))
            if len(pos) == 3:
                break
        carry = [re.match(r"mad\((v\d+), (v\d+), (v\d+)\)", e).group(3) for _, _, e in pos]
        # the first two samples of the body = mask + base of the main step
        smp = [i for i in body if ".SampleLevel(" in L[i]][:2]
        assert "sMaskMap" in L[smp[0]] and "sBase" in L[smp[1]], (L[smp[0]], L[smp[1]])
        defs = {}
        for i in body:
            m = STMT.match(L[i])
            if m and i <= smp[1]:
                defs.setdefault(m.group(2), []).append(i)
        assert all(len(v) == 1 for v in defs.values()), "a value is defined twice before the samples"
        posv = {v for _, v, _ in pos}
        need, order = set(), []

        def take(v):
            if v in need or v not in defs or v in posv:
                return
            need.add(v)
            i = defs[v][0]
            for o in re.findall(r"\bv\d+\b", STMT.match(L[i]).group(3)):
                take(o)
            order.append(i)
        for i in smp:
            take(STMT.match(L[i]).group(2))
        order.sort()
        inloop = {v for v in defs}
        leaves = set()
        for i in order:
            for o in re.findall(r"\bv\d+\b", STMT.match(L[i]).group(3)):
                if o in inloop and o not in need and o not in posv:
                    leaves.add(o)
        assert not leaves, "sample coordinates depend on loop-variant values other than the position: %s" % leaves
        tag = "L%d" % li
        ind = re.match(r"^(\s*)", L[ob + 1]).group(1)
        indo = re.match(r"^(\s*)", L[li]).group(1)

        def clone(prefix, posmap):
            out = []
            ren = dict(posmap)
            for i in order:
                ind_, v, e = STMT.match(L[i]).groups()
                nv = "%s_%s" % (prefix, v)
                e2 = re.sub(r"\bv\d+\b", lambda m: ren.get(m.group(0), m.group(0)), e)
                out.append("%s %s = %s;" % (types[v], nv, e2))
                ren[v] = nv
            return out, ren
        # prologue (before the loop): position of step 0 from the carried start
        pro = []
        pm = {}
        for (_, v, e), c in zip(pos, carry):
            pro.append("float %s_p_%s = %s;" % (tag, v, e))
            pm[v] = "%s_p_%s" % (tag, v)
        cl, ren = clone(tag + "_p", pm)
        pro += cl
        mv, bv = STMT.match(L[smp[0]]).group(2), STMT.match(L[smp[1]]).group(2)
        pro += ["float4 %s_pfM = %s;" % (tag, ren[mv]), "float4 %s_pfB = %s;" % (tag, ren[bv])]
        edits.append((li, "insert_before", [indo + x for x in ["// prefetch prologue (prefetch.py): mask/base samples of step 0"] + pro]))
        # in-loop: next position q = mad(dir, step, p_i), clone, results nfM/nfB
        nxt = []
        nm = {}
        for (_, v, e), c in zip(pos, carry):
            e2 = e.replace(c + ")", v + ")")
            nxt.append("float %s_n_%s = %s;" % (tag, v, e2))
            nm[v] = "%s_n_%s" % (tag, v)
        cl, ren = clone(tag + "_n", nm)
        nxt += cl
        nxt += ["float4 %s_nfM = %s;" % (tag, ren[mv]), "float4 %s_nfB = %s;" % (tag, ren[bv])]
        edits.append((pos[-1][0], "insert_after", [ind + x for x in ["// prefetch (prefetch.py): mask/base samples of step i+1"] + nxt]))
        # body: read the carried samples
        for i, (cv, nmv) in zip(smp, ((mv, "pfM"), (bv, "pfB"))):
            ind_ = re.match(r"^(\s*)", L[i]).group(1)
            edits.append((i, "replace", ["%s%s = %s_%s;   // prefetched (was %s)" % (ind_, cv, tag, nmv, L[i].strip())]))
        # the continue: advance the carried samples
        conts = [i for i in body if L[i].strip() == "continue;"]
        assert len(conts) == 1, conts
        ci = conts[0]
        indc = re.match(r"^(\s*)", L[ci]).group(1)
        edits.append((ci, "insert_before", ["%s%s_pfM = %s_nfM;" % (indc, tag, tag), "%s%s_pfB = %s_nfB;" % (indc, tag, tag)]))
        print("loop @%d: pos %s, samples %s/%s, %d coordinate statements cloned" % (li + 1, ",".join(posv), mv, bv, len(order)))
    ins_b, ins_a, rep = {}, {}, {}
    for i, k, t in edits:
        {"insert_before": ins_b, "insert_after": ins_a, "replace": rep}[k].setdefault(i, []).extend(t)
    out = []
    for i, l in enumerate(L):
        out += ins_b.get(i, [])
        out += rep.get(i, [l])
        out += ins_a.get(i, [])
    open(dst, "w", encoding="utf-8").write("\n".join(out))


if __name__ == "__main__":
    main()
