"""Draw/dispatch counts and est GPU us per shader family (vertex/compute group prefix), direct vs indirect,
plus meshlet (Mshlt) vs non-meshlet split. usage: py -I rgp_families.py <events.csv> <pso_map.csv>"""
import csv, sys, re, collections
ev, mp = sys.argv[1], sys.argv[2]
names = {}
for r in csv.DictReader(open(mp, newline='', encoding='utf-8', errors='replace')):
    sh = r['shaders(stage:dxbc_digest=file/group#index)']
    d = {}
    for t in sh.split():
        if '=' in t and '/' in t:
            d[t.split(':', 1)[0]] = t.split('/', 1)[1]
    names[r['api_pso_hash'].lower()] = d
fam = collections.defaultdict(lambda: collections.Counter())
est = collections.defaultdict(float)
for e in csv.DictReader(open(ev, newline='')):
    k = e['kind']
    if k not in ('DrawIndexedInstanced', 'DrawInstanced', 'api30', 'api31', 'Dispatch', 'api32'): continue
    d = names.get(e['pso'].lower())
    if d is None: f = 'UNMAPPED'
    else:
        g = d.get('vertex') or d.get('compute') or '?'
        f = g.split('-')[0].split('#')[0]
        if 'Mshlt' in g: f += '[Mshlt]'
    kk = 'indirect' if k in ('api30', 'api31', 'api32') else ('dispatch' if k == 'Dispatch' else 'direct')
    fam[f][kk] += 1
    try: est[f] += float(e['est_us'] or 0)
    except ValueError: pass
tot = collections.Counter()
print(f"{'family':24s} {'direct':>7s} {'indir':>6s} {'disp':>6s} {'est_us':>8s}")
for f, c in sorted(fam.items(), key=lambda kv: -sum(kv[1].values())):
    tot.update(c)
    print(f"{f:24s} {c['direct']:7d} {c['indirect']:6d} {c['dispatch']:6d} {est[f]:8.1f}")
print('TOTAL', dict(tot))
