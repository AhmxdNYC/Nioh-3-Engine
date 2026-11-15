"""verdict.py KEY RUNDIR : ABBA launches in time order A1 B1 B2 A2 -> run1 (A1 vs B1), run2 (A2 vs B2). B = bundle minus KEY.
Earns its place when removing it (B) raises process CPU >= 2 % or drops 1% low >= 1 fps in BOTH runs; else dead weight (#103509)."""
import glob, json, os, statistics, sys
sys.path.insert(0, os.path.dirname(__file__)); from hsumm import arm
key, run = sys.argv[1], sys.argv[2]
L = []
for d in sorted(glob.glob(os.path.join(run, "*-[AB]"))):
    w = arm(d)
    if len(w) < 2: continue
    m = lambda f: statistics.mean(x[f] for x in w)
    L.append({"arm": d[-1], "dir": os.path.basename(d), "cores": m("cores"), "low1": m("low1"), "low01": m("low01"), "fps": m("fps"), "h33": sum(x["h33"] for x in w)})
A = [x for x in L if x["arm"] == "A"]; B = [x for x in L if x["arm"] == "B"]
if len(A) < 2 or len(B) < 2: print(json.dumps({"key": key, "incomplete": [x["dir"] for x in L]})); sys.exit(1)
runs = [(A[0], B[0]), (A[1], B[1])]
cpu = [100 * (b["cores"] / a["cores"] - 1) for a, b in runs]; lo = [a["low1"] - b["low1"] for a, b in runs]
earn = all(c >= 2 for c in cpu) or all(d >= 1 for d in lo)
f = lambda v: [round(x, 2) for x in v]
print(json.dumps({"key": key, "verdict": "EARNS" if earn else "DEAD-WEIGHT?",
    "A_cores": f([a["cores"] for a, _ in runs]), "B_cores": f([b["cores"] for _, b in runs]), "cpu_up_pct_without": f(cpu),
    "A_low1": f([a["low1"] for a, _ in runs]), "B_low1": f([b["low1"] for _, b in runs]), "low1_drop_without": f(lo),
    "A_low01": f([a["low01"] for a, _ in runs]), "B_low01": f([b["low01"] for _, b in runs]),
    "A_fps": f([a["fps"] for a, _ in runs]), "B_fps": f([b["fps"] for _, b in runs])}))
