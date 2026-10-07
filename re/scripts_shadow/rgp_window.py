"""Print RGP events in a time window with group names, collapsing runs of the same PSO+kind.
usage: py -I rgp_window.py <events.csv> <pso_map.csv> t0 t1 [queue]"""
import csv, sys
ev_path, map_path, t0, t1 = sys.argv[1], sys.argv[2], float(sys.argv[3]), float(sys.argv[4])
q = sys.argv[5] if len(sys.argv) > 5 else None
names = {}
for r in csv.DictReader(open(map_path, newline='', encoding='utf-8', errors='replace')):
    sh = r['shaders(stage:dxbc_digest=file/group#index)']
    g = [f"{t.split(':',1)[0][:2]}:{t.split('/',1)[1]}" for t in sh.split() if '=' in t and '/' in t]
    names[r['api_pso_hash'].lower()] = ' '.join(g) if g else '?'
run = None
def flush():
    if run: print(f"{run['q']:5s} t={run['t0']:8.1f}..{run['t1']:8.1f} seg={run['s0']}-{run['s1']} x{run['n']:<4d} {run['k']:20s} est={run['est']:7.1f} tg={run['tg']} {run['name'][:120]}")
for e in csv.DictReader(open(ev_path, newline='')):
    t = float(e['t_us'])
    if t < t0 or t > t1 or (q and e['queue'] != q): continue
    nm = names.get(e['pso'].lower(), 'UNMAPPED' if e['pso'] != 'ffffffffffffffff' else '-')
    key = (e['queue'], e['pso'], e['kind'])
    est = float(e['est_us'] or 0)
    if run and run['key'] == key:
        run['n'] += 1; run['t1'] = t; run['s1'] = e['seg']; run['est'] += est
    else:
        flush(); run = {'key': key, 'q': e['queue'], 't0': t, 't1': t, 's0': e['seg'], 's1': e['seg'], 'n': 1, 'k': e['kind'], 'est': est, 'tg': e['threadgroup'], 'name': nm + ' ' + e['pso'][:8]}
flush()
