r"""perfmodel.py - latency model of a compute shader from its DXIL, using the reference interpreter's timing mode.

    py -I perfmodel.py --kind refprobe name=a.ll[:waves1103[:waves1201]] name2=b.ll[:...] --seeds 0:64

Per thread it measures, on the synthetic scenes (only threads that write output):
  rt    dependent memory round trips on the critical path (load = 1, ALU = 0; a block cannot start before the
        branch guarding it resolved, i.e. no load is hoisted above its guarding branch - what the GPU compilers do);
  cyc   the same critical path in cycles with load latency LM (default 500) and dependent-ALU latency LA (default 4);
  ins   dynamic DXIL instruction count (ALU issue work); mem = dynamic memory reads.
Per 64-thread wave: critical path = max over lanes, issue = max over lanes (optimistic for divergence).
SIMD estimate for a shader with W resident waves: t ~= max(C, W * I) / W per wave (latency hidden by the other
W-1 waves until issue-bound). The ratio of two such estimates is the predicted speed-up; it is a model, not a
measurement.
"""
import argparse, multiprocessing as mp, sys
import os as _os
sys.path.insert(0, _os.path.dirname(_os.path.abspath(__file__)))
import dxil_interp as D
import scenes


def one(args):
    path, kind, seed, lm, la = args
    text = open(path).read()
    sh = D.Shader(text, _os.path.basename(path), timing=(lm, la))
    sh1 = D.Shader(text, _os.path.basename(path), timing=(1.0, 0.0))
    otext = open(ORIG).read() if ORIG else text
    out = []
    for s, tm in ((sh, "cyc"), (sh1, "rt")):
        sc, groups = scenes.refprobe_scene(otext, seed, realistic=REAL) if kind == "refprobe" else scenes.vfscat_scene(otext, seed)
        X, _ = s.dispatch(sc, groups)
        out.append(X.times)
    writers = set()
    for k, r in sc.b.items():
        if k[0] == 1 and hasattr(r, "writes"):
            for w in r.writes:
                writers.add(w[0])
    return out, writers


ORIG = None
REAL = True


def main():
    global ORIG
    ap = argparse.ArgumentParser()
    ap.add_argument("items", nargs="+")
    ap.add_argument("--kind", default="refprobe")
    ap.add_argument("--seeds", default="0:48")
    ap.add_argument("--lm", type=float, default=500.0)
    ap.add_argument("--la", type=float, default=4.0)
    ap.add_argument("--orig", default=None, help=".ll whose header defines the scene layout (default: each item)")
    ap.add_argument("--jobs", type=int, default=10)
    ap.add_argument("--coverage-scenes", action="store_true", help="use the random coverage scenes, not the realistic ones")
    a = ap.parse_args()
    ORIG = a.orig
    lo, hi = (int(x) for x in a.seeds.split(":"))
    rows = []
    for it in a.items:
        name, rest = it.split("=", 1)
        parts = rest.split(":")
        path = parts[0] if not (len(parts) > 1 and len(parts[0]) == 1) else parts[0] + ":" + parts[1]
        tail = parts[1:] if path == parts[0] else parts[2:]
        waves = [int(x) for x in tail] if tail else []
        with mp.Pool(a.jobs, initializer=_init, initargs=(a.orig, not a.coverage_scenes)) as p:
            res = p.map(one, [(path, a.kind, s, a.lm, a.la) for s in range(lo, hi)])
        th = []      # (rt, cyc, ins, mem) per writing thread
        waves_c, waves_i, waves_rt = [], [], []
        for (tc, trt), writers in res:
            byw = {}
            for (tid, cyc, ins, mem), (tid2, rt, _, _) in zip(tc, trt):
                if tid not in writers:
                    continue
                th.append((rt, cyc, ins, mem))
                byw.setdefault((tid[0] // 8, tid[1] // 8, tid[2]), []).append((rt, cyc, ins))
            for w in byw.values():
                waves_rt.append(max(x[0] for x in w)); waves_c.append(max(x[1] for x in w)); waves_i.append(max(x[2] for x in w))
        n = len(th)
        avg = lambda i: sum(x[i] for x in th) / max(n, 1)
        C = sum(waves_c) / max(len(waves_c), 1)
        I = sum(waves_i) / max(len(waves_i), 1)
        RT = sum(waves_rt) / max(len(waves_rt), 1)
        est = {}
        for chip, W in zip(("gfx1103", "gfx1201"), waves):
            est[chip] = max(C, W * I) / W
        rows.append((name, n, avg(0), RT, avg(1), C, avg(2), I, avg(3), waves, est))
    print("| shader | threads | round trips/thread | round trips/wave(max) | crit. cycles/thread | crit. cycles/wave | DXIL instr/thread | instr/wave | mem reads/thread | waves/SIMD | est. cycles/wave gfx1103 | gfx1201 |")
    print("|---|---|---|---|---|---|---|---|---|---|---|---|")
    for r in rows:
        print("| %s | %d | %.1f | %.1f | %.0f | %.0f | %.0f | %.0f | %.1f | %s | %s | %s |" % (
            r[0], r[1], r[2], r[3], r[4], r[5], r[6], r[7], r[8], "/".join(map(str, r[9])) or "-",
            "%.0f" % r[10]["gfx1103"] if "gfx1103" in r[10] else "-", "%.0f" % r[10]["gfx1201"] if "gfx1201" in r[10] else "-"))
    if len(rows) > 1:
        b = rows[0]
        for r in rows[1:]:
            sp = ["%s x%.2f" % (c, b[10][c] / r[10][c]) for c in r[10] if c in b[10]]
            print("model speed-up %s vs %s: round trips x%.2f, critical cycles x%.2f; %s" % (
                r[0], b[0], b[3] / max(r[3], 1e-9), b[5] / max(r[5], 1e-9), ", ".join(sp)))


def _init(orig, real):
    global ORIG, REAL
    ORIG, REAL = orig, real


if __name__ == "__main__":
    main()
