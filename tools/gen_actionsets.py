"""Build mod/Nioh3Training/Nioh3Training_actionsets.tsv: one line per archive_01 action file = its action object hash set (rows +0xF0), for the runtime
auto-bind (training: autobind.h). Names come from notes/boss-action-bind-auto.tsv (file -> character), '?' when the file has no setting DB owner.
  py -I tools/gen_actionsets.py [game dir]            cols: file(62 hex)  char  rows  id,id,...(hex)"""
import csv, os, struct, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import lfm_extract as L

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..')
GAME = sys.argv[1] if len(sys.argv) > 1 else 'C:/Program Files (x86)/Steam/steamapps/common/Nioh3'
REC = 0x150
paths = {int(f[1]): f[5].lstrip('/') for f in (ln.split('\t') for ln in open(os.path.join(ROOT, 'data', 'lfm_list.tsv'), encoding='utf-8')) if f[0] == '01'}
owner = {}
for r in csv.DictReader(open(os.path.join(ROOT, 'notes', 'boss-action-bind-auto.tsv'), encoding='utf-8'), delimiter='\t'):
    if r['char_name'] and r['file'] not in owner:
        owner[r['file']] = r['char_name'].split(',')[0]
d = open(os.path.join(GAME, 'archive', 'archive_01.bin'), 'rb').read()
out = []
for i, e in enumerate(L.entries(d)):
    if not 2000 < e[1] < 40_000_000:
        continue
    try:
        b = L.blob(d, e)
        if struct.unpack_from('<I', b, 0)[0] != 16:
            continue
        o, s = struct.unpack_from('<II', b, 12)
        if not s or s % REC or s // REC <= 3:
            continue
        ids = sorted({struct.unpack_from('<I', b, o + r * REC + 0xF0)[0] for r in range(s // REC)} - {0, 0xFFFFFFFF})  # action object name hashes (row +0xF0)
    except Exception:
        continue
    p = paths.get(i)
    if p:
        out.append('%s\t%s\t%d\t%s' % (p.replace('/', ''), owner.get(p, '?'), s // REC, ','.join('%X' % x for x in ids)))
dst = os.path.join(ROOT, 'mod', 'Nioh3Training', 'Nioh3Training_actionsets.tsv')
open(dst, 'w', newline='\n').write('# Nioh3Training action sets (tools/gen_actionsets.py): file, character, rows, action object hashes (hex)\n' + '\n'.join(out) + '\n')
print(len(out), 'files', os.path.getsize(dst), 'bytes')
