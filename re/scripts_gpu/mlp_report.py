r"""mlp_report.py - digest the meshlet front-end probe (ASI 1.7.47 [Profiler] MeshletProbe / MpcCheck; notes\meshlet-frontend.md).

    py -I re\scripts_gpu\mlp_report.py <Nioh3EngineFix.log | run01.log> [--csv mlp_events.csv] [--min-frames 4]

Reads the "GP: MLP:" (GPU half), "MLPC:" (CPU half) and "MPCK:" (self-check) lines. Prints
  * front-end headline averaged over the windows (frame-weighted): total / dispatch / barrier / gap ms, dispatches and barrier calls,
    <= 1-wave and empty dispatches, the floor and the structural vs above-floor split
  * the regions and dispatch rows of the window with the most timing frames, CS digests named after the game's shader records
    (gpu-shaders\dxil_index.csv: <group>#<record index in its file>; replacement builds: mpc / mshlod / lodu)
  * the last CPU-half block and the MPCK totals.
With --csv: per (region, op) median us from the raw events and the per-level breakdown of MeshletResolveLOD.
"""
import argparse
import collections
import csv
import os
import re
import statistics
import sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..'))
REPL = {'c7e0a06b': 'MPC#23(mpc)', '55198d09': 'MPC#28(mpc)', 'd17e19a0': 'MPC#29(mpc)', '718b35fe': 'MPC#30(mpc)', '676577ac': 'MPC#31(mpc)',
        '794b5de5': 'MshLod#3(mshlod)'}


def names():
    m = dict(REPL)
    p = os.path.join(ROOT, 'gpu-shaders', 'dxil_index.csv')
    if not os.path.exists(p):
        return m
    idx = collections.Counter()
    with open(p, newline='') as f:
        for r in csv.DictReader(f):
            k = r['container_digest'][:8]
            n = idx[r['file']]
            idx[r['file']] += 1
            m.setdefault(k, '%s#%d' % (r['group'], n))
    return m


HEAD = re.compile(r'GP: MLP: (\d+) timing \+ (\d+) stats frames; meshlet front-end ([\d.]+) ms/frame = dispatch ([\d.]+) \+ barrier ([\d.]+) \+ gap ([\d.]+) \| '
                  r'([\d.]+) regions, ([\d.]+) Dispatch \+ ([\d.]+) ExecuteIndirect, ([\d.]+) barrier calls \(([\d.]+) structs, ([\d.]+) UAV\)/frame \| '
                  r'<= 1 wave \(<= 64 threads\): ([\d.]+) dispatches ([\d.]+) ms/frame, empty \(0 threads\): ([\d.]+) dispatches ([\d.]+) ms/frame \| '
                  r'floor \(median us of <= 1-wave dispatches\) ([\d.]+) us -> structural \(barrier \+ gap \+ floor x dispatches\) ([\d.]+) ms, work above the floor ([\d.]+) ms')
FIELDS = ['tf', 'sf', 'tot', 'disp', 'bar', 'gap', 'regions', 'nd', 'ne', 'nbc', 'nbs', 'nbu', 'tinyN', 'tinyMs', 'zeroN', 'zeroMs', 'floor', 'struct', 'work']
CSKEY = re.compile(r'\b(D|EI) ([0-9a-f]{8})\b')


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('log')
    ap.add_argument('--csv')
    ap.add_argument('--min-frames', type=int, default=4)
    a = ap.parse_args()
    nm = names()
    wins, cur = [], None
    mlpc, mpck = [], []
    for line in open(a.log, encoding='utf-8', errors='replace'):
        if 'GP: MLP:' in line:
            m = HEAD.search(line)
            if m:
                cur = {'h': dict(zip(FIELDS, [float(x) for x in m.groups()])), 'rows': []}
                wins.append(cur)
            elif cur is not None:
                body = line.split('GP: MLP:', 1)[1].rstrip()
                cur['rows'].append(CSKEY.sub(lambda k: '%s %s %s' % (k.group(1), k.group(2), nm.get(k.group(2), '?')), body))
        elif 'MLPC:' in line:
            if 'CPU frames with meshlet recorder calls' in line:
                mlpc = []
            mlpc.append(line.split('MLPC:', 1)[1].rstrip())
        elif 'MPCK:' in line:
            mpck.append(line.split('MPCK:', 1)[1].rstrip())
    good = [w for w in wins if w['h']['tf'] >= a.min_frames]
    print('log: %s - %d MLP windows (%d with >= %d timing frames)' % (a.log, len(wins), len(good), a.min_frames))
    if good:
        fw = sum(w['h']['tf'] for w in good)
        avg = {k: sum(w['h'][k] * w['h']['tf'] for w in good) / fw for k in FIELDS}
        print('front-end %.3f ms/frame = dispatch %.3f + barrier %.3f + gap %.3f | %.1f Dispatch + %.1f EI, %.1f barrier calls/frame | '
              '<= 1 wave %.1f dispatches %.3f ms, empty %.1f dispatches %.3f ms | floor %.1f us -> structural %.3f ms, above floor %.3f ms '
              '(frame-weighted over %d windows, %d timing frames)' % (avg['tot'], avg['disp'], avg['bar'], avg['gap'], avg['nd'], avg['ne'], avg['nbc'],
                                                                      avg['tinyN'], avg['tinyMs'], avg['zeroN'], avg['zeroMs'], avg['floor'],
                                                                      avg['struct'], avg['work'], len(good), fw))
        best = max(good, key=lambda w: w['h']['tf'])
        print('\nwindow with the most timing frames (%d):' % best['h']['tf'])
        for r in best['rows']:
            print(' ', r)
    if mlpc:
        print('\nCPU half (last window):')
        for r in mlpc:
            print(' ', r)
    if mpck:
        tot = [x for x in mpck if 'TOTAL' in x]
        print('\nMPCK total: %s' % (tot[-1].split('TOTAL:', 1)[1].strip() if tot else '(no window line)'))
        for x in mpck:
            if 'MISMATCH #' in x:
                print('  ', x.strip())
                break
    if a.csv and os.path.exists(a.csv):
        per = collections.defaultdict(list)
        lvl = collections.defaultdict(float)
        rows = list(csv.reader(open(a.csv, newline='')))
        hdr, rows = rows[0], rows[1:]
        by = collections.defaultdict(list)
        for r in rows:
            if r[1] == 'T':
                by[(r[0], r[2])].append(r)
        for (_, _), ev in by.items():
            ev.sort(key=lambda r: int(r[6]))
            op = 0
            for i, r in enumerate(ev[:-1]):
                dt = float(ev[i + 1][11]) - float(r[11])
                if r[7] in ('1', '2'):
                    per[(r[3], r[4], op, r[9], r[8])].append(dt)
                    op += 1
                if r[3] == 'LOD':
                    lvl[r[8]] += dt
        nfr = len({r[0] for r in rows if r[1] == 'T'}) or 1
        print('\nraw events (%d timing frames): dispatch rows by median us' % nfr)
        for k, v in sorted(per.items(), key=lambda kv: -sum(kv[1]))[:40]:
            print('  %s#%s op%d %s %-18s [%s] median %.1f us, %.1f calls/frame' % (k[0], k[1], k[2], k[3], nm.get(k[3].lower()[:8], '?'), k[4],
                                                                                    statistics.median(v), len(v) / nfr))
        if lvl:
            print('MeshletResolveLOD time by innermost marker (level), ms/frame:',
                  ', '.join('%s %.3f' % (k, v / nfr / 1000.0) for k, v in sorted(lvl.items(), key=lambda kv: (len(kv[0]), kv[0]))))
    return 0


if __name__ == '__main__':
    sys.exit(main())
