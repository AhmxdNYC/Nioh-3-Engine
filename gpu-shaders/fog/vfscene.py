r"""vfscene.py - synthetic inputs for VFSCAT #235 (volumetric fog froxel scattering, 8x8x4 groups, RWTexture3D out).

    vfscat_scene(text, seed, realistic=False) -> (Scene, groups)      (gi\scenes.vfscat_scene forwards here)

Geometry is consistent: froxel (x,y,z) -> view ray (gAngleAndNearFar tan/near, exponential depth gCalcVolumeFactor.w)
-> world (gV2W); clustered light lists are built from real sphere-vs-cell overlap (30x17 tiles of 64 px, 16 log
slices), so neighbouring z slices share lights (LDS de-dup path) exactly like the game's lists.
Coverage scenes (default) randomise every switch the shader has: parallel-light count 0-3 and shadow mode
(none / exp / compare, single map / 4 cascades), VSM levels 0-16 + resident-page density + valid-clip mask, depth
exactly 0/1 (VSM skip), point/spot light types (poly / exp / IES), shadowed or not (cube/2D, exp or compare),
disabled lights (radius <= 0), > 16 applied lights per thread (LDS list full), slices >= 16, LPV box/fade,
fog noise on/off, height fog, temporal reprojection on/off/outside.
realistic=True: one shadowed sun with 4 cascades + 8 VSM levels with mostly resident pages, 24 point / 8 spot
lights (VFSCAT_LIGHTS=moderate: 10 point / 3 spot,
heavy: 24 / 8, none) near the camera (40 % shadowed, 15 % IES), LPV + noise + reprojection on - the performance-model scene.
"""
import math, struct
import numpy as np
import os as _os, sys as _sys
_GI = _os.path.join(_os.path.dirname(_os.path.dirname(_os.path.abspath(__file__))), "gi")
_sys.path.insert(0, _GI)
from dxil_interp import Texture, TypedBuffer, StructBuffer, RWTexture, Sampler, Scene  # noqa: E402
from ll2hlsl import Iface  # noqa: E402
from scenes import CB, rot  # noqa: E402

SCR = (1920.0, 1080.0)
NTX, NTY, NSL = 30, 17, 16


def f4(*v):
    return [float(x) for x in v]


def _setmat(cb, name, M):
    cb.set(name, np.asarray(M, dtype=np.float64).ravel())


def vfscat_scene(text, seed, realistic=False):
    rng = np.random.default_rng(seed + (7919 if realistic else 0))
    R = realistic
    ifc = Iface(text)
    g = CB(ifc, "$Globals")
    f = CB(ifc, "VolumetricFogConstBuf")
    pick = lambda p: rng.random() < p          # noqa: E731
    # ---------------------------------------------------------------- volume + camera
    W, H, D = (160, 90, 64) if R else (int(rng.choice([16, 24, 160])), int(rng.choice([8, 16, 90])), int(rng.choice([8, 16, 64])))
    near, far = (0.5, 96.0) if R else (float(rng.uniform(0.2, 1.0)), float(rng.uniform(30, 150)))
    gx, gy, gz = (W + 7) // 8, (H + 7) // 8, (D + 3) // 4
    ng = 2 if R else 1
    groups = []
    while len(groups) < ng:
        gg = (int(rng.integers(0, gx)), int(rng.integers(0, gy)), int(rng.integers(0, gz)) if not R else int(rng.integers(0, max(gz * 3 // 4, 1))))
        if gg not in groups:
            groups.append(gg)
    axis_spot = (not R) and pick(0.25)          # one spot aimed exactly down a froxel ray (segment || axis path)

    tanY = float(rng.uniform(0.55, 0.75)); tanX = tanY * SCR[0] / SCR[1]
    eye = rng.uniform(-20, 20, 3)
    Rv = rot(rng)                                # rows = view x, y, z axes in world (row-vector: world = v @ V2W)
    V2W = np.eye(4); V2W[:3, :3] = Rv; V2W[3, :3] = eye
    W2V = np.linalg.inv(V2W)
    f.set("gV2W", V2W.ravel())
    f.set("gAngleAndNearFar", f4(tanX, tanY, near, far))
    A, B = far / (far - near), -far * near / (far - near)
    f.set("gCalcVolumeFactor", f4(-B / near, -A, far / near, math.log(far / near)))
    f.set("gEyePosAndFogFar", f4(*eye, far))

    def proj(eye_off, rv):
        v2w = np.eye(4); v2w[:3, :3] = rv; v2w[3, :3] = eye + eye_off
        P = np.zeros((4, 4)); P[0, 0] = 1 / tanX; P[1, 1] = 1 / tanY; P[2, 2] = -A; P[3, 2] = B; P[2, 3] = -1.0
        return np.linalg.inv(v2w) @ P
    # current W2P: reverse-Z infinite (z/w = near/d) unless the variant forces depth 0 / 1 (VSM skip path)
    W2P = np.zeros((4, 4)); W2P[:, 3] = -W2V[:, 2]
    dmode = 0 if R else int(rng.choice([0, 0, 0, 1, 2]))
    W2P[3, 2] = near if dmode == 0 else 0.0
    if dmode == 2:
        W2P[:, 2] = W2P[:, 3]                      # z == w -> depth exactly 1
    W2P[:, 0] = W2V[:, 0] / tanX; W2P[:, 1] = W2V[:, 1] / tanY
    f.set("gW2P", W2P.ravel())
    prev = proj(rng.normal(0, 0.05 if R else 0.6, 3), Rv if (R or pick(0.6)) else rot(rng))
    f.set("gPrevW2P", prev.ravel())
    jit = (R or pick(0.7)) and not axis_spot
    f.set("gInjectionDesc", f4(float(rng.integers(1, 16)) if jit else 0.0, (0.9 if R else rng.choice([0.0, 0.5, 0.95])),
                               rng.uniform(0.2, 1.0) if (R or pick(0.8)) else 0.0, 0.0))
    # cluster slices: slice = log2(depth * x + y) * z + w with depth = near / d
    ncl, fcl = near, (far * 0.6 if R else far * rng.uniform(0.3, 1.0))
    s = NSL / math.log2(fcl / ncl)
    y0 = 0.0 if dmode == 0 else 1.0
    g.set("vClstZPrm", f4(1.0, y0, -s, s * math.log2(near / ncl)))
    g.set("nClstPtch", [NTX, NTX * NTY])
    g.set("g_vScrnSize", f4(SCR[0], SCR[1], 1 / SCR[0], 1 / SCR[1]))
    g.set("vEye", f4(*(eye + rng.normal(0, 0.1, 3)), 1.0))
    g.set("vCllPkPrm", [0.0, 0.0, 0.0, rng.uniform(0.0, 0.5), 0.0, 0.0, 0.0, 0.0])

    def froxel_world(ix, iy, zf):
        nx = (ix + 0.5) / W * 2 - 1; ny = 1 - (iy + 0.5) / H * 2
        d = near * math.exp(zf * math.log(far / near))
        return np.array([tanX * nx * d, tanY * ny * d, -d, 1.0]) @ V2W

    # ---------------------------------------------------------------- local lights
    def place(n):
        out = []
        for _ in range(n):
            ix, iy = rng.uniform(0, W), rng.uniform(0, H)
            zf = rng.uniform(0.0, 0.8) if R else rng.uniform(0, 1.0)
            out.append(froxel_world(ix - 0.5, iy - 0.5, zf)[:3] + rng.normal(0, 1.0, 3))
        return out
    lv = _os.environ.get("VFSCAT_LIGHTS", "moderate")      # realistic light load: none | moderate | heavy
    nP = {"none": 0, "moderate": 10, "heavy": 24}[lv] if R else int(rng.choice([0, 3, 12, 40]))
    nS = {"none": 0, "moderate": 3, "heavy": 8}[lv] if R else int(rng.choice([0, 2, 8, 24]))
    Ppos = place(nP); Spos = place(nS)
    Prad = [rng.uniform(2, 7) if R else rng.choice([rng.uniform(1, 12), rng.uniform(15, 40), 0.0, -1.0], p=[.75, .15, .05, .05]) for _ in range(nP)]
    Srad = [rng.uniform(4, 12) if R else rng.choice([rng.uniform(2, 20), rng.uniform(20, 50), 0.0], p=[.8, .15, .05]) for _ in range(nS)]
    Sdir = [-(p - eye) / np.linalg.norm(p - eye) * 0.3 + rng.normal(0, 1, 3) for p in Spos]
    Sdir = [d / np.linalg.norm(d) for d in Sdir]
    if axis_spot:
        nS = max(nS, 1)
        ix, iy, iz = groups[0][0] * 8 + int(rng.integers(0, 8)), groups[0][1] * 8 + int(rng.integers(0, 8)), groups[0][2] * 4 + int(rng.integers(0, 4))
        pf = froxel_world(ix, iy, (iz + 0.5) / D)[:3]
        dr = (pf - eye) / np.linalg.norm(pf - eye)
        if not Spos:
            Spos, Srad, Sdir = [None], [None], [None]
        Spos[0], Srad[0], Sdir[0] = pf - dr * 2.0, 12.0, dr
    # cells: tile ndc rectangle x slice depth range -> bounding sphere
    cells = {}
    corners = []
    for sl in range(NSL + 2):
        d0 = ncl * 2 ** (sl / s); d1 = ncl * 2 ** ((sl + 1) / s)
        for ty in range(NTY):
            for tx in range(NTX):
                x0, x1 = tx * 64 / SCR[0] * 2 - 1, min((tx + 1) * 64 / SCR[0], 1.0) * 2 - 1
                y0_, y1_ = 1 - ty * 64 / SCR[1] * 2, 1 - min((ty + 1) * 64 / SCR[1], 1.0) * 2
                pts = [np.array([tanX * x * d, tanY * y * d, -d, 1.0]) @ V2W for x in (x0, x1) for y in (y0_, y1_) for d in (d0, d1)]
                pts = np.array(pts)[:, :3]
                c = pts.mean(0); r = float(np.max(np.linalg.norm(pts - c, axis=1)))
                cells[(tx, ty, sl)] = (c, r)
    lists = {}
    for k, (c, r) in cells.items():
        if k[2] >= NSL:
            continue
        pl = [i for i in range(nP) if Prad[i] > 0 and np.linalg.norm(Ppos[i] - c) < r + Prad[i]]
        sp = [i for i in range(nS) if np.linalg.norm(Spos[i] - c) < r + Srad[i]]
        if not R:
            if pick(0.15):
                pl += [int(x) for x in rng.integers(0, max(nP, 1), int(rng.integers(0, 4)))] if nP else []
            if pick(0.15):
                sp += [int(x) for x in rng.integers(0, max(nS, 1), int(rng.integers(0, 3)))] if nS else []
            if nP and pick(0.03):
                pl += list(range(nP))          # >16 lights in one cell
            rng.shuffle(pl); rng.shuffle(sp)
        lists[k] = (pl, sp)
    hdr = [0] * (NTX * NTY * NSL * 4)
    body = []
    base = len(hdr)
    for k, (pl, sp) in lists.items():
        tx, ty, sl = k
        cell = sl * NTX * NTY + ty * NTX + tx
        off = base + len(body)
        body += pl + sp
        hdr[cell * 4:cell * 4 + 4] = [off & 0xFFFF, off >> 16, len(pl), len(sp)]
    b = {}
    b[(0, 16)] = TypedBuffer(hdr + body + [0] * 4, isint=True)
    # point lights
    ptype = [int(rng.choice([0, 1, 2, 3], p=[.45, .1, .3, .15])) for _ in range(nP)]
    b[(0, 17)] = TypedBuffer([tuple(f4(*Ppos[i], Prad[i])) for i in range(nP)] or [(0.0,) * 4])
    attr, exa, info, sca = [], [], bytearray(), []
    for i in range(nP):
        col = rng.uniform(0.2, 4, 3)
        if ptype[i] == 3:
            up = rng.normal(0, 1, 3); up /= np.linalg.norm(up)
            dr = rng.normal(0, 1, 3); dr -= up * dr.dot(up); dr /= np.linalg.norm(dr)
            attr += [tuple(col), tuple(up), tuple(dr)]
        elif ptype[i] == 2:
            attr += [tuple(col), (rng.uniform(0.01, 0.5), rng.uniform(0.5, 2), 0.0), tuple(rng.normal(0, 1, 3))]
        else:
            attr += [tuple(col), (1.0, rng.uniform(0, 0.5), rng.uniform(0, 0.3)), tuple(rng.normal(0, 1, 3))]
        exa += [ptype[i], 0, int(rng.integers(0, 4))]
        sh = (0.4 if R else 0.6)
        mode = float(rng.choice([2.0, 1.0, 0.0], p=[1 - sh, sh / 2, sh / 2]))
        info += struct.pack("<16f", mode, 0, rng.uniform(0.5, 4), 0, *rng.uniform(0.3, 2, 3), 0,
                            rng.uniform(-0.5, 0.5), 0, 0, 0, 1.0 / max(abs(Prad[i]), 1), rng.uniform(-0.05, 0.05), 0, 0)
        sca += [rng.uniform(0.2, 2), rng.uniform(-0.8, 0.8)]
    b[(0, 18)] = TypedBuffer([tuple(float(x) for x in a) for a in attr] or [(0.0,) * 3])
    b[(0, 19)] = TypedBuffer(exa or [0], isint=True)
    b[(0, 26)] = StructBuffer(bytes(info) or bytes(64), 64)
    b[(0, 28)] = TypedBuffer([float(x) for x in sca] or [0.0])
    # spot lights
    sattr, sexa, sinfo, ssca = [], [], bytearray(), []
    for i in range(nS):
        typ = int(rng.choice([0, 2, 3], p=[.55, .3, .15]))
        cosO = rng.uniform(0.5, 0.95); cosI = min(cosO + rng.uniform(0.02, 0.2), 0.999)
        sc_ = 1.0 / (cosI - cosO)
        rsq = Srad[i] ** 2 if Srad[i] > 0 else (0.0 if pick(0.5) else -1.0)
        up = rng.normal(0, 1, 3); up -= Sdir[i] * up.dot(Sdir[i]); up /= np.linalg.norm(up)
        att = tuple(up) if typ == 3 else ((rng.uniform(0.01, 0.5), rng.uniform(0.5, 2), 0.0) if typ == 2 else (1.0, rng.uniform(0, .5), rng.uniform(0, .3)))
        sattr += [tuple(Spos[i]), tuple(Sdir[i]), tuple(rng.uniform(0.2, 4, 3)), att, (rsq, -cosO * sc_, sc_)]
        sexa += [typ, 0, int(rng.integers(0, 4))]
        # shadow matrix: world -> spot clip (uv in [0,1] around the cone, z = depth / range)
        z = -Sdir[i]; x = np.cross(up, z); x /= np.linalg.norm(x); y = np.cross(z, x)
        v2w = np.eye(4); v2w[:3, :3] = np.array([x, y, z]); v2w[3, :3] = Spos[i]
        t = math.tan(math.acos(cosO))
        P = np.zeros((4, 4)); P[0, 0] = 0.5 / t; P[1, 1] = -0.5 / t; P[2, 0] = -0.5; P[2, 1] = -0.5
        P[2, 2] = -1.0 / max(Srad[i], 1); P[2, 3] = -1.0
        M = np.linalg.inv(v2w) @ P
        sh = (0.4 if R else 0.6)
        mode = float(rng.choice([2.0, 1.0, 0.0], p=[1 - sh, sh / 2, sh / 2]))
        sinfo += struct.pack("<12f", mode, 0, rng.uniform(0.5, 4), 0, *rng.uniform(0.3, 2, 3), 0, rng.uniform(-0.5, .5), 0, 0, 0)
        sinfo += struct.pack("<16f", *M.ravel())
        ssca += [rng.uniform(0.2, 2), rng.uniform(-0.8, 0.8)]
    b[(0, 20)] = TypedBuffer([tuple(float(v) for v in a) for a in sattr] or [(0.0,) * 3])
    b[(0, 21)] = TypedBuffer(sexa or [0], isint=True)
    b[(0, 27)] = StructBuffer(bytes(sinfo) or bytes(112), 112)
    b[(0, 29)] = TypedBuffer([float(x) for x in ssca] or [0.0])
    # textures for local lights
    b[(0, 0)] = Texture([rng.uniform(0, 2, (4, 16, 16, 4)).astype(np.float32)], "2darray")
    cube = rng.uniform(0.0, 1.0, (6 * max(nP, 1), 4, 4, 4)).astype(np.float32)
    b[(0, 13)] = Texture([cube], "cubearray")
    b[(0, 12)] = Texture([rng.uniform(0.0, 1.0, (max(nS, 1), 8, 8, 4)).astype(np.float32)], "2darray")
    # ---------------------------------------------------------------- parallel lights + VSM
    nPar = 1 if R else int(rng.choice([0, 1, 1, 2, 3]))
    f.set("gLightInfo", [nPar, 0, 0, 0])
    par = bytearray()
    for i in range(max(nPar, 1)):
        mode = 0.0 if R else float(rng.choice([2.0, 1.0, 0.0]))
        casc = R or pick(0.6)
        L = rng.normal(0, 1, 3); L /= np.linalg.norm(L)
        par += struct.pack("<12f", mode, 0, rng.uniform(0.5, 4), 0, *rng.uniform(0.3, 2, 3), 0, rng.uniform(-0.6, 0.6), 0, 0, 0)
        par += struct.pack("<8f", *L, 0, *rng.uniform(0.5, 3, 3), 0)
        par += struct.pack("<4f", 1.0 if casc else 0.0, 0, 0, 0)
        z = -L; up = np.array([0, 1.0, 0]) if abs(L[1]) < 0.9 else np.array([1.0, 0, 0])
        x = np.cross(up, z); x /= np.linalg.norm(x); y = np.cross(z, x)
        for c in range(4):
            ext = 8.0 * 3 ** c
            v2w = np.eye(4); v2w[:3, :3] = np.array([x, y, z]); v2w[3, :3] = eye + rng.normal(0, 2, 3)
            O = np.diag([0.5 / ext, -0.5 / ext, -1.0 / (4 * ext), 1.0]); O[3, :3] = [0.5, 0.5, 0.5]
            par += struct.pack("<16f", *(np.linalg.inv(v2w) @ O).ravel())
        segs = sorted(rng.uniform(near, far * 0.8, 3)) if not R else [8.0, 24.0, 60.0]
        par += struct.pack("<4f", *segs, 0)
        for c in range(4):
            par += struct.pack("<4f", 0.5, 0.5, 0.5 * (c & 1), 0.5 * (c >> 1))
    b[(0, 25)] = StructBuffer(bytes(par), 432)
    b[(0, 10)] = Texture([rng.uniform(0.0, 1.0, (1, 32, 32, 4)).astype(np.float32)], "2d")
    nV = 8 if R else int(rng.choice([0, 1, 4, 8, 16]))
    g.set("nArraySize_VirtualSM", nV)
    g.set("nClipLevelMin", 3 if R else int(rng.integers(0, 6)))
    g.set("nClipmapLevelBias", 0 if R else int(rng.choice([0, 0, 1, 3])))
    g.set("nValidClipmapFlag", 0xFFFF if (R or pick(0.6)) else int(rng.integers(0, 0x10000)))
    pages = 16
    g.set("scMinMipMapVirtualSM", f4(pages, pages, pages * pages, 0))
    Lsun = np.array([0.3, -0.8, 0.5]); Lsun /= np.linalg.norm(Lsun)
    z = -Lsun; x = np.cross([0, 1.0, 0], z); x /= np.linalg.norm(x); y = np.cross(z, x)
    for l in range(16):
        ext = 4.0 * 2 ** l
        v2w = np.eye(4); v2w[:3, :3] = np.array([x, y, z]); v2w[3, :3] = eye
        O = np.diag([0.5 / ext, -0.5 / ext, -1.0 / (8 * ext), 1.0]); O[3, :3] = [0.5, 0.5, 0.5]
        _setmat(g, "mW2S_VirtualSM[%d]" % l, np.linalg.inv(v2w) @ O)
        g.set("vClipOriginOffset[%d]" % l, f4(*rng.uniform(0, 1, 2)))
    dens = 0.85 if R else float(rng.choice([0.0, 0.3, 0.7, 1.0]))
    flags = (rng.random(pages * pages * max(nV, 1)) < dens).astype(np.int64) | (rng.integers(0, 4, pages * pages * max(nV, 1)) << 1)
    b[(0, 32)] = TypedBuffer([int(v) for v in flags], isint=True)
    b[(0, 2)] = Texture([rng.uniform(0.0, 1.0, (max(nV, 1), 16, 16, 4)).astype(np.float32)], "2darray")
    # ---------------------------------------------------------------- LPV, noise, scattering, reprojection
    lext = rng.uniform(20, 60)
    M = np.eye(4) / lext; M[3, :3] = -(eye - lext / 2) / lext; M[3, 3] = 1
    _setmat(g, "mTrsW2LPV[0]", M)
    Msr = M.copy() if (R or pick(0.6)) else M * 1.6
    Msr[3, 3] = 1
    _setmat(g, "mTrsW2LPVSR[0]", Msr)
    dep = 16 if R else int(rng.choice([8, 16, 32]))
    g.set("nLPV3DDepth", dep)
    g.set("fDiffuseScale", rng.uniform(0.2, 2)); g.set("fNormalDir", rng.choice([1.0, -1.0, 0.5]))
    g.set("fSHBlendRegion", f4(0.1 if R else rng.choice([0.0, 0.05, 0.2]), 0, 0, 0))
    b[(0, 1)] = Texture([rng.normal(0.3, 0.5, (3 * dep, 16, 16, 4)).astype(np.float32)], "2darray")
    g.set("g_vFltOrgOfs", f4(*rng.uniform(-100, 100, 3)))
    f.set("gFogNoiseInfo", f4(*rng.uniform(0, 50, 3), 0.15 if R else rng.choice([0.0, 0.1, 0.7])))
    perm = rng.permutation(256)
    pt = np.zeros((1, 256, 256, 4), np.float32)
    pt[0, :, :, 0] = (perm[np.arange(256)[None, :]] + np.arange(256)[:, None]) % 256 / 256.0
    pt[0, :, :, 1:] = rng.uniform(0, 1, (256, 256, 3))
    b[(0, 8)] = Texture([pt], "2d")
    f.set("gScatteringInfo", f4(rng.uniform(0.01, 0.2), rng.uniform(-10, 30) + eye[1], 0, 0))
    f.set("gAccParams", f4(rng.uniform(0.5, 2), 0, 0, 0))
    f.set("gAttParam", f4(0, 0, rng.uniform(-5, 20), rng.uniform(0.0, 0.2)))
    vs = rng.uniform(0, 1, (8, 16, 16, 4)).astype(np.float32)
    vs[..., 3] = rng.choice([-1.0, 0.0, 0.5, 1.0], size=vs.shape[:3]) if not R else 1.0
    b[(0, 9)] = Texture([vs], "3d")
    # ---------------------------------------------------------------- samplers, cbuffers, output, groups
    for sreg, filt, addr, cmp in ((0, "linear", "clamp", None), (1, "linear", "clamp", None), (2, "linear", "clamp", "lesseq"),
                                  (8, "point", "wrap", None), (9, "linear", "clamp", None), (10, "linear", "clamp", None),
                                  (12, "linear", "clamp", None), (13, "linear", "clamp", None)):
        b[(3, sreg)] = Sampler(filt, addr, cmp)
    b[(2, 0)] = g.cbuf()
    b[(2, 1)] = f.cbuf()
    b[(1, 0)] = RWTexture(W, H, D, dim="3d")
    return Scene(b), groups
