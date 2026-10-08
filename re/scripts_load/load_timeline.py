# load_timeline.py - per-bucket timeline of an Ally level load (MangoHud CSV + mod log stop flags).
#   py -I re\scripts_load\load_timeline.py <mangohud.csv> <modlog.log> [bucket_s=5] [offset_s=0]
# Prints, per bucket from 10 s before 58A to 30 s after it clears: frames, mean/max frame ms, CPU %, CPU W, GPU %,
# GPU MHz, GPU W, VRAM/RAM/swap/RSS (MB). Rows are de-duplicated like load_phases.py (10 Hz logs repeat rows).
import csv, re, sys
CSV, LOG = sys.argv[1], sys.argv[2]
B = float(sys.argv[3]) if len(sys.argv) > 3 else 5.0
OFF = float(sys.argv[4]) if len(sys.argv) > 4 else 0.0
TS = re.compile(r'^\[(\d\d):(\d\d):(\d\d)\.(\d\d\d)\]\s(.*)$')
t0 = None; wins = []; cur = None
for line in open(LOG, encoding='utf-8', errors='replace'):
    m = TS.match(line)
    if not m: continue
    t = int(m[1])*3600 + int(m[2])*60 + int(m[3]) + int(m[4])/1000
    if t0 is None: t0 = t
    if t < t0: t += 86400
    s = m[5]
    if 'engine stop flags' in s:
        if '58A' in s and cur is None: cur = t - t0
        elif s.rstrip().endswith('none') and cur is not None: wins.append((cur, t - t0)); cur = None
rows = []; hdr = None; last = None
for r in csv.reader(open(CSV, newline='')):
    if hdr is None:
        if r and r[0] == 'fps': hdr = r
        continue
    if len(r) != len(hdr): continue
    d = dict(zip(hdr, r))
    key = (d['frametime'], d['cpu_load'], d['gpu_load'])
    if key == last: continue
    last = key
    try: rows.append({k: float(d[k]) for k in hdr if d[k] not in ('', None)} | {'t': float(d['elapsed'])/1e9 + OFF})
    except ValueError: pass
F = ['cpu_load','cpu_power','gpu_load','gpu_core_clock','gpu_power','gpu_vram_used','ram_used','swap_used','process_rss']
for a, b in wins:
    print(f'load window t {a:.1f}-{b:.1f} s ({b-a:.1f} s)')
    print(' t(rel)  frm  ms_avg  ms_max  cpu%  cpuW  gpu%   MHz  gpuW  vramMB  ramMB swapMB  rssMB')
    t = a - 10
    while t < b + 30:
        sel = [x for x in rows if t <= x['t'] < t + B]
        if sel:
            n = len(sel); ft = [x['frametime'] for x in sel]
            av = lambda k: sum(x.get(k, 0) for x in sel) / n
            mb = lambda k: av(k) * 1024 if av(k) < 100 else av(k) / 1024 / 1024
            print(f'{t-a:6.0f} {n:5d} {sum(ft)/n:7.1f} {max(ft):7.1f} {av("cpu_load"):5.0f} {av("cpu_power"):5.1f} {av("gpu_load"):5.0f} {av("gpu_core_clock"):5.0f} {av("gpu_power"):5.1f} {mb("gpu_vram_used"):7.0f} {mb("ram_used"):6.0f} {mb("swap_used"):6.0f} {mb("process_rss"):6.0f}' + ('  <- flag clear' if t <= b < t + B else ''))
        t += B
