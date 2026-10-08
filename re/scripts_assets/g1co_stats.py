"""G1CO (collision) triangle/vertex counts vs the render mesh. usage: py -I g1co_stats.py col_dir model_dir col_pairs.tsv audit.tsv
HVB_ header (+0x10): magic 'HVB_', ver, 0, size; then u32 nNodes, nodeOff, nTris, triOff, nVerts, vertOff (offsets rel. +0x10); verts = float4."""
import os, struct, sys, csv
cd, md, pairs, aud = sys.argv[1:5]
A = {r['file'][2:10]: r for r in csv.DictReader(open(aud), delimiter='\t')}
rows = []
for ln in open(pairs):
    c, m, n, _ = ln.rstrip('\n').split('\t')
    fn = os.path.join(cd, '0x%s.g1co' % c)
    if not os.path.exists(fn): continue
    b = open(fn, 'rb').read(); o = b.find(b'HVB_')
    if o < 0: rows.append((c, m, int(n), -1, -1, 0)); continue
    nn, no, nt, to, nv, vo = struct.unpack_from('<6I', b, o + 16)
    ok = (o + vo + 16 * nv) <= len(b)
    r = A.get(m, {}); rt = int(r.get('tris') or 0)
    rows.append((c, m, int(n), nt, nv, rt, ok))
rows.sort(key=lambda x: -x[2] * max(x[3], 0))
tc = sum(x[2] * max(x[3], 0) for x in rows); tr = sum(x[2] * x[5] for x in rows)
print(f'# collision files={len(rows)} layout_ok={sum(1 for x in rows if x[-1] is True)} inst-weighted col tris={tc} render tris(LOD all levels)={tr}')
print('col\tmodel\tinst\tcol_tris\tcol_verts\trender_tris\tratio')
for x in rows[:15]:
    print('\t'.join(map(str, x[:6])) + '\t%.3f' % (x[3] / x[5] if x[5] else float('nan')))
