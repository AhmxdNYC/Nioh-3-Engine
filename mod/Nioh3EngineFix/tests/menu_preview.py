"""Render a menu preview PNG from a menu_harness draw dump (N3_MENU_DUMP=file). Rects and texts come from the real
EngineFix draw code; the font is a stand-in monospace (the game uses the mod's stroke font), so only layout/colours are exact.
Usage: py -I mod/Nioh3EngineFix/tests/menu_preview.py dump.txt out.png [fade]   (fade = the harness open-fade factor to undo, default 0.35)"""
import sys
from PIL import Image, ImageDraw, ImageFont

src, out = sys.argv[1], sys.argv[2]
fade = float(sys.argv[3]) if len(sys.argv) > 3 else 0.35
rows = [l.rstrip("\n") for l in open(src, encoding="utf-8")]
_, sc, gw, gh = rows[0].split()
sc, gw, gh = float(sc), int(gw), int(gh)
rects, texts = [], []
for l in rows[1:]:
    if l.startswith("R "):
        _, x, y, w, h, c = l.split(" ", 5)
        rects.append((float(x), float(y), float(w), float(h), int(c, 16)))
    elif l.startswith("T "):
        _, x, y, s, c, t = (l.split(" ", 5) + [""])[:6]
        texts.append((float(x), float(y), float(s), int(c, 16), t))
x0 = min(r[0] for r in rects) - 20
y0 = min(r[1] for r in rects) - 20
x1 = max(r[0] + r[2] for r in rects) + 20
y1 = max(r[1] + r[3] for r in rects) + 20
img = Image.new("RGBA", (int(x1 - x0), int(y1 - y0)), (46, 52, 44, 255))  # muted arena-like backdrop


def rgba(c):  # 0xAABBGGRR
    a = min(255, int(((c >> 24) & 255) / fade))
    return (c & 255, (c >> 8) & 255, (c >> 16) & 255, a)


for x, y, w, h, c in rects:
    layer = Image.new("RGBA", img.size, (0, 0, 0, 0))
    ImageDraw.Draw(layer).rectangle([x - x0, y - y0, x - x0 + max(w, 1) - 1, y - y0 + max(h, 1) - 1], fill=rgba(c))
    img = Image.alpha_composite(img, layer)
d = ImageDraw.Draw(img)
try:
    font = ImageFont.truetype("consola.ttf", int(gh * sc * 0.95))
except OSError:
    font = ImageFont.load_default()
for x, y, s, c, t in texts:
    d.text((x - x0, y - y0), t, fill=rgba(c), font=font)
img.convert("RGB").save(out)
print("wrote", out, img.size)
