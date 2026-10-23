"""Aggregate the pc2 12-run R7B35 batch (FG on/off x STOCKLIKE/PURE/FULL x 2 reps). Read-only on inputs.

  py -I tools/testmatrix/aggregate_fg3arm.py --run-dir <captures/audit-screen/run-pc2-...> --results <captures/fight26758-control/results.csv>
        --schedule <captures/audit-screen/final-pc2fg/schedule.csv> [--out-prefix <path>] [--allow-incomplete]

FG on/off per run comes from the schedule's fg column (native config.xml FRAME_GENERATION: FSR=on, OFF=off) - authoritative; the mod's
"FG off" presenter telemetry is NOT used. A run is valid when its LAST results.csv row has valid==1 (earlier failed attempts are ignored).
Each (fg, arm) needs exactly 2 valid runs, else it is reported INCOMPLETE (exit 2 unless --allow-incomplete).
Per run: avg fps and 1% low (mean of slowest 1%) from <id>-run.json; 1% high = 1000/mean of fastest ceil(1%) frame times (same frame
source as run.json: v2 FrameTime, fallback v1 msBetweenPresents); display latency = mean of v2 DisplayLatency; CPU/GPU % from the 1 Hz
collector (run.json hw); process CPU cores from <id>-cpu.json summary. Temp, power and input latency are printed as 'unavailable' when absent."""
import argparse, csv, json, math, os, sys
from collections import defaultdict

ARMS = ["STOCKLIKE", "PURE", "FULL"]
FGS = ["FSR", "OFF"]
NA = "unavailable"


def num(x):
    try:
        v = float(x)
        return v if math.isfinite(v) else None
    except (TypeError, ValueError):
        return None


def read_col(path, col):
    with open(path, newline="", encoding="utf-8-sig") as f:
        return [num(row.get(col)) for row in csv.DictReader(f)]


def frametimes(run_dir, rid):
    for name, col in (("-presentmon-v2.csv", "FrameTime"), ("-presentmon.csv", "msBetweenPresents")):
        p = os.path.join(run_dir, rid + name)
        if os.path.isfile(p):
            v = [x for x in read_col(p, col) if x is not None and x > 0]
            if v:
                return v
    return []


def lat_mean(run_dir, rid):
    p = os.path.join(run_dir, rid + "-presentmon-v2.csv")
    if not os.path.isfile(p):
        return None
    v = [x for x in read_col(p, "DisplayLatency") if x is not None]
    return sum(v) / len(v) if v else None


def cpu_cores(run_dir, rid):
    p = os.path.join(run_dir, rid + "-cpu.json")
    if not os.path.isfile(p):
        return None
    last = None
    for line in open(p, encoding="utf-8-sig"):
        line = line.strip()
        if line.startswith("{"):
            try:
                o = json.loads(line)
            except ValueError:
                continue
            if o.get("kind") == "summary" and o.get("complete", True):
                last = o
    return num(last.get("cpu_cores")) if last else None


def hwval(j, key):
    d = (j.get("hw") or {}).get(key)
    return num(d.get("mean")) if isinstance(d, dict) else None


def metrics(run_dir, rid):
    jp = os.path.join(run_dir, rid + "-run.json")
    if not os.path.isfile(jp):
        return None
    j = json.load(open(jp, encoding="utf-8-sig"))
    fps = j.get("fps") or {}
    ft = frametimes(run_dir, rid)
    high = None
    if ft:
        n = max(1, math.ceil(len(ft) * 0.01))
        high = 1000.0 / (sum(sorted(ft)[:n]) / n)
    pm = j.get("presentmon") or {}
    return dict(avg=num(fps.get("avg_fps")), low1=num(fps.get("low1_meantail_fps")), high1=high,
                lat=lat_mean(run_dir, rid), cpu=hwval(j, "cpu_pct"), gpu=hwval(j, "gpu_pct"), cores=cpu_cores(run_dir, rid),
                cpu_temp=hwval(j, "cpu_temp"), gpu_temp=hwval(j, "gpu_temp"), cpu_w=hwval(j, "cpu_w"), gpu_w=hwval(j, "gpu_w"),
                input_lat=num((pm.get("v2_AllInputToPhotonLatency") or {}).get("median")))


def mean(xs):
    xs = [x for x in xs if x is not None]
    return sum(xs) / len(xs) if xs else None


def fmt(x, d=2):
    return NA if x is None else f"{x:.{d}f}"


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--run-dir", required=True)
    ap.add_argument("--results", required=True)
    ap.add_argument("--schedule", required=True)
    ap.add_argument("--out-prefix", default="")
    ap.add_argument("--allow-incomplete", action="store_true")
    a = ap.parse_args()

    sched = {r["run_id"]: r for r in csv.DictReader(open(a.schedule, newline="", encoding="utf-8-sig"))}
    last = {}
    for r in csv.DictReader(open(a.results, newline="", encoding="utf-8-sig")):
        last[r["run_id"]] = r   # last attempt wins
    groups = defaultdict(list)
    skipped = []
    for rid, r in last.items():
        s = sched.get(rid)
        if not s:
            continue
        if r.get("valid") != "1":
            skipped.append((rid, (r.get("note") or "invalid")[:80]))
            continue
        m = metrics(a.run_dir, rid)
        if not m or m["avg"] is None:
            skipped.append((rid, "no run.json metrics"))
            continue
        m["id"] = rid
        groups[(s["fg"], s["arm"])].append(m)

    rows, incomplete = [], []
    keys = ("avg", "low1", "high1", "lat", "cpu", "gpu", "cores", "cpu_temp", "gpu_temp", "cpu_w", "gpu_w", "input_lat")
    for fg in FGS:
        for arm in ARMS:
            g = groups.get((fg, arm), [])
            if len(g) != 2:
                incomplete.append(f"{fg}/{arm}: {len(g)} valid (need exactly 2)")
            if not g:
                continue
            row = dict(fg="ON" if fg == "FSR" else "OFF", arm=arm, n=len(g), ids="+".join(x["id"][-4:] for x in g),
                       spread=(max(x["avg"] for x in g) - min(x["avg"] for x in g)))
            row.update({k: mean([x[k] for x in g]) for k in keys})
            rows.append(row)

    hdr = ["fg", "arm", "n", "runs", "avg_fps", "avg_spread", "low1_meantail_fps", "high1_meantail_fps", "display_latency_ms", "cpu_pct", "gpu_pct",
           "proc_cpu_cores", "cpu_temp_c", "gpu_temp_c", "cpu_w", "gpu_w", "input_latency_ms"]
    table = [[r["fg"], r["arm"], r["n"], r["ids"], fmt(r["avg"]), fmt(r["spread"]), fmt(r["low1"]), fmt(r["high1"]), fmt(r["lat"]),
              fmt(r["cpu"], 1), fmt(r["gpu"], 1), fmt(r["cores"]), fmt(r["cpu_temp"], 1), fmt(r["gpu_temp"], 1), fmt(r["cpu_w"], 1),
              fmt(r["gpu_w"], 1), fmt(r["input_lat"])] for r in rows]
    md = ["| " + " | ".join(hdr) + " |", "|" + "---|" * len(hdr)] + ["| " + " | ".join(map(str, t)) + " |" for t in table]
    base = {r["fg"]: r for r in rows if r["arm"] == "STOCKLIKE"}
    md.append("")
    for r in rows:
        b = base.get(r["fg"])
        if b and r["arm"] != "STOCKLIKE" and b["avg"] and r["avg"]:
            md.append(f"- FG {r['fg']} {r['arm']} vs STOCKLIKE: avg fps {100 * (r['avg'] / b['avg'] - 1):+.1f}%"
                      + (f", 1% low {100 * (r['low1'] / b['low1'] - 1):+.1f}%" if b["low1"] and r["low1"] else ""))
    md.append("")
    md.append("Notes: FG on/off = native config.xml (schedule fg column); mod FG-off presenter telemetry ignored. CPU/GPU % are 1 Hz machine-wide "
              "samples; 'unavailable' = collector had no such column (temp/power/input latency). 1% low/high = mean-tail of slowest/fastest 1% frames.")
    if skipped:
        md.append("Excluded invalid: " + "; ".join(f"{i} ({n})" for i, n in skipped))
    if incomplete:
        md.append("INCOMPLETE: " + "; ".join(incomplete))
    out = "\n".join(md)
    print(out)
    if a.out_prefix:
        open(a.out_prefix + ".md", "w", encoding="utf-8").write(out + "\n")
        with open(a.out_prefix + ".csv", "w", newline="", encoding="utf-8") as f:
            w = csv.writer(f)
            w.writerow(hdr)
            w.writerows(table)
    return 0 if (not incomplete or a.allow_incomplete) else 2


if __name__ == "__main__":
    sys.exit(main())
