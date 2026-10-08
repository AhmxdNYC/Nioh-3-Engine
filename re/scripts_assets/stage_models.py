"""Resolve a stage's placed models: placement REFs -> WorldPQ template -> Displayset::Model -> ModelData KTID.
usage: py -I stage_models.py <kod_txt_dir> <rdb_list.tsv> placement1.txt [placement2.txt ...]
Inputs are n3kod.py text dumps (templates 9d683d24.txt and displaysets b157c8a0.txt must be in kod_txt_dir)."""
import sys, re, os, collections
d, rdbl = sys.argv[1], sys.argv[2]
def objs(path):
    cur = None; o = {}
    for ln in open(path, encoding='utf-8', errors='replace'):
        if ln.startswith('== obj'):
            p = ln.split(); cur = {'_type': p[3], '_ref': p[4] if len(p) > 4 else ''}; o[p[2]] = cur
        elif cur is not None and ln.startswith('   '):
            k, _, v = ln.strip().partition(' '); cur[k] = v.strip()
    return o
tmpl = objs(os.path.join(d, '9d683d24.txt')); disp = objs(os.path.join(d, 'b157c8a0.txt'))
types = {}
for ln in open(rdbl, encoding='utf-8'):
    c = ln.split('\t')
    if len(c) > 4: types[c[1]] = (c[2], c[3])
cnt = collections.Counter(); miss = collections.Counter()
for pf in sys.argv[3:]:
    for k, o in objs(pf).items():
        if o['_type'] != 'REF': continue
        t = tmpl.get(o['_ref'].split('/')[1].lstrip('0') .rjust(8, '0'))
        if not t: miss['tmpl'] += 1; continue
        ds = disp.get(t.get('DisplaysetObjectNameHash', '0x0')[2:].rjust(8, '0'))
        if not ds or 'KTGLModelDataResourceHash' not in ds: miss['disp'] += 1; continue
        cnt[ds['KTGLModelDataResourceHash'][2:].rjust(8, '0')] += 1
print('# instances', sum(cnt.values()), 'models', len(cnt), 'unresolved', dict(miss))
for m, n in cnt.most_common():
    ty, sz = types.get(m, ('?', '0'))
    print(f'{m}\t{n}\t{ty}\t{sz}')
