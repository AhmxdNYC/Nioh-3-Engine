"""Summarise g1m_audit.py output. usage: py -I audit_summary.py audit.tsv"""
import sys, csv, collections
R = list(csv.DictReader(open(sys.argv[1]), delimiter='\t'))
f = lambda r, k: float(r[k]) if r[k] not in ('', 'None') else 0.0
for kind in ('meshlet', 'indexed'):
    rs = [r for r in R if r['kind'] == kind]
    for w in ('models', 'instances'):
        W = (lambda r: 1) if w == 'models' else (lambda r: int(r['inst']))
        n = sum(W(r) for r in rs)
        if not n: continue
        tris = sum(W(r) * f(r, 'tris') for r in rs); vb = sum(W(r) * f(r, 'vb_verts') for r in rs)
        dup = sum(W(r) * f(r, 'dup_verts') for r in rs)
        line = f'{kind:8s} by {w:9s}: n={n} tris={tris:.0f} vb_verts={vb:.0f} exact-dup verts={dup:.0f} ({100*dup/max(vb,1):.2f}%)'
        if kind == 'meshlet':
            ml = sum(W(r) * f(r, 'meshlets') for r in rs); mn = sum(W(r) * f(r, 'min_meshlets') for r in rs)
            refs = sum(W(r) * f(r, 'mlv_refs') for r in rs); mg = sum(W(r) * f(r, 'min_meshlets_grp') for r in rs)
            wv = sum(W(r) * f(r, 'waves') for r in rs); wu = sum(W(r) * f(r, 'waves_used') for r in rs)
            l0 = sum(W(r) * f(r, 'lod0_meshlets') for r in rs); l0t = sum(W(r) * f(r, 'lod0_fill') * f(r, 'lod0_meshlets') for r in rs)
            line += f' waves_used={100*wu/wv:.1f}% lod0_fill={l0t/max(l0,1):.3f}'
            line += f' meshlets={ml:.0f} fill={tris/(128*ml):.3f} min_meshlets={mn:.0f} (lanes -{100*(1-mn/ml):.1f}%) group-preserving={mg:.0f} (lanes -{100*(1-mg/ml):.1f}%) meshlet-verts/tri={refs/tris:.2f}'
        else:
            mi = sum(W(r) * f(r, 'acmr') * f(r, 'tris') for r in rs); mo = sum(W(r) * f(r, 'acmr_opt') * f(r, 'tris') for r in rs)
            line += f' ACMR={mi/max(tris,1):.3f} ACMR_forsyth={mo/max(tris,1):.3f}'
        print(line)
ms = [r for r in R if r['kind'] == 'meshlet']
print('meshlet max_vtx histogram:', collections.Counter(r['max_vtx'] for r in ms).most_common(5))
print('meshlet max_tri histogram:', collections.Counter(r['max_tri'] for r in ms).most_common(5))
print('indexed tris histogram:', collections.Counter(r['tris'] for r in R if r['kind'] == 'indexed').most_common(8))
ix = sorted([r for r in R if r['kind'] == 'indexed' and f(r, 'tris') > 50], key=lambda r: -f(r, 'tris') * max(1, int(r['inst'])))
for r in ix[:8]:
    print('  idx', r['file'], 'inst', r['inst'], 'tris', r['tris'], 'acmr', r['acmr'], '->', r['acmr_opt'], 'dup', r['dup_verts'], '/', r['vb_verts'])
worst = sorted(ms, key=lambda r: -(f(r, 'meshlets') - f(r, 'min_meshlets')) * max(1, int(r['inst'])))
for r in worst[:8]:
    print('  ml', r['file'], 'inst', r['inst'], 'tris', r['tris'], 'meshlets', r['meshlets'], 'min', r['min_meshlets'], 'fill', r['fill'], 'maxv', r['max_vtx'], 'mlv/tri', round(f(r, 'mlv_refs') / f(r, 'tris'), 2))
