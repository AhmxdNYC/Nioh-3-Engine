#!/usr/bin/env python3
"""rp_sim.py - replay a ReadProbe=2 trace (Nioh3EngineFix_reads_<n>.bin + .txt sidecar).
Predicts RC (read cache) hit rate / in-call time per block+cache size, AQ (async queue) times per depth, and the PF
(1.7.26 trace prefetch) upper bound: every read at 631FD5 / 31305A / 3130D6 served from the page cache.
Usage: py -I rp_sim.py TRACE.bin [--log modlog.txt] [--cached-us N --dev-4k-us N --dev-64k-us N --dev-256k-us N]
         [--rc-max-kb 64] [--event-us N --cached-ovl-us N] [--model] [--json] | py -I rp_sim.py --selftest"""
import sys, os, re, json, struct, argparse, heapq, tempfile, random, io
from collections import defaultdict

HDR = struct.Struct('<8sQQQQII')
REC = struct.Struct('<QQIIHBBBBBB')  # t0 off n us slot site flags cls inflG inflH loc
LOC = {0: 'reread', 1: 'seq', 2: 'near', 3: 'inblk', 4: 'far', 255: 'none'}
SAVE_RVA, AQ_RVAS = '1FD9CA3', ('31305A', '3130D6', '740392')
PF_RVAS = ('631FD5', '31305A', '3130D6')
NOOFF = (1 << 64) - 1


def load(path):
    d = open(path, 'rb').read()
    magic, freq, q0, cnt, drop, rs, win = HDR.unpack_from(d, 0)
    assert magic == b'RPTRACE1' and rs == REC.size, 'bad trace header'
    n = min(cnt, (len(d) - HDR.size) // REC.size)
    recs = list(REC.iter_unpack(d[HDR.size:HDR.size + n * REC.size]))
    side = {'sites': {}, 'pools': {}, 'handles': {}}
    tp = os.path.splitext(path)[0] + '.txt'
    if os.path.exists(tp):
        for ln in open(tp, errors='replace'):
            p = ln.rstrip('\n').split('\t')
            if p[0] == 'site' and len(p) >= 3:
                side['sites'][int(p[1])] = p[2].upper()
            elif p[0] == 'pool' and len(p) >= 3:
                side['pools'][int(p[1])] = p[2]
            elif p[0] == 'handle' and len(p) >= 5:
                side['handles'][int(p[1])] = (int(p[2]), int(p[3]), p[4])
    return dict(freq=freq, q0=q0, dropped=drop, window=win, recs=recs, **side)


def parse_calib(logf):
    c = {}
    if not logf:
        return c
    last = None
    for ln in open(logf, errors='replace'):
        if 'ReadProbe calib' in ln and 'cached 4K' in ln:
            last = ln
    if last:
        for k, rx in (('cached_us', r'cached 4K sync ([\d.]+)'), ('cached_ovl_us', r'cached 4K sync [\d.]+ us ovl ([\d.]+)'),
                      ('dev_4k_us', r'median 4K ([\d.]+)'), ('dev_64k_us', r'64K ([\d.]+) us 256K'),
                      ('dev_256k_us', r'256K ([\d.]+)'), ('event_us', r'CreateEventA\+CloseHandle ([\d.]+)')):
            m = re.search(rx, last)
            if m:
                c[k] = float(m.group(1))
    return c


class Dev:
    """Device cost (us) by read size, piecewise-linear through the 4K/64K/256K calibration points."""
    def __init__(s, a4, a64, a256):
        s.pts = [(4096, a4), (65536, a64), (262144, a256)]

    def __call__(s, size):
        p = s.pts
        if size <= p[0][0]:
            return p[0][1]
        for (x0, y0), (x1, y1) in zip(p, p[1:]):
            if size <= x1:
                return y0 + (y1 - y0) * (size - x0) / (x1 - x0)
        (x0, y0), (x1, y1) = p[-2], p[-1]
        return y1 + (y1 - y0) * (size - x1) / (x1 - x0)


class Clock:
    def __init__(s, cap):
        s.cap = max(1, cap)
        s.keys, s.ref, s.idx, s.hand = [], [], {}, 0

    def has(s, k):
        i = s.idx.get(k)
        if i is None:
            return False
        s.ref[i] = 1
        return True

    def add(s, k):
        if len(s.keys) < s.cap:
            s.idx[k] = len(s.keys)
            s.keys.append(k)
            s.ref.append(1)
            return
        while s.ref[s.hand]:
            s.ref[s.hand] = 0
            s.hand = (s.hand + 1) % s.cap
        del s.idx[s.keys[s.hand]]
        s.keys[s.hand] = k
        s.ref[s.hand] = 1
        s.idx[k] = s.hand
        s.hand = (s.hand + 1) % s.cap


def eligible(t, rc_max):
    """Reads RC would serve: known offset and handle, <= RCMaxReadKB, not an error, not save data (1FD9CA3)."""
    out = []
    for r in t['recs']:
        off, n, slot, site, fl = r[1], r[2], r[4], r[5], r[6]
        if off == NOOFF or slot == 0xFFFF or n == 0 or n > rc_max * 1024 or fl & 4:
            continue
        if t['sites'].get(site, '') == SAVE_RVA:
            continue
        out.append(r)
    return out


def sim_rc(elig, bs, mb, dev, total_in_s):
    cl = Clock(mb * 1048576 // bs)
    hits = hb = tot_b = miss_blocks = 0
    pred = rec = 0.0
    for r in elig:
        off, n, us, slot = r[1], r[2], r[3], r[4]
        miss = [b for b in range(off // bs, (off + n - 1) // bs + 1) if not cl.has((slot, b))]
        copy = 1.0 + n / 1024.0
        rec += us
        tot_b += n
        if not miss:
            hits += 1
            hb += n
            pred += copy
        else:
            for b in miss:
                cl.add((slot, b))
            miss_blocks += len(miss)
            pred += len(miss) * dev(bs) + copy
    N = len(elig)
    return dict(block_kb=bs // 1024, cache_mb=mb, calls=N, hit_calls=round(hits / N, 4) if N else 0,
                hit_bytes=round(hb / tot_b, 4) if tot_b else 0, miss_block_reads=miss_blocks,
                extra_mb=round(max(0, miss_blocks * bs - (tot_b - hb)) / 1048576, 1),
                rec_s=round(rec / 1e6, 3), pred_s=round(pred / 1e6, 3),
                load_pred_s=round(total_in_s - rec / 1e6 + pred / 1e6, 3))


def sim_aq(t, freq, dev, cached_us, use_model):
    """c workers, FIFO, arrivals at recorded t0. Step threads pay only cached_us (submit); latency = queue wait + service."""
    rs = sorted((r for r in t['recs'] if t['sites'].get(r[5], '') in AQ_RVAS and r[1] != NOOFF), key=lambda r: r[0])
    out = []
    if not rs:
        return out
    base = rs[0][0]
    for c in (1, 2, 3, 4, 6, 8):
        free = [0.0] * c
        lat, end = [], 0.0
        for r in rs:
            arr = (r[0] - base) * 1e6 / freq
            svc = dev(r[2]) if use_model else float(r[3])
            fin = max(heapq.heappop(free), arr) + svc
            heapq.heappush(free, fin)
            lat.append(fin - arr)
            end = max(end, fin)
        lat.sort()
        n = len(rs)
        out.append(dict(depth=c, calls=n, submit_s=round(n * cached_us / 1e6, 3), rec_in_call_s=round(sum(r[3] for r in rs) / 1e6, 3),
                        lat_mean_us=round(sum(lat) / n, 1), lat_p95_us=round(lat[min(n - 1, int(n * .95))], 1),
                        makespan_s=round(end / 1e6, 3), span_rec_s=round((rs[-1][0] - base) / freq + rs[-1][3] / 1e6, 3)))
    return out


def verdict(S, cal, rc_ref):
    L = S['loc']
    tot = sum(L.values()) or 1
    good = (L.get('seq', 0) + L.get('near', 0) + L.get('inblk', 0)) / tot
    far = L.get('far', 0) / tot
    c4, d4, d64 = cal['cached_us'], cal['dev_4k_us'], cal['dev_64k_us']
    v = []
    if c4 >= 40:
        v.append('RC (per-call Wine overhead)')
    elif c4 <= 15 and d64 <= 1.5 * d4 and good >= .6:
        v.append('RC (device latency, good locality)')
    elif good >= .6 and rc_ref and rc_ref['hit_calls'] >= .6:
        v.append('RC (locality + sim hit rate >= 60 %)')
    if c4 <= 15 and d64 <= 1.5 * d4 and far >= .5:
        v.append('AQ then TSY (device latency, random)')
    if cal.get('event_us', 0) >= 15 and S['calls'] >= 300000:
        v.append('EP (event churn)')
    if cal.get('cached_ovl_us') and cal['cached_ovl_us'] >= 1.5 * c4:
        v.append('SH (overlapped handles cost extra, Ally only)')
    return '; '.join(v) or 'no rule fires (see tables)', good, far


def summarize(t):
    S = dict(calls=0, mb=0.0, in_call_s=0.0, site={}, pool={}, loc=defaultdict(int))
    for r in t['recs']:
        n, us, site, cls, loc = r[2], r[3], r[5], r[7], r[10]
        S['calls'] += 1
        S['mb'] += n / 1048576
        S['in_call_s'] += us / 1e6
        for d, k in ((S['site'], t['sites'].get(site, str(site))), (S['pool'], t['pools'].get(cls, str(cls)))):
            e = d.setdefault(k, [0, 0.0, 0.0])
            e[0] += 1
            e[1] += n / 1048576
            e[2] += us / 1e6
        S['loc'][LOC.get(loc, str(loc))] += 1
    S['loc'] = dict(S['loc'])
    return S


def sim_pf(t, cached_ovl_us, copy_mb_s=8000.0):
    # PF upper bound: a prefetched read costs the cached overlapped call + the copy, never more than it did
    by = {}
    for r in t['recs']:
        site = t['sites'].get(r[5], str(r[5]))
        if site not in PF_RVAS:
            continue
        n, us = r[2], r[3]
        e = by.setdefault(site, [0, 0.0, 0.0])
        e[0] += 1
        e[1] += us / 1e6
        e[2] += min(us, cached_ovl_us + n / copy_mb_s) / 1e6
    return {k: dict(calls=v[0], rec_s=round(v[1], 2), pred_s=round(v[2], 2)) for k, v in by.items()}


def run(a, out=sys.stdout):
    t = load(a.trace)
    cal = parse_calib(a.log)
    for k, dv in (('cached_us', 10.0), ('dev_4k_us', 95.0), ('dev_64k_us', 130.0), ('dev_256k_us', 330.0)):
        v = getattr(a, k)
        cal[k] = v if v is not None else cal.get(k, dv)
    if a.event_us is not None:
        cal['event_us'] = a.event_us
    if a.cached_ovl_us is not None:
        cal['cached_ovl_us'] = a.cached_ovl_us
    dev = Dev(cal['dev_4k_us'], cal['dev_64k_us'], cal['dev_256k_us'])
    S = summarize(t)
    elig = eligible(t, a.rc_max_kb)
    P = lambda *x: print(*x, file=out)
    P(f"trace window {t['window']}: {S['calls']} calls, {S['mb']:.0f} MB, in-call {S['in_call_s']:.2f} s, dropped {t['dropped']}")
    P('calib: ' + ', '.join(f'{k} {v:g}' for k, v in cal.items()))
    P('per site (rva: calls, MB, in-call s):')
    for k, e in sorted(S['site'].items(), key=lambda x: -x[1][2]):
        P(f'  {k:>9}: {e[0]:>9} {e[1]:>9.0f} {e[2]:>8.2f}')
    P('per thread class:')
    for k, e in sorted(S['pool'].items(), key=lambda x: -x[1][2]):
        P(f'  {k:>12}: {e[0]:>9} {e[1]:>9.0f} {e[2]:>8.2f}')
    n = S['calls'] or 1
    P('locality (recorded): ' + ' '.join(f'{k} {100 * v / n:.0f}%' for k, v in S['loc'].items()))
    P(f'RC sim ({len(elig)} eligible reads <= {a.rc_max_kb} KiB, CLOCK):')
    P('  blk   MB hit%calls hit%bytes missblk extraMB  rec_s  pred_s  load_pred_s')
    rc = []
    for bs in (16, 64, 256):
        for mb in (32, 64, 128, 256):
            r = sim_rc(elig, bs * 1024, mb, dev, S['in_call_s'])
            rc.append(r)
            P(f"  {bs:>3} {mb:>4} {100 * r['hit_calls']:>8.1f} {100 * r['hit_bytes']:>9.1f} {r['miss_block_reads']:>7} {r['extra_mb']:>7} "
              f"{r['rec_s']:>6} {r['pred_s']:>7} {r['load_pred_s']:>8}")
    aq = sim_aq(t, t['freq'], dev, cal['cached_us'], a.model)
    P('AQ sim (sites ' + ','.join(AQ_RVAS) + f"; service = {'device model' if a.model else 'recorded us'}):")
    P('  depth calls submit_s rec_in_call_s lat_mean_us lat_p95_us makespan_s span_rec_s')
    for r in aq:
        P(f"  {r['depth']:>5} {r['calls']:>5} {r['submit_s']:>8} {r['rec_in_call_s']:>12} {r['lat_mean_us']:>11} "
          f"{r['lat_p95_us']:>10} {r['makespan_s']:>10} {r['span_rec_s']:>10}")
    pf = sim_pf(t, cal.get('cached_ovl_us', 60.0))
    P(f"PF sim (sites {','.join(PF_RVAS)}; every read a page-cache hit at {cal.get('cached_ovl_us', 60.0):g} us + copy):")
    for k, r in sorted(pf.items()):
        P(f"  {k:>9}: {r['calls']:>7} calls, in-call {r['rec_s']:>7} s -> {r['pred_s']:>7} s")
    ref = next((r for r in rc if r['block_kb'] == 64 and r['cache_mb'] == 64), None)
    vd, good, far = verdict(S, cal, ref)
    P(f'VERDICT: {vd} (seq+near+inblk {100 * good:.0f}%, far {100 * far:.0f}%)')
    return dict(summary=S, calib=cal, rc=rc, aq=aq, pf=pf, verdict=vd)


def synth(path, kind, n=20000):
    random.seed(1)
    recs, off = [], 0
    for i in range(n):
        if kind == 'seq':
            o = off
            off += 4096
        else:
            o = random.randrange(0, 1 << 20) << 12  # 4 GiB file
        recs.append(REC.pack(1000 + i * 100, o, 4096, 60 if kind == 'seq' else 600, 0, 1 if i % 4 else 2, 1, 0, 1, 1,
                             1 if kind == 'seq' else 4))
    with open(path, 'wb') as f:
        f.write(HDR.pack(b'RPTRACE1', 1000000, 0, n, 0, REC.size, 1) + b''.join(recs))
    with open(os.path.splitext(path)[0] + '.txt', 'w') as f:
        f.write('site\t0\t1FD9CA3\nsite\t1\t740392\nsite\t2\t631FD5\npool\t0\tTrySync\nhandle\t0\t0\t4294967296\troot.rdb\n')


def selftest():
    d = tempfile.mkdtemp()
    res = {}
    for kind in ('seq', 'rnd'):
        p = os.path.join(d, f'Nioh3EngineFix_reads_{kind}.bin')
        synth(p, kind)
        a = parse([p])
        res[kind] = run(a, io.StringIO())
        assert res[kind]['aq'], 'aq empty'
    h = lambda k: next(r for r in res[k]['rc'] if r['block_kb'] == 64 and r['cache_mb'] == 64)
    assert h('seq')['hit_calls'] > .9, h('seq')
    assert h('rnd')['hit_calls'] < .2, h('rnd')
    assert h('seq')['pred_s'] < h('seq')['rec_s']
    pf = res['rnd']['pf'].get('631FD5')
    assert pf and pf['calls'] == 5000 and pf['pred_s'] < pf['rec_s'] / 5, pf
    print('RP_SIM SELFTEST OK')


def parse(argv):
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument('trace', nargs='?')
    p.add_argument('--log')
    p.add_argument('--json', action='store_true')
    for k in ('cached-us', 'dev-4k-us', 'dev-64k-us', 'dev-256k-us', 'event-us', 'cached-ovl-us'):
        p.add_argument('--' + k, type=float)
    p.add_argument('--rc-max-kb', type=int, default=64)
    p.add_argument('--model', action='store_true', help='AQ service time from device model instead of recorded us')
    p.add_argument('--selftest', action='store_true')
    return p.parse_args(argv)


if __name__ == '__main__':
    a = parse(sys.argv[1:])
    if a.selftest:
        selftest()
    elif not a.trace:
        sys.exit('trace .bin required (or --selftest)')
    else:
        r = run(a, sys.stderr if a.json else sys.stdout)
        if a.json:
            print(json.dumps(r, indent=1))
