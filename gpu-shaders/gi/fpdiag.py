r"""fpdiag.py - first values of the ORIGINAL program that a rebuild never produces, for one thread (find_precise aid).

    py -I fpdiag.py NEW.ll SEED X Y [--kind refgeo] [--n 15]
"""
import argparse, os, sys
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import dxil_interp as D, find_precise as FP, mkfast  # noqa: E402

ap = argparse.ArgumentParser()
ap.add_argument("new"); ap.add_argument("seed", type=int); ap.add_argument("x", type=int); ap.add_argument("y", type=int)
ap.add_argument("--kind", default="refgeo"); ap.add_argument("--n", type=int, default=15); ap.add_argument("--realistic", action="store_true")
a = ap.parse_args()
FP.REAL = a.realistic
otext = open(os.path.join(mkfast.ORIG, "RefProbe_414.ll")).read()
so = D.Shader(otext, "orig"); sn = D.Shader(open(a.new).read(), "new")
A = FP.trace(so, a.kind, otext, a.seed, (a.x, a.y, 0))
B = FP.trace(sn, a.kind, otext, a.seed, (a.x, a.y, 0))
bits = lambda v: D.f2bits(v) if v == v else -1
setB = set(bits(v) for v, _ in B) | set(bits(-v) for v, _ in B)
val = {}
k = 0
for v, ins in A:
    val[ins.dest] = v
    if bits(v) in setB or ins.op == "phi":
        continue
    ops = []
    for o in ins.args:
        if getattr(o, "kind", None) == "reg" and isinstance(val.get(o.v), float):
            ops.append("%s%s" % (o.v, "" if bits(val[o.v]) in setB else "(MISSING)"))
    print("%s = %s %s  [%s]" % (ins.dest, ins.op, ins.extra.get("fn", ""), " ".join(ops)))
    k += 1
    if k >= a.n:
        break

if os.environ.get("FPD_OPS"):
    want = {}
    for v, ins in A:
        if ins.dest in os.environ["FPD_OPS"].split(","):
            want[bits(v)] = ins.dest
    print("orig values:", {k: "%s=%r" % (d, [v for v, i in A if i.dest == d][0]) for k, d in want.items()})
    bv = {}
    for v, ins in B:
        bv[ins.dest] = v
        args = [o.v for o in ins.args if getattr(o, "kind", None) == "reg"]
        hit = [want[bits(bv[x])] for x in args if isinstance(bv.get(x), float) and bits(bv[x]) in want]
        if hit:
            print("  new %s = %s %s %s -> %r  uses %s" % (ins.dest, ins.op, ins.extra.get("fn", ""), args, v, hit))

if os.environ.get("FPD_NEAR"):
    d = os.environ["FPD_NEAR"]
    ov = [(v, i) for v, i in A if i.dest == d]
    for v, i in ov[:3]:
        print("orig %s = %r  ops %s" % (d, v, [(o.v, val.get(o.v)) for o in i.args if getattr(o, "kind", None) == "reg"]))
        bv = {}
        for w, j in B:
            bv[j.dest] = w
            if isinstance(w, float) and w == w and v == v and 0 < D.ulp_diff(v, w) <= 8:
                print("   new %s = %s %r  ops %s" % (j.dest, j.op, w, [(o.v, bv.get(o.v)) for o in j.args if getattr(o, "kind", None) == "reg"]))
