"""Extract every archive_01 action file bound in notes/boss-action-files-22709.csv (+ Ippon-Datara) into data/extract/lfm/archive_01.
  py -I tools/lfm_extract_all.py [game archive dir]      index -> path comes from data/lfm_list.tsv (archive 01 rows)"""
import csv, os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import lfm_extract as L

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..')
GAME = sys.argv[1] if len(sys.argv) > 1 else 'C:/Program Files (x86)/Steam/steamapps/common/Nioh3/archive'
d = open(os.path.join(GAME, 'archive_01.bin'), 'rb').read()
es = L.entries(d)
idx = {}
for ln in open(os.path.join(ROOT, 'data', 'lfm_list.tsv'), encoding='utf-8'):
    f = ln.split('\t')
    if f[0] == '01':
        idx[f[5].lstrip('/').upper()] = int(f[1])
want = {r['archive_01_file'].upper() for r in csv.DictReader(open(os.path.join(ROOT, 'notes', 'boss-action-files-22709.csv'))) if r['archive_01_file']}
want.add('FD/D80A7992FD2F3EB4AE582BAE0A06A071449E2F89C91978B13471449D28C062')
n = 0
for w in sorted(want):
    if w not in idx:
        print('no index for', w); continue
    p = os.path.join(ROOT, 'data', 'extract', 'lfm', 'archive_01', *w.split('/'))
    os.makedirs(os.path.dirname(p), exist_ok=True)
    open(p, 'wb').write(L.blob(d, es[idx[w]])); n += 1
print('extracted', n, 'of', len(want))
