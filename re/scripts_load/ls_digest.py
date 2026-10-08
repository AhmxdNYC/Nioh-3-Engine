# ls_digest.py - one row per LoadStats window (58A) across mod logs, plus ticks (LoadStats = 2) and gate lines if present.
#   py -I re\scripts_load\ls_digest.py <log or dir> [...]
import re, sys, os, glob
W = re.compile(r'LoadStats #(\d+): ([\d.]+) s \| frames (\d+) \(([\d.]+) ms\) \| CPU ([\d.]+) cores \| stage ready ([+-][\d.]+) s \| '
               r'loaders busy ([\d.]+)/([\d.]+) s \(first ([+-][\d.]+), last ([+-][\d.]+)\), in-flight avg A ([\d.]+) B ([\d.]+) of (\d+).*?'
               r'ready-wait ([+-]?[\d.]+)\.\.([+-]?[\d.]+) s \(acc max ([\d.]+).*?event scene ([\d.]+) s')
IO = re.compile(r'LoadStats #\d+ io: read (\d+) MB archive \((\d+) calls, (\d+) MB overlapped, (\d+) ms in-call\) \+ (\d+) MB other '
                r'\((\d+) calls, (\d+) ms\) \| inflate ([\d.]+) CPU-s.*?max (\d+) parallel.*?PSO gfx (\d+) rt.*?cmp (\d+) rt.*?([\d.]+) CPU-s')
files = []
for a in sys.argv[1:]:
    files += sorted(glob.glob(os.path.join(a, '*.log'))) if os.path.isdir(a) else [a]
print('log | win s | frames | ms | CPU | stage+ | mgr busy A | mgr last | inflight A/cap | ready-wait | ev s | MB arch+other | infl CPU-s | PSO CPU-s')
for f in files:
    name = os.path.basename(f)
    rows, io, extra = [], [], []
    for line in open(f, encoding='utf-8', errors='replace'):
        m = W.search(line)
        if m: rows.append(m); continue
        m = IO.search(line)
        if m: io.append(m); continue
        if 'LoadStats tick' in line or 'LoadGate' in line or 'LoadSeq' in line: extra.append(line.rstrip()[15:])
    for i, m in enumerate(rows):
        g = io[i] if i < len(io) else None
        mb = f'{g[1]}+{g[5]}' if g else '-'
        print(f'{name} #{m[1]} | {m[2]} | {m[3]} | {m[4]} | {m[5]} | {m[6]} | {m[7]} | {m[10]} | {m[11]}/{m[13]} | '
              f'{m[14]}..{m[15]} (acc {m[16]}) | {m[17]} | {mb} | {g[8] if g else "-"} | {g[12] if g else "-"}')
    for l in extra[:400]: print('   ', l)
