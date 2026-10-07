"""Gameplay detector: is the HUD health/ki bar strip visible (i.e. not a menu, pause, tip or loading screen)?
    py -I ingame.py <screenshot.png>   -> prints "INGAME <frac>" or "NOT_INGAME <frac>", exit 0 / 1
Calibrated 2026-10-06: gameplay 0.30-0.35 of the strip > 180 brightness; menus/tips/loading 0.000.
"""
import sys
import numpy as np
from PIL import Image

im = np.asarray(Image.open(sys.argv[1]).convert('L'), dtype=float)
h, w = im.shape
band = im[int(h * 0.877):int(h * 0.885), int(w * 0.43):int(w * 0.63)]
frac = float((band > 180).mean())
ok = frac > 0.15
print('%s %.3f' % ('INGAME' if ok else 'NOT_INGAME', frac))
sys.exit(0 if ok else 1)
