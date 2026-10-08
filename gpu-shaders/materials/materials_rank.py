r"""materials_rank.py - G-buffer material cost concentration from the PC RGP r5 trace.
  py -I gpu-shaders\materials\materials_rank.py [--csv out.csv]
Selects every draw whose pixel shader is a material (group name carries 'Gb' = writes the G-buffer, or PB2/STD/UMTV
families) excluding the full-screen deferred passes (PB2DS/PB2MRLR/PBRLR, WDO_SST vertex shader), sums RGP est_us per
PSO and per pixel-shader record, and attaches gfx1201 ISA stats of the PS (instructions, image samples, max VGPR)."""
import csv, collections as C, os, re, sys
ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
sys.path.insert(0, os.path.join(ROOT, 'gpu-shaders', 'sweep'))
T = 'Nioh3_2026-10-06_19-39-58'
OUT = os.path.join(ROOT, 're', 'scripts_gpu', 'out', 'rgp_top300')
SH = os.path.join(OUT, T + '_shaders')
ev = list(csv.DictReader(open(os.path.join(ROOT, 'captures', 'rgp', T + '.events.csv'))))  
mp = {}
for r in csv.DictReader(open(os.path.join(SH, 'pso_map.csv'))):
    st = {}
    for s in r['shaders(stage:dxbc_digest=file/group#index)'].split():
        stg, rest = s.split(':', 1); dg, _, loc = rest.partition('='); st[stg] = (dg, loc)
    mp[r['api_pso_hash']] = st
SAMPLE = re.compile(r'^\s*image_(sample|gather)'); LOAD = re.compile(r'^\s*(image_load|buffer_load|global_load|s_buffer_load|s_load)')
VREG = re.compile(r'\bv\[?(\d+)(?::(\d+))?\]?')
def isa_ps(pso):
    p = os.path.join(SH, 'isa', pso + '.txt')
    if not os.path.exists(p): return None
    cur = None; L = C.defaultdict(list)
    for l in open(p, encoding='utf-8', errors='replace'):
        m = re.match(r'^(_amdgpu_\w+?_main)\w*:', l)
        if m: cur = m.group(1); continue
        if cur and l.startswith('\t') and not l.strip().startswith('//'): L[cur].append(l.split('//')[0])
    res = {}
    for st, ins in L.items():
        mv = 0
        for x in ins:
            for a, b in VREG.findall(x): mv = max(mv, int(b or a))
        res[st] = dict(n=len(ins), smp=sum(1 for x in ins if SAMPLE.search(x)), ld=sum(1 for x in ins if LOAD.search(x)),
                       vgpr=mv + 1, kill=sum(1 for x in ins if 'v_cmpx' in x or 's_wqm' in x or 'kill' in x))
    return res
def grp(loc):
    m = re.search(r'([0-9a-fx]+\.file)/([^#]+)#(\d+)', loc or ''); return m.groups() if m else ('', '', '')
def is_material(st):
    f, g, i = grp(st.get('pixel', ('', ''))[1]); vf, vg, vi = grp(st.get('vertex', ('', ''))[1])
    if not g or vg.startswith('WDO_SST') or g.startswith(('PB2DS', 'PB2MRLR', 'PBRLR', 'G1E', 'SM-', 'AMBSL')): return False
    return 'Gb' in g or g.startswith(('PB2', 'STD', 'UMTV'))
per = C.defaultdict(lambda: dict(us=0.0, n=0, w=0, t0=1e9, mesh=0))
for e in ev:
    st = mp.get(e['pso'], {})
    if e['queue'] != 'GFX' or not is_material(st): continue
    p = per[e['pso']]; p['us'] += float(e['est_us']); p['n'] += 1; p['w'] += int(e['approx_waves'] or 0)
    p['t0'] = min(p['t0'], float(e['t_us'])); p['mesh'] += e['kind'] in ('api31', 'api30')
tot_frame = sum(float(e['est_us']) for e in ev)
mat = sum(p['us'] for p in per.values())
print(f'material draws: {len(per)} PSOs, {sum(p["n"] for p in per.values())} draws, {mat:.0f} us = {100*mat/tot_frame:.1f}% of {tot_frame:.0f} us')
byps = C.defaultdict(lambda: dict(us=0.0, n=0, psos=0, w=0, isa=None, loc=''))
for pso, p in per.items():
    st = mp[pso]; dg, loc = st['pixel']; b = byps[dg]
    b['us'] += p['us']; b['n'] += p['n']; b['psos'] += 1; b['w'] += p['w']; b['loc'] = loc
    if b['isa'] is None:
        I = isa_ps(pso); b['isa'] = (I or {}).get('_amdgpu_ps_main')
rows = sorted(byps.items(), key=lambda kv: -kv[1]['us'])
print(f'{len(rows)} distinct material pixel shaders')
cum = 0
print(f'{"us":>7s} {"cum%":>5s} {"draws":>5s} {"psos":>4s} {"waves":>6s} {"inst":>5s} {"smp":>4s} {"ld":>4s} {"vgpr":>4s}  pixel shader')
w = csv.writer(open(sys.argv[sys.argv.index('--csv') + 1], 'w', newline='')) if '--csv' in sys.argv else None
if w: w.writerow(['ps_digest', 'file', 'group', 'index', 'us', 'draws', 'psos', 'waves', 'inst', 'samples', 'loads', 'vgpr'])
for dg, b in rows:
    cum += b['us']; I = b['isa'] or dict(n=0, smp=0, ld=0, vgpr=0)
    print(f'{b["us"]:7.1f} {100*cum/mat:5.1f} {b["n"]:5d} {b["psos"]:4d} {b["w"]:6d} {I["n"]:5d} {I["smp"]:4d} {I["ld"]:4d} {I["vgpr"]:4d}  {b["loc"].split(".file/")[-1] if b["loc"] else dg} [{b["loc"].split("/")[0]}]')
    if w: w.writerow([dg, *grp(b['loc']), round(b['us'], 1), b['n'], b['psos'], b['w'], I['n'], I['smp'], I['ld'], I['vgpr']])
