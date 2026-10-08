r"""mk414.py - build gi\out\rp414_src.hlsl, the latency rewrite of RefProbe #414 (GI reflection probe trace), from
the shipped DXIL, reproducibly.

    py -I mk414.py                 # decompile + restructure -> out\rp414_src.hlsl
    py -I mkfast.py rp414          # pins + dxc -> out\rp414_fast.{hlsl,dxil,ll}

1. ll2hlsl.py --collapse (full unroll + if-conversion) of the constant-trip probe loops in COLLAPSE, with --cdpred
   (a block that is control-equivalent to an earlier block of the same iteration reuses its predicate, so an
   iteration's reads no longer wait for the previous iteration's branch conditions).
   Loop 1789 (the 8-probe visibility/sky-occlusion gather) is NOT collapsed: unrolled, its 8 probes' reads need
   156-256 VGPRs (3 or 2 waves on gfx1103, scratch spills).
2. if-conversion of the small arithmetic-only if/else blocks in IFCONV (the Chebyshev visibility test inside loop
   1789): both arms are evaluated and the phi becomes a select. The arms contain no reads/writes, so the result is
   the same value; what changes is that the reads after the join (second depth sample, 9 sky-occlusion reads) no
   longer wait behind a branch on the first depth sample (4 -> 2 dependent round trips per probe).
3. PREFETCH (selective software pipelining of loop 1789): the next probe's oct_probe_info reads are issued at the
   top of the current iteration (index of the next list entry; -1 entries and the last one read past the end of
   the buffer = 0 and are never used).
Every arithmetic statement of the shipped program is kept verbatim; asserts check every rewritten pattern.
"""
import os, re, subprocess, sys
HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.join(HERE, "..", "..")
ORIG = os.path.join(ROOT, "re", "scripts_gpu", "out", "RefProbe_414.ll")
OUT = os.path.join(HERE, "out", "rp414_src.hlsl")
COLLAPSE = "475:8,1662:8,2856:8,3266:8,3501:8,3881:8,4291:8,4524:8,4753:8,5062:8,5291:8"
IFCONV = ["v1932"]
PREFETCH = "--prefetch" in sys.argv     # off: s2 is already issue-bound in the model (see notes), +8 VGPR
STMT = re.compile(r"^\s*(// bb \d+|v\d+ = [^;]*;)$")
RESOURCE = re.compile(r"\b(oct_probe_info|oct_ref_table|sky_occ|uOutTex1|ldsTraceResult|a\d+)\[|\.(Sample\w*|Load|GetDimensions)\(")


def block(lines, i):
    """lines[i] == '{' -> index of the matching '}'"""
    assert lines[i].strip() == "{", lines[i]
    d = 0
    for j in range(i, len(lines)):
        s = lines[j].strip()
        d += s == "{"
        d -= s == "}"
        if d == 0:
            return j
    raise ValueError("unbalanced")


def ifconv(text, cond):
    lines = text.split("\n")
    hits = [i for i, l in enumerate(lines) if l.strip() == "if (%s)" % cond]
    assert len(hits) == 1, (cond, len(hits))
    i = hits[0]
    ind = lines[i][:len(lines[i]) - len(lines[i].lstrip())]
    t0 = i + 1
    t1 = block(lines, t0)
    assert lines[t1 + 1].strip() == "else"
    e0 = t1 + 2
    e1 = block(lines, e0)
    arms = []
    for a, b in ((t0 + 1, t1), (e0 + 1, e1)):
        body = lines[a:b]
        for l in body:
            assert STMT.match(l) and not RESOURCE.search(l), "%s: not arithmetic-only: %r" % (cond, l)
        arms.append([l.strip() for l in body])
    defs = [{re.match(r"(v\d+) =", s).group(1): s for s in arm if not s.startswith("//")} for arm in arms]
    phis = [v for v in defs[0] if v in defs[1]]
    assert phis, cond
    out = []
    for arm, d in zip(arms, defs):
        out += [ind + s for s in arm if s.startswith("//") or re.match(r"(v\d+) =", s).group(1) not in phis]
    for v in phis:
        tv = defs[0][v].split("=", 1)[1].strip().rstrip(";")
        ev = defs[1][v].split("=", 1)[1].strip().rstrip(";")
        out.append("%s%s = %s ? %s : %s;   // if-converted (mk414)" % (ind, v, cond, tv, ev))
    return "\n".join(lines[:i] + out + lines[e1 + 1:])


ALL_OFFS = {0: "v1796", 1: "v1799", 2: "v1802", 4: "v1805", 6: "v1885", 7: "v1889", 8: "v1893", 9: "v1897"}
PF_OFFS = {c: a for c, a in ALL_OFFS.items()
           if c in [int(x) for x in os.environ.get("MK414_PF", "0,1,2,4,6,7,8,9").split(",") if x]}


def prefetch1789(text):
    """loop 1789 walks the 8-entry probe list a42[] (= v1732..v1739); per probe it reads oct_probe_info[10*i + c]
    for c in PF_OFFS, then two depth samples and 9 sky-occlusion values whose addresses need those reads. Now the
    reads of list entry k+1 are issued at the top of iteration k (pn*) and handed over on the back edge (pf*), so
    an iteration's samples only wait for reads issued one iteration earlier. Same buffer, same addresses, read-only
    SRV -> same values. Entry k+1 = a42[min(k+1, 7)]: the back edge is only taken for k+1 < 8, where it is exactly
    the next v1790; a -1 entry (no probe) reads past the end of the buffer (0) and is never used."""
    lines = text.split("\n")
    h = [i for i, l in enumerate(lines) if l.strip() == "v1790 = v1732;"]
    assert len(h) == 1
    i = h[0]
    ind = lines[i][:len(lines[i]) - len(lines[i].lstrip())]
    assert [l.strip() for l in lines[i + 1:i + 7]] == ["v1791 = 0.0f;", "v1792 = 0.0f;", "v1793 = 0u;", "_f3 = false;",
                                                        "[loop] for (;;)", "{"], lines[i + 1:i + 7]
    pre = [ind + "uint pfB = v1732 * 10u;   // mk414 prefetch: list entry 0's probe info"]
    pre += [ind + "float pf%d = oct_probe_info[pfB + %du];" % (c, c) for c in PF_OFFS]
    lines[i + 1:i + 1] = pre
    j = i + 1 + len(pre) + 6           # first line of the loop body
    ind2 = lines[j][:len(lines[j]) - len(lines[j].lstrip())]
    top = [ind2 + "uint pfN = a42[min(v1793 + 1u, 7u)] * 10u;   // mk414 prefetch: list entry k+1's probe info"]
    top += [ind2 + "float pn%d = oct_probe_info[pfN + %du];" % (c, c) for c in PF_OFFS]
    lines[j:j] = top
    text = "\n".join(lines)
    # the loop's reads -> the prefetched values (each address v1796 + c is read once, 6/7/8 twice)
    for c, a in PF_OFFS.items():
        pat = re.compile(r"^(\s*)(v\d+) = oct_probe_info\[%s\];$" % a, re.M)
        n = len(pat.findall(text))
        assert n == (2 if c in (6, 7, 8) else 1), (c, a, n)
        text = pat.sub(lambda m: "%s%s = pf%d;" % (m.group(1), m.group(2), c), text)
    # back edge: hand the prefetched entry over
    old = "v1790 = v2103;"
    assert text.count(old) == 1
    k = text.index(old)
    ls = text.rfind("\n", 0, k) + 1
    ind3 = text[ls:k]
    text = text[:k] + old + "".join("\n%spf%d = pn%d;" % (ind3, c, c) for c in PF_OFFS) + text[k + len(old):]
    assert "v2103 = a42[v2099];" in text
    return text


def main():
    tmp = OUT + ".col.hlsl"
    subprocess.run([sys.executable, "-I", os.path.join(HERE, "ll2hlsl.py"), ORIG, tmp, "--collapse", COLLAPSE, "--cdpred"],
                   check=True)
    text = open(tmp).read()
    os.remove(tmp)
    for c in IFCONV:
        text = ifconv(text, c)
    if PREFETCH:
        text = prefetch1789(text)
    text = "// generated by gpu-shaders/gi/mk414.py from RefProbe_414.ll - do not edit by hand\n" + text
    open(OUT, "w", newline="\n").write(text)
    print("wrote", OUT)


if __name__ == "__main__":
    main()
