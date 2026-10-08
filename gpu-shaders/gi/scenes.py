r"""scenes.py - synthetic but plausible inputs for the GI / fog shaders, for dxil_interp differential tests.

The values are random within ranges that drive every path of the shipped shaders (stencil pass/fail, viewport
edges, sky pixels, probe field inside/outside/fade, probe search hit at level 0/1/2/none, valid/invalid refs, probe
flags, Chebyshev visibility pass/fail, ...). Coverage of the ORIGINAL program's basic blocks is measured by verify.py,
so a block the generator never reaches is reported, not silently ignored.
"""
import math, struct
import numpy as np
import os as _os, sys as _sys
_sys.path.insert(0, _os.path.dirname(_os.path.abspath(__file__)))
from dxil_interp import Texture, TypedBuffer, StructBuffer, RWTexture, CBuffer, Sampler, Scene
from ll2hlsl import Iface, cb_leaf_map


class CB:
    """build a cbuffer image from member names using the shader's own layout."""

    def __init__(self, iface, cbname):
        name, mem, size = [c for c in iface.cbuffers if c[0] == cbname][0]
        self.size = size
        self.raw = bytearray((size + 15) // 16 * 16)
        self.lmap = cb_leaf_map(mem)
        self.byexpr = {}
        for (reg, comp), (expr, base) in self.lmap.items():
            self.byexpr[expr] = (reg, comp, base)

    def set(self, name, vals):
        if not isinstance(vals, (list, tuple, np.ndarray)):
            vals = [vals]
        vals = list(np.asarray(vals).ravel())
        keys = [k for k in self.byexpr if k == name or k.startswith(name + ".") or k.startswith(name + "[")]
        # order: row-major over [r].xyzw / .xyzw
        def order(k):
            r = 0
            m = k[len(name):]
            import re
            mm = re.match(r"^\[(\d+)\]", m)
            if mm:
                r = int(mm.group(1))
            c = "xyzw".find(k[-1]) if k[-2] == "." else 0
            return (r, c)
        keys.sort(key=order)
        if not keys:
            self.missing = getattr(self, "missing", []) + [name]     # member not in this shader's layout
            return self
        if len(vals) > len(keys):
            raise ValueError("%s: %d values for %d slots" % (name, len(vals), len(keys)))
        for k, v in zip(keys, vals):
            reg, comp, base = self.byexpr[k]
            off = reg * 16 + comp * 4
            if base in ("int", "uint"):
                struct.pack_into("<I", self.raw, off, int(v) & 0xFFFFFFFF)
            else:
                struct.pack_into("<f", self.raw, off, float(v))
        return self

    def has(self, name):
        return any(k == name or k.startswith(name + ".") or k.startswith(name + "[") for k in self.byexpr)

    def cbuf(self):
        return CBuffer(bytes(self.raw))


def f32(x):
    return float(np.float32(x))


def asfloat(u):
    return struct.unpack("<f", struct.pack("<I", u & 0xFFFFFFFF))[0]


def rot(rng):
    q = rng.normal(size=4)
    q /= np.linalg.norm(q)
    a, b, c, d = q
    return np.array([[a*a+b*b-c*c-d*d, 2*(b*c-a*d), 2*(b*d+a*c)],
                     [2*(b*c+a*d), a*a-b*b+c*c-d*d, 2*(c*d-a*b)],
                     [2*(b*d-a*c), 2*(c*d+a*b), a*a-b*b-c*c+d*d]])


def tex2d(arr):
    a = np.asarray(arr, dtype=arr.dtype)
    if a.ndim == 3:
        a = a[None]
    return Texture([a], "2d")


def refprobe_scene(text, seed, out_w=24, out_h=16, variant=None, realistic=False):
    """inputs for RefProbe-* (#121 diffuse, #414 reflection). `text` = the shader's .ll (layout source).
    realistic=True: a populated field as the game builds it (probes present at every cell, level bit L set where
    the cell is on the 2^L lattice, few invalid probes, all LOD levels enabled) - used for the performance model;
    the default random scenes are for path coverage."""
    rng = np.random.default_rng(seed)
    ifc = Iface(text)
    cb = CB(ifc, "$Globals")
    names = {r["name"] for r in ifc.res}
    scale = float(rng.choice([1.0, 1.25, 2.0]))
    full_w, full_h = int(round(out_w * scale)), int(round(out_h * scale))
    cb.set("gv_info_out", [out_w - int(rng.integers(0, 3)), out_h - int(rng.integers(0, 3)), 1.0 / out_w, rng.uniform(0.2, 1.5)])
    cb.set("gv_info_in", [full_w, full_h, scale, 1.0 / scale])
    vp = [rng.choice([0.0, 0.05]), rng.choice([0.0, 0.1]), 0.0, 0.0]
    vp[2] = 1.0 - vp[0] - rng.choice([0.0, 0.05]); vp[3] = 1.0 - vp[1] - rng.choice([0.0, 0.05])
    cb.set("gv_viewport", vp)
    # probe field: N cells per axis, pitch P; field coords f = (world - dist) * R + t
    N = np.array([int(rng.integers(4, 9)), int(rng.integers(3, 6)), int(rng.integers(4, 9))]) if not realistic else         np.array([int(rng.integers(12, 21)), int(rng.integers(8, 13)), int(rng.integers(12, 21))])
    P = float(rng.choice([0.5, 1.0, 2.0]))
    R = rot(rng) if rng.random() < 0.5 else np.eye(3)
    t = rng.uniform(-1, 1, 3)
    ext = (N - 1) * P
    w2f = np.eye(4); w2f[:3, :3] = R.T; w2f[3, :3] = t           # row-vector convention: f = rel @ M + t
    f2w = np.linalg.inv(w2f)
    cb.set("gm_w2f", w2f.ravel())
    cb.set("gm_f2w", f2w.ravel())
    pw = P / 4.0 if rng.random() < 0.7 else P / float(rng.choice([1.0, 2.0]))
    cb.set("gv_probefield_pitch_size", [N[0], N[1], N[2], P if realistic else (pw if rng.random() < 0.5 else P)])
    cb.set("gv_probefield_pitch_size_rv", [1.0 / N[0], 1.0 / N[1], 1.0 / N[2], 1.0 / P])
    org_rel = (-t) @ np.linalg.inv(R.T)
    cb.set("gv_probefield_org", [*org_rel, P])
    dist = rng.uniform(-2, 2, 3)
    cb.set("gv_distance_pos", [*dist, 0.0])
    # camera: world = ndc.x*R0 + (-ndc.y)*R1 + depth*R2 + R3 (w = 1), centred on the field (with margins outside)
    centre_f = ext * rng.uniform(0.3, 0.7, 3)
    centre_rel = (centre_f - t) @ np.linalg.inv(R.T)
    span = ext * rng.uniform(0.5, 0.9) + P if not realistic else ext * rng.uniform(0.25, 0.5)
    p2w = np.zeros((4, 4))
    p2w[0, :3] = [span[0] * 0.6, 0, 0]
    p2w[1, :3] = [0, span[1] * 0.6, 0]
    p2w[2, :3] = [0, 0, span[2] * 1.2]
    p2w[3, :3] = centre_rel + dist - np.array([0, 0, span[2] * 0.6])
    p2w[:, 3] = [0, 0, 0, 1]
    if rng.random() < 0.3:                                      # mild perspective
        p2w[2, 3] = rng.uniform(-0.2, 0.2)
    cb.set("mP2W", p2w.ravel())
    v2w = np.eye(4); v2w[:3, :3] = rot(rng)
    cb.set("mV2W", v2w.ravel())
    dense = (seed % 4 == 0) or realistic   # fully populated field, probes fully weighted ('weight reached 1' exits)
    mz, mw = (2.0, 0.5) if dense else (rng.uniform(0, 1), rng.uniform(0.05, 1))
    cb.set("gv_mask_param", [1.0, 1.0, mz, mw] if rng.random() < 0.8 else [0.0, 0.0, mz, mw])
    lod = (7 if realistic else int(rng.integers(0, 8))) if variant is None else variant
    cb.set("gv_lod_param", [lod, int(rng.integers(0, 4)), int(rng.integers(0, 4)), int(rng.integers(0, 4))])
    cb.set("gv_indirect_weight", [rng.uniform(0.2, 2), rng.choice([0.0, 0.7, 1.0, 1.0, 1.0]), rng.uniform(0, 1), rng.uniform(0, 1)])
    cb.set("gv_indirect_weight_2", [rng.uniform(0, 1), rng.uniform(0, 1), rng.choice([0.0, 0.5, 1.0]), rng.uniform(0, 1)])
    cb.set("gv_trace_bias", [rng.uniform(0, 0.3), rng.uniform(0, 0.3), rng.uniform(0, 0.3), rng.uniform(0, 1)])
    if cb.has("gv_trace_bias2"):
        cb.set("gv_trace_bias2", [rng.uniform(0, 0.3), rng.uniform(0, 0.3), rng.uniform(0, 0.3), rng.uniform(0, 1)])
    cb.set("gv_probe_scale_offset", [rng.uniform(0.5, 2), rng.uniform(-0.2, 0.2), rng.uniform(0, 1), rng.uniform(0, 1)])
    cb.set("gv_unified_info", [float(rng.integers(0, 2)), rng.uniform(0, 1), rng.uniform(0, 1), rng.uniform(0, 1)])
    cb.set("gv_time_param", [rng.uniform(0, 10), 0, 0, 0])
    for opt in ("gv_distance_info", "gv_mip_info", "vEye", "gv_tiledoct_pitch_octsize"):
        if cb.has(opt):
            cb.set(opt, [rng.uniform(0.5, 4), rng.uniform(0, 4), rng.uniform(0, 2), rng.uniform(0, 1)])
    if cb.has("fRoughLmt"):
        cb.set("fRoughLmt", rng.uniform(0.1, 0.9))
    # octahedral atlas
    octsize = int(rng.choice([6, 8]))
    tiles = 6
    aw = tiles * octsize
    slices = 3
    cb.set("gv_tiledoct_pitch_octsize_rv", [1.0 / aw, 1.0 / aw, 1.0 / octsize, 1.0 if rng.random() < 0.7 else 0.5])
    # probes
    ncell = int(N.prod())
    ref = np.full(ncell, -1, dtype=np.int64)
    valid_p = (0.97 if realistic else 1.0) if dense else rng.uniform(0.5, 0.95)
    probes = []
    for k in range(N[2]):
        for j in range(N[1]):
            for i in range(N[0]):
                if rng.random() < valid_p:
                    idx = i + j * N[0] + k * N[0] * N[1]
                    ref[idx] = len(probes)
                    fpos = np.array([i, j, k]) * P + rng.normal(0, 0.15 * P, 3) * (rng.random() < 0.5)
                    rel = (fpos - t) @ np.linalg.inv(R.T)
                    flags = int(rng.integers(0, 8)) | (int(rng.integers(0, 0x10000)) << 3 & 0xFFF8 if rng.random() < 0.2 else 0)
                    if rng.random() < 0.15:
                        flags |= 0x7
                    if realistic:
                        flags = sum(1 << L for L in range(3) if i % (1 << L) == 0 and j % (1 << L) == 0 and k % (1 << L) == 0
                                    and rng.random() < 0.97)
                    info = [rel[0], rel[1], rel[2], asfloat(flags),
                            rng.uniform(0, 1) if not realistic else rng.uniform(0, 0.3), rng.uniform(0.1, 1.0) if dense else rng.choice([rng.uniform(0.1, 1.0), 0.0, -0.5], p=[0.85, 0.1, 0.05]),
                            float(rng.integers(0, tiles)), float(rng.integers(0, tiles)), float(rng.integers(0, slices)), float(octsize)]
                    probes.append(info)
    nprobe = max(len(probes), 1)
    info = [f32(x) for p in probes for x in p] or [0.0] * 10
    b = {}
    b[(0, 17)] = TypedBuffer([int(x) & 0xFFFFFFFF for x in ref], isint=True)
    b[(0, 16)] = TypedBuffer(info)
    # octahedral depth (mean, mean^2 + var) in distance units (~ 0..ext)
    mean = rng.uniform(0.0, 1.2 * float(ext.max()) + P, size=(slices, aw, aw)).astype(np.float32)
    var = rng.uniform(0, 0.5, size=(slices, aw, aw)).astype(np.float32)
    oct_depth = np.stack([mean, mean * mean + var, rng.uniform(0, 1, size=mean.shape), rng.uniform(0, 1, size=mean.shape)], -1).astype(np.float32)
    b[(0, 0)] = Texture([oct_depth], "2darray")
    # SH coefficients and sky occlusion per probe
    sh = rng.normal(0, 0.5, size=(nprobe, 9, 3)).astype(np.float32)
    sh[:, 0, :] = np.abs(sh[:, 0, :]) + 0.5
    b[(1, 2)] = StructBuffer(sh.tobytes(), 108)
    sky = rng.uniform(0, 1, size=nprobe * 9).astype(np.float32)
    b[(1, 4)] = TypedBuffer([float(x) for x in sky])
    # G-buffer at full res
    H, W = full_h, full_w
    st = rng.integers(0, 8, size=(H, W, 4)).astype(np.uint32)
    b[(0, 14)] = Texture([st[None]], "2d")
    g0 = rng.uniform(0, 1, size=(1, H, W, 4)).astype(np.float32)
    b[(0, 12)] = Texture([g0], "2d")
    g1 = rng.uniform(0, 1, size=(1, H, W, 4)).astype(np.float32)
    b[(0, 13)] = Texture([g1], "2d")
    dep = rng.uniform(0.02, 0.98, size=(1, H, W, 4)).astype(np.float32)
    sky_mask = rng.random(size=(H, W)) < 0.08
    dep[0][sky_mask] = 1.0
    b[(0, 11)] = Texture([dep], "2d")
    # reflection extras (#414)
    ow = aw
    rad = rng.uniform(0, 4, size=(slices, ow, ow, 4)).astype(np.float32)
    b[(0, 10)] = Texture([rad, rad[:, ::2, ::2].copy(), rad[:, ::4, ::4].copy()], "2darray")
    nrm = rng.uniform(0, 1, size=(slices, ow, ow, 4)).astype(np.float32)
    b[(0, 1)] = Texture([nrm], "2darray")
    msk = rng.uniform(0, 1, size=(slices, ow, ow, 4)).astype(np.float32)
    b[(0, 6)] = Texture([msk], "2darray")
    rlr = rng.uniform(0, 2, size=(1, H, W, 4)).astype(np.float32)
    rlr[..., 3] = rng.choice([0.0, 0.3, 1.0], size=(1, H, W))
    b[(0, 7)] = Texture([rlr], "2d")
    # samplers (filter choice only changes the synthetic data path, identical for both programs)
    for s in (9, 10, 11, 12, 13):
        b[(3, s)] = Sampler("linear" if s in (9, 10, 13) else "point", "clamp")
    b[(2, 0)] = cb.cbuf()
    b[(1, 0)] = RWTexture(out_w, out_h, 1)
    groups = [(gx, gy, 0) for gy in range((out_h + 7) // 8 + 1) for gx in range((out_w + 7) // 8)]
    return Scene(b), groups


def vfscat_scene(text, seed, realistic=False):
    """VFSCAT #235 (volumetric fog) inputs: see gpu-shaders\fog\vfscene.py."""
    fog = _os.path.join(_os.path.dirname(_os.path.dirname(_os.path.abspath(__file__))), "fog")
    if fog not in _sys.path:
        _sys.path.insert(0, fog)
    import vfscene
    return vfscene.vfscat_scene(text, seed, realistic=realistic)
