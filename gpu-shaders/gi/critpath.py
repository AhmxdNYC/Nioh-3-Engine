r"""critpath.py - why is a block slow? dataflow arrival time (in round trips) of every memory read in one basic
block, for the first writing threads of a scene (in-order timing model, load = 1, ALU = 0).

    py -I critpath.py X.ll BLOCK [--orig ORIG.ll] [--kind refgeo] [--seed 0] [--threads 4]
"""
import argparse, os, sys
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import dxil_interp as D, scenes  # noqa: E402


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("ll"); ap.add_argument("block"); ap.add_argument("--orig", default=None)
    ap.add_argument("--kind", default="refgeo"); ap.add_argument("--seed", type=int, default=0)
    ap.add_argument("--threads", type=int, default=4)
    a = ap.parse_args()
    text = open(a.ll).read()
    otext = open(a.orig).read() if a.orig else text
    sh = D.Shader(text, "x", timing=(1.0, 0.0, True))
    sc, groups = scenes.make(a.kind, otext, a.seed, realistic=True)
    X = D.Ctx(sc)
    nt = sh.numthreads
    bi = [i for i, b in enumerate(sh.m.blocks) if b.name == a.block][0]
    blk = sh.m.blocks[bi]
    loads = [i for i in blk.instrs if i.op == "call" and any(k in i.extra["fn"] for k in ("bufferLoad", "textureLoad", "sample"))]
    shown = 0
    for g in groups:
        lds = {n: ([0] * c if t != "float" else [0.0] * c) for n, (c, t) in sh.lds_globals.items()}
        F = sh._build(X, sh.consts, lds, lambda p: None)
        for ty in range(nt[1]):
            for tx in range(nt[0]):
                X.gid = g; X.gtid = (tx, ty, 0)
                X.tid = (g[0] * nt[0] + tx, g[1] * nt[1] + ty, 0)
                X.flat = tx + nt[0] * ty; X.who = X.tid
                R = [None] * sh.nregs
                for r, n, t in sh.alloca_init:
                    R[r] = [0.0] * n if t == "float" else [0] * n
                T = [0.0] * sh.nregs
                S = [0.0, 0, 0.0, 0, 0.0]
                M = {}
                b = 0
                while b >= 0:
                    s0 = S[0]
                    b2 = F[b](R, T, S, M)
                    if b == bi:
                        ts = sorted((T[sh.regs[i.dest]] - s0, i.dest, i.extra["fn"].split(".")[2]) for i in loads)
                        print("thread %s: block entry %.0f, %d reads; arrival (rt after entry) histogram:" % (X.tid, s0, len(ts)))
                        h = {}
                        for t, d, f in ts:
                            h[t] = h.get(t, []) + ["%s:%s" % (d, f)]
                        for t in sorted(h):
                            print("   +%.0f: %d  %s" % (t, len(h[t]), " ".join(h[t][:6])))
                        shown += 1
                        if shown >= a.threads:
                            return
                    b = b2


if __name__ == "__main__":
    main()
