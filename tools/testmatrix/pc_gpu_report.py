#!/usr/bin/env python3
"""pc_gpu_report.py - analysis for tools\\testmatrix\\pc_gpu_session.ps1 Phase B (PC GPU A/B of the same-look shader / ASI items).

  py -I tools\\testmatrix\\pc_gpu_report.py report <session>\\B [--min-draws 1000] [--out report.txt]
  py -I tools\\testmatrix\\pc_gpu_report.py fake-launch --dir D --launch N --arm X     (used by the session's -DryRun)
  py -I tools\\testmatrix\\pc_gpu_report.py selftest

Input (written by the session): <dir>\\manifest.jsonl, one line per measuring window
  {"launch": 3, "arm": "wdo28", "w": 1, "t0_utc": "...Z", "t1_utc": "...", "t0_local": "HH:MM:SS", "t1_local": ..., "noisy": false}
plus per launch L03_wdo28.csv (PresentMon v2: CPUStartDateTime in UTC, FrameTime, GPUBusy), L03_wdo28.log (mod log copy),
optional L03_wdo28.png (screenshot at the fixed spawn view) and launches.jsonl (one line per launch: ok / why).

Metrics per launch (non-noisy windows only):
  gpu  = mean PresentMon GPUBusy per frame (ms)  <- PRIMARY (direct GPU time, independent of pacing and the fps cap)
  fps  = 1000 / mean FrameTime (FG is off in Phase B by default, so presented = rendered)
  GP2  = per PSO-set ms/frame from the mod's GP2 windows that fall inside [first window start, last window end + 35 s]
         and have >= --min-draws draws+dispatches/frame (gameplay)
Per item launch the BASE value is interpolated from the nearest BASE launch before and after (drift-aware).
Verdict per item (GPU ms; noise floor nf = median |BASE(k+1) - BASE(k)|, at least 0.03 ms):
  KEEP     mean delta <= -nf, every launch delta < 0, shots within noise, (shader items) a new PSO hash was seen
  DROP     mean delta >= +nf, or shotdiff DIFFERENT
  INVALID  shader item without any new PSO hash in GP2 (file not picked up), ASI item without its log line, < 1 good launch
  NEUTRAL  otherwise (the GP2 target-row delta is still printed: small real wins live below the fps noise)
Stdlib + (for shots only) numpy/PIL through shotdiff.py.
"""
import json
import math
import os
import random
import re
import statistics
import subprocess
import sys
from datetime import datetime, timedelta, timezone

HERE = os.path.dirname(os.path.abspath(__file__))
SHOTDIFF = os.path.join(HERE, 'shotdiff.py')

GP2F = re.compile(r'^\[(\d\d):(\d\d):(\d\d)\.(\d+)\] GP2: frame ([\d.]+) ms GPU span \(wall ([\d.]+) ms\); covered ([\d.]+) ms.*?'
                  r'async-compute ([\d.]+) ms; (\d+) intervals/frame, (\d+) draws\+dispatches/frame, PSO stamps (ON|OFF)')
GP2R = re.compile(r'GP2:\s+(\d+) (.*?) \| +([\d.]+) ms \| +([\d.]+) \| +([\d.]+) \| \[(.*)\]\s*$')
OSD = re.compile(r'OSD: stats: rendered ([\d.]+) fps')

# what proves an item ran (ASI items log a line; shader items show a new PSO hash) + known GP2 target hashes (notes)
ASI_PROOF = {'AS0': r'AS0: slot 0 forced on', 'GIM': r'stats/s gimerge: .*merge on: .*, merged (?!0\.00)\d'}
TARGETS = {   # original digest prefixes of the records each item replaces (for the target-row delta); new ones are found as novel hashes
    'fogpar': ['641524ee'], 'wdo14': ['8a013bc7'], 'wdo28': ['32e8deff'], 'gi': ['4b4c3cbe', '6ae59bd0'],
    'toonedge2': ['318fa766'], 'GIM': ['4b4c3cbe', '6ae59bd0'],
    'msv': ['1559714c', '70d0ee9f', 'f311c8e2'],   # meshlet VS (notes/gpu-rewrite-round3.md); GP2 rows read vs=...
}


def utc(s):
    s = s.strip().replace('Z', '+00:00')
    m = re.match(r'(\d+-\d+-\d+[ T]\d+:\d+:\d+)(?:\.(\d+))?(.*)$', s)
    base = datetime.strptime(m.group(1).replace('T', ' '), '%Y-%m-%d %H:%M:%S')
    frac = (m.group(2) or '0')[:6].ljust(6, '0')
    return base.replace(microsecond=int(frac), tzinfo=timezone.utc)


def secs_local(hms):
    h, m, s = [float(x) for x in hms.split(':')]
    return h * 3600 + m * 60 + s


def read_jsonl(p):
    out = []
    if os.path.exists(p):
        for ln in open(p, encoding='utf-8-sig'):
            ln = ln.strip()
            if ln:
                out.append(json.loads(ln))
    return out


def pm_stats(csv_path, wins):
    """mean GPUBusy / FrameTime over the frames inside the given windows (UTC)."""
    if not os.path.exists(csv_path):
        return None
    spans = [(utc(w['t0_utc']), utc(w['t1_utc'])) for w in wins]
    gb, ft = [], []
    with open(csv_path, encoding='utf-8-sig', errors='replace') as f:
        head = f.readline().strip().split(',')
        ix = {k: i for i, k in enumerate(head)}
        ct = ix.get('CPUStartDateTime'); fti = ix.get('FrameTime', ix.get('MsBetweenPresents')); gbi = ix.get('GPUBusy', ix.get('MsGPUBusy'))
        if ct is None or fti is None or gbi is None:
            return None
        for ln in f:
            c = ln.rstrip('\n').split(',')
            if len(c) <= max(ct, fti, gbi):
                continue
            try:
                t = utc(c[ct].replace('-', '-'))
                a, b = float(c[fti]), float(c[gbi])
            except Exception:
                continue
            if any(t0 <= t <= t1 for t0, t1 in spans):
                ft.append(a); gb.append(b)
    if len(ft) < 30:
        return None
    return dict(gpu=statistics.fmean(gb), fps=1000.0 / statistics.fmean(ft), frames=len(ft),
                bound=statistics.fmean(gb) / statistics.fmean(ft))


def gp2_stats(log_path, wins, min_draws):
    """per PSO-set ms/frame over the gameplay GP2 windows that end inside the measuring period (+35 s for the last one)."""
    if not os.path.exists(log_path) or not wins:
        return None
    a = secs_local(wins[0]['t0_local'])
    unwrap = lambda v: v + 86400 if v < a - 3600 else v   # a launch that crosses midnight
    a = min(unwrap(secs_local(w['t0_local'])) for w in wins)
    b = max(unwrap(secs_local(w['t1_local'])) for w in wins) + 35
    cur, wl = None, []
    for ln in open(log_path, encoding='utf-8', errors='replace'):
        m = GP2F.search(ln)
        if m:
            t = unwrap(int(m[1]) * 3600 + int(m[2]) * 60 + int(m[3]))
            cur = dict(t=t, span=float(m[5]), cov=float(m[7]), draws=int(m[10]), on=m[11] == 'ON', rows={})
            wl.append(cur)
            continue
        m = GP2R.search(ln)
        if m and cur is not None:
            k = m[2].strip()
            cur['rows'][k] = cur['rows'].get(k, 0.0) + float(m[3])
    g = [w for w in wl if w['on'] and w['draws'] >= min_draws and a <= w['t'] <= b]
    if not g:
        return None
    n = len(g)
    rows = {}
    for w in g:
        for k, v in w['rows'].items():
            rows[k] = rows.get(k, 0.0) + v / n
    return dict(n=n, span=sum(w['span'] for w in g) / n, cov=sum(w['cov'] for w in g) / n, rows=rows)


def interp(launches, i, key):
    """BASE value for launch index i: mean of the nearest BASE launch before and after that has the metric."""
    vals = []
    for rng in (range(i - 1, -1, -1), range(i + 1, len(launches))):
        for j in rng:
            L = launches[j]
            if L['arm'] == 'BASE' and L.get(key) is not None:
                vals.append((j, L[key])); break
    return vals


def shot_verdict(a, b, noise, out):
    if not (a and b and noise and all(os.path.exists(p) for p in (a, b, noise))):
        return 'no shots'
    try:
        r = subprocess.run([sys.executable, '-I', SHOTDIFF, a, b, '--noise', noise, '--out', out],
                           capture_output=True, text=True, timeout=300)
        m = re.search(r'VERDICT: (.*)', r.stdout)
        return m.group(1).strip() if m else 'shotdiff failed: ' + (r.stderr or r.stdout)[-200:]
    except Exception as e:
        return 'shotdiff error: %s' % e


def report(d, min_draws=1000, out=None, shots=True):
    man = read_jsonl(os.path.join(d, 'manifest.jsonl'))
    lmeta = {L['launch']: L for L in read_jsonl(os.path.join(d, 'launches.jsonl'))}
    by = {}
    for w in man:
        by.setdefault(w['launch'], []).append(w)
    launches = []
    for n in sorted(set(list(by) + list(lmeta))):
        ws = by.get(n, [])
        arm = (ws[0]['arm'] if ws else lmeta[n]['arm'])
        stem = os.path.join(d, 'L%02d_%s' % (n, arm))
        good = [w for w in ws if not w.get('noisy')]
        pm = pm_stats(stem + '.csv', good) if good else None
        gp = gp2_stats(stem + '.log', ws, min_draws) if ws else None
        logt = open(stem + '.log', encoding='utf-8', errors='replace').read() if os.path.exists(stem + '.log') else ''
        L = dict(launch=n, arm=arm, wins=len(ws), noisy=len(ws) - len(good), gpu=pm and pm['gpu'], fps=pm and pm['fps'],
                 frames=pm and pm['frames'], bound=pm and pm['bound'], gp2=gp, shot=stem + '.png' if os.path.exists(stem + '.png') else None,
                 ok=lmeta.get(n, {}).get('ok', bool(ws)), why=lmeta.get(n, {}).get('why', ''), log=logt)
        launches.append(L)
    lines = ['PC GPU A/B report: %s' % d, '']
    lines.append('launch | arm | windows (noisy) | frames | GPU busy ms/frame | fps | GPU-bound share | GP2 windows | GP2 span ms | note')
    for L in launches:
        lines.append('%2d | %-9s | %d (%d) | %s | %s | %s | %s | %s | %s | %s' % (
            L['launch'], L['arm'], L['wins'], L['noisy'], L['frames'] or '-',
            '%.3f' % L['gpu'] if L['gpu'] is not None else '-', '%.1f' % L['fps'] if L['fps'] is not None else '-',
            '%.0f %%' % (100 * L['bound']) if L['bound'] else '-', L['gp2']['n'] if L['gp2'] else 0,
            '%.2f' % L['gp2']['span'] if L['gp2'] else '-', '' if L['ok'] else 'FAILED: ' + L['why']))
    bases = [L for L in launches if L['arm'] == 'BASE' and L['gpu'] is not None]
    diffs = [abs(bases[k + 1]['gpu'] - bases[k]['gpu']) for k in range(len(bases) - 1)]
    nf = max(0.03, statistics.median(diffs)) if diffs else 0.05
    fdiffs = [abs(bases[k + 1]['fps'] - bases[k]['fps']) / bases[k]['fps'] * 100 for k in range(len(bases) - 1)]
    lines += ['', 'BASE launches %d; noise floor (median |BASE(k+1)-BASE(k)|): GPU %.3f ms (used: %.3f), fps %.2f %%' % (
        len(bases), statistics.median(diffs) if diffs else float('nan'), nf, statistics.median(fdiffs) if fdiffs else float('nan')), '']
    base_hashes = set()
    for L in launches:
        if L['arm'] == 'BASE' and L['gp2']:
            base_hashes |= set(L['gp2']['rows'])
    verdicts = []
    for item in dict.fromkeys(L['arm'] for L in launches if L['arm'] != 'BASE'):
        il = [(i, L) for i, L in enumerate(launches) if L['arm'] == item]
        dg, df, gp_t, novel, notes, shotv = [], [], [], set(), [], []
        for i, L in il:
            b = interp(launches, i, 'gpu')
            if L['gpu'] is not None and b:
                bg = statistics.fmean(v for _, v in b)
                bf = statistics.fmean(launches[j]['fps'] for j, _ in b)
                dg.append(L['gpu'] - bg); df.append((L['fps'] - bf) / bf * 100)
            if L['gp2']:
                novel |= set(k for k in L['gp2']['rows'] if k not in base_hashes)
                bj = [j for j, _ in interp(launches, i, 'gp2')]
                if bj:
                    keys = set(L['gp2']['rows'])
                    for j in bj:
                        keys |= set(launches[j]['gp2']['rows'])
                    tk = [k for k in keys if k not in base_hashes or any(t in k for t in TARGETS.get(item, []))]
                    for j in bj:
                        tk += [k for k in launches[j]['gp2']['rows'] if k not in L['gp2']['rows']]   # rows that vanished
                    tk = set(tk)
                    bsum = statistics.fmean(sum(launches[j]['gp2']['rows'].get(k, 0.0) for k in tk) for j in bj)
                    gp_t.append(sum(L['gp2']['rows'].get(k, 0.0) for k in tk) - bsum)
            if shots:
                b0 = [j for j, _ in interp(launches, i, 'shot')]
                if len(b0) == 2:
                    shotv.append(shot_verdict(launches[b0[0]]['shot'], L['shot'], launches[b0[1]]['shot'],
                                              os.path.join(d, 'diff_L%02d_%s.png' % (L['launch'], item))))
        ran = None
        if item in ASI_PROOF:
            ran = all(re.search(ASI_PROOF[item], L['log']) for _, L in il if L['log'])
            if not ran:
                notes.append('log proof missing: /%s/' % ASI_PROOF[item])
        elif any(L['gp2'] for _, L in il):
            ran = bool(novel)
            if not ran:
                notes.append('no new PSO hash in GP2 (replacement not picked up?)')
        mg = statistics.fmean(dg) if dg else None
        bad_shot = any(s.startswith('DIFFERENT') for s in shotv)
        if not dg or ran is False:
            v = 'INVALID'
        elif bad_shot or mg >= nf:
            v = 'DROP'
        elif mg <= -nf and all(x < 0 for x in dg):
            v = 'KEEP'
        else:
            v = 'NEUTRAL'
        msg = '%s: %s  GPU %s ms/frame (launches %s), fps %s %%, GP2 target rows %s ms, new hashes %s, shots [%s]%s' % (
            item, v, '%+.3f' % mg if mg is not None else '-', ', '.join('%+.3f' % x for x in dg) or '-',
            '%+.2f' % statistics.fmean(df) if df else '-', '%+.3f' % statistics.fmean(gp_t) if gp_t else '-',
            ', '.join(sorted(h[:40] for h in novel)[:4]) or '-', '; '.join(shotv) or 'none',
            ('  (' + '; '.join(notes) + ')') if notes else '')
        verdicts.append(msg)
    lines += ['VERDICTS (KEEP = same look and GPU time down beyond the BASE noise floor)'] + verdicts
    txt = '\n'.join(lines) + '\n'
    if out:
        open(out, 'w', encoding='utf-8').write(txt)
    return txt, verdicts


# ---------------------------------------------------------------------------------------------------------------- fakes
FAKE_EFFECT = {'wdo28': (-0.20, 'ps=9945c0635ca06e3267abf943ae64a1cc', 'ps=32e8deff99183945b94bc3be679bfa95'),
               'AS0': (+0.15, None, None), 'esm': (0.0, 'ps=016b302cc61049bbc7aad04479fd3a83', 'ps=c570890a00000000000000000000aaaa')}


def fake_shot(path, launch, arm):
    try:
        import numpy as np
        from PIL import Image
    except Exception:
        return
    r = np.random.default_rng(launch)
    a = np.full((90, 160, 3), 100, np.int16) + r.integers(-3, 4, (90, 160, 3))
    Image.fromarray(np.clip(a, 0, 255).astype(np.uint8)).save(path)


def fake_launch(d, launch, arm, seed=None, shot=False):
    """synthesize the PresentMon csv + mod log for one launch from its manifest windows (dry run / selftest)."""
    rnd = random.Random(seed if seed is not None else launch * 7919)
    ws = [w for w in read_jsonl(os.path.join(d, 'manifest.jsonl')) if w['launch'] == launch]
    eff, newh, oldh = FAKE_EFFECT.get(arm, (0.0, None, None))
    if arm not in ('BASE', 'AS0', 'GIM') and not newh:
        newh = 'ps=fake%s0000000000000000000000' % arm   # every shader item gets a new PSO hash
    stem = os.path.join(d, 'L%02d_%s' % (launch, arm))
    drift = 0.004 * launch
    with open(stem + '.csv', 'w', encoding='utf-8') as f:
        f.write('Application,ProcessID,SwapChainAddress,PresentRuntime,SyncInterval,PresentFlags,AllowsTearing,PresentMode,'
                'CPUStartDateTime,FrameTime,CPUBusy,CPUWait,GPULatency,GPUTime,GPUBusy,GPUWait\n')
        for w in ws:
            t, t1 = utc(w['t0_utc']), utc(w['t1_utc'])
            t = t + timedelta(milliseconds=5)
            while t < t1:
                g = 9.40 + eff + drift + rnd.gauss(0, 0.25)
                ft = g + 0.3 + abs(rnd.gauss(0, 0.2))
                f.write('Nioh3.exe,1,0x1,DXGI,1,0,0,Composed: Flip,%s,%.4f,4.0,1.0,0.3,%.4f,%.4f,0.1\n' % (
                    t.strftime('%Y-%m-%d %H:%M:%S.%f'), ft, g + 0.1, g))
                t += timedelta(milliseconds=ft)
    with open(stem + '.log', 'w', encoding='utf-8') as f:
        f.write('[00:00:00.000] Nioh3EngineFix fake log for %s\n' % arm)
        if arm == 'AS0':
            f.write('[00:00:01.000] AS0: slot 0 forced on\n')
        if arm == 'GIM':
            f.write('[00:00:01.000] stats/s gimerge: pairs ok 99 bad 0 (window +30/+0) why: | merge on: armed rp121=1(clean 60), '
                    'merged 2.00/frame (12 groups skipped/frame), replays 0.00/frame\n')
        if ws:
            a = secs_local(ws[0]['t0_local'])
            for k in range(3):
                s = a + 5 + 25 * k
                ts = '[%02d:%02d:%06.3f]' % (int(s // 3600) % 24, int(s % 3600 // 60), s % 60)
                f.write('%s GP2: frame %.2f ms GPU span (wall 9.6 ms); covered 9.0 ms (94 %% of span); async-compute 0.50 ms; '
                        '500 intervals/frame, 2400 draws+dispatches/frame, PSO stamps ON | sampled 100 frames (1 in 8)\n' % (ts, 9.6 + eff))
                f.write('%s GP2: top 100 of 300 (PSO shader set, marker) buckets - hash stage(s) | ms/frame | draws+dispatches/frame | intervals/frame | [marker]\n' % ts)
                f.write('%s GP2:    1 vs=aaaa ps=bbbb |   1.500 ms |    31.0 |    10.0 | [-]\n' % ts)
                h = newh if newh else (oldh or 'ps=32e8deff99183945b94bc3be679bfa95')
                ms = 0.367 + (eff if newh else 0.0)
                f.write('%s GP2:    2 vs=d21b4b07 %s |   %.3f ms |     3.0 |     3.0 | [-]\n' % (ts, h, ms))
    if shot:
        fake_shot(stem + '.png', launch, arm)
    return stem


def selftest():
    import tempfile
    d = tempfile.mkdtemp(prefix='pcgpu_selftest_')
    arms = ['BASE', 'wdo28', 'AS0', 'BASE', 'AS0', 'wdo28', 'BASE', 'esm', 'BASE', 'esm', 'BASE']
    t = datetime(2026, 10, 9, 23, 50, 0, tzinfo=timezone.utc)
    loc = 23 * 3600 + 50 * 60
    with open(os.path.join(d, 'manifest.jsonl'), 'w') as mf, open(os.path.join(d, 'launches.jsonl'), 'w') as lf:
        for n, arm in enumerate(arms, 1):
            lf.write(json.dumps(dict(launch=n, arm=arm, ok=True, why='')) + '\n')
            for w in range(1, 4):
                t0, t1 = t, t + timedelta(seconds=20)
                l0 = loc
                mf.write(json.dumps(dict(launch=n, arm=arm, w=w, t0_utc=t0.isoformat().replace('+00:00', 'Z'),
                                         t1_utc=t1.isoformat().replace('+00:00', 'Z'),
                                         t0_local='%02d:%02d:%02d' % (l0 // 3600 % 24, l0 % 3600 // 60, l0 % 60),
                                         t1_local='%02d:%02d:%02d' % ((l0 + 20) // 3600 % 24, (l0 + 20) % 3600 // 60, (l0 + 20) % 60),
                                         noisy=False)) + '\n')
                t += timedelta(seconds=25); loc += 25
            t += timedelta(seconds=200); loc += 200
    for n, arm in enumerate(arms, 1):
        fake_launch(d, n, arm)
    txt, v = report(d, shots=False)
    want = {'wdo28': 'KEEP', 'AS0': 'DROP', 'esm': 'NEUTRAL'}
    got = {x.split(':')[0]: x.split(':')[1].split()[0] for x in v}
    fails = [k for k in want if got.get(k) != want[k]]
    if fails:
        print(txt)
        print('PC_GPU_REPORT SELFTEST: FAILED %s (got %s)' % (fails, got))
        return 1
    print('PC_GPU_REPORT SELFTEST: ALL PASSED (wdo28 KEEP, AS0 DROP, esm NEUTRAL; drift-interpolated BASE, GP2 novel hash, midnight wrap)')
    return 0


def main(argv):
    if not argv or argv[0] in ('-h', '--help'):
        print(__doc__); return 0
    cmd = argv[0]
    if cmd == 'selftest':
        return selftest()
    if cmd == 'fake-launch':
        a = dict(zip(argv[1::2], argv[2::2]))
        fake_launch(a['--dir'], int(a['--launch']), a['--arm'], shot=a.get('--shot') == '1')
        return 0
    if cmd == 'report':
        d = argv[1]
        a = dict(zip(argv[2::2], argv[3::2]))
        txt, _ = report(d, int(a.get('--min-draws', 1000)), a.get('--out'), shots='--no-shots' not in argv)
        print(txt)
        return 0
    print(__doc__); return 2


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
