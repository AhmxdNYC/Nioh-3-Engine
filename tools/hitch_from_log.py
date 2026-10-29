"""Summarize the HITCH lines of Nioh3EngineFix.log ([Debug] HitchLog, mod/Nioh3EngineFix/src/hitchlog.cpp).

Usage: py -I tools/hitch_from_log.py Nioh3EngineFix.log [--min-ms 100] [--csv out.csv] [--top 15]

Per hitch it classifies the likely cause from the logged counters:
  wait:<stage>  one instrumented wait (W3/L1/F1/W6/GF/DS) covered >= 50 % of the gap
  cpu           the game thread was on CPU for >= 60 % of the gap
  disk          >= 2 MB read or >= 2000 page faults in the gap, game thread mostly off CPU
  paused        mod menu open, game menu / stop flags or loading (pause != 0)
  blocked       game thread mostly off CPU, no instrumented wait, no big I/O (lock / OS / driver stall)
"""
import argparse
import csv
import re
import statistics
import sys
from collections import Counter

LINE = re.compile(r'^\[(?P<wall>[\d:.]+)\] HITCH #(?P<n>\d+) gap (?P<gap>[\d.]+) ms')
FIELDS = {
    'gt_cpu': r'gt cpu (?P<v>[\d.]+) ms',
    'proc_cpu': r'proc cpu (?P<v>[\d.]+) ms',
    'pf': r'\| pf (?P<v>\d+)',
    'rd_mb': r'\| rd (?P<v>[\d.]+) MB',
    'rd_ops': r'\| rd [\d.]+ MB (?P<v>\d+) ops',
    'wr_mb': r'\| wr (?P<v>[\d.]+) MB',
    'menu': r'\| menu (?P<v>-?\d+)',
    'pause': r' pause (?P<v>-?\d+)',
    'mode': r' mode (?P<v>-?\d+)',
    'ram_free': r'\| ram (?P<v>-?[\d.]+)/',
    'commit': r'\| commit (?P<v>-?[\d.]+)/',
    'ws': r'\| ws (?P<v>-?[\d.]+) MB',
    't': r'\| t (?P<v>[\d.]+) s',
}
LAST = re.compile(r'\| last (?P<stage>\S+) (?P<ms>[\d.]+) ms')
MAX = re.compile(r'\| max (?P<stage>\S+) (?P<ms>[\d.]+) ms tid (?P<tid>\d+) (?P<who>\S+)')
WAITS = re.compile(r'(?P<stage>W3|L1|F1|W6|GF|DS) (?P<n>\d+)/(?P<ms>[\d.]+)')
VRAM = re.compile(r'\| vram (?P<u>[\d.]+)/(?P<b>[\d.]+) MB')
LD = re.compile(r'\| ld A (?P<a>\S+) B (?P<b>\S+) infl (?P<i>\S+)')


def parse(path):
    out = []
    with open(path, encoding='utf-8', errors='replace') as f:
        for raw in f:
            m = LINE.match(raw)
            if not m:
                continue
            h = {'wall': m['wall'], 'n': int(m['n']), 'gap': float(m['gap'])}
            for k, rx in FIELDS.items():
                mm = re.search(rx, raw)
                h[k] = float(mm['v']) if mm else None
            mm = LAST.search(raw)
            h['last'], h['last_ms'] = (mm['stage'], float(mm['ms'])) if mm else ('-', 0.0)
            mm = MAX.search(raw)
            h['max'], h['max_ms'], h['max_who'] = (mm['stage'], float(mm['ms']), mm['who']) if mm else ('-', 0.0, '-')
            waits = raw.split('| waits ', 1)[1].split('|', 1)[0] if '| waits ' in raw else ''
            h['waits'] = {w['stage']: (int(w['n']), float(w['ms'])) for w in WAITS.finditer(waits)}
            mm = VRAM.search(raw)
            h['vram_used'], h['vram_budget'] = (float(mm['u']), float(mm['b'])) if mm else (None, None)
            mm = LD.search(raw)
            h['ld'] = f"A {mm['a']} B {mm['b']} infl {mm['i']}" if mm else '-'
            h['cause'] = classify(h)
            out.append(h)
    return out


def classify(h):
    gap = h['gap'] or 1.0
    if h['max'] != '-' and h['max_ms'] >= 0.5 * gap:
        return 'wait:' + h['max']
    if (h['pause'] or 0) != 0 or (h['menu'] or 0) != 0:
        return 'paused'
    cpu = h['gt_cpu'] or 0.0
    if cpu >= 0.6 * gap:
        return 'cpu'
    if (h['rd_mb'] or 0) >= 2.0 or (h['pf'] or 0) >= 2000:
        return 'disk'
    return 'blocked'


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('log')
    ap.add_argument('--min-ms', type=float, default=0.0, help='ignore hitches shorter than this')
    ap.add_argument('--csv', help='write one row per hitch')
    ap.add_argument('--top', type=int, default=15, help='list the N longest hitches')
    a = ap.parse_args()
    hs = [h for h in parse(a.log) if h['gap'] >= a.min_ms]
    if not hs:
        print('no HITCH lines (is [Debug] HitchLog = 1? the line needs the hitchlog build)')
        return 1
    gaps = [h['gap'] for h in hs]
    span = (hs[-1]['t'] or 0) - (hs[0]['t'] or 0)
    print(f'{len(hs)} hitches, gap median {statistics.median(gaps):.0f} ms, max {max(gaps):.0f} ms, total {sum(gaps) / 1000:.1f} s'
          + (f' over {span:.0f} s of log' if span > 0 else ''))
    for b0, b1 in ((100, 250), (250, 500), (500, 1000), (1000, 10 ** 9)):
        n = sum(b0 <= g < b1 for g in gaps)
        if n:
            print(f'  {b0}-{b1 if b1 < 10 ** 9 else "inf"} ms: {n}')
    print('cause (heuristic):')
    for c, n in Counter(h['cause'] for h in hs).most_common():
        sub = [h for h in hs if h['cause'] == c]
        print(f'  {c:10s} {n:4d}  median gap {statistics.median(h["gap"] for h in sub):.0f} ms, '
              f'median gt cpu {statistics.median(h["gt_cpu"] or 0 for h in sub):.1f} ms, '
              f'median rd {statistics.median(h["rd_mb"] or 0 for h in sub):.1f} MB, median pf {statistics.median(h["pf"] or 0 for h in sub):.0f}')
    stall = Counter(h['max'] for h in hs if h['max'] != '-')
    if stall:
        print('longest instrumented wait per hitch: ' + ', '.join(f'{k} {v}' for k, v in stall.most_common()))
    ctx = Counter(f"menu{int(h['menu'] or 0)}/pause{int(h['pause'] or 0)}/mode{int(h['mode'] or 0)}" for h in hs)
    print('state (menu/pause/training mode): ' + ', '.join(f'{k} {v}' for k, v in ctx.most_common()))
    rams = [h['ram_free'] for h in hs if h['ram_free'] is not None and h['ram_free'] >= 0]
    if rams:
        print(f'free RAM at hitch: min {min(rams):.0f} MB, median {statistics.median(rams):.0f} MB')
    vr = [(h['vram_used'], h['vram_budget']) for h in hs if h['vram_used'] is not None]
    if vr:
        over = sum(u > b for u, b in vr)
        print(f'VRAM at hitch: max used {max(u for u, _ in vr):.0f} MB, budget {min(b for _, b in vr):.0f} MB, over budget {over}')
    print(f'top {min(a.top, len(hs))} by gap:')
    for h in sorted(hs, key=lambda x: -x['gap'])[:a.top]:
        print(f"  [{h['wall']}] #{h['n']} {h['gap']:.0f} ms  {h['cause']:10s} gt cpu {h['gt_cpu'] or 0:.1f} ms  "
              f"rd {h['rd_mb'] or 0:.1f} MB pf {int(h['pf'] or 0)}  max {h['max']} {h['max_ms']:.1f} ms ({h['max_who']})  "
              f"pause {int(h['pause'] or 0)} menu {int(h['menu'] or 0)} mode {int(h['mode'] or 0)}  ld {h['ld']}")
    if a.csv:
        keys = ['wall', 'n', 'gap', 'cause', 'gt_cpu', 'proc_cpu', 'pf', 'rd_mb', 'rd_ops', 'wr_mb', 'last', 'last_ms', 'max',
                'max_ms', 'max_who', 'menu', 'pause', 'mode', 'ld', 'ram_free', 'commit', 'ws', 'vram_used', 'vram_budget', 't']
        with open(a.csv, 'w', newline='') as f:
            w = csv.writer(f)
            w.writerow(keys + ['waits'])
            for h in hs:
                w.writerow([h[k] for k in keys] + [' '.join(f'{s}:{n}/{ms}' for s, (n, ms) in h['waits'].items())])
        print('wrote', a.csv)
    return 0


if __name__ == '__main__':
    sys.exit(main())
