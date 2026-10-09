r"""hoist.py - inside every `// FLAT_BEGIN n` .. `// FLAT_END n` region (flatten.py output; straight-line SSA code),
move each SampleLevel and the statements it depends on to the top of the region, so the DXIL (and amdxc, which
keeps source order in such long blocks) issues all samples of a light march before consuming any. Pure reorder of
SSA statements: every value is computed by the same expression from the same operands.

    py -I hoist.py IN.hlsl OUT.hlsl [--split N]   (N: first N samples + the last one in batch 1, the rest in batch 2)
"""
import re, sys

STMT = re.compile(r"^(\s*)(v\d+) = (.*);(\s*//.*)?$")


SPLIT = 0


def main():
    global SPLIT
    src, dst = sys.argv[1], sys.argv[2]
    if "--split" in sys.argv:
        SPLIT = int(sys.argv[sys.argv.index("--split") + 1])
    L = open(src, encoding="utf-8").read().split("\n")
    out = []
    i = 0
    while i < len(L):
        if "// FLAT_BEGIN" not in L[i]:
            out.append(L[i]); i += 1; continue
        j = i + 1
        while "// FLAT_END" not in L[j]:
            j += 1
        body = L[i + 1:j]
        st = []
        for l in body:
            if not l.strip() or l.strip().startswith("//"):
                continue
            m = STMT.match(l)
            assert m, "non-SSA line in flat region: %r" % l
            st.append((m.group(2), m.group(3), l))
        defs = {v: k for k, (v, e, l) in enumerate(st)}
        deps = [set(defs[x] for x in re.findall(r"\bv\d+\b", e) if x in defs and defs[x] < k) for k, (v, e, l) in enumerate(st)]
        moved = set()
        order = []

        def take(k):
            if k in moved:
                return
            for d in sorted(deps[k]):
                take(d)
            moved.add(k); order.append(k)
        samples = [k for k, (v, e, l) in enumerate(st) if ".SampleLevel(" in e]
        late = samples[SPLIT:-1] if SPLIT else []       # second batch (light steps 3-4); the rest goes first
        first = [k for k in samples if k not in late]
        for k in first:
            for d in sorted(deps[k]):
                take(d)
        for k in first:
            take(k)
        # closure of the late batch, so it can be emitted as one block where it is first needed
        need = set()
        def clos(k):
            if k in need or k in moved:
                return
            for d in deps[k]:
                clos(d)
            need.add(k)
        for k in late:
            clos(k)
        out.append(L[i])
        out += [st[k][2] for k in order]
        emitted = False
        for k in range(len(st)):
            if k in moved:
                continue
            if late and not emitted and (k in need or deps[k] & need):
                blk = sorted(need)
                out += [st[q][2] for q in blk if q not in late] + [st[q][2] for q in late]
                moved.update(need); emitted = True
                if k in moved:
                    continue
            if k not in moved:
                out.append(st[k][2])
        out.append(L[j])
        print("region %s: %d statements, %d samples hoisted with %d deps" % (L[i].split()[-1], len(st), len(samples), len(order) - len(samples)))
        i = j + 1
    open(dst, "w", encoding="utf-8").write("\n".join(out))


if __name__ == "__main__":
    main()
