r"""group_rollup.py <events.csv> <pso_map.csv> [pso_map2.csv ...]
Sum est_us of events per shader group (group name without '#index', pixel/compute stage preferred),
using the names from one or more pso_map.csv files. Prints groups sorted by time, plus unnamed PSOs."""
import csv, sys, collections, re
ev, maps = sys.argv[1], sys.argv[2:]
name = {}
for m in maps:
    for r in csv.DictReader(open(m, newline="", encoding="utf-8")):
        s = r["shaders(stage:dxbc_digest=file/group#index)"]
        best = None
        for tok in s.split():
            st, _, rest = tok.partition(":")
            g = rest.split("/", 1)[1] if "/" in rest else ("exe:" + rest[:8] if rest else "?")
            if st in ("pixel", "compute") or best is None:
                best = st[:2] + ":" + g
        name.setdefault(r["api_pso_hash"], best or "?")
tot = collections.defaultdict(float); cnt = collections.Counter(); segs = collections.defaultdict(set)
for r in csv.DictReader(open(ev, newline="")):
    n = name.get(r["pso"], "pso:" + r["pso"])
    if r["pso"] == "ffffffffffffffff":
        n = "(clear/copy/barrier-only)"
    tot[n] += float(r["est_us"] or 0); cnt[n] += 1; segs[n].add(r["seg"])
grp = collections.defaultdict(float)
for n, t in tot.items():
    grp[re.sub(r"#\d+$", "", n)] += t
print("== per PSO name (top 60)")
for n, t in sorted(tot.items(), key=lambda kv: -kv[1])[:60]:
    print("%8.1f us  %4d ev  %s" % (t, cnt[n], n))
print("== per group (top 40)")
for n, t in sorted(grp.items(), key=lambda kv: -kv[1])[:40]:
    print("%8.1f us  %s" % (t, n))
