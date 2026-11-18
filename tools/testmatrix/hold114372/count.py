# count.py arm-*.jsonl...: per input, the first new action after the press, its lead time; Y clean = MA 0x629d by 0.4 s.
import json, sys, statistics, collections
for f in sys.argv[1:]:
    rows = [json.loads(l) for l in open(f)]
    print(f)
    for inp in ("Y", "RB", "LB"):
        R = [r for r in rows if r["inp"] == inp]
        firsts = collections.Counter(); leads = []; ma = []
        for r in R:
            a = [(t, x) for t, x in r["acts"] if t >= 0]
            if a: firsts[a[0][1]] += 1; leads.append(a[0][0])
            m = [t for t, x in a if x == "0x629d"]; ma.append(m[0] if m else None)
        fm = [r["fps_med"] for r in R if r["fps_med"]]
        line = "  %-2s n=%d fps_med=%s first=%s lead_med=%s" % (inp, len(R), statistics.median(fm) if fm else None,
               dict(firsts.most_common(3)), round(statistics.median(leads), 3) if leads else None)
        if inp == "Y":
            ok = sum(1 for m in ma if m is not None and m <= 0.4); late = sum(1 for m in ma if m is not None and m > 0.4)
            line += " MA clean %d late %d none %d leads %s" % (ok, late, ma.count(None), [m for m in ma])
        else:
            line += " leads %s seqs %s" % ([round(x, 3) for x in leads], [ [x for t, x in r["acts"]][:3] for r in R][:3])
        print(line)
