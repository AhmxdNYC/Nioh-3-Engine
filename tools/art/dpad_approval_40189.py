"""Art-only Xbox/PlayStation D-pad approval sheet, vector-derived PNG."""
from pathlib import Path
import argparse
import resvg_py
p = argparse.ArgumentParser(); p.add_argument('out', type=Path); a=p.parse_args(); a.out.mkdir(parents=True, exist_ok=True)
s=['<svg xmlns="http://www.w3.org/2000/svg" width="1640" height="858" viewBox="0 0 1640 858">','<rect width="1640" height="858" fill="#11141a"/>']
def label(x,y,t,size=18,color='#eef0f5',anchor='start'):
    s.append(f'<text x="{x}" y="{y}" font-family="Segoe UI, sans-serif" font-size="{size}" fill="{color}" text-anchor="{anchor}">{t}</text>')
def pad(cx,cy,scale,direction,xbox):
    s.append(f'<g transform="translate({cx-17.5*scale} {cy-17.5*scale}) scale({scale})">')
    if xbox:
        # One continuous plate, the identifying Xbox silhouette. Flat center.
        s.append('<path d="M13 3 H22 L23 4 V12 H31 L32 13 V22 L31 23 H23 V31 L22 32 H13 L12 31 V23 H4 L3 22 V13 L4 12 H12 V4 Z" fill="#333741" stroke="#969daa" stroke-width="1.25" stroke-linejoin="round"/>')
        s.append(f'<g transform="rotate({direction*90} 17.5 17.5)"><path d="M13.25 4.25 H21.75 V13 H13.25 Z" fill="#f1f2f5"/></g>')
    else:
        # Four physically separated PlayStation keys, with inward chamfers.
        for arm in range(4):
            fill='#f1f2f5' if arm==direction else '#333741'
            s.append(f'<g transform="rotate({arm*90} 17.5 17.5)"><path d="M14 3 H21 L22 4 V11.5 L20 14 H15 L13 11.5 V4 Z" fill="{fill}" stroke="#969daa" stroke-width="1.15" stroke-linejoin="round"/></g>')
    s.append('</g>')
label(28,44,'D-PAD / XBOX + PLAYSTATION',30)
label(28,78,'Separate controller skins. Pressed arm in white. Movement keeps its existing shaft-and-head arrows.',20,'#b1b8c6')
for top,bg,fg,muted,cell,theme in [(116,'#20242c','#eef0f5','#aab2c1','#292e38','DARK'),(452,'#e4e8ef','#1c2430','#4c586b','#cdd5e0','BRIGHT')]:
    for skin,left in [('XBOX',24),('PLAYSTATION',836)]:
        xbox=skin=='XBOX'
        s.append(f'<rect x="{left}" y="{top}" width="780" height="312" rx="12" fill="{bg}"/>')
        label(left+20,top+31,skin,23,fg)
        label(left+760,top+29,theme+' BACKGROUND',14,muted,'end')
        label(left+20,top+55,'Single joined cross-plate' if xbox else 'Four separated directional keys',16,muted)
        label(left+13,top+121,'1x',13,muted)
        label(left+13,top+155,'3x',13,muted)
        for direction,name in enumerate(['UP','RIGHT','DOWN','LEFT']):
            cx=left+105+190*direction
            label(cx,top+81,name,15,fg,'middle')
            for scale,cy in [(1,top+116),(3,top+224)]:
                w,h=59*scale,44*scale
                s.append(f'<rect x="{cx-w/2}" y="{cy-h/2}" width="{w}" height="{h}" rx="{3*scale}" fill="{cell}"/>')
                pad(cx,cy,scale,direction,xbox)
label(28,803,'ART ONLY / APPROVAL PREVIEW',20)
label(28,834,'35px native icon in the same 59 x 44px input cell; 3x detail below each row. No game changes.',18,'#b1b8c6')
s.append('</svg>'); svg='\n'.join(s)
(a.out/'dpad-xbox-ps-40189.svg').write_text(svg,encoding='utf-8')
f=a.out/'dpad-xbox-ps-40189.png'; f.write_bytes(resvg_py.svg_to_bytes(svg_string=svg)); print(f)
