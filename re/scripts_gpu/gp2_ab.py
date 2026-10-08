#!/usr/bin/env python3
"""gp2_ab.py - on-vs-off GP2 ms per replaced shader. py -I re/scripts_gpu/gp2_ab.py ON.log OFF.log [--win N]
GP2 key = DXBC container digest (header bytes 4..20). Pairs new<->original by container position
(gpu-shaders/backup vs replacement) and post\out\<origkey>.dxbc. Averages the last N ON windows (steady gameplay)."""
import sys, os, re, glob
sys.path.insert(0, os.path.dirname(__file__))
import gp2_digest as g

def pairs():
    P = {}   # new cd -> (orig cd, file, idx)
    for rp in sorted(glob.glob(os.path.join(g.G, 'replacement', '*.file'))):
        fn = os.path.basename(rp); bk = os.path.join(g.G, 'backup', fn)
        if not os.path.exists(bk):
            continue
        o = list(g.containers(bk)); n = list(g.containers(rp))
        if len(o) != len(n):
            continue
        for i, ((ok, oc, ob), (nk, nc, nb)) in enumerate(zip(o, n)):
            if ob != nb:
                P[nc] = (oc, fn, i)
    for p in glob.glob(os.path.join(g.G, 'post', 'out', '*.dxbc')):
        orig = os.path.basename(p).split('.')[0]
        for k, cd, b in g.containers(p):
            P[cd] = (orig, 'post/' + os.path.basename(p), 0)
    return P

def windows(path, n):
    w = [x for x in g.parse([path], False) if x['span'] > 40]   # heavy gameplay windows, not the 16.6 ms menu
    return w[-n:]

def main():
    on, off = sys.argv[1], sys.argv[2]
    n = int(sys.argv[sys.argv.index('--win') + 1]) if '--win' in sys.argv else 6
    P = pairs(); W1, W0 = windows(on, n), windows(off, n)
    def norm(desc):
        return re.sub(r'[0-9a-f]{32}', lambda m: P[m.group(0)][0] if m.group(0) in P else m.group(0), desc)
    def mean(W):
        d = {}
        for w in W:
            for (desc, mk), (ms, dr, iv) in w['rows'].items():
                d.setdefault((norm(desc), mk), []).append(ms)
        return {k: (sum(v) / len(W), len(v)) for k, v in d.items()}
    A, B = mean(W1), mean(W0)
    print('ON windows %d span %.1f cov %.1f async %.2f | OFF windows %d span %.1f cov %.1f async %.2f' % (
        len(W1), sum(w['span'] for w in W1) / len(W1), sum(w['covered'] for w in W1) / len(W1), sum(w['async'] for w in W1) / len(W1),
        len(W0), sum(w['span'] for w in W0) / len(W0), sum(w['covered'] for w in W0) / len(W0), sum(w['async'] for w in W0) / len(W0)))
    keys = set(P_ for P_ in A if re.search('|'.join(x[0] for x in P.values()), P_[0])) | set(k for k in B if re.search('|'.join(x[0] for x in P.values()), k[0]))
    print('file idx | set (orig hashes) | ON ms (windows seen/%d) | OFF ms (seen) | delta' % len(W1))
    for k in sorted(keys, key=lambda k: -(A.get(k, (0,))[0] + B.get(k, (0,))[0])):
        a, b = A.get(k), B.get(k)
        hs = re.findall(r'[0-9a-f]{32}', k[0]); fi = [v for v in P.values() if v[0] in hs]
        tag = ','.join('%s#%d' % (f[1], f[2]) for f in fi[:2])
        print('%s | %s | %s | %s | %s' % (tag, k[0][:110], '%.3f (%d)' % a if a else 'n/a(<top40)', '%.3f (%d)' % b if b else 'n/a(<top40)',
              '%+.3f' % (a[0] - b[0]) if a and b else ''))
main()
