#!/usr/bin/env python3
"""shotpairs.py - level 0 vs level N screenshot pairs of a fastab -Shots session (notes\\extra-performance-settings.md).

  py -I tools\\testmatrix\\shotpairs.py <session dir> [<session dir> ...]

For every item with both pair-0 shots (manifest 'shot' fields, shots\\<item>_A.png / _B.png) it writes
pairs\\<item>.jpg (A | B side by side, half size, so the user sees what the level gives up) and a look-change number:
mean abs diff (0-255) and the share of pixels that changed by more than 12 levels. Noise floor: the same numbers for
consecutive A shots of different items in the same session (both at the game's picture, same camera, a few seconds
apart: TAA / particles / wind). Writes pairs\\pairs.json and prints one line per item.
"""
import json
import pathlib
import sys

import numpy as np
from PIL import Image

HALF = 2


def load(p):
    return np.asarray(Image.open(p).convert("RGB"), dtype=np.int16)


def diff(a, b):
    d = np.abs(a - b).max(axis=2)
    return float(np.abs(a - b).mean()), float((d > 12).mean() * 100.0)


def session(sess):
    sess = pathlib.Path(sess)
    rows = [json.loads(l) for l in sess.joinpath("manifest.jsonl").read_text(encoding="utf-8-sig").splitlines() if l.strip()]
    shots = {}
    for r in rows:
        if r.get("kind") == "window" and r.get("shot"):
            shots.setdefault(r["item"], {})[r["arm"]] = sess / r["shot"]
    out = sess / "pairs"
    out.mkdir(exist_ok=True)
    res = {}
    a_list = [(it, s["A"]) for it, s in shots.items() if "A" in s]
    noise = [diff(load(a_list[i][1]), load(a_list[i + 1][1])) for i in range(len(a_list) - 1)]
    for it, s in shots.items():
        if "A" not in s or "B" not in s:
            continue
        a, b = load(s["A"]), load(s["B"])
        mad, chg = diff(a, b)
        h, w = a.shape[0] // HALF, a.shape[1] // HALF
        ia = Image.open(s["A"]).convert("RGB").resize((w, h), Image.LANCZOS)
        ib = Image.open(s["B"]).convert("RGB").resize((w, h), Image.LANCZOS)
        sbs = Image.new("RGB", (2 * w + 8, h), (255, 255, 255))
        sbs.paste(ia, (0, 0))
        sbs.paste(ib, (w + 8, 0))
        name = "".join(c if c.isalnum() or c in "._" else "_" for c in it)
        jp = out / (name + ".jpg")
        sbs.save(jp, quality=88)
        res[it] = {"pair": str(jp), "A": str(s["A"]), "B": str(s["B"]), "mean_abs_diff": round(mad, 2), "changed_pct": round(chg, 2)}
        print("%-10s look change: mean abs diff %5.2f, pixels changed %5.1f %%   %s" % (it, mad, chg, jp))
    if noise:
        nm = sorted(n[0] for n in noise)[len(noise) // 2]
        nc = sorted(n[1] for n in noise)[len(noise) // 2]
        res["_noise_A_vs_A_median"] = {"mean_abs_diff": round(nm, 2), "changed_pct": round(nc, 2), "n": len(noise)}
        print("noise floor (A vs A, median of %d): mean abs diff %.2f, pixels changed %.1f %%" % (len(noise), nm, nc))
    (out / "pairs.json").write_text(json.dumps(res, indent=1), encoding="utf-8")


if __name__ == "__main__":
    if len(sys.argv) < 2:
        sys.exit(__doc__)
    for s in sys.argv[1:]:
        session(s)
