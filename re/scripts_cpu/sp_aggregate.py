"""sp_aggregate.py - aggregate the ASI's SP (sampling profiler) blocks from a Nioh3EngineFix log.

usage: py -I sp_aggregate.py <log> [first_profile_index=0] [last_index=-1] [top=25]

Each SP report (every 30 s) has one header per thread group plus up to 25 rows:
  SP <group> <pct> fn 0xRVA                 RIP inside that Nioh3.exe .pdata fragment
  SP <group> <pct> [module] <- fn 0xRVA     RIP in system/driver code, first Nioh3 return address on the stack
Rows are weighted by the block's sample count and summed over the selected profiles. RVAs are folded to their
primary function (chained .pdata unwind info) and named from re/symbols/symbols.tsv. Stdlib only; reads the dump.

Caveats (see notes/cpu-hotspots-ally.md section 1): SP samples blocked threads too, so a share is wall time, not CPU;
the "<- fn" attribution scans the stack for the first valid-looking return address and can land on a stale slot
(e.g. "[ntdll] <- 1180A4" is FenceWait's Sleep(0), "[DINPUT8] <- 4E390" is the ASI loader's Sleep IAT shim).
"""
import re, sys, struct, bisect, collections, os
ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
DUMP = os.path.join(ROOT, 're', 'Nioh3.dump.exe')
SYMS = os.path.join(ROOT, 're', 'symbols', 'symbols.tsv')

def load_pdata():
    b = open(DUMP, 'rb').read()
    e = struct.unpack_from('<I', b, 0x3c)[0]
    dd = e + 24 + 112
    rva, sz = struct.unpack_from('<II', b, dd + 3 * 8)
    rf = sorted(struct.unpack_from('<III', b, rva + 12 * i) for i in range(sz // 12))
    return b, rf, [r[0] for r in rf]

B, RF, ST = load_pdata()

def primary(x):
    i = bisect.bisect_right(ST, x) - 1
    if i < 0 or not (RF[i][0] <= x < RF[i][1]):
        return x
    r = RF[i]
    for _ in range(10):
        ui = r[2] & ~1
        flags, cnt = B[ui] >> 3, B[ui + 2]
        if not flags & 4:
            break
        r = struct.unpack_from('<III', B, ui + 4 + ((cnt + 1) & ~1) * 2)
    return r[0]

names = {}
for l in open(SYMS, encoding='utf-8'):
    f = l.rstrip('\n').split('\t')
    if len(f) > 2 and f[0].startswith('0x') and f[1] in ('function', 'label'):
        names.setdefault(int(f[0], 16), f[2])

HDR = re.compile(r'SP (main \[App\]Game|KIDSTaskScheduler|KIDSTS|SynergisticDispatcher|\[App\] other): (\d+) samples in \d+ s from (\d+) thread\(s\), RIP in Nioh3.exe ([\d.]+) %')
ROW = re.compile(r'SP (\w+)\s+([\d.]+) %\s+(.*)$')
GM = {'main [App]Game': 'main', 'KIDSTaskScheduler': 'kidsTS', 'KIDSTS': 'kidsts',
      'SynergisticDispatcher': 'dispatch', '[App] other': 'app'}

def main():
    log = sys.argv[1]
    lo = int(sys.argv[2]) if len(sys.argv) > 2 else 0
    hi = int(sys.argv[3]) if len(sys.argv) > 3 else -1
    top = int(sys.argv[4]) if len(sys.argv) > 4 else 25
    profs, cur = [], None
    for l in open(log, encoding='utf-8', errors='replace'):
        m = HDR.search(l)
        if m:
            g = GM[m.group(1)]
            if g == 'main':
                cur = {}
                profs.append(cur)
            cur[g] = [int(m.group(2)), int(m.group(3)), float(m.group(4)), []]
            continue
        m = ROW.search(l)
        if m and cur is not None and m.group(1) in cur:
            cur[m.group(1)][3].append((float(m.group(2)), m.group(3).strip()))
    sel = profs[lo:(hi + 1 if hi >= 0 else None)]
    print(f'{len(profs)} profiles in the log, aggregating {len(sel)} (index {lo}..{hi if hi >= 0 else len(profs) - 1})')
    for g in ('main', 'kidsTS', 'kidsts', 'dispatch', 'app'):
        tot = thr = 0; rip = 0.0; agg = collections.Counter()
        for p in sel:
            if g not in p:
                continue
            s, th, r, rows = p[g]
            tot += s; thr = max(thr, th); rip += r * s
            for pct, rest in rows:
                fm = re.search(r'fn 0x([0-9A-F]+)', rest)
                if fm:
                    f = primary(int(fm.group(1), 16))
                    mod = rest.split(']')[0] + ']' if rest.startswith('[') else ''
                    key = f'{mod + " <- " if mod else ""}{f:#x} {names.get(f, "")}'
                else:
                    key = rest
                agg[key] += pct * s / 100
        if not tot:
            continue
        print(f'\n== {g}: {tot} samples, up to {thr} thread(s), RIP in Nioh3.exe {rip / tot:.1f} % '
              f'(~{rip / tot / 100 * thr:.2f} thread-equivalents of game code if all threads were sampled evenly)')
        for k, v in agg.most_common(top):
            print(f'  {100 * v / tot:6.2f} %  {k}')

if __name__ == '__main__':
    main()
