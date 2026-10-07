import re,glob,collections,sys
# map: site -> next ip (site + len)
sites={}
for l in open('lk_census_out.txt'):
    if 'ZERO-LOAD' not in l: continue
    p=l.split()
    rva=int(p[0],16); ln=int(re.search(r'len (\d+)',l).group(1))
    sites[rva+ln]=(rva,l.strip())
caps=glob.glob(r'C:/Users/Ahmad Hamza/nioh3-engine/captures/cpuprof-*.txt')
hits=collections.defaultdict(list)
for fn in caps:
    name=fn.split('cpuprof-')[1]
    for line in open(fn,encoding='utf-8',errors='replace'):
        m=re.match(r'\s+([\d.]+)%\s+([\d.]+)%\s+Nioh3\+([0-9A-F]+)',line)
        if not m or '|' not in line: continue
        selfpct=float(m.group(1))
        for rip,pct in re.findall(r'([0-9A-F]+):(\d+)%',line.split('|',1)[1]):
            r=int(rip,16)
            if r in sites: hits[sites[r][0]].append((name,selfpct*int(pct)/100))
for s,v in sorted(hits.items(), key=lambda x:-sum(y for _,y in x[1])):
    print('%X  sum %.2f %%-of-thread  over %d thread-captures   e.g. %s' % (s, sum(y for _,y in v), len(v), v[:3]))
