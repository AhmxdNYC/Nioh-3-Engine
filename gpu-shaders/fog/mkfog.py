r"""mkfog.py - build fog\vf235_src.hlsl (the latency rewrite of VFSCAT #235) from the mechanical decompile.

    py -I mkfog.py            # gi\out\vf235_dec.hlsl (ll2hlsl of the shipped DXIL) -> fog\vf235_src.hlsl
    py -I ..\gi\mkfast.py vf235   # then: pins + dxc -> gi\out\vf235_fast.{hlsl,dxil,ll}

Every arithmetic statement of the shipped program is kept verbatim (same SSA names, same operands, same order of
operations); only WHERE loads are issued and how often loops iterate changes:

1. Parallel light (sun): the three per-froxel samples (A z+0.5, B z+0.167, C z+0.833) each ran a virtual-shadow-map
   clip-level search loop over ALL levels start..count-1 (two dependent round trips per level: a per-lane constant
   buffer row fetch, then the page-flag fetch). Now ONE merged loop walks the levels once for all three samples and
   stops as soon as each sample has its level. The shipped loop keeps min(level) over valid levels in ascending
   order, i.e. the first valid one, so stopping there is exact. The final (x,y,z) of the found level are the loop's
   own values (the shipped code recomputes the same expressions after the loop). Cascade-map samples and the three
   final VSM samples are issued before any of them is consumed.
2. Clustered point/spot lights: software-pipelined list walk. Light k+1's position (spot: position+range) and light
   k+2's index are fetched one/two iterations ahead; light k's remaining attributes are fetched at the top of its
   iteration (before the culling tests, speculatively). Out-of-list prefetches read the next cell's data or past the
   end of the buffer (returns 0) and are never used.
3. Tail (LPV, fog noise, temporal reprojection): the reprojection sample and the six LPV samples are issued before
   the noise lookups and consumed after them.
The script asserts that every renamed template equals the corresponding shipped (decompiled) text.
"""
import os, re, subprocess, sys
HERE = os.path.dirname(os.path.abspath(__file__))
GI = os.path.join(os.path.dirname(HERE), "gi")
DEC = os.path.join(GI, "out", "vf235_dec.hlsl")
OUT = os.path.join(HERE, "vf235_src.hlsl")
ORIG = os.path.join(GI, "..", "..", "re", "scripts_gpu", "out", "VFSCAT_235.ll")

POS = {"A": ("v139", "v143", "v147", "v151"), "B": ("v296", "v300", "v304", "v308"), "C": ("v329", "v333", "v337", "v341")}


def ren(text, lo, hi, off, smp, extra=None):
    """rename vN (lo <= N <= hi) to v(N+off); positions A -> smp; extra = {old: new} exact names."""
    pa = dict(zip(POS["A"], POS[smp]))
    ex = dict(extra or {})

    def f(m):
        n = m.group(0)
        if n in ex:
            return ex[n]
        if n in pa:
            return pa[n]
        k = int(n[1:])
        return "v%d" % (k + off) if lo <= k <= hi else n
    return re.sub(r"\bv\d+\b", f, text)


def stmts(text):
    return [l.strip() for l in text.split("\n") if l.strip() and not l.strip().startswith("//") and l.strip() not in ("{", "}", "else")]


class Src:
    def __init__(self, text):
        self.L = text.split("\n")

    def find(self, s, start=0):
        hits = [i for i in range(start, len(self.L)) if self.L[i].strip() == s]
        assert hits, "line not found: %r" % s
        return hits[0]

    def between(self, a, b, start=0):
        """statement text strictly after line `a` up to and including line `b` (dedented)."""
        i = self.find(a, start)
        j = self.find(b, i)
        return dedent(self.L[i + 1:j + 1])


def dedent(ls):
    ind = min((len(l) - len(l.lstrip()) for l in ls if l.strip()), default=0)
    return "\n".join(l[ind:] for l in ls)


def indent(text, n):
    return "\n".join((" " * n + l) if l.strip() else l for l in text.split("\n"))


def block_end(L, i):
    """index of the '}' closing the block whose '{' is at line i+1 (i = the if/for line)."""
    assert L[i + 1].strip() == "{", L[i + 1]
    d = 0
    for j in range(i + 1, len(L)):
        t = L[j].strip()
        if t == "{":
            d += 1
        elif t == "}":
            d -= 1
            if d == 0:
                return j
    raise ValueError(i)


def ifelse_end(L, i):
    j = block_end(L, i)
    if j + 1 < len(L) and L[j + 1].strip() == "else":
        return block_end(L, j + 1)
    return j


# ------------------------------------------------------------------------------------------- 1. parallel light
def parallel_section(S):
    # cascade-map coordinates (A as shipped; B/C checked against the shipped text)
    casc = {"A": S.between("// bb 511", "v557 = v556 || v555;")}
    casc_orig = {"B": S.between("v772 = min(v577, v767);", "v798 = v797 || v796;"),
                 "C": S.between("v1011 = v1010 + v772;", "v1037 = v1036 || v1035;")}
    for s, off in (("B", 241), ("C", 480)):
        casc[s] = ren(casc["A"], 532, 577, off, s)
        assert casc[s] == casc_orig[s], s
    issue_t = """if (!(v557))
{
    v559 = v548 * v528;
    v560 = v549 * v529;
    v561 = v559 + v530;
    v562 = v560 + v531;
    v563 = sParallelShadowMap.SampleLevel(__smpsParallelShadowMap, float2(v561, v562), 0.0f);
}"""
    cons_t = """if (v557)
{
    v577 = 1.0f;
}
else
{
    v564 = v563.x;
    if (v502)
    {
        v566 = v359 * -1.442695f;
        v567 = v566 * v550;
        v568 = exp2(v567);
        v569 = v564 * 1.2f;
        v570 = v569 * v568;
        v571 = v570 + -0.2f;
        v572 = saturate(v571);
        v577 = v572;
    }
    else
    {
        v574 = v564 > v550;
        v575 = v574 ? 1.0f : 0.0f;
        v577 = v575;
    }
}"""
    a_if = S.find("if (v557)")
    orig_a = dedent(S.L[a_if:ifelse_end(S.L, a_if) + 1])
    assert sorted(stmts(issue_t + "\n" + cons_t)) == sorted(stmts(orig_a) + ["if (!(v557))"]), "cascade A"
    for s, off, flag in (("B", 241, "v798"), ("C", 480, "v1037")):
        i = S.find("if (%s)" % flag)
        o = dedent(S.L[i:ifelse_end(S.L, i) + 1])
        assert sorted(stmts(ren(issue_t + "\n" + cons_t, 532, 577, off, s))) == sorted(stmts(o) + ["if (!(%s))" % flag]), s
    issue = {s: ren(issue_t, 532, 577, off, s) for s, off in (("A", 0), ("B", 241), ("C", 480))}
    cons = {s: ren(cons_t, 532, 577, off, s) for s, off in (("A", 0), ("B", 241), ("C", 480))}

    # VSM start levels (bb 585 / 823 / 1062)
    start = {"A": S.between("// bb 585", "v614 = v613 < v579;")}
    body = {"A": S.between("// bb 616", "v686 = v669 && v685;")}
    fin = {"A": S.between("// bb 733", "v765 = min(1.0f, v764);")}
    for s, off, m1, m2, m3 in (("B", 238, "// bb 823", "// bb 854", "// bb 971"), ("C", 477, "// bb 1062", "// bb 1093", "// bb 1210")):
        start[s] = ren(start["A"], 587, 614, off, s)
        assert start[s] == S.between(m1, ren("v614 = v613 < v579;", 587, 614, off, s)), "start " + s
        body[s] = ren(body["A"], 619, 687, off, s, {"v618": "v%d" % (618 + off)})
        assert body[s] == S.between(m2, ren("v686 = v669 && v685;", 619, 687, off, s)), "body " + s
        fin[s] = ren(fin["A"], 732, 771, off, s, {"v728": "v%d" % (728 + off), "v729": "v%d" % (729 + off),
                                                   "v730": "v%d" % (730 + off), "v731": "v%d" % (731 + off)})
        assert fin[s] == S.between(m3, ren("v765 = min(1.0f, v764);", 732, 771, off, s)), "final " + s
    # z of the level's shadow coordinate: the shipped post-loop code (bb 693) on the found level
    zA = S.between("v694 = v688 << 2u;", "v726 = mad(v151, v714, v725);")
    zl = [l for l in zA.split("\n") if re.match(r"^(v699|v704|v709|v714) = mW2S_VirtualSM\[v688\]\[\d\]\.z;$", l)
          or re.match(r"^v72[3-6] = ", l)]
    assert len(zl) == 8, zl
    z_t = "\n".join(zl).replace("[v688]", "[vlv]")
    zs = {s: ren(z_t, 699, 726, off, s) for s, off in (("A", 0), ("B", 238), ("C", 477))}
    # loop body per sample: level variable -> vlv
    lvl = {"A": "v618", "B": "v856", "C": "v1095"}
    bod = {s: (body[s] + "\n" + zs[s]).replace(lvl[s], "vlv") for s in "ABC"}
    bod = {s: re.sub(r"\b%s\b" % lvl[s], "vlv", body[s] + "\n" + zs[s]) for s in "ABC"}
    # split each final block into load issue / consume
    def split_final(t, off):
        n = lambda k: "v%d" % (k + off)   # noqa: E731
        cons_names = {n(759), n(760), n(762), n(763), n(764), n(765)}
        iss, con = [], []
        for l in t.split("\n"):
            m = re.match(r"^(v\d+) = ", l)
            (con if m and m.group(1) in cons_names else iss).append(l)
        return "\n".join(iss), "\n".join(con)
    fi, fc = {}, {}
    for s, off in (("A", 0), ("B", 238), ("C", 477)):
        fi[s], fc[s] = split_final(fin[s], off)

    def V(k, s):
        return "v%d" % (k + {"A": 0, "B": 238, "C": 477}[s])
    out = []
    out += [casc["A"], casc["B"], casc["C"], issue["A"], issue["B"], issue["C"]]
    out.append("""v579 = asuint(nArraySize_VirtualSM);
v580 = asint(v579) > asint(0u);
if (v580)
{
    v582 = v249 == 0.0f;
    v583 = v249 == 1.0f;
    v584 = v582 || v583;
    if (v584)
    {
%(skip)s
    }
    else
    {
%(starts)s
        // merged clip-level search for the three samples: first valid level >= each sample's start
        vfa = v579;
        vfb = v579;
        vfc = v579;
        vxa = 0.0f; vya = 0.0f; vza = 0.0f; vxb = 0.0f; vyb = 0.0f; vzb = 0.0f; vxc = 0.0f; vyc = 0.0f; vzc = 0.0f;
        vlv = min(min(v614 ? v613 : v579, v852 ? v851 : v579), v1091 ? v1090 : v579);
        [loop] while (vlv < v579)
        {
%(bodies)s
            if (v614 && vlv >= v613 && vfa == v579 && v686)
            {
                vfa = vlv; vxa = v639; vya = v643; vza = v726;
            }
            if (v852 && vlv >= v851 && vfb == v579 && v924)
            {
                vfb = vlv; vxb = v877; vyb = v881; vzb = v964;
            }
            if (v1091 && vlv >= v1090 && vfc == v579 && v1163)
            {
                vfc = vlv; vxc = v1116; vyc = v1120; vzc = v1203;
            }
            if (!((v614 && vfa == v579) || (v852 && vfb == v579) || (v1091 && vfc == v579)))
            {
                break;
            }
            vlv = vlv + 1u;
        }
%(tuples)s
%(issues)s
%(conss)s
    }
}
else
{
%(skip)s
}""" % dict(
        skip=indent("""v767 = 1.0f;
v768 = v353;
v769 = v354;
v770 = v355;
v771 = v352;
v1005 = 1.0f;
v1006 = v768;
v1007 = v769;
v1008 = v770;
v1009 = v771;
v1244 = 1.0f;
v1245 = v1006;
v1246 = v1007;
v1247 = v1008;
v1248 = v1009;""", 8).replace(" " * 8 + "v", " " * 8 + "v"),
        starts=indent("\n".join(start[s] for s in "ABC"), 8),
        bodies=indent("\n".join(bod[s] for s in "ABC"), 12),
        tuples=indent("\n".join(tuple_t(s, V) for s in "ABC"), 8),
        issues=indent("\n".join("if (%s)\n{\n%s\n}" % (V(732, s), indent(fi[s], 4)) for s in "ABC"), 8),
        conss=indent("\n".join("if (%s)\n{\n%s\n    %s = %s;\n}\nelse\n{\n    %s = 1.0f;\n}" % (
            V(732, s), indent(fc[s], 4), V(767, s), V(765, s), V(767, s)) for s in "ABC"), 8)))
    out += [cons["A"], cons["B"], cons["C"]]
    out.append("v772 = min(v577, v767);\nv1010 = min(v818, v1005);\nv1011 = v1010 + v772;")
    return "\n".join(out)


def tuple_t(s, V):
    run = {"A": "v614", "B": "v852", "C": "v1091"}[s]
    f = {"A": "vfa", "B": "vfb", "C": "vfc"}[s]
    x, y, z = ("vx" + f[2], "vy" + f[2], "vz" + f[2])
    fb = {"A": ("v353", "v354", "v355", "v352"), "B": ("v768", "v769", "v770", "v771"), "C": ("v1006", "v1007", "v1008", "v1009")}[s]
    t = [V(728, s), V(729, s), V(730, s), V(731, s)]
    return """if (%(run)s)
{
    %(found)s = %(f)s < v579;
    if (%(found)s)
    {
        %(t0)s = %(x)s;
        %(t1)s = %(y)s;
        %(t2)s = %(z)s;
        %(t3)s = %(f)s;
    }
    else
    {
        %(t0)s = %(fb0)s;
        %(t1)s = %(fb1)s;
        %(t2)s = %(fb2)s;
        %(t3)s = %(f)s;
    }
}
else
{
    %(t0)s = %(fb0)s;
    %(t1)s = %(fb1)s;
    %(t2)s = %(fb2)s;
    %(t3)s = %(fb3)s;
}
%(c0)s = %(t0)s;
%(c1)s = %(t1)s;
%(c2)s = %(t2)s;
%(c3)s = %(t3)s;
%(ok)s = asint(%(t3)s) < asint(v579);""" % dict(run=run, found=V(692, s), f=f, x=x, y=y, z=z, t0=t[0], t1=t[1], t2=t[2], t3=t[3],
                                                  fb0=fb[0], fb1=fb[1], fb2=fb[2], fb3=fb[3], c0=V(768, s), c1=V(769, s),
                                                  c2=V(770, s), c3=V(771, s), ok=V(732, s))


# ------------------------------------------------------------------------------------------- 2. clustered loop
PT_TOP = """v1565 = sPointLightInfo[v1433].sCommon.vColorScale;"""


PF = "idx"                       # "pos": index 2 ahead + position 1 ahead; "idx": index 1 ahead only
HOIST = {"sp1", "sp2"}       # which per-light attribute groups are fetched at the top of the iteration


def clustered(L):
    T = "\n".join(L)
    # (a) loop head: prefetch light 0 / 1 before the loop, rotate inside it
    head_old = """                        v1430 = v1388;
                        [loop] for (;;)
                        {
                            // bb 1425
                            v1431 = v1422 + v1427;
                            v1432 = tCllLightIndices[v1431];
                            v1433 = v1432.x;
"""
    assert T.count(head_old) == 1
    # (b) point attribute loads (bb 1564) and spot attribute loads (bb 1939, bb 2028) move to the top of the iteration
    pt_a = T.index("                                            v1565 = sPointLightInfo[v1433].sCommon.vColorScale;\n")
    pt_b = T.index("                                            v1600 = v1599.x;\n") + len("                                            v1600 = v1599.x;\n")
    pt = T[pt_a:pt_b]
    sp1_a = T.index("                                    v1940 = v1433 * 5u;\n")
    sp1_b = T.index("                                    v1964 = v1961.z;\n") + len("                                    v1964 = v1961.z;\n")
    sp1 = T[sp1_a:sp1_b]
    sp2_a = T.index("                                            v2029 = v1433 * 3u;\n")
    sp2_b = T.index("                                            v2068 = v2067.x;\n") + len("                                            v2068 = v2067.x;\n")
    sp2 = T[sp2_a:sp2_b]
    # spot: position/range come from the prefetch; direction/colour/attenuation rows are fetched at the top
    sp1_top = "\n".join(l.strip() for l in sp1.split("\n") if l.strip() and not re.match(
        r"^(v1941|v1942|v1943|v1944|v1945|v1946|v1947|v1948|v1949) = ", l.strip()))
    sp1_keep = """v1942 = vca.x;
v1943 = vca.y;
v1944 = vca.z;
v1947 = vca.w;
v1948 = vcb.y;
v1949 = vcb.z;"""
    pre = """// software-pipelined light list: light k+1 position (spot: position + range row) and light k+2 index are
// fetched ahead; light k's other attributes at the top of its iteration (speculative, before the culling tests)
vq0 = tCllLightIndices[v1422].x;
vq1 = tCllLightIndices[v1422 + 1u].x;
if (v1419)
{
    vqa = tCllLightPositions[vq0];
}
else
{
    vqs = vq0 * 5u;
    vqa = float4(tCllSptLightAttributes[vqs], tCllSptLightAttributes[vqs + 4u].x);
}"""
    top = """v1431 = v1422 + v1427;
v1433 = vq0;
vca = vqa;
vq0 = vq1;
vq1 = tCllLightIndices[v1431 + 2u].x;
if (v1419)
{
    vqa = tCllLightPositions[vq0];
%s
}
else
{
    vqs = vq0 * 5u;
    vqa = float4(tCllSptLightAttributes[vqs], tCllSptLightAttributes[vqs + 4u].x);
    vcb = tCllSptLightAttributes[v1433 * 5u + 4u];
%s
%s
}
""" % (indent(dedent(pt.split("\n")), 4) if "pt" in HOIST else "", indent(sp1_top, 4) if "sp1" in HOIST else "",
       indent(dedent(sp2.split("\n")), 4) if "sp2" in HOIST else "")
    if PF == "idx":
        # index-only pipeline: light k+1's index is fetched one iteration ahead; light k's position (spot: position
        # + range row) and all its other attributes are fetched together at the top of its iteration
        pre = """// software-pipelined light list: light k+1's index is fetched one iteration ahead; light k's position and
// attributes are all fetched at the top of its iteration (speculatively, before the culling tests)
vq0 = tCllLightIndices[v1422].x;"""
        top = """v1431 = v1422 + v1427;
v1433 = vq0;
vq0 = tCllLightIndices[v1431 + 1u].x;
if (v1419)
{
    vca = tCllLightPositions[v1433];
%s
}
else
{
    vcb = tCllSptLightAttributes[v1433 * 5u + 4u];
    vca = float4(tCllSptLightAttributes[v1433 * 5u], vcb.x);
%s
%s
}
""" % (indent(dedent(pt.split(chr(10))), 4) if "pt" in HOIST else "", indent(sp1_top, 4) if "sp1" in HOIST else "",
       indent(dedent(sp2.split(chr(10))), 4) if "sp2" in HOIST else "")
    head_new = "                        v1430 = v1388;\n" + indent(pre, 24) + "\n" + \
        "                        [loop] for (;;)\n                        {\n                            // bb 1425\n" + indent(top, 28) + "\n"
    if "sp2" in HOIST:
        T = T[:sp2_a] + T[sp2_b:]
    T = T[:sp1_a] + indent(sp1_keep + ("" if "sp1" in HOIST else "\n" + sp1_top), 36) + "\n" + T[sp1_b:]
    if "pt" in HOIST:
        T = T[:pt_a] + T[pt_b:]
    T = T.replace(head_old, head_new)
    old = "                                    v1489 = tCllLightPositions[v1433];\n"
    assert T.count(old) == 1
    T = T.replace(old, "                                    v1489 = vca;\n")
    return T.split("\n")


# ------------------------------------------------------------------------------------------- 3. tail
def tail(L):
    T = "\n".join(L)
    # reprojection: coordinates + sample issued before the LPV block, consumed where they were
    i_rep = T.index("            v2801 = gInjectionDesc.y;\n")
    i_if = T.index("                // bb 2803\n")
    i_2851 = T.index("                if (v2851)\n", i_if)
    i_cons = T.index("                // bb 2889\n")
    rep_pre = T[i_rep:i_if]                          # v2801, v2802, if (v2802) {
    rep_coords = T[i_if:i_2851]                      # bb 2803: matrix, v2840.. v2851
    rep_branch = T[i_2851:i_cons]                    # if (v2851) {...} else {... sample ...}
    Lb = rep_branch.split("\n")
    # split the else-branch: everything up to and including the sample is issued early
    k = [i for i, l in enumerate(Lb) if "sVolumeSrc.SampleLevel" in l]
    assert len(k) == 1
    k = k[0]
    e0 = [i for i, l in enumerate(Lb) if l.strip() == "// bb 2852"][0]
    issue_lines = Lb[e0 + 1:k + 1]
    cons_lines = Lb[k + 1:]
    early = rep_pre.replace("            if (v2802)\n            {\n", "") if False else None
    # build: early block
    early = ("            v2801 = gInjectionDesc.y;\n            v2802 = v2801 > 0.0f;\n            if (v2802)\n            {\n" +
             rep_coords + "                if (!(v2851))\n                {\n" + "\n".join(issue_lines) + "\n                }\n            }\n")
    late_branch = "\n".join(Lb[:e0 + 1] + cons_lines)
    # remove v2801/v2802 + bb 2803 coords from the original place; keep the branch (consume part) + blend
    head = T[i_rep:i_if]
    assert head.strip().split("\n")[0].strip() == "v2801 = gInjectionDesc.y;"
    T = T[:i_rep] + "            if (v2802)\n            {\n" + late_branch + T[i_cons:]
    # LPV: split bb 2460 into sample issue / consume
    i_lpv = T.index("            v2459 = v2458 != 0.0f;\n")
    i_lb = T.index("                // bb 2460\n", i_lpv)
    i_le = T.index("            // bb 2553\n", i_lb)
    lpv = T[i_lb:i_le].split("\n")
    # lpv = ['// bb 2460', stmts..., '}', 'else', '{', zeros..., '}', '']
    close = [i for i, l in enumerate(lpv) if l == "            }"][0]
    body_l = lpv[1:close]
    iss = [l for l in body_l if re.match(r"^\s*v(246[2-9]|247[0-4]|2483|2496|2497|2498|2507|2520|2521|2522|2531) = ", l)]
    con = [l for l in body_l if l not in iss]
    T = (T[:i_lb] + "                // bb 2460 (consume)\n" + "\n".join(con) + "\n" + "\n".join(lpv[close:]) + T[i_le:])
    lpv_issue = "            if (v2459)\n            {\n" + "\n".join(iss) + "\n            }\n"
    # insert: early reprojection + LPV issue right after `v2459 = v2458 != 0.0f;` and before its consume branch
    i_ins = T.index("            v2459 = v2458 != 0.0f;\n") + len("            v2459 = v2458 != 0.0f;\n")
    T = T[:i_ins] + early + lpv_issue + T[i_ins:]
    # the noise block sits after the LPV consume; move the LPV consume (if v2459 {...} else {...}) after the noise block
    i_c0 = T.index("            if (v2459)\n            {\n                // bb 2460 (consume)\n")
    i_c1 = T.index("            // bb 2553\n", i_c0)
    lpv_cons = T[i_c0:i_c1]
    T = T[:i_c0] + T[i_c1:]
    i_n0 = T.index("            // bb 2553\n")
    i_n1 = T.index("            v2570 = gFogNoiseInfo.w;\n", i_n0)
    part_scale = T[i_n0:i_n1]                        # v2557.. v2568 (uses v2554..)
    T = T[:i_n0] + T[i_n1:]
    i_after_noise = T.index("            // bb 2758\n")
    T = T[:i_after_noise] + lpv_cons + part_scale + T[i_after_noise:]
    return T.split("\n")


def main():
    if not os.path.exists(DEC):
        subprocess.run([sys.executable, "-I", os.path.join(GI, "ll2hlsl.py"), ORIG, DEC], check=True)
    S = Src(open(DEC).read())
    i0 = S.find("// bb 511")
    i1 = S.find("// bb 1243")
    ind = len(S.L[i0]) - len(S.L[i0].lstrip())
    skip = set(a for a in sys.argv[1:] if a.startswith("--no-"))       # experiments: --no-par --no-cl --no-tail
    for a in sys.argv[1:]:
        if a.startswith("--pf="):
            globals()["PF"] = a[5:]
        if a.startswith("--hoist="):                                    # --hoist=pt,sp1,sp2 (default: all)
            HOIST.clear()
            HOIST.update(x for x in a[8:].split(",") if x)
    L = S.L
    if "--no-par" not in skip:
        L = S.L[:i0] + indent(parallel_section(S), ind).split("\n") + S.L[i1:]
    if "--no-cl" not in skip:
        L = clustered(L)
    if "--no-tail" not in skip:
        L = tail(L)
    T = "\n".join(L)
    T = T.replace("    uint4 v38;\n", "    uint4 v38;\n    uint vlv, vfa, vfb, vfc, vq0, vq1, vqs;\n"
                  "    float vxa, vya, vza, vxb, vyb, vzb, vxc, vyc, vzc;\n    float4 vqa, vca;\n    float3 vcb;\n", 1)
    T = T.replace("// decompiled by gpu-shaders/gi/ll2hlsl.py from VFSCAT_235.ll - do not edit by hand, see the *_fast.hlsl",
                  "// VFSCAT #235 latency rewrite: generated by gpu-shaders/fog/mkfog.py from the decompile of the shipped DXIL", 1)
    open(OUT, "w", newline="\n").write(T)
    print("wrote", OUT, len(T.split("\n")), "lines")


if __name__ == "__main__":
    main()
