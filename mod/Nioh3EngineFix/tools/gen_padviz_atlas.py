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
       "RIGHT", "BACK", "START", "LB", "RB", "LT", "RT", "TOUCH", "META", "STICK_R", "STICK_R_P"]


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
    shoulders_ds(S)  # redrawn LB / RB / LT / RT (see shoulder())
    # .triggers 619x108 @ left 94 top 0; .trigger 111x108 opacity = value; right bg -113 0, rotateY(0) (not mirrored)
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


def face_lit(src, ps):
    """Pressed face button as ONE tintable sprite (human: the whole button - rim, disc and symbol - takes the pressed colour):
    light rim, slightly darker disc, the symbol (cross / circle / square / triangle / letter) a darker shade of the same colour so it
    stays readable. Grey levels only: the vertex-colour multiply in padviz.h Layer turns it into the pressed colour."""
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
    out = Image.new("RGBA", src.size)
    for y in range(h):
        for x in range(w):
            a = src.getpixel((x, y))[3]
            if a:
                base = 255 if math.hypot(x - cx, y - cy) >= 21.0 else 232
                k = mask.getpixel((x, y)) / 255.0
                v = int(base * (1 - k) + 105 * k + 0.5)
                out.putpixel((x, y), (v, v, v, a))
    return out


SS = 4  # shape supersampling


def _q(p0, c, p1, n=24):
    return [((1 - t) ** 2 * p0[0] + 2 * t * (1 - t) * c[0] + t * t * p1[0], (1 - t) ** 2 * p0[1] + 2 * t * (1 - t) * c[1] + t * t * p1[1])
            for t in (i / n for i in range(n + 1))]


def shoulder(size, segs, rnd, lo=255, hi=212):
    """A smooth shoulder sprite (bumper / trigger): closed outline from quadratic segments [(p0, ctrl, p1), ...] in local px,
    corners rounded by `rnd` px (blur + threshold), 4x supersampled, vertical gradient lo (top) -> hi (bottom). Returns the
    tight-cropped RGBA sprite and its (x, y) offset inside `size`."""
    from PIL import ImageDraw, ImageFilter
    pts = []
    for p0, c, p1 in segs:
        pts += _q(p0, c, p1)
    W, H = size
    m = Image.new("L", (W * SS, H * SS), 0)
    ImageDraw.Draw(m).polygon([(x * SS, y * SS) for x, y in pts], fill=255)
    m = m.filter(ImageFilter.GaussianBlur(rnd * SS)).point(lambda v: 255 if v > 128 else 0)
    m = m.filter(ImageFilter.GaussianBlur(0.3 * SS)).resize((W, H), Image.LANCZOS)
    bb = m.point(lambda v: 255 if v > 8 else 0).getbbox()
    m = m.crop(bb)
    gh = max(1, m.size[1] - 1)
    g = Image.new("L", m.size)
    for y in range(m.size[1]):
        for x in range(m.size[0]):
            g.putpixel((x, y), int(lo + (hi - lo) * y / gh))
    return Image.merge("RGBA", (g, g, g, m)), bb[0], bb[1]


def place_pair(S, name_l, name_r, size, segs, rnd, x0, y0, centre):
    """Left shoulder sprite at canvas (x0, y0) and its mirror image about the vertical line x = centre."""
    sp, ox, oy = shoulder(size, segs, rnd)
    S[name_l] = (sp, x0 + ox, y0 + oy)
    S[name_r] = (sp.transpose(Image.FLIP_LEFT_RIGHT), 2 * centre - (x0 + ox) - sp.size[0], y0 + oy)


# Redrawn shoulder shapes (human: bumpers / triggers looked like plain squares): a curved bumper band and a tapered, rounded
# trigger, smooth like the rest of the skin, drawn on the canvas slots of the body's own bumper / trigger outlines.
def shoulders_xbox(S):
    place_pair(S, "LB", "RB", (176, 129),
               [((6, 70), (60, 16), (172, 8)), ((172, 8), (180, 24), (172, 40)), ((172, 40), (80, 44), (10, 108)),
                ((10, 108), (0, 92), (6, 70))], 4, 77.32, 93.5, 77.32 + 566.86 / 2)
    place_pair(S, "LT", "RT", (84, 103),
               [((0, 102), (20, 62), (36, 14)), ((36, 14), (44, 0), (62, 2)), ((62, 2), (80, 6), (84, 30)),
                ((84, 30), (86, 70), (84, 102)), ((84, 102), (42, 92), (0, 102))], 5, 143.1, 0, 143.1 + 434.62 / 2)


def shoulders_ds(S):
    place_pair(S, "LB", "RB", (110, 35),
               [((0, 32), (14, 8), (60, 2)), ((60, 2), (96, 0), (110, 14)), ((110, 14), (112, 26), (104, 28)),
                ((104, 28), (60, 20), (10, 34)), ((10, 34), (2, 36), (0, 32))], 3, 93, 114, 93 + 620 / 2)
    place_pair(S, "LT", "RT", (111, 108),
               [((5, 105), (10, 50), (18, 24)), ((18, 24), (28, 2), (62, 2)), ((62, 2), (98, 4), (104, 40)),
                ((104, 40), (108, 70), (109, 105)), ((109, 105), (56, 96), (5, 105))], 5, 94, 0, 94 + 619 / 2)


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
        S[k + "_LIT"] = (face_lit(svg(k + "_", (50, 50)), False), bx + px, by + py)  # pressed: whole button tintable
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
    # .triggers 434.62x103.07 @ left 143.1; .trigger 84.2x103.07 trigger_.svg, meter: clip-path inset((1-v)*100% 0 0 0);
    # right mirrored
    shoulders_xbox(S)  # redrawn LB / RB / LT / RT (see shoulder())
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
                atlas.paste(S[key][0], place[key])
            for key in ("LB", "RB", "LT", "RT"):  # redrawn shoulder shapes: bright, no tone mapping
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
                atlas.paste(face_lit(S[key][0], True), place[key + "_LIT"])  # pressed: the whole button, tintable
            for key in ("LB", "RB", "LT", "RT"):  # redrawn shoulder shapes: bright, no tone mapping
                atlas.paste(S[key][0], place[key])
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
