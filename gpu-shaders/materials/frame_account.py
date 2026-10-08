r"""frame_account.py - account for EVERY GPU event of the PC RGP r5 trace (marked or not) by pass family and PSO,
and scale the families to the ROG Ally at MAX native 1080p.

    py -I gpu-shaders\materials\frame_account.py [--trace NAME] [--psos] [--timeline] [--ally-ms 59]

Inputs: captures\rgp\<trace>.events.csv (wave-weighted est_us; the rgp_top300 copy has no wave data, so its est_us is a
uniform split inside multi-draw segments) + re\scripts_gpu\out\rgp_top300\<trace>_shaders\pso_map.csv (PSO -> shader
file/group#index) + D:\nioh3-re\gp2_hash_index.json (names of exe-embedded / runtime shaders). Stdlib only.

Families come from the shader group names; exe-embedded shaders are named from the notes (EXE table) or by their place
in the frame (particle simulation window, post/upscale window, UI tail). Ally scaling classes:
  fixed  = same work on the Ally (shadow maps/filters at fixed map size, culling, particle sim, fog froxels)
  render = render-resolution pixel work (PC r5 renders 1920x1080, the Ally at MAX native renders 1920x1080 too)
  output = output-resolution work (PC 2560x1440 -> Ally 1920x1080: x0.5625)
One throughput factor k (Ally us per PC us) is fitted so that the scaled sum + GI-HIGHEST extra + the Ally's measured
FSR3 native AA equals the measured Ally GPU frame (--ally-ms, default 59).
"""
import argparse, csv, collections as C, json, os, re
ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
OUT = os.path.join(ROOT, 're', 'scripts_gpu', 'out', 'rgp_top300')
ap = argparse.ArgumentParser()
ap.add_argument('--trace', default='Nioh3_2026-10-06_19-39-58')
ap.add_argument('--psos', action='store_true'); ap.add_argument('--timeline', action='store_true')
ap.add_argument('--ally-ms', type=float, default=59.0)
a = ap.parse_args()
ev = list(csv.DictReader(open(os.path.join(ROOT, 'captures', 'rgp', a.trace + '.events.csv'))))
pm = list(csv.DictReader(open(os.path.join(OUT, a.trace + '_shaders', 'pso_map.csv'))))
try:
    IDX = json.load(open(r'D:\nioh3-re\gp2_hash_index.json'))
except OSError:
    IDX = {'idx': {}, 'notes': {}}

# exe-embedded / third-party PSOs named in notes\gpu-copies-postres.md, gpu-profile.md, gpu-savings-round2.md
EXE = {'10c323d1': ('temporal filter for thin-surface BSDF (449cf0ae)', 'post (render res, exe)', 'render'),
       '54e610d3': ('4-target copy/convert 7c5acaaf (GBufferEdit copy)', 'G-buffer edit copy (exe)', 'render'),
       'bc8a53cc': ('FSR4 ML pass', 'upscaler (FSR4 on PC)', 'output'),
       '2156f246': ('FSR4 pass', 'upscaler (FSR4 on PC)', 'output'),
       'f53ffece': ('FSR4 pass', 'upscaler (FSR4 on PC)', 'output'),
       '5e9935b5': ('output-res 1-tap copy d787dbd0', 'post (output res, exe)', 'output'),
       'c12e69fd': ('bloom downsample', 'post (output res, exe)', 'output'),
       '80896c31': ('depth copy (oDepth = t0)', 'clears / copies / internal', 'render'),
       '07072497': ('CustomDepthUpscale 0cbad2cb', 'post (output res, exe)', 'output'),
       '0aac43e6': ('particle update 5e771e53 (75 x Dispatch(1,1,1))', 'particle simulation (exe CS)', 'fixed'),
       '660f2f60': ('compose', 'post (output res, exe)', 'output'),
       '90e56d9e': ('bloom', 'post (output res, exe)', 'output'), 'ab11dcd7': ('bloom', 'post (output res, exe)', 'output'),
       '06054478': ('tonemap / eye adaptation', 'post (output res, exe)', 'output')}


def grp(loc):
    m = re.search(r'/([^#]+)#', loc or '')
    return m.group(1) if m else ''


mp = {}
for r in pm:
    st = {}
    for s in r['shaders(stage:dxbc_digest=file/group#index)'].split():
        stg, rest = s.split(':', 1); dg, _, loc = rest.partition('=')
        if not loc:
            e = IDX['idx'].get(dg, {})
            loc = 'exe:' + '/'.join(str(x) for x in (e.get('name'), e.get('guess')) if x)
        st[stg] = (dg, loc)
    mp[r['api_pso_hash']] = st

per = C.defaultdict(lambda: dict(us=0.0, n=0, t0=1e9))
for e in ev:
    p = per[e['pso']]; p['us'] += float(e['est_us']); p['n'] += 1; p['t0'] = min(p['t0'], float(e['t_us']))


def family(pso, p):
    """-> (family, scale class, label)"""
    if pso == 'ffffffffffffffff':
        return 'clears / copies / internal', 'render', 'clears / copies / driver internal'
    for k, v in EXE.items():
        if pso.startswith(k):
            return v[1], v[2], v[0]
    st = mp.get(pso, {})
    ps, cs, vs = (grp(st.get(s, ('', ''))[1]) for s in ('pixel', 'compute', 'vertex'))
    main = cs or ps or vs
    lab = ' / '.join(v[1].split('.file/')[-1] for v in st.values())[:70] or pso
    t = p['t0']
    if not st:
        return 'clears / copies / internal', 'render', 'unmapped PSO'
    if re.match(r'RefProbe', main):
        return 'GI / reflection probes (RefProbe)', 'render', lab
    if re.match(r'VFSCAT|VCLOUD', main):
        return 'volumetric fog / clouds', 'fixed', lab
    if re.match(r'ESM|WDO|GHDM', main) and not ps.startswith(('PB2', 'PBRLR')):
        return 'shadow filtering (ESM/WDO/GHDM)', 'render' if main.startswith('WDO') else 'fixed', lab
    if re.match(r'MPC|MshLod|MCLRC|MGLRC|CTS|MIC|GVD|IGW|IGP|IGN|CLC|CBC|DYM|GFM|TREE2WIND', main):
        return 'GPU culling / meshlet LOD / indirect args', 'fixed', lab
    if re.match(r'G1E|G1S|PFB', main):
        return 'particles (draws + PFB)', 'render' if ps else 'fixed', lab
    if re.match(r'SM-|SMTREE2|LSSM|DEPTH', ps or vs) and not re.match(r'PB2', ps):
        return 'shadow-map / depth draws', 'fixed', lab
    if vs.startswith('WDO_SST') or ps.startswith(('PB2DS', 'PB2MRLR', 'PBRLR', 'ARPSSS', 'TOONEDGE')) or cs.startswith('TOONEDGE'):
        return 'deferred lighting / SSS / SSR / outline (full-screen)', 'render', lab
    if ps and ('Gb' in ps or ps.startswith(('PB2', 'STD', 'UMTV', 'SRIVER', 'WET'))):
        if t > 6400:   # after deferred lighting started: water / river / translucent material draws
            return 'water / translucent material draws', 'render', lab
        return 'G-buffer material draws', 'render', lab
    if re.match(r'WET', main):
        return 'water / translucent material draws', 'render', lab
    if re.match(r'AMBSL', main):
        return 'UI', 'output', lab
    if 'exe' in ''.join(v[1] for v in st.values()):
        if 1300 <= t < 1720:
            return 'particle simulation (exe CS)', 'fixed', 'exe CS ' + pso[:8]
        if 9000 <= t < 10236:
            return 'post (output res, exe)', 'output', 'exe post ' + pso[:8]
        if t >= 10236:
            return 'UI', 'output', 'exe UI/final ' + pso[:8]
        if 8000 <= t < 9000:
            return 'post (render res, exe)', 'render', 'exe ' + pso[:8]
        return 'other exe-embedded', 'render', 'exe ' + pso[:8]
    return 'other', 'render', lab


rows = []
for pso, p in per.items():
    f, cl, lab = family(pso, p)
    rows.append((p['us'], pso, f, cl, lab, p))
tot = sum(r[0] for r in rows)
fam = C.defaultdict(lambda: [0.0, 0, 0, '', C.Counter()])
for us, pso, f, cl, lab, p in rows:
    x = fam[f]; x[0] += us; x[1] += 1; x[2] += p['n']; x[3] = cl; x[4][lab] += us

SCALE = {'fixed': 1.0, 'render': 1.0, 'output': 0.5625}
GI_EXTRA_PC = 1804.0          # notes/gpu-maxsettings.md §3: RefProbe gathers at scale 1.0 (HIGHEST) vs HIGH
FSR3_AA_ALLY = 6900.0         # STATUS: Ally FSR3 native AA = 6.9 ms (GP-labelled); replaces the PC FSR4 cost
base = sum(v[0] * SCALE[v[3]] for kf, v in fam.items() if not kf.startswith('upscaler')) + GI_EXTRA_PC
k = (a.ally_ms * 1000 - FSR3_AA_ALLY) / base
print(f'trace {a.trace}: {len(ev)} events, {len(per)} PSOs, est GPU work {tot:.0f} us (incl. async queues; frame span ~10.7 ms)')
print(f'Ally model: k = {k:.2f} Ally-us per PC-us (fitted to {a.ally_ms:.0f} ms incl. GI HIGHEST +{GI_EXTRA_PC/1000:.1f} ms '
      f'PC-equiv and FSR3 AA {FSR3_AA_ALLY/1000:.1f} ms)\n')
print('| family | PC us | PC % | PSOs | events | Ally class | Ally est. ms | Ally % |')
print('|---|---|---|---|---|---|---|---|')
out = []
for f, (us, n, ne, cl, labs) in fam.items():
    if f.startswith('upscaler'):
        ally, cl2 = FSR3_AA_ALLY / 1000, 'FSR3 native AA (measured)'
    else:
        ally = k * us * SCALE[cl] / 1000 + (k * GI_EXTRA_PC / 1000 if f.startswith('GI') else 0)
        cl2 = cl + (' + GI HIGHEST' if f.startswith('GI') else '')
    out.append((ally, f, us, n, ne, cl2))
for ally, f, us, n, ne, cl2 in sorted(out, key=lambda r: -r[0]):
    print(f'| {f} | {us:.0f} | {100*us/tot:.1f} | {n} | {ne} | {cl2} | {ally:.1f} | {100*ally/a.ally_ms:.0f} |')
print(f'| **total** | {tot:.0f} | 100 | {len(per)} | {len(ev)} | | {sum(r[0] for r in out):.1f} | |')
if a.psos:
    print('\n== top PSOs per family (PC us) ==')
    for f, (us, n, ne, cl, labs) in sorted(fam.items(), key=lambda kv: -kv[1][0]):
        print(f'{f}: ' + '; '.join(f'{l} {u:.0f}' for l, u in labs.most_common(6)))
if a.timeline:
    segs = C.OrderedDict()
    fo = {pso: f for us, pso, f, cl, lab, p in rows}
    for e in ev:
        s = segs.setdefault((e['queue'], int(e['seg'])), dict(t=float(e['t_us']), us=0.0, fam=C.Counter(), n=0))
        s['us'] += float(e['est_us']); s['fam'][fo[e['pso']]] += float(e['est_us']); s['n'] += 1
    print('\n== GFX timeline (consecutive segments merged by dominant family, >= 15 us) ==')
    run, outl = None, []
    for (q, sid), s in sorted(segs.items(), key=lambda kv: kv[1]['t']):
        if q != 'GFX':
            continue
        dom = s['fam'].most_common(1)[0][0]
        if run and run['dom'] == dom:
            run['us'] += s['us']; run['n'] += s['n']
        else:
            run = dict(t=s['t'], dom=dom, us=s['us'], n=s['n']); outl.append(run)
    for r in outl:
        if r['us'] >= 15:
            print(f"{r['t']:8.0f} us  {r['us']:7.1f}  ev={r['n']:4d}  {r['dom']}")
