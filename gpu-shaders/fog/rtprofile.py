r"""rtprofile.py - where do a shader's dependent memory round trips go? (in-order timing model, realistic scenes)

    py -I rtprofile.py X.ll [--seeds 0:4] [--orig ORIG.ll] [--top 25] [--lm 1]

Runs every thread of the realistic VFSCAT scenes with the interpreter's in-order timing (load = --lm, ALU = 0) and
charges each increase of the control clock to the basic block in which it happened; prints the blocks with the
largest share (block name = DXIL label, i.e. `// bb N` in the decompiled HLSL) and their execution counts.
"""
import argparse, os, sys
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(os.path.dirname(HERE), "gi"))
import dxil_interp as D, scenes  # noqa: E402


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("ll"); ap.add_argument("--seeds", default="0:4"); ap.add_argument("--orig", default=None)
    ap.add_argument("--top", type=int, default=25); ap.add_argument("--lm", type=float, default=1.0)
    a = ap.parse_args()
    text = open(a.ll).read()
    otext = open(a.orig).read() if a.orig else text
    sh = D.Shader(text, "x", timing=(a.lm, 0.0, True))
    lo, hi = map(int, a.seeds.split(":"))
    charge, cnt = {}, {}
    nthr, tot = 0, 0.0
    for seed in range(lo, hi):
        sc, groups = scenes.vfscat_scene(otext, seed, realistic=True)
        X = D.Ctx(sc)
        nt = sh.numthreads
        for g in groups:
            lds = {n: ([0] * c if t != "float" else [0.0] * c) for n, (c, t) in sh.lds_globals.items()}
            F = sh._build(X, sh.consts, lds, lambda p: None)
            for tz in range(nt[2]):
                for ty in range(nt[1]):
                    for tx in range(nt[0]):
                        X.gid = g; X.gtid = (tx, ty, tz)
                        X.tid = (g[0] * nt[0] + tx, g[1] * nt[1] + ty, g[2] * nt[2] + tz)
                        X.flat = tx + nt[0] * (ty + nt[1] * tz); X.who = X.tid
                        R = [None] * sh.nregs
                        for r, n, t in sh.alloca_init:
                            R[r] = [0.0] * n if t == "float" else [0] * n
                        T = [0.0] * sh.nregs
                        S = [0.0, 0, 0.0, 0, 0.0]
                        M = {}
                        b = 0
                        while b >= 0:
                            s0 = S[0]
                            name = sh.m.blocks[b].name
                            b2 = F[b](R, T, S, M)
                            charge[name] = charge.get(name, 0.0) + (S[0] - s0)
                            cnt[name] = cnt.get(name, 0) + 1
                            b = b2
                        nthr += 1
                        tot += max(S[0], S[2])
    print("%d threads, %.1f round trips/thread (in-order, control clock incl. final store)" % (nthr, tot / nthr))
    for name, c in sorted(charge.items(), key=lambda kv: -kv[1])[:a.top]:
        print("  bb %-6s %6.2f rt/thread  (%5.1f %%)  executed %.2f x/thread" % (name, c / nthr, 100 * c / tot, cnt[name] / nthr))


if __name__ == "__main__":
    main()
