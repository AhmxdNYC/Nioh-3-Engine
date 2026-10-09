#!/usr/bin/env python3
"""xsys_compare.py - cross-system GPU pass comparison (PC RDNA4 / pc2 RDNA2 / Ally RDNA3-RADV).

Finds passes whose relative cost differs between architectures by more than raw GPU speed explains
(notes/cross-system-compare.md).

  py -I tools\\xsys_compare.py --in pc=LOG --in pc2=LOG --in ally=LOG [--points 7356:1,52:1,7292:1,5631:1]
                               [--min-draws 1000] [--base pc2] [--ref HASH|FAMILY] [--top 30] [--pm pc=presentmon.csv ...]

Inputs (one --in per system, name=path):
  * a mod log with GP2 per-PSO windows (GPUProf = 2). Tour runs label every window with its tour point
    (key + point index): windows are matched across systems by that label (--points filters; default = the
    labels every system has). Logs without tour labels use every PSO-stamped window with >= --min-draws.
  * an RGP trace (.rgp, or the base path of its rgp_report outputs): needs <base>.events.csv and
    <base>.summary.txt; the PSO -> DXBC digest map is built from the .rgp once (<base>.pso_digests.csv).
    One frame; its label is '*' and it matches any point.

Normalisation:
  * share  = pass ms / mean GP2 span of the matched windows (graphics queue; async rows listed apart).
  * speed  = per system pair, the median of ms_a/ms_b over shaders that both ran (>= --min-ms), or the
             ms ratio of --ref (a digest prefix or a family name). That is the 'raw GPU speed' factor.
  * anomaly = (ms_a/ms_b) / speed. 1.0 = scales like the GPU; >= 2 or <= 0.5 = architecture/driver specific.
    Computed per identical shader (same DXBC digests on both systems) and per family (arch_budget.FAMILIES).
Stdlib only. Names come from re/scripts_gpu/gp2_digest (hash index cache D:\\nioh3-re\\gp2_hash_index.json).
"""
import argparse, csv, math, os, re, statistics, sys
from collections import defaultdict
from datetime import datetime

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, 're', 'scripts_gpu'))
sys.path.insert(0, os.path.join(ROOT, 'patcher', 'gpu'))

GP2F = re.compile(r'GP2: frame ([\d.]+) ms GPU span \(wall ([\d.]+) ms\); covered ([\d.]+) ms.*?async-compute ([\d.]+) ms; '
                  r'(\d+) intervals/frame, (\d+) draws\+dispatches/frame, PSO stamps (ON|OFF)')
GP2R = re.compile(r'GP2:\s+(\d+) (.*?) \| +([\d.]+) ms \| +([\d.]+) \| +([\d.]+) \| \[(.*)\]\s*$')
TOURP = re.compile(r'^\[(\d\d:\d\d:\d\d)\.\d+\] Tour P .*?key=(\d+) pt=(\d+)/\d+ .*?dwell=([\d.]+)')
SWAP = re.compile(r'OSD: swapchain \S+: (\d+)x(\d+)')
MINRUN = re.compile(r'GPUProf2_MinRun\D+(\d+)')
TOK = re.compile(r'\b(vs|hs|ds|gs|ps|cs|ms|as)=([0-9a-f]{32})')
STAGE_ORDER = {'vs': 0, 'hs': 1, 'ds': 2, 'gs': 3, 'ms': 4, 'as': 5, 'ps': 6, 'cs': 7}
RGP_STAGE = {'vertex': 'vs', 'hull': 'hs', 'domain': 'ds', 'geometry': 'gs', 'pixel': 'ps', 'compute': 'cs',
             'mesh': 'ms', 'task': 'as'}
COPYDESC = '(copy / resolve / clear)'


def key_of(desc):
    """canonical shader key: stage-ordered digests; non-shader buckets keep their text."""
    toks = sorted(TOK.findall(desc), key=lambda t: STAGE_ORDER[t[0]])
    return ' '.join('%s=%s' % t for t in toks) if toks else re.sub(r'\s*\(async compute\)', '', desc).strip()


# ---------------------------------------------------------------- loaders
def load_gp2(path):
    """-> dict(windows=[{label,span,asy,draws,rows{key:(ms,draws,async)},t_end,dwell}], out=(w,h), minrun)"""
    wins, label, cur, out, minrun, tend, dwell = [], None, None, None, None, None, None
    for ln in open(path, encoding='utf-8', errors='replace'):
        m = TOURP.search(ln)
        if m:
            label, tend, dwell = '%s:%s' % (m[2], m[3]), m[1], float(m[4]); continue
        m = GP2F.search(ln)
        if m:
            cur = dict(label=label, span=float(m[1]), asy=float(m[4]), draws=int(m[6]), on=m[7] == 'ON', rows={},
                       t_end=tend, dwell=dwell)
            wins.append(cur); label = None; continue
        m = GP2R.search(ln)
        if m and cur is not None:
            desc = m[2].strip()
            k = key_of(desc)
            ms, dr = float(m[3]), float(m[4])
            o = cur['rows'].get(k, (0.0, 0.0, False))
            cur['rows'][k] = (o[0] + ms, o[1] + dr, o[2] or '(async compute)' in desc); continue
        if out is None:
            m = SWAP.search(ln)
            if m:
                out = (int(m[1]), int(m[2]))
        if minrun is None:
            m = MINRUN.search(ln)
            if m:
                minrun = int(m[1])
    if minrun is None:   # the log does not print it; tour / session captures keep ini_used.ini next to the log
        for ini in (os.path.join(os.path.dirname(path), 'ini_used.ini'), os.path.join(os.path.dirname(path), '..', 'ini_used.ini')):
            if os.path.exists(ini):
                m = re.search(r'^\s*GPUProf2_MinRun\s*=\s*(\d+)', open(ini, encoding='utf-8', errors='replace').read(), re.M)
                minrun = int(m[1]) if m else 4
                break
    return dict(windows=[w for w in wins if w['on']], out=out, minrun=minrun, kind='gp2')


def load_rgp(path):
    base = re.sub(r'\.(rgp|events\.csv|summary\.txt)$', '', path)
    dig_csv = base + '.pso_digests.csv'
    if not os.path.exists(dig_csv):
        import rdf as R, rgp_report as G
        r = R.RDF(base + '.rgp')
        m, cos = G.pso_table(r)
        with open(dig_csv, 'w', newline='') as f:
            w = csv.writer(f); w.writerow(['api_pso_hash', 'stages'])
            for api, (ph, _) in m.items():
                c = cos.get(ph)
                if c is not None:
                    w.writerow([api, ' '.join('%s:%s' % x for x in G.api_shader_hashes(r.data(c)))])
    dig = {}
    for r in csv.DictReader(open(dig_csv)):
        toks = [x.split(':') for x in r['stages'].split()]
        dig[r['api_pso_hash']] = key_of(' '.join('%s=%s' % (RGP_STAGE.get(s, s), h) for s, h in toks if s in RGP_STAGE))
    span = None
    for ln in open(base + '.summary.txt', encoding='utf-8-sig', errors='replace'):
        m = re.search(r'frame span \(first\.\.last marker\): ([\d.]+) ms', ln)
        if m:
            span = float(m[1])
    rows, asy, n = {}, 0.0, 0
    for e in csv.DictReader(open(base + '.events.csv')):
        us = float(e['est_us'] or 0)
        isasy = e['queue'] != 'GFX'
        k = dig.get(e['pso'])
        if not k:
            k = COPYDESC if re.search(r'Clear|Copy|Resolve|api15', e['kind']) else 'unattributed (no PSO)'
        o = rows.get(k, (0.0, 0.0, False))
        rows[k] = (o[0] + us / 1000, o[1] + 1, isasy or o[2])
        n += 1
        if isasy:
            asy += us / 1000
    w = dict(label='*', span=span, asy=asy, draws=n, on=True, rows=rows, t_end=None, dwell=None)
    return dict(windows=[w], out=None, minrun=0, kind='rgp')


def load(path):
    if path.endswith('.rgp') or os.path.exists(path + '.events.csv') or path.endswith('.events.csv'):
        return load_rgp(path)
    return load_gp2(path)


def pm_gpubusy(csvpath, wins):
    """PresentMon median GPUBusy / FrameTime per tour window (matched by local time of day of the dwell)."""
    rows = []
    with open(csvpath, encoding='utf-8-sig', errors='replace') as f:
        for r in csv.DictReader(f):
            try:
                t = r['CPUStartDateTime'].split(' ')[1][:15]
                h, mi, s = t.split(':')
                rows.append((int(h) * 3600 + int(mi) * 60 + float(s), float(r['GPUBusy']), float(r['FrameTime'])))
            except (KeyError, ValueError, IndexError):
                pass
    spans = []
    for w in wins:
        if w['t_end']:
            h, mi, s = w['t_end'].split(':')
            e = int(h) * 3600 + int(mi) * 60 + int(s)
            spans.append((e - (w['dwell'] or 12), e))
    sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), 'testmatrix'))
    import pm_time
    off = pm_time.detect_offset_tod([r[0] for r in rows], spans, os.path.basename(csvpath))
    if off:
        rows = [((r[0] + off) % 86400, r[1], r[2]) for r in rows]
    out = {}
    for w in wins:
        if not w['t_end']:
            continue
        h, mi, s = w['t_end'].split(':')
        t1 = int(h) * 3600 + int(mi) * 60 + int(s); t0 = t1 - (w['dwell'] or 12)
        sel = [r for r in rows if t0 <= r[0] <= t1]
        if sel:
            out.setdefault(w['label'], []).append((statistics.median(r[1] for r in sel), statistics.median(r[2] for r in sel)))
    return {k: (statistics.mean(a for a, _ in v), statistics.mean(b for _, b in v)) for k, v in out.items()}


# ---------------------------------------------------------------- aggregation
def aggregate(sysd, labels, min_draws):
    ws = sysd['windows']
    if sysd['kind'] == 'rgp':
        g = ws
    elif labels is not None and any(w['label'] for w in ws):
        g = [w for w in ws if w['label'] in labels]
    else:
        g = [w for w in ws if w['draws'] >= min_draws]
    if not g:
        return None
    n = len(g)
    rows = defaultdict(lambda: [0.0, 0.0, False])
    for w in g:
        for k, (ms, dr, a) in w['rows'].items():
            r = rows[k]; r[0] += ms / n; r[1] += dr / n; r[2] = r[2] or a
    return dict(n=n, span=sum(w['span'] for w in g) / n, asy=sum(w['asy'] for w in g) / n,
                draws=sum(w['draws'] for w in g) / n, rows=dict(rows), labels=sorted({w['label'] for w in g}, key=str),
                noise=repeat_noise(g))


def repeat_noise(g, min_ms=0.05):
    """Same-device repeatability: windows of the same tour label split into first visit vs later visits (tour passes);
    per shader |log2(ms per draw, A / B)|. Returns (p50, p90, n rows) or None with < 2 visits."""
    by = defaultdict(list)
    for w in g:
        if w['label'] and w['label'] != '*':
            by[w['label']].append(w)
    A, B = defaultdict(lambda: [0.0, 0.0]), defaultdict(lambda: [0.0, 0.0])
    m = 0
    for ws in by.values():
        if len(ws) < 2:
            continue
        m += 1
        for half, sub in ((A, ws[:1]), (B, ws[1:])):
            for w in sub:
                for k, (ms, dr, _) in w['rows'].items():
                    half[k][0] += ms / len(sub); half[k][1] += dr / len(sub)
    if not m:
        return None
    d = sorted(abs(math.log2((A[k][0] / A[k][1]) / (B[k][0] / B[k][1]))) for k in A
               if k in B and TOK.search(k) and min(A[k][0], B[k][0]) >= min_ms * m and A[k][1] > 0 and B[k][1] > 0)
    if len(d) < 5:
        return None
    return d[len(d) // 2], d[int(len(d) * 0.9)], len(d)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--in', dest='ins', action='append', required=True, help='name=path (GP2 mod log or RGP trace)')
    ap.add_argument('--points', help='tour labels key:pt,... (default: labels common to all tour logs)')
    ap.add_argument('--min-draws', type=int, default=1000, help='gameplay filter for logs without tour labels')
    ap.add_argument('--base', help='system the others are compared against (default: first --in)')
    ap.add_argument('--ref', help='reference pass for the speed factor: digest prefix or family name (default: median)')
    ap.add_argument('--min-ms', type=float, default=0.05, help='ignore shaders below this on either system')
    ap.add_argument('--top', type=int, default=30)
    ap.add_argument('--norm', choices=('draw', 'frame'), default='draw',
                    help='identical-shader anomaly on ms per draw/dispatch (default; removes workload-count '
                         'differences between spots) or on ms per frame')
    ap.add_argument('--pm', action='append', default=[], help='name=presentmon.csv (GPUBusy per matched window)')
    a = ap.parse_args()

    from gp2_digest import load_index, resolve
    from arch_budget import family
    I = load_index(False)
    names_cache = {}

    def names(k):
        if k not in names_cache:
            _, _, nm, _, _ = resolve(k, I)
            names_cache[k] = ' / '.join(dict.fromkeys(nm))
        return names_cache[k]

    def fam(k, isasy):
        f = family('%s | %s | -' % (names(k), k))
        return f + (' [async]' if isasy else '')

    systems = []
    for s in a.ins:
        nm, p = s.split('=', 1)
        systems.append((nm, p, load(p)))
    base = a.base or systems[0][0]

    tour_sets = [{w['label'] for w in d['windows'] if w['label']} for _, _, d in systems if d['kind'] == 'gp2']
    tour_sets = [t for t in tour_sets if t]
    if a.points:
        labels = set(a.points.split(','))
    elif tour_sets:
        labels = set.intersection(*tour_sets) if len(tour_sets) > 1 else tour_sets[0]
    else:
        labels = None

    agg = {}
    print('== inputs ==')
    for nm, p, d in systems:
        g = aggregate(d, labels, a.min_draws)
        if g is None:
            sys.exit('%s: no windows match (labels %s, min-draws %d)' % (nm, sorted(labels or []), a.min_draws))
        agg[nm] = g
        print('%-5s %s: %s, %d window(s) %s, span %.2f ms, async %.2f ms, %.0f draws+dispatches/frame, output %s, MinRun %s'
              % (nm, d['kind'], os.path.basename(p), g['n'], ','.join(map(str, g['labels']))[:60], g['span'], g['asy'],
                 g['draws'], '%dx%d' % d['out'] if d['out'] else '?', d['minrun']))
        if g['noise']:
            print('      repeatability (pass 1 vs later passes, per shader ms/draw): median x%.2f, p90 x%.2f over %d shaders'
                  % (2 ** g['noise'][0], 2 ** g['noise'][1], g['noise'][2]))
    pmv = {}
    for s in a.pm:
        nm, p = s.split('=', 1)
        d = dict((x[0], x[2]) for x in systems)[nm]
        pmv[nm] = pm_gpubusy(p, [w for w in d['windows'] if labels is None or w['label'] in labels])
        if pmv[nm]:
            print('%-5s PresentMon GPUBusy / FrameTime per point: %s' % (nm, ', '.join(
                '%s %.1f/%.1f' % (k, b, f) for k, (b, f) in sorted(pmv[nm].items()))))
    names_ = [n for n, _, _ in systems]

    # family totals
    famt = {n: defaultdict(float) for n in names_}
    for n in names_:
        for k, (ms, dr, isasy) in agg[n]['rows'].items():
            famt[n][fam(k, isasy)] += ms

    def speed(o):
        if a.ref:
            ref = a.ref.lower()
            if ref in famt[base] or any(ref in f.lower() for f in famt[base]):
                f = next(f for f in famt[base] if ref in f.lower())
                return famt[o][f] / famt[base][f] if famt[base][f] else float('nan'), 'family %s' % f
            ka = [k for k in agg[base]['rows'] if ref in k]
            if ka and ka[0] in agg[o]['rows']:
                return agg[o]['rows'][ka[0]][0] / agg[base]['rows'][ka[0]][0], 'shader %s' % ka[0][:40]
        rs = [unit(o, k) / unit(base, k) for k, v in agg[base]['rows'].items()
              if k in agg[o]['rows'] and v[0] >= a.min_ms and agg[o]['rows'][k][0] >= a.min_ms and not v[2]
              and TOK.search(k)]
        return (statistics.median(rs) if rs else float('nan')), 'median of %d shared graphics shaders' % len(rs)

    def unit(n, k):
        ms, dr, _ = agg[n]['rows'][k]
        return ms / dr if a.norm == 'draw' and dr > 0 else ms

    others = [n for n in names_ if n != base]
    spd = {}
    print('\n== speed factors (ms%s on X / on %s; >1 = X slower) ==' % (' per draw' if a.norm == 'draw' else '', base))
    for o in others:
        spd[o] = speed(o)
        print('%-5s x%.2f  (%s; span ratio x%.2f)' % (o, spd[o][0], spd[o][1], agg[o]['span'] / agg[base]['span']))

    # strong-flag threshold: 2x, or the worst same-device repeat noise (p90) if that is larger
    nz = [agg[n]['noise'][1] for n in names_ if agg[n]['noise']]
    thr = max(2.0, 2 ** max(nz)) if nz else 2.0
    print('strong anomaly threshold: x%.2f (%s)' % (thr, 'max(2, same-device p90 repeat noise)' if nz else
          'no repeated tour points in the inputs: noise unknown, treat single rows below ~x3 as unproven'))

    def flag(x):
        if not x or x != x:
            return ''
        return ('<<<' if x >= thr else ('<<' if x >= 1.5 else ('>>>' if x <= 1 / thr else ('>>' if x <= 0.67 else ''))))

    # family table: per frame; its speed factor is the median per-frame ratio of the shared shaders
    fspd = {}
    for o in others:
        rs = [agg[o]['rows'][k][0] / v[0] for k, v in agg[base]['rows'].items() if k in agg[o]['rows'] and v[0] >= a.min_ms
              and agg[o]['rows'][k][0] >= a.min_ms and not v[2] and TOK.search(k)]
        fspd[o] = spd[o][0] if a.ref else (statistics.median(rs) if rs else float('nan'))
    print('\nfamily speed factors (median per-frame ms ratio): ' + ', '.join('%s x%.2f' % (o, fspd[o]) for o in others))
    # family table
    allf = sorted({f for n in names_ for f in famt[n]}, key=lambda f: -max(famt[n][f] / agg[n]['span'] for n in names_))
    print('\n== families: ms (%% of span), anomaly vs %s after the speed factor ==' % base)
    hdr = 'family'.ljust(52) + ''.join(('%s ms (%%)' % n).rjust(16) for n in names_) + ''.join(('anom %s' % o).rjust(12) for o in others)
    print(hdr)
    for f in allf:
        line = f[:51].ljust(52) + ''.join(('%.2f (%.1f)' % (famt[n][f], 100 * famt[n][f] / agg[n]['span'])).rjust(16) for n in names_)
        for o in others:
            b, x = famt[base][f], famt[o][f]
            an = (x / b) / fspd[o] if b >= a.min_ms and x >= a.min_ms else float('nan')
            line += ('%.2f%s' % (an, flag(an)) if an == an else '-').rjust(12)
        print(line)

    # identical-shader table
    print('\n== identical shaders on every system (same DXBC digests), sorted by max |log2 anomaly| ==')
    keys = [k for k in agg[base]['rows'] if TOK.search(k) and all(k in agg[o]['rows'] for o in others)]
    out = []
    for k in keys:
        if agg[base]['rows'][k][0] < a.min_ms:
            continue
        b = unit(base, k)
        an = {}
        for o in others:
            an[o] = (unit(o, k) / b) / spd[o][0] if agg[o]['rows'][k][0] >= a.min_ms else float('nan')
        score = max((abs(math.log2(v)) for v in an.values() if v == v), default=0)
        out.append((score, k, an))
    out.sort(key=lambda t: -t[0])
    print('shader'.ljust(44) + ''.join(('%s ms/frame x draws' % n).rjust(22) for n in names_) +
          ''.join(('anom %s' % o).rjust(11) for o in others) + '  name / family')
    for score, k, an in out[:a.top]:
        short = ' '.join(t[0] + '=' + t[1][:8] for t in TOK.findall(k))
        line = short[:43].ljust(44)
        for n in names_:
            ms, dr, isasy = agg[n]['rows'][k]
            line += ('%.3f (%.1f%%) x%-5.1f%s' % (ms, 100 * ms / agg[n]['span'], dr, 'a' if isasy else ' ')).rjust(22)
        for o in others:
            v = an[o]
            line += ('%.2f%s' % (v, flag(v)) if v == v else '-').rjust(11)
        isasy = agg[base]['rows'][k][2]
        print(line + '  ' + (names(k) or '?')[:48] + ' | ' + fam(k, isasy).split(' (')[0])
    print('\n(anomaly on ms per %s: <<< >= 2x relatively more expensive on that system than on %s, << >= 1.5x; '
          '>>> / >> the same, cheaper. a = async queue row: wall time, overlaps graphics.)' % (a.norm, base))

    # shaders that exist on only some systems (big ones)
    print('\n== large rows present on one system only (>= 2 % of its span) ==')
    for n in names_:
        for k, (ms, dr, isasy) in sorted(agg[n]['rows'].items(), key=lambda kv: -kv[1][0]):
            if ms / agg[n]['span'] < 0.02:
                break
            if not all(k in agg[m]['rows'] for m in names_ if m != n):
                miss = [m for m in names_ if m != n and k not in agg[m]['rows']]
                print('%-5s %.3f ms (%.1f %%)%s  %s  [%s]  missing on %s' % (n, ms, 100 * ms / agg[n]['span'], ' async' if isasy else '',
                      ' '.join(t[0] + '=' + t[1][:8] for t in TOK.findall(k)) or k[:40], names(k)[:50] or fam(k, isasy), ','.join(miss)))


if __name__ == '__main__':
    main()
