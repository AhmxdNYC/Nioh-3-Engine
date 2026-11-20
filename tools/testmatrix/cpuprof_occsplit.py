import csv,sys,collections
for f in sys.argv[1:]:
    rows=list(csv.DictReader(open(f),delimiter='\t'))
    th=collections.defaultdict(dict)
    for r in rows:
        th[(r['tid'],r['thread'])][(r['view'],r['function'])]=(float(r['pct']),float(r['busy'] or 0))
    print('==',f)
    for (tid,name),d in th.items():
        b=d.get(('incl','Nioh3+B616A4')); c=d.get(('incl','Nioh3+3D7CC8')); w=d.get(('incl','KERNELBASE.dll!SleepConditionVariableSRW'))
        if (b and b[0]>=1) or (c and c[0]>=1):
            print(f"{tid:>6} {name[:32]:32} B616A4 {b} 3D7CC8 {c} SleepCV {w} self-top {sorted(((v[0],k[1]) for k,v in d.items() if k[0]=='self'),reverse=True)[:3]}")
