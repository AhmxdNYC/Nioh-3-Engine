import json, sys, statistics
# lead = time of the 2nd act (Y heavy start -> hold-branch act; 0x6311 on the PC2 save, 0x629d on Main PC); late > 0.4 s
for f in sys.argv[1:]:
    late = nohold = 0; leads = []; fm = []; ends = []
    rows = [r for r in map(json.loads, open(f)) if "kind" not in r]
    for r in rows:
        if r["fps_med"]: fm.append(r["fps_med"])
        a = r["acts"]
        if len(a) < 2: nohold += 1; continue
        leads.append(a[1][0]); late += a[1][0] > 0.4; ends.append(a[-1][0])
    ids = sorted(set(r["acts"][1][1] for r in rows if len(r["acts"]) > 1))
    print(f.split("/")[-1], "late %d/%d nohold %d fps_med %s lead min/med/max %.3f/%.3f/%.3f end_med %.3f ids %s" % (late, len(rows), nohold, statistics.median(fm), min(leads), statistics.median(leads), max(leads), statistics.median(ends), ids))
