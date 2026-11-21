"""Demon iron rank letters (human #127233, picked from sheet #127216 style 8): black forged iron, cracks glowing in the rank colour,
lit bevel, thin rank-colour rim, hard alpha edge (no glow outside the letter, human #127149/#127168).
Rewrites the 7 letter rectangles of style/rank_atlas.png in place; the rank colour per letter is read from the sprite it replaces.
Run: py -I mod/Nioh3Training/style/demon_iron.py   (needs numpy + Pillow)"""
import json, os
import numpy as np
from PIL import Image, ImageDraw, ImageFilter, ImageFont

HERE = os.path.dirname(os.path.abspath(__file__))
FONT = os.path.join(os.environ.get("WINDIR", "C:/Windows"), "Fonts", "cambriab.ttf")
RANKS = ("D", "C", "B", "A", "S", "SS", "SSS")
SEED = {r: 31 + i * 7 for i, r in enumerate(RANKS)}


def U(a): return np.asarray(a, np.float32) / 255
def I(a): return Image.fromarray((np.clip(a, 0, 1) * 255).astype(np.uint8))
def blur(a, r): return U(I(a).filter(ImageFilter.GaussianBlur(r)))
def dil(a, r): return U(I(a).filter(ImageFilter.MaxFilter(int(r) | 1)))
def ero(a, r): return U(I(a).filter(ImageFilter.MinFilter(int(r) | 1)))


def noise(h, w, scale, seed, octaves=1):
    out = np.zeros((h, w), np.float32); amp = 1.0; tot = 0.0
    for o in range(octaves):
        s = max(1, int(scale) >> o); r = np.random.default_rng(seed + o)
        sm = r.random((h // s + 2, w // s + 2)).astype(np.float32)
        out += amp * U(Image.fromarray((sm * 255).astype(np.uint8)).resize((w, h), Image.BICUBIC)); tot += amp; amp *= 0.5
    return out / tot


def glyph(r, w, h):
    """Letter mask whose ink box matches the old sprites (top ~15% of the box, ~70% tall, >= 45 px side margins)."""
    n = len(r); S = 600; f = ImageFont.truetype(FONT, S); cw = f.getbbox("S")[2] * 0.62
    big = Image.new("L", (int(S * (1.2 + 0.62 * n)), int(S * 1.4)), 0); d = ImageDraw.Draw(big)
    for i, ch in enumerate(r): d.text((big.width / 2 + (i - (n - 1) / 2) * cw, big.height / 2), ch, font=f, fill=255, anchor="mm")
    sh = 0.14; big = big.transform(big.size, Image.AFFINE, (1, sh, -sh * big.height / 2, 0, 1, 0), Image.BICUBIC)
    big = big.crop(big.getbbox())
    th = int(h * 0.70); tw = int(big.width * th / big.height)
    if tw > w - 90: tw = w - 90; th = int(big.height * tw / big.width)
    m = Image.new("L", (w, h), 0); m.paste(big.resize((tw, th), Image.LANCZOS), ((w - tw) // 2, int(h * 0.15) + (int(h * 0.70) - th) // 2))
    return U(m)


def shade(g, bevel, k, light=(-0.6, -0.7, 0.4)):
    hgt = blur(ero(g, 3), bevel) * g; gy, gx = np.gradient(hgt * bevel * 2.2)
    nrm = np.dstack([-gx, -gy, np.ones_like(hgt)]); nrm /= np.linalg.norm(nrm, axis=2, keepdims=True)
    l = np.array(light, np.float32); l /= np.linalg.norm(l); hv = l + np.array([0, 0, 1.0]); hv /= np.linalg.norm(hv)
    return np.clip((nrm * l).sum(2), 0, 1), np.clip((nrm * hv).sum(2), 0, 1) ** k


def demon(r, w, h, rank_rgb):
    k = h / 300.0; g = glyph(r, w, h); c = np.asarray(rank_rgb, np.float32) / 255
    d, s = shade(g, 5 * k, 20)
    nz = noise(h, w, 26 * k, SEED[r], 3)
    cr = (np.abs(nz - 0.5) < 0.018).astype(np.float32) * ero(g, 5 * k)      # cracks stay inside the letter
    cr = np.clip(blur(cr, 1.0 * k) * 1.6, 0, 1)
    iron = np.array([0.09, 0.08, 0.09]) * (0.6 + 0.9 * d)[..., None] + s[..., None] * 0.25
    ember = np.clip(c * 1.3 + 0.15, 0, 1)
    face = iron * (1 - cr[..., None]) + ember * cr[..., None]
    face = np.clip(face + (blur(cr, 4 * k) * g)[..., None] * c * 0.8, 0, 1)   # heat bleeds into the iron, not past it
    rim = dil(g, 6 * k); rd, rs = shade(rim, 3 * k, 40)
    rimc = np.clip(c * (0.35 + 0.8 * rd)[..., None] + rs[..., None] * 0.9, 0, 1)
    outer = dil(g, 10 * k)
    rgb = np.zeros((h, w, 3), np.float32) + np.array([0.03, 0.03, 0.04])
    rgb = rgb * (1 - rim[..., None]) + rimc * rim[..., None]
    rgb = rgb * (1 - g[..., None]) + face * g[..., None]
    return rgb, outer


def main():
    js = json.load(open(os.path.join(HERE, "rank_atlas.json"), encoding="utf-8"))
    path = os.path.join(HERE, "rank_atlas.png"); atlas = Image.open(path).convert("RGBA"); a = np.asarray(atlas).copy()
    for r in RANKS:
        x, y, w, h = js["sprites"]["letter_" + r]; spr = a[y:y + h, x:x + w].astype(np.float32)
        solid = spr[..., 3] > 230
        col = spr[..., :3][solid].mean(0) if solid.any() else np.array([200, 60, 50])
        col = col * (230.0 / max(col.max(), 1))           # same hue, full strength
        rgb, alpha = demon(r, w, h, col)
        a[y:y + h, x:x + w, :3] = (np.clip(rgb, 0, 1) * 255).astype(np.uint8)
        a[y:y + h, x:x + w, 3] = (np.clip(alpha, 0, 1) * 255).astype(np.uint8)
        print(r, (x, y, w, h), "rank colour", col.astype(int).tolist())
    Image.fromarray(a, "RGBA").save(path, optimize=True); print("wrote", path)


if __name__ == "__main__":
    main()
