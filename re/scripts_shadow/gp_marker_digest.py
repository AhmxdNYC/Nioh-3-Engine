"""gp_marker_digest.py <regex> <modlog> [...] -- mean per-marker GPU ms from the ASI GP marker tables ([Profiler] GPUProf>=1).
incl/self are ms per frame averaged over the window (gpuprof.cpp LogLine); only the top-N rows of each window are logged,
so "in k/n win" = windows where the marker made the cut."""
import re,sys,collections
pat=re.compile(r"GP:\s+\d+ (.+?)\s+incl\s+([\d.]+) ms\s+self\s+([\d.]+) ms\s+([\d.]+)/frame")
fr=re.compile(r"GP: frame ([\d.]+) ms GPU.*?(\d+) markers/frame")
keys=re.compile(sys.argv[1])
for fn in sys.argv[2:]:
    blocks=[];cur=None
    for l in open(fn,errors="replace"):
        m=fr.search(l)
        if m: cur={"_f":float(m.group(1))};blocks.append(cur);continue
        m=pat.search(l)
        if m and cur is not None: cur[m.group(1).strip()]=(float(m.group(2)),float(m.group(3)),float(m.group(4)))
    print("==",fn,len(blocks),"windows; mean frame %.2f"%(sum(b["_f"] for b in blocks)/max(1,len(blocks))))
    agg=collections.defaultdict(list)
    for b in blocks:
        for k,v in b.items():
            if k!="_f" and keys.search(k): agg[k].append(v)
    for k,vs in sorted(agg.items(),key=lambda kv:-sum(v[0] for v in kv[1])/len(blocks)):
        n=len(vs);print("  %-28s in %2d/%d win  incl %.3f self %.3f  %.2f/frame (mean over present)"%(k,n,len(blocks),sum(v[0] for v in vs)/n,sum(v[1] for v in vs)/n,sum(v[2] for v in vs)/n))
