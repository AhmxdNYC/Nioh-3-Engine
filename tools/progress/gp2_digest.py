"""gp2_digest.py - read our mod's GPUProf=2 'GP2:' rows and resolve each shader digest (stdlib only).

Used by progress_treemap.py for docs/progress/gpu.svg. Inputs (data/profiles/gpu/):
  ally_gp2.txt    'GP2: frame X ms GPU span ...; covered ...; async-compute ...; PSO stamps ON|OFF' window lines and
                  'GP2: <rank> <vs=<digest> ps=<digest> | cs=<digest> | (copy / resolve / clear)> | <ms> ms | <draws> |
                  <intervals> | [<marker>]' rows. Windows with PSO stamps OFF (A/B overhead windows) are skipped.
  gp2_index.tsv   digest -> indexed (1 = we located it), kind (disk / exe / runtime / replacement), where it lives
                  (shader_dx12 record, exe-embedded rva, runtime-compiled), pass name, and whether we replaced it in
                  private tests. Built from our shader inventory; only the digests of this capture are published.
  exe_passes.tsv  names we gave to exe-embedded / runtime shaders no other index names (key = 8-hex digest prefix).
"""
import os, re, sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import common as C  # noqa: E402

GPU = os.path.join(C.PROFILES, 'gpu')
LOG = os.path.join(GPU, 'ally_gp2.txt')


def load_index(rebuild=False):
    idx, reps = {}, {}
    for ln in open(os.path.join(GPU, 'gp2_index.tsv'), encoding='utf-8'):
        f = ln.rstrip('\n').split('\t')
        if len(f) < 6 or f[0] == 'hash':
            continue
        h, indexed, kind, loc, name, rep = f[:6]
        if indexed == '1':
            idx[h] = {k: v for k, v in (('kind', kind), ('loc', loc), ('name', name)) if v}
        if rep:
            reps[h] = rep
    return {'idx': idx, 'reps': reps, 'notes': {}}


FRAME = re.compile(r'GP2: frame ([\d.]+) ms GPU span.*?covered ([\d.]+) ms.*?async-compute ([\d.]+) ms.*?PSO stamps (ON|OFF)')
ROW = re.compile(r'GP2:\s+(\d+) (.*?) \| +([\d.]+) ms \| +([\d.]+) \| +([\d.]+) \| \[(.*)\]\s*$')
TOK = re.compile(r'\b(ps|vs|gs|hs|ds|cs|ms|as)=([0-9a-f]{32}|f[0-9a-f]{16})')


def parse(paths, last_only):
    wins = []   # dict(span, covered, rows{(desc,marker):(ms,draws,ivals)})
    cur = None
    for p in paths:
        for ln in open(p, encoding='utf-8', errors='replace'):
            if 'GP2:' not in ln:
                continue
            m = FRAME.search(ln)
            if m:
                cur = {'span': float(m.group(1)), 'covered': float(m.group(2)), 'async': float(m.group(3)),
                       'on': m.group(4) == 'ON', 'rows': {}}
                wins.append(cur)
                continue
            m = ROW.search(ln)
            if m and cur is not None:
                cur['rows'][(m.group(2).strip(), m.group(6))] = (float(m.group(3)), float(m.group(4)), float(m.group(5)))
    wins = [w for w in wins if w['on']]
    if last_only and wins:
        wins = wins[-1:]
    return wins


def exe_passes():
    out = {}
    for ln in open(os.path.join(GPU, 'exe_passes.tsv'), encoding='utf-8'):
        f = ln.rstrip('\r\n').split('\t')
        if len(f) >= 2 and re.match(r'^[0-9a-f]{8}$', f[0]):
            out[f[0]] = f[1]
    return out


EXE_PASSES = exe_passes()


def resolve(desc, I):
    idx, reps, notes = I['idx'], I['reps'], I['notes']
    stages, locs, names, rep, nt = [], [], [], [], []
    for st, h in TOK.findall(desc):
        stages.append(st)
        if h.startswith('f') and len(h) == 17:
            locs.append('%s fnv-key %s' % (st, h)); continue
        e = idx.get(h)
        if e:
            locs.append('%s %s' % (st, e.get('loc', '?')))
            if e.get('name') and e['name'] != '(not in inventory)':
                names.append('%s' % e['name'])
        else:
            locs.append('%s UNKNOWN (not in index)' % st)
        if h[:8] in EXE_PASSES and not (e and e.get('kind') == 'disk' and e.get('name')):
            names.append(EXE_PASSES[h[:8]])
        if h in reps:
            rep.append('%s: %s' % (st, reps[h]))
        if h in notes:
            nt.append('%s=%s' % (h[:8], notes[h]))
    return stages, locs, names, rep, nt
