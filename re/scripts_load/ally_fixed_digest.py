"""Digest Ally mod logs around each 58A window: per stats period (30 s) frames, game ticks, GF fence waits,
VRAM, RAM, SetEvent, and the window length. Usage: py -I ally_fixed_digest.py <log>..."""
import re, sys
def t(s):
    h, m, x = s.split(':'); return int(h) * 3600 + int(m) * 60 + float(x)
pat = {
 'tiles': re.compile(r'stats tiles: ([\d.]+) s, (\d+) frames'),
 'tg': re.compile(r'game ticks ([\d.]+)/s vs rendered ([\d.]+)/s'),
 'gf': re.compile(r'GF: fence waits (\d+), avg wait (\d+) us'),
 'mem': re.compile(r'VRAM local used/budget (\d+)/(\d+) MB, non-local (\d+)/\d+ MB \| sys RAM avail (\d+) MB'),
 'wk': re.compile(r'stats wakes/s: SetEvent (\d+)'),
 'osd': re.compile(r'OSD: stats: (?:rendered|real) ([\d.]+) fps.*?game ticks ([\d.]+)/s, speed ([\d.]+)x.*?armed (\d+), draws (\d+)'),
 'osd2': re.compile(r'OSD: stats: (?:rendered|real) ([\d.]+) fps.*?speed ([\d.]+)x.*?armed (\d+), draws (\d+)'),
}
for fn in sys.argv[1:]:
    rows = []; sets = []
    for ln in open(fn, encoding='utf-8', errors='replace'):
        m = re.match(r'\[(\d+:\d+:[\d.]+)\] (.*)', ln)
        if not m: continue
        ts, body = t(m.group(1)), m.group(2)
        if 'engine stop flags' in body:
            sets.append((ts, body.split('now: ')[1].strip()))
        for k, p in pat.items():
            mm = p.search(body)
            if mm: rows.append((ts, k, mm.groups())); break
    print('==', fn.split('/')[-1])
    win = []
    for i, (ts, v) in enumerate(sets):
        if '58A' in v:
            end = next((s for s, w in sets[i + 1:] if '58A' not in w), None)
            if end: win.append((ts, end)); print(f'  window {ts:.3f} -> {end:.3f} = {end - ts:.3f} s')
    for a, b in win:
        for ts, k, g in rows:
            if a - 35 <= ts <= b + 35:
                tag = 'IN ' if a <= ts <= b + 30 else '   '
                print(f'  {tag}{ts - a:+7.1f} {k:5s} {" ".join(g)}')

# ---- integrated counts in each window (stats periods are ~30 s and end at the line's timestamp) ----
def integrate(fn):
    per = []; sets = []; tg = []
    for ln in open(fn, encoding='utf-8', errors='replace'):
        m = re.match(r'\[(\d+:\d+:[\d.]+)\] (.*)', ln)
        if not m: continue
        ts, body = t(m.group(1)), m.group(2)
        if 'engine stop flags' in body: sets.append((ts, '58A' in body))
        mm = pat['tiles'].search(body)
        if mm: per.append((ts - float(mm.group(1)), ts, int(mm.group(2))))
        mm = pat['tg'].search(body)
        if mm: tg.append((ts, float(mm.group(1)), float(mm.group(2))))
    out = []
    for i, (a, on) in enumerate(sets):
        if not on: continue
        b = next((s for s, w in sets[i + 1:] if not w), None)
        if b is None: continue
        fr = 0.0; tk = 0.0; cov = 0.0
        for (p0, p1, n) in per:
            ov = max(0.0, min(b, p1) - max(a, p0))
            if ov > 0:
                fr += n * ov / (p1 - p0); cov += ov
                g = min(tg, key=lambda r: abs(r[0] - p1))
                tk += g[1] * ov
        out.append((b - a, fr, tk, cov))
    return out
if __name__ == '__main__':
    print('window s | frames (interp.) | game ticks (interp.) | covered s')
    for fn in sys.argv[1:]:
        for w, fr, tk, cov in integrate(fn):
            print(f'{fn.split("/")[-1][:40]:40s} {w:8.3f} {fr:8.0f} {tk:8.0f} {cov:6.1f}')
