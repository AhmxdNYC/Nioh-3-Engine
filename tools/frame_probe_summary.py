"""Turn FRAMEPROBE lines of Nioh3Training.log (mod/Nioh3Training/src/frame_probe_game.h, #22356) into per-move frame data.

  py -I tools/frame_probe_summary.py <Nioh3Training.log or extract> [--csv notes/yoritomo-move-summary.csv] [--chr 16102] [--runs]

One boss action run = the lines between 'FRAMEPROBE start' and its 'FRAMEPROBE run' summary. Each 'hit' line is one attack-collision
creation (one hitbox row / collider). A move hit = a group of overlapping collider windows (open .. end). Numbers are in ms and in 60 fps
frames (ms * 0.06): startup = first frame a hit window is open (frame index, 1-based), active = frames of each hit window, recovery = frames from the
last window end to the next action.
"""
import re
import statistics
import sys

RE_T = r'\[(\d+):(\d+):(\d+)\.(\d+)\]'
RE_HIT = re.compile(RE_T + r' FRAMEPROBE hit chr (\d+) boss (\w) act 0x([0-9A-F]+) #(\d+) at ([\d.]+) ms .*? great (\d) .*?power (\d+) (\d+) (\d+)')
RE_RUN = re.compile(RE_T + r' FRAMEPROBE run chr (\d+) boss (\w) act 0x([0-9A-F]+) -> 0x([0-9A-F]+) ran ([\d.]+) ms .*? hits (\d+)(?:.*?hit/end ms \[([^\]]*)\])?')
RE_START = re.compile(RE_T + r' FRAMEPROBE start chr (\d+) boss (\w) act 0x([0-9A-F]+)')
RE_END = re.compile(RE_T + r' FRAMEPROBE hitend chr (\d+) boss (\w) act 0x([0-9A-F]+) #(\d+) window ([\d.]+) ms .*? ends at ([\d.]+) ms')
F60 = 0.06
TOL_MS = 17.0  # windows that open within one 60 fps frame of the current group's end belong to the same hit


def parse(lines):
    """Windows come from the 'hit' (open, power) and 'hitend' (end) lines; the run line's hit/end list (first 8 or 12) is the fallback."""
    runs, pend, last_run = [], {}, None
    for ln in lines:
        m = RE_START.search(ln)
        if m:
            pend[(m.group(5), m.group(6))] = {}
            continue
        m = RE_HIT.search(ln)
        if m:
            pend.setdefault((m.group(5), m.group(6)), {})[int(m.group(8))] = {
                'open': float(m.group(9)), 'end': None, 'great': int(m.group(10)), 'power': (int(m.group(11)), int(m.group(12)), int(m.group(13)))}
            continue
        m = RE_END.search(ln)
        if m:
            w = pend.get((m.group(5), m.group(6)), {}).get(int(m.group(8)))
            if w is not None:
                w['end'] = float(m.group(10))
            elif runs and runs[-1]['chr'] == int(m.group(5)) and int(m.group(8)) < len(runs[-1]['win']):  # window outlived its action
                runs[-1]['win'][int(m.group(8))]['end'] = float(m.group(10))
            continue
        m = RE_RUN.search(ln)
        if m:
            if ln[15:] == last_run:  # 2026-10-21 first test build printed each run line once per collider: keep one
                continue
            last_run = ln[15:]
            key = (m.group(5), m.group(6))
            r = {'chr': int(m.group(5)), 'boss': m.group(6), 'act': int(m.group(7), 16), 'next': int(m.group(8), 16), 'ran': float(m.group(9)),
                 'hits': int(m.group(10)), 'win': [], 'time': '%s:%s:%s' % m.group(1, 2, 3)}
            hp = pend.get(key, {})
            pairs = (m.group(11) or '').split()
            for i in range(max(len(pairs), max(hp) + 1 if hp else 0)):
                w = dict(hp.get(i, {'open': None, 'end': None, 'great': 0, 'power': None}))
                if i < len(pairs):
                    o, e = (float(x) for x in pairs[i].split('/'))
                    w['open'] = o if w['open'] is None else w['open']
                    w['end'] = w['end'] if w['end'] is not None else (e if e >= 0 else None)
                if w['open'] is not None:
                    r['win'].append(w)
            runs.append(r)
            pend[key] = {}
    return runs


def groups(win):
    """Merge overlapping collider windows into move hits: [(open, end, powers)]."""
    out = []
    for w in sorted(win, key=lambda w: w['open']):
        e = w['end'] if w['end'] is not None else w['open']
        if out and w['open'] <= out[-1][1] + TOL_MS:
            g = out[-1]
            out[-1] = (g[0], max(g[1], e), g[2] | {w['power']}, g[3] and w['end'] is not None)
        else:
            out.append((w['open'], e, {w['power']}, w['end'] is not None))
    return out


def frame(ms):
    return ms * F60


def run_numbers(r):
    g = groups(r['win'])
    if not g:
        return None
    return {'n': len(g), 'startup_ms': g[0][0], 'windows_ms': [(a, b) for a, b, _, _ in g], 'powers': [sorted(p for p in ps if p) for _, _, ps, _ in g],
            'ends_seen': all(ok for *_, ok in g), 'recovery_ms': r['ran'] - g[-1][1], 'ran': r['ran']}


def load_csv(path):
    out = {}
    for ln in open(path, encoding='utf-8'):
        f = ln.rstrip('\n').split(',')
        if f[0].startswith('0x'):
            out[int(f[0], 16)] = {'unblockable': f[4], 'hits': f[5], 'windows': f[6]}
    return out


def fmt(xs, nd=1):
    if not xs:
        return '-'
    lo, hi, md = min(xs), max(xs), statistics.median(xs)
    return ('%.*f' % (nd, md)) + ('' if hi - lo < 0.05 else ' (%.*f..%.*f)' % (nd, lo, nd, hi))


def main(argv):
    path = argv[1]
    csv = argv[argv.index('--csv') + 1] if '--csv' in argv else None
    only = int(argv[argv.index('--chr') + 1]) if '--chr' in argv else None
    runs = [r for r in parse(open(path, encoding='utf-8', errors='replace')) if only is None or r['chr'] == only]
    static = load_csv(csv) if csv else {}
    by = {}
    for r in runs:
        by.setdefault((r['chr'], r['act']), []).append(r)
    print('chr\tmove\truns\thit_runs\thits\tstartup_f\tactive_f (per hit, open-end frames)\trecovery_f\ttotal_f\tpowers\tgreat\tstatic_hits\tstatic_windows')
    for (chr_, act), rs in sorted(by.items()):
        nums = [n for n in (run_numbers(r) for r in rs) if n]
        if not nums:
            if len(rs) >= 2 or act in static:
                print('%d\t0x%X\t%d\t0\t0\t-\t-\t-\t%s\t-\t-\t%s\t%s' % (chr_, act, len(rs), fmt([frame(r['ran']) for r in rs]),
                      static.get(act, {}).get('hits', ''), static.get(act, {}).get('windows', '')))
            continue
        nh = statistics.mode(n['n'] for n in nums)
        full = [n for n in nums if n['n'] == nh]
        wins = []
        for k in range(nh):
            o = [frame(n['windows_ms'][k][0]) for n in full]
            e = [frame(n['windows_ms'][k][1]) for n in full]
            wins.append('%s-%s (%s f)' % (fmt(o), fmt(e), fmt([b - a for a, b in zip(o, e)])))
        great = max(w['great'] for r in rs for w in r['win']) if any(r['win'] for r in rs) else 0
        print('%d\t0x%X\t%d\t%d\t%s\t%s\t%s\t%s\t%s\t%s\t%d\t%s\t%s' % (
            chr_, act, len(rs), len(nums), nh if len({n['n'] for n in nums}) == 1 else '%d (counts %s)' % (nh, sorted({n['n'] for n in nums})),
            fmt([frame(n['startup_ms']) + 1 for n in full]), ' | '.join(wins), fmt([frame(n['recovery_ms']) for n in full]),
            fmt([frame(n['ran']) for n in full]), ' / '.join(str(p) for p in full[0]['powers']), great,
            static.get(act, {}).get('hits', ''), static.get(act, {}).get('windows', '')))
    if '--runs' in argv:
        for r in runs:
            n = run_numbers(r)
            print('%s 0x%X -> 0x%X ran %.1f f%s' % (r['time'], r['act'], r['next'], frame(r['ran']),
                  '' if not n else ' hits %d windows %s recovery %.1f f' % (n['n'], ' '.join('%.1f-%.1f' % (frame(a), frame(b)) for a, b in n['windows_ms']), frame(n['recovery_ms']))))


if __name__ == '__main__':
    main(sys.argv)
