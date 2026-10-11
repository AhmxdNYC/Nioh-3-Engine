"""Merge re\\symbols\\incoming\\*.tsv (one file per contributor, same columns as symbols.tsv) into symbols.tsv.

    py re\\symbols\\merge_incoming.py [--dry]

Rules:
- New RVA (for struct_field: new Struct::Field name) -> row appended.
- RVA already present -> the existing name is kept; the incoming name/description is appended to the existing
  description as "also (<file>): <name> - <desc>" unless it is already mentioned. Confidence is raised if higher.
- Rows with a bad kind / RVA / name are reported and skipped.
- A name collision with a different RVA gets the incoming name suffixed with _<rva>.
Processed files are moved to incoming\\merged\\<timestamp>_<file>. Run build_exports.py afterwards (done automatically).
"""
import csv, os, re, shutil, subprocess, sys, time

HERE = os.path.dirname(os.path.abspath(__file__))
MAIN = os.path.join(HERE, 'symbols.tsv')
INC = os.path.join(HERE, 'incoming')
KINDS = {'function', 'label', 'global', 'data', 'struct_field', 'vtable', 'string_table'}
CONF = {'low': 0, 'medium': 1, 'high': 2}
COLS = ['rva', 'kind', 'name', 'signature', 'description', 'confidence', 'source']
dry = '--dry' in sys.argv

lines = open(MAIN, encoding='utf-8').read().splitlines()
head = [l for l in lines if l.startswith('#')]
body = [l for l in lines if l and not l.startswith('#')]
rows = list(csv.DictReader(body, delimiter='\t', quoting=csv.QUOTE_NONE))
key = lambda r: ('F:' + r['name']) if r['kind'] == 'struct_field' else ('A:' + r['rva'].lower())
index = {key(r): r for r in rows}
names = {r['name'] for r in rows}
added = updated = skipped = 0
files = [f for f in sorted(os.listdir(INC)) if f.endswith('.tsv')] if os.path.isdir(INC) else []
for fn in files:
    inc = [l for l in open(os.path.join(INC, fn), encoding='utf-8').read().splitlines() if l and not l.startswith('#')]
    for r in csv.DictReader(inc, delimiter='\t', quoting=csv.QUOTE_NONE):
        r = {c: (r.get(c) or '').strip().replace('\t', ' ') for c in COLS}
        if r['kind'] not in KINDS or not re.fullmatch(r'0x[0-9a-f]+', r['rva'].lower()) or not re.fullmatch(r'[A-Za-z_][A-Za-z0-9_:]*', r['name']):
            print('skip (invalid) %s: %s' % (fn, r)); skipped += 1; continue
        r['rva'] = r['rva'].lower()
        r['confidence'] = r['confidence'] if r['confidence'] in CONF else 'low'
        k = key(r)
        if k in index:
            e = index[k]
            note = 'also (%s): %s - %s' % (fn, r['name'], r['description'])
            if r['name'] != e['name'] and r['name'] not in e['description']:
                e['description'] = (e['description'] + '; ' + note).strip('; ')
                updated += 1
            if CONF[r['confidence']] > CONF.get(e['confidence'], 0):
                e['confidence'] = r['confidence']
            continue
        if r['name'] in names:
            r['name'] = '%s_%s' % (r['name'], r['rva'][2:])
        if not r['source']:
            r['source'] = 'incoming/' + fn
        rows.append(r); index[k] = r; names.add(r['name']); added += 1
print('added %d, updated %d, skipped %d from %d file(s)' % (added, updated, skipped, len(files)))
if dry or not files:
    sys.exit(0)
rows.sort(key=lambda r: (r['kind'] == 'struct_field', r['name'].split('::')[0] if r['kind'] == 'struct_field' else '', int(r['rva'], 16)))
with open(MAIN, 'w', encoding='utf-8', newline='') as f:
    for h in head:
        f.write(h + '\n')
    f.write('\t'.join(COLS) + '\n')
    for r in rows:
        f.write('\t'.join(r[c] for c in COLS) + '\n')
os.makedirs(os.path.join(INC, 'merged'), exist_ok=True)
for fn in files:
    shutil.move(os.path.join(INC, fn), os.path.join(INC, 'merged', time.strftime('%Y%m%d-%H%M%S_') + fn))
subprocess.run([sys.executable, '-I', os.path.join(HERE, 'build_exports.py')])
