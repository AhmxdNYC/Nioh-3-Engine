"""Build the controller-display sprite atlases (padviz.h, user #23125/#23132/#23135) from the Gamepad Viewer skins.

Inputs (checked in under assets/padviz/, downloaded once - see assets/padviz/CREDITS.txt):
  dualsense/  PNG sprite sheets of the Gamepad Viewer DualSense skin (CrazyGrape, touchpad + PS button by Cubio300;
              CC BY 3.0 US) and its CSS (DualSense_Skin.css).
  xbox/       SVG layers of the "xbox_star" skin of skyjty/MyGamepadViewer (MIT, skyqyzm) and its CSS (main.css).
Outputs:
  assets/padviz/atlas_dualsense.png, atlas_xbox.png   (straight-alpha RGBA, for review)
  src/padviz_atlas.h       sprite rects + canvas placement (from the skins' CSS), included by padviz.h
  src/padviz_atlas_data.h  the two atlas PNG files as byte arrays (decoded at run time by padviz_png.h)

Placement numbers are the skins' CSS on top of gamepadviewer.com's own .xbox rules (style.css: bumper.right /
trigger.right mirrored, floats right inside their containers). Needs Pillow; the Xbox SVGs need resvg-py
(pip install --user resvg-py). Run: py -I tools\\gen_padviz_atlas.py   (the generated headers are checked in).
"""
import io
import math
import pathlib
import site
import sys

sys.path.append(site.getusersitepackages())
from PIL import Image  # noqa: E402

ROOT = pathlib.Path(__file__).resolve().parent.parent
ASSETS = ROOT / "assets" / "padviz"
PAD = 8  # transparent texels around every sprite (mip levels 1-3 do not bleed)
ATLAS_W = 1024

# sprite ids shared by both skins (padviz.h draws by id; a skin may leave one empty)
IDS = ["BODY", "STICK", "STICK_P", "A", "B", "X", "Y", "A_LIT", "B_LIT", "X_LIT", "Y_LIT", "UP", "DOWN", "LEFT",
       "RIGHT", "BACK", "START", "LB", "RB", "LT", "RT", "TOUCH", "META", "STICK_R", "STICK_R_P", "A_GLYPH", "B_GLYPH", "X_GLYPH",
       "Y_GLYPH"]


def crop(img, x, y, w, h):
    """CSS background-position crop with repeat (negative / wrapped offsets)."""
    out = Image.new("RGBA", (w, h))
    W, H = img.size
    for yy in range(h):
        sy = (y + yy) % H
        row = None
        for xx in range(w):
            sx = (x + xx) % W
            out.putpixel((xx, yy), img.getpixel((sx, sy)))
    return out


def dualsense():
    d = ASSETS / "dualsense"
    L = lambda n: Image.open(d / f"{n}.png").convert("RGBA")
    stick, btn, dpad, bs, trig, bump = L("stick"), L("buttons"), L("dpad"), L("backstart"), L("trigger"), L("bumper")
    S = {}
    # .controller.xbox 807x651 background
    S["BODY"] = (L("body"), 0, 0)
    # .sticks 367x100 @ (220,333); .stick 100x100 bg 0 0, .pressed -102px; .stick.right left 267
    S["STICK"] = (crop(stick, 0, 0, 100, 100), 220, 333)
    S["STICK_P"] = (crop(stick, 102, 0, 100, 100), 220, 333)
    S["STICK_R"] = (crop(stick, 0, 0, 100, 100), 487, 333)
    S["STICK_R_P"] = (crop(stick, 102, 0, 100, 100), 487, 333)
    # .abxy 181x181 @ (573,178); .button 58x58; a -0 (61,123), b -59 (123,62), x -118 (0,61), y -177 (61,0); pressed y -59
    for k, sx, px, py in (("A", 0, 61, 123), ("B", 59, 123, 62), ("X", 118, 0, 61), ("Y", 177, 61, 0)):
        S[k] = (crop(btn, sx, 0, 58, 58), 573 + px, 178 + py)
        S[k + "_LIT"] = (crop(btn, sx, 59, 58, 58), 573 + px, 178 + py)
        S[k + "_GLYPH"] = (S[k][0], 573 + px, 178 + py)  # replaced by face_glyph() in main()
    # .dpad 144x144 @ (71,196); .face opacity 0 (pressed 1); up 0 -68 52x63 (46,0); down -54 +63 52x63 (46,81);
    # left -108 -68 64x52 (-1,47); right -175 -68 63x52 (81,46)
    S["UP"] = (crop(dpad, 0, 68, 52, 63), 71 + 46, 196 + 0)
    S["DOWN"] = (crop(dpad, 54, -63, 52, 63), 71 + 46, 196 + 81)
    S["LEFT"] = (crop(dpad, 108, 68, 64, 52), 71 - 1, 196 + 47)
    S["RIGHT"] = (crop(dpad, 175, 68, 63, 52), 71 + 81, 196 + 46)
    # .arrows 416x57 @ (195,140); back/start 27x57 opacity 0 (pressed 1); back bg 0 0 (float left); start bg 27px 0 (float right)
    S["BACK"] = (crop(bs, 0, 0, 27, 57), 195, 140)
    S["START"] = (crop(bs, -27, 0, 27, 57), 195 + 416 - 27, 140)
    # .bumpers 620x35 @ (93,114); .bumper 110x35 opacity 0 (pressed 1); right floats right + rotateY(180deg) (site CSS)
    S["LB"] = (crop(bump, 0, 0, 110, 35), 93, 114)
    S["RB"] = (crop(bump, 0, 0, 110, 35).transpose(Image.FLIP_LEFT_RIGHT), 93 + 620 - 110, 114)
    # .triggers 619x108 @ left 94 top 0; .trigger 111x108 opacity = value; right bg -113 0, rotateY(0) (not mirrored)
    S["LT"] = (crop(trig, 0, 0, 111, 108), 94, 0)
    S["RT"] = (crop(trig, 113, 0, 111, 108), 94 + 619 - 111, 0)
    # .touchpad 333x177 @ (237,118), pressed: sprite centred; .meta 51x40 @ left 378 bottom 255 -> top 651-255-40
    S["TOUCH"] = (L("touchpad"), 237, 118)
    S["META"] = (L("meta"), 378, 651 - 255 - 40)
    return (807, 651), S


# human: the DualSense face buttons in clean saturated PlayStation colours (the greyscale pass made them near-white discs and the
# pressed ones a white flash). Built from the resting sprite's shape: its alpha, and the glyph = the dark lines inside the face.
XBOX_FACE_RGB = {"A": (80, 214, 96), "B": (255, 64, 64), "X": (66, 135, 255), "Y": (255, 205, 40)}
PS_FACE_RGB = {"A": (66, 135, 255), "B": (255, 64, 64), "X": (255, 105, 200), "Y": (40, 215, 140)}  # cross, circle, square, triangle


def ps_face(src, rgb, lit):
    """58x58 face button: dark disc, grey rim, coloured glyph; pressed: the rim and a tint of the disc in the colour too."""
    from PIL import ImageFilter
    w, h = src.size
    cx, cy = (w - 1) / 2.0, (h - 1) / 2.0
    glyph = Image.new("L", src.size)
    for y in range(h):
        for x in range(w):
            r, g, b, a = src.getpixel((x, y))
            if a and math.hypot(x - cx, y - cy) < 21.0:  # inside the light face (rim ring starts at ~22)
                glyph.putpixel((x, y), max(0, min(255, int((222 - r) * 255 / (222 - 88)))))
    glyph = glyph.filter(ImageFilter.MaxFilter(3))  # bolder lines, readable at overlay size like the Xbox letters
    dark, rim = (30, 30, 34), (96, 96, 102)
    out = Image.new("RGBA", src.size)
    for y in range(h):
        for x in range(w):
            a = src.getpixel((x, y))[3]
            if not a:
                continue
            d = math.hypot(x - cx, y - cy)
            base = (rgb if lit else rim) if d >= 21.0 else tuple(int(dk * 0.65 + c * 0.35) for dk, c in zip(dark, rgb)) if lit else dark
            k = glyph.getpixel((x, y)) / 255.0
            out.putpixel((x, y), tuple(int(bc * (1 - k) + c * k + 0.5) for bc, c in zip(base, rgb)) + (a,))
    return out


def face_fill(src):
    """Pressed face button BACKGROUND only (rim + disc, no symbol), neutral grey / white so the pressed-colour setting tints it
    (vertex colour multiply). The symbol is the separate *_GLYPH sprite, drawn untinted on top (human: pressed colour must not
    recolour the outline / glyph)."""
    w, h = src.size
    cx, cy = (w - 1) / 2.0, (h - 1) / 2.0
    out = Image.new("RGBA", src.size)
    for y in range(h):
        for x in range(w):
            a = src.getpixel((x, y))[3]
            if a:
                v = 255 if math.hypot(x - cx, y - cy) >= 21.0 else 110
                out.putpixel((x, y), (v, v, v, a))
    return out


def face_glyph(src, rgb, ps):
    """The face symbol alone (cross / circle / square / triangle, or the Xbox letter) in its own colour with a thin dark outline,
    transparent elsewhere: readable on any pressed-colour fill."""
    from PIL import ImageFilter
    w, h = src.size
    cx, cy = (w - 1) / 2.0, (h - 1) / 2.0
    mask = Image.new("L", src.size)
    for y in range(h):
        for x in range(w):
            r, g, b, a = src.getpixel((x, y))
            if a and math.hypot(x - cx, y - cy) < 21.0:
                v = (222 - r) * 255 / (222 - 88) if ps else (r - 38) * 255 / (247 - 38)
                mask.putpixel((x, y), max(0, min(255, int(v))))
    mask = mask.filter(ImageFilter.MaxFilter(3))
    halo = mask.filter(ImageFilter.MaxFilter(5)).filter(ImageFilter.GaussianBlur(0.5))
    out = Image.new("RGBA", src.size, (16, 16, 20, 0))
    out.putalpha(halo.point(lambda v: min(255, int(v * 1.6))))
    fg = Image.new("RGBA", src.size, rgb + (0,))
    fg.putalpha(mask)
    return Image.alpha_composite(out, fg)


def xbox_face(src, rgb, lit=True):
    """50x50 pressed face button: dark disc tinted with the button colour, coloured rim and letter (was a white-letter flash)."""
    from PIL import ImageFilter
    w, h = src.size
    cx, cy = (w - 1) / 2.0, (h - 1) / 2.0
    glyph = Image.new("L", src.size)
    for y in range(h):
        for x in range(w):
            r, g, b, a = src.getpixel((x, y))
            if a and math.hypot(x - cx, y - cy) < 21.0:
                glyph.putpixel((x, y), max(0, min(255, int((r - 38) * 255 / (247 - 38)))))
    glyph = glyph.filter(ImageFilter.MaxFilter(3))
    dark = (30, 30, 34)
    disc = tuple(int(dk * 0.65 + c * 0.35) for dk, c in zip(dark, rgb)) if lit else dark
    out = Image.new("RGBA", src.size)
    for y in range(h):
        for x in range(w):
            a = src.getpixel((x, y))[3]
            if not a:
                continue
            base = (rgb if lit else (96, 96, 102)) if math.hypot(x - cx, y - cy) >= 21.0 else disc
            k = glyph.getpixel((x, y)) / 255.0
            out.putpixel((x, y), tuple(int(bc * (1 - k) + c * k + 0.5) for bc, c in zip(base, rgb)) + (a,))
    return out


def svg(name, box=None):
    import resvg_py
    png = bytes(resvg_py.svg_to_bytes(svg_path=str(ASSETS / "xbox" / f"{name}.svg")))
    im = Image.open(io.BytesIO(png)).convert("RGBA")
    if box:  # CSS element box: the background sits at its top-left at natural size, clipped to the box
        out = Image.new("RGBA", box)
        out.paste(im, (0, 0))
        im = out
    return im


def xbox():
    S = {}
    # #gamepad 721.14x600, background gamepad.svg
    S["BODY"] = (svg("gamepad"), 0, 0)
    # .sticks 346.4x188 @ (141.5,204); .stick 72x72 stick.svg, pressed stick_.svg; left (0,0); right bottom 0 right 0
    st, stp = svg("stick", (72, 72)), svg("stick_", (72, 72))
    S["STICK"], S["STICK_P"] = (st, 141.5, 204), (stp, 141.5, 204)
    rx, ry = 141.5 + 346.4 - 72, 204 + 188 - 72
    S["STICK_R"], S["STICK_R_P"] = (st, rx, ry), (stp, rx, ry)
    # .buttons 147x147 @ (470.71,166.9); .button 50x50 opacity 0 (pressed 1); a bottom 0 left 48.5; b top 48.8 right 0;
    # x top 48.5 right 98; y top 0 left 48.5
    bx, by = 470.71, 166.9
    for k, px, py in (("A", 48.5, 147 - 50), ("B", 147 - 50, 48.8), ("X", 147 - 98 - 50, 48.5), ("Y", 48.5, 0)):
        S[k + "_LIT"] = (face_fill(svg(k + "_", (50, 50))), bx + px, by + py)  # pressed: tintable background; symbol = _GLYPH
        S[k + "_GLYPH"] = (face_glyph(svg(k + "_", (50, 50)), XBOX_FACE_RGB[k], False), bx + px, by + py)
        S[k] = (xbox_face(svg(k + "_", (50, 50)), XBOX_FACE_RGB[k], False), bx + px, by + py)  # resting: coloured letter like the PS pad
    # .dpad 112.2 @ (213,299.8); .face 39x39 pad_.svg opacity 0 (pressed 1); up rotate .25turn (36.1,1); down .75turn
    # bottom 1 left 36.1; left (1,37); right .5turn top 37 right 1   (CSS rotate = clockwise; PIL rotate = counter-clockwise)
    pad = svg("pad_", (39, 39))
    dx, dy = 213, 299.8
    S["UP"] = (pad.rotate(-90), dx + 36.1, dy + 1)
    S["DOWN"] = (pad.rotate(-270), dx + 36.1, dy + 112.2 - 1 - 39)
    S["LEFT"] = (pad, dx + 1, dy + 37)
    S["RIGHT"] = (pad.rotate(-180), dx + 112.2 - 1 - 39, dy + 37)
    # .arrows 139.03x32.93 @ (291.2,222.37); select map_.svg / start settings_.svg 32.92x32.92 opacity 0 (pressed 1)
    S["BACK"] = (svg("map_", (33, 33)), 291.2, 222.37)
    S["START"] = (svg("settings_", (33, 33)), 291.2 + 139.03 - 32.92, 222.37)
    # .bumpers 566.86x128.92 @ (77.32,93.5); .bumper 175.74x128.92 bumper_.svg opacity 0 (pressed 1); right mirrored
    b = svg("bumper_")
    S["LB"] = (b, 77.32, 93.5)
    S["RB"] = (b.transpose(Image.FLIP_LEFT_RIGHT), 77.32 + 566.86 - 175.74, 93.5)
    # .triggers 434.62x103.07 @ left 143.1; .trigger 84.2x103.07 trigger_.svg, meter: clip-path inset((1-v)*100% 0 0 0);
    # right mirrored
    t = svg("trigger_")
    S["LT"] = (t, 143.1, 0)
    S["RT"] = (t.transpose(Image.FLIP_LEFT_RIGHT), 143.1 + 434.62 - 84.2, 0)
    # .meta 58x58 @ (331.57,135) meta.svg opacity 0 (pressed 1). No touchpad on an Xbox pad.
    S["META"] = (svg("meta", (58, 58)), 331.57, 135)
    return (721.14, 600), S


def pack(S):
    items = sorted(S.items(), key=lambda kv: -kv[1][0].size[1])
    x = y = PAD
    shelf = 0
    place = {}
    for k, (im, _, _) in items:
        w, h = im.size
        if x + w + PAD > ATLAS_W:
            x, y = PAD, y + shelf + PAD
            shelf = 0
        place[k] = (x, y)
        x += w + PAD
        shelf = max(shelf, h)
    H = y + shelf + PAD
    H = (H + 3) // 4 * 4
    atlas = Image.new("RGBA", (ATLAS_W, H))
    for k, (im, _, _) in S.items():
        atlas.paste(im, place[k])
    return atlas, place


def main():
    hdr = ["// Generated by tools\\gen_padviz_atlas.py from assets\\padviz - do not edit.",
           "// Sprite rects in the atlas (texels) and their place on the skin canvas (CSS px). See assets\\padviz\\CREDITS.txt.",
           "#pragma once", "#include <cstdint>", "", "namespace padviz {", "",
           "enum Sprite : int {" + ", ".join(f"S_{n}" for n in IDS) + ", S_COUNT};",
           "struct SpriteRect { uint16_t x, y, w, h; float cx, cy; };  // w = 0: the skin has no such layer",
           "struct Skin { float canvasW, canvasH; int atlasW, atlasH; SpriteRect s[S_COUNT]; };", ""]
    data = ["// Generated by tools\\gen_padviz_atlas.py from assets\\padviz - do not edit.",
            "// The two sprite atlases as PNG files (decoded once at run time by padviz_png.h). Art credits:",
            "//   DualSense: Gamepad Viewer DualSense skin, CrazyGrape (vector art) + Cubio300 (touchpad, PS button),",
            "//              CC BY 3.0 US. DualSense design and PlayStation logo are Sony IP.",
            "//   Xbox:      skyjty/MyGamepadViewer \"xbox_star\" skin, (c) 2024 skyqyzm, MIT. Xbox design/logo are Microsoft IP.",
            "#pragma once", "#include <cstddef>", "", "namespace padviz {", ""]
    for name, fn in (("dualsense", dualsense), ("xbox", xbox)):
        (cw, ch), S = fn()
        atlas, place = pack(S)
        # user #23181: black / white / grey only - drop all colour, keep alpha
        # user #23313: dark see-through like the mod menu. Xbox: invert everything (dark body, white lit buttons).
        # DualSense: body darkened to 0.08..0.40, every other sprite bright (0.55..1.0) so lit parts show on it.
        lum = atlas.convert("L")
        if name == "xbox":
            lum = lum.point(lambda v: int(255 * (0.10 + 0.90 * (1 - v / 255.0))))
            atlas = Image.merge("RGBA", (lum, lum, lum, atlas.getchannel("A")))
            for key in XBOX_FACE_RGB:  # human #38760: same clean coloured face buttons as the PS pad (pressed state)
                atlas.paste(S[key + "_LIT"][0], place[key + "_LIT"])
                atlas.paste(S[key + "_GLYPH"][0], place[key + "_GLYPH"])
                atlas.paste(S[key][0], place[key])
        else:
            out_l = Image.new("L", atlas.size)
            bright = lum.point(lambda v: int(255 * (0.55 + 0.45 * (1 - v / 255.0))))
            dark = lum.point(lambda v: int(255 * (0.06 + 0.22 * v / 255.0)))
            # user #23343: outlines as crisp as the Xbox skin - light edge lines on the dark body
            from PIL import ImageFilter, ImageChops
            edge = ImageChops.difference(lum.filter(ImageFilter.GaussianBlur(1.2)), lum).point(lambda v: min(255, int(v * 7)))
            edge = edge.filter(ImageFilter.GaussianBlur(0.4)).point(lambda v: int(v * 0.75))
            dark = ImageChops.add(dark, edge)
            bx, by, bw, bh = place["BODY"] + S["BODY"][0].size
            out_l.paste(bright, (0, 0))
            out_l.paste(dark.crop((bx, by, bx + bw, by + bh)), (bx, by))
            lit = lum.point(lambda v: int(255 * (0.70 + 0.30 * v / 255.0)))
            for key, tone in (("STICK", dark), ("STICK_R", dark), ("STICK_P", lit), ("STICK_R_P", lit)):  # resting caps dark, pressed bright
                if key in S:
                    kx, ky = place[key]
                    kw, kh = S[key][0].size
                    out_l.paste(tone.crop((kx, ky, kx + kw, ky + kh)), (kx, ky))
            atlas = Image.merge("RGBA", (out_l, out_l, out_l, atlas.getchannel("A")))
            for key, rgb in PS_FACE_RGB.items():  # human: coloured PS face buttons (resting sprite shape for both states)
                atlas.paste(ps_face(S[key][0], rgb, False), place[key])
                atlas.paste(face_fill(S[key][0]), place[key + "_LIT"])  # pressed: tintable background; symbol = _GLYPH
                atlas.paste(face_glyph(S[key][0], rgb, True), place[key + "_GLYPH"])
        out = ASSETS / f"atlas_{name}.png"
        atlas.save(out, optimize=True)
        raw = out.read_bytes()
        rows = []
        for n in IDS:
            if n in S:
                im, cx, cy = S[n]
                x, y = place[n]
                rows.append(f"    {{{x}, {y}, {im.size[0]}, {im.size[1]}, {float(cx):.2f}f, {float(cy):.2f}f}},  // {n}")
            else:
                rows.append(f"    {{0, 0, 0, 0, 0.0f, 0.0f}},  // {n} (none)")
        hdr.append(f"inline constexpr Skin kSkin_{name} = {{{float(cw):.2f}f, {float(ch):.2f}f, {atlas.size[0]}, {atlas.size[1]}, {{")
        hdr += rows
        hdr += ["}};", ""]
        data.append(f"inline const unsigned char kAtlasPng_{name}[{len(raw)}] = {{")
        for i in range(0, len(raw), 32):
            data.append("    " + ",".join(str(b) for b in raw[i:i + 32]) + ",")
        data += ["};", ""]
        print(f"{name}: atlas {atlas.size}, png {len(raw)} bytes, {len(S)} sprites")
    hdr += ["}  // namespace padviz", ""]
    data += ["}  // namespace padviz", ""]
    (ROOT / "src" / "padviz_atlas.h").write_text("\n".join(hdr), newline="\n")
    (ROOT / "src" / "padviz_atlas_data.h").write_text("\n".join(data), newline="\n")


if __name__ == "__main__":
    main()
