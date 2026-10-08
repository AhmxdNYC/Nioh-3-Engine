r"""verify_post.py -- bit-exact proof of the runtime-post replacements against the shipped DXBC (Ally dump).

Both programs run in dxbc_interp on the same synthetic scenes; every SV_Target component of every pixel must have the
same 32 bits (0 ulp; NaN payloads included), the same discard/hang outcome. Also reports fetches and wait rounds.
Usage: py -I verify_post.py [name ...] [--scenes N] [--px N]  -> out\verify_<name>.txt
"""
import argparse, math, multiprocessing as mp, os, random, struct, sys, time

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import build_post as B  # noqa: E402
import dxbc_interp as DI  # noqa: E402

F32 = np.float32
SPECIAL = [0.0, -0.0, 1.0, -1.0, 0.5, 1e-7, -1e-7, 1e-30, 1.4e-45, -1.4e-45, 65504.0, 65024.0, 3e38, -3e38,
           float("inf"), float("-inf"), float("nan")]


def u(f):
    return DI.f2b(f)


def vec(*fs):
    return [u(f) for f in fs] + [0] * (4 - len(fs))


def rtex(rng, w, h, lo=-2.0, hi=2.0, special=0.02, quant=None, dup=0.0, d=None):
    shape = (h, w, 4) if d is None else (d, h, w, 4)
    a = rng.uniform(lo, hi, size=shape).astype(np.float32)
    if quant:
        a = (np.round(a * quant) / quant).astype(np.float32)
    if dup:
        m = rng.random(shape[:-1]) < dup
        a[m] = a.reshape(-1, 4)[0]
    if special:
        m = rng.random(shape) < special
        a[m] = rng.choice(np.array(SPECIAL, dtype=np.float32), size=int(m.sum()))
    return a


def rv(rng, lo, hi, special=0.0):
    if rng.random() < special:
        return float(rng.choice(SPECIAL))
    return float(F32(rng.uniform(lo, hi)))


def pixels(rng, W, H, n, uvjit=True):
    """SV_Position + TEXCOORD as the full-screen VS produces them (+ random off-grid points)."""
    out = []
    for _ in range(n):
        x, y = int(rng.integers(0, W)), int(rng.integers(0, H))
        uvx, uvy = F32((x + 0.5) / W), F32((y + 0.5) / H)
        if uvjit and rng.random() < 0.15:
            uvx, uvy = F32(rng.uniform(-0.1, 1.1)), F32(rng.uniform(-0.1, 1.1))
        out.append(((x, y), {0: vec(x + 0.5, y + 0.5, 0.5, 1.0), 1: vec(float(uvx), float(uvy), float(uvx), float(uvy))}))
    return out


# ---------------------------------------------------------------- scenes per shader
def cb_mb(rng, tileN=None):
    W, H = int(rng.integers(24, 97)), int(rng.integers(16, 65))
    n = tileN if tileN is not None else float(rng.choice([0, 1, 2, 3, 4, 7, 8, 9, 15, 16, 17, 20, 2.5, 7.9, 8.0001, -1, 1e-7]))
    mb0 = vec(rv(rng, 0, 1), float(rng.choice([0.0, 1.0, 0.5, 2.0, rv(rng, -3, 3)])), rv(rng, 0, 40),
              float(rng.choice([0, 1, 2, 3, 5, 8, 10, 12, 4.5, 16, rv(rng, 0, 20)])))
    tile = vec(float(rng.choice([0.0, 0.5, -0.5, rv(rng, -4, 4)])), float(rng.choice([0.0, 0.5, rv(rng, -4, 4)])), n,
               rv(rng, 0, 4))
    scr = vec(W, H, 1.0 / W, 1.0 / H)
    tinfo = vec(rv(rng, 0, 1), rv(rng, 0, 1), rv(rng, -0.05, 0.05), rv(rng, -0.05, 0.05))
    time_ = vec(rv(rng, 0, 100), float(rng.choice([0.0, rv(rng, 0, 1000)])), rv(rng, 0, 1), rv(rng, 0, 1))
    cb = [mb0, tile, scr, tinfo, time_, vec(1, 0, 1, 0), vec(1, 1, 0, 0), vec(1, 1, 1, 1)]
    return W, H, {3: cb}


def scene_mbtile(seed):
    rng = np.random.default_rng(seed)
    W, H, cbs = cb_mb(rng)
    tw, th = int(rng.integers(8, 129)), int(rng.integers(8, 97))
    t0 = rtex(rng, tw, th, -40, 40, special=0.01 if seed % 3 == 0 else 0.0, quant=float(rng.choice([0, 1, 4, 256])) or None,
              dup=0.3 if seed % 2 else 0.0)
    if seed % 5 == 1:  # many equal magnitudes: first-maximum-wins ordering
        t0[..., :2] = np.sign(t0[..., :2]) * 3.0
    return cbs, {0: DI.Tex(t0, 0x10)}, tile_pixels(rng, tw, th)


def tile_pixels(rng, tw, th, n=24):
    out = []
    for _ in range(n):
        x, y = int(rng.integers(0, max(tw // 8, 1) + 1)), int(rng.integers(0, max(th // 8, 1) + 1))
        uvx, uvy = F32(x * 8.0 / tw), F32(y * 8.0 / th)
        if rng.random() < 0.2:
            uvx, uvy = F32(rng.uniform(-0.2, 1.2)), F32(rng.uniform(-0.2, 1.2))
        out.append(((x, y), {0: vec(x + 0.5, y + 0.5, 0.5, 1.0), 1: vec(float(uvx), float(uvy), float(uvx), float(uvy))}))
    return out


def scene_mbgather(seed):
    rng = np.random.default_rng(seed)
    W, H, cbs = cb_mb(rng)
    sp = 0.01 if seed % 4 == 0 else 0.0
    t0 = rtex(rng, W, H, 0, 8, special=sp)                      # colour
    t3 = rtex(rng, max(W // 8, 1), max(H // 8, 1), -30, 30, special=sp)  # neighbour max (pixels)
    if seed % 3 == 1:
        t3[..., :2] *= 0.05                                      # mostly below the 2-px early-out
    t4 = rtex(rng, W, H, 0, 1, special=sp)                      # velocity info: xy in [0,1] (2x-1), z = depth
    t4[..., 2] = rng.uniform(0.0, 1.0, size=t4.shape[:2]).astype(np.float32) if seed % 2 else rng.choice(
        np.array([0.25, 0.5, 0.75], dtype=np.float32), size=t4.shape[:2])
    texs = {0: DI.Tex(t0, 0x20), 3: DI.Tex(t3, 0x23), 4: DI.Tex(t4, 0x24)}
    return cbs, texs, pixels(rng, W, H, 40)


def scene_dofgather(seed):
    rng = np.random.default_rng(seed)
    W, H = int(rng.integers(24, 97)), int(rng.integers(16, 65))
    n = int(rng.choice([1, 2, 3, 4, 5, 7, 8, 9, 11, 13, 16, 0]))
    rad = float(rng.choice([0, 1, 2, 3, 4, 6, 8, 2.5, -1, 100, rv(rng, 0, 10)]))
    cocmax = float(rng.choice([1.0, 4.0, 8.0, 16.0, rv(rng, 0.1, 20)]))
    cb = [vec(rv(rng, 0, 2)) for _ in range(7)]
    cb[1] = vec(cocmax, rad, rv(rng, 0.001, 0.05), rv(rng, 0.5, 2.0))
    cb[2] = vec(rv(rng, 0, 1), float(rng.choice([rv(rng, -2, 2), 0.0, 1e30])), rv(rng, 0, 1), rv(rng, 0, 1))
    cb[6] = vec(float(n), float(rng.choice([0.0, 0.5, 1.0, 2.0, rv(rng, -1, 4)])), rv(rng, 0, 1), rv(rng, 0, 1))
    sp = 0.01 if seed % 4 == 0 else 0.0
    t0 = rtex(rng, W, H, 0, 8, special=sp)
    t1 = rtex(rng, W, H, -cocmax * 1.5, cocmax * 1.5, special=sp)
    t2 = rtex(rng, W, H, 0, 4, special=sp)
    t2[..., 3] = rng.choice(np.array([0.0, 0.0, 0.5, 1.0, 2.0, 70000.0], dtype=np.float32), size=t2.shape[:2])
    t3 = rtex(rng, W, H, 0, 4, special=sp)
    t3[..., 3] = rng.choice(np.array([0.0, 0.25, 1.0, 3.0], dtype=np.float32), size=t3.shape[:2])
    tw = max(n, 1)
    t4 = rtex(rng, tw, tw, -1.5, 1.5, special=sp)  # sample offset table
    t4[..., 2] = rng.uniform(0, 3, size=t4.shape[:2]).astype(np.float32)
    texs = {0: DI.Tex(t0, 0x30), 1: DI.Tex(t1, 0x31), 2: DI.Tex(t2, 0x32), 3: DI.Tex(t3, 0x33), 4: DI.Tex(t4, 0x34)}
    return {2: cb}, texs, pixels(rng, W, H, 16)


def scene_compose(seed, ncb=40):
    rng = np.random.default_rng(seed)
    W, H = int(rng.integers(24, 97)), int(rng.integers(16, 65))
    cb = [vec(rv(rng, -1, 2, 0.02), rv(rng, -1, 2, 0.02), rv(rng, -1, 2, 0.02), rv(rng, -1, 2, 0.02)) for _ in range(ncb)]
    # feature switches: random on/off so all branches run
    for i, c in ((2, 2), (5, 3), (23, 0), (23, 1), (25, 0), (26, 0), (3, 0), (19, 3), (24, 0), (24, 1)):
        cb[i][c] = u(float(rng.choice([0.0, 0.0, 1.0, rv(rng, 0, 2)])))
    cb[3] = vec(float(rng.choice([0.0, 0.5, 1.0, 0.02, rv(rng, 0, 1)])), float(rng.choice([0, 1, 2, 3, 4, 5])),
                rv(rng, 0, 1), rv(rng, 0, 1))
    cb[0] = vec(W, H, 1.0 / W, 1.0 / H)
    # dramatic-LUT info: .z is a bit field (low 16 bits: use mask texture when 0; bits 16..23 and 24..31: curve modes),
    # .w integer flags
    for r in (19, 21):
        cb[r][2] = int(rng.choice([0, 1, 0x10000, 0x20000, 0x30000, 0x40000, 0x50000, 0x1234, 0x30001]))
        cb[r][2] |= int(rng.choice([0, 0, 0x1000000, 0x2000000, 0x3000000, 0x4000000, 0x5000000]))
        cb[r][3] = int(rng.choice([0, 1, 2, 3]))
    sp = 0.01 if seed % 4 == 0 else 0.0
    texs = {}
    for t in range(12):
        texs[t] = DI.Tex(rtex(rng, W, H, 0, 4, special=sp), 0x40 + t)
    texs[2] = DI.Tex(rtex(rng, 1, 1, 0.1, 4), 0x42)
    for t in (3, 4, 5, 7):
        texs[t] = DI.Tex(rtex(rng, 8, 8, 0, 1, d=8), 0x40 + t)
    return {2: cb}, texs, pixels(rng, W, H, 24)


SCENES = {"mbtile": scene_mbtile, "mbgather": scene_mbgather, "dofgather": scene_dofgather,
          "compose_ca": scene_compose, "compose_ca2": scene_compose}


def run_scene(args):
    name, seed, otext, ntext = args
    po, pn = DI.Program(otext), DI.Program(ntext)
    cbs, texs, px = SCENES[name](seed)
    res = dict(px=0, bad=0, fo=0, fn=0, ro=0, rn=0, hang=0, first=None, cov=set())
    for xy, inp in px:
        a = DI.Machine(po, cbs, texs).run(inp)
        b = DI.Machine(pn, cbs, texs).run(inp)
        res["px"] += 1
        res["cov"] |= a.get("cov", set())
        if a.get("hang") or b.get("hang"):
            res["hang"] += 1
            if bool(a.get("hang")) != bool(b.get("hang")):
                res["bad"] += 1
                res["first"] = res["first"] or (seed, xy, "hang mismatch")
            continue
        ok = a["o"] == b["o"] and a["discard"] == b["discard"]
        if not ok:
            res["bad"] += 1
            if res["first"] is None:
                res["first"] = (seed, xy, [hex(x) for x in a["o"].get(0, [])], [hex(x) for x in b["o"].get(0, [])])
        res["fo"] += a["fetches"]; res["fn"] += b["fetches"]; res["ro"] += a["rounds"]; res["rn"] += b["rounds"]
    return res


def verify(name, nscenes, procs=None, ntext=None, quiet=False, src=None):
    orig, new, outp, probs = B.build(name, src)
    otext = DI.disassemble(orig)
    ntext = ntext or DI.disassemble(new)
    t0 = time.time()
    jobs = [(name, s, otext, ntext) for s in range(nscenes)]
    with mp.Pool(procs or max(1, os.cpu_count() - 2)) as pool:
        rs = pool.map(run_scene, jobs, chunksize=1)
    tot = {k: sum(r[k] for r in rs) for k in ("px", "bad", "fo", "fn", "ro", "rn", "hang")}
    first = next((r["first"] for r in rs if r["first"]), None)
    cov = set().union(*[r["cov"] for r in rs])
    P = DI.Program(otext).ins
    alu = [i for i, ins in enumerate(P) if ins.op not in ("else", "endif", "endloop", "loop", "ret")]
    miss = [P[i].text for i in alu if i not in cov]
    n = max(tot["px"] - tot["hang"], 1)
    lines = ["%s: original %s vs replacement %s (sha256 %s)" % (name, B.SHADERS[name][0], os.path.basename(outp),
                                                                 __import__("hashlib").sha256(new).hexdigest()),
             "interface: %s" % ("OK (ISGN/OSGN identical, same bindings and cbuffer layout)" if not probs else probs),
             "scenes %d, pixels %d (hang-in-both %d), mismatching pixels %d%s" % (
                 nscenes, tot["px"], tot["hang"], tot["bad"], "" if not first else "  first: %s" % (first,)),
             "coverage: %d / %d shipped instructions executed%s" % (len(alu) - len(miss), len(alu),
                                                                   ("; not reached: " + " | ".join(miss[:40])) if miss else ""),
             "per pixel: fetches %.2f -> %.2f, wait rounds %.2f -> %.2f" % (tot["fo"] / n, tot["fn"] / n, tot["ro"] / n,
                                                                          tot["rn"] / n),
             "RESULT: %s  (%.0f s)" % ("BIT-EXACT (0 ulp, all outputs)" if tot["bad"] == 0 and not probs else "FAIL",
                                      time.time() - t0)]
    if not quiet:
        print("\n".join(lines))
    open(os.path.join(HERE, "out", "verify_%s.txt" % name), "w").write("\n".join(lines) + "\n")
    return tot["bad"] == 0 and not probs


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("names", nargs="*")
    ap.add_argument("--scenes", type=int, default=200)
    ap.add_argument("--src", default=None, help="verify this HLSL instead of the shader's own source")
    a = ap.parse_args()
    ok = True
    for n in a.names or list(SCENES):
        ok &= verify(n, a.scenes, src=a.src)
    print("ALL BIT-EXACT" if ok else "SOME FAILED")


if __name__ == "__main__":
    main()
