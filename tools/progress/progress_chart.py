"""Renderer decompilation progress chart -> docs\\progress\\renderer.svg (stdlib only).

One row per renderer package: its core functions (data\\partition.tsv depth < 50)
as a 100 % bar, green = real name, amber = Unknown_*, grey = not named (same rule and colours as
progress_treemap.py). The thin bar under it is the package's region-filled neighbours (depth 50:
functions that sit between known renderer code, unverified). Names come from symbols\\symbols.tsv, so rerun after
each symbols merge:  py -I tools\\progress\\progress_chart.py [--dump PATH] [--out DIR]
"""
import collections, os, sys
from xml.sax.saxutils import escape

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import common as C  # noqa: E402
import progress_treemap as pt  # noqa: E402  (load_syms, status, colour)
import perf_coverage as pc  # noqa: E402  (fold)

PART = C.data('partition.tsv')
OUT = os.path.join(C.OUT, 'renderer.svg')
# subsystem -> package (the map our renderer partition was built with)
PACKAGE = {'PRESENT': 'post', 'POST': 'post', 'UI': 'ui', 'VOLNAT': 'volumetrics', 'LIGHT': 'lighting', 'FX': 'effects',
           'DRAW': 'drawlists', 'SCENE': 'meshlet', 'PSO': 'materials', 'RESOURCE': 'resources', 'BACKEND': 'backend',
           'FRAME': 'frame-graph'}
NUM = {'backend': '01', 'materials': '02', 'lighting': '03', 'post': '04', 'frame-graph': '05', 'resources': '06',
       'drawlists': '07', 'meshlet': '08', 'volumetrics': '09', 'effects': '10', 'ui': '11'}
WHAT = {'backend': 'D3D12 submission, token replay', 'materials': 'shaders, PSOs, material binds',
        'lighting': 'G-buffer, shadows, GI, AO', 'post': 'upscaler, post chain, FG, present',
        'frame-graph': 'render graph build + walk', 'resources': 'heaps, render targets, uploads',
        'drawlists': 'views, draw lists, per-draw record', 'meshlet': 'geometry LOD + culling',
        'volumetrics': 'fog, clouds, sky, water', 'effects': 'particles, decals, effect lights',
        'ui': 'HUD / menus, glyphs'}
INK, INK2, MUTED, BG = '#e6edf3', '#c9d1d9', '#8b949e', '#0d1117'


NOTES_WRITTEN = set(C.meta().get('renderer_notes_written', '').split(','))


def note_written(pkg):
    return pkg in NOTES_WRITTEN


def main():
    names = pt.load_syms()
    rows = collections.defaultdict(lambda: {'core': [0, 0, 0], 'fill': [0, 0, 0]})
    for l in open(PART, encoding='utf-8'):
        if l.startswith('#'):
            continue
        f = l.rstrip('\n').split('\t')
        if len(f) < 3 or not f[0].startswith('0x') or f[1] not in PACKAGE:
            continue
        s = pt.status(names, pc.fold(int(f[0], 16)))
        k = 0 if s == 1.0 else 1 if s == 0.5 else 2
        rows[PACKAGE[f[1]]]['core' if int(f[2]) < 50 else 'fill'][k] += 1
    pkgs = sorted(rows, key=lambda p: -sum(rows[p]['core']))
    tot = [sum(rows[p]['core'][i] for p in pkgs) for i in range(3)]
    ftot = [sum(rows[p]['fill'][i] for p in pkgs) for i in range(3)]
    n_core, n_fill = sum(tot), sum(ftot)
    pct = lambda a, b: 100.0 * a / b if b else 0.0

    W, X0, BW, RH, TOP = 1000, 235, 500, 46, 92
    H = TOP + RH * len(pkgs) + 46
    o = ['<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 %d %d" style="width:100%%;height:auto" '
         'font-family="Segoe UI,Helvetica,Arial,sans-serif">' % (W, H),
         '<rect width="100%%" height="100%%" fill="%s"/>' % BG,
         '<text x="8" y="19" fill="%s" font-size="15" font-weight="700">Renderer decompilation: %s core functions '
         'in %d packages, %s named (%.1f %%)%s</text>' % (
             INK, format(n_core, ','), len(pkgs), format(tot[0], ','), pct(tot[0], n_core),
             ', %s Unknown_*' % format(tot[1], ',') if tot[1] else ''),
         pt.legend(W),
         '<text x="8" y="62" fill="%s" font-size="12">Thick bar = core functions (seeds + call-graph neighbours); '
         'thin bar = %s region-filled neighbours (unverified, %.1f %% named). Sorted by core size.</text>' % (
             MUTED, format(n_fill, ','), pct(ftot[0], n_fill))]
    for i, p in enumerate(pkgs):
        y = TOP + i * RH
        c, fl = rows[p]['core'], rows[p]['fill']
        n, nf = sum(c), sum(fl)
        o.append('<text x="8" y="%d" fill="%s" font-size="13" font-weight="600">%s %s</text>' % (y + 14, INK, NUM[p], p))
        o.append('<text x="8" y="%d" fill="%s" font-size="11">%s%s</text>' % (
            y + 29, MUTED, escape(WHAT[p]), '' if note_written(p) else ' (no note yet)'))
        for bar, cnt, tot_n, hgt, dy in ((c, None, n, 16, 3), (fl, None, nf, 5, 23)):
            x = X0
            for k, v in enumerate(bar):
                if not v:
                    continue
                w = BW * v / tot_n
                gap = 2 if x + w < X0 + BW - 0.5 else 0  # 2px surface gap between segments
                o.append('<rect x="%.1f" y="%d" width="%.1f" height="%d" fill="%s" opacity="%s"><title>%s %s: %d of %d</title></rect>' % (
                    x, y + dy, max(w - gap, 0.5), hgt, pt.colour((1.0, 0.5, 0.0)[k]), '1' if hgt > 5 else '0.75',
                    p, ('named', 'Unknown_*', 'not named')[k] + (' (region-filled)' if hgt == 5 else ''), v, tot_n))
                x += w
        o.append('<text x="%d" y="%d" fill="%s" font-size="12"><tspan font-weight="600">%.0f %%</tspan> named '
                 '<tspan fill="%s">(%s of %s%s)</tspan></text>' % (
                     X0 + BW + 12, y + 15, INK2, pct(c[0], n), MUTED, format(c[0], ','), format(n, ','),
                     ', %s Unknown_*' % format(c[1], ',') if c[1] else ''))
        if nf:
            o.append('<text x="%d" y="%d" fill="%s" font-size="11">+%s region-filled, %.1f %% named</text>' % (
                X0 + BW + 12, y + 30, MUTED, format(nf, ','), pct(fl[0], nf)))
    o.append('<text x="8" y="%d" fill="%s" font-size="11">Package 12 (capture-check) has no functions: it checks these '
             'notes against D3D12 frame captures from two PCs and the ROG Ally. Plan: '
             'research/renderer/00-map.md.</text>' % (H - 14, MUTED))
    o.append('</svg>')
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, 'w', encoding='utf-8', newline='\n') as fh:
        fh.write('\n'.join(o) + '\n')
    print('renderer: %d core fns, named %d (%.1f %%), Unknown_ %d; region-filled %d, named %.1f %% -> %s' % (
        n_core, tot[0], pct(tot[0], n_core), tot[1], n_fill, pct(ftot[0], n_fill), OUT))
    for p in pkgs:
        c = rows[p]['core']
        print('  %-12s core %5d named %5d (%5.1f %%) Unknown_ %4d' % (p, sum(c), c[0], pct(c[0], sum(c)), c[1]))


if __name__ == '__main__':
    main()
