r"""geoscene.py - geometry-consistent scenes for RefProbe #414 (reflection probe trace) and #121.

    scenes.refprobe_scene(text, seed, geo=True[, realistic=...])  ->  geoscene.refprobe_geo_scene(...)

The old random scenes (scenes.refprobe_scene) fill the probe octahedral maps and the G-buffer with unrelated noise:
fine for path coverage, but a reflection ray then "hits" at random and the march length (the #414 cost) is
meaningless. Here everything is ray-cast from ONE analytic scene, in probe-field coordinates:
  * a room (field AABB + margin, optionally open at the top = sky) with random boxes and spheres inside;
  * a probe at every lattice cell (level bit L where the cell is on the 2^L lattice); probes inside an obstacle are
    invalid (ref -1); each probe's octahedral tile holds, per texel direction, the hit distance (.z, and the
    Chebyshev moments .x mean / .y mean^2+var), the hit normal (sOctNormalSource, n*0.5+0.5), the radiance
    (3 mips) and the radiance mask (0 for sky);
  * a perspective camera inside the room: depth (D3D 0..1, 1 = sky), view-space octahedral normal (GBuf0.xy via
    mV2W), roughness (GBuf1.w), stencil, vEye are all those of the ray-cast hit.
Octahedral mapping = the shader's: uv = (tile*oct + 1 + oct_uv*(oct-2)) * rv.xy * rv.w (1-texel border).
realistic=True: game-like parameters (all LOD levels, stencil pass, field box = lattice, RLR mostly missing);
realistic=False: the same geometry with randomised parameters/edge cases (for coverage).
"""
import math, struct
import numpy as np
import os as _os, sys as _sys
_sys.path.insert(0, _os.path.dirname(_os.path.abspath(__file__)))
from dxil_interp import Texture, TypedBuffer, StructBuffer, RWTexture, Sampler, Scene  # noqa: E402
from ll2hlsl import Iface  # noqa: E402
import scenes as S  # noqa: E402

INF = 1e30


class Geo:
    """room [lo, hi] (inside; top open if `open_top`), boxes (lo, hi), spheres (c, r); all in field coordinates."""

    def __init__(self, lo, hi, boxes, spheres, open_top):
        self.lo, self.hi, self.boxes, self.spheres, self.open_top = lo, hi, boxes, spheres, open_top

    def inside_obstacle(self, p):
        p = np.atleast_2d(p)
        r = np.zeros(len(p), bool)
        for lo, hi in self.boxes:
            r |= np.all((p > lo) & (p < hi), axis=1)
        for c, rad in self.spheres:
            r |= np.linalg.norm(p - c, axis=1) < rad
        return r

    def cast(self, o, d):
        """o, d: (n,3) (d unit). -> t (inf = sky), normal (n,3), material id (n,)"""
        n = len(o)
        with np.errstate(divide="ignore", invalid="ignore"):
            inv = 1.0 / d
            tw = np.where(d > 0, (self.hi - o) * inv, (self.lo - o) * inv)
            tw = np.where(d == 0, INF, tw)
        ax = np.argmin(tw, axis=1)
        t = tw[np.arange(n), ax]
        nrm = np.zeros((n, 3))
        nrm[np.arange(n), ax] = -np.sign(d[np.arange(n), ax])
        mat = ax.astype(np.int64)
        if self.open_top:
            sky = (ax == 1) & (d[:, 1] > 0)
            t = np.where(sky, INF, t)
            mat = np.where(sky, -1, mat)
        for k, (lo, hi) in enumerate(self.boxes):
            with np.errstate(divide="ignore", invalid="ignore"):
                t1, t2 = (lo - o) * inv, (hi - o) * inv
            tn, tf = np.minimum(t1, t2), np.maximum(t1, t2)
            tn = np.where(np.isnan(tn), -INF, tn); tf = np.where(np.isnan(tf), INF, tf)
            ent = np.max(tn, axis=1); ext = np.min(tf, axis=1)
            hit = (ent <= ext) & (ent > 1e-6) & (ent < t)
            if hit.any():
                a = np.argmax(tn, axis=1)
                t = np.where(hit, ent, t)
                nb = np.zeros((n, 3)); nb[np.arange(n), a] = -np.sign(d[np.arange(n), a])
                nrm = np.where(hit[:, None], nb, nrm)
                mat = np.where(hit, 3 + k, mat)
        for k, (c, rad) in enumerate(self.spheres):
            oc = o - c
            b = np.sum(oc * d, axis=1)
            disc = b * b - (np.sum(oc * oc, axis=1) - rad * rad)
            s = np.sqrt(np.maximum(disc, 0))
            th = -b - s
            hit = (disc > 0) & (th > 1e-6) & (th < t)
            if hit.any():
                t = np.where(hit, th, t)
                p = o + d * th[:, None]
                nrm = np.where(hit[:, None], (p - c) / rad, nrm)
                mat = np.where(hit, 20 + k, mat)
        return t, nrm, mat


def oct_decode(u, v):
    """u, v in [0,1] -> unit direction (the inverse of the shader's encode, z <= 0 folded)."""
    px, py = u * 2 - 1, v * 2 - 1
    z = 1 - np.abs(px) - np.abs(py)
    fold = z < 0
    x = np.where(fold, (1 - np.abs(py)) * np.where(px >= 0, 1.0, -1.0), px)
    y = np.where(fold, (1 - np.abs(px)) * np.where(py >= 0, 1.0, -1.0), py)
    d = np.stack([x, y, z], -1)
    return d / np.linalg.norm(d, axis=-1, keepdims=True)


def oct_encode(n):
    n = n / np.sum(np.abs(n), axis=-1, keepdims=True)
    x, y, z = n[..., 0], n[..., 1], n[..., 2]
    fx = np.where(z < 0, (1 - np.abs(y)) * np.where(x >= 0, 1.0, -1.0), x)
    fy = np.where(z < 0, (1 - np.abs(x)) * np.where(y >= 0, 1.0, -1.0), y)
    return fx * 0.5 + 0.5, fy * 0.5 + 0.5


def persp(fovy, aspect, zn, zf):
    """D3D row-vector perspective (z_ndc 0..1)."""
    h = 1.0 / math.tan(fovy / 2)
    m = np.zeros((4, 4))
    m[0, 0] = h / aspect; m[1, 1] = h
    m[2, 2] = zf / (zf - zn); m[2, 3] = 1.0
    m[3, 2] = -zn * zf / (zf - zn)
    return m


def refprobe_geo_scene(text, seed, realistic=False, out_w=None, out_h=None):
    rng = np.random.default_rng(seed + 0x5EED)
    ifc = Iface(text)
    cb = S.CB(ifc, "$Globals")
    out_w = out_w or (32 if realistic else 24)
    out_h = out_h or (24 if realistic else 16)
    scale = 1.0 if realistic else float(rng.choice([1.0, 1.25, 2.0]))
    full_w, full_h = int(round(out_w * scale)), int(round(out_h * scale))
    cb.set("gv_info_out", [out_w if realistic else out_w - int(rng.integers(0, 3)),
                           out_h if realistic else out_h - int(rng.integers(0, 3)), 1.0 / out_w, rng.uniform(0.2, 1.5)])
    cb.set("gv_info_in", [full_w, full_h, scale, 1.0 / scale])
    vp = [0.0, 0.0, 1.0, 1.0] if realistic or rng.random() < 0.6 else \
        [rng.choice([0.0, 0.05]), rng.choice([0.0, 0.1]), 0.9, 0.85]
    cb.set("gv_viewport", vp)

    # ---- probe field (field coords f = rel @ R^T + t, rel = world - gv_distance_pos)
    # world units: the shader's parallax query point sits 10 units off the surface, so fields span tens of units
    N = np.array([int(rng.integers(10, 17)), int(rng.integers(5, 9)), int(rng.integers(10, 17))]) if realistic else         np.array([int(rng.integers(6, 15)), int(rng.integers(4, 9)), int(rng.integers(6, 15))])
    P = float(rng.choice([1.0, 2.0, 4.0])) if realistic else float(rng.choice([0.5, 1.0, 2.0, 4.0]))
    R = S.rot(rng) if rng.random() < 0.5 else np.eye(3)
    t = rng.uniform(-1, 1, 3)
    ext = (N - 1) * P
    w2f = np.eye(4); w2f[:3, :3] = R.T; w2f[3, :3] = t
    f2w = np.linalg.inv(w2f)
    cb.set("gm_w2f", w2f.ravel()); cb.set("gm_f2w", f2w.ravel())
    pw = P / 4.0 if (realistic or rng.random() < 0.7) else P
    cb.set("gv_probefield_pitch_size", [N[0], N[1], N[2], pw])
    cb.set("gv_probefield_pitch_size_rv", [1.0 / N[0], 1.0 / N[1], 1.0 / N[2], 1.0 / P])
    org_rel = (-t) @ R
    cb.set("gv_probefield_org", [*org_rel, P])
    dist = rng.uniform(-2, 2, 3)
    cb.set("gv_distance_pos", [*dist, 0.0])
    to_rel = lambda f: (np.asarray(f) - t) @ R          # field -> rel (R orthonormal: inv(R^T) = R)
    dir_rel = lambda d: np.asarray(d) @ R

    # ---- geometry (field coords)
    m = P * rng.uniform(0.2, 0.6)
    lo, hi = -m * np.ones(3), ext + m
    boxes, spheres = [], []
    for _ in range(int(rng.integers(1, 6))):
        c = rng.uniform(lo + P, hi - P); h = rng.uniform(0.3, 1.5, 3) * P
        if rng.random() < 0.3:      # pillar floor..ceiling
            h[1] = (hi[1] - lo[1]) / 2; c[1] = (hi[1] + lo[1]) / 2
        boxes.append((c - h, c + h))
    for _ in range(int(rng.integers(0, 4))):
        spheres.append((rng.uniform(lo + P, hi - P), rng.uniform(0.3, 1.2) * P))
    geo = Geo(lo, hi, boxes, spheres, open_top=rng.random() < 0.3)

    # ---- camera inside the room, outside obstacles
    for _ in range(100):
        E = rng.uniform(lo + 0.3 * P, hi - 0.3 * P)
        if not geo.inside_obstacle(E)[0]:
            break
    yaw = rng.uniform(0, 2 * math.pi); pitch = rng.uniform(-0.5, 0.3)
    fwd_f = np.array([math.cos(pitch) * math.sin(yaw), math.sin(pitch), math.cos(pitch) * math.cos(yaw)])
    right_f = np.cross([0, 1, 0], fwd_f); right_f /= np.linalg.norm(right_f)
    up_f = np.cross(fwd_f, right_f)
    E_w = to_rel(E) + dist
    Bw = np.stack([dir_rel(right_f), dir_rel(up_f), dir_rel(fwd_f)])     # rows: view axes in world
    V = np.eye(4); V[:3, :3] = Bw.T; V[3, :3] = -E_w @ Bw.T
    zn, zf = 0.05 * P, 400.0 * P
    Pm = persp(rng.uniform(0.9, 1.4), full_w / full_h, zn, zf)
    VP = V @ Pm
    p2w = np.linalg.inv(VP)
    cb.set("mP2W", p2w.ravel())
    v2w = np.eye(4); v2w[:3, :3] = Bw
    cb.set("mV2W", v2w.ravel())
    if cb.has("vEye"):
        cb.set("vEye", [*E_w, 1.0])

    # ---- G-buffer: ray-cast every full-res texel centre
    H, W = full_h, full_w
    xs = (np.arange(W) + 0.5) / W * 2 - 1
    ys = 1 - (np.arange(H) + 0.5) / H * 2
    gx, gy = np.meshgrid(xs, ys)
    nd = np.stack([gx.ravel(), gy.ravel(), np.zeros(gx.size), np.ones(gx.size)], 1) @ p2w
    fd = np.stack([gx.ravel(), gy.ravel(), np.ones(gx.size), np.ones(gx.size)], 1) @ p2w
    pn, pf = nd[:, :3] / nd[:, 3:], fd[:, :3] / fd[:, 3:]
    dw = pf - pn; dw /= np.linalg.norm(dw, axis=1, keepdims=True)
    df = dw @ R.T                                         # world dir -> field dir
    th, nf, mat = geo.cast(np.repeat(E[None], len(df), 0), df)
    sky = ~np.isfinite(th) | (th > 1e20)
    th = np.where(sky, 1.0, th)
    hitf = E + df * th[:, None]
    hw = np.concatenate([to_rel(hitf) + dist, np.ones((len(hitf), 1))], 1) @ VP
    depth = np.where(sky, 1.0, np.clip(hw[:, 2] / hw[:, 3], 0.0, 0.999999)).reshape(H, W)
    nw = dir_rel(nf)
    nv = nw @ Bw.T                                        # world normal -> view (n_w = n_v @ Bw)
    gu, gv = oct_encode(nv)
    g0 = rng.uniform(0, 1, size=(1, H, W, 4)).astype(np.float32)
    g0[0, :, :, 0] = gu.reshape(H, W); g0[0, :, :, 1] = gv.reshape(H, W)
    g1 = rng.uniform(0, 1, size=(1, H, W, 4)).astype(np.float32)
    rough = np.where(rng.random(H * W) < 0.35, rng.uniform(0.0, 0.08, H * W), rng.uniform(0.08, 0.5, H * W))
    rough = np.where(mat % 3 == 0, rough * 0.5, rough)    # some materials glossier
    g1[0, :, :, 3] = rough.reshape(H, W)
    dep = np.zeros((1, H, W, 4), np.float32); dep[0, :, :, 0] = depth
    st = np.zeros((1, H, W, 4), np.uint32)
    st[0, :, :, 1] = np.where(sky.reshape(H, W), 0, 1)
    if not realistic:
        st[0, :, :, 1] |= rng.integers(0, 8, size=(H, W)).astype(np.uint32) << 1
    if realistic:
        cb.set("gv_mask_param", [1.0, 1.0, 2.0, 0.5])      # stencil bit 0 = geometry
    else:
        cb.set("gv_mask_param", [1.0, 1.0, rng.uniform(0, 1), rng.uniform(0.05, 1)] if rng.random() < 0.8 else
               [0.0, 0.0, rng.uniform(0, 1), rng.uniform(0.05, 1)])
    cb.set("fRoughLmt", rng.uniform(0.3, 0.9) if realistic else rng.uniform(0.1, 0.9))

    # ---- probes
    ncell = int(N.prod())
    ref = np.full(ncell, -1, dtype=np.int64)
    ii, jj, kk = np.meshgrid(np.arange(N[0]), np.arange(N[1]), np.arange(N[2]), indexing="ij")
    cells = np.stack([ii.ravel(), jj.ravel(), kk.ravel()], 1)
    cells = cells[np.lexsort((cells[:, 0], cells[:, 1], cells[:, 2]))]     # i fastest
    fpos = cells * P
    jit = rng.random() < (0.3 if realistic else 0.5)
    if jit:                                               # relocated probes (pushed out of geometry)
        fpos = fpos + rng.normal(0, 0.1 * P, fpos.shape) * (rng.random(len(fpos)) < 0.3)[:, None]
    bad = geo.inside_obstacle(fpos)
    if not realistic:
        bad |= rng.random(len(fpos)) < rng.uniform(0.0, 0.3)
    octsize = 8 if realistic else int(rng.choice([6, 8]))
    slices = 2
    nprobe = int((~bad).sum())
    tx = max(4, int(math.ceil(math.sqrt(max(nprobe, 1) / slices))))
    ty = max(1, int(math.ceil(max(nprobe, 1) / (slices * tx))))
    aw, ah = tx * octsize, ty * octsize
    rvw = 1.0 if (realistic or rng.random() < 0.7) else 0.5
    cb.set("gv_tiledoct_pitch_octsize_rv", [1.0 / aw, 1.0 / ah, 1.0 / octsize, rvw])
    if cb.has("gv_tiledoct_pitch_octsize"):
        cb.set("gv_tiledoct_pitch_octsize", [aw, ah, octsize, 1.0 / rvw])
    unified = 0.0 if realistic else float(rng.integers(0, 2))
    cb.set("gv_unified_info", [unified, rng.uniform(0, 1), rng.uniform(0, 1), rng.uniform(0, 1)])
    nsl = slices + int(unified)
    depth_t = np.zeros((nsl, ah, aw, 4), np.float32)
    nrm_t = np.full((nsl, ah, aw, 4), 0.5, np.float32)
    rad_t = np.zeros((nsl, ah, aw, 4), np.float32)
    msk_t = np.zeros((nsl, ah, aw, 4), np.float32)
    # texel directions of one tile (border texels clamp to the nearest interior direction)
    tc = np.clip((np.arange(octsize) + 0.5 - 1) / (octsize - 2), 0, 1)
    tu, tv = np.meshgrid(tc, tc)
    tdir = oct_decode(tu.ravel(), tv.ravel())             # (oct*oct, 3): row = v, col = u
    albedo = rng.uniform(0.1, 1.0, size=(40, 3))
    sky_col = rng.uniform(1, 4, size=3)
    probes = []
    pid = 0
    for c in range(len(fpos)):
        if bad[c]:
            continue
        i, j, k = cells[c]
        idx = i + j * N[0] + k * N[0] * N[1]
        ref[idx] = pid
        sl, rem = divmod(pid, tx * ty)
        tyi, txi = divmod(rem, tx)
        tt, tn, tm = geo.cast(np.repeat(fpos[c][None], len(tdir), 0), tdir)
        skyd = tt > 1e20
        dd = np.where(skyd, 100.0 * P, tt)
        y0, x0 = tyi * octsize, txi * octsize
        blk = depth_t[sl + int(unified), y0:y0 + octsize, x0:x0 + octsize]
        var = (0.02 * dd) ** 2
        blk[..., 0] = dd.reshape(octsize, octsize); blk[..., 1] = (dd * dd + var).reshape(octsize, octsize)
        blk[..., 2] = dd.reshape(octsize, octsize); blk[..., 3] = 1.0
        nr = dir_rel(tn) * 0.5 + 0.5
        nrm_t[sl + int(unified), y0:y0 + octsize, x0:x0 + octsize, :3] = nr.reshape(octsize, octsize, 3)
        col = np.where(skyd[:, None], sky_col, albedo[np.clip(tm, 0, 39)] * (0.5 + 0.5 * np.abs(tn[:, 1:2])))
        rad_t[sl + int(unified), y0:y0 + octsize, x0:x0 + octsize, :3] = col.reshape(octsize, octsize, 3)
        rad_t[sl + int(unified), y0:y0 + octsize, x0:x0 + octsize, 3] = 1.0
        msk_t[sl + int(unified), y0:y0 + octsize, x0:x0 + octsize, 0] = np.where(skyd, 0.0, 1.0).reshape(octsize, octsize)
        flags = sum(1 << L for L in range(3) if i % (1 << L) == 0 and j % (1 << L) == 0 and k % (1 << L) == 0)
        if not realistic and rng.random() < 0.1:
            flags = int(rng.integers(0, 8))
        rel = to_rel(fpos[c])
        wgt = 1.0 if realistic else float(rng.choice([rng.uniform(0.1, 1.0), 1.0, 0.0], p=[0.6, 0.35, 0.05]))
        probes.append([rel[0], rel[1], rel[2], S.asfloat(flags), rng.uniform(0, 0.3), wgt,
                       float(txi), float(tyi), float(sl), float(octsize)])
        pid += 1
    nprobe = max(len(probes), 1)
    b = {}
    b[(0, 17)] = TypedBuffer([int(x) & 0xFFFFFFFF for x in ref], isint=True)
    b[(0, 16)] = TypedBuffer([S.f32(x) for p in probes for x in p] or [0.0] * 10)
    b[(0, 0)] = Texture([depth_t], "2darray")
    b[(0, 1)] = Texture([nrm_t], "2darray")
    b[(0, 6)] = Texture([msk_t], "2darray")
    b[(0, 10)] = Texture([rad_t, rad_t[:, ::2, ::2].copy(), rad_t[:, ::4, ::4].copy()], "2darray")
    sh = rng.normal(0, 0.5, size=(nprobe, 9, 3)).astype(np.float32)
    sh[:, 0, :] = np.abs(sh[:, 0, :]) + 0.5
    b[(1, 2)] = StructBuffer(sh.tobytes(), 108)
    b[(1, 4)] = TypedBuffer([float(x) for x in rng.uniform(0, 1, size=nprobe * 9).astype(np.float32)])
    b[(0, 14)] = Texture([st], "2d")
    b[(0, 12)] = Texture([g0], "2d")
    b[(0, 13)] = Texture([g1], "2d")
    b[(0, 11)] = Texture([dep], "2d")
    rlr = rng.uniform(0, 2, size=(1, H, W, 4)).astype(np.float32)
    rlr[..., 3] = rng.choice([0.0, 0.5, 1.0], p=[0.7, 0.15, 0.15], size=(1, H, W)) if realistic else \
        rng.choice([0.0, 0.3, 1.0], size=(1, H, W))
    b[(0, 7)] = Texture([rlr], "2d")

    # ---- the remaining parameters (meaning unknown / tuning values): plausible ranges as in scenes.py
    lod = 7 if realistic else int(rng.integers(0, 8))
    cb.set("gv_lod_param", [lod, int(rng.integers(0, 4)), int(rng.integers(0, 4)), int(rng.integers(0, 4))])
    cb.set("gv_indirect_weight", [rng.uniform(0.2, 2), 1.0 if realistic else rng.choice([0.0, 0.7, 1.0, 1.0, 1.0]),
                                  rng.uniform(0, 1), rng.uniform(0, 1)])
    cb.set("gv_indirect_weight_2", [rng.uniform(0, 1), 1.0 if realistic else rng.uniform(0, 1),
                                    rng.choice([0.0, 0.5, 1.0]), rng.uniform(0, 1)])
    cb.set("gv_trace_bias", [rng.uniform(0, 0.3), rng.uniform(0, 0.3), rng.uniform(0, 0.3), rng.uniform(0, 1)])
    if cb.has("gv_trace_bias2"):
        cb.set("gv_trace_bias2", [rng.uniform(0, 0.3), rng.uniform(0, 0.3), rng.uniform(0, 0.3), rng.uniform(0, 1)])
    cb.set("gv_probe_scale_offset", [1.0 if realistic else rng.uniform(0.5, 2), 0.0 if realistic else rng.uniform(-0.2, 0.2),
                                     rng.uniform(0, 1), rng.uniform(0, 1)])
    cb.set("gv_time_param", [rng.uniform(0, 10), 0, 0, 0])
    for opt in ("gv_distance_info", "gv_mip_info"):
        if cb.has(opt):
            cb.set(opt, [rng.uniform(0.5, 4), rng.uniform(0, 4), rng.uniform(0, 2), rng.uniform(0, 1)])
    for s in (9, 10, 11, 12, 13):
        b[(3, s)] = Sampler("linear" if s in (9, 10, 13) else "point", "clamp")
    b[(2, 0)] = cb.cbuf()
    b[(1, 0)] = RWTexture(out_w, out_h, 1)
    groups = [(gx_, gy_, 0) for gy_ in range((out_h + 7) // 8 + (0 if realistic else 1)) for gx_ in range((out_w + 7) // 8)]
    return Scene(b), groups
