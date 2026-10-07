"""timeline.py <trace-prefix> <t0_us> <t1_us> [min_us]
Prints the RGP event timeline (events.csv) between t0 and t1 with shader group names from pso_map.csv
(and from gpu-shaders/dxil_index.csv for PSOs not in pso_map). Events below min_us (default 3) are
collapsed into a count per segment.
"""
import csv, sys, os, collections
pre, t0, t1 = sys.argv[1], float(sys.argv[2]), float(sys.argv[3])
mn = float(sys.argv[4]) if len(sys.argv) > 4 else 3.0
names = {}
pm = pre + "_shaders/pso_map.csv"
if os.path.exists(pm):
    for r in csv.DictReader(open(pm, newline="")):
        s = r["shaders(stage:dxbc_digest=file/group#index)"]
        parts = []
        for tok in s.split():
            st, _, rest = tok.partition(":")
            g = rest.split("=", 1)[1].split("/", 1)[1] if "=" in rest and "/" in rest else "?"
            parts.append(st[:2] + ":" + g)
        names[r["api_pso_hash"]] = " ".join(parts)
skipped = collections.Counter()
for r in csv.DictReader(open(pre + ".events.csv", newline="")):
    t = float(r["t_us"])
    if t < t0 or t > t1:
        continue
    us = float(r["est_us"] or 0)
    if us < mn:
        skipped[r["seg"]] += 1
        continue
    print("%9.1f %4s %-5s %-22s %-10s %s %7.1f %7s  %s" % (t, r["seg"], r["queue"][:5], r["kind"][:22], r["threadgroup"],
          r["pso"], us, r["approx_waves"], names.get(r["pso"], "")))
if skipped:
    print("(+%d small events in %d segments)" % (sum(skipped.values()), len(skipped)))
