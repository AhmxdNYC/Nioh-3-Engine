r"""verify.py - differential test of two DXIL programs (original vs rewrite) on synthetic scenes.

    py -I verify.py ORIG.ll NEW.ll --kind refprobe|vfscat --seeds 0:64 [--jobs 10] [--ulp 0]

For every seed both programs run on identical inputs (scenes.py) in the reference interpreter (dxil_interp.py);
every UAV write (thread, address, 4 values) is compared. Reported:
  * writes compared, bit-exact count, max float32 ulp distance, worst examples;
  * block coverage of the ORIGINAL program (and the list of never-executed blocks) over all seeds;
  * a non-zero exit code if any write differs by more than --ulp ulps or the write sets differ.
"""
import argparse, multiprocessing as mp, sys, time
import os as _os
sys.path.insert(0, _os.path.dirname(_os.path.abspath(__file__)))
import dxil_interp as D
import scenes

_cache = {}


F64 = False


def _shader(path):
    if path not in _cache:
        _cache[path] = (D.Shader(open(path).read(), _os.path.basename(path), f64=F64), open(path).read())
    return _cache[path]


REAL = False


def make_scene(kind, text, seed):
    if kind == "refprobe":
        return scenes.refprobe_scene(text, seed, realistic=REAL or kind == "refprobe-real")
    if kind == "vfscat":
        return scenes.vfscat_scene(text, seed, realistic=REAL)
    raise ValueError(kind)


def run_one(args):
    orig, new, kind, seed, f64, real = args
    global F64, REAL
    F64, REAL = f64, real
    so, to = _shader(orig)
    sn, tn = _shader(new)
    t0 = time.time()
    sc1, groups = make_scene(kind, to, seed)
    sc2, _ = make_scene(kind, to, seed)
    cov = [0] * len(so.m.blocks)
    so.dispatch(sc1, groups, coverage=cov)
    sn.dispatch(sc2, groups)
    u1, u2 = D.uav_state(sc1), D.uav_state(sc2)
    res = dict(seed=seed, n=0, exact=0, maxulp=0, worst=[], setdiff=0, cov=cov, t=time.time() - t0, hist={},
               h16=0, h16max=0, maxabs=0.0, maxrel=0.0, relbad=0)
    for k in u1:
        w1 = {}
        for w in u1[k][1]:
            w1.setdefault((w[0], w[1] if len(w) == 4 else (w[1], w[2])), []).append(w[-2])
        w2 = {}
        for w in u2[k][1]:
            w2.setdefault((w[0], w[1] if len(w) == 4 else (w[1], w[2])), []).append(w[-2])
        if set(w1) != set(w2):
            res["setdiff"] += len(set(w1) ^ set(w2))
        for key in set(w1) & set(w2):
            a, b = w1[key], w2[key]
            if len(a) != len(b):
                res["setdiff"] += 1
                continue
            for va, vb in zip(a, b):
                res["n"] += 1
                d = max(D.ulp_diff(x, y) for x, y in zip(va, vb))
                res["hist"][min(d, 1 << 20)] = res["hist"].get(min(d, 1 << 20), 0) + 1
                rel = max([abs(x - y) / max(abs(x), abs(y), 1e-6) for x, y in zip(va, vb) if x == x and y == y] or [0.0])
                if any((x != x) != (y != y) for x, y in zip(va, vb)):
                    rel = 1.0
                res["maxrel"] = max(res["maxrel"], rel)
                if rel > 1e-9:
                    res["relbad"] += 1
                if d == 0:
                    res["exact"] += 1
                else:
                    import numpy as np
                    a16 = np.array(va, dtype=np.float32).astype(np.float16)
                    b16 = np.array(vb, dtype=np.float32).astype(np.float16)
                    u16 = [abs(int(x) - int(y)) for x, y in zip(a16.view(np.int16), b16.view(np.int16))
                           if not (np.isnan(x) and np.isnan(y))]
                    if any(u16):
                        res["h16"] += 1
                        res["h16max"] = max(res["h16max"], max(u16))
                    ad = max([abs(x - y) for x, y in zip(va, vb) if x == x and y == y] or [0.0])
                    if ad > res["maxabs"]:
                        res["maxabs"] = ad
                        res["maxabs_ex"] = (key, va, vb)
                if d > res["maxulp"]:
                    res["maxulp"] = d
                if d > 0 and len(res["worst"]) < 3:
                    res["worst"].append((key, va, vb, d))
    return res


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("orig"); ap.add_argument("new")
    ap.add_argument("--kind", default="refprobe")
    ap.add_argument("--seeds", default="0:16")
    ap.add_argument("--jobs", type=int, default=10)
    ap.add_argument("--ulp", type=int, default=0)
    ap.add_argument("--uncovered", action="store_true")
    ap.add_argument("--f64", action="store_true", help="evaluate both programs in float64 (reassociation-blind)")
    ap.add_argument("--realistic", action="store_true", help="realistic (dense probe field) scenes instead of coverage scenes")
    a = ap.parse_args()
    lo, hi = (int(x) for x in a.seeds.split(":"))
    jobs = [(a.orig, a.new, a.kind, s, a.f64, a.realistic) for s in range(lo, hi)]
    global F64
    F64 = a.f64
    t0 = time.time()
    if a.jobs > 1:
        with mp.Pool(a.jobs) as p:
            results = p.map(run_one, jobs, chunksize=1)
    else:
        results = [run_one(j) for j in jobs]
    so, _ = _shader(a.orig)
    cov = [0] * len(so.m.blocks)
    tot = dict(n=0, exact=0, maxulp=0, setdiff=0, h16=0, h16max=0, maxabs=0.0, maxrel=0.0, relbad=0)
    hist = {}
    for r in results:
        for i, c in enumerate(r["cov"]):
            cov[i] += c
        for k in tot:
            tot[k] = max(tot[k], r[k]) if k in ("maxulp", "h16max", "maxabs", "maxrel") else tot[k] + r[k]
        for k, v in r["hist"].items():
            hist[k] = hist.get(k, 0) + v
        if r["worst"]:
            for w in r["worst"][:1]:
                print("  seed %d: %s orig=%s new=%s (%d ulp)" % (r["seed"], w[0], w[1], w[2], w[3]))
    nb = len(cov)
    hit = sum(1 for c in cov if c)
    print("%s vs %s: %d seeds, %d writes, bit-exact %d (%.4f%%), max %d ulp, write-set differences %d, %.0fs" % (
        _os.path.basename(a.orig), _os.path.basename(a.new), len(results), tot["n"], tot["exact"],
        100.0 * tot["exact"] / max(tot["n"], 1), tot["maxulp"], tot["setdiff"], time.time() - t0))
    mx = max(results, key=lambda r: r["maxabs"])
    if mx.get("maxabs_ex"):
        print("  largest absolute difference: seed %d %s orig=%s new=%s" % (mx["seed"], *mx["maxabs_ex"]))
    print("after fp16 quantisation: %d writes differ (max %d fp16 ulp); max abs diff %.3g" % (tot["h16"], tot["h16max"], tot["maxabs"]))
    print("max relative difference %.3g; writes with relative difference > 1e-9: %d" % (tot["maxrel"], tot["relbad"]))
    print("ulp histogram:", ", ".join("%s:%d" % (k if k < (1 << 20) else ">=2^20", v) for k, v in sorted(hist.items())))
    print("original block coverage: %d / %d (%.1f%%)" % (hit, nb, 100.0 * hit / nb))
    if a.uncovered:
        print("never executed:", " ".join(so.m.blocks[i].name for i, c in enumerate(cov) if not c))
    if a.f64:
        ok = tot["setdiff"] == 0 and tot["relbad"] == 0 and tot["n"] > 0
    else:
        ok = tot["setdiff"] == 0 and tot["maxulp"] <= a.ulp and tot["n"] > 0
    print("RESULT:", "PASS" if ok else "FAIL")
    sys.exit(0 if ok else 1)


if __name__ == "__main__":
    main()
