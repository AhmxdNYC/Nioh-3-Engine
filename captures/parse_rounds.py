"""Collect round 2-4 benchmark runs (task outputs of round2.ps1 / round3.ps1 / round4_physics.ps1) into
captures\\rounds2-4.csv. Usage: py parse_rounds.py <round2.out> <round3.out> <round4.out>"""
import csv, re, sys

INVALID = {(3, "P0a"): "stuck on the autosave notice (UAC prompt stole focus)",
           (3, "P1a"): "stuck on a loading tip (Afterburner dialog stole focus)"}
rows = []
# args: either 3 paths (rounds 2, 3, 4) or N=path pairs
pairs = [(int(a.split("=", 1)[0]), a.split("=", 1)[1]) if "=" in a[:3] else (r, a)
         for r, a in zip((2, 3, 4, 5, 6, 7, 8), sys.argv[1:])]
for rnd, path in pairs:
    cur, f1 = None, ""
    for line in open(path, encoding="utf-8", errors="replace"):
        line = line.strip()
        m = re.match(r"=== (\S+)\s+(.*)", line)
        if m:
            cur, f1 = {"round": rnd, "run": m.group(1), "setup": m.group(2)}, ""
            continue
        if line.startswith("--- F1"):
            f1 = line[4:]
            continue
        if cur is None:
            continue
        m = re.search(r"displayed interval: avg\s+[\d.]+ ms \(\s*([\d.]+) fps\).*1%low\s+([\d.]+) fps.*spikes (\d+)", line)
        if m:
            r = dict(cur, f1=f1 or ("F1 on" if rnd > 2 else ""), avg_fps=m.group(1), low1_fps=m.group(2), spikes=m.group(3))
            key = (rnd, cur["run"])
            r["valid"] = "no: " + INVALID[key] if key in INVALID else "yes"
            rows.append(r)
            continue
        m = re.search(r"GPU busy \(app frames\): avg\s+([\d.]+)", line)
        if m and rows and rows[-1]["run"] == cur["run"]:
            rows[-1]["gpu_busy_ms"] = m.group(1)
        m = re.search(r"CPU busy \(app frames\): avg\s+([\d.]+)", line)
        if m and rows and rows[-1]["run"] == cur["run"]:
            rows[-1]["cpu_busy_ms"] = m.group(1)
        m = re.search(r"Nioh3 CPU: ([\d.]+) cores", line)
        if m and rows and rows[-1]["run"] == cur["run"]:
            rows[-1]["cores"] = m.group(1)
keys = ["round", "run", "setup", "f1", "valid", "avg_fps", "low1_fps", "spikes", "gpu_busy_ms", "cpu_busy_ms", "cores"]
out = r"C:\Users\Ahmad Hamza\nioh3-engine\captures\rounds2-4.csv"
with open(out, "w", newline="", encoding="utf-8") as f:
    w = csv.DictWriter(f, keys)
    w.writeheader()
    for r in rows:
        w.writerow({k: r.get(k, "") for k in keys})
for r in rows:
    print(r["round"], r["run"], r["f1"], r["avg_fps"], r["low1_fps"], r["spikes"], r.get("gpu_busy_ms", ""), r.get("cores", ""), r["valid"][:3])
print("wrote", out, len(rows), "rows")
