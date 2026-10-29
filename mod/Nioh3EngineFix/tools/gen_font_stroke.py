#!/usr/bin/env python3
"""Generate the original angular UI stroke font (ASCII 32..126).

The approved input-history digits and B/L/N/R/T are imported unchanged from
inputhist_glyphs.inc. Remaining letters and punctuation use the same straight
strokes and clipped corners. Rendering is deterministic, uses only stdlib,
and preserves the existing 8x16 / four-bit OSD atlas and text layout contract.
Optional --preview PATH uses Pillow to show the actual generated atlas.
"""
from pathlib import Path
import argparse
import math
import re

ROOT = Path(__file__).resolve().parents[1]
W, H, FIRST, LAST = 8, 16, 32, 126
GLYPHS = {}


def glyph(char, *paths):
    """Polyline strings in a unit box; semicolon-separated x,y coordinates."""
    segments = []
    for path in paths:
        points = [tuple(map(float, point.split(','))) for point in path.split(';')]
        segments.extend((*a, *b) for a, b in zip(points, points[1:]))
    GLYPHS[char] = segments


# Uppercase: clipped, angular bowls rather than a rounded system typeface.
glyph('A', '0,1;0,.22;.22,0;.78,0;1,.22;1,1', '0,.56;1,.56')
glyph('C', '1,.12;.84,0;.16,0;0,.16;0,.84;.16,1;.84,1;1,.88')
glyph('D', '0,0;0,1;.72,1;1,.78;1,.22;.72,0;0,0')
glyph('E', '1,0;0,0;0,1;1,1', '0,.5;.8,.5')
glyph('F', '1,0;0,0;0,1', '0,.5;.8,.5')
glyph('G', '1,.12;.84,0;.16,0;0,.16;0,.84;.16,1;.84,1;1,.84;1,.55;.55,.55')
glyph('H', '0,0;0,1', '1,0;1,1', '0,.5;1,.5')
glyph('I', '.15,0;.85,0', '.5,0;.5,1', '.15,1;.85,1')
glyph('J', '.2,0;1,0;1,.83;.83,1;.17,1;0,.83;0,.68')
glyph('K', '0,0;0,1', '1,0;0,.55', '.4,.33;1,1')
glyph('M', '0,1;0,0;.5,.48;1,0;1,1')
glyph('O', '.16,0;.84,0;1,.16;1,.84;.84,1;.16,1;0,.84;0,.16;.16,0')
glyph('P', '0,1;0,0;.84,0;1,.16;1,.4;.84,.56;0,.56')
glyph('Q', '.16,0;.84,0;1,.16;1,.8;.8,1;.16,1;0,.84;0,.16;.16,0', '.6,.65;1,1')
glyph('S', '1,.12;.84,0;.16,0;0,.16;0,.34;.16,.5;.84,.5;1,.66;1,.84;.84,1;.16,1;0,.88')
glyph('U', '0,0;0,.84;.16,1;.84,1;1,.84;1,0')
glyph('V', '0,0;.5,1;1,0')
glyph('W', '0,0;.2,1;.5,.58;.8,1;1,0')
glyph('X', '0,0;1,1', '1,0;0,1')
glyph('Y', '0,0;.5,.5;1,0', '.5,.5;.5,1')
glyph('Z', '0,0;1,0;0,1;1,1')

# Lowercase retains word shape, ascenders and descenders for long menu labels.
glyph('a', '0,.38;.17,.28;.83,.28;1,.45;1,1', '1,.56;.17,.56;0,.7;0,.85;.17,1;.83,1;1,.85')
glyph('b', '0,0;0,1', '0,.45;.18,.28;.82,.28;1,.45;1,.83;.82,1;.18,1;0,.83')
glyph('c', '1,.4;.82,.28;.18,.28;0,.46;0,.82;.18,1;.82,1;1,.88')
glyph('d', '1,0;1,1', '1,.45;.82,.28;.18,.28;0,.45;0,.83;.18,1;.82,1;1,.83')
glyph('e', '0,.62;1,.62;1,.46;.82,.28;.18,.28;0,.46;0,.82;.18,1;.82,1;1,.88')
glyph('f', '.25,1;.25,.16;.42,0;.85,0', '0,.32;.75,.32')
glyph('g', '1,.28;1,1.12;.82,1.26;.18,1.26', '1,.45;.82,.28;.18,.28;0,.45;0,.82;.18,1;.82,1;1,.82')
glyph('h', '0,0;0,1', '0,.46;.18,.28;.82,.28;1,.46;1,1')
glyph('i', '.3,.3;.55,.3;.55,1', '.3,1;.8,1', '.55,.02;.55,.06')
glyph('j', '.35,.3;.65,.3;.65,1.1;.5,1.26;.1,1.26', '.65,.02;.65,.06')
glyph('k', '0,0;0,1', '1,.28;0,.7', '.42,.52;1,1')
glyph('l', '.25,0;.5,0;.5,.88;.62,1;.85,1')
glyph('m', '0,1;0,.28', '0,.45;.16,.28;.34,.28;.5,.45;.66,.28;.84,.28;1,.45;1,1', '.5,.45;.5,1')
glyph('n', '0,1;0,.28', '0,.46;.18,.28;.82,.28;1,.46;1,1')
glyph('o', '.18,.28;.82,.28;1,.46;1,.82;.82,1;.18,1;0,.82;0,.46;.18,.28')
glyph('p', '0,1.26;0,.28', '0,.45;.18,.28;.82,.28;1,.45;1,.83;.82,1;.18,1;0,.83')
glyph('q', '1,1.26;1,.28', '1,.45;.82,.28;.18,.28;0,.45;0,.83;.18,1;.82,1;1,.83')
glyph('r', '0,1;0,.28', '0,.48;.2,.28;.75,.28;1,.45')
glyph('s', '1,.4;.82,.28;.18,.28;0,.43;0,.51;.18,.64;.82,.64;1,.79;1,.85;.82,1;.18,1;0,.88')
glyph('t', '.35,.05;.35,.85;.5,1;.85,1', '0,.32;.85,.32')
glyph('u', '0,.28;0,.82;.18,1;.82,1;1,.82', '1,.28;1,1')
glyph('v', '0,.28;.5,1;1,.28')
glyph('w', '0,.28;.2,1;.5,.65;.8,1;1,.28')
glyph('x', '0,.28;1,1', '1,.28;0,1')
glyph('y', '0,.28;.5,1', '1,.28;.38,1.26;.1,1.26')
glyph('z', '0,.28;1,.28;0,1;1,1')

# Full printable ASCII coverage: punctuation uses the same weight as letters.
GLYPHS[' '] = []
glyph('!', '.5,0;.5,.68', '.5,.94;.5,1')
glyph('"', '.25,0;.25,.22', '.75,0;.75,.22')
glyph('#', '.35,.08;.2,.92', '.8,.08;.65,.92', '0,.35;1,.35', '0,.65;1,.65')
glyph('$', '1,.18;.8,.08;.2,.08;0,.25;0,.35;.2,.5;.8,.5;1,.65;1,.75;.8,.92;.2,.92;0,.82', '.5,0;.5,1')
glyph('%', '0,1;1,0', '0,.05;.32,.05;.32,.3;0,.3;0,.05', '.68,.7;1,.7;1,.95;.68,.95;.68,.7')
glyph('&', '1,1;.15,.28;.15,.13;.3,0;.6,0;.75,.13;.75,.28;0,.7;0,.85;.15,1;.65,1;1,.65')
glyph("'", '.5,0;.5,.22')
glyph('(', '.7,0;.3,.22;.3,.78;.7,1')
glyph(')', '.3,0;.7,.22;.7,.78;.3,1')
glyph('*', '.5,.2;.5,.8', '.1,.35;.9,.65', '.1,.65;.9,.35')
glyph('+', '.1,.5;.9,.5', '.5,.2;.5,.8')
glyph(',', '.55,.88;.55,1;.3,1.2')
glyph('-', '.15,.55;.85,.55')
glyph('.', '.5,.94;.5,1')
glyph('/', '0,1;1,0')
glyph(':', '.5,.35;.5,.4', '.5,.9;.5,.95')
glyph(';', '.5,.35;.5,.4', '.5,.88;.5,1;.3,1.2')
glyph('<', '.85,.15;.15,.5;.85,.85')
glyph('=', '.1,.35;.9,.35', '.1,.7;.9,.7')
glyph('>', '.15,.15;.85,.5;.15,.85')
glyph('?', '0,.18;.18,0;.82,0;1,.18;1,.36;.5,.62;.5,.72', '.5,.94;.5,1')
glyph('@', '.8,.78;.8,.35;.4,.35;.25,.5;.25,.65;.4,.8;1,.8;1,.2;.8,0;.2,0;0,.2;0,.8;.2,1;.8,1')
glyph('[', '.8,0;.25,0;.25,1;.8,1')
glyph(chr(92), '0,0;1,1')
glyph(']', '.2,0;.75,0;.75,1;.2,1')
glyph('^', '.1,.35;.5,0;.9,.35')
glyph('_', '0,1.12;1,1.12')
glyph('`', '.3,0;.65,.22')
glyph('{', '.8,0;.5,0;.35,.15;.35,.35;.15,.5;.35,.65;.35,.85;.5,1;.8,1')
glyph('|', '.5,0;.5,1')
glyph('}', '.2,0;.5,0;.65,.15;.65,.35;.85,.5;.65,.65;.65,.85;.5,1;.2,1')
glyph('~', '0,.6;.25,.42;.75,.62;1,.44')


def import_approved():
    source = (ROOT / 'src/inputhist_glyphs.inc').read_text()
    for char, values in re.findall(r'static const float G_(.)\[\] = \{([^}]+)\}', source):
        coords = [float(value.rstrip('f')) for value in values.split(',')]
        assert len(coords) % 4 == 0
        GLYPHS[char] = [tuple(coords[i:i + 4]) for i in range(0, len(coords), 4)]
    assert all(char in GLYPHS for char in map(chr, range(FIRST, LAST + 1)))


def distance(px, py, segment):
    ax, ay, bx, by = segment
    dx, dy = bx - ax, by - ay
    t = max(0., min(1., ((px - ax) * dx + (py - ay) * dy) / (dx * dx + dy * dy)))
    return math.hypot(px - ax - t * dx, py - ay - t * dy)


def raster(char):
    # One-pixel side bearings and two-pixel top bearing; reserve descender room.
    segments = [(1.1 + a * 5.8, 2.2 + b * 10., 1.1 + c * 5.8, 2.2 + d * 10.)
                for a, b, c, d in GLYPHS[char]]
    pixels = []
    for y in range(H):
        row = []
        for x in range(W):
            covered = sum(any(distance(x + (sx + .5) / 8, y + (sy + .5) / 8, seg) <= .53
                              for seg in segments) for sy in range(8) for sx in range(8))
            row.append((covered * 15 + 32) // 64)
        pixels.append(row)
    return pixels


def generate():
    import_approved()
    atlas = {char: raster(char) for char in map(chr, range(FIRST, LAST + 1))}
    words = [sum(value << (4 * x) for x, value in enumerate(row))
             for pixels in atlas.values() for row in pixels]
    assert len(words) == 1520 and not any(words[:16])
    assert all(any(any(row) for row in pixels) for char, pixels in atlas.items() if char != ' ')
    header = [
        '// Generated by tools/gen_font_stroke.py - do not edit.',
        '// Original angular stroke glyphs; digits/B/L/N/R/T reuse approved input-history paths.',
        '// ASCII 32..126, 8x16 cells, 4-bit coverage; pixel x in bits 4x..4x+3.',
        '#pragma once', '#include <cstdint>', 'namespace osdfont {',
        'constexpr int kW = 8, kH = 16, kFirst = 32, kLast = 126;',
        'constexpr int kWords = 1520;',
        'alignas(16) static const uint32_t kBits[kWords] = {',
    ]
    header += ['    ' + ', '.join(f'0x{word:08X}' for word in words[i:i + 8]) + ','
               for i in range(0, len(words), 8)]
    header += ['};', '} // namespace osdfont', '']
    (ROOT / 'src/osd_font.h').write_text('\n'.join(header), encoding='utf-8')
    return atlas


def preview(atlas, destination):
    from PIL import Image
    lines = ['MOD MENU   CLOSE   REPORT', 'Boss Matches   Practice   Display Overlays',
             'Optimizations / Camera / Frame Meter', 'Left / right: -10   Up / down: +5',
             'Perf Ki   Invulnerable   Guard   Armor', 'Return to default camera',
             '0123456789  LB RB LT RT  N',
             'ABCDEFGHIJKLMNOPQRSTUVWXYZ', 'abcdefghijklmnopqrstuvwxyz',
             ''.join(map(chr, range(32, 65))), ''.join(map(chr, range(91, 97))) + '{|}~']
    image = Image.new('RGB', (max(map(len, lines)) * W + 20, len(lines) * 20 + 16), (20, 22, 28))
    for line_no, line in enumerate(lines):
        for index, char in enumerate(line):
            for y, row in enumerate(atlas[char]):
                for x, value in enumerate(row):
                    t = value / 15
                    image.putpixel((10 + index * W + x, 8 + line_no * 20 + y),
                                   tuple(round(bg * (1 - t) + fg * t) for bg, fg in zip((20, 22, 28), (232, 234, 240))))
    image.resize((image.width * 3, image.height * 3), Image.Resampling.NEAREST).save(destination)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--preview', type=Path)
    args = parser.parse_args()
    atlas = generate()
    if args.preview:
        preview(atlas, args.preview)
    print('Generated 95 ASCII glyphs, 1520 words, 8x16 four-bit coverage.')
