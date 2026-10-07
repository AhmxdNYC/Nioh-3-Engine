# load_phases.py - per-load report for the Ally test plan in notes\stutter.md section 5.
#   py -I re\scripts_stutter\load_phases.py <mangohud.csv> <modlog.log> [offset_s=0]
# For every level-load window (mod log "engine stop flags ... now: 58A" -> "now: none") prints: duration; swap, RAM and
# GPU-memory change; frames > 66.7 / 100 / 200 ms; p99 and max; the same for the last 20 s of the window (phase C); the
# swap-burst phase (largest 10 s swap rise in the window, phase A); and how long after the flag clears it takes until
# 60 consecutive frames are < 66.7 ms. Works with log_interval=0 (every frame) and with the 10 Hz logs (de-duplicated).
import csv, re, sys, bisect

if len(sys.argv) < 3:
    sys.exit(__doc__ if __doc__ else 'usage: load_phases.py <csv> <log> [offset]')
CSV, LOG = sys.argv[1], sys.argv[2]
OFF = float(sys.argv[3]) if len(sys.argv) > 3 else 0.0
TS = re.compile(r'^\[(\d\d):(\d\d):(\d\d)\.(\d\d\d)\]\s(.*)$')

rows = []
with open(CSV, newline='') as f:
    hdr = None
    for r in csv.reader(f):
        if hdr is None:
            if r and r[0] == 'fps':
                hdr = r
            continue
        if len(r) != len(hdr):
            continue
        d = dict(zip(hdr, r))
        try:
            ft = float(d['frametime'])
            if not (0 < ft < 60000) or float(d['fps']) < 0.5:
                continue
            x = (float(d['elapsed']) / 1e9 + OFF, ft, float(d['gpu_vram_used']) * 1024, float(d['ram_used']) * 1024,
                 float(d['swap_used']) * 1024, float(d['gpu_load']), float(d['cpu_load']))
        except (KeyError, ValueError):
            continue
        if rows and rows[-1][1] == x[1] and abs(rows[-1][0] - x[0]) < 0.15:
            continue
        rows.append(x)
ts = [r[0] for r in rows]

ev, t0 = [], None
for line in open(LOG, encoding='utf-8', errors='replace'):
    m = TS.match(line.rstrip('\n'))
    if not m:
        continue
    s = int(m[1]) * 3600 + int(m[2]) * 60 + int(m[3]) + int(m[4]) / 1000
    t0 = s if t0 is None else t0
    if s < t0 - 43200:
        s += 86400
    ev.append((s - t0, m[5]))
wins, start = [], None
for t, s in ev:
    if 'engine stop flags' in s:
        if 'none' in s and start is not None:
            wins.append((start, t)); start = None
        elif 'none' not in s:
            start = t

def sl(a, b):
    i, j = bisect.bisect_left(ts, a), bisect.bisect_right(ts, b)
    return rows[i:j]
def pct(v, p):
    v = sorted(v)
    return v[min(len(v) - 1, int(len(v) * p / 100))] if v else float('nan')
def desc(rs):
    f = [r[1] for r in rs]
    return (f'{len(f):5d} frames  >66.7 {sum(x > 66.7 for x in f):4d}  >100 {sum(x > 100 for x in f):4d}  '
            f'>200 {sum(x > 200 for x in f):4d}  p50 {pct(f,50):6.1f}  p99 {pct(f,99):6.1f}  max {max(f) if f else 0:6.1f} ms')

print(f'{CSV}: {len(rows)} frames, {len(wins)} load window(s)')
for k, (a, b) in enumerate(wins, 1):
    w = sl(a, b)
    if not w:
        continue
    print(f'load {k}: t {a:.0f}-{b:.0f} s ({b - a:.0f} s)')
    print(f'  window      {desc(w)}')
    print(f'  memory      GPU {w[0][2]:.0f} -> {w[-1][2]:.0f} MB, RAM {w[0][3]:.0f} -> {w[-1][3]:.0f} MB, swap {w[0][4]:.0f} -> {w[-1][4]:.0f} MB')
    # phase A: the 10 s with the largest swap rise
    best, bt = 0.0, None
    for r in w:
        e = sl(r[0], r[0] + 10)
        if e and e[-1][4] - r[4] > best:
            best, bt = e[-1][4] - r[4], r[0]
    if bt is not None and best > 20:
        print(f'  phase A     swap +{best:.0f} MB in 10 s at t {bt:.0f}: {desc(sl(bt, bt + 10))}')
    c = sl(b - 20, b + 2)
    gl = sum(r[5] for r in c) / max(len(c), 1)
    print(f'  phase C     last 20 s (GPU load {gl:.0f} %): {desc(c)}')
    run, settled = 0, None
    for r in sl(b, b + 600):
        run = run + 1 if r[1] < 66.7 else 0
        if run >= 60:
            settled = r[0] - b
            break
    print(f'  settle      60 frames < 66.7 ms in a row reached {settled:.1f} s after the flag cleared' if settled is not None else '  settle      not reached within 600 s')
    print(f'  next 4 min  {desc(sl(b + 2, b + 240))}')
