#!/usr/bin/env python3
"""samelook_report.py - turn a samelook_ab.ps1 session into the same-look table (notes/same-look-package.md).

  py -I tools\\testmatrix\\samelook_report.py report <session dir> [--thresh 8]

Inputs: manifest.jsonl (kind arrive / knob / move, cond, pass, stage, t0/t1 UTC, shot), presentmon*.csv (PresentMon v2 --date_time),
shots/*.png, mod_run*.log (stats/s VSMA / RCO lines). Output: samelook_report.md.
  move   : VSMA off vs on during the stick runs: frame time p50 / p99 / p99.9 and mean GPUBusy over all frames of each condition,
           plus the paired per-run delta (same stage + pass, ABBA order).
  knob   : per knob, GPUBusy vs the BASE windows of the same dwell (interpolated by position), and the shot vs the nearest BASE shot
           (mean abs diff, % pixels > thresh, SSIM); BASE-vs-BASE of the same dwell = noise floor.
  arrive : shots of the same stage spawn after a load with VSMA on vs off; floor = the two off arrivals (and the two on ones).
Uses the helpers of cost_vs_looks.py (same folder).
"""
import glob
import math
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import cost_vs_looks as cv  # noqa: E402


def pct(v, q):
    if not v:
        return float('nan')
    s = sorted(v)
    k = (len(s) - 1) * q
    a, b = int(math.floor(k)), int(math.ceil(k))
    return s[a] + (s[b] - s[a]) * (k - a)


def seg(pm, ts, t0, t1):
    import bisect
    a, b = bisect.bisect_left(ts, t0), bisect.bisect_right(ts, t1)
    return pm[a:b]


def f(v, nd=2):
    return cv.fnum(v, nd)


def report(d, thresh=8.0):
    rows = cv.read_jsonl(os.path.join(d, 'manifest.jsonl'))
    pm = []
    for p in sorted(glob.glob(os.path.join(d, 'presentmon_sl_*.csv'))) or [os.path.join(d, 'presentmon_tour.csv')]:
        pm += cv.read_pm(p)
    pm.sort()
    if pm and rows:
        cv.align_pm(pm, [(cv.utc_epoch(r['t0_utc']), cv.utc_epoch(r['t1_utc'])) for r in rows if r.get('t0_utc') and r.get('t1_utc')], os.path.basename(os.path.normpath(d)))
    ts = [r[0] for r in pm]
    out = ['# Same-look A/B report: %s' % os.path.basename(os.path.normpath(d)), '']
    if not rows or not pm:
        out.append('no manifest rows (%d) or PresentMon frames (%d)' % (len(rows), len(pm)))
        open(os.path.join(d, 'samelook_report.md'), 'w', encoding='utf-8').write('\n'.join(out) + '\n')
        print(out[-1])
        return 1
    for r in rows:
        r['_t0'], r['_t1'] = cv.utc_epoch(r['t0_utc']), cv.utc_epoch(r['t1_utc'])
        s = seg(pm, ts, r['_t0'], r['_t1'])
        r['_ft'] = [x[2] for x in s]
        r['_gb'] = [x[1] for x in s]

    # ---- movement ----
    out += ['## VSMA while moving (stick runs, VSMA live, ABBA per dwell)', '',
            '| cond | runs | frames | ft p50 | ft p99 | ft p99.9 | ft max | GPUBusy mean | GPUBusy p99 |', '|---|---|---|---|---|---|---|---|---|']
    mv = [r for r in rows if r['kind'] == 'move' and len(r['_ft']) >= 20]
    agg = {}
    for c in ('VSMA0', 'VSMA1'):
        rr = [r for r in mv if r['cond'] == c]
        ft = [x for r in rr for x in r['_ft']]
        gb = [x for r in rr for x in r['_gb']]
        agg[c] = dict(p99=pct(ft, .99), p999=pct(ft, .999), gb=sum(gb) / len(gb) if gb else float('nan'))
        out.append('| %s | %d | %d | %s | %s | %s | %s | %s | %s |' % (c, len(rr), len(ft), f(pct(ft, .5)), f(pct(ft, .99)), f(pct(ft, .999)),
                                                                     f(max(ft) if ft else float('nan')), f(agg[c]['gb'], 3), f(pct(gb, .99))))
    # paired: per dwell (pass, stage) the mean of the two on runs minus the two off runs
    pairs = {}
    for r in mv:
        pairs.setdefault((r['pass'], r['stage']), {}).setdefault(r['cond'], []).append(r)
    dp99, dgb = [], []
    for k, v in sorted(pairs.items()):
        if 'VSMA0' in v and 'VSMA1' in v:
            p0 = sum(pct(r['_ft'], .99) for r in v['VSMA0']) / len(v['VSMA0'])
            p1 = sum(pct(r['_ft'], .99) for r in v['VSMA1']) / len(v['VSMA1'])
            g0 = sum(sum(r['_gb']) / len(r['_gb']) for r in v['VSMA0']) / len(v['VSMA0'])
            g1 = sum(sum(r['_gb']) / len(r['_gb']) for r in v['VSMA1']) / len(v['VSMA1'])
            dp99.append(p1 - p0)
            dgb.append(g1 - g0)
    if dp99:
        m = sum(dp99) / len(dp99)
        sd = (sum((x - m) ** 2 for x in dp99) / max(1, len(dp99) - 1)) ** 0.5
        mg = sum(dgb) / len(dgb)
        sg = (sum((x - mg) ** 2 for x in dgb) / max(1, len(dgb) - 1)) ** 0.5
        out += ['', 'Paired per dwell (on - off, %d dwells): frame time p99 %+.3f ms (sd %.3f, se %.3f), GPUBusy mean %+.3f ms (sd %.3f, se %.3f)'
                % (len(dp99), m, sd, sd / math.sqrt(len(dp99)), mg, sg, sg / math.sqrt(len(dgb)))]
    # mod stats lines
    vs = []
    for lg in sorted(glob.glob(os.path.join(d, 'mod_run*.log'))):
        for ln in open(lg, encoding='utf-8', errors='replace'):
            if 'stats/s VSMA' in ln or 'RCO: engine values' in ln or 'VSMA: ' in ln and 'installed' in ln or 'RCO: update-rate' in ln:
                vs.append(ln.strip())
    if vs:
        out += ['', 'Mod lines (first / last):', '', '```'] + vs[:4] + (['...'] + vs[-3:] if len(vs) > 7 else vs[4:]) + ['```']

    # ---- knobs ----
    out += ['', '## Update-rate knobs (static spawn view, BASE brackets each dwell)', '',
            '| knob | dwells | GPUBusy delta ms (mean, se) | shot mad | %% px > %d | SSIM | BASE-BASE mad | BASE-BASE %% | BASE-BASE SSIM |' % thresh,
            '|---|---|---|---|---|---|---|---|---|']
    knobs = sorted({r['cond'] for r in rows if r['kind'] == 'knob' and r['cond'] != 'BASE'})
    dwells = {}
    for r in rows:
        if r['kind'] == 'knob':
            dwells.setdefault((r['pass'], r['stage']), []).append(r)
    imgc = {}

    def img(p):
        if p not in imgc:
            imgc[p] = cv.load_img(os.path.join(d, p)) if p else None
        return imgc[p]

    for k in knobs:
        dg, cmp, flo = [], [], []
        for key, lst in dwells.items():
            idx = [i for i, r in enumerate(lst) if r['cond'] == k]
            base = [i for i, r in enumerate(lst) if r['cond'] == 'BASE']
            if not idx or len(base) < 2 or not lst[idx[0]]['_gb']:
                continue
            i = idx[0]
            b0, b1 = base[0], base[-1]
            g = lambda r: sum(r['_gb']) / len(r['_gb']) if r['_gb'] else float('nan')
            gb = g(lst[b0]) + (g(lst[b1]) - g(lst[b0])) * (i - b0) / max(1, b1 - b0)
            dg.append(g(lst[i]) - gb)
            if cv.np is not None:
                nb = b0 if abs(i - b0) <= abs(i - b1) else b1
                c = cv.compare(img(lst[i].get('shot')), img(lst[nb].get('shot')), thresh)
                fl = cv.compare(img(lst[b0].get('shot')), img(lst[b1].get('shot')), thresh)
                if c:
                    cmp.append(c)
                if fl:
                    flo.append(fl)
        if not dg:
            continue
        m = sum(dg) / len(dg)
        se = ((sum((x - m) ** 2 for x in dg) / max(1, len(dg) - 1)) ** 0.5) / math.sqrt(len(dg))
        av = lambda L, key: sum(x[key] for x in L) / len(L) if L else float('nan')
        out.append('| %s | %d | %+.3f (%.3f) | %s | %s | %s | %s | %s | %s |' % (k, len(dg), m, se, f(av(cmp, 'mad'), 3), f(av(cmp, 'pct'), 3),
                                                                          f(av(cmp, 'ssim'), 4), f(av(flo, 'mad'), 3), f(av(flo, 'pct'), 3), f(av(flo, 'ssim'), 4)))

    # ---- arrival shots ----
    out += ['', '## VSMA arrival shots (same spawn after a load: VSMA on vs off; floor = off vs off, on vs on)', '',
            '| stage | on-vs-off mad | on-vs-off % px | SSIM | off-vs-off mad | off-vs-off % px | SSIM | on-vs-on mad | on-vs-on % px | SSIM |',
            '|---|---|---|---|---|---|---|---|---|---|']
    arr = {}
    for r in rows:
        if r['kind'] == 'arrive' and r.get('shot'):
            arr.setdefault(r['stage'], {}).setdefault(r['cond'], []).append(r)
    for s, v in sorted(arr.items()):
        on, off = v.get('VSMA1', []), v.get('VSMA0', [])
        if cv.np is None or not on or not off:
            continue
        x = cv.compare(img(on[0]['shot']), img(off[0]['shot']), thresh)
        y = cv.compare(img(off[0]['shot']), img(off[-1]['shot']), thresh) if len(off) > 1 else None
        z = cv.compare(img(on[0]['shot']), img(on[-1]['shot']), thresh) if len(on) > 1 else None
        cell = lambda c: (f(c['mad'], 3), f(c['pct'], 3), f(c['ssim'], 4)) if c else ('-', '-', '-')
        out.append('| %s | %s | %s | %s | %s | %s | %s | %s | %s | %s |' % ((s,) + cell(x) + cell(y) + cell(z)))
    # arrival frame times (static 3 s after the load settles)
    out += ['', '## Arrival windows (static, 3 s): frame time p99 / GPUBusy by VSMA state', '']
    for c in ('VSMA0', 'VSMA1'):
        rr = [r for r in rows if r['kind'] == 'arrive' and r['cond'] == c]
        ft = [x for r in rr for x in r['_ft']]
        gb = [x for r in rr for x in r['_gb']]
        out.append('- %s: %d windows, %d frames, ft p99 %s ms, GPUBusy %s ms' % (c, len(rr), len(ft), f(pct(ft, .99)), f(sum(gb) / len(gb) if gb else float('nan'), 3)))
    open(os.path.join(d, 'samelook_report.md'), 'w', encoding='utf-8').write('\n'.join(out) + '\n')
    print('\n'.join(out))
    print('report: ' + os.path.join(d, 'samelook_report.md'))
    return 0


def main(argv):
    if len(argv) >= 2 and argv[0] == 'report':
        th = 8.0
        if '--thresh' in argv:
            th = float(argv[argv.index('--thresh') + 1])
        return report(argv[1], th)
    print(__doc__)
    return 2


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
