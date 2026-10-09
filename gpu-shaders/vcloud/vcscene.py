r"""vcscene.py - synthetic inputs for VCLOUD (shader_dx12\0x5f17ca9d.file record 7, CS b76874bb; 8x8 groups,
RWTexture2D<float4> u0 + RWTexture2D<float2> u1), for gi\verify.py --kind vcloud | vcloud-real.

Geometry is consistent: planet radius R with the centre below the eye (the shader's convention: centre = (eye.x,
-R, eye.z)), two cloud shells (layer 0 / 1) as radius pairs in g_vEarthRadius[l].yz, camera corner rays in
g_vCameraDirVectors, a perspective mW2P with its inverse in g_mtP2W (mask-mip projection) and g_mtPrevW2P.
Coverage scenes randomise: eye on the ground / inside layer 0 / above both layers / above the atmosphere, sun and moon
on/off and above/below the -0.2 cut, layer on/off (g_vCloudParams1.x/.w), step counts, light-step length (so light
steps leave the layer at step 1..4 or never), density/mask thresholds (empty / sparse / thick = the 1/255
transmittance exit), the lighting threshold g_vOptionValue.x, clamp and cirrus switches. Textures: small random noise.
"""
import numpy as np
import os as _os, sys as _sys
_GI = _os.path.join(_os.path.dirname(_os.path.dirname(_os.path.abspath(__file__))), "gi")
_sys.path.insert(0, _GI)
from dxil_interp import Texture, RWTexture, Sampler, Scene  # noqa: E402
from ll2hlsl import Iface  # noqa: E402
from scenes import CB  # noqa: E402


def _mips3d(rng, n, c, lo=0.0, hi=1.0, levels=3):
    m = [rng.uniform(lo, hi, size=(n, n, n, c)).astype(np.float32)]
    while len(m) < levels and m[-1].shape[0] > 1:
        m.append(m[-1][::2, ::2, ::2].copy())
    return m


def _mips2d(rng, n, c, layers=1, lo=0.0, hi=1.0, levels=4):
    m = [rng.uniform(lo, hi, size=(layers, n, n, c)).astype(np.float32)]
    while len(m) < levels and m[-1].shape[1] > 1:
        m.append(m[-1][:, ::2, ::2].copy())
    return m


def vcloud_scene(text, seed, realistic=False):
    rng = np.random.default_rng(seed + (104729 if realistic else 0))
    pick = lambda p: rng.random() < p          # noqa: E731
    ifc = Iface(text)
    g = CB(ifc, "$Globals")
    W, H = (16, 8) if not realistic else (16, 16)
    sw, sh = (W - int(rng.integers(0, 3)), H - int(rng.integers(0, 2))) if not realistic else (W, H)
    g.set("g_uSurfaceWidth", sw)
    g.set("g_uSurfaceHeight", sh)
    g.set("g_iBayerPosNum", int(rng.integers(0, 16)))
    g.set("g_vProcSize", [W, H, 1.0 / W, 1.0 / H])
    # ---------------------------------------------------------------- planet, shells, eye
    R = 60.0
    lay = []
    for l in range(2):
        r0 = R + float(rng.uniform(0.8, 2.0)) + 2.5 * l
        th = float(rng.uniform(0.8, 2.0))
        lay.append((r0, r0 + th, th))
    hpick = rng.random()
    if realistic or hpick < 0.6:
        eyeh = float(rng.uniform(0.01, 0.5))                                   # on the ground (the game's case)
    elif hpick < 0.75:
        eyeh = lay[0][0] - R + float(rng.uniform(0.1, 0.9)) * lay[0][2]       # inside layer 0
    elif hpick < 0.9:
        eyeh = lay[1][1] - R + float(rng.uniform(0.2, 1.5))                    # above both layers
    else:
        eyeh = float(rng.uniform(15, 25))                                      # outside the atmosphere
    eye = np.array([rng.uniform(-5, 5), eyeh, rng.uniform(-5, 5)])
    g.set("vEye", [*eye, 1.0])
    atm = R + float(rng.choice([10.0, 12.0]))
    g.set("g_vSky2RayleighExtinction", [0.0058, 0.0135, 0.0331, atm])
    g.set("g_vSky2MieExtinction", [0.004, 0.004, 0.004, 1.2])
    for l in range(2):
        r0, r1, th = lay[l]
        g.set("g_vEarthRadius[%d]" % l, [R, r0, r1, atm])
        on = realistic or pick(0.85)
        g.set("g_vCloudParams0[%d]" % l, [float(rng.uniform(0.0, 1.0)), float(rng.uniform(0.2, 1.0)), 1.0 / th, th])
        g.set("g_vCloudParams1[%d]" % l, [float(rng.uniform(0.0, 0.9)) if on else 1.5, float(rng.uniform(0, 1)),
                                         float(rng.uniform(0, 1)),
                                         (float(rng.choice([0.0, 2.0, 10.0, 30.0])) if not realistic else 8.0)])
        g.set("g_vRaySampleNum[%d]" % l, [float(rng.uniform(2, 12)),
                                         (float(rng.choice([4, 8, 12, 24])) if not realistic else 24.0),
                                         float(rng.uniform(0.0, 1.0)), 1.31033])
        g.set("g_vBaseTexOffset[%d]" % l, rng.uniform(-1, 1, 3))
        g.set("g_vBaseTexScale[%d]" % l, rng.uniform(0.05, 0.4, 3))
        g.set("g_vDetailTexOffset[%d]" % l, rng.uniform(-1, 1, 3))
        g.set("g_vDetailTexScale[%d]" % l, rng.uniform(0.3, 2.0, 3))
        g.set("g_vDistortTexOffset[%d]" % l, rng.uniform(-1, 1, 2))
        g.set("g_vDistortTexScale[%d]" % l, rng.uniform(0.05, 0.5, 2))
        g.set("g_vDistortScale[%d]" % l, [*rng.uniform(0.0, 0.2, 3), float(rng.uniform(0.5, 2.0))])
        g.set("g_vErodeScale[%d]" % l, rng.uniform(0.0, 1.0, 2))
        g.set("g_fBaseSamplingScaleFactor[%d]" % l, float(rng.uniform(0.0, 0.5)))
        g.set("g_vW2M[%d]" % l, [float(rng.uniform(0.02, 0.08)), float(rng.uniform(0.02, 0.08)), 0.5, 0.5])
        g.set("g_vMaskTexSliceID[%d]" % l, int(rng.integers(0, 2)))
    g.set("g_vMaskTexSliceID[2]", int(rng.integers(0, 2)))
    lo_thr = 0.6 if pick(0.2) else 0.0           # sometimes nearly empty sky
    for k in range(8):
        g.set("g_vMaskThreshold[%d]" % k, [float(rng.uniform(lo_thr, 0.6 + lo_thr * 0.5)), float(rng.uniform(0.3, 1.5)),
                                          float(rng.uniform(-0.2, 0.5)), float(rng.uniform(0.5, 2.0))])
    g.set("g_vCloudParams2", [float(rng.choice([0.05, 0.2, 0.5, 1.5])), float(rng.uniform(0.5, 2)),
                              float(rng.uniform(0, 1)), float(rng.uniform(0, 1))])
    # ---------------------------------------------------------------- sun / moon / phase / misc

    def sdir(upv):
        d = rng.normal(size=3)
        d[1] = upv
        return d / np.linalg.norm(d)
    sun = sdir(float(rng.choice([0.6, 0.1, -0.1, -0.5])) if not realistic else 0.5)
    moon = sdir(float(rng.choice([0.6, -0.1, -0.5])) if not realistic else -0.5)
    g.set("g_vSky2SunDir", [*sun, 0.0])
    g.set("g_vSky2MoonDir", [*moon, 0.0])
    g.set("litDir[0]", [*sun, 0.0])
    g.set("g_vSky2SunRGB", [*rng.uniform(0.5, 3, 3), (float(rng.choice([1.0, 1.0, 0.0])) if not realistic else 1.0)])
    g.set("g_vSky2MoonRGB", [*rng.uniform(0.01, 0.2, 3), (float(rng.choice([0.0, 0.5])) if not realistic else 0.0)])
    g.set("g_vHgParams0", [float(rng.uniform(0.05, 0.2)), 1.64, -1.6, float(rng.uniform(0, 1))])
    g.set("g_vHgParams1", [float(rng.uniform(0.05, 0.2)), 1.04, -0.4, float(rng.uniform(0, 1))])
    g.set("g_vStableDim", rng.uniform(0, 1, 4))
    g.set("g_vClampValue", [float(rng.uniform(0, 4)), float(rng.uniform(0.2, 1.0)), float(rng.uniform(0.5, 1.5)),
                            float(rng.choice([0.0, 0.0, rng.uniform(0.5, 4.0)]))])
    g.set("g_vOptionValue", [float(rng.choice([0.0, 0.05, 0.3])), float(rng.uniform(0, 1)), float(rng.uniform(0, 1)),
                             float(rng.uniform(0, 1))])
    for nm in ("g_vBaseMipParams", "g_vDetailMipParams"):
        g.set(nm, [float(rng.uniform(0, 1)), float(rng.uniform(0, 0.5)), float(rng.uniform(0, 2)), float(rng.uniform(0, 1))])
    g.set("g_vDistortMipParams", [float(rng.uniform(0, 1)), float(rng.uniform(0, 0.5)), float(rng.uniform(0, 2)),
                                  float(rng.uniform(0.5, 2))])
    g.set("g_vCirrusTexOffset", rng.uniform(-1, 1, 2))
    g.set("g_vCirrusTexScale", rng.uniform(0.01, 0.2, 2))
    g.set("g_vCirrusDistortTexOffset", rng.uniform(-1, 1, 2))
    g.set("g_vCirrusDistortTexScale", rng.uniform(0.01, 0.2, 2))
    g.set("g_vCirrusParams", [float(rng.choice([0.0, rng.uniform(0.2, 1)])), float(rng.uniform(0, 1)),
                              float(rng.uniform(0, 1)), float(rng.uniform(0, 1))])
    g.set("g_vCirrusDistortScale", rng.uniform(0, 0.2, 2))
    # fog.w: aerial-perspective ground radius (the (|eye| - fog.w - s) / s > 10 test skips that block when far)
    g.set("fog", [*rng.uniform(0, 1, 3), (R + float(rng.uniform(-0.2, 0.3))) if pick(0.7) else float(rng.uniform(0, 1))])
    for k in range(4):        # .w of Scat[0]/[1]: aerial-perspective height scale (ratio > 10 skips the block)
        g.set("Scat[%d]" % k, [*rng.uniform(0, 1, 3), float(rng.choice([0.01, 0.1, 0.5, 2.0]))])
    # ---------------------------------------------------------------- camera
    fwd = rng.normal(size=3)
    fwd[1] = float(rng.choice([0.3, 0.6, 1.2, -0.2])) if not realistic else 0.5
    if eyeh > 12:
        fwd[1] = -2.5                          # above the atmosphere: look down into it
    fwd /= np.linalg.norm(fwd)
    right = np.cross([0.0, 1.0, 0.0], fwd)
    right /= np.linalg.norm(right)
    up = np.cross(fwd, right)
    th, aspect = 0.6, W / H
    corners = [fwd - aspect * th * right + th * up, fwd - aspect * th * right - th * up,
               fwd + aspect * th * right + th * up, fwd + aspect * th * right - th * up]
    for k, c in enumerate(corners):                 # the shader marches along -normalize(lerp(corners))
        g.set("g_vCameraDirVectors[%d]" % k, [*(-c), 0.0])
    V = np.eye(4)
    V[:3, 0], V[:3, 1], V[:3, 2] = right, up, fwd
    V[3, :3] = [-eye @ right, -eye @ up, -eye @ fwd]
    n_, f_ = 0.1, 200.0
    P = np.zeros((4, 4))
    P[0, 0], P[1, 1] = 1 / (aspect * th), 1 / th
    P[2, 2], P[2, 3], P[3, 2] = n_ / (n_ - f_), 1.0, -f_ * n_ / (n_ - f_)
    M = V @ P                                        # clip = [x y z 1] @ M
    g.set("mW2P", M.T.ravel())                       # rows mW2P[i] = column i of M (dot(p, row))
    g.set("g_mtP2W", np.linalg.inv(M).ravel())
    Mp = M.copy()
    Mp[3, :3] += rng.uniform(-0.02, 0.02, 3)
    g.set("g_mtPrevW2P", Mp.T.ravel())
    g.set("g_vProjFarNear", [f_, n_])
    # ---------------------------------------------------------------- resources (t8.. / s8.. / u0 u1 / b0)
    b = {(2, 0): g.cbuf()}
    b[(0, 8)] = Texture(_mips3d(rng, 8, 4), "3d")                        # sBase
    b[(0, 9)] = Texture(_mips3d(rng, 8, 4), "3d")                        # sDetail
    b[(0, 10)] = Texture(_mips2d(rng, 8, 4), "2d")                       # sDistort
    b[(0, 11)] = Texture(_mips2d(rng, 8, 4), "2d")                       # sCirrus
    b[(0, 12)] = Texture(_mips2d(rng, 8, 4, hi=2.0), "2d")               # sSkyAmbientAltitude2dTable
    b[(0, 13)] = Texture(_mips2d(rng, 8, 4), "2d")                       # sTransmit2dTable
    b[(0, 14)] = Texture(_mips3d(rng, 4, 4, hi=0.5), "3d")               # sSctr3dTable
    b[(0, 15)] = Texture(_mips2d(rng, 16, 4, layers=2, levels=5), "2darray")   # sMaskMap
    for s in range(8, 16):
        b[(3, s)] = Sampler("linear", "wrap" if s in (8, 9, 10, 11, 15) else "clamp")
    b[(1, 0)] = RWTexture(W, H, 1, 4, "2d")
    b[(1, 1)] = RWTexture(W, H, 1, 2, "2d")
    groups = [(gx, gy, 0) for gy in range((H + 7) // 8) for gx in range((W + 7) // 8)]
    return Scene(b), groups
