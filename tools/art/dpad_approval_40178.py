"""Art-only D-pad proposal. Vector geometry; no runtime assets/builds changed."""
from pathlib import Path
import argparse
import resvg_py

parser = argparse.ArgumentParser()
parser.add_argument('out', type=Path)
args = parser.parse_args()
args.out.mkdir(parents=True, exist_ok=True)
parts = ['<svg xmlns="http://www.w3.org/2000/svg" width="1040" height="740" viewBox="0 0 1040 740">',
         '<rect width="1040" height="740" fill="#11141a"/>']

def text(x, y, value, size=18, color='#e9ebf0', anchor='start'):
    parts.append(f'<text x="{x}" y="{y}" font-family="Segoe UI, sans-serif" font-size="{size}" fill="{color}" text-anchor="{anchor}">{value}</text>')

def dpad(cx, cy, scale, direction):
    # 35px icon, 29px cross silhouette. Corners chamfered, no arrowheads.
    parts.append(f'<g transform="translate({cx-17.5*scale} {cy-17.5*scale}) scale({scale})">')
    parts.append('<path d="M14 3 H21 L22 4 V13 H31 L32 14 V21 L31 22 H22 V31 L21 32 H14 L13 31 V22 H4 L3 21 V14 L4 13 H13 V4 Z" fill="#333741" stroke="#959ba8" stroke-width="1.25" stroke-linejoin="round"/>')
    # A filled rectangular arm is unmistakably a pad press, never movement art.
    parts.append(f'<g transform="rotate({direction*90} 17.5 17.5)"><path d="M14 4 H21 V13.5 H14 Z" fill="#f1f2f5"/></g>')
    parts.append('<rect x="14.25" y="14.25" width="6.5" height="6.5" rx="0.65" fill="#191c23"/>')
    parts.append('</g>')

text(36, 45, 'D-PAD / INPUT DISPLAY', 28)
text(36, 76, 'One cross. Only the pressed arm lights up. No movement-arrow shape.', 18, '#b1b8c6')
for panel_y, bg, fg, secondary, cell in [(104, '#20242c', '#eef0f5', '#aab2c1', '#292e38'),
                                       (390, '#e4e8ef', '#1c2430', '#4c586b', '#cdd5e0')]:
    parts.append(f'<rect x="24" y="{panel_y}" width="992" height="258" rx="12" fill="{bg}"/>')
    text(44, panel_y+30, 'DARK BACKGROUND' if panel_y==104 else 'BRIGHT BACKGROUND', 16, fg)
    for col, direction in enumerate(['UP', 'RIGHT', 'DOWN', 'LEFT']):
        cx = 300 + col * 194
        text(cx, panel_y+30, direction, 16, fg, 'middle')
        for scale, cy in [(1, panel_y+80), (3, panel_y+182)]:
            # Exactly the 59x44 column/row used by the current 1440p history.
            w, h = 59*scale, 44*scale
            parts.append(f'<rect x="{cx-w/2}" y="{cy-h/2}" width="{w}" height="{h}" rx="{3*scale}" fill="{cell}"/>')
            dpad(cx, cy, scale, col)
    text(44, panel_y+75, '1x / ACTUAL SIZE', 16, fg)
    text(44, panel_y+99, '35px icon, 59 x 44px cell', 13, secondary)
    text(44, panel_y+175, '3x / DETAIL', 16, fg)
    text(44, panel_y+199, 'Same geometry, enlarged', 13, secondary)
text(36, 688, 'ART PROPOSAL ONLY / awaiting approval', 18)
text(36, 716, 'D-pad presses stay distinct from the existing shaft-and-head movement arrows.', 16, '#aab2c1')
parts.append('</svg>')
svg = '\n'.join(parts)
(args.out / 'dpad-approval-40178.svg').write_text(svg, encoding='utf-8')
png = args.out / 'dpad-approval-40178.png'
png.write_bytes(resvg_py.svg_to_bytes(svg_string=svg))
print(png)
