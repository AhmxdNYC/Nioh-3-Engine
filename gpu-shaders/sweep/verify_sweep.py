r"""verify_sweep.py - prove that each sweep replacement computes exactly what the shipped shader computes.

    py -I verify_sweep.py [mpc|mshlod|mpc23|toonedge2 ...]      (run build_sweep.py first; it writes build\<item>_{orig,fast}.ll)
    (mpc = the combined MPC chain file 0x29ee0902.file; its proof is gpu-shaders\culling\verify_culling.py)

Both DXIL programs (the shipped record and our compiled replacement, as `dxc -dumpbin` text) are executed by the
reference interpreter (sim.py: IEEE float32 per IR op, robust out-of-bounds loads = 0 / stores dropped, groupshared
memory with barriers and a race detector) on the same synthetic scenes, and every UAV is compared: final contents
bit for bit, plus the multiset of (thread, address, value) writes. Scenes cover the edge cases of each shader
(element counts around the 64/1024 boundaries, multi-group dispatches, image borders, out-of-range threads,
quantisation boundaries, NaN/Inf/denormal inputs). PASS = 0 differences in every scene.
"""
import os, random, struct, sys, time
import numpy as np
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import sim  # noqa: E402

BUILD = os.path.join(HERE, "build")


def load(item):
    o = sim.BShader(open(os.path.join(BUILD, item + "_orig.ll"), encoding="utf-8").read(), item + "_orig")
    f = sim.BShader(open(os.path.join(BUILD, item + "_fast.ll"), encoding="utf-8").read(), item + "_fast")
    return o, f


def snapshot(scene):
    out = {}
    for k, r in scene.b.items():
        if k[0] != 1:
            continue
        if isinstance(r, sim.TypedBuffer):
            out[k] = ([tuple(x) for x in r.data], sorted(map(repr, r.writes)))
        elif isinstance(r, sim.RWTexture):
            out[k] = (sorted((kk, tuple(struct.pack("<f", float(v)) for v in vv)) for kk, vv in r.data.items()),
                      sorted(repr((w[0], w[1], tuple(struct.pack("<f", float(v)) for v in w[2]), w[3])) for w in r.writes))
    return out


def same(a, b):
    if a == b:
        return True, ""
    for k in a:
        if a[k] != b[k]:
            if a[k][0] != b[k][0]:
                return False, "UAV %r final contents differ" % (k,)
            return False, "UAV %r write log differs" % (k,)
    return False, "?"


# ------------------------------------------------------------------------------------------------ MPC#23
def mpc23_scene(rng, cnt, off, nmax):
    cb = struct.pack("<4i4I", off, rng.randrange(-5, 5), 0, 0, rng.randrange(1 << 32), cnt, rng.randrange(1 << 32), 0)
    hi = rng.choice([1 << 32, 1 << 8, 1 << 31])
    u2 = sim.TypedBuffer([rng.randrange(hi) for _ in range(off + 4 * nmax + 16)], isint=True)
    u9 = sim.TypedBuffer([rng.randrange(1 << 32) for _ in range(9 * nmax + 32)], isint=True)
    u11 = sim.TypedBuffer([rng.randrange(hi) for _ in range(3 * nmax + 8)], isint=True)
    return sim.Scene({(2, 0): sim.CBuffer(cb), (1, 2): u2, (1, 9): u9, (1, 11): u11})


def verify_mpc23():
    o, f = load("mpc23")
    rng = random.Random(23)
    cases = []
    for cnt in (0, 1, 2, 3, 63, 64, 65, 66, 127, 128, 129, 500, 1000, 1022, 1023, 1024, 1025):
        cases.append((cnt, [(0, 0, 0)]))
    for cnt in (1023, 1024, 1025, 1500, 2047, 2048, 2049):
        cases.append((cnt, [(0, 0, 0), (1, 0, 0)]))
    for cnt in (63, 64, 100, 1100):
        cases.append((cnt, [(0, 0, 0), (0, 1, 0)]))
    cases.append((700, [(0, 0, 0), (0, 0, 1)]))
    cases.append((3000, [(1, 0, 0), (2, 1, 0)]))
    n = bad = 0
    for cnt, groups in cases:
        for rep in range(2):
            off = rng.choice([0, 1, 7, 100])
            nmax = 1024 * (1 + max(g[0] for g in groups)) + 128 * max(g[1] for g in groups) + 64
            seed = rng.randrange(1 << 30)
            sa = mpc23_scene(random.Random(seed), cnt, off, nmax)
            sb = mpc23_scene(random.Random(seed), cnt, off, nmax)
            o.dispatch(sa, groups)
            f.dispatch(sb, groups)
            ok, why = same(snapshot(sa), snapshot(sb))
            n += 1
            if not ok:
                bad += 1
                print("  FAIL cnt=%d groups=%r off=%d: %s" % (cnt, groups, off, why))
    print("mpc23: %d scenes, %d differences -> %s" % (n, bad, "PASS" if bad == 0 else "FAIL"))
    return bad == 0


# ------------------------------------------------------------------------------------------------ TOONEDGE2#2
def _f32(x):
    return struct.unpack("<f", struct.pack("<f", x))[0]


def toon_scene(rng, W, H, nx, ny, thick, pz=0.55):
    """texture W x H; cbuffer nResX/nResY = nx/ny (may differ from the texture size, may be <= 0)."""
    edge = [k / 255.0 + d for k in (0, 1, 2, 127, 128, 254, 255) for d in (-0.5 / 255, -1e-7, 0.0, 1e-7, 0.5 / 255)]
    special = [float("nan"), float("inf"), -float("inf"), 1e-40, -1e-40, -0.0, -0.3, 2.0]

    def val():
        r = rng.random()
        if r < pz:
            return 0.0
        if r < pz + 0.25:
            return _f32(rng.choice(edge))
        if r < pz + 0.32:
            return rng.choice(special)
        return _f32(rng.uniform(-0.2, 1.2))
    inp = np.zeros((1, H, W, 4), np.float32)
    blob = rng.random() < 0.5
    for yy in range(H):
        for xx in range(W):
            if blob and ((xx // 5 + yy // 4) % 3):       # large all-zero regions (whole-group early outs too)
                continue
            for ch in range(4):
                if ch < 2 or rng.random() < 0.15:
                    inp[0, yy, xx, ch] = val()
    dset = [0.1, 0.2, 0.2, 0.5, 0.9, 1.0]
    dep = np.array([[[[rng.choice(dset) if rng.random() < 0.9 else rng.choice([float("nan"), 0.0, -1.0, float("inf")])]
                      for _ in range(W)] for _ in range(H)]], np.float32)
    sten = np.array([[[[rng.randrange(256), rng.randrange(256) if rng.random() < 0.3 else rng.randrange(128) * 2]
                       for _ in range(W)] for _ in range(H)]], np.uint32)
    cb = struct.pack("<iif", nx, ny, thick)
    out = sim.RWTexture(max(W, 1), max(H, 1), 1, 1, "2d")
    return sim.Scene({(2, 0): sim.CBuffer(cb), (0, 8): sim.Texture([inp]), (0, 9): sim.Texture([dep]),
                      (0, 10): sim.Texture([sten]), (1, 0): out})


def verify_toonedge2():
    o, f = load("toonedge2")
    rng = random.Random(2)
    # (tex W, tex H, nResX, nResY, groups-x, groups-y, group origin)
    shapes = [(37, 21, 37, 21, 3, 2, (0, 0)), (32, 16, 32, 16, 2, 1, (0, 0)), (5, 3, 5, 3, 1, 1, (0, 0)),
              (40, 24, 37, 21, 3, 2, (0, 0)), (30, 18, 37, 21, 3, 2, (0, 0)), (37, 21, 0, 0, 1, 1, (0, 0)),
              (37, 21, -3, 21, 1, 1, (0, 0)), (70, 60, 70, 60, 1, 1, (2, 1)), (17, 17, 17, 17, 2, 2, (0, 0)),
              (48, 40, 48, 33, 3, 3, (0, 0)), (16, 16, 1, 1, 1, 1, (0, 0)), (34, 20, 34, 20, 3, 2, (0, 0))]
    n = bad = 0
    for W, H, nx, ny, gx, gy, (ox, oy) in shapes:
        for rep in range(8):
            thick = rng.choice([0.0, 1.0, 2.5, -0.5, -3.0, 7.25, 1e-3])
            pz = rng.choice([0.3, 0.6, 0.9])
            seed = rng.randrange(1 << 30)
            groups = [(ox + i, oy + j, 0) for j in range(gy) for i in range(gx)]
            sa = toon_scene(random.Random(seed), W, H, nx, ny, thick, pz)
            sb = toon_scene(random.Random(seed), W, H, nx, ny, thick, pz)
            o.dispatch(sa, groups)
            f.dispatch(sb, groups)
            A, B = snapshot(sa), snapshot(sb)
            ok, why = same(A, B)
            n += 1
            if not ok:
                bad += 1
                print("  FAIL tex %dx%d nRes %d,%d groups %dx%d@%r thick %g: %s" % (W, H, nx, ny, gx, gy, (ox, oy), thick, why))
            vals = {}
            for _, v in sa.b[(1, 0)].data.items():
                vals[v[0]] = vals.get(v[0], 0) + 1
            if rep == 0:
                print("  tex %2dx%2d nRes %3d,%3d groups %dx%d: %4d writes, outputs %s" % (
                    W, H, nx, ny, gx, gy, len(sa.b[(1, 0)].writes),
                    " ".join("%g:%d" % (k, c) for k, c in sorted(vals.items(), key=lambda kv: -kv[1])[:6])))
    print("toonedge2: %d scenes, %d differences -> %s" % (n, bad, "PASS" if bad == 0 else "FAIL"))
    return bad == 0


# ------------------------------------------------------------------------------------------------ load counts
def stats():
    """memory instructions issued per thread (lane-level) by the original vs the replacement, on typical scenes."""
    cnt = {"n": 0}
    tl, bl = sim.Texture.load, sim.TypedBuffer.load

    def tload(self, *a):
        cnt["n"] += 1
        return tl(self, *a)

    def bload(self, *a):
        cnt["n"] += 1
        return bl(self, *a)
    sim.Texture.load, sim.TypedBuffer.load = tload, bload
    try:
        o, f = load("toonedge2")
        for pz, label in ((0.97, "mostly-empty outline mask (typical)"), (0.6, "dense outline mask"), (0.0, "no zero texels")):
            res = []
            for sh in (o, f):
                cnt["n"] = 0
                sc = toon_scene(random.Random(7), 64, 48, 64, 48, 1.0, pz)
                sh.dispatch(sc, [(i, j, 0) for j in range(3) for i in range(4)])
                res.append(cnt["n"] / (64 * 48))
            print("toonedge2 %-38s image loads/pixel: original %.2f, fast %.2f" % (label, res[0], res[1]))
        o, f = load("mpc23")
        for c in (256, 1000):
            res = []
            for sh in (o, f):
                cnt["n"] = 0
                sh.dispatch(mpc23_scene(random.Random(3), c, 0, 1200), [(0, 0, 0)])
                res.append(cnt["n"])
            print("mpc23 cnt=%4d: buffer loads %d (original: up to 64 serialised round trips per thread) vs %d "
                  "(fast: 1 round trip + LDS scan)" % (c, res[0], res[1]))
    finally:
        sim.Texture.load, sim.TypedBuffer.load = tl, bl
    return True


def verify_mpc():
    """combined 0x29ee0902.file (item "mpc": MPC #23 #28 #29 #30 #31): chain-level proof in gpu-shaders\culling."""
    import subprocess
    r = subprocess.run([sys.executable, "-I", os.path.join(HERE, "..", "culling", "verify_culling.py")])
    return r.returncode == 0


def verify_mshlod():
    """0x8d9282e6.file (item "mshlod": MshLod #3): proof in gpu-shaders/culling/verify_culling.py mshlod."""
    import subprocess
    r = subprocess.run([sys.executable, "-I", os.path.join(HERE, "..", "culling", "verify_culling.py"), "mshlod"])
    return r.returncode == 0


ITEMS = {"mpc": verify_mpc, "mshlod": verify_mshlod, "mpc23": verify_mpc23, "toonedge2": verify_toonedge2}
EXTRA = {"stats": stats}

if __name__ == "__main__":
    allok = True
    for it in (sys.argv[1:] or list(ITEMS)):
        t = time.time()
        allok &= {**ITEMS, **EXTRA}[it]()
        print("  (%.0f s)" % (time.time() - t))
    sys.exit(0 if allok else 1)
