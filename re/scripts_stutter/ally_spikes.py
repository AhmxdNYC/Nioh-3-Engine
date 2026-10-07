# ally_spikes.py - spike census of the Ally MangoHud CSVs, correlated with GPU memory / RAM / swap and the mod-log
# events (stop flag 58A = level load window, menu open/close, live switches, OSD/stats lines).
#   py -I re\scripts_stutter\ally_spikes.py [captures\ally]  -> prints the census; writes ally_spikes.tsv next to it
#
# MangoHud logs a sample every ~100 ms (10 Hz). `frametime` (ms) is the frame time current at the sample, so a frame
# of length F is seen by ~F/100ms samples (>= 1 for F >= 100 ms, probability F/100ms below that). Consecutive
# identical (frametime, fps) samples are the same frame and are counted once.
# Spike = frame > 2 x the rolling median of the surrounding 20 s (and > 2 x 1000/60 ms when the median is < 16.7 ms).
import csv, os, re, sys, statistics, datetime, bisect

D = sys.argv[1] if len(sys.argv) > 1 else os.path.join(os.path.dirname(__file__), '..', '..', 'captures', 'ally')
PAIRS = {  # csv -> (mod log, seconds to add to csv t to get log wall clock offset from log start; None = auto)
    'nioh3_ally_mangohud_v1-mod-L1FGwine_20261006-2345.csv': None,
    'nioh3_ally_mangohud_v2-mod_20261007-0305.csv': 'nioh3_ally_modlog_v2_20261007-0305.log',
    'nioh3_ally_mangohud_v4-fgoff-futamata-lightest50_20261007-0423.csv': 'nioh3_ally_modlog_v4-fgoff-futamata_20261007-0428.log',
    'nioh3_ally_mangohud_v5-w6-ABA-futamata-lightest50_20261007-0432.csv': 'nioh3_ally_modlog_v5-w6ab-futamata_20261007-0441.log',
}
TS = re.compile(r'^\[(\d\d):(\d\d):(\d\d)\.(\d\d\d)\]\s(.*)$')

def load_log(p):
    ev = []
    if not p or not os.path.exists(p):
        return None, ev
    t0 = None
    for line in open(p, encoding='utf-8', errors='replace'):
        m = TS.match(line.rstrip('\n'))
        if not m:
            continue
        s = int(m[1]) * 3600 + int(m[2]) * 60 + int(m[3]) + int(m[4]) / 1000
        if t0 is None:
            t0 = s
        if s < t0 - 43200:
            s += 86400
        ev.append((s - t0, m[5]))
    return t0, ev

def load_csv(p):
    rows = []
    with open(p, newline='') as f:
        r = csv.reader(f)
        hdr = None
        for row in r:
            if hdr is None:
                if row and row[0] == 'fps':
                    hdr = row
                continue
            if len(row) != len(hdr):
                continue
            d = dict(zip(hdr, row))
            try:
                rows.append({'t': float(d['elapsed']) / 1e9, 'ft': float(d['frametime']), 'fps': float(d['fps']),
                             'vram': float(d['gpu_vram_used']) * 1024, 'ram': float(d['ram_used']) * 1024,
                             'swap': float(d['swap_used']) * 1024, 'cpu': float(d['cpu_load']), 'gpu': float(d['gpu_load']),
                             'gclk': float(d['gpu_core_clock']), 'cpuw': float(d['cpu_power'])})
            except ValueError:
                pass
    # drop warm-up rows (fps not yet valid) and dedupe repeated samples of one frame
    rows = [x for x in rows if x['fps'] > 0.5 and 0 < x['ft'] < 60000]
    out = []
    for x in rows:
        if out and abs(out[-1]['ft'] - x['ft']) < 1e-6 and abs(out[-1]['fps'] - x['fps']) < 1e-6:
            continue
        out.append(x)
    return out

def windows_from_log(ev):
    """level-load windows from the stop-flag lines (58A set -> none)"""
    w, start = [], None
    for t, s in ev:
        if 'engine stop flags' in s:
            if 'none' in s:
                if start is not None:
                    w.append((start, t)); start = None
            else:
                start = t
    return w

def val_at(rows, ts, key, t):
    i = bisect.bisect_left(ts, t)
    i = min(max(i, 0), len(rows) - 1)
    return rows[i][key]

def analyse(csvname, logname):
    rows = load_csv(os.path.join(D, csvname))
    t0, ev = load_log(os.path.join(D, logname) if logname else None)
    ts = [x['t'] for x in rows]
    fts = [x['ft'] for x in rows]
    # csv t -> log t: MangoHud starts with the process (first row ~0.1 s); the mod log starts ~0.2 s after process
    # start. Cross-check: v5 load t 122-207 s == log 04:34:34-04:35:59 (memory-leaks.md) with offset 0.
    off = 0.0
    loads = windows_from_log(ev)
    n = len(rows)
    spikes = []
    for i, x in enumerate(rows):
        lo = bisect.bisect_left(ts, x['t'] - 10); hi = bisect.bisect_right(ts, x['t'] + 10)
        med = statistics.median(fts[lo:hi])
        thr = max(2 * med, 2 * 16.67 if med < 16.67 else 0)
        if x['ft'] > thr:
            spikes.append((i, med))
    # phase of each sample
    first_render = next((t for t, s in ev if 'engine is rendering' in s), 6.0)
    def phase(t):
        for a, b in loads:
            if a - 2 <= t <= b + 2:
                return 'load'
        for a, b in loads:
            if b < t <= b + 240:
                return 'settle(<4min after load)'
        if loads and t < loads[0][0]:
            return 'title/menu (pre-load)'
        if not loads and t < 210:
            return 'title+load (no log flag)'
        return 'gameplay'
    # memory deltas around the spike (+-1.5 s)
    res = []
    for i, med in spikes:
        x = rows[i]; t = x['t']
        dv = val_at(rows, ts, 'vram', t + 1.5) - val_at(rows, ts, 'vram', t - 1.5)
        dsw = val_at(rows, ts, 'swap', t + 1.5) - val_at(rows, ts, 'swap', t - 1.5)
        dram = val_at(rows, ts, 'ram', t + 1.5) - val_at(rows, ts, 'ram', t - 1.5)
        near = [s for (te, s) in ev if abs(te - (t + off)) <= 2.0 and not s.startswith(('stats', 'SP ', 'OSD: stats'))]
        res.append({'t': t, 'ft': x['ft'], 'med': med, 'ratio': x['ft'] / med, 'phase': phase(t), 'dvram': dv, 'dswap': dsw,
                    'dram': dram, 'vram': x['vram'], 'swap': x['swap'], 'ram': x['ram'], 'cpu': x['cpu'], 'gpu': x['gpu'],
                    'gclk': x['gclk'], 'near': near[:3]})
    return rows, loads, res

def pct(a, p):
    a = sorted(a)
    if not a:
        return float('nan')
    k = (len(a) - 1) * p / 100
    f = int(k); c = min(f + 1, len(a) - 1)
    return a[f] + (a[c] - a[f]) * (k - f)

allres = []
for c, l in PAIRS.items():
    if not os.path.exists(os.path.join(D, c)):
        continue
    rows, loads, res = analyse(c, l)
    tag = c.split('_')[2]
    print(f'=== {tag}  {len(rows)} distinct samples, {rows[0]["t"]:.0f}-{rows[-1]["t"]:.0f} s, load windows {[(round(a), round(b)) for a, b in loads]}')
    # per-phase frame time stats
    def ph_rows(name):
        out = []
        for x in rows:
            p = None
            t = x['t']
            if any(a - 2 <= t <= b + 2 for a, b in loads): p = 'load'
            elif any(b < t <= b + 240 for a, b in loads): p = 'settle'
            elif loads and t < loads[0][0]: p = 'pre'
            elif not loads: p = 'title+load' if t < 210 else 'gameplay'
            else: p = 'gameplay'
            if p == name: out.append(x['ft'])
        return out
    for name in ('pre', 'title+load', 'load', 'settle', 'gameplay'):
        a = ph_rows(name)
        if a:
            print(f'   {name:10s} n={len(a):5d} median {pct(a,50):6.1f} p95 {pct(a,95):6.1f} p99 {pct(a,99):6.1f} max {max(a):6.1f} ms')
    by = {}
    for r in res:
        by.setdefault(r['phase'], []).append(r)
    for ph, lst in by.items():
        sw = sum(1 for r in lst if r['dswap'] > 20)
        vr = sum(1 for r in lst if r['dvram'] > 50)
        print(f'   spikes in {ph:28s}: {len(lst):4d}  median {pct([r["ft"] for r in lst],50):6.1f} ms, max {max(r["ft"] for r in lst):6.1f} ms, '
              f'ratio med {pct([r["ratio"] for r in lst],50):.2f}; with GPU-mem rise >50MB/3s: {vr}, with swap rise >20MB/3s: {sw}')
    big = sorted(res, key=lambda r: -r['ft'])[:12]
    for r in sorted(big, key=lambda r: r['t']):
        print(f'     t={r["t"]:6.1f}s {r["ft"]:6.1f} ms (x{r["ratio"]:.1f} of {r["med"]:.1f}) {r["phase"]:24s} vram {r["vram"]:5.0f} ({r["dvram"]:+5.0f}) '
              f'swap {r["swap"]:5.0f} ({r["dswap"]:+4.0f}) ram {r["ram"]:5.0f} ({r["dram"]:+4.0f}) cpu {r["cpu"]:3.0f}% gpu {r["gpu"]:3.0f}% {r["gclk"]:.0f}MHz {" | ".join(r["near"])[:90]}')
    for r in res:
        r['cap'] = tag
    allres += res

out = os.path.join(D, 'ally_spikes.tsv')
with open(out, 'w', newline='') as f:
    w = csv.writer(f, delimiter='\t')
    w.writerow(['capture', 't_s', 'frame_ms', 'median_ms', 'ratio', 'phase', 'dvram_MB_3s', 'dswap_MB_3s', 'dram_MB_3s', 'vram_MB', 'swap_MB', 'ram_MB', 'cpu_pct', 'gpu_pct', 'gpu_mhz', 'log_events'])
    for r in allres:
        w.writerow([r['cap'], f'{r["t"]:.1f}', f'{r["ft"]:.1f}', f'{r["med"]:.1f}', f'{r["ratio"]:.2f}', r['phase'], f'{r["dvram"]:.0f}', f'{r["dswap"]:.0f}', f'{r["dram"]:.0f}',
                    f'{r["vram"]:.0f}', f'{r["swap"]:.0f}', f'{r["ram"]:.0f}', f'{r["cpu"]:.0f}', f'{r["gpu"]:.0f}', f'{r["gclk"]:.0f}', ' | '.join(r['near'])])
print('wrote', out)
