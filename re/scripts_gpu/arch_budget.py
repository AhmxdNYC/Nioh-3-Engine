#!/usr/bin/env python3
"""arch_budget.py - GP2 per-PSO rows rolled up into render-architecture families (notes/render-architecture.md).

  py -I re\\scripts_gpu\\arch_budget.py LOG [--min-draws 1600] [--rows]     families over the heavy (gameplay) windows
  py -I re\\scripts_gpu\\arch_budget.py LOG --min-draws 0 --max-draws 1599   the light windows instead

Why: the tour mixes spawn points (real levels, 25-34 ms GPU on pc2) with warp points (often outside the level,
8-14 ms); ally_budget's mean over both (23.2 ms) is neither. GP2 rows are billed by family here, async rows are kept
apart (they overlap graphics), and GP1 marker means are printed for the same windows.
Stdlib only; PSO names come from gp2_digest.resolve (hash index cache).
"""
import argparse, os, re, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from ally_budget import windows  # noqa: E402

# (family, regex on "names | marker | desc") - first match wins
FAMILIES = [
    ('copy/resolve/clear (+absorbed draws)', r'\(copy / resolve / clear\)'),
    ('FSR3 upscaler', r'FFX_FSR3|FSR3|fsr/upscale'),
    ('meshlet LOD/culling/HiZ', r'MeshletResolveLOD|\| ?\d+ ?$|MeshletInstanceCulling|MeshletPreculling|MeshletCheckRequestLoad|'
                                r'MeshletRequestLoad|GenerateSceneHiZ|GPUCulling|MshLod|\bMIC\b|\bMPC\b|MCLRC|MGLRC|GHDM|GFM-Hiz|\bCBC\b|\bCLC\b'),
    ('sky/clouds (VCLOUD)', r'VCLOUD|DrawSky'),
    ('volumetric fog (VFSCAT/SSFOG)', r'VFSCAT|SSFOG'),
    ('shadow maps (SM/VSM/ESM)', r'Cascade|VirtualShadowmap|Clipmap|Batch Tiles|Tile #|\bSM-|SMTREE2|DEPTH-|\bESM\b'),
    ('deferred lighting/shadow resolve (WDO/PB2DS/MRLR)', r'WDO|PB2DS|PB2DUS|PB2MRLR|SDFOCC'),
    ('GI/reflection gathers (RefProbe/SSR/AO)', r'RefProbe|PBRLR|HBAO|\bGFM\b|GFM-D'),
    ('water/ocean', r'UMTV|PB2UMAH|SRIVER|\bPFB\b|\bIGP\b|BWVM|Ultramarine|CTS|\bWET\b|App_CopyMRLR'),
    ('effects/particles/decals', r'G1E|PB2RIPPLE|PB2DECAL|TOONEDGE|EffectMerge'),
    ('G-buffer geometry (PB2/STD/terrain/trees)', r'PB2|STD-|G1M|TREE2|PB2LS|\bGVD\b'),
    ('post (bloom/MB/DOF/compose/SSS)', r'post VS|bloom|motion blur|compose|ARPSSS|SSFade|TAA|DOF'),
    ('UI/output', r'AMBSL|ExeUI|VideoOut|CustomHighlight|ExeCopy'),
]


def family(text):
    for name, rx in FAMILIES:
        if re.search(rx, text):
            return name
    return 'unnamed'


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('log')
    ap.add_argument('--min-draws', type=int, default=1600)
    ap.add_argument('--max-draws', type=int, default=10 ** 9)
    ap.add_argument('--rows', action='store_true', help='also print every row with its family')
    a = ap.parse_args()
    from gp2_digest import load_index, resolve
    I = load_index(False)
    w1, w2 = windows(a.log)
    g = [w for w in w2 if w['on'] and a.min_draws <= w['draws'] <= a.max_draws]
    if not g:
        sys.exit('no GP2 windows in range')
    n = len(g)
    span = sum(w['span'] for w in g) / n
    cov = sum(w['cov'] for w in g) / n
    asy = sum(w['asy'] for w in g) / n
    fam, rows = {}, {}
    for w in g:
        for (desc, mk), (ms, dr) in w['rows'].items():
            r = rows.setdefault((desc, mk), [0.0, 0.0])
            r[0] += ms / n; r[1] += dr / n
    for (desc, mk), (ms, dr) in rows.items():
        _, _, names, _, _ = resolve(desc, I)
        text = '%s | %s | %s' % (' / '.join(names), desc, mk)
        f = family(text)
        is_async = '(async compute)' in desc
        key = (f + (' [async]' if is_async else ''))
        s = fam.setdefault(key, [0.0, 0.0, 0])
        s[0] += ms; s[1] += dr; s[2] += 1
        rows[(desc, mk)] = (ms, dr, key, ' / '.join(dict.fromkeys(names)))
    top100 = sum(v[0] for k, v in fam.items() if '[async]' not in k)
    print('%d GP2 windows with %d..%s draws+dispatches/frame: mean span %.2f ms, covered %.2f, async %.2f; '
          'graphics-queue rows in the top-100 lists: %.2f ms (rest = rows below the top-100 cut + uncovered gaps)'
          % (n, a.min_draws, a.max_draws if a.max_draws < 10 ** 9 else 'inf', span, cov, asy, top100))
    print('family|ms/frame|%span|draws/frame|rows')
    for k, (ms, dr, nr) in sorted(fam.items(), key=lambda kv: -kv[1][0]):
        print('%s|%.2f|%.1f|%.0f|%d' % (k, ms, 100 * ms / span, dr, nr))
    if a.rows:
        print('\nrow|ms|draws|family|marker|names|desc')
        for (desc, mk), (ms, dr, key, names) in sorted(rows.items(), key=lambda kv: -kv[1][0]):
            print('|%.3f|%.1f|%s|%s|%s|%s' % (ms, dr, key, mk, names or '-', desc[:90]))
    # GP1 markers for comparable windows (GP1 windows have no draw count; pair them by span)
    h1 = [w for w in w1 if abs(w['span'] - span) < 0.35 * span]
    if h1:
        acc = {}
        for w in h1:
            for k, (inc, slf, pf) in w['rows'].items():
                s = acc.setdefault(k, [0.0, 0.0, 0.0])
                s[0] += inc / len(h1); s[1] += slf / len(h1); s[2] += pf / len(h1)
        print('\nGP1 markers, %d windows with span within 35 %% of %.1f ms (top 25 by incl):' % (len(h1), span))
        for k, (inc, slf, pf) in sorted(acc.items(), key=lambda kv: -kv[1][0])[:25]:
            print('  %-44s incl %6.3f self %6.3f %4.1f/frame' % (k, inc, slf, pf))


if __name__ == '__main__':
    main()
