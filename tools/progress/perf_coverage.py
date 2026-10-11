"""perf_coverage.py - how much of the profiled time in Nioh3.exe is covered by named functions in the symbol DB.

usage: py -I tools\\progress\\perf_coverage.py [--before] [--self] [--precise] [--list N] [--incl N] [--gpu] [--tsv out.tsv]
       [--dump PATH]

Sources (all from data/, stdlib only):
  ally  every 'SP ' block in data\\profiles\\ally\\sp_*.txt (our ASI sampling profiler on the ROG Ally; rows 'fn 0xRVA' and
        '<- fn 0xRVA'), weighted pct * block samples, normalised per log, averaged over logs.
  pc    data\\profiles\\pc\\cpuprof-*.txt.tsv (our external PC sampler), view 'attr' (time attributed to the first Nioh3
        frame, self + system time below it; top 20 per thread) plus 'self' Nioh3 rows missing from attr (top 30 per
        thread), normalised per profile (share of all samples of that capture), averaged over profiles.
  incl  the 'incl' (inclusive) view of the same captures: the callers on the hot stacks (not part of the coverage %).
  gpu   function starts cited in our GPU notes (data\\gpu_note_rvas.tsv; engine code behind GPU passes).
  --self   count only samples whose RIP is in game code (Ally 'fn' rows, PC 'self' rows): CPU work, waits excluded
  --precise coverage with 2 decimals, and every remaining unnamed function listed with 4 decimals (% of combined game time)
  --before symbols.tsv only (without pending symbols\\incoming\\*.tsv rows)
Every RVA is folded to its primary function start through chained .pdata unwind info in your game dump
(tools\\progress\\common.py says where it is looked for).
'combined' = mean of the ally and pc shares. Coverage = share of profiled game-function time whose function start has a
kind=function row in symbols\\symbols.tsv (+ pending symbols\\incoming\\*.tsv unless --before) with a non-auto name;
'Unknown_*' rows are counted separately (described, role unclear).
Caveat: both samplers measure wall time (blocked threads too), so wait loops weigh as much as compute.
"""
import bisect, collections, glob, os, re, struct, sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import common as C  # noqa: E402

ROOT = C.ROOT
SYMS = C.SYMS
INCOMING = C.incoming_files()
DECOMP = os.environ.get('NIOH3_DECOMP', '')   # optional: a folder with index.tsv of your own decompiler output

B = C.load_dump()
_e = struct.unpack_from('<I', B, 0x3c)[0]
_rva, _sz = struct.unpack_from('<II', B, _e + 24 + 112 + 3 * 8)
RF = sorted(struct.unpack_from('<III', B, _rva + 12 * i) for i in range(_sz // 12))
ST = [r[0] for r in RF]


def primary(x):
    """function start of rva x via .pdata (chained unwind info followed); None if x is in no .pdata range."""
    i = bisect.bisect_right(ST, x) - 1
    if i < 0 or not (RF[i][0] <= x < RF[i][1]):
        return None
    r = RF[i]
    for _ in range(10):
        ui = r[2] & ~1
        flags, cnt = B[ui] >> 3, B[ui + 2]
        if not flags & 4:
            break
        r = struct.unpack_from('<III', B, ui + 4 + ((cnt + 1) & ~1) * 2)
    return r[0]


_GS, _GZ = [], []
for _l in open(C.data('functions.tsv'), encoding='utf-8'):
    _f = _l.split('\t')
    if _f[0].startswith('0x'):
        _GS.append(int(_f[0], 16)); _GZ.append(int(_f[1]))
_o = sorted(range(len(_GS)), key=lambda i: _GS[i]); _GS = [_GS[i] for i in _o]; _GZ = [_GZ[i] for i in _o]


_LEAFSYM = set()
for _l in open(SYMS, encoding='utf-8'):
    _f = _l.split('\t')
    if len(_f) > 4 and _f[0].startswith('0x') and _f[1] == 'function' and ' leaf (no .pdata)' in _f[4]:
        _LEAFSYM.add(int(_f[0], 16))


# code with no .pdata (leaf functions + their cold pieces) -> owning function (data/leaf_ranges.tsv)
_LR, _LE, _LO = [], [], []
_lrp = C.data('leaf_ranges.tsv')
if os.path.exists(_lrp):
    for _l in open(_lrp, encoding='utf-8'):
        _f = _l.rstrip('\n').split('\t')
        if len(_f) == 3 and _f[0] != 'start' and not _f[0].startswith('#'):
            _s = int(_f[0], 16); _LR.append(_s); _LE.append(_s + int(_f[1], 16)); _LO.append(int(_f[2], 16) if _f[2] else _s)


def in_leaf_table(x):
    i = bisect.bisect_right(_LR, x) - 1
    return i >= 0 and x < _LE[i]


def fold(x):
    """function start: .pdata first (chained unwind followed); code without .pdata via data/leaf_ranges.tsv (leaf
    functions and cold pieces, found by disassembling the .pdata gaps); then the Ghidra function table; else x itself."""
    p = primary(x)
    if p is not None:
        return p
    if x in _LEAFSYM:   # a named leaf row in symbols.tsv wins over a Ghidra function whose size wrongly swallows it
        return x
    i = bisect.bisect_right(_LR, x) - 1
    if i >= 0 and x < _LE[i]:
        return _LO[i]
    i = bisect.bisect_right(_GS, x) - 1
    if i >= 0 and x < _GS[i] + max(_GZ[i], 1):
        return _GS[i]
    return x


# Ghidra defaults and the profile-seeded placeholders ('hotspot ... role not identified') count as unnamed
AUTO = re.compile(r'^(FUN_|sub_|LAB_|thunk_|Sleep\d?Call_|\w+Call_[0-9A-F]+$|(Sched0Hot|MainHot|RenderSubmitHot|PauseSpin)_[0-9A-F]+$)')


def load_names(before):
    names = {}
    files = [SYMS] + ([] if before else [f for f in INCOMING if os.path.exists(f)])
    for fn in files:
        for l in open(fn, encoding='utf-8'):
            f = l.rstrip('\n').split('\t')
            if len(f) > 2 and f[0].startswith('0x') and f[1] == 'function':
                r = int(f[0], 16)
                if r not in names or AUTO.match(names[r]) or fn.endswith('-renames.tsv'):  # an incoming name replaces a placeholder
                    names[r] = f[2]
    return names


def ally_shares(selfonly=False):
    hdr = re.compile(r'SP (main \[App\]Game|KIDSTaskScheduler|KIDSTS|SynergisticDispatcher|\[App\] other): (\d+) samples')
    row = re.compile(r'SP (\w+)\s+([\d.]+) %\s+(.*)$')
    gm = {'main [App]Game': 'main', 'KIDSTaskScheduler': 'kidsTS', 'KIDSTS': 'kidsts',
          'SynergisticDispatcher': 'dispatch', '[App] other': 'app'}
    per_log = []
    for log in sorted(glob.glob(os.path.join(C.PROFILES, 'ally', 'sp_*.txt'))):
        agg = collections.Counter(); tot = 0; cur = {}
        for l in open(log, encoding='utf-8', errors='replace'):
            m = hdr.search(l)
            if m:
                cur[gm[m.group(1)]] = int(m.group(2)); tot += int(m.group(2)); continue
            m = row.search(l)
            if m and m.group(1) in cur:
                fm = re.search(r'fn 0x([0-9A-Fa-f]+)', m.group(3))
                if fm and not (selfonly and '<-' in m.group(3)):
                    agg[fold(int(fm.group(1), 16))] += float(m.group(2)) * cur[m.group(1)] / 100
        if tot:
            per_log.append({k: v / tot for k, v in agg.items()})
    return per_log


def pc_shares(selfonly=False):
    per, per_incl = [], []
    for fn in sorted(glob.glob(os.path.join(C.PROFILES, 'pc', 'cpuprof-*.txt.tsv'))):
        thr_tot, leaf = {}, {}
        txt = fn[:-4]
        for l in open(txt, encoding='utf-8', errors='replace'):
            m = re.match(r'## Thread (\d+) .*samples (\d+)', l)
            if m:
                thr_tot[m.group(1)] = int(m.group(2))
            # '(leaf?)' rows carry a guessed start (no .pdata); their hottest RIP gives the real function
            m = re.search(r'Nioh3\+([0-9A-F]+)\(leaf\?\).*\| ([0-9A-F]+):', l)
            if m:
                leaf.setdefault(int(m.group(1), 16), int(m.group(2), 16))
        total = sum(thr_tot.values()) or 1
        attr, selfr, incl = collections.Counter(), collections.Counter(), collections.Counter()
        for l in open(fn, encoding='utf-8'):
            f = l.rstrip('\n').split('\t')
            if len(f) < 7 or f[2] not in ('attr', 'self', 'incl'):
                continue
            m = re.match(r'Nioh3\+([0-9A-Fa-f]+)', f[6])
            if not m:
                continue
            x = int(m.group(1), 16)
            if '(leaf?)' in f[6]:
                rip = leaf.get(x, x)
                fs = fold(rip)
                if fs == rip and not in_leaf_table(rip):
                    fs = x  # RIP in no known function: keep the profiler's guessed start
            else:
                fs = fold(x)
            key = (f[0], fs)
            {'attr': attr, 'self': selfr, 'incl': incl}[f[2]][key] += int(f[3])
        agg, ai = collections.Counter(), collections.Counter()
        for (tid, fs), c in selfr.items():
            agg[fs] += c if selfonly else max(c, attr.get((tid, fs), 0))
        for (tid, fs), c in attr.items():
            if (tid, fs) not in selfr and not selfonly:
                agg[fs] += c
        for (tid, fs), c in incl.items():
            ai[fs] += c
        per.append({k: v / total for k, v in agg.items()})
        per_incl.append({k: v / total for k, v in ai.items()})
    return per, per_incl


def mean(dicts):
    out = collections.Counter()
    for d in dicts:
        for k, v in d.items():
            out[k] += v / len(dicts)
    return out


def gpu_rvas():
    """function start -> number of citations in our GPU notes (data/gpu_note_rvas.tsv, extracted from the notes with
    command-buffer ids, graph-file byte offsets and '+0x' module offsets filtered out)"""
    out = collections.Counter()
    p = C.data('gpu_note_rvas.tsv')
    if os.path.exists(p):
        for l in open(p, encoding='utf-8'):
            f = l.rstrip('\n').split('\t')
            if f[0].startswith('0x'):
                out[int(f[0], 16)] += int(f[1])
    return out


def decomp_info():
    info = {}
    p = os.path.join(DECOMP, 'index.tsv')
    if DECOMP and os.path.exists(p):
        for l in open(p, encoding='utf-8', errors='replace'):
            f = l.rstrip('\n').split('\t')
            if f[0].startswith('0x'):
                info[int(f[0], 16)] = f
    return info


def status(names, f):
    n = names.get(f)
    if not n or AUTO.match(n):
        return 'none'
    return 'unknown' if n.startswith('Unknown_') else 'named'


def main():
    a = sys.argv[1:]
    before = '--before' in a
    selfonly = '--self' in a
    precise = '--precise' in a
    nlist = int(a[a.index('--list') + 1]) if '--list' in a else 0
    nincl = int(a[a.index('--incl') + 1]) if '--incl' in a else 0
    names = load_names(before)
    ally_l = ally_shares(selfonly)
    pc_l, incl_l = pc_shares(selfonly)
    ally, pc, incl = mean(ally_l), mean(pc_l), mean(incl_l)
    comb = collections.Counter()
    for k in set(ally) | set(pc):
        comb[k] = (ally.get(k, 0) + pc.get(k, 0)) / 2
    print(f'symbols: {"symbols.tsv only (--before)" if before else "symbols.tsv + pending symbols/incoming/*.tsv"}; '
          f'{len(ally_l)} Ally logs with SP blocks, {len(pc_l)} PC cpuprof captures; '
          f'{"SELF time only (RIP in game code, waits excluded)" if selfonly else "wall time incl. system/wait time attributed to the game caller"}')
    for label, d in (('ally', ally), ('pc', pc), ('combined', comb)):
        tot = sum(d.values()) or 1
        s = collections.Counter()
        for k, v in d.items():
            s[status(names, k)] += v
        nf = sum(1 for k in d if status(names, k) == 'none')
        pf = (lambda x, w: f'{x:{w + 1}.2f}') if precise else (lambda x, w: f'{x:{w}.1f}')
        unnamed = 100 * s['none'] / tot
        print(f'  {label:9s} {len(d):5d} fns, game-attributed time {100 * tot:5.1f} % of samples | covered: named '
              f'{pf(100 * s["named"] / tot, 5)} %  +Unknown_ {100 * s["unknown"] / tot:4.1f} %  = '
              f'{pf(100 * (s["named"] + s["unknown"]) / tot, 5)} % | unnamed '
              f'{(f"{unnamed:.4f}" if precise else f"{unnamed:4.1f}")} % in {nf} fns')
    if precise:
        print('remaining unnamed functions (combined / ally / pc share, % of samples; * = Unknown_ placeholder):')
        left = [k for k in comb if status(names, k) != 'named']
        for k in sorted(left, key=lambda k: -comb[k]):
            print(f'  {k:#x}  {100 * comb[k]:.4f}  {100 * ally.get(k, 0):.4f}  {100 * pc.get(k, 0):.4f}  {"*" if status(names, k) == "unknown" else " "}{names.get(k, "")}')
        print(f'  total {len(left)} fns, {100 * sum(comb[k] for k in left):.4f} % of samples')
    gr = gpu_rvas()
    gn = sum(1 for k in gr if status(names, k) != 'none')
    print(f'  gpu-notes {len(gr):5d} fns cited in GPU notes, {gn} named ({100 * gn / max(len(gr), 1):.0f} %)')
    ic = [k for k, v in incl.items() if v >= 0.003]
    icn = sum(1 for k in ic if status(names, k) != 'none')
    print(f'  incl      {len(ic):5d} fns with >=0.3 % inclusive share on PC, {icn} named')
    info = decomp_info()
    if nlist:
        tot = sum(comb.values())
        print(f'\nTop unnamed by combined share (cum = cumulative share of game time; dc = in NIOH3_DECOMP index):')
        cum = sum(v for k, v in comb.items() if status(names, k) != 'none')
        for k, v in sorted(comb.items(), key=lambda kv: -kv[1]):
            if status(names, k) != 'none':
                continue
            cum += v
            i = info.get(k)
            print(f'  {k:#x}  {100 * v / tot:5.2f} %  ally {100 * ally.get(k, 0):5.2f}  pc {100 * pc.get(k, 0):5.2f}  '
                  f'cum {100 * cum / tot:5.1f}  dc={"y" if i else "n"} size={i[2] if i else "?"}  {names.get(k, "")}')
            nlist -= 1
            if not nlist:
                break
    if nincl:
        print('\nTop unnamed by PC inclusive share:')
        for k, v in sorted(incl.items(), key=lambda kv: -kv[1]):
            if status(names, k) != 'none':
                continue
            i = info.get(k)
            print(f'  {k:#x}  incl {100 * v:5.2f} %  dc={"y" if i else "n"} size={i[2] if i else "?"}')
            nincl -= 1
            if not nincl:
                break
    if '--gpu' in a:
        print('\nUnnamed functions cited in GPU notes:')
        for k, c in gr.most_common():
            if status(names, k) == 'none':
                print(f'  {k:#x}  cites {c}  dc={"y" if k in info else "n"}')
    if '--tsv' in a:
        with open(a[a.index('--tsv') + 1], 'w', encoding='utf-8') as o:
            o.write('rva\tcombined\tally\tpc\tincl\tgpu_cites\tstatus\tname\n')
            for k in sorted(set(comb) | set(incl) | set(gr), key=lambda k: -(comb.get(k, 0) + incl.get(k, 0) / 10)):
                o.write(f'{k:#x}\t{comb.get(k, 0):.6f}\t{ally.get(k, 0):.6f}\t{pc.get(k, 0):.6f}\t{incl.get(k, 0):.6f}\t'
                        f'{gr.get(k, 0)}\t{status(names, k)}\t{names.get(k, "")}\n')


if __name__ == '__main__':
    main()
