#!/usr/bin/env python3
"""gpu_unattr.py - where the GP2 stamped span goes that the per-PSO table does not attribute (notes\\gpu-unattributed.md).

  py -I re\\scripts_gpu\\gpu_unattr.py LOG [--pm PRESENTMON.csv] [--min-draws 0] [--top 80] [--windows]
  py -I re\\scripts_gpu\\gpu_unattr.py --selftest

Input: the mod log (GPUProf = 2 'GP2:' window blocks, optional 'GQ:' blocks, optional tour 'Tour P' lines) and the
PresentMon --v2_metrics CSV of the same launch. Stdlib only.

Per GP2 window (all numbers per stamped frame unless marked):
  span S      first -> last GP2 stamp on the DIRECT queue(s)            covered C = sum of DIRECT intervals
  uncovered   S - C: DIRECT time between lists / before a list's first stamp / lists without stamps
  PSO tail    C - (DIRECT rows in the table): buckets below the table cut (log GPUProf2_Top, digest --top)
  async rows  async-compute buckets in the table: they overlap the DIRECT span, so summing them with DIRECT rows over-counts
  overhead    GP2's own line: wall frame of stamped vs unstamped frames (SampleEvery > 1), or ON vs OFF windows (AB, SampleEvery 1);
              PresentMon fold: GPUBusy by frame index mod SampleEvery (the stamped phase stands out when the stamps cost GPU time)
  PresentMon  FrameTime, GPUBusy (any queue of the process busy), GPUWait (idle inside the frame's GPU work), FT - GPUTime (idle
              between frames: CPU-late submit / present wait). Matched to the window by time (offset found automatically).
  GQ          gaps between DIRECT lists (idle or cross-queue waits), unmeasured submissions, list HEADS (barriers / transitions before
              the first draw), tails, lists without GP2 coverage. Short regular gaps (edges >= 0.9/frame, mean <= 150 us) = per-submit
              cost class; the rest = CPU-late submits or fence waits.
Output: one table per arm (stamps ON / OFF windows) with three groups: IDLE by cause, UNMEASURED work by category, PROFILER overhead.
"""
import argparse, csv, datetime as dt, math, os, re, sys

TS = re.compile(r'^\[(\d\d):(\d\d):(\d\d)\.(\d{3})\]')
GP2F = re.compile(r'GP2: frame ([\d.]+) ms GPU span \(wall ([\d.]+) ms\); covered ([\d.]+) ms.*?async-compute ([\d.]+) ms; '
                  r'(\d+) intervals/frame, (\d+) draws\+dispatches/frame, PSO stamps (ON|OFF)[^|]*\|.*?sampled (\d+) frames \(1 in (\d+)\)')
GP2T = re.compile(r'GP2: top \d+ of (\d+) ')
GP2R = re.compile(r'GP2:\s+(\d+) (.*?) \| +([\d.]+) ms \| +([\d.]+) \| +([\d.]+) \| \[(.*)\]\s*$')
OVS = re.compile(r'GP2: overhead: wall frame, sampled ([\d.]+) ms \((\d+)\) vs unsampled ([\d.]+) ms \((\d+)\) -> ([-+\d.]+) ms on a sampled frame'
                 r'.*?stamps (\d+)/sampled frame.*?hook CPU ([\d.]+) ms/sampled frame')
OVAB = re.compile(r'GP2: overhead: wall frame with PSO stamps ON ([\d.]+) ms \((\d+) windows\) vs OFF ([\d.]+) ms \((\d+) windows\)')
GQF = re.compile(r'GQ: frame: ([\d.]+) DIRECT lists/frame .*? over (\d+) sampled frames; span ([\d.]+) ms, lists busy ([\d.]+) ms, '
                 r'GAPS ([\d.]+) ms/frame \(([\d.]+) gaps/frame; per frame p50 ([\d.]+) p90 ([\d.]+) max ([\d.]+) ms\), gaps around '
                 r'unmeasured submissions ([\d.]+) ms \(([\d.]+)/frame, excluded\), list HEADS ([\d.]+) ms .*?tails ([\d.]+) ms, lists '
                 r'without GP2 coverage ([\d.]+) ms \| GP2 uncovered \(span - covered\) (?:([\d.]+) ms \(gaps \+ heads \+ no-cov = ([\d.]+)\)|n/a)')
GQE = re.compile(r'GQ:\s+\d+ \[(.*) -> (.*)\] \| ([\d.]+) ms \| ([\d.]+) \| ([\d.]+) \| ([\d.]+) \| (.*)$')
GQG = re.compile(r'GQ:\s+\d+ ([\d.]+) us \| \d+ -> \d+ of \d+ \| \[(.*)\] -> \[(.*)\] \| (\d+) -> (\d+)(.*?)\|')
TOURP = re.compile(r'Tour P pass=(\d+) stage=(\d+)/\d+ key=(\d+) pt=(\d+)/\d+ .*?dwell=([\d.]+) .*?t0=(\d+) t1=(\d+) label=(.*)$')
SE_INIT = re.compile(r'GP2: per-PSO attribution on - stamps in 1 frame of (\d+)')


def secs(m):
    return int(m[1]) * 3600 + int(m[2]) * 60 + int(m[3]) + int(m[4]) / 1000.0


def parse_log(lines):
    """-> (windows, sample_every). Window times are log-local seconds, unwrapped over midnight."""
    W, cur, gq, day, last_t, prev_end, tour, se = [], None, None, 0.0, None, None, None, 8
    for ln in lines:
        m = TS.match(ln)
        if m:
            t = secs(m) + day
            if last_t is not None and t < last_t - 43200:
                day += 86400.0; t += 86400.0
            last_t = t
        else:
            t = last_t
        s = ln[m.end():] if m else ln
        m2 = SE_INIT.search(s)
        if m2:
            se = int(m2[1])
        m2 = TOURP.search(s)
        if m2:
            tour = dict(stage=int(m2[2]), key=int(m2[3]), pt=int(m2[4]), dwell=float(m2[5]), t0=int(m2[6]), t1=int(m2[7]),
                        label=m2[8].strip(), t=t)
            continue
        m2 = GP2F.search(s)
        if m2:
            cur = dict(t=t, span=float(m2[1]), wall=float(m2[2]), cov=float(m2[3]), asy=float(m2[4]), iv=float(m2[5]),
                       draws=float(m2[6]), on=m2[7] == 'ON', frames=int(m2[8]), se=int(m2[9]), rows=[], nb=None, ov=None, gq=None,
                       tour=None, t_start=prev_end)
            if tour and t is not None and tour['t'] is not None and abs(t - tour['t']) < 2.0:
                cur['tour'] = tour
                cur['t_start'] = t - tour['dwell']
            tour = None
            prev_end = t
            W.append(cur); gq = None
            continue
        if cur is None:
            continue
        m2 = GP2T.search(s)
        if m2:
            cur['nb'] = int(m2[1]); continue
        m2 = GP2R.search(s)
        if m2:
            desc = m2[2].strip()
            asy = desc.endswith('(async compute)')
            cur['rows'].append(dict(desc=desc, asy=asy, ms=float(m2[3]), draws=float(m2[4]), iv=float(m2[5]), mk=m2[6]))
            continue
        m2 = OVS.search(s)
        if m2:
            cur['ov'] = dict(kind='sampled', s=float(m2[1]), ns=int(m2[2]), u=float(m2[3]), nu=int(m2[4]), d=float(m2[5]),
                             stamps=float(m2[6]), hook=float(m2[7]))
            continue
        m2 = OVAB.search(s)
        if m2:
            cur['ov'] = dict(kind='ab', s=float(m2[1]), ns=int(m2[2]), u=float(m2[3]), nu=int(m2[4]), d=float(m2[1]) - float(m2[3]))
            continue
        m2 = GQF.search(s)
        if m2:
            gq = dict(lists=float(m2[1]), frames=int(m2[2]), span=float(m2[3]), busy=float(m2[4]), gaps=float(m2[5]),
                      ngaps=float(m2[6]), p50=float(m2[7]), p90=float(m2[8]), gmax=float(m2[9]), unk=float(m2[10]),
                      nunk=float(m2[11]), heads=float(m2[12]), tails=float(m2[13]), nocov=float(m2[14]),
                      uncov=float(m2[15]) if m2[15] else None, edges=[], worst=[])
            cur['gq'] = gq
            continue
        if gq is not None:
            m2 = GQE.search(s)
            if m2:
                gq['edges'].append(dict(a=m2[1], b=m2[2], ms=float(m2[3]), n=float(m2[4]), mean=float(m2[5]), mx=float(m2[6])))
                continue
            m2 = GQG.search(s)
            if m2:
                gq['worst'].append(dict(us=float(m2[1]), a=m2[2], b=m2[3], unk='UNMEASURED' in m2[6]))
    return W, se


# ---- PresentMon ---------------------------------------------------------------------------------------------------------------
def fnum(x):
    try:
        v = float(x)
        return v if math.isfinite(v) else None
    except (TypeError, ValueError):
        return None


def load_pm(path):
    rows, day, last = [], 0.0, None
    with open(path, newline='', encoding='utf-8-sig', errors='replace') as f:
        for r in csv.DictReader(f):
            if (r.get('Application') or '').lower() != 'nioh3.exe':
                continue
            ts = (r.get('CPUStartDateTime') or '').strip()
            m = re.search(r'(\d+):(\d\d):(\d\d)(?:\.(\d+))?$', ts)
            if not m:
                continue
            t = int(m[1]) * 3600 + int(m[2]) * 60 + int(m[3]) + (float('0.' + m[4]) if m[4] else 0.0) + day
            if last is not None and t < last - 43200:
                day += 86400.0; t += 86400.0
            last = t
            rows.append((t, fnum(r.get('FrameTime') or r.get('msBetweenPresents')), fnum(r.get('GPUBusy')), fnum(r.get('GPUTime')),
                         fnum(r.get('GPUWait'))))
    return rows


def pm_slice(pm, a, b):
    import bisect
    keys = [r[0] for r in pm] if not hasattr(pm_slice, 'k') or pm_slice.pm is not pm else pm_slice.k
    pm_slice.pm, pm_slice.k = pm, keys
    return pm[bisect.bisect_left(keys, a):bisect.bisect_right(keys, b)]


def mean(v):
    v = [x for x in v if x is not None]
    return sum(v) / len(v) if v else None


def find_offset(W, pm):
    """log-local seconds = PM seconds + offset; candidates every 15 min within +-26 h, scored by |PM frame time - GP2 wall|"""
    best = None
    ws = [w for w in W if w.get('t_start') is not None and w['t'] - w['t_start'] >= 3]
    if not ws or not pm:
        return None
    for k in range(-104, 105):
        o = k * 900.0
        err, hit = 0.0, 0
        for w in ws:
            sl = pm_slice(pm, w['t_start'] - o + 1, w['t'] - o - 1)
            if len(sl) < 20:
                continue
            ft = mean([r[1] for r in sl])
            if ft:
                err += abs(ft - w['wall']) / w['wall']; hit += 1
        if hit >= max(1, len(ws) // 4):
            sc = err / hit - 0.02 * hit / len(ws)
            if best is None or sc < best[0]:
                best = (sc, o, hit)
    return best


def fold(sl, se):
    """GPUBusy / FrameTime by frame index mod se; -> (peak phase, d_busy, sem, d_ft)"""
    if se <= 1 or len(sl) < 4 * se:
        return None
    ph = [[] for _ in range(se)]
    for i, r in enumerate(sl):
        if r[2] is not None and r[1] is not None:
            ph[i % se].append((r[2], r[1]))
    mb = [mean([x[0] for x in p]) for p in ph]
    if any(x is None for x in mb):
        return None
    k = max(range(se), key=lambda i: mb[i])
    others = [x[0] for i, p in enumerate(ph) if i != k for x in p]
    o = mean(others)
    var = sum((x - o) ** 2 for x in others) / max(1, len(others) - 1)
    sem = math.sqrt(var / max(1, len(ph[k])) + var / max(1, len(others)))
    dft = mean([x[1] for x in ph[k]]) - mean([x[1] for i, p in enumerate(ph) if i != k for x in p])
    # median-based check: the excess of the peak phase over the median phase (robust to a scene change inside the window)
    srt = sorted(mb)
    return dict(phase=k, d=mb[k] - o, sem=sem, dft=dft, spread=srt[-2] - srt[0], n=len(sl))


# ---- report ---------------------------------------------------------------------------------------------------------------------
def agg_top(ws, top):
    acc = {}
    for w in ws:
        for r in w['rows']:
            k = (r['desc'], r['mk'])
            acc[k] = acc.get(k, 0.0) + r['ms']
    n = len(ws)
    rows = sorted(acc.items(), key=lambda kv: -kv[1])[:top]
    d = sum(v for (desc, _), v in rows if not desc.endswith('(async compute)')) / n
    a = sum(v for (desc, _), v in rows if desc.endswith('(async compute)')) / n
    return d, a


def f3(x):
    return '   -  ' if x is None else '%6.2f' % x


def report(W, se, pm, a, out):
    out('gpu_unattr: %d GP2 windows in the log (1 frame in %d stamped)' % (len(W), se))
    off = find_offset(W, pm) if pm else None
    if pm:
        out('PresentMon: %d frames; time offset %s' % (len(pm), 'not found (no overlap with the GP2 windows)' if off is None else
            '%+.2f h (%d windows matched)' % (off[1] / 3600.0, off[2])))
    for w in W:
        w['pm'] = None
        if off is not None and w.get('t_start') is not None:
            sl = pm_slice(pm, w['t_start'] - off[1] + 1, w['t'] - off[1] - 1)
            if len(sl) >= 20:
                ft, gb, gt, gw = (mean([r[i] for r in sl]) for i in (1, 2, 3, 4))
                w['pm'] = dict(n=len(sl), ft=ft, busy=gb, gtime=gt, wait=gw, fold=fold(sl, w['se']))
    if a.windows:
        out('')
        out(' # | t        | stamps | draws | wall  | span  | cov   | uncov | tail  | async | ovh/smp | PM ft | PM busy | PM wait | PM ft-gt | fold d(+-) | tour')
        for i, w in enumerate(W):
            rd = sum(r['ms'] for r in w['rows'] if not r['asy'])
            p = w['pm'] or {}
            fo = p.get('fold')
            tag = ('%d/%d %s' % (w['tour']['stage'], w['tour']['pt'], w['tour']['label'][:28])) if w['tour'] else ''
            out('%2d | %8s | %-6s | %5.0f | %5.2f | %5.2f | %5.2f | %5.2f | %5.2f | %5.2f | %7s | %5s | %7s | %7s | %8s | %10s | %s' % (
                i, str(dt.timedelta(seconds=int(w['t'] % 86400))), 'ON' if w['on'] else 'OFF', w['draws'], w['wall'], w['span'],
                w['cov'], w['span'] - w['cov'], w['cov'] - rd if w['on'] else 0, w['asy'],
                '%+.2f' % w['ov']['d'] if w['ov'] else '-', f3(p.get('ft')).strip(), f3(p.get('busy')).strip(), f3(p.get('wait')).strip(),
                f3(p['ft'] - p['gtime']).strip() if p.get('ft') and p.get('gtime') else '-',
                ('%+.2f(%.2f)' % (fo['d'], fo['sem'])) if fo else '-', tag))
    arms = [('stamps ON', [w for w in W if w['on'] and w['draws'] >= a.min_draws and w['frames'] > 0]),
            ('stamps OFF (A/B windows)', [w for w in W if not w['on'] and w['draws'] >= a.min_draws])]
    for name, ws in arms:
        if not ws:
            continue
        n = len(ws)
        S = mean([w['span'] for w in ws]); C = mean([w['cov'] for w in ws]); A = mean([w['asy'] for w in ws])
        wall = mean([w['wall'] for w in ws])
        out('')
        out('=== %s: %d windows (draws >= %d) ===' % (name, n, a.min_draws))
        if ws[0]['on']:
            d100 = mean([sum(r['ms'] for r in w['rows'] if not r['asy']) for w in ws])
            a100 = mean([sum(r['ms'] for r in w['rows'] if r['asy']) for w in ws])
            dN, aN = agg_top(ws, a.top)
            out('stamped span S %.2f ms | covered C %.2f | uncovered S-C %.2f | async (not in S) %.2f | wall %.2f' % (S, C, S - C, A, wall))
            out('table sums: log top rows DIRECT %.2f + async %.2f; digest top %d DIRECT %.2f + async %.2f = %.2f "attributed"' %
                (d100, a100, a.top, dN, aN, dN + aN))
            out('bridge: S - attributed %.2f = uncovered %.2f + PSO tail below the cut %.2f (log cut %.2f + digest cut %.2f) - async rows %.2f' %
                (S - dN - aN, S - C, C - dN, C - d100, d100 - dN, aN))
        ov = [w['ov'] for w in ws if w['ov']]
        pms = [w['pm'] for w in ws if w['pm']]
        gqs = [w['gq'] for w in ws if w['gq'] and w['gq']['frames'] > 0]
        out('')
        out('IDLE by cause (ms/frame)')
        if pms:
            out('  PresentMon, %d windows: frame %.2f, GPUBusy %.2f -> GPU idle %.2f = inside the frame (GPUWait) %.2f + between frames '
                '(FT - GPUTime: CPU-late submit / present / flip) %.2f' % (
                    len(pms), mean([p['ft'] for p in pms]), mean([p['busy'] for p in pms]),
                    mean([p['ft'] - p['busy'] for p in pms]), mean([p['wait'] for p in pms]),
                    mean([p['ft'] - p['gtime'] for p in pms])))
        else:
            out('  PresentMon: no frames matched')
        if gqs:
            short = mean([sum(e['ms'] for e in g['edges'] if e['n'] >= 0.9 and e['mean'] <= 150) for g in gqs])
            out('  GQ, %d windows: DIRECT gaps %.3f (%.1f/frame, p50 %.3f p90 %.3f) of which regular short edges (per-submit class, '
                'top edges only) %.3f; gaps around unmeasured submissions %.3f' % (
                    len(gqs), mean([g['gaps'] for g in gqs]), mean([g['ngaps'] for g in gqs]), mean([g['p50'] for g in gqs]),
                    mean([g['p90'] for g in gqs]), short, mean([g['unk'] for g in gqs])))
            ed = {}
            for g in gqs:
                for e in g['edges']:
                    k = (e['a'], e['b'])
                    x = ed.setdefault(k, [0.0, 0.0, 0.0, 0])
                    x[0] += e['ms']; x[1] += e['n']; x[2] = max(x[2], e['mx']); x[3] += 1
            for (pa, pb), (msum, nsum, mx, k) in sorted(ed.items(), key=lambda kv: -kv[1][0])[:8]:
                out('    [%s -> %s] %.3f ms/frame, %.2f/frame, max %.0f us (in %d/%d windows)' % (pa, pb, msum / len(gqs), nsum / len(gqs), mx, k, len(gqs)))
        else:
            out('  GQ: not in this log (GQ = 0): DIRECT gaps vs list heads cannot be separated - see uncovered below')
        if ws[0]['on']:
            out('UNMEASURED work by category (ms/stamped frame)')
            if gqs:
                h = mean([g['heads'] for g in gqs]); tl = mean([g['tails'] for g in gqs]); nc = mean([g['nocov'] for g in gqs])
                gp = mean([g['gaps'] + g['unk'] for g in gqs])
                out('  list HEADS (barriers / transitions / state before the first draw) %.3f, lists without GP2 coverage %.3f, tails %.3f;'
                    ' uncovered %.3f - (gaps + heads + no-cov) %.3f = residual %.3f' % (h, nc, tl, S - C, gp + h + nc, S - C - gp - h - nc))
            else:
                out('  uncovered S - C %.2f = DIRECT gaps + list heads + lists without stamps (GQ splits it)' % (S - C))
            out('  PSO tail below the table cut %.2f (inside covered: real work, small buckets); async-compute %.2f overlaps the DIRECT span'
                % (C - agg_top(ws, a.top)[0], A))
        out('PROFILER overhead')
        if ov:
            k = ov[0]['kind']
            if k == 'sampled':
                out('  wall frame stamped vs unstamped (same windows): %+.2f ms per stamped frame (mean of %d windows; range %+.2f..%+.2f), '
                    '%.0f stamps/stamped frame, hook CPU %.2f ms/stamped frame' % (
                        mean([o['d'] for o in ov]), len(ov), min(o['d'] for o in ov), max(o['d'] for o in ov),
                        mean([o['stamps'] for o in ov]), mean([o['hook'] for o in ov])))
            else:
                out('  A/B windows: wall frame stamps ON %.2f vs OFF %.2f -> %+.2f ms/frame' % (ov[-1]['s'], ov[-1]['u'], ov[-1]['d']))
        fl = [p['fold'] for p in pms if p.get('fold')]
        if fl:
            out('  PresentMon fold (GPUBusy by frame mod %d, %d windows): peak phase - other phases %+.2f ms (mean sem %.2f; '
                'second-highest minus lowest phase %.2f = the no-effect spread)' % (
                    ws[0]['se'], len(fl), mean([x['d'] for x in fl]), mean([x['sem'] for x in fl]), mean([x['spread'] for x in fl])))
    on = [w for w in W if w['on'] and w['pm']]
    off = [w for w in W if not w['on'] and w['pm']]
    if on and off:
        out('')
        out('A/B (stamps ON vs OFF windows, PresentMon): frame %.2f vs %.2f, GPUBusy %.2f vs %.2f -> GP2 GPU cost %+.2f ms/frame' % (
            mean([w['pm']['ft'] for w in on]), mean([w['pm']['ft'] for w in off]), mean([w['pm']['busy'] for w in on]),
            mean([w['pm']['busy'] for w in off]), mean([w['pm']['busy'] for w in on]) - mean([w['pm']['busy'] for w in off])))


SELF_LOG = """[10:00:00.000] GP2: per-PSO attribution on - stamps in 1 frame of 8, intervals >= 4 draws
[10:00:30.000] Tour P pass=1 stage=1/15 key=1 pt=1/3 src=spawn warp=0 pos=0,0,0 mission=1 region=1 dwell=12.0 frames=1 fps=120.0 t0=1 t1=12001 label=Test
[10:00:30.001] GP2: frame 20.00 ms GPU span (wall 19.50 ms); covered 18.00 ms (90 % of span); async-compute 2.00 ms; 300 intervals/frame, 2000 draws+dispatches/frame, PSO stamps ON | dropped: cap 0, rec 0, unresolved 0, heap full 0 | sampled 50 frames (1 in 8), min run 4 draws
[10:00:30.001] GP2: top 3 of 9 (PSO shader set, marker) buckets - hash stage(s) | ms/frame | draws+dispatches/frame | intervals/frame | [marker]
[10:00:30.001] GP2:    1 cs=aa (async compute) |   1.500 ms |     1.0 |     1.0 | [DrawSky]
[10:00:30.001] GP2:    2 cs=bb |   10.000 ms |     1.0 |     1.0 | [-]
[10:00:30.001] GP2:    3 (copy / resolve / clear) |   5.000 ms |     1.0 |     1.0 | [-]
[10:00:30.001] GP2: overhead: wall frame, sampled 20.30 ms (50) vs unsampled 19.40 ms (350) -> +0.90 ms on a sampled frame = +0.11 ms/frame averaged (+0.6 %; x); stamps 300/sampled frame for 2000 draws; hook CPU 0.500 ms/sampled frame (2000 calls, ~250 ns/call, sampled) = 0.1 ms/frame averaged; rest
[10:00:30.002] GQ: frame: 20 DIRECT lists/frame (100 % with ECL info) over 50 sampled frames; span 20.000 ms, lists busy 19.000 ms, GAPS 1.200 ms/frame (20.0 gaps/frame; per frame p50 1.100 p90 1.500 max 2.000 ms), gaps around unmeasured submissions 0.100 ms (0.5/frame, excluded), list HEADS 0.600 ms (start -> first GP2 stamp), tails 0.010 ms, lists without GP2 coverage 0.100 ms | GP2 uncovered (span - covered) 2.000 ms (gaps + heads + no-cov = 2.000) | armed 1000, no head 0, dropped: rec cap 0, ECL table 0, late 0, bad ts 0
[10:00:30.002] GQ: top 2 of 5 gap edges [previous list's last marker -> next list's first marker] | ms/frame | gaps/frame | mean us | max us | x
[10:00:30.002] GQ:   1 [A -> B] | 0.500 ms | 1.00 | 500.0 | 900.0 | 4F9240 < 4F9214
[10:00:30.002] GQ:   2 [C -> D] | 0.400 ms | 4.00 | 100.0 | 150.0 | 4F9240 < 4F9214
"""


def selftest():
    W, se = parse_log(SELF_LOG.splitlines())
    assert se == 8 and len(W) == 1, (se, len(W))
    w = W[0]
    assert w['tour'] and abs(w['t_start'] - (36030.001 - 12.0)) < 1e-6, w['t_start']
    assert len(w['rows']) == 3 and w['rows'][0]['asy'] and w['nb'] == 9
    assert w['ov']['d'] == 0.90 and w['gq']['heads'] == 0.6 and len(w['gq']['edges']) == 2
    # PresentMon: 10:00:18..10:00:30 log-local = 03:00:18..03:00:30 in the CSV (offset +7 h); stamped phase +1 ms GPUBusy
    pm = []
    for i in range(600):
        t = 3 * 3600 + 18 + i * 0.0195
        busy = 19.0 + (1.0 if i % 8 == 3 else 0.0)
        pm.append((t, 19.5, busy, busy + 0.2, 0.2))
    off = find_offset(W, pm)
    assert off and abs(off[1] - 7 * 3600) < 1, off
    lines = []

    class A: pass
    a = A(); a.min_draws = 0; a.top = 3; a.windows = True
    report(W, se, pm, a, lines.append)
    txt = '\n'.join(lines)
    fo = W[0]['pm']['fold']
    assert abs(fo['d'] - 1.0) < 1e-6 and fo['spread'] < 1e-9, fo
    # S 20, C 18, top 3 = DIRECT 10 + 5, async 1.5 -> attributed 16.5; S - attributed 3.5 = uncovered 2 + tail (18 - 15) - async 1.5
    assert 'bridge: S - attributed 3.50 = uncovered 2.00 + PSO tail below the cut 3.00 (log cut 3.00 + digest cut 0.00) - async rows 1.50' in txt, txt
    assert 'regular short edges (per-submit class, top edges only) 0.400' in txt, txt
    assert re.search(r'residual -?0\.000', txt), txt
    print('gpu_unattr SELFTEST: ALL PASSED')


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('log', nargs='?'); ap.add_argument('--pm'); ap.add_argument('--min-draws', type=int, default=0)
    ap.add_argument('--top', type=int, default=80); ap.add_argument('--windows', action='store_true'); ap.add_argument('--selftest', action='store_true')
    ap.add_argument('--out')
    a = ap.parse_args()
    if a.selftest:
        return selftest()
    if not a.log:
        ap.error('LOG required')
    W, se = parse_log(open(a.log, encoding='utf-8', errors='replace'))
    if not W:
        sys.exit('no GP2 windows in %s' % a.log)
    pm = load_pm(a.pm) if a.pm else []
    lines = []
    report(W, se, pm, a, lines.append)
    txt = '\n'.join(lines)
    print(txt)
    if a.out:
        open(a.out, 'w', encoding='utf-8').write(txt + '\n')


if __name__ == '__main__':
    main()
