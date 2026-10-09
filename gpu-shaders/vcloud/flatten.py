r"""flatten.py - if-convert the VCLOUD light-march chains (exact).

Each light march (2 layers x sun/moon = 4 chains, 4 steps each) is shipped as nested
    if (cK) { T = vK; } else { ...step K body...; if (cK+1) { T = vK+1; } else { ... T = vLast; } }
where cK = "step K is outside the cloud layer" depends only on geometry, never on a sample. Every body is pure
(SampleLevel + arithmetic, no stores), so computing all four steps and selecting
    T = vLast; T = c4 ? v4 : T; ... ; T = c1 ? v1 : T
gives the same T bit for bit, and the 12 mask/base/detail samples of a chain no longer wait for each other
(the shipped ISA has one texture round trip per step plus the constant/descriptor reloads of every step).
Only chains whose then-blocks are a single `T = value;` are touched.

    py -I flatten.py IN.hlsl OUT.hlsl [--min-depth 2]
"""
import re, sys

IF = re.compile(r"^\s*if \((v\d+)\)$")
ASG = re.compile(r"^\s*(v\d+) = ([^;]+);$")


def match_brace(L, i):
    """L[i] is a '{' line -> index of its matching '}' line."""
    d = 0
    for j in range(i, len(L)):
        s = L[j].strip()
        d += s.count("{") - s.count("}")
        if d == 0:
            return j
    raise ValueError(i)


def chain_at(L, i, target=None):
    """if L[i..i+5] is `if (c) { T = v; } else {` -> (c, T, v, else_open, else_close) else None"""
    m = IF.match(L[i])
    if not m or i + 5 >= len(L):
        return None
    if L[i + 1].strip() != "{" or L[i + 3].strip() != "}" or L[i + 4].strip() != "else" or L[i + 5].strip() != "{":
        return None
    a = ASG.match(L[i + 2])
    if not a or (target and a.group(1) != target):
        return None
    return m.group(1), a.group(1), a.group(2), i + 5, match_brace(L, i + 5)


def main():
    src, dst = sys.argv[1], sys.argv[2]
    mind = int(sys.argv[sys.argv.index("--min-depth") + 1]) if "--min-depth" in sys.argv else 3
    L = open(src, encoding="utf-8").read().split("\n")
    drop, after, before = set(), {}, {}
    nchains = 0
    i = 0
    while i < len(L):
        c = chain_at(L, i)
        if not c or i in drop:
            i += 1
            continue
        # follow the nesting: the next link must be inside this else-block, same target
        links = [c]
        cur = c
        while True:
            _, T, _, a, b = cur
            nxt = None
            for j in range(a + 1, b):
                cj = chain_at(L, j, T)
                if cj and cj[4] == b - 1 or (cj and all(L[k].strip() == "}" for k in range(cj[4] + 1, b))):
                    nxt = cj
                    break
            if not nxt:
                break
            links.append(nxt)
            cur = nxt
        if len(links) < mind:
            i += 1
            continue
        # innermost else-body must end with `T = vLast;`
        T = links[0][1]
        last_close = links[-1][4]
        k = last_close - 1
        while not L[k].strip():
            k -= 1
        assert ASG.match(L[k]) and ASG.match(L[k]).group(1) == T, (L[k], T)
        for (cnd, _, val, a, b) in links:
            start = a - 5
            for q in range(start, a + 1):
                drop.add(q)
            drop.add(b)
        ind = re.match(r"^(\s*)", L[links[0][0] and (links[0][3] - 5)]).group(1)
        sel = ["%s%s = %s ? %s : %s;   // flattened link %d" % (ind, T, cnd, val, T, n + 1)
               for n, (cnd, _, val, a, b) in reversed(list(enumerate(links)))]
        after[last_close] = sel
        # region = the whole enclosing block (straight-line once flattened): its '{' .. matching '}'
        d, ob = 0, links[0][3] - 6
        while True:
            t = L[ob].strip()
            d += t.count("}") - t.count("{")
            if d < 0:
                break
            ob -= 1
        cb = match_brace(L, ob)
        before[ob + 1] = "%s// FLAT_BEGIN %d" % (ind, nchains + 1)
        before[cb] = "%s// FLAT_END %d" % (ind, nchains + 1)
        nchains += 1
        print("chain %d: %s, %d links, conds %s" % (nchains, T, len(links), ",".join(l[0] for l in links)))
        i = last_close + 1
    out = []
    for i, l in enumerate(L):
        if i in before:
            out.append(before[i])
        if i not in drop:
            out.append(l)
        if i in after:
            out += after[i]
    open(dst, "w", encoding="utf-8").write("\n".join(out))


if __name__ == "__main__":
    main()
