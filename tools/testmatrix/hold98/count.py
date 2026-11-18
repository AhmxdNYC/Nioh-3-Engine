import json, sys, statistics
for f in sys.argv[1:]:
    late = nohold = 0; leads = []; fm = []
    for l in open(f):
        r = json.loads(l); fm.append(r["fps_med"]) if r["fps_med"] else None
        t = [tt for tt, a in r["acts"] if a == "0x629d"]
        if not t: nohold += 1; continue
        leads.append(t[0]); late += t[0] > 0.4
    print(f, "late %d/40 nohold %d fps_med %s lead_ok_med %.3f" % (late, nohold, statistics.median(fm) if fm else None, statistics.median([x for x in leads if x <= 0.4] or [0])))
