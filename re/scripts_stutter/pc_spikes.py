# pc_spikes.py - spike census of the PC PresentMon captures (captures\*.csv).
#   py -I re\scripts_stutter\pc_spikes.py [captures] [glob]
# Spike = MsBetweenPresents > 2 x the median of the surrounding 2 s. Each spike is classified by which side grew:
#   CPU  : MsCPUBusy (app CPU start -> Present) > 1.5 x its local median and > 60 % of the frame
#   GPU  : MsGPUBusy > 1.5 x its local median and > 60 % of the frame
#   WAIT : neither busy figure grew (frame time spent in MsCPUWait / present queue: a stall outside the measured work,
#          e.g. a blocking call before the CPU start of the frame, OS paging, or the frame limiter)
# FG captures (two presents per rendered frame) are skipped unless named explicitly.
import csv, glob, os, sys, statistics, bisect

D = sys.argv[1] if len(sys.argv) > 1 else os.path.join(os.path.dirname(__file__), '..', '..', 'captures')
PAT = sys.argv[2] if len(sys.argv) > 2 else 'bench-*.csv'

def f(x):
    try:
        return float(x)
    except (TypeError, ValueError):
        return float('nan')

def pct(a, p):
    a = sorted(a)
    k = (len(a) - 1) * p / 100; i = int(k); j = min(i + 1, len(a) - 1)
    return a[i] + (a[j] - a[i]) * (k - i)

tot = {'CPU': 0, 'GPU': 0, 'WAIT': 0}
allsp = []
print(f'{"capture":34s} {"frames":>6s} {"med":>6s} {"p99":>6s} {"p99.9":>6s} {"max":>7s} {"spk":>4s} {"/min":>5s}  CPU GPU WAIT  worst spikes (ms: kind cpu/gpu/wait)')
for p in sorted(glob.glob(os.path.join(D, PAT))):
    name = os.path.basename(p)
    if 'fg' in name.lower() and PAT == 'bench-*.csv':
        continue
    with open(p, newline='', encoding='utf-8-sig') as fh:
        rows = [r for r in csv.DictReader(fh) if r.get('Application', '').lower().startswith('nioh3')]
    if len(rows) < 200:
        continue
    t = [f(r['TimeInMs']) for r in rows]
    ft = [f(r['MsBetweenPresents']) for r in rows]
    cb = [f(r.get('MsCPUBusy')) for r in rows]
    cw = [f(r.get('MsCPUWait')) for r in rows]
    gb = [f(r.get('MsGPUBusy')) for r in rows]
    ok = [i for i in range(len(rows)) if ft[i] == ft[i]]
    spikes = []
    for i in ok:
        lo = bisect.bisect_left(t, t[i] - 1000); hi = bisect.bisect_right(t, t[i] + 1000)
        win = [ft[k] for k in range(lo, hi) if ft[k] == ft[k]]
        med = statistics.median(win)
        if ft[i] > 2 * med and ft[i] > 8:
            mcb = statistics.median([cb[k] for k in range(lo, hi) if cb[k] == cb[k]] or [0])
            mgb = statistics.median([gb[k] for k in range(lo, hi) if gb[k] == gb[k]] or [0])
            if cb[i] > 1.5 * mcb and cb[i] > 0.6 * ft[i]:
                kind = 'CPU'
            elif gb[i] > 1.5 * mgb and gb[i] > 0.6 * ft[i]:
                kind = 'GPU'
            else:
                kind = 'WAIT'
            spikes.append((ft[i], kind, cb[i], gb[i], cw[i], med, t[i]))
    dur_min = (t[-1] - t[0]) / 60000
    k = {x: sum(1 for s in spikes if s[1] == x) for x in tot}
    for x in tot:
        tot[x] += k[x]
    allsp += [(name,) + s for s in spikes]
    fv = [ft[i] for i in ok]
    worst = sorted(spikes, key=lambda s: -s[0])[:3]
    ws = ' '.join(f'{s[0]:.0f}:{s[1]}{s[2]:.0f}/{s[3]:.0f}/{s[4]:.0f}' for s in worst)
    print(f'{name:34s} {len(fv):6d} {pct(fv,50):6.2f} {pct(fv,99):6.2f} {pct(fv,99.9):6.2f} {max(fv):7.1f} {len(spikes):4d} {len(spikes)/max(dur_min,1e-9):5.1f}  {k["CPU"]:3d} {k["GPU"]:3d} {k["WAIT"]:4d}  {ws}')
print('total spikes by kind:', tot)
if allsp:
    sz = sorted(s[1] for s in allsp)
    print(f'spike size: median {pct(sz,50):.1f} ms, p90 {pct(sz,90):.1f} ms, max {sz[-1]:.1f} ms; ratio to local median: median {pct([s[1]/s[6] for s in allsp],50):.2f}')
    # spacing: are spikes periodic? (autosave / timers)
    by = {}
    for s in allsp:
        by.setdefault(s[0], []).append(s[7])
    gaps = []
    for n, ts in by.items():
        ts.sort()
        gaps += [b - a for a, b in zip(ts, ts[1:])]
    if gaps:
        print(f'gaps between spikes in one capture: median {pct(gaps,50)/1000:.1f} s, p10 {pct(gaps,10)/1000:.2f} s, p90 {pct(gaps,90)/1000:.1f} s')
