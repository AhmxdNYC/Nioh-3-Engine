r"""emulate_esm.py - offline check of ESM_fast.hlsl against the original ESM blur semantics (CPU, float32 numpy).

Models: the original's 2R+1 bilinear Sample taps (texel-space position quantised to 1/256 like the texture unit,
both round-to-nearest and truncation), D3D Gather component order (x=(u0,v1) y=(u1,v1) z=(u1,v0) w=(u0,v0)),
clamp addressing, the exact UV/corner arithmetic of both shaders in float32, and the interpolator putting tc up
to +-2 ulp off the pixel centre. Checks every R=1..5, V and H, 1:1 mapping (fast path) and 2:1 (must fall back).
    py -I emulate_esm.py
"""
import numpy as np

f32 = np.float32
W_TAB = {1: [0x3eb78898, 0x3ea43bb4], 2: [0x3e6cea6c, 0x3e5b5fe5, 0x3e2e2ac3],
         3: [0x3e356bca, 0x3e2b0ea2, 0x3e0f6274, 0x3dd5b24f],
         4: [0x3e16aa08, 0x3e0fa872, 0x3df91149, 0x3dc44bf5, 0x3d8ca70d],
         5: [0x3e03165d, 0x3dfbe554, 0x3ddf6901, 0x3db6e9bc, 0x3d8a3e7d, 0x3d40e5a3]}


def wts(R):
    return [np.array([x], dtype=np.uint32).view(np.float32)[0] for x in W_TAB[R]]


def texel(tex, x, y):
    h, w = tex.shape
    return tex[min(max(y, 0), h - 1), min(max(x, 0), w - 1)]


def sample_bilinear(tex, u, v, mode):
    h, w = tex.shape
    px = f32(u) * f32(w) - f32(0.5)
    py = f32(v) * f32(h) - f32(0.5)
    x0, y0 = int(np.floor(px)), int(np.floor(py))
    fx, fy = float(px) - x0, float(py) - y0
    q = (lambda a: round(a * 256) / 256) if mode == "round" else (lambda a: np.floor(a * 256) / 256)
    fx, fy = q(fx), q(fy)
    if fx >= 1: x0, fx = x0 + 1, 0.0
    if fy >= 1: y0, fy = y0 + 1, 0.0
    a, b = texel(tex, x0, y0), texel(tex, x0 + 1, y0)
    c, d = texel(tex, x0, y0 + 1), texel(tex, x0 + 1, y0 + 1)
    return f32((a * (1 - fx) + b * fx) * (1 - fy) + (c * (1 - fx) + d * fx) * fy)


def gather(tex, u, v):
    h, w = tex.shape
    x0 = int(np.floor(f32(u) * f32(w) - f32(0.5)))
    y0 = int(np.floor(f32(v) * f32(h) - f32(0.5)))
    return (texel(tex, x0, y0 + 1), texel(tex, x0 + 1, y0 + 1), texel(tex, x0 + 1, y0), texel(tex, x0, y0))  # x y z w


def combine(s, R, Wt):
    acc = f32((s[R + 1] + s[R - 1]) * Wt[1] + s[R] * Wt[0])
    for k in range(2, R + 1):
        acc = f32(acc + f32(s[R + k] + s[R - k]) * Wt[k])
    return acc


def original(tex, tc, R, V, mode):
    h, w = tex.shape
    Wt = wts(R)
    d = f32(1) / f32(h if V else w)
    s = [None] * (2 * R + 1)
    for k in range(-R, R + 1):
        if k < 0:   a = f32((tc[1] if V else tc[0]) - f32(d * f32(-k)))
        elif k == 0: a = tc[1] if V else tc[0]
        else:       a = f32(f32(d * f32(k)) + (tc[1] if V else tc[0]))
        u, v = (tc[0], a) if V else (a, tc[1])
        s[k + R] = sample_bilinear(tex, u, v, mode)
    return combine(s, R, Wt)


def fast(tex, tc, R, V, mode):
    h, w = tex.shape
    size = (f32(w), f32(h))
    p = (f32(tc[0] * size[0] - f32(0.5)), f32(tc[1] * size[1] - f32(0.5)))
    ip = (f32(np.floor(p[0] + f32(0.5))), f32(np.floor(p[1] + f32(0.5))))
    fr = (abs(f32(p[0] - ip[0])), abs(f32(p[1] - ip[1])))
    if not (fr[0] <= 1 / 1024 and fr[1] <= 1 / 1024):
        return original(tex, tc, R, V, mode), False          # fallback = original taps
    rcp = (f32(1) / size[0], f32(1) / size[1])
    t = []
    for j in range(R + 1):
        if V: uv = (f32(f32(ip[0] + f32(1)) * rcp[0]), f32(f32(ip[1] + f32(2 * j - R + 1)) * rcp[1]))
        else: uv = (f32(f32(ip[0] + f32(2 * j - R + 1)) * rcp[0]), f32(f32(ip[1] + f32(1)) * rcp[1]))
        gx, gy, gz, gw = gather(tex, *uv)
        t += [gw, gx] if V else [gw, gz]
    return combine(t, R, wts(R)), True


def run(N=48, ratio=1, seed=1):
    rng = np.random.default_rng(seed)
    tex = rng.random((N, N), dtype=np.float32) * f32(50) + f32(1)      # ESM-like positive values
    dst = N // ratio
    worst = 0.0; nfast = 0; ntot = 0; worst_exact = 0.0
    for R in range(1, 6):
        for V in (0, 1):
            for j in range(dst):
                for i in range(dst):
                    tc = [f32((i + 0.5) / dst), f32((j + 0.5) / dst)]
                    tc = [np.nextafter(tc[0], f32(2) * rng.integers(0, 2), dtype=np.float32) if rng.random() < .5 else tc[0],
                          np.nextafter(tc[1], f32(2) * rng.integers(0, 2), dtype=np.float32) if rng.random() < .5 else tc[1]]
                    tc = [f32(tc[0]), f32(tc[1])]
                    for mode in ("round", "trunc"):
                        o = original(tex, tc, R, V, mode)
                        fv, used_fast = fast(tex, tc, R, V, mode)
                        nfast += used_fast; ntot += 1
                        rel = abs(float(fv) - float(o)) / max(abs(float(o)), 1e-30)
                        worst = max(worst, rel)
                        if mode == "round":
                            worst_exact = max(worst_exact, rel)
    return worst_exact, worst, nfast, ntot


if __name__ == "__main__":
    for ratio in (1, 2):
        we, w, nf, nt = run(ratio=ratio)
        print(f"src:dst = {ratio}:1  fast path used {nf}/{nt}  max rel diff vs original: "
              f"round-to-nearest filter {we:.2e}, truncating filter {w:.2e}")
