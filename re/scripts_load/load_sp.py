# load_sp.py - SP profiler rows inside the 58A load window vs the 4 minutes after it, per thread group, with names.
#   py -I re\scripts_load\load_sp.py <modlog.log> [topN=12]
# Shares are wall time (blocked threads are sampled too; see notes\cpu-hotspots-ally.md §1).
import re, sys, bisect, os, collections
LOG = sys.argv[1]; N = int(sys.argv[2]) if len(sys.argv) > 2 else 12
SYM = os.path.join(os.path.dirname(__file__), '..', 'symbols', 'symbols.tsv')
names = {}
for l in open(SYM, encoding='utf-8'):
    p = l.split('\t')
    if len(p) > 2 and p[1] == 'function':
        try: names[int(p[0], 16)] = p[2]
        except ValueError: pass
TS = re.compile(r'^\[(\d\d):(\d\d):(\d\d)\.(\d\d\d)\]\s(.*)$')
ROW = re.compile(r'^SP (\w+)\s+([\d.]+) %\s+(.*)$')
win = None; ph = None; agg = {'load': collections.defaultdict(float), 'after': collections.defaultdict(float)}
nrep = collections.Counter(); tclear = None
for line in open(LOG, encoding='utf-8', errors='replace'):
    m = TS.match(line)
    if not m: continue
    t = int(m[1])*3600 + int(m[2])*60 + int(m[3]); s = m[5]
    if 'engine stop flags' in s:
        if '58A' in s and ph is None: ph = 'load'
        elif s.rstrip().endswith('none') and ph == 'load': ph = 'after'; tclear = t
        continue
    if ph == 'after' and t - tclear > 240: break
    if ph is None: continue
    if s.startswith('SP ') and 'samples in' in s:
        nrep[(ph, s.split(':')[0][3:].strip())] += 1; continue
    r = ROW.match(s)
    if r: agg[ph][(r[1], r[3])] += float(r[2])
reps = {}
for (ph, g), n in nrep.items(): reps[ph] = max(reps.get(ph, 0), n)
def nm(txt):
    m = re.search(r'fn 0x([0-9A-F]+)', txt)
    return names.get(int(m[1], 16), '') if m else ''
groups = sorted({k[0] for k in agg['load']})
for g in groups:
    print(f'== {g}  (load reports {reps.get("load",0)}, after {reps.get("after",0)}; avg %% per report)')
    rows = [(v / max(reps.get('load', 1), 1), k[1]) for k, v in agg['load'].items() if k[0] == g]
    for v, txt in sorted(rows, reverse=True)[:N]:
        a = agg['after'].get((g, txt), 0) / max(reps.get('after', 1), 1)
        print(f'  {v:5.1f}  after {a:5.1f}  {txt[:52]:52} {nm(txt)}')
