"""Summarise an ibaraki_bench.ps1 run dir: per arm avg FPS, 1% low, process CPU cores, OFF vs ON deltas."""
import csv, json, sys, glob, os, statistics as st
sys.path.insert(0, os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "bench")))
import arm_budget
d = sys.argv[1]
rows = []
for f in sorted(glob.glob(os.path.join(d, "*-presentmon.csv"))):
    tag = os.path.basename(f).split("-presentmon")[0]
    ms = [float(r["MsBetweenPresents"]) for r in csv.DictReader(open(f, encoding="utf-8-sig")) if r["MsBetweenPresents"] not in ("NA", "") and float(r["MsBetweenPresents"]) > 0]
    srt = sorted(ms, reverse=True); k = max(1, -(-len(ms) // 100))
    cpu = None
    cf = os.path.join(d, tag + "-cpu.json")
    if os.path.exists(cf):
        for l in open(cf):
            j = json.loads(l)
            if j.get("kind") == "summary": cpu = j["cpu_cores"]
    hit = arm_budget.hitch(ms)
    rows.append(dict(tag=tag, arm=tag.split("-")[1], frames=len(ms), avg_fps=len(ms) / (sum(ms) / 1000), low1=1000 / (sum(srt[:k]) / k), cpu=cpu, hitch_count=hit["count"], hitch_worst_ms=hit["worst"]))
    print("%-6s frames=%d avg_fps=%.2f 1%%low=%.2f cpu_cores=%s hitch_count=%s hitch_worst_ms=%s" % (tag, len(ms), rows[-1]["avg_fps"], rows[-1]["low1"], cpu and round(cpu, 2), hit["count"], hit["worst"]))
for a in ("OFF", "ON"):
    r = [x for x in rows if x["arm"] == a]
    if r: print(a, "mean avg_fps %.2f low1 %.2f cpu %.2f" % (st.mean(x["avg_fps"] for x in r), st.mean(x["low1"] for x in r), st.mean(x["cpu"] for x in r if x["cpu"])))
o = [x for x in rows if x["arm"] == "OFF"]; n = [x for x in rows if x["arm"] == "ON"]
if o and n:
    fo, fn = st.mean(x["avg_fps"] for x in o), st.mean(x["avg_fps"] for x in n)
    print("FPS change ON vs OFF: %+.1f%%" % ((fn / fo - 1) * 100))
