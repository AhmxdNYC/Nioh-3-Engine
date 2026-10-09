r"""table_rows.py - (re)write the 'msv' rows of gpu-shaders\ally_shaders.sh TABLE from msv_manifest_all.tsv
(pc_gpu_session.ps1 and ally_shaders.sh read that TABLE; item msv = the flat layout from splice_msv.py --flat).
    py -I gpu-shaders\meshvs\table_rows.py [--set all|hot]
"""
import os, re, sys
HERE = os.path.dirname(os.path.abspath(__file__))
which = sys.argv[sys.argv.index('--set') + 1] if '--set' in sys.argv else 'all'
sh = os.path.join(HERE, '..', 'ally_shaders.sh')
t = open(sh, encoding='utf-8', newline='').read()
man = [l.split('\t') for l in open(os.path.join(HERE, 'msv_manifest_%s.tsv' % which), encoding='utf-8').read().splitlines()
       if l and not l.startswith('#')]
m = re.search(r'TABLE="\n(.*?)\n"', t, re.S)
keep = [l for l in m.group(1).split('\n') if not l.startswith('msv ')]
rows = keep + ['msv   %s  %s  %s' % (f, so, sn) for f, so, sn, n in man]
t = t[:m.start(1)] + '\n'.join(rows) + t[m.end(1):]
open(sh, 'w', encoding='utf-8', newline='').write(t)
print('%d msv rows (%s) in %s' % (len(man), which, os.path.abspath(sh)))
