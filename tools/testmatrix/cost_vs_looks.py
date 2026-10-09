#!/usr/bin/env python3
"""cost_vs_looks.py - turn a cost_vs_looks.ps1 session folder into the COST vs LOOKS table (notes/cost-vs-looks.md).

  py -I tools\\testmatrix\\cost_vs_looks.py report <session dir> [--no-diff] [--thresh 8]
  py -I tools\\testmatrix\\cost_vs_looks.py selftest

Inputs in <session dir>: manifest.jsonl (one row per condition per tour point: cond, t0_utc/t1_utc, shot, skipped counts),
presentmon.csv (PresentMon v2 --date_time: CPUStartDateTime UTC, GPUBusy), cvl_groups.json (labels + pre-filled expectations),
shots/*.png. Outputs: cvl_report.md (table + ranked list + per-point detail), cvl_table.csv, diffs/*.png (x8 abs diff vs BASE).

Per point:
  cost  = BASE GPUBusy interpolated to the condition's slot (BASE runs at the start, middle and end of the cycle) - condition GPUBusy
          (mean ms/frame over the measuring window); noise = spread of the point's BASE windows.
  looks = condition shot vs the nearest BASE shot: mean abs diff (0-255), % pixels whose max channel differs by > --thresh, SSIM
          (grey, 8x8 box windows); the BASE-vs-BASE values of the same point are the noise floor (TAA jitter, wind, idle animation);
          'excess %' = % changed minus that floor.
  present = the mod's ack counted skipped draws/dispatches at that point (0 = the effect is not in that scene: the row is left out).
Verdicts (averaged over the points where the effect is present):
  WASTE        ms >= 0.15 and > 2x noise, excess < 1 % and SSIM within 0.01 of BASE-vs-BASE   -> candidate optional toggle / cheaper rewrite first
  REWRITE      ms >= 0.15, excess < 5 %                                    -> candidate cheaper rewrite (small visible change)
  KEEP-visible ms >= 0.15, excess >= 5 %                                   -> keep; only a same-look rewrite helps
  KEEP-cheap   saving below max(0.1 ms, 2x noise)
  COST-REF     structural groups (geometry, culling, FSR, compose, UI): their row is a price tag, not a removal candidate
  stale-risk groups (compute producers): a skipped compute pass keeps its last output, so a static view can look unchanged although the
  effect is gone - their 'looks' column is a lower bound; judge from the diff image / the *_all group that also skips the consumer.
"""
import csv
import json
import math
import os
import sys
import tempfile
from datetime import datetime, timezone

try:
    import numpy as np
    from PIL import Image
except Exception:  # pragma: no cover - report still runs without images
    np = None
    Image = None

COST_REF = {"PB2_be0f", "PB2_2c6f", "PB2_meshlet", "STD_9669", "MPC", "MIC", "MshLod", "FSR3", "PostCompose", "ExeUI", "SKY_all"}


def utc_epoch(s):
    s = s.strip().replace('T', ' ').replace('Z', '')
    if '+' in s[10:]:
        s = s[:10] + s[10:].split('+')[0]
    main, _, frac = s.partition('.')
    dt = datetime.strptime(main, '%Y-%m-%d %H:%M:%S').replace(tzinfo=timezone.utc)
    return dt.timestamp() + (float('0.' + frac) if frac else 0.0)


def read_jsonl(p):
    out = []
    if os.path.exists(p):
        for ln in open(p, encoding='utf-8-sig'):
            ln = ln.strip()
            if ln:
                out.append(json.loads(ln))
    return out


def read_pm(p):
    """-> sorted list of (t, gpubusy, frametime)."""
    rows = []
    if not os.path.exists(p):
        return rows
    with open(p, encoding='utf-8-sig', errors='replace') as f:
        head = f.readline().strip().split(',')
        ix = {k: i for i, k in enumerate(head)}
        ct = ix.get('CPUStartDateTime')
        gb = ix.get('GPUBusy', ix.get('MsGPUBusy'))
        ft = ix.get('FrameTime', ix.get('MsBetweenPresents'))
        if ct is None or gb is None:
            return rows
        for ln in f:
            c = ln.rstrip('\n').split(',')
            if len(c) <= max(ct, gb, ft or 0):
                continue
            try:
                rows.append((utc_epoch(c[ct]), float(c[gb]), float(c[ft]) if ft is not None else float('nan')))
            except Exception:
                continue
    rows.sort()
    return rows


def align_pm(pm, spans, label='presentmon'):
    """Shift the (t, ...) rows in place by the auto-detected PresentMon clock offset (see pm_time.py)."""
    sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
    import pm_time
    off = pm_time.detect_offset([r[0] for r in pm], spans, label)
    if off:
        pm[:] = [(r[0] + off,) + tuple(r[1:]) for r in pm]
    window.src = None
    return off


def window(pm, t0, t1):
    import bisect
    ts = window.ts if getattr(window, 'src', None) is pm else None
    if ts is None:
        window.ts = ts = [r[0] for r in pm]
        window.src = pm
    a, b = bisect.bisect_left(ts, t0), bisect.bisect_right(ts, t1)
    seg = pm[a:b]
    if len(seg) < 10:
        return None
    g = sum(r[1] for r in seg) / len(seg)
    f = sum(r[2] for r in seg) / len(seg)
    return dict(gpu=g, fps=1000.0 / f if f > 0 else float('nan'), frames=len(seg))


# ---- images ----------------------------------------------------------------------------------------------------------------------
def load_img(p):
    if Image is None or not p or not os.path.exists(p):
        return None
    a = np.asarray(Image.open(p).convert('RGB'), dtype=np.float32)
    return a


def box(x, k):
    c = np.cumsum(np.cumsum(np.pad(x, ((1, 0), (1, 0))), 0), 1)
    return (c[k:, k:] - c[:-k, k:] - c[k:, :-k] + c[:-k, :-k]) / (k * k)


def ssim(a, b, k=8):
    ga = a @ np.array([0.299, 0.587, 0.114], dtype=np.float32)
    gb = b @ np.array([0.299, 0.587, 0.114], dtype=np.float32)
    if ga.shape[0] > 720:   # 2x downsample: faster, and TAA sub-pixel jitter matters less
        h, w = ga.shape[0] // 2 * 2, ga.shape[1] // 2 * 2
        ga = ga[:h, :w].reshape(h // 2, 2, w // 2, 2).mean((1, 3))
        gb = gb[:h, :w].reshape(h // 2, 2, w // 2, 2).mean((1, 3))
    c1, c2 = (0.01 * 255) ** 2, (0.03 * 255) ** 2
    ma, mb = box(ga, k), box(gb, k)
    va = box(ga * ga, k) - ma * ma
    vb = box(gb * gb, k) - mb * mb
    cov = box(ga * gb, k) - ma * mb
    s = ((2 * ma * mb + c1) * (2 * cov + c2)) / ((ma * ma + mb * mb + c1) * (va + vb + c2))
    return float(s.mean())


def compare(a, b, thresh):
    if a is None or b is None or a.shape != b.shape:
        return None
    d = np.abs(a - b)
    px = d.max(axis=2)
    return dict(mad=float(d.mean()), pct=float((px > thresh).mean() * 100.0), ssim=ssim(a, b), diff=d)


# ---- report ----------------------------------------------------------------------------------------------------------------------
def fnum(v, nd=2, dash='-'):
    return dash if v is None or (isinstance(v, float) and math.isnan(v)) else ('%.*f' % (nd, v))


def report(d, no_diff=False, thresh=8.0):
    rows = read_jsonl(os.path.join(d, 'manifest.jsonl'))
    if not rows:
        print('no manifest rows in', d)
        return 1
    gpath = os.path.join(d, 'cvl_groups.json')
    if not os.path.exists(gpath):
        gpath = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'cvl_groups.json')
    groups = {g['id']: g for g in json.load(open(gpath, encoding='utf-8-sig'))['groups']}
    order = list(groups)
    # the harness's own PresentMon (presentmon_cvl_<n>.csv, restarted if it stops) first; run_tour's presentmon.csv as fallback
    own = sorted(f for f in os.listdir(d) if f.startswith('presentmon_cvl_') and f.endswith('.csv'))
    pm = sorted(sum((read_pm(os.path.join(d, f)) for f in own), [])) if own else []
    if not pm:
        pm = read_pm(os.path.join(d, 'presentmon.csv'))
    if not pm:
        print('WARNING: no presentmon.csv frames - cost columns empty')
    if pm:
        align_pm(pm, [(utc_epoch(r['t0_utc']), utc_epoch(r['t1_utc'])) for r in rows], os.path.basename(os.path.normpath(d)))
    ddir = os.path.join(d, 'diffs')
    if not no_diff and np is not None:
        os.makedirs(ddir, exist_ok=True)

    per = {}   # cond -> list of per-point dicts
    detail = []
    points = sorted({r['point'] for r in rows})
    for p in points:
        pr = sorted([r for r in rows if r['point'] == p], key=lambda r: r['idx'])
        for r in pr:
            r['ok'] = bool(r.get('acked', True)) and not r.get('dwell_ended', False)
            w = window(pm, utc_epoch(r['t0_utc']), utc_epoch(r['t1_utc'])) if pm else None
            r['gpu'] = w['gpu'] if w else None
            r['fps'] = w['fps'] if w else None
            r['img'] = load_img(os.path.join(d, r['shot'])) if r.get('shot') else None
        bases = [r for r in pr if r['cond'] == 'BASE' and r['ok'] and r['gpu'] is not None]
        bimgs = [r for r in pr if r['cond'] == 'BASE' and r['ok'] and r['img'] is not None]
        noise_ms = (max(b['gpu'] for b in bases) - min(b['gpu'] for b in bases)) if len(bases) >= 2 else None
        nf = [compare(bimgs[i]['img'], bimgs[i + 1]['img'], thresh) for i in range(len(bimgs) - 1)]
        nf = [x for x in nf if x]
        noise_pct = sum(x['pct'] for x in nf) / len(nf) if nf else 0.0
        noise_ssim = min(x['ssim'] for x in nf) if nf else None
        detail.append(('BASE', p, None, noise_ms, noise_pct, noise_ssim, len(bases)))
        for r in pr:
            if r['cond'] == 'BASE' or not r['ok']:
                continue
            prev = [b for b in bases if b['idx'] < r['idx']]
            nxt = [b for b in bases if b['idx'] > r['idx']]
            base = None
            if prev and nxt:
                b0, b1 = prev[-1], nxt[0]
                f = (r['idx'] - b0['idx']) / float(b1['idx'] - b0['idx'])
                base = b0['gpu'] + f * (b1['gpu'] - b0['gpu'])
            elif prev or nxt:
                base = (prev[-1] if prev else nxt[0])['gpu']
            saved = (base - r['gpu']) if (base is not None and r['gpu'] is not None) else None
            cands = sorted(bimgs, key=lambda b: abs(b['idx'] - r['idx']))
            cmp = compare(cands[0]['img'], r['img'], thresh) if cands and r['img'] is not None else None
            if cmp is not None and not no_diff:
                Image.fromarray(np.clip(cmp['diff'] * 8, 0, 255).astype(np.uint8)).save(os.path.join(ddir, r['tag'] + '_x8.png'))
            skipped = sum(max(0, int(r.get(k, 0) or 0)) for k in ('skipped_draws', 'skipped_dispatches', 'skipped_ei', 'skipped_mesh'))
            ackless = all(int(r.get(k, -1)) < 0 for k in ('skipped_draws', 'skipped_dispatches'))
            present = skipped > 0 or ackless
            ent = dict(point=p, saved=saved, gpu=r['gpu'], base=base, noise_ms=noise_ms, present=present, skipped=skipped,
                       mad=cmp['mad'] if cmp else None, pct=cmp['pct'] if cmp else None, ssim=cmp['ssim'] if cmp else None,
                       excess=(cmp['pct'] - noise_pct) if cmp else None, shot=r.get('shot', ''), tag=r['tag'],
                       drop=(noise_ssim - cmp['ssim']) if (cmp and noise_ssim is not None) else None)
            per.setdefault(r['cond'], []).append(ent)
            detail.append((r['cond'], p, saved, noise_ms, cmp['pct'] if cmp else None, cmp['ssim'] if cmp else None, skipped))

    def mean(v):
        v = [x for x in v if x is not None]
        return sum(v) / len(v) if v else None

    table = []
    for cid in order + [c for c in per if c not in groups]:
        if cid not in per:
            continue
        g = groups.get(cid, {})
        es = per[cid]
        pres = [e for e in es if e['present']]
        use = pres if pres else []
        ms = mean([e['saved'] for e in use])
        noise = mean([e['noise_ms'] for e in use])
        pct = mean([e['pct'] for e in use])
        exc = mean([e['excess'] for e in use])
        ss = [e['ssim'] for e in use if e['ssim'] is not None]
        smin = min(ss) if ss else None
        drops = [e['drop'] for e in use if e.get('drop') is not None]
        dmax = max(drops) if drops else None
        if not pres:
            verdict = 'ABSENT (not drawn at these points)'
        elif cid in COST_REF:
            verdict = 'COST-REF (price tag, not removable)'
        elif ms is None:
            verdict = '? (no PresentMon frames)'
        elif ms < max(0.1, 2 * (noise or 0)):
            verdict = 'KEEP-cheap'
        elif ms >= 0.15 and exc is not None and exc < 1.0 and (dmax is None or dmax <= 0.01):
            verdict = 'WASTE: optional toggle / cheaper rewrite' + (' (stale-risk: check diff)' if g.get('stale_risk') else '')
        elif ms >= 0.15 and exc is not None and exc < 5.0:
            verdict = 'REWRITE candidate (small visible change)'
        elif exc is None:
            verdict = 'cost only (no shots)'
        else:
            verdict = 'KEEP-visible (same-look rewrite only)'
        score = (ms / ((exc if exc and exc > 0 else 0) + 0.5)) if (ms is not None and pres and cid not in COST_REF) else None
        table.append(dict(id=cid, label=g.get('label', cid), ms=ms, noise=noise, gp2=g.get('gp2_ms'), pct=pct, excess=exc, ssim=smin, drop=dmax,
                          present='%d/%d' % (len(pres), len(es)), looks=g.get('expect_looks', ''), verdict=verdict,
                          repl=g.get('replacement', ''), repl_ms=g.get('repl_ms'), repl_risk=g.get('repl_risk', ''), score=score,
                          stale=bool(g.get('stale_risk')), shot=(pres[0]['tag'] if pres else '')))

    with open(os.path.join(d, 'cvl_table.csv'), 'w', newline='', encoding='utf-8') as f:
        w = csv.writer(f)
        w.writerow(['id', 'ms_saved', 'noise_ms', 'gp2_ms_125', 'pct_changed', 'excess_pct', 'ssim_min', 'present', 'verdict', 'waste_score',
                    'stale_risk', 'replacement', 'repl_est_ms', 'repl_visual_risk', 'label'])
        for t in table:
            w.writerow([t['id'], fnum(t['ms'], 3), fnum(t['noise'], 3), t['gp2'], fnum(t['pct'], 2), fnum(t['excess'], 2), fnum(t['ssim'], 4),
                        t['present'], t['verdict'], fnum(t['score'], 3), t['stale'], t['repl'], t['repl_ms'], t['repl_risk'], t['label']])

    bl = [x for x in detail if x[0] == 'BASE']
    L = []
    L.append('# Cost vs looks - %s' % os.path.basename(os.path.abspath(d)))
    L.append('')
    L.append('Points: %d; BASE noise per point (GPUBusy spread ms / %% px changed BASE-vs-BASE / SSIM): %s' % (
        len(points), '; '.join('P%d %s / %s %% / %s' % (x[1], fnum(x[3]), fnum(x[4], 1), fnum(x[5], 3)) for x in bl)))
    L.append('ms saved = BASE GPUBusy (PresentMon, interpolated) minus skipped; %% px = pixels whose max channel moved > %g/255 vs the nearest '
             'BASE shot; excess = %% px above the BASE-vs-BASE floor; SSIM = worst point. stale = compute producer (looks is a lower bound).' % thresh)
    L.append('')
    L.append('| effect | ms saved | GP2 #125 | % px changed (excess) | SSIM | what it looks like | verdict | replacement idea | est. ms | visual risk |')
    L.append('|---|---|---|---|---|---|---|---|---|---|')
    for t in table:
        look = t['looks'] + (' **[stale-risk]**' if t['stale'] else '') + (' (shot %s, diff diffs/%s_x8.png)' % (t['shot'], t['shot']) if t['shot'] else '')
        L.append('| %s | %s (+-%s) | %s | %s (%s) | %s | %s | %s | %s | %s | %s |' % (
            t['id'], fnum(t['ms']), fnum(t['noise']), fnum(t['gp2']) if isinstance(t['gp2'], (int, float)) else '-', fnum(t['pct'], 1),
            fnum(t['excess'], 1), fnum(t['ssim'], 3) + (' (-%s)' % fnum(t['drop'], 3) if t.get('drop') is not None else ''), look.replace('|', '/'), t['verdict'] + ' [%s pts]' % t['present'],
            str(t['repl']).replace('|', '/'), fnum(t['repl_ms']) if isinstance(t['repl_ms'], (int, float)) else '-', t['repl_risk']))
    L.append('')
    L.append('## Ranked: most ms per unit of visible change (removal / rewrite candidates first; COST-REF and absent rows left out)')
    rk = sorted([t for t in table if t['score'] is not None], key=lambda t: -t['score'])
    for i, t in enumerate(rk, 1):
        L.append('%d. **%s** - %s ms for %s %% excess px (SSIM %s): %s. Replacement: %s (%s ms, risk %s)' % (
            i, t['id'], fnum(t['ms']), fnum(t['excess'], 1), fnum(t['ssim'], 3), t['verdict'], t['repl'],
            fnum(t['repl_ms']) if isinstance(t['repl_ms'], (int, float)) else '?', t['repl_risk']))
    L.append('')
    L.append('## Cost references (structural; not removable)')
    for t in table:
        if t['verdict'].startswith('COST-REF'):
            L.append('- %s: %s ms (GP2 #125 %s) - %s' % (t['id'], fnum(t['ms']), t['gp2'], t['label']))
    L.append('')
    L.append('## Per point')
    L.append('| cond | point | ms saved | BASE noise ms | % px | SSIM | skipped calls |')
    L.append('|---|---|---|---|---|---|---|')
    for x in detail:
        L.append('| %s | %s | %s | %s | %s | %s | %s |' % (x[0], x[1], fnum(x[2]), fnum(x[3]), fnum(x[4], 1), fnum(x[5], 3), x[6]))
    open(os.path.join(d, 'cvl_report.md'), 'w', encoding='utf-8').write('\n'.join(L) + '\n')
    waste = [t['id'] for t in rk if t['verdict'].startswith('WASTE')]
    print('cvl report: %d effects, %d points; WASTE: %s; top by score: %s -> %s' % (
        len(table), len(points), ', '.join(waste) or 'none', ', '.join(t['id'] for t in rk[:5]), os.path.join(d, 'cvl_report.md')))
    return 0


def selftest():
    if np is None:
        print('selftest needs numpy + PIL')
        return 1
    d = tempfile.mkdtemp(prefix='cvl_selftest_')
    os.makedirs(os.path.join(d, 'shots'))
    groups = {'groups': [
        {'id': 'BIGWASTE', 'label': 'costly + invisible', 'gp2_ms': 1.0, 'stale_risk': False},
        {'id': 'VISIBLE', 'label': 'costly + visible', 'gp2_ms': 1.0, 'stale_risk': False},
        {'id': 'CHEAP', 'label': 'cheap', 'gp2_ms': 0.02, 'stale_risk': False},
        {'id': 'GONE', 'label': 'absent', 'gp2_ms': 0.5, 'stale_risk': False}]}
    json.dump(groups, open(os.path.join(d, 'cvl_groups.json'), 'w'))
    rng = np.random.default_rng(3)
    scene = (rng.random((180, 320, 3)) * 200).astype(np.uint8)
    t = datetime(2026, 10, 9, 12, 0, 0, tzinfo=timezone.utc).timestamp()
    plan = ['BASE', 'BIGWASTE', 'VISIBLE', 'BASE', 'CHEAP', 'GONE', 'BASE']
    cost = {'BIGWASTE': 1.0, 'VISIBLE': 1.0, 'CHEAP': 0.02, 'GONE': 0.0}
    man, pmrows = [], []
    for p in (1, 2):
        for i, c in enumerate(plan, 1):
            img = scene.copy()
            img[rng.integers(0, 180, 30), rng.integers(0, 320, 30)] = 255   # noise
            if c == 'VISIBLE':
                img[0:90] = 0
            tag = '%d_%02d_%s' % (p, i, c)
            Image.fromarray(img).save(os.path.join(d, 'shots', tag + '.png'))
            t0, t1 = t + 2, t + 5
            man.append(dict(point=p, idx=i, cond=c, tag=tag, shot='shots/%s.png' % tag, acked=True, dwell_ended=False,
                            t0_utc=datetime.fromtimestamp(t0, timezone.utc).strftime('%Y-%m-%dT%H:%M:%S.%fZ')[:-4] + 'Z',
                            t1_utc=datetime.fromtimestamp(t1, timezone.utc).strftime('%Y-%m-%dT%H:%M:%S.%fZ')[:-4] + 'Z',
                            skipped_draws=0 if c in ('GONE', 'BASE') else 50, skipped_dispatches=0, skipped_ei=0, skipped_mesh=0))
            for k in range(120):
                tt = t0 + k * 0.025
                g = 23.0 - cost.get(c, 0) + float(rng.normal(0, 0.05))
                pmrows.append('%s,25.0,%.3f' % (datetime.fromtimestamp(tt, timezone.utc).strftime('%Y-%m-%d %H:%M:%S.%f')[:-3], g))
            t += 6
    with open(os.path.join(d, 'manifest.jsonl'), 'w') as f:
        for m in man:
            f.write(json.dumps(m) + '\n')
    with open(os.path.join(d, 'presentmon.csv'), 'w') as f:
        f.write('CPUStartDateTime,FrameTime,GPUBusy\n' + '\n'.join(pmrows) + '\n')
    report(d)
    rows = {r['id']: r for r in csv.DictReader(open(os.path.join(d, 'cvl_table.csv'), encoding='utf-8'))}
    ok = (rows['BIGWASTE']['verdict'].startswith('WASTE') and rows['VISIBLE']['verdict'].startswith('KEEP-visible')
          and rows['CHEAP']['verdict'] == 'KEEP-cheap' and rows['GONE']['verdict'].startswith('ABSENT')
          and abs(float(rows['BIGWASTE']['ms_saved']) - 1.0) < 0.1)
    print('SELFTEST', 'OK' if ok else 'FAILED', {k: (v['verdict'], v['ms_saved'], v['excess_pct']) for k, v in rows.items()})
    return 0 if ok else 1


def main(argv):
    if len(argv) >= 2 and argv[1] == 'selftest':
        return selftest()
    if len(argv) >= 3 and argv[1] == 'report':
        thresh = 8.0
        if '--thresh' in argv:
            thresh = float(argv[argv.index('--thresh') + 1])
        return report(argv[2], no_diff='--no-diff' in argv, thresh=thresh)
    print(__doc__)
    return 2


if __name__ == '__main__':
    sys.exit(main(sys.argv))
