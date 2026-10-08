#!/usr/bin/env python3
"""ally_budget.py - Ally GPU budget from mod logs, GAMEPLAY windows only (menus excluded).

  py -I re\\scripts_gpu\\ally_budget.py gp2 LOG [--min-draws 2000] [--top 60]   per-PSO GP2 means over gameplay windows
  py -I re\\scripts_gpu\\ally_budget.py gp1 LOG [--min-span 28]                  engine-marker GP (no PSO stamps) means
  py -I re\\scripts_gpu\\ally_budget.py windows LOG                              list every GP/GP2 window + OSD fps

Why: gp2_digest.py averages ALL stamped windows. In the 175-L* logs 26-28 of 36-40 windows are the title/menu
(16.6 ms span, ~56 draws/frame, FSR3 runs every frame, nothing else does), so FSR3 dominated the old table and the
gameplay passes were diluted ~4x. GP2 gameplay = windows with >= --min-draws draws+dispatches/frame.
Stdlib only; labels come from gp2_digest.resolve (hash index cache).
"""
import argparse, os, re, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

GP2F = re.compile(r'GP2: frame ([\d.]+) ms GPU span \(wall ([\d.]+) ms\); covered ([\d.]+) ms.*?async-compute ([\d.]+) ms; '
                  r'(\d+) intervals/frame, (\d+) draws\+dispatches/frame, PSO stamps (ON|OFF)')
GP2R = re.compile(r'GP2:\s+(\d+) (.*?) \| +([\d.]+) ms \| +([\d.]+) \| +([\d.]+) \| \[(.*)\]\s*$')
GP1F = re.compile(r'GP: frame ([\d.]+) ms GPU \(sum of top-level markers; first-to-last span ([\d.]+) ms\), (\d+) markers/frame over (\d+) frames')
GP1R = re.compile(r'GP:\s+(\d+) (.+?)\s+incl\s+([\d.]+) ms\s+self\s+([\d.]+) ms\s+([\d.]+)/frame')
OSD = re.compile(r'OSD: stats: rendered ([\d.]+) fps')


def windows(path):
    w2, w1, cur2, cur1, fps = [], [], None, None, []
    for ln in open(path, encoding='utf-8', errors='replace'):
        m = GP2F.search(ln)
        if m:
            cur2 = dict(span=float(m[1]), wall=float(m[2]), cov=float(m[3]), asy=float(m[4]), iv=int(m[5]),
                        draws=int(m[6]), on=m[7] == 'ON', rows={}, fps=None)
            w2.append(cur2); continue
        m = GP2R.search(ln)
        if m and cur2 is not None:
            cur2['rows'][(m[2].strip(), m[6])] = (float(m[3]), float(m[4])); continue
        m = GP1F.search(ln)
        if m:
            cur1 = dict(sum=float(m[1]), span=float(m[2]), mk=int(m[3]), frames=int(m[4]), rows={}, fps=None)
            w1.append(cur1); continue
        m = GP1R.search(ln)
        if m and cur1 is not None:
            cur1['rows'][m[2].strip()] = (float(m[3]), float(m[4]), float(m[5])); continue
        m = OSD.search(ln)
        if m:
            for w in (cur1, cur2):
                if w is not None and w['fps'] is None:
                    w['fps'] = float(m[1])
    return w1, w2


def cmd_windows(a):
    w1, w2 = windows(a.log)
    for i, w in enumerate(w1):
        print('GP1 %2d span %6.2f sum %6.2f mk %3d fps %s' % (i, w['span'], w['sum'], w['mk'], w['fps']))
    for i, w in enumerate(w2):
        print('GP2 %2d span %6.2f wall %6.2f cov %6.2f async %5.2f draws %4d %s fps %s' %
              (i, w['span'], w['wall'], w['cov'], w['asy'], w['draws'], 'ON ' if w['on'] else 'OFF', w['fps']))


def cmd_gp2(a):
    from gp2_digest import load_index, resolve
    _, w2 = windows(a.log)
    g = [w for w in w2 if w['on'] and w['draws'] >= a.min_draws]
    if not g:
        sys.exit('no gameplay GP2 windows')
    n = len(g)
    span = sum(w['span'] for w in g) / n
    acc = {}
    for w in g:
        for k, (ms, dr) in w['rows'].items():
            s = acc.setdefault(k, [0.0, 0.0, 0])
            s[0] += ms; s[1] += dr; s[2] += 1
    I = load_index(False)
    rows = sorted(acc.items(), key=lambda kv: -kv[1][0])[:a.top]
    print('%d gameplay windows (draws >= %d); mean stamped span %.2f ms, fps(stamped) %s' %
          (n, a.min_draws, span, [w['fps'] for w in g]))
    print('rank|ms|%span|draws|seen|desc|marker|name|replacement')
    for r, ((desc, mk), (ms, dr, seen)) in enumerate(rows, 1):
        _, _, names, rep, _ = resolve(desc, I)
        print('%d|%.3f|%.1f|%.1f|%d/%d|%s|%s|%s|%s' % (r, ms / n, 100 * ms / n / span, dr / n, seen, n, desc, mk,
              ' / '.join(dict.fromkeys(names)) or '-', '; '.join(rep) or 'no'))


def cmd_gp1(a):
    w1, _ = windows(a.log)
    g = [w for w in w1 if w['span'] >= a.min_span and w['mk'] >= 55]
    if not g:
        sys.exit('no gameplay GP1 windows')
    n = len(g)
    acc = {}
    for w in g:
        for k, (inc, slf, pf) in w['rows'].items():
            s = acc.setdefault(k, [0.0, 0.0, 0.0, 0])
            s[0] += inc; s[1] += slf; s[2] += pf; s[3] += 1
    print('%d gameplay GP1 windows (span >= %.0f); mean span %.2f, marker sum %.2f, fps %s' %
          (n, a.min_span, sum(w['span'] for w in g) / n, sum(w['sum'] for w in g) / n, [w['fps'] for w in g]))
    for k, (inc, slf, pf, seen) in sorted(acc.items(), key=lambda kv: -kv[1][0]):
        print('%-45s incl %.3f self %.3f  %.1f/frame  seen %d/%d' % (k, inc / n, slf / n, pf / n, seen, n))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('cmd', choices=['gp2', 'gp1', 'windows']); ap.add_argument('log')
    ap.add_argument('--min-draws', type=int, default=2000); ap.add_argument('--min-span', type=float, default=28.0)
    ap.add_argument('--top', type=int, default=60)
    a = ap.parse_args()
    {'gp2': cmd_gp2, 'gp1': cmd_gp1, 'windows': cmd_windows}[a.cmd](a)


if __name__ == '__main__':
    main()
