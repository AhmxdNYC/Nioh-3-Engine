# sp_families.py - aggregate the ASI SP profiler lines of an Ally modlog per thread group and per worker-job family.
#   py sp_families.py <modlog>
import re, sys, collections
f = sys.argv[1]
grp = None; n = 0
agg = collections.defaultdict(collections.Counter); tot = collections.Counter()
for line in open(f, encoding='utf-8', errors='replace'):
    if ' SP ' not in line: continue
    m = re.search(r'\] SP (main \[App\]Game|KIDSTaskScheduler|KIDSTS|SynergisticDispatcher|\[App\] other): (\d+) samples', line)
    if m: grp = m.group(1); n = int(m.group(2)); tot[grp] += n; continue
    m = re.search(r'\] SP (\S+)\s+([\d.]+) %\s+(.*)$', line)
    if m and grp: agg[grp][m.group(3).strip()] += float(m.group(2)) * n / 100
WAIT = ['0x1A6BB84', '0xB616A4', '0xB611BC', '0xB60FFC', '0xB61524']
FAM = {
 'collision (D80B0 loop D8152, D8B70, D8740@D8770, D8D90@D8DC0, 43CEC, D89E0, D6E9C, 168E80)': ['0xD8152','0xD8B70','0xD8770','0xD8DC0','0x43CEC','0xD80B0','0xD89E0','0xD8A3E','0xD6E9C','0x168E80'],
 'stage placeables (DA830@DA879, DBA70, DBB90@DBBE1, DA6E0, DD7AC/DDC70@DDCFF, D8E00)': ['0xDA879','0xDBA70','0xDDCFF','0xDBBE1','0xD8E00','0xDA6E0','0xDBB48','0xDD7AC','0xDA830','0xDBB90'],
 'object update (121FC0@1220BD/1218EC, param scans 121EB3/121EB5/120227, 1A7BF0, 425B0@4280B, 1A188C)': ['0x1220BD','0x121EB3','0x121EB5','0x1218EC','0x121FC0','0x120227','0x1A7BF0','0x4280B','0x1A188C'],
 'job system (411C0, 40800, 3E7CC, 403A0)': ['0x411C0','0x40800','0x3E7CC','0x403A0'],
 'meshlet (48E90@48F19, 4A2D0)': ['0x48F19','0x48E90','0x4A2D0'],
 'AABB (6EA50, 71C60@71D1D, 6EFD0)': ['0x6EA50','0x71D1D','0x71C60','0x6EFD0','0x6EFD4','0x6EFDB','0x6EFDF','0x6F010'],
}
for g in ['KIDSTS', 'KIDSTaskScheduler']:
    rows = agg[g]; t = sum(rows.values())
    wait = sum(v for k, v in rows.items() if k.startswith('[') and any(w in k for w in WAIT))
    nw = t - wait
    print('%s: %d samples, %d in idle waits, %d other (100 %% below)' % (g, t, wait, nw))
    for name, keys in FAM.items():
        s = sum(v for k, v in rows.items() if k.split()[-1] in keys)
        if s: print('   %6.0f  %5.1f %%  %s' % (s, 100 * s / nw, name))
