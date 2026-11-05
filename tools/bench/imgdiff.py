#!/usr/bin/env python3
"""imgdiff.py - PSNR / SSIM of same-spot screenshots against a reference arm (#67772 Extreme HQ benches).

  py -I tools\\bench\\imgdiff.py REF.png LABEL=arm.png [LABEL=arm.png ...] [--noise REF2.png] [--crop x,y,w,h]
                                 [--ui-mask top,bottom] [--json out.json]

REF = the native arm (Preset 2), one shot per arm taken at the identical pose (same save, same shrine / spawn, camera not
touched, HUD hidden if possible). REF2 = a second REF-arm shot of the same pose from another launch: separate launches are
never bit-identical (TAA / FSR jitter, wind, particles), so REF-vs-REF2 is the noise floor a 1:1 arm cannot beat.
Arms of a different size (none expected: XRES changes the internal size, not the screenshot) are resized to REF with
Lanczos and flagged.

Prints per arm: PSNR (dB, RGB, peak 255), SSIM (luma BT.601, 11x11 Gaussian sigma 1.5, K1 0.01, K2 0.03 - the Wang 2004
reference settings), mean abs diff, share of pixels changed > 12 levels, and the same against the noise floor.
--ui-mask top,bottom drops that many pixel rows from the top / bottom (HUD bars). Needs Pillow + numpy only.
"""
import argparse
import json
import math
import sys

import numpy as np
from PIL import Image


def load(path, crop, ui, size=None):
    im = Image.open(path).convert('RGB')
    resized = False
    if size and im.size != size:
        im = im.resize(size, Image.LANCZOS)
        resized = True
    a = np.asarray(im, dtype=np.float64)
    if ui:
        t, b = ui
        a = a[t:a.shape[0] - b if b else a.shape[0]]
    if crop:
        x, y, w, h = crop
        a = a[y:y + h, x:x + w]
    return a, resized


def gauss_kernel(n=11, sigma=1.5):
    x = np.arange(n) - (n - 1) / 2.0
    k = np.exp(-(x * x) / (2 * sigma * sigma))
    return k / k.sum()


def filt(img, k):
    # separable 'valid' convolution (same as skimage / the Wang reference with gaussian_weights, no padding)
    n = len(k)
    h, w = img.shape
    tmp = np.zeros((h, w - n + 1))
    for i in range(n):
        tmp += k[i] * img[:, i:i + w - n + 1]
    out = np.zeros((h - n + 1, w - n + 1))
    for i in range(n):
        out += k[i] * tmp[i:i + h - n + 1, :]
    return out


def luma(a):
    return 0.299 * a[..., 0] + 0.587 * a[..., 1] + 0.114 * a[..., 2]


def ssim(a, b):
    x, y = luma(a), luma(b)
    k = gauss_kernel()
    c1, c2 = (0.01 * 255) ** 2, (0.03 * 255) ** 2
    mx, my = filt(x, k), filt(y, k)
    sxx = filt(x * x, k) - mx * mx
    syy = filt(y * y, k) - my * my
    sxy = filt(x * y, k) - mx * my
    m = ((2 * mx * my + c1) * (2 * sxy + c2)) / ((mx * mx + my * my + c1) * (sxx + syy + c2))
    return float(m.mean())


def psnr(a, b):
    mse = float(np.mean((a - b) ** 2))
    return float('inf') if mse == 0 else 10 * math.log10(255.0 ** 2 / mse)


def stats(a, b):
    if a.shape != b.shape:
        sys.exit('size mismatch %s vs %s (use the same output resolution for every arm)' % (a.shape, b.shape))
    d = np.abs(a - b)
    return dict(psnr=round(psnr(a, b), 2), ssim=round(ssim(a, b), 4), mad=round(float(d.mean()), 2),
                changed=round(float((d.max(axis=2) > 12).mean()) * 100, 2))


def main(argv):
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument('ref')
    ap.add_argument('arms', nargs='+', help='LABEL=path.png')
    ap.add_argument('--noise')
    ap.add_argument('--crop')
    ap.add_argument('--ui-mask')
    ap.add_argument('--json')
    a = ap.parse_args(argv)
    crop = tuple(int(v) for v in a.crop.split(',')) if a.crop else None
    ui = tuple(int(v) for v in a.ui_mask.split(',')) if a.ui_mask else None
    size = Image.open(a.ref).size
    ref, _ = load(a.ref, crop, ui)
    out = dict(ref=a.ref, size=list(size), arms={})
    floor = None
    if a.noise:
        n, rs = load(a.noise, crop, ui, size)
        floor = stats(ref, n)
        out['noise_floor'] = floor
        print('noise floor (REF vs REF2): PSNR %.2f dB  SSIM %.4f  mad %.2f  changed %.2f %%' %
              (floor['psnr'], floor['ssim'], floor['mad'], floor['changed']))
    for spec in a.arms:
        if '=' not in spec:
            sys.exit('arm must be LABEL=path: %s' % spec)
        lab, path = spec.split('=', 1)
        b, rs = load(path, crop, ui, size)
        s = stats(ref, b)
        s['resized'] = rs
        if floor:
            s['ssim_vs_floor'] = round(s['ssim'] - floor['ssim'], 4)
            s['psnr_vs_floor'] = round(s['psnr'] - floor['psnr'], 2)
            s['near_1to1'] = s['ssim'] >= floor['ssim'] - 0.01 and s['psnr'] >= floor['psnr'] - 1.5
        out['arms'][lab] = s
        print('%-6s PSNR %6.2f dB  SSIM %.4f  mad %5.2f  changed %5.2f %%%s%s' % (
            lab, s['psnr'], s['ssim'], s['mad'], s['changed'],
            ('  (vs floor: SSIM %+.4f, PSNR %+.2f dB -> %s)' % (s['ssim_vs_floor'], s['psnr_vs_floor'],
                                                               'near 1:1' if s['near_1to1'] else 'visible change')) if floor else '',
            '  [resized]' if rs else ''))
    if a.json:
        json.dump(out, open(a.json, 'w', encoding='utf-8'), indent=1)
    return 0


def selftest():
    rng = np.random.default_rng(1)
    base = rng.integers(0, 256, (64, 80, 3)).astype(np.float64)
    assert psnr(base, base) == float('inf') and abs(ssim(base, base) - 1.0) < 1e-9
    noisy = np.clip(base + rng.normal(0, 8, base.shape), 0, 255)
    p = psnr(base, noisy)
    assert 29 < p < 31, p  # sigma 8 -> ~30 dB
    s = ssim(base, noisy)
    assert 0.5 < s < 1.0, s
    blur = (base + np.roll(base, 1, 0) + np.roll(base, 1, 1) + np.roll(base, 1, (0, 1))) / 4
    assert ssim(base, blur) < ssim(base, noisy * 0 + base), 'blur must lower SSIM'
    print('imgdiff selftest OK (psnr %.2f ssim %.4f)' % (p, s))
    return 0


if __name__ == '__main__':
    sys.exit(selftest() if sys.argv[1:2] == ['selftest'] else main(sys.argv[1:]))
