r"""renderer_progress.py - "Rendering engine decompilation" graph, badge and table for the README.

usage: py -I tools\progress\renderer_progress.py [--check] [--dump PATH] [--out DIR]
Per renderer package: every function of data\partition.tsv (size from that file),
"understood" = it has a real name in symbols\symbols.tsv (not FUN_/sub_/Unknown_; same rule as progress_treemap.py).
Percent is weighted by code bytes. Package status: note written (renderer_notes_written in data\progress_meta.tsv),
capture-checked (claims checked against our D3D12 frame captures; CHECKED below).
Writes docs\progress\renderer_understood.svg, renderer_badge.svg, renderer_table.md (stdlib only). Rerun after each symbols merge.
"""
import collections, datetime, os, re, sys
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import common as C  # noqa: E402
import progress_treemap as pt  # noqa: E402
import perf_coverage as pc  # noqa: E402

ROOT = C.ROOT
PART = C.data('partition.tsv')
OUT = C.OUT
PACKAGE = {'PRESENT': 'post', 'POST': 'post', 'UI': 'ui', 'VOLNAT': 'volumetrics', 'LIGHT': 'lighting', 'FX': 'effects',
           'DRAW': 'drawlists', 'SCENE': 'meshlet', 'PSO': 'materials', 'RESOURCE': 'resources', 'BACKEND': 'backend',
           'FRAME': 'frame-graph'}
NUM = {'backend': '01', 'materials': '02', 'lighting': '03', 'post': '04', 'frame-graph': '05', 'resources': '06',
       'drawlists': '07', 'meshlet': '08', 'volumetrics': '09', 'effects': '10', 'ui': '11'}
CHECKED = {'backend', 'materials', 'lighting', 'post'}  # claims confirmed against our FCAP1 D3D12 frame captures
NOTES_WRITTEN = set(C.meta().get('renderer_notes_written', '').split(','))


def note_written(p):
    return p in NOTES_WRITTEN


def compute():
    names = pt.load_syms()
    seen, d = set(), collections.defaultdict(lambda: [0, 0, 0, 0])  # fn, bytes, named fn, named bytes
    for l in open(PART, encoding='utf-8'):
        f = l.rstrip('\n').split('\t')
        if len(f) < 4 or not f[0].startswith('0x') or f[1] not in PACKAGE:
            continue
        r = int(f[0], 16)
        if r in seen:
            continue
        seen.add(r)
        sz = int(f[3])
        ok = pt.status(names, pc.fold(r)) == 1.0
        x = d[PACKAGE[f[1]]]
        x[0] += 1; x[1] += sz
        if ok:
            x[2] += 1; x[3] += sz
    return d


def pct(a, b):
    return 100.0 * a / b if b else 0.0


def svg(d, total, today):
    pk = sorted(d, key=lambda p: -d[p][1])
    W, X0, BW, RH, TOP = 760, 150, 420, 30, 58
    H = TOP + RH * len(pk) + 14
    o = ['<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 %d %d" width="%d" height="%d" font-family="Segoe UI,Helvetica,Arial,sans-serif">' % (W, H, W, H),
         '<style>.t{fill:#1f2328}.m{fill:#59636e}.tr{fill:#d1d9e0}.b{fill:#1a7f37}.bg{fill:#fff}'
         '@media (prefers-color-scheme:dark){.t{fill:#e6edf3}.m{fill:#9198a1}.tr{fill:#3d444d}.b{fill:#3fb950}.bg{fill:#0d1117}}</style>',
         '<rect class="bg" width="100%" height="100%" rx="6"/>',
         '<text class="t" x="12" y="26" font-size="16" font-weight="700">Rendering engine decompilation: %.1f%% understood</text>' % total,
         '<text class="m" x="12" y="45" font-size="11.5">Share of code bytes with a real function name, per renderer package. %s</text>' % today]
    for i, p in enumerate(pk):
        y = TOP + i * RH
        n, b, nn, nb = d[p]
        v = pct(nb, b)
        o.append('<text class="t" x="12" y="%d" font-size="13">%s %s</text>' % (y + 15, NUM[p], p))
        o.append('<rect class="tr" x="%d" y="%d" width="%d" height="14" rx="3"/>' % (X0, y + 3, BW))
        if v > 0:
            o.append('<rect class="b" x="%d" y="%d" width="%.1f" height="14" rx="3"/>' % (X0, y + 3, max(3, BW * v / 100)))
        o.append('<text class="t" x="%d" y="%d" font-size="12.5" font-weight="600">%.1f%%</text>' % (X0 + BW + 10, y + 15, v))
        o.append('<text class="m" x="%d" y="%d" font-size="11.5">%s fn, %.0f KB</text>' % (X0 + BW + 62, y + 15, format(n, ','), b / 1024))
    o.append('</svg>')
    return '\n'.join(o)


def badge(v):
    col = '#e05d44' if v < 25 else '#dfb317' if v < 50 else '#97ca00' if v < 75 else '#4c1'
    lab, val = 'renderer decompiled', '%d%%' % round(v)
    lw, vw = 6 * len(lab) + 12, 7 * len(val) + 12
    return ('<svg xmlns="http://www.w3.org/2000/svg" width="%d" height="20" role="img" aria-label="%s: %s">'
            '<rect width="%d" height="20" rx="3" fill="#555"/><rect x="%d" width="%d" height="20" rx="3" fill="%s"/>'
            '<rect x="%d" width="4" height="20" fill="%s"/>'
            '<g fill="#fff" font-family="Verdana,DejaVu Sans,sans-serif" font-size="11" text-anchor="middle">'
            '<text x="%d" y="14">%s</text><text x="%d" y="14">%s</text></g></svg>') % (
        lw + vw, lab, val, lw, lw, vw, col, lw, col, lw // 2, lab, lw + vw // 2, val)


def table(d):
    r = ['| Package | Functions | Code | Understood (bytes) | Note | Capture-checked |', '|---|--:|--:|--:|:-:|:-:|']
    for p in sorted(d, key=lambda p: NUM[p]):
        n, b, nn, nb = d[p]
        r.append('| %s %s | %s | %.0f KB | %.1f%% | %s | %s |' % (NUM[p], p, format(n, ','), b / 1024, pct(nb, b),
                 'yes' if note_written(p) else 'no', 'yes' if p in CHECKED else 'no'))
    t = [sum(d[p][i] for p in d) for i in range(4)]
    r.append('| **Total** | **%s** | **%.0f KB** | **%.1f%%** | | |' % (format(t[0], ','), t[1] / 1024, pct(t[3], t[1])))
    return '\n'.join(r) + '\n'


def main():
    d = compute()
    t = [sum(d[p][i] for p in d) for i in range(4)]
    total = pct(t[3], t[1])
    print('overall %.2f%% by bytes (%d/%d fn named)' % (total, t[2], t[0]))
    for p in sorted(d, key=lambda p: NUM[p]):
        print('%s %-12s %6d fn %8d B  %.2f%%' % (NUM[p], p, d[p][0], d[p][1], pct(d[p][3], d[p][1])))
    if '--check' in sys.argv:
        return
    today = datetime.date.today().isoformat()
    os.makedirs(OUT, exist_ok=True)
    open(os.path.join(OUT, 'renderer_understood.svg'), 'w', encoding='utf-8', newline='\n').write(svg(d, total, today))
    open(os.path.join(OUT, 'renderer_badge.svg'), 'w', encoding='utf-8', newline='\n').write(badge(total))
    open(os.path.join(OUT, 'renderer_table.md'), 'w', encoding='utf-8', newline='\n').write(table(d))


if __name__ == '__main__':
    main()
