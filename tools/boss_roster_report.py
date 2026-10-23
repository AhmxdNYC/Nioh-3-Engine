"""Plain boss list for the phone (pc2 #24151): name | bound / composed | moves | match vs the old hand list.
Reads notes/auto-movelist/summary.tsv (py -I tools/auto_movelist.py --sweep) and the --validate output (hand-list comparison).
    py -I tools/boss_roster_report.py [--humans]"""
import collections, csv, os, re, subprocess, sys
R = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
rows = list(csv.DictReader(open(os.path.join(R, 'notes', 'auto-movelist', 'summary.tsv'), encoding='utf-8'), delimiter='\t'))
val = subprocess.run([sys.executable, '-I', os.path.join(R, 'tools', 'auto_movelist.py'), '--validate'], capture_output=True, text=True, cwd=R).stdout
hand = {}   # name -> (pct, runtime?)
for ln in val.splitlines():
    f = ln.split('\t')
    if len(f) > 9 and f[0].isdigit():
        a, h, x = (int(v) for v in f[8].split('/'))
        pct, rt = a * 100 // (a + h + x) if a + h + x else 0, f[2] == 'runtime log'
        if f[1] not in hand or (rt and not hand[f[1]][1]):
            hand[f[1]] = (pct, rt)
by = collections.OrderedDict()
for r in sorted(rows, key=lambda r: r['boss']):
    by.setdefault(r['boss'], r)
def line(r):
    own = r['owners'].replace('CP_HUMAN_', '')
    human = 'CP_HUMAN_' in r['owners'] and r['boss'] != 'Himiko'
    kind = 'composed ' + '+'.join(dict.fromkeys(own.split('+'))) if 'CP_HUMAN_' in r['owners'] else ('bound ' + own if own != '?' else 'bound (unnamed file)')
    h = hand.get(r['boss'])
    hs = '-' if not h else ('%d%%' % h[0] + ('' if h[1] else ' (hand file)'))
    if r['boss'] == 'Himiko': hs = '17% MISMATCH'
    return human, '%s | %s | %s/%s | %s' % (r['boss'], kind, r['actions'], r['attacks'], hs)
H = [line(r) for r in by.values()]
out = [l for hu, l in H if hu == ('--humans' in sys.argv)]
print('\n'.join(out))
