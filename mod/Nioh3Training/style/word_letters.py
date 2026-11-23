"""Rank letters drawn in the same lettering as the rank words (human #127238): Palatino italic, the words' cream fill, solid black
outline. Rewrites the 7 letter rectangles of style/rank_atlas.png in place (words and gauge untouched).
Ink box matches the previous sprites (top ~15% of the box, ~70% tall, >= 45 px side margins) so on-screen size stays the same.
The outline is fully opaque: the loader fades letter pixels under alpha 198 (de-halo, #127168).
Run: py -I mod/Nioh3Training/style/word_letters.py   (needs Pillow)"""
import json, os
from PIL import Image, ImageDraw, ImageFont

HERE = os.path.dirname(os.path.abspath(__file__))
FONT = os.path.join(os.environ.get("WINDIR", "C:/Windows"), "Fonts", "palai.ttf")   # same face as the words
FILL = (236, 232, 222, 255)                                                            # same cream as the words
OUTLINE = (0, 0, 0, 255)
RANKS = ("D", "C", "B", "A", "S", "SS", "SSS")


def letter(r, w, h):
    S = 600; f = ImageFont.truetype(FONT, S); stroke = int(S * 0.035)
    cw = f.getlength("S") * 0.80
    big = Image.new("RGBA", (int(S * (1.3 + 0.8 * len(r))), int(S * 1.5)), (0, 0, 0, 0)); d = ImageDraw.Draw(big)
    for i, ch in enumerate(r):
        d.text((big.width / 2 + (i - (len(r) - 1) / 2) * cw, big.height / 2), ch, font=f, fill=FILL, anchor="mm", stroke_width=stroke, stroke_fill=OUTLINE)
    big = big.crop(big.getbbox())
    th = int(h * 0.70); tw = int(big.width * th / big.height)
    if tw > w - 90: tw = w - 90; th = int(big.height * tw / big.width)
    out = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    out.paste(big.resize((tw, th), Image.LANCZOS), ((w - tw) // 2, int(h * 0.15) + (int(h * 0.70) - th) // 2))
    return out


def main():
    js = json.load(open(os.path.join(HERE, "rank_atlas.json"), encoding="utf-8"))
    path = os.path.join(HERE, "rank_atlas.png"); atlas = Image.open(path).convert("RGBA")
    for r in RANKS:
        x, y, w, h = js["sprites"]["letter_" + r]
        atlas.paste(Image.new("RGBA", (w, h), (0, 0, 0, 0)), (x, y)); atlas.paste(letter(r, w, h), (x, y))
        print(r, (x, y, w, h))
    atlas.save(path, optimize=True); print("wrote", path)


if __name__ == "__main__":
    main()
