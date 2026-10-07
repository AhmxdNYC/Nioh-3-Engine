# Collect hot functions on worker threads (not [App]Game / main) from the PC cpuprof TSVs (self view, cores)
import csv,glob,collections,re,sys
agg=collections.Counter()
caps=glob.glob(r'C:/Users/Ahmad Hamza/nioh3-engine/captures/cpuprof-r[5679]*.tsv')+[r'C:/Users/Ahmad Hamza/nioh3-engine/captures/cpuprof-ingame2.txt.tsv']
for fn in caps:
    for i,l in enumerate(open(fn,encoding='utf-8',errors='replace')):
        if i==0: continue
        p=l.rstrip('\n').split('\t')
        if len(p)<7 or p[2]!='self': continue
        if p[1].startswith('[App]Game'): continue
        m=re.match(r'Nioh3\+([0-9A-F]+)',p[6])
        if not m: continue
        agg[int(m.group(1),16)]+=float(p[4])/100/len(caps)
for rva,c in sorted(agg.items(),key=lambda x:-x[1]):
    if c<0.002: break
    print('%X\t%.4f'%(rva,c))
