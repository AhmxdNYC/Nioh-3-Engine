import os, sys, struct, collections, re
root = sys.argv[1]; out = sys.argv[2]
files = []
for dp, dn, fn in os.walk(root):
    for f in fn:
        files.append(os.path.join(dp, f))
def parse(b):
    o = 0; r = []
    while o + 8 <= len(b):
        i, n = struct.unpack_from('<II', b, o); o += 8
        if n > 200000 or o + 2 * n > len(b):
            return None
        s = b[o:o + 2 * n].decode('utf-16-le', 'replace').rstrip('\x00'); o += 2 * n
        r.append((i, s))
    return r if o == len(b) else None
eng = re.compile(r'\b(the|of|and|to|your|you)\b')
rows = []; bad = 0
for p in files:
    b = open(p, 'rb').read(); r = parse(b)
    if r is None:
        bad += 1; continue
    txt = ' '.join(s for _, s in r)
    score = len(eng.findall(txt)) / max(1, len(txt.split()))
    asc = sum(1 for c in txt if ord(c) < 128) / max(1, len(txt))
    rows.append((p, b[:4].hex(), len(r), score, asc, r))
print('bad', bad, 'ok', len(rows))
by = collections.defaultdict(list)
for row in rows:
    by[row[1]].append(row)
BS = chr(92)
with open(out, 'w', encoding='utf-8') as f:
    for k, v in by.items():
        v.sort(key=lambda x: -x[3] * x[4])
        best = v[0]
        rel = os.path.relpath(best[0], root).replace(BS, '/')
        for i, s in best[5]:
            s = s.replace('\n', BS + 'n').replace('\t', ' ').replace('\r', '')
            f.write('%s\t%08x\t%s\t%s\n' % (k, i, rel, s))
        print(k, len(v), best[2], round(best[3], 3), rel)
