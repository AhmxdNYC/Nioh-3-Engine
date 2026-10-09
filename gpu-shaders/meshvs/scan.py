r"""scan.py - which VS digests (hot list) match the MSV pattern; extracts one record per digest into work/."""
import csv, os, subprocess, sys
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path[:0] = [HERE, os.path.join(HERE, '..', 'materials'), os.path.join(HERE, '..')]
import idrk, dxcom, scalarize  # noqa: E402
SD = r"C:\Program Files (x86)\Steam\steamapps\common\Nioh3\shader_dx12"
rows = list(csv.DictReader(open(os.path.join(HERE, '..', 'dxil_index.csv'))))
if sys.argv[1] == '--all-mshlt':
    seen = []
    for r in rows:
        if r['kind'] == 'vs' and 'Mshlt' in r['group'] and r['container_digest'][:8] not in seen:
            seen.append(r['container_digest'][:8])
    want = seen
else:
    want = sys.argv[1].split(',')
for w in want:
    r = next((r for r in rows if r['container_digest'].startswith(w)), None)
    if not r:
        print(w, 'not in index'); continue
    n = sum(1 for x in rows if x['container_digest'] == r['container_digest'])
    base = os.path.join(HERE, 'work', 'all' if len(want) > 50 else '', w)
    if not os.path.exists(base + '.ll'):
        p = idrk.unpack(open(os.path.join(SD, r['file']), 'rb').read())
        o, s = int(r['dxbc_off']), int(r['dxbc_size'])
        open(base + '.dxbc', 'wb').write(p[o:o + s])
        open(base + '.ll', 'w', encoding='utf-8').write(dxcom.dumpbin(base + '.dxbc'))
        if len(want) > 50:
            pass
    t = open(base + '.ll', encoding='utf-8').read()
    try:
        _, info = scalarize.rewrite(t, True)
        res = 'MATCH phis=%d hoisted=%d' % (info['phis'], info['hoisted'])
    except Exception as e:  # noqa: BLE001
        res = 'no: ' + str(e)[:60]
    print('%s %-34s files=%3d %s' % (w, r['group'][:34], n, res))
