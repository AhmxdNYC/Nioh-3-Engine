# load_zlib_sp.py - zlib inflate thread-seconds inside the 58A load window, from SP profiler rows (round-robin, so
# share x threads x report seconds = thread-seconds). Rows are rounded to 0.1 % and cut at topN, so it prints a
# lower bound (listed rows - 0.05) and an upper bound (listed rows + 0.05, or the smallest listed row when absent).
#   py -I re\scripts_load\load_zlib_sp.py <modlog.log> [...]
import re, sys
TS = re.compile(r'^\[[\d:.]+\]\s(.*)$')
HDR = re.compile(r'^SP (.+?): (\d+) samples in (\d+) s from (\d+) thread')
ROW = re.compile(r'^SP (\w+)\s+([\d.]+) %\s+(.*)$')
ZLIB = lambda a: 0xB33000 <= a < 0xB38000 or a in (0x420654, 0xA09A40, 0x324D064, 0x46F4D8)
KEY = {'KIDSTS': 'kidsts', 'KIDSTaskScheduler': 'kidsTS', '[App] other': 'app'}
for log in sys.argv[1:]:
    ph = None; hdr = {}; reps = []  # per (report, group): [threads, sec, zlib pct, min pct]
    cur = {}
    for line in open(log, encoding='utf-8', errors='replace'):
        m = TS.match(line)
        if not m: continue
        s = m[1]
        if 'engine stop flags' in s:
            if '58A' in s and ph is None: ph = 'load'
            elif s.rstrip().endswith('none') and ph == 'load': break
            continue
        if ph != 'load': continue
        h = HDR.match(s)
        if h and h[1] in KEY:
            cur[KEY[h[1]]] = r = [int(h[4]), int(h[3]), 0.0, 99.0, False]; reps.append((h[1], r)); continue
        r = ROW.match(s)
        if r and r[1] in cur:
            g = cur[r[1]]; p = float(r[2]); g[3] = min(g[3], p)
            f = re.search(r'fn 0x([0-9A-F]+)', r[3])
            if f and ZLIB(int(f[1], 16)): g[2] += p; g[4] = True
    lo = hi = 0.0; win = 0
    for name, (thr, sec, z, mn, seen) in reps:
        lo += max(0.0, z - 0.05) / 100 * thr * sec
        hi += ((z + 0.05) if seen else (mn + 0.05)) / 100 * thr * sec
        win = max(win, sec)
    print(f'{log.split(chr(92))[-1].split("/")[-1]}: {len(reps)} group reports; zlib thread-s in window: '
          f'{lo:.1f} .. {hi:.1f}')
