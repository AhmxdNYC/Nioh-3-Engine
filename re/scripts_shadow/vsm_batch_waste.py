"""vsm_batch_waste.py <modlog> [...] -- per GP window: VSM updates/frame, "Batch Tiles N" batches/frame and ms; estimates the
redundant share of batch work with s = max(0, 1 - B/T), T = 8*batches per VSM frame, B = per-frame tile budget (env VSM_B,
default 8 = SHADOW_QUAL STANDARD..HIGHEST; 2 at LIGHTEST/LIGHT). See notes/shadows-redundancy.md S1."""
import re,sys,os
B=float(os.environ.get("VSM_B","8"))
pat=re.compile(r"GP:\s+\d+ (.+?)\s+incl\s+([\d.]+) ms\s+self\s+([\d.]+) ms\s+([\d.]+)/frame")
fr=re.compile(r"GP: frame ([\d.]+) ms GPU.*over (\d+) frames")
W=[]
for fn in sys.argv[1:]:
    cur=None
    for l in open(fn,errors="replace"):
        m=fr.search(l)
        if m: cur={"_n":int(m.group(2))};W.append(cur);continue
        m=pat.search(l)
        if m and cur is not None: cur[m.group(1).strip()]=(float(m.group(2)),float(m.group(3)),float(m.group(4)))
fr_tot=sum(w["_n"] for w in W);bt_tot=0;sv_tot=0;sv_hi=0;burst=[]
for w in W:
    v=w.get("VirtualShadowmap")
    bt=[x for k,x in w.items() if k.startswith("Batch Tiles")]
    if not v or not bt: continue
    btc=sum(x[2] for x in bt); bti=sum(x[1] for x in bt)  # self ms
    T=8*btc/max(v[2],0.05)
    s=max(0.0,1-B/T)
    bt_tot+=bti*w["_n"]; sv_tot+=s*bti*w["_n"]
    if s>0: burst.append((round(T),round(s,2),round(bti,2),round(s*bti,2)))
print("frames",fr_tot,"windows",len(W))
print("batch self ms/frame (frame-weighted, all windows): %.3f"%(bt_tot/fr_tot))
print("est. redundant batch ms/frame (frame-weighted): %.3f"%(sv_tot/fr_tot))
print("windows with T>16 (T, s, batch self ms/f, saved ms/f):",burst)
