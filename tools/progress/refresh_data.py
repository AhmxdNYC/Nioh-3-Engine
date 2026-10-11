"""refresh_data.py - MAINTAINERS ONLY: rebuild the published data/ tables from the private working tree.

usage: py -I tools\\progress\\refresh_data.py --private <private repo root> --gp2-index <gp2_hash_index.json>

Contributors never need this: they use the files in data/ as published. It only READS the private tree and writes
data/ in this repository. Everything it writes holds addresses, sizes, our own names, or the few fields of our own
profile captures that the chart scripts parse; it never copies game bytes or game text:
  data/functions.tsv       rva, size of every function Ghidra found (names dropped)
  data/leaf_ranges.tsv     code without .pdata -> owning function (leaf_ranges.py output)
  data/imports.tsv         imports: DLL, function name, IAT rva, callers
  data/string_xrefs.tsv    rva, encoding, referencing functions, pointer slots of every game string that has xrefs;
                           the TEXT column is dropped (make_strings.py re-reads it from your own dump)
  data/partition.tsv       renderer partition (rva, subsystem, depth, size, region; names dropped)
  data/area_overrides.tsv  manual area overrides of progress_treemap.py
  data/gpu_note_rvas.tsv   function starts cited in our GPU notes, with citation counts (for perf_coverage --gpu)
  data/progress_meta.tsv   values the scripts used to read from notes: GPU passes mapped, renderer notes written,
                           and the identity of the game build (PE timestamp, SizeOfImage, .pdata hash)
  data/profiles/ally/sp_NN.txt        Ally sampling-profiler 'SP' header and 'fn 0xRVA' rows (one file per log)
  data/profiles/pc/cpuprof-*.txt(.tsv) PC profiler: per-thread sample totals, leaf RIPs, Nioh3+RVA rows
  data/profiles/gpu/ally_gp2.txt      GP2 frame and per-PSO rows of the GPU capture behind gpu.svg
  data/profiles/gpu/gp2_index.tsv     shader digest -> where it lives / its pass name / replacement status
  data/profiles/gpu/exe_passes.tsv    names we gave to exe-embedded and runtime shaders (digest prefix -> name)
"""
import argparse, collections, glob, hashlib, json, os, re, struct, sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import common as C  # noqa: E402

ap = argparse.ArgumentParser()
ap.add_argument('--private', required=True)
ap.add_argument('--gp2-index', required=True)
A = ap.parse_args()
P = os.path.abspath(A.private)
D = C.DATA


def w(path, lines):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, 'w', encoding='utf-8', newline='\n') as f:
        f.write(''.join(l if l.endswith('\n') else l + '\n' for l in lines))
    print('wrote', os.path.relpath(path, C.ROOT), len(lines), 'lines')


def rd(*p):
    return open(os.path.join(P, *p), encoding='utf-8', errors='replace')


# ---------------------------------------------------------------- static tables
w(os.path.join(D, 'functions.tsv'), ['rva\tsize'] + ['\t'.join(l.split('\t')[:2]) for l in rd('re', 'out', 'functions.tsv')
                                                     if l.startswith('0x')])
w(os.path.join(D, 'leaf_ranges.tsv'), ['# code without .pdata -> owning function (start, size, owner; hex without 0x; '
                                       'empty owner = the range is its own function). Disassembly of the .pdata gaps.']
  + [l.rstrip('\n') for l in rd('re', 'out', 'leaf_ranges.tsv') if not l.startswith('#')])
w(os.path.join(D, 'imports.tsv'), [l.rstrip('\n') for l in rd('re', 'out', 'imports.tsv')])
xr = ['rva\tenc\tfuncs\tvia']
for l in rd('re', 'out', 'strings.tsv'):
    f = l.rstrip('\n').split('\t')
    if f[0].startswith('0x') and (f[2] or f[3]):
        xr.append('\t'.join(f[:4]))
w(os.path.join(D, 'string_xrefs.tsv'), xr)
w(os.path.join(D, 'partition.tsv'), ['# renderer partition: rva\tsubsystem\tdepth(0 seed,50 region-fill)\tsize\tregion']
  + ['\t'.join(l.rstrip('\n').split('\t')[:5]) for l in rd('re', 'scripts_renderer', 'partition.tsv') if l.startswith('0x')])
w(os.path.join(D, 'area_overrides.tsv'), ['# area overrides for progress_treemap.py: <0xRVA or name glob>\t<area> '
                                          '(applied before every rule; RVA beats glob)']
  + [l.rstrip('\n') for l in rd('re', 'scripts_re', 'area_overrides.tsv') if not l.startswith('#')])

# ---------------------------------------------------------------- the dump (identity + .pdata folding)
B = open(os.path.join(P, 're', 'Nioh3.dump.exe'), 'rb').read()
_e = struct.unpack_from('<I', B, 0x3c)[0]
_r, _sz = struct.unpack_from('<II', B, _e + 24 + 112 + 3 * 8)
RF = sorted(struct.unpack_from('<III', B, _r + 12 * i) for i in range(_sz // 12))
ST = [r[0] for r in RF]


def primary(x):
    import bisect
    i = bisect.bisect_right(ST, x) - 1
    if i < 0 or not (RF[i][0] <= x < RF[i][1]):
        return None
    r = RF[i]
    for _ in range(10):
        ui = r[2] & ~1
        if not (B[ui] >> 3) & 4:
            break
        r = struct.unpack_from('<III', B, ui + 4 + ((B[ui + 2] + 1) & ~1) * 2)
    return r[0]


# GPU notes -> cited function starts (same filter as the old perf_coverage.gpu_rvas)
NONFUNC_LINE = re.compile(r'^\| 0xa9|byte offsets are')
cites = collections.Counter()
for p in glob.glob(os.path.join(P, 'notes', 'gpu-*.md')) + [os.path.join(P, 'notes', n) for n in ('d3d12-submission.md', 'invisible-gpu.md')]:
    if not os.path.exists(p):
        continue
    for ln in open(p, encoding='utf-8', errors='replace'):
        if NONFUNC_LINE.search(ln):
            continue
        for pre, h in re.findall(r'([+]?)\b0x([0-9A-Fa-f]{5,7})\b', ln):
            if not pre:
                fs = primary(int(h, 16))
                if fs is not None:
                    cites[fs] += 1
w(os.path.join(D, 'gpu_note_rvas.tsv'), ['rva\tcites'] + ['0x%x\t%d' % kv for kv in cites.items()])

# ---------------------------------------------------------------- meta
gm = re.search(r'now (\d+)/(\d+)\)', rd('STATUS.md').read())
NUM = {'backend': '01', 'materials': '02', 'lighting': '03', 'post': '04', 'frame-graph': '05', 'resources': '06',
       'drawlists': '07', 'meshlet': '08', 'volumetrics': '09', 'effects': '10', 'ui': '11'}
notes = [p for p in NUM if any(os.path.exists(os.path.join(P, 'notes', 'renderer', f)) for f in (p + '.md', NUM[p] + '-' + p + '.md'))]
ts, soi, ph = C.pe_identity(B)
w(os.path.join(D, 'progress_meta.tsv'), [
    'key\tvalue\twhat',
    'gpu_passes_mapped\t%s\tGPU passes whose engine code path is named (was the "now X/Y" count in our status notes)' % ('%s/%s' % gm.groups() if gm else 'n/a'),
    'renderer_notes_written\t%s\trenderer packages that have a written note' % ','.join(notes),
    'dump_timestamp\t%s\tPE TimeDateStamp of the game build all RVAs refer to (Steam v2.02)' % ts,
    'dump_size_of_image\t%s\tSizeOfImage of that build' % soi,
    'dump_pdata_sha256\t%s\tsha256 of its .pdata section (the same in every dump of that build)' % ph])

# ---------------------------------------------------------------- Ally SP profiles
hdr = re.compile(r'SP (main \[App\]Game|KIDSTaskScheduler|KIDSTS|SynergisticDispatcher|\[App\] other): (\d+) samples')
row = re.compile(r'SP (\w+)\s+([\d.]+) %\s+(.*)$')
gm_ = {'main [App]Game': 'main', 'KIDSTaskScheduler': 'kidsTS', 'KIDSTS': 'kidsts', 'SynergisticDispatcher': 'dispatch', '[App] other': 'app'}
for old in glob.glob(os.path.join(D, 'profiles', 'ally', 'sp_*.txt')):
    os.remove(old)
n = 0
for log in sorted(glob.glob(os.path.join(P, 'captures', 'ally', '*.log'))):
    out, cur, tot = [], {}, 0
    for l in open(log, encoding='utf-8', errors='replace'):
        m = hdr.search(l)
        if m:
            cur[gm_[m.group(1)]] = int(m.group(2)); tot += int(m.group(2))
            out.append('SP %s: %s samples' % (m.group(1), m.group(2)))
            continue
        m = row.search(l)
        if m and m.group(1) in cur:
            fm = re.search(r'fn 0x([0-9A-Fa-f]+)', m.group(3))
            if fm:
                out.append('SP %s %s %% %sfn 0x%s' % (m.group(1), m.group(2), '<- ' if '<-' in m.group(3) else '', fm.group(1)))
    if tot:
        n += 1
        w(os.path.join(D, 'profiles', 'ally', 'sp_%02d.txt' % n),
          ['# Ally sampling profiler (SP) blocks: thread-group headers with sample counts, then rows "<group> <pct> % [<- ]fn 0xRVA"'] + out)

# ---------------------------------------------------------------- PC cpuprof profiles
for old in glob.glob(os.path.join(D, 'profiles', 'pc', 'cpuprof-*')):
    os.remove(old)
for fn in sorted(glob.glob(os.path.join(P, 'captures', 'cpuprof-*.txt.tsv'))):
    if 'cpuprof-test' in fn:
        continue
    base = os.path.basename(fn)[:-8]
    txt, seen = [], set()
    for l in open(fn[:-4], encoding='utf-8', errors='replace'):
        m = re.match(r'## Thread (\d+) .*samples (\d+)', l)
        if m:
            txt.append('## Thread %s samples %s' % m.groups())
        m = re.search(r'Nioh3\+([0-9A-F]+)\(leaf\?\).*\| ([0-9A-F]+):', l)
        if m and m.group(1) not in seen:
            seen.add(m.group(1))
            txt.append('Nioh3+%s(leaf?) | %s:' % m.groups())
    w(os.path.join(D, 'profiles', 'pc', base + '.txt'), txt)
    rows = ['tid\t-\tview\tcount\t-\t-\tfunction']
    for l in open(fn, encoding='utf-8'):
        f = l.rstrip('\n').split('\t')
        if len(f) >= 7 and f[2] in ('attr', 'self', 'incl') and re.match(r'Nioh3\+[0-9A-Fa-f]+', f[6]):
            rows.append('\t'.join((f[0], '', f[2], f[3], '', '', f[6].strip())))
    w(os.path.join(D, 'profiles', 'pc', base + '.txt.tsv'), rows)

# ---------------------------------------------------------------- GPU capture (GP2) behind gpu.svg
log = os.path.join(P, 'captures', 'ally', 'nioh3_ally_modlog_175-L2_2112.log')
newer = sorted(glob.glob(os.path.join(P, 'captures', 'ally', '*gp2*.log')), key=os.path.getmtime)
if newer:
    log = newer[-1]
FRAME = re.compile(r'GP2: frame ([\d.]+) ms GPU span.*?covered ([\d.]+) ms.*?async-compute ([\d.]+) ms.*?PSO stamps (ON|OFF)')
ROW = re.compile(r'GP2:\s+(\d+) (.*?) \| +([\d.]+) ms \| +([\d.]+) \| +([\d.]+) \| \[(.*)\]\s*$')
TOK = re.compile(r'\b(ps|vs|gs|hs|ds|cs|ms|as)=([0-9a-f]{32}|f[0-9a-f]{16})')
g, hashes = [], []
for ln in open(log, encoding='utf-8', errors='replace'):
    if 'GP2:' not in ln:
        continue
    if FRAME.search(ln) or ROW.search(ln):
        g.append(ln[ln.index('GP2:'):].rstrip('\n'))
        m = ROW.search(ln)
        if m:
            hashes += [h for _, h in TOK.findall(m.group(2))]
w(os.path.join(D, 'profiles', 'gpu', 'ally_gp2.txt'),
  ['# GP2 per-PSO GPU timing (Ally, one capture): frame lines, then rows "rank shader-set | ms/frame | draws | intervals | [marker]"'] + g)
I = json.load(open(A.gp2_index))
idx_rows = ['hash\tindexed\tkind\tloc\tname\treplacement']
for h in dict.fromkeys(hashes):
    e = I['idx'].get(h)
    if e is None and h not in I['reps']:
        continue
    x = e or {}
    idx_rows.append('\t'.join((h, '1' if e is not None else '0', x.get('kind', ''), x.get('loc', ''), x.get('name', ''),
                               I['reps'].get(h, ''))))
w(os.path.join(D, 'profiles', 'gpu', 'gp2_index.tsv'), idx_rows)
ep = ['key\tname']
for ln in rd('re', 'scripts_gpu', 'exe_passes.tsv'):
    f = ln.rstrip('\r\n').split('\t')
    if len(f) >= 2 and re.match(r'^[0-9a-f]{8}$', f[0]):
        ep.append(f[0] + '\t' + f[1])
w(os.path.join(D, 'profiles', 'gpu', 'exe_passes.tsv'), ep)
