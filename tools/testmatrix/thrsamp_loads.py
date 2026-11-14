import json, re, sys, collections, datetime
RN = {0:"Exec",4:"Sleep",5:"Susp",6:"UserReq",13:"WrUserReq",15:"WrQueue",2:"PageIn",9:"WrPageIn",7:"WrExec",18:"WrRes",31:"WrAlert",37:"WrAlertTid"}
def tsec(s): h,m,x = s.split(":"); return int(h)*3600+int(m)*60+float(x)
out = collections.defaultdict(list)
for L in ("L1","L2","L3"):
    eng = open(L+".eng.log", errors="replace").read().splitlines()
    wins = {}
    for l in eng:
        m = re.match(r"\[(\S+)\] LoadStats #(\d): ([\d.]+) s \|", l)
        if m and int(m.group(2)) in (1,2): wins[int(m.group(2))] = (tsec(m.group(1)) - float(m.group(3)), tsec(m.group(1)), float(m.group(3)))
    thr, secs = {}, []
    for l in open(L+".thr.jsonl"):
        j = json.loads(l)
        if j["k"] == "thr": thr[str(j["tid"])] = j["name"] or j["start"]
        elif j["k"] == "s": secs.append(j)
    for n,(a,b,dur) in wins.items():
        agg = collections.defaultdict(lambda: [0.0,0,0,collections.Counter(),0]); rd = 0; cnt = collections.Counter()
        for s in secs:
            t = tsec(s["t"])
            if not (a < t <= b): continue
            rd += s["rd"]
            for tid,(cpu,run,rdy,w) in s["th"].items():
                g = thr.get(tid,"?"); g = re.sub(r"\d+$","#",g)
                r = agg[g]; r[0]+=cpu; r[1]+=run; r[2]+=rdy; r[3].update({int(k):v for k,v in w.items()}); r[4] += run+rdy+sum(w.values())
        tot_cpu = sum(r[0] for r in agg.values())
        rows = sorted(agg.items(), key=lambda x:-x[1][0])[:8]
        name = "Continue" if n==1 else "BossMatch"
        print(f"{L} #{n} {name} {dur:.1f} s  proc read {rd/1048576/dur:.0f} MB/s  CPU {tot_cpu/1000/dur:.2f} cores")
        for g,r in rows:
            nthr = sum(1 for tid,nm in thr.items() if re.sub(r'\d+$','#',nm)==g)
            sm = r[4] or 1; w = r[3]
            top = ", ".join(f"{RN.get(k,k)} {100*v/sm:.0f}%" for k,v in w.most_common(2))
            print(f"   {g[:40]:40s} cpu {r[0]/1000/dur:5.2f} cores | run {100*r[1]/sm:4.0f}% ready {100*r[2]/sm:3.0f}% | wait {top}")
