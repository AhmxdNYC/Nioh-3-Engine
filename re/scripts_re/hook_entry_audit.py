"""Entry-detour audit: for each hooked function entry, which registers the hook thunk may clobber that the original
function never writes (so an LTCG caller may keep them live across the call), and whether any direct caller does.

  py -I hook_entry_audit.py <thunk_clobbers> <rva> [<rva> ...]
  thunk_clobbers: comma list, e.g. rax,xmm0,xmm1,xmm2,xmm3,xmm4,xmm5,flags (bosslog thunk up to 1.7.23)
Also reports xmm0-3 read at entry (float arguments the C++ hook would destroy before the original runs).
"""
import os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import hook_liveness as H

def main():
    tc = sys.argv[1].split(',')
    for a in sys.argv[2:]:
        e = int(a, 16)
        cl = H.clobbers(e)
        miss = [r for r in tc if r not in cl]
        xin = H.live_after(e, ['xmm0', 'xmm1', 'xmm2', 'xmm3'], fn=H.func_of(e))
        xin = sorted(r for r, v in xin.items() if any(k == 'read' for _, k, _ in v))
        line = '%X clob-missing=%s xmm-args=%s' % (e, ','.join(miss) or '-', ','.join(xin) or '-')
        bad = []
        if miss:
            for site, op in H.callers(e):
                if op != 0xE8:
                    bad.append('%X:TAILJMP' % site); continue
                h = H.resolve(H.live_after(site + 5, miss), H.func_of(site + 5))
                h = {r: v for r, v in h.items() if any(k in ('read', 'escape') or k.startswith('ARG') for _, k, _ in v)}
                if h: bad.append('%X:%s' % (site, '/'.join('%s@%X' % (r, v[0][0]) for r, v in sorted(h.items()))))
            line += ' callers=%d' % len(H.callers(e))
        print(line + ('  RISK ' + ' '.join(bad) if bad else ''))

if __name__ == '__main__':
    main()
