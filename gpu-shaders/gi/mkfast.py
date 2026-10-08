r"""mkfast.py - build a GI/fog replacement from the shipped DXIL, reproducibly.

    py -I mkfast.py rp121|rp414|vf235 [--plain]

1. ll2hlsl.py decompiles re\scripts_gpu\out\<orig>.ll with the item's --collapse loops (full unroll + if-conversion:
   the probe-corner / light loops become straight-line code, so dxc and the driver can issue every independent load
   of the 8 corners up front = memory-level parallelism, and the loop-carried phis/branches disappear).
2. The item's PRECISE locals are pinned (see pin(): an exact-identity FMad, or `precise` as a fallback): dxc's
   fast-math would otherwise reassociate a few products/sums differently from the shipped binary (found by
   find_precise.py's trace diff); pinning only stops that reassociation, the arithmetic is the shipped one.
3. dxc (RDTS, validator 1.5, -O3 -Zs, same as build_sweep.py) -> out\<item>_fast.dxil + .ll (dumpbin).
--plain skips step 1's collapse (pure round trip, the baseline for proofs/timing).
"""
import os, re, subprocess, sys
HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.join(HERE, "..", "..")
ORIG = os.path.join(ROOT, "re", "scripts_gpu", "out")
DXC = os.path.join(ROOT, "tools", "rdts", "RadeonDeveloperToolSuite-2026-05-28-1806", "utils", "dx12", "dxc", "dxc.exe")
OUT = os.path.join(HERE, "out")

ITEMS = {
    "rp121": dict(orig="RefProbe_121.ll", collapse="325:8,413:8,725:8",
                  precise="v1311,v1312,v1313,v1317,v1322,v1324,v1326,v1327,v1359,v1361,v1363,v1435,v1699,v1937,v1983,v1984,v1985,v1986,v1987,v1988,v1989,v1990,v1991,v1992,v1993,v1994,v1995,v1996,v1997,v1998,v1999,v2000,v2001,v2002,v2003,v2004,v2005,v2006,v2007,v2008,v2009,v2040,v2041,v2042,v2044,v2054,v2058,v2071,v2081,v2089,v2091,v2099,v2101,v2109,v2214,v2215,v2330,v2331,v2569,v2615,v2616,v2617,v2618,v2619,v2620,v2621,v2622,v2623,v2624,v2625,v2626,v2627,v2628,v2629,v2630,v2631,v2632,v2633,v2634,v2635,v2636,v2637,v2638,v2639,v2640,v2641,v2671,v2672,v2673,v2674,v2675,v2686,v2690,v2703,v2713,v2719,v2721,v2723,v2729,v2731,v2733,v2741,v2815"),
    "rp414": dict(orig="RefProbe_414.ll", collapse="475:8,1662:8,1789:8,2856:8,3266:8,3501:8,3881:8,4291:8,4524:8,4753:8,5062:8,5291:8", precise=""),
    # vf235: pins are max(x, x) - exact for every x incl. NaN; dxc keeps the FMax so it cannot reassociate through
    # it, while Mesa NIR folds fmax(a,a) -> fcanonicalize(a) -> a, so RADV/ACO sees (and FMA-fuses) the shipped
    # expression DAG. Divisions are pinned too. Source: the hand-restructured fog\vf235_src.hlsl (--plain = decompile)
    "vf235": dict(orig="VFSCAT_235.ll", collapse="", pin_lo="self", pin_div=True,
                  precise=open(os.path.join(HERE, "..", "fog", "vf235_pins.txt")).read().strip(),
                  src=os.path.join(HERE, "..", "fog", "vf235_src.hlsl")),
}


BIN = re.compile(r"^(\s*)(v\d+) = (\S+) ([-+*]) (\S+);$")
BINDIV = re.compile(r"^(\s*)(v\d+) = (\S+) ([-+*/]) (\S+);$")


def pin(text, names, use_precise=False, lo="-3.402823466e+38f", div=False):
    """keep dxc from reassociating THROUGH each named value: a plain `v = a op b;` becomes an FMad with an exact
    identity operand (a*b -> mad(a,b,-0.0); a+b -> mad(a,1,b); a-b -> mad(b,-1,a)): IEEE-identical to the shipped op
    (also when the driver fuses it), but an intrinsic call that LLVM's reassociation cannot look through, so it
    costs ~nothing. Any other definition (saturate(x), c ? a : b, ...) pins its binary-op operands instead; only
    when there are none does it fall back to `precise` (which also blocks FMA contraction and propagates backwards
    through the whole expression tree, so it is the expensive last resort)."""
    names = [n for n in names.split(",") if n]
    BINRE = BINDIV if div else BIN
    lines = text.split("\n")
    defs = {}
    for i, l in enumerate(lines):
        m = re.match(r"^\s*(v\d+) = (.*);$", l)
        if m:
            defs.setdefault(m.group(1), []).append(i)
    extra = []
    for n in names:
        if n in defs and not any(BINRE.match(lines[i]) for i in defs[n]):
            ops = [o for i in defs[n] for o in re.findall(r"\bv\d+\b", lines[i].split("=", 1)[1])]
            ops = [o for o in ops if o in defs and any(BINRE.match(lines[i]) for i in defs[o])]
            if ops:
                extra += ops
                names = [x for x in names if x != n]
    for o in extra:
        if o not in names:
            names.append(o)
    rest = []
    for n in names:
        hit = False
        for i, l in enumerate(lines):
            m = BINRE.match(l)
            if not m or m.group(2) != n:
                continue
            ind, _, a, op, b = m.groups()
            if lo == "self":     # max(x, x): exact identity (NaN too); dxc keeps the FMax, NIR folds it away
                e = "max(%s %s %s, %s %s %s)" % (a, op, b, a, op, b)
            else:
                e = "max(%s %s %s, %s)" % (a, op, b, lo)
            lines[i] = "%s%s = %s;" % (ind, n, e)
            hit = True
        if not hit or use_precise:
            rest.append(n)
    return mark_precise("\n".join(lines), ",".join(rest))


def mark_precise(text, names):
    names = [n for n in names.split(",") if n]
    for n in names:
        pat = re.compile(r"^(\s*)float ([^;]*\b%s\b[^;]*);" % re.escape(n), re.M)
        m = pat.search(text)
        assert m, "declaration of %s not found" % n
        rest = [x.strip() for x in m.group(2).split(",") if x.strip() != n]
        rep = m.group(1) + "precise float %s;" % n
        if rest:
            rep = m.group(1) + "float %s;\n" % ", ".join(rest) + rep
        text = text[:m.start()] + rep + text[m.end():]
    return text


def build(item, plain=False, collapse=None, precise=None, tag="fast"):
    it = ITEMS[item]
    col = it["collapse"] if collapse is None else collapse
    pre = it["precise"] if precise is None else precise
    stem = os.path.join(OUT, "%s_%s" % (item, tag))
    if it.get("src") and not plain:
        # hand-restructured source (fog/vf235_src.hlsl): the pins are applied to a copy, like a decompile
        open(stem + ".hlsl", "w", newline=chr(10)).write(open(it["src"]).read())
    else:
        cmd = [sys.executable, "-I", os.path.join(HERE, "ll2hlsl.py"), os.path.join(ORIG, it["orig"]), stem + ".hlsl"]
        if col and not plain:
            cmd += ["--collapse", col]
        subprocess.run(cmd, check=True)
    src = open(stem + ".hlsl").read()
    if pre:
        txt = pin(src, pre, lo=it.get("pin_lo", "-3.402823466e+38f"), div=it.get("pin_div", False))
        open(stem + ".hlsl", "w", newline="\n").write(txt)
    r = subprocess.run([DXC, "-T", "cs_6_0", "-E", "main", "-O3", "-validator-version", "1.5", "-Zs", "-Fd", OUT + os.sep,
                        "-Fo", stem + ".dxil", stem + ".hlsl"], capture_output=True, text=True)
    if r.returncode:
        print(r.stdout[-3000:], r.stderr[-3000:])
        raise SystemExit("compile failed")
    for f in os.listdir(OUT):
        if f.endswith(".pdb"):
            os.remove(os.path.join(OUT, f))
    ll = subprocess.run([DXC, "-dumpbin", stem + ".dxil"], capture_output=True, text=True, check=True).stdout
    open(stem + ".ll", "w", newline="\n").write(ll)
    return stem


if __name__ == "__main__":
    a = sys.argv[1:]
    print(build(a[0], plain="--plain" in a, tag="dec" if "--plain" in a else "fast"))
