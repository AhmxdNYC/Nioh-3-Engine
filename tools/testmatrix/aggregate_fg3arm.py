"""Aggregate the pc2 12-run R7B35 batch (FG on/off x STOCKLIKE/PURE/FULL x 2 reps). Read-only on inputs.

  py -I tools/testmatrix/aggregate_fg3arm.py --run-dir <run-pc2-A> [<run-pc2-B> ...] --results <captures/fight26758-control/results.csv> [more.csv]
        --schedule <captures/audit-screen/final-pc2fg/schedule.csv> [--out-prefix <path>] [--allow-incomplete]

FG on/off per run comes from the schedule's fg column (native config.xml FRAME_GENERATION: FSR=on, OFF=off) - authoritative; the mod's
"FG off" presenter telemetry is NOT used. A run is valid when its LAST results.csv row has valid==1 (earlier failed attempts are ignored).
Each (fg, arm) needs exactly 2 valid runs, else it is reported INCOMPLETE (exit 2 unless --allow-incomplete).
--run-dir takes several dirs (resumed batch): a run's evidence comes from the NEWEST dir that has <id>-run.json (later --results files override
earlier rows). Capture-window OSD speed: mod log '<id>-Nioh3EngineFix.log' 'OSD: stats ... speed N.NNx' lines whose local time-of-day lies
inside the cpu.json summary start_utc..end_utc (converted to local); min/mean/count are reported and runs with unequal simulation speed are
FLAGGED (raw fps is never normalised).
Per run: avg fps and 1% low (mean of slowest 1%) from <id>-run.json; 1% high = 1000/mean of fastest ceil(1%) frame times (same frame
source as run.json: v2 FrameTime, fallback v1 msBetweenPresents); display latency = mean of v2 DisplayLatency; CPU/GPU % from the 1 Hz
collector (run.json hw); process CPU cores from <id>-cpu.json summary. Temp, power and input latency are printed as 'unavailable' when absent."""
import argparse, csv, json, math, os, re, sys
from datetime import datetime
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


SPEED_RE = re.compile(r"^\[(\d\d):(\d\d):(\d\d)\.(\d+)\] OSD: stats:.*?\bspeed ([0-9.]+)x")


def osd_speeds(run_dir, rid):
    """speed values (list) of OSD stats lines inside the capture window, or None when the window/log is unavailable."""
    cp, lp = os.path.join(run_dir, rid + "-cpu.json"), os.path.join(run_dir, rid + "-Nioh3EngineFix.log")
    if not (os.path.isfile(cp) and os.path.isfile(lp)):
        return None
    win = None
    for line in open(cp, encoding="utf-8-sig"):
        if line.startswith("{"):
            try:
                o = json.loads(line)
            except ValueError:
                continue
            if o.get("kind") == "summary" and o.get("start_utc") and o.get("end_utc"):
                win = o
    if not win:
        return None
    def tod(u):
        d = datetime.fromisoformat(u).astimezone()   # UTC -> local, same clock as the mod log
        return d.hour * 3600 + d.minute * 60 + d.second + d.microsecond / 1e6
    t0, t1 = tod(win["start_utc"]), tod(win["end_utc"])
    out = []
    for line in open(lp, encoding="utf-8", errors="replace"):
        m = SPEED_RE.match(line)
        if not m:
            continue
        t = int(m.group(1)) * 3600 + int(m.group(2)) * 60 + int(m.group(3)) + float("0." + m.group(4))
        if (t0 <= t <= t1) if t0 <= t1 else (t >= t0 or t <= t1):   # window may cross midnight
            out.append(float(m.group(5)))
    return out


def hwval(j, key):
    d = (j.get("hw") or {}).get(key)
    return num(d.get("mean")) if isinstance(d, dict) else None


def find_dir(run_dirs, rid):
    """newest run dir (last in the --run-dir order, i.e. sorted by name) that holds <id>-run.json"""
    for d in sorted(run_dirs, reverse=True):
        if os.path.isfile(os.path.join(d, rid + "-run.json")):
            return d
    return None


def metrics(run_dirs, rid):
    run_dir = find_dir(run_dirs, rid)
    if not run_dir:
        return None
    jp = os.path.join(run_dir, rid + "-run.json")
    j = json.load(open(jp, encoding="utf-8-sig"))
    fps = j.get("fps") or {}
    ft = frametimes(run_dir, rid)
    high = None
    if ft:
        n = max(1, math.ceil(len(ft) * 0.01))
        high = 1000.0 / (sum(sorted(ft)[:n]) / n)
    pm = j.get("presentmon") or {}
    sp = osd_speeds(run_dir, rid)
    return dict(speeds=sp, avg=num(fps.get("avg_fps")), low1=num(fps.get("low1_meantail_fps")), high1=high,
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
    ap.add_argument("--run-dir", required=True, nargs="+")
    ap.add_argument("--results", required=True, nargs="+")
    ap.add_argument("--schedule", required=True)
    ap.add_argument("--out-prefix", default="")
    ap.add_argument("--allow-incomplete", action="store_true")
    a = ap.parse_args()

    sched = {r["run_id"]: r for r in csv.DictReader(open(a.schedule, newline="", encoding="utf-8-sig"))}
    last = {}
    for rp in a.results:
        for r in csv.DictReader(open(rp, newline="", encoding="utf-8-sig")):
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
            sp = [v for x in g for v in (x["speeds"] or [])]
            row.update(sp_min=min(sp) if sp else None, sp_mean=mean(sp), sp_n=len(sp),
                       sp_runs=" / ".join(",".join(f"{v:g}" for v in (x["speeds"] or [])) or "-" for x in g))
            rows.append(row)

    hdr = ["fg", "arm", "n", "runs", "avg_fps", "avg_spread", "low1_meantail_fps", "high1_meantail_fps", "display_latency_ms", "cpu_pct", "gpu_pct",
           "proc_cpu_cores", "cpu_temp_c", "gpu_temp_c", "cpu_w", "gpu_w", "input_latency_ms", "osd_speed_min", "osd_speed_mean", "osd_speed_n", "osd_speed_per_run"]
    table = [[r["fg"], r["arm"], r["n"], r["ids"], fmt(r["avg"]), fmt(r["spread"]), fmt(r["low1"]), fmt(r["high1"]), fmt(r["lat"]),
              fmt(r["cpu"], 1), fmt(r["gpu"], 1), fmt(r["cores"]), fmt(r["cpu_temp"], 1), fmt(r["gpu_temp"], 1), fmt(r["cpu_w"], 1),
              fmt(r["gpu_w"], 1), fmt(r["input_lat"]),
              fmt(r["sp_min"]), fmt(r["sp_mean"]), r["sp_n"], r["sp_runs"]] for r in rows]
    md = ["| " + " | ".join(hdr) + " |", "|" + "---|" * len(hdr)] + ["| " + " | ".join(map(str, t)) + " |" for t in table]
    base = {r["fg"]: r for r in rows if r["arm"] == "STOCKLIKE"}
    md.append("")
    for r in rows:
        b = base.get(r["fg"])
        if b and r["arm"] != "STOCKLIKE" and b["avg"] and r["avg"]:
            md.append(f"- FG {r['fg']} {r['arm']} vs STOCKLIKE: avg fps {100 * (r['avg'] / b['avg'] - 1):+.1f}%"
                      + (f", 1% low {100 * (r['low1'] / b['low1'] - 1):+.1f}%" if b["low1"] and r["low1"] else ""))
    md.append("")
    for fg in FGS:   # unequal simulation speed => raw fps arms are not like-for-like (flag only, no normalisation)
        rs = [r for r in rows if r["fg"] == ("ON" if fg == "FSR" else "OFF") and r["sp_mean"] is not None]
        if len(rs) > 1 and max(r["sp_mean"] for r in rs) - min(r["sp_mean"] for r in rs) > 0.03:
            md.append(f"WARNING: FG {rs[0]['fg']} arms ran at UNEQUAL simulation speed ("
                      + ", ".join(f"{r['arm']} {r['sp_mean']:.2f}x" for r in rs) + "): raw FPS is shown as measured and is not normalised; "
                      "fps differences are partly game-speed differences.")
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
