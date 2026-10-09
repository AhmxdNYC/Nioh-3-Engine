r"""mkvc.py - VCLOUD (shader_dx12\0x5f17ca9d.file record 7, CS b76874bb) restructure, from the mechanical decompile.

    py -I mkvc.py IN.hlsl OUT.hlsl [--vk SETS] [--lit]

--vk SETS  move the listed cbuffer constants that the two ray-march loops read into VGPRs, once, right before each
           loop: `K = asfloat(asuint(cb) ^ vz)` with vz = DTid.x >> 31 (always 0 at run time: DTid.x < surface width,
           so every K is bit-identical to its constant; the compiler cannot prove vz == 0, so K lives in a VGPR).
           The shipped binary needs ~90 SGPRs per light-march step (3 T# + 3 S# + ~54 dwords) against a ~102 limit,
           so amdxc re-loads constants/descriptors before nearly every use: 506 s_buffer_load / 514 full
           lgkmcnt(0) waits for 81 samples on gfx1102. Arithmetic is unchanged (same operands, VGPR vs SGPR).
           SETS = comma list of groups: proj (mW2P rows 0-2, g_mtP2W x/z, g_vW2M = mask-mip projection),
           mask (g_vMaskThreshold, slice id), base (g_vBaseTex*, sampling factor), detail (g_vDetailTex*, mip params),
           misc (erode, clamp), all.
"""
import re, sys

GROUPS = {
    "proj": [r"mW2P\[[012]\]\.[xyzw]", r"g_mtP2W\[[0-3]\]\.[xz]", r"g_vW2M\[[01]\]\.[xyzw]"],
    "mask": [r"g_vMaskThreshold\[\d\]\.[xyzw]", r"g_vMaskTexSliceID\[\d\]"],
    "base": [r"g_vBaseTexScale\[\d\]\.[xyz]", r"g_vBaseTexOffset\[\d\]\.[xyz]", r"g_fBaseSamplingScaleFactor\[\d\]"],
    "detail": [r"g_vDetailTexScale\[\d\]\.[xyz]", r"g_vDetailTexOffset\[\d\]\.[xyz]", r"g_vDetailMipParams\.[xyzw]"],
    "misc": [r"g_vErodeScale\[\d\]\.[xy]", r"g_vClampValue\.[yz]"],
}


def loop_ranges(L):
    """(start, end) line indices of every `[loop] for (;;)` body, by brace matching."""
    out = []
    for i, l in enumerate(L):
        if "[loop] for (;;)" in l:
            d = 0; j = i + 1
            while True:
                d += L[j].count("{") - L[j].count("}")
                if d == 0 and "}" in L[j]:
                    break
                j += 1
            out.append((i, j))
    return out


def main():
    src, dst = sys.argv[1], sys.argv[2]
    sets = []
    if "--vk" in sys.argv:
        g = sys.argv[sys.argv.index("--vk") + 1].split(",")
        if "all" in g:
            g = list(GROUPS)
        for k in g:
            sets += GROUPS[k]
    pat = re.compile(r"^(\s*)(v\d+) = (asuint\()?(" + "|".join("(?:%s)" % s for s in sets) + r")(\))?;$") if sets else None
    L = open(src, encoding="utf-8").read().split("\n")
    # vz after the early return (first `// bb 28`-style block after `return;`)
    for i, l in enumerate(L):
        if l.strip() == "return;":
            j = i
            while not L[j].strip().startswith("// bb"):
                j += 1
            L[j] = L[j] + "\n" + re.match(r"^(\s*)", L[j]).group(1) + "uint vz = DTid.x >> 31u;   // 0 at run time"
            break
    ins = {}
    n = 0
    if pat:
        for (a, b) in loop_ranges(L):
            seen = {}
            for i in range(a, b + 1):
                m = pat.match(L[i])
                if not m:
                    continue
                ind, v, au, c, _ = m.groups()
                key = (bool(au), c)
                if key not in seen:
                    seen[key] = "K%d" % n; n += 1
                L[i] = "%s%s = %s;" % (ind, v, seen[key])
            ind = re.match(r"^(\s*)", L[a]).group(1)
            decl = []
            for (isu, c), k in seen.items():
                if isu:
                    decl.append("%suint %s = asuint(%s) ^ vz;" % (ind, k, c))
                else:
                    decl.append("%sfloat %s = asfloat(asuint(%s) ^ vz);" % (ind, k, c))
            ins[a] = decl
    out = []
    for i, l in enumerate(L):
        if i in ins:
            out += ins[i]
        out.append(l)
    open(dst, "w", encoding="utf-8").write("\n".join(out))
    print("%s: %d VGPR constants" % (dst, n))


if __name__ == "__main__":
    main()
