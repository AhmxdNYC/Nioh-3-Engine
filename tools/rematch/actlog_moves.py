"""Summarise boss action ids from Nioh3Training.log ACTLOG / MOVESEEN lines (read only).

Usage: py tools/rematch/actlog_moves.py <Nioh3Training.log> [--chr 16102] [--csv out.csv]

Per action id: how often it started, the duration of each run (median / min / max),
whether the player lost HP while it ran (HB heartbeat lines), the boss HP and Ki fraction
when it started (HB / GOLDKI diag lines), and the most common previous / next ids.
Used for #13786 to split real attacks from short state 'twitches'.
"""
import argparse
import csv
import re
import statistics
import sys
from collections import Counter, defaultdict

TS = re.compile(r'^\[(\d+):(\d+):(\d+)\.(\d+)\]')
ACT = re.compile(r'(?:ACTLOG|MOVESEEN) boss (\w+) chr (\d+): action 0x([0-9A-Fa-f]+) \(prev 0x([0-9A-Fa-f]+), ran (\d+) ms')
HB = re.compile(r' HB \+\d+ ms boss HP (\d+)/(\d+) player HP (\d+)/(\d+)')
KI = re.compile(r'GOLDKI diag .*?Ki (-?[\d.]+) HP (-?[\d.]+)')
REP_ON = re.compile(r'REPEAT chr (\d+) 0x[0-9A-Fa-f]+ ON')
REP_OFF = re.compile(r'REPEAT chr (\d+) (?:0x[0-9A-Fa-f]+ stopped|stop:)')


def ms(line):
    m = TS.match(line)
    if not m:
        return None
    h, mi, s, f = (int(x) for x in m.groups())
    return ((h * 60 + mi) * 60 + s) * 1000 + f


def parse(path, want_chr):
    runs = []          # dicts: id, chr, start, dur, prev, next, ki, bhp, dmg
    open_run = {}      # boss ptr -> run dict
    ki = bhp = None
    php = None
    rep_on = set()     # chr ids with a Training REPEAT / drill armed: their runs are forced, not natural
    with open(path, encoding='utf-8', errors='replace') as f:
        for line in f:
            t = ms(line)
            m = HB.search(line)
            if m:
                bhp = int(m.group(1)) / max(1, int(m.group(2)))
                p = int(m.group(3))
                if php is not None and p < php:
                    for r in open_run.values():
                        r['dmg'] += php - p
                php = p
                continue
            m = KI.search(line)
            if m:
                ki = float(m.group(1))
                continue
            m = REP_ON.search(line)
            if m:
                rep_on.add(int(m.group(1)))
                continue
            m = REP_OFF.search(line)
            if m:
                rep_on.discard(int(m.group(1)))
                continue
            m = ACT.search(line)
            if not m:
                continue
            boss, chr_id, aid, prev, ran = m.group(1), int(m.group(2)), int(m.group(3), 16), int(m.group(4), 16), int(m.group(5))
            if want_chr and chr_id != want_chr:
                continue
            r = open_run.get(boss)
            if r and r['id'] == prev:
                r['dur'] = ran
                r['next'] = aid
            nr = dict(id=aid, chr=chr_id, start=t, dur=None, prev=prev, next=None, ki=ki, bhp=bhp, dmg=0, forced=chr_id in rep_on)
            runs.append(nr)
            open_run[boss] = nr
    return runs


def summarise(runs):
    by = defaultdict(list)
    for r in runs:
        by[(r['chr'], r['id'])].append(r)
    out = []
    for (chr_id, aid), rs in sorted(by.items(), key=lambda kv: (kv[0][0], kv[0][1])):
        d = [r['dur'] for r in rs if r['dur'] is not None and r['dur'] < 60000 and not r['forced']]
        fd = [r['dur'] for r in rs if r['dur'] is not None and r['dur'] < 60000 and r['forced']]
        kis = [r['ki'] for r in rs if r['ki'] is not None]
        out.append(dict(
            chr=chr_id, action_id='0x%X' % aid, starts=len(rs), forced=len(fd),
            med_ms=int(statistics.median(d)) if d else '', min_ms=min(d) if d else '', max_ms=max(d) if d else '',
            hits=sum(1 for r in rs if r['dmg'] > 0 and not r['forced']), dmg=sum(r['dmg'] for r in rs if not r['forced']),
            ki_min=round(min(kis), 2) if kis else '', ki_max=round(max(kis), 2) if kis else '',
            prev=' '.join('0x%X' % k for k, _ in Counter(r['prev'] for r in rs).most_common(3)),
            next=' '.join('0x%X' % k for k, _ in Counter(r['next'] for r in rs if r['next'] is not None).most_common(3))))
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('log')
    ap.add_argument('--chr', type=int, default=0)
    ap.add_argument('--csv')
    a = ap.parse_args()
    rows = summarise(parse(a.log, a.chr))
    cols = ['chr', 'action_id', 'starts', 'forced', 'med_ms', 'min_ms', 'max_ms', 'hits', 'dmg', 'ki_min', 'ki_max', 'prev', 'next']
    w = csv.DictWriter(open(a.csv, 'w', newline='', encoding='utf-8') if a.csv else sys.stdout, cols)
    w.writeheader()
    w.writerows(rows)


if __name__ == '__main__':
    main()
