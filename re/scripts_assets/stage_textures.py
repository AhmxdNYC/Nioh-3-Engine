"""Futamata textures: placed displaysets -> Textures render state -> bind table (KTIDFileBinary) -> Texture::Static -> TexContext.
usage: py -I stage_textures.py <kod_txt_dir> <rdb_list.tsv> <pkgdir> <outdir> placement.txt...   (writes outdir/tex.tsv)"""
import os, sys, struct, collections
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', 'data', 'tools'))
import n3rdb
d, rdbl, pkg, out = sys.argv[1:5]
def objs(path):
    cur = None; o = {}
    for ln in open(path, encoding='utf-8', errors='replace'):
        if ln.startswith('== obj'):
            p = ln.split(); cur = {'_t': p[3], '_r': p[4] if len(p) > 4 else ''}; o[p[2]] = cur
        elif cur is not None and ln.startswith('   '):
            k, _, v = ln.strip().partition(' '); cur[k] = v.strip()
    return o
h8 = lambda v: v[2:].rjust(8, '0')
T = objs(os.path.join(d, '9d683d24.txt')); D = objs(os.path.join(d, 'b157c8a0.txt'))
info = {}
for ln in open(rdbl, encoding='utf-8'):
    c = ln.split('\t')
    if len(c) > 4: info[c[1]] = (c[2], int(c[3]) if c[3].isdigit() else 0)
rs = collections.Counter()
for pf in sys.argv[5:]:
    for k, o in objs(pf).items():
        if o['_t'] != 'REF': continue
        t = T.get(o['_r'].split('/')[1])
        ds = t and D.get(h8(t.get('DisplaysetObjectNameHash', '0x0')))
        if ds and ds.get('TexturesRenderStateObjectHash', '0') != '0':
            rs[h8(ds['TexturesRenderStateObjectHash'])] += 1
tables = {h8(D[r]['TextureBindTableCSVFileResourceHash']) for r in rs if r in D}
os.makedirs(out, exist_ok=True)
open(os.path.join(out, 'tables.txt'), 'w').write('\n'.join(tables))
sys.argv = ['n3rdb', 'dump', pkg, 'root', os.path.join(out, 'tables.txt'), os.path.join(out, 'tb')]
n3rdb.main()
texobj = collections.Counter()
for fn in os.listdir(os.path.join(out, 'tb')):
    b = open(os.path.join(out, 'tb', fn), 'rb').read()
    for i in range(0, len(b) - 7, 8):
        texobj['%08x' % struct.unpack_from('<I', b, i + 4)[0]] += 1
ctx = collections.Counter()
for h, n in texobj.items():
    o = D.get(h)
    if o and 'KTGLTexContextResourceHash' in o: ctx[h8(o['KTGLTexContextResourceHash'])] += n
with open(os.path.join(out, 'tex.tsv'), 'w') as f:
    for h, n in ctx.most_common():
        ty, sz = info.get(h, ('?', 0)); f.write(f'{h}\t{n}\t{ty}\t{sz}\n')
print('renderstates', len(rs), 'tables', len(tables), 'texture objs', len(texobj), 'texcontexts', len(ctx),
      collections.Counter(info.get(h, ('?',))[0] for h in ctx), 'MB', sum(info.get(h, ('', 0))[1] for h in ctx) / 1e6)
