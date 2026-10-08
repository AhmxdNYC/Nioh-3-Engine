#!/usr/bin/env python3
"""shotdiff.py - screenshot compare for shader A/B items (Ally or PC).

  py -I tools\\testmatrix\\shotdiff.py A.png B.png [--noise A2.png] [--out diff.png] [--crop x,y,w,h]

A = stock (replacement off), B = replacement on, A2 = a second stock shot of the same view (other launch).
Separate launches are never bit-identical (TAA/FSR jitter, particles, wind), so a verdict needs A2:
B passes when A-vs-B is not worse than A-vs-A2 (mean abs diff, 99.9th pct and changed-pixel share, 25 % margin).
Without --noise only the raw stats are printed (0 changed pixels = identical).
--out writes a x8-amplified abs-diff image. --crop limits the compare to the region the pass touches.
"""
import argparse
import sys

import numpy as np
from PIL import Image


def load(p, crop):
    a = np.asarray(Image.open(p).convert('RGB'), dtype=np.int16)
    if crop:
        x, y, w, h = crop
        a = a[y:y + h, x:x + w]
    return a


def stats(a, b):
    if a.shape != b.shape:
        sys.exit('size mismatch %s vs %s' % (a.shape, b.shape))
    d = np.abs(a - b)
    px = d.max(axis=2)
    return d, dict(mean=float(d.mean()), p999=float(np.percentile(px, 99.9)), max=int(px.max()),
                   changed=float((px > 2).mean() * 100.0))


def fmt(s):
    return 'mean %.3f  p99.9 %.0f  max %d  changed(>2/255) %.3f %%' % (s['mean'], s['p999'], s['max'], s['changed'])


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('a'); ap.add_argument('b')
    ap.add_argument('--noise'); ap.add_argument('--out')
    ap.add_argument('--crop', type=lambda v: tuple(int(x) for x in v.split(',')))
    o = ap.parse_args()
    a, b = load(o.a, o.crop), load(o.b, o.crop)
    d, s = stats(a, b)
    print('A vs B : ' + fmt(s))
    if o.out:
        Image.fromarray(np.clip(d * 8, 0, 255).astype(np.uint8)).save(o.out)
    if not o.noise:
        print('VERDICT: IDENTICAL' if s['max'] == 0 else 'VERDICT: (no --noise shot; judge by eye or add A2)')
        return
    _, n = stats(a, load(o.noise, o.crop))
    print('A vs A2: ' + fmt(n))
    worse = [k for k in ('mean', 'p999', 'changed') if s[k] > n[k] * 1.25 + 0.01]
    print('VERDICT: ' + ('WITHIN NOISE' if not worse else 'DIFFERENT (' + ', '.join(worse) + ' above the A-vs-A2 floor) -> DROP'))


if __name__ == '__main__':
    main()
