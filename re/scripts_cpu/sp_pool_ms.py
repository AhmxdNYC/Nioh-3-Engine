"""sp_pool_ms.py - main-thread SP rows of several Nioh3EngineFix logs pooled into ms per frame.

usage: py -I sp_pool_ms.py <log> [<log> ...] [--top N] [--minrip 40] [--maxfps 59]

Each SP main report is paired with the latest preceding OSD `stats: rendered X fps ... draws N` line, so a row's share
becomes ms/frame (share x 1000 / fps) in that window. Only gameplay windows are kept (RIP-in-game > --minrip, fps <
--maxfps: drops title/loading/cap-60 windows). Prints mean/median ms per row (raw .pdata fragment labels as logged),
the part of game code below the per-report top-N cut ("tail"), DrawEmit ns per draw, and the correlation of the
`[ntdll] <- 5DA48` row with the GF fence-wait row (notes/ally-drawprep-2.md s.2). Stdlib only.
"""
import re, sys, statistics as st, collections

HDR = re.compile(r'SP main \[App\]Game: (\d+) samples in \d+ s from \d+ thread\(s\), RIP in Nioh3.exe ([\d.]+) %')
ROW = re.compile(r'SP main\s+([\d.]+) %\s+(.*)$')
OSD = re.compile(r'stats: rendered ([\d.]+) fps.*draws (\d+)')


def corr(a, b):
    ma, mb = st.mean(a), st.mean(b)
    num = sum((x - ma) * (y - mb) for x, y in zip(a, b))
    da = sum((x - ma) ** 2 for x in a) ** .5
    db = sum((y - mb) ** 2 for y in b) ** .5
    return num / da / db if da and db else float('nan')


def main():
    args, top, minrip, maxfps = [], 45, 40.0, 59.0
    it = iter(sys.argv[1:])
    for a in it:
        if a == '--top': top = int(next(it))
        elif a == '--minrip': minrip = float(next(it))
        elif a == '--maxfps': maxfps = float(next(it))
        else: args.append(a)
    reps = []
    for f in args:
        fps = dr = None; cur = None
        for l in open(f, encoding='utf-8', errors='replace'):
            m = OSD.search(l)
            if m:
                fps, dr = float(m.group(1)), int(m.group(2)); continue
            m = HDR.search(l)
            if m:
                cur = {'f': f, 'rip': float(m.group(2)), 'fps': fps, 'dr': dr, 'rows': collections.Counter()}
                reps.append(cur); continue
            m = ROW.search(l)
            if m and cur:
                cur['rows'][m.group(2).strip().replace('fn 0x', '')] += float(m.group(1))
    g = [r for r in reps if r['rip'] > minrip and r['fps'] and r['fps'] < maxfps]
    if not g:
        print('no gameplay reports'); return
    ms = lambda r, v: v * 10 / r['fps']          # share % -> ms/frame
    print(f'{len(g)} gameplay reports of {len(reps)}; fps median {st.median(r["fps"] for r in g):.1f}, '
          f'draws/frame median {st.median(r["dr"] for r in g):.0f}, frame {st.median(1000 / r["fps"] for r in g):.2f} ms, '
          f'game code {st.median(ms(r, r["rip"]) for r in g):.2f} ms/frame')
    tot = collections.Counter()
    for r in g:
        for k, v in r['rows'].items(): tot[k] += ms(r, v)
    print(f'{"mean ms":>8} {"median":>8}  row')
    for k, v in tot.most_common(top):
        print(f'{v / len(g):8.3f} {st.median(ms(r, r["rows"].get(k, 0)) for r in g):8.3f}  {k}')
    listed = [ms(r, sum(v for k, v in r['rows'].items() if not k.startswith('['))) for r in g]
    print(f'\ngame code in listed rows {st.median(listed):.2f} ms, below the per-report cut (tail) '
          f'{st.median(ms(r, r["rip"]) - l for r, l in zip(g, listed)):.2f} ms (medians)')
    de = [ms(r, sum(r['rows'].get(k, 0) for k in ('1E81AE', '1E80B0', '1E8065'))) * 1e6 / r['dr'] for r in g if r['dr']]
    if de: print(f'DrawEmit 1E80B0 (all fragments) {st.median(de):.0f} ns per draw (median)')
    a = [r['rows'].get('[ntdll.dll] <- 5DA48', 0) for r in g]
    b = [r['rows'].get('[ntdll.dll] <- 1180A4', 0) for r in g]
    c = [r['rip'] for r in g]
    print(f'r([ntdll] <- 5DA48, [ntdll] <- 1180A4 fence wait) = {corr(a, b):.3f}; r(5DA48, RIP in game) = {corr(a, c):.3f}')


if __name__ == '__main__':
    main()
