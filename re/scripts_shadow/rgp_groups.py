"""Join an RGP events.csv with pso_map.csv (shader group names) and summarise by group.
usage: py -I rgp_groups.py <events.csv> <pso_map.csv> [regex-filter]
Prints: per-group event counts by kind, est_us sum, first/last t; then (filtered) an ordered timeline.
"""
import csv, sys, re, collections
ev_path, map_path = sys.argv[1], sys.argv[2]
flt = re.compile(sys.argv[3]) if len(sys.argv) > 3 else None
names = {}
for r in csv.DictReader(open(map_path, newline='', encoding='utf-8', errors='replace')):
    sh = r['shaders(stage:dxbc_digest=file/group#index)']
    grp = []
    for tok in sh.split():
        if '=' in tok and '/' in tok:
            st = tok.split(':', 1)[0]
            g = tok.split('/', 1)[1]
            grp.append(f"{st[:2]}:{g}")
    names[r['api_pso_hash'].lower()] = ' '.join(grp) if grp else (r['api_name'] or '?')
evs = list(csv.DictReader(open(ev_path, newline='')))
def gname(pso):
    return names.get(pso.lower(), 'UNMAPPED' if pso != 'ffffffffffffffff' else '-')
agg = collections.OrderedDict()
for e in evs:
    n = gname(e['pso'])
    key = n
    a = agg.setdefault(key, {'kinds': collections.Counter(), 'est': 0.0, 't0': float(e['t_us']), 't1': 0.0, 'segs': set(), 'q': collections.Counter()})
    a['kinds'][e['kind']] += 1
    try: a['est'] += float(e['est_us'] or 0)
    except ValueError: pass
    a['t1'] = float(e['t_us']); a['segs'].add(int(e['seg'])); a['q'][e['queue']] += 1
tot = collections.Counter(e['kind'] for e in evs)
print('TOTAL events by kind:', dict(tot))
unm = sum(1 for e in evs if gname(e['pso']) == 'UNMAPPED')
print('unmapped events:', unm, 'of', len(evs))
rows = sorted(agg.items(), key=lambda kv: -kv[1]['est'])
for k, a in rows:
    if flt and not flt.search(k): continue
    print(f"{a['est']:9.1f}us  n={sum(a['kinds'].values()):4d}  t={a['t0']:8.1f}..{a['t1']:8.1f}  segs={len(a['segs']):3d}  {dict(a['kinds'])}  {k[:150]}")
if flt:
    print('\n-- ordered timeline of filtered events --')
    for e in evs:
        n = gname(e['pso'])
        if flt.search(n):
            print(f"{e['queue']:5s} t={float(e['t_us']):8.1f} seg={e['seg']:>4s} {e['kind']:22s} tg={e['threadgroup']:10s} waves={e['approx_waves']:>6s} est={e['est_us']:>7s} {n[:110]}")
