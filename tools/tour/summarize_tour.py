r"""summarize_tour.py - report for a Tour run (tools\tour\run_tour.ps1, notes\tour-mode.md). stdlib only.

    py -I tools\tour\summarize_tour.py <capture dir> [--symbols re\symbols\symbols.tsv] [--out tour_report.md]

Reads every *.log in <capture dir>\logs (the mod logs of each launch, in name order), optional presentmon.csv and progress.txt.
Writes tour_report.md + tour_points.csv into the capture dir and prints the report path. Sections:
  run overview (stages ok / skipped / stalled, crash points, save-block counters, guard denials, total time),
  per-stage table, worst points (p99) with what dominates there (GPU pass / PSO vs CPU function; GPU- vs CPU-bound),
  opportunities (GPU passes, PSO buckets and CPU functions ranked by ms x points hit; costs that vary a lot between areas),
  leak check (pass 1 vs pass 2 at the same points; first vs last point), warnings.
"""
import argparse
import bisect
import csv
import datetime as dt
import math
import pathlib
import re
import statistics
import sys

TS = re.compile(r"^\[(\d\d):(\d\d):(\d\d)\.(\d{3})\] (.*)$")
KV = re.compile(r"(\w+)=(\S+)")


def fnum(s):
    try:
        v = float(s)
        return v if math.isfinite(v) else None
    except (TypeError, ValueError):
        return None


def load_symbols(path):
    sym = {}
    if path and path.exists():
        for ln in path.read_text(encoding="utf-8", errors="replace").splitlines():
            p = ln.split("\t")
            if len(p) >= 3 and p[0].startswith("0x") and p[1] == "function":
                try:
                    sym[int(p[0], 16)] = p[2]
                except ValueError:
                    pass
    keys = sorted(sym)
    return sym, keys


def sym_name(sym, keys, rva):
    if rva in sym:
        return sym[rva]
    i = bisect.bisect_right(keys, rva) - 1
    if i >= 0 and rva - keys[i] < 0x400:
        return "%s+0x%X" % (sym[keys[i]], rva - keys[i])
    return "fn 0x%X" % rva


class Window:
    def __init__(self):
        self.gp_frame = None      # GP: sum of top-level markers ms
        self.gp = {}              # pass -> self ms/frame
        self.gp_incl = {}
        self.gp2_frame = None
        self.gp2 = {}             # PSO bucket -> ms/frame
        self.sp_main_samples = 0
        self.sp = {}              # (group, fn label) -> %


def parse_logs(files):
    pts, stages, events, warns, windows = [], {}, [], [], {}
    counters = {"sb_user": 0, "sb_backup": 0, "guard": 0}
    armed = done = False
    cur_gp = cur_sp = None
    first_t = last_t = None
    for run, f in enumerate(files, 1):
        day = 0
        prev = None
        for raw in f.read_text(encoding="utf-8", errors="replace").splitlines():
            m = TS.match(raw)
            if not m:
                continue
            h, mi, s, ms, text = m.groups()
            t = int(h) * 3600 + int(mi) * 60 + int(s) + int(ms) / 1000.0
            if prev is not None and t < prev - 3600:
                day += 1
            prev = t
            t += day * 86400
            if first_t is None:
                first_t = (run, t)
            last_t = (run, t)
            if text.startswith("Tour P "):
                d = dict(KV.findall(text.split(" label=")[0]))
                d["label"] = text.split(" label=", 1)[1] if " label=" in text else "-"
                d["run"] = run
                pts.append(d)
                cur_gp = cur_sp = None
            elif text.startswith("Tour: "):
                body = text[6:]
                if body.startswith("ARMED"):
                    armed = True
                if body.startswith("DONE"):
                    done = True
                    m2 = re.search(r"user (\d+), backup (\d+); guard denials (\d+)", body)
                    if m2:
                        counters.update(sb_user=int(m2.group(1)), sb_backup=int(m2.group(2)), guard=int(m2.group(3)))
                if body.startswith("hb "):
                    m2 = re.search(r"user=(\d+) backup=(\d+) guard=(\d+)", body)
                    if m2:
                        counters["sb_user"] = max(counters["sb_user"], int(m2.group(1)))
                        counters["sb_backup"] = max(counters["sb_backup"], int(m2.group(2)))
                        counters["guard"] = max(counters["guard"], int(m2.group(3)))
                    continue
                m2 = re.match(r"stage (\d+) arrived: request -> field ([\d.]+) s", body)
                if m2:
                    stages.setdefault(int(m2.group(1)), {})["load_s"] = float(m2.group(2))
                if not body.startswith("DONE") and any(k in body for k in ("STALL", "RESUME", "DISARMED", "SAVE GUARD", "interrupted", "skipped", "earlier crash", "exception")):
                    events.append((run, raw))
            elif text.startswith("GP: window "):
                cur_gp = windows.setdefault(text[len("GP: window "):].strip(), Window())
            elif text.startswith("SP window "):
                cur_sp = windows.setdefault(text[len("SP window "):].strip(), Window())
            elif text.startswith("GP: ") and cur_gp is not None:
                m2 = re.match(r"GP: frame ([\d.]+) ms GPU", text)
                if m2:
                    cur_gp.gp_frame = float(m2.group(1))
                m2 = re.match(r"GP:\s+\d+ (.+?)\s+incl\s+([\d.]+) ms\s+self\s+([\d.]+) ms", text)
                if m2:
                    cur_gp.gp[m2.group(1).strip()] = float(m2.group(3))
                    cur_gp.gp_incl[m2.group(1).strip()] = float(m2.group(2))
            elif text.startswith("GP2: ") and cur_gp is not None:
                m2 = re.match(r"GP2: frame ([\d.]+) ms GPU span", text)
                if m2:
                    cur_gp.gp2_frame = float(m2.group(1))
                m2 = re.match(r"GP2:\s+\d+ (.+?) \|\s+([\d.]+) ms \|.*\| \[(.*)\]\s*$", text)
                if m2:
                    key = "%s [%s]" % (m2.group(1).strip(), m2.group(3))
                    cur_gp.gp2[key] = cur_gp.gp2.get(key, 0.0) + float(m2.group(2))
            elif text.startswith("SP ") and cur_sp is not None:
                m2 = re.match(r"SP main \[App\]Game: (\d+) samples", text)
                if m2:
                    cur_sp.sp_main_samples = int(m2.group(1))
                m2 = re.match(r"SP (\S+)\s+([\d.]+) %\s+(.*)$", text)
                if m2 and m2.group(1) != "window":
                    cur_sp.sp[(m2.group(1), m2.group(3).strip())] = float(m2.group(2))
            elif re.search(r"\b(error|fail|warn|exception)", text, re.I) and not text.startswith(("SP ", "GP")):
                warns.append((run, raw))
    span = None
    if first_t and last_t:
        span = last_t[1] - first_t[1] if first_t[0] == last_t[0] else None
    return pts, stages, events, warns, windows, counters, armed, done, span


def load_progress(p):
    res = {"crash_pts": [], "crash_loads": [], "stall": [], "skipped": [], "finished": False}
    if not p.exists():
        return res
    for ln in p.read_text(encoding="utf-8", errors="replace").splitlines():
        f = ln.split()
        if not f:
            continue
        if f[0] == "C" and len(f) >= 4:
            res["crash_pts"].append(tuple(int(x) + 1 for x in f[1:4]))
        elif f[0] == "A" and len(f) >= 5 and f[4] != "ok":
            (res["stall"] if f[4] in ("stall", "crash") else res["skipped"]).append((int(f[1]) + 1, int(f[2]) + 1, f[3], f[4]))
        elif f[0] == "E":
            res["finished"] = True
    return res


def load_pm(path):
    """[(posix_s, frame_ms, gpu_busy, cpu_busy)] from a PresentMon v2 csv (CPUStartDateTime local time)."""
    rows = []
    if not path.exists():
        return rows
    with open(path, newline="", encoding="utf-8-sig", errors="replace") as f:
        for r in csv.DictReader(f):
            if r.get("Application", "").lower() not in ("nioh3.exe", ""):
                continue
            m = re.match(r"(\d+)-(\d+)-(\d+) (\d+):(\d+):(\d+)(?:\.(\d+))?", r.get("CPUStartDateTime") or "")
            if not m:
                continue
            y, mo, d, h, mi, se, fr = m.groups()
            ts = dt.datetime(int(y), int(mo), int(d), int(h), int(mi), int(se), int((fr or "0")[:6].ljust(6, "0"))).timestamp()
            rows.append((ts, fnum(r.get("FrameTime") or r.get("MsBetweenPresents")), fnum(r.get("GPUBusy")), fnum(r.get("CPUBusy"))))
    rows.sort()
    return rows


def pm_window(pm, t0, t1):
    if not pm:
        return None
    ts = [r[0] for r in pm]
    a, b = bisect.bisect_left(ts, t0), bisect.bisect_right(ts, t1)
    sel = pm[a:b]
    if len(sel) < 5:
        return None
    def mean(i):
        v = [r[i] for r in sel if r[i] is not None]
        return statistics.fmean(v) if v else None
    return {"n": len(sel), "ft": mean(1), "gpu_busy": mean(2), "cpu_busy": mean(3)}


def bound_of(p):
    """GPU-bound / CPU-bound / mixed from the GPU frame (GP or PresentMon GPUBusy) vs the frame time and main-thread busy."""
    ft = 1000.0 / p["fps"] if p.get("fps") else None
    gpu = p.get("pm_gpu") or p.get("gp_frame")
    main = p.get("main")
    if not ft:
        return "?"
    if gpu and gpu >= 0.9 * ft:
        return "GPU"
    if main is not None and main >= 85:
        return "CPU(main)"
    if gpu and gpu < 0.75 * ft:
        return "CPU/wait"
    return "mixed"


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("capture")
    ap.add_argument("--symbols", default=None)
    ap.add_argument("--out", default="tour_report.md")
    a = ap.parse_args()
    cap = pathlib.Path(a.capture)
    here = pathlib.Path(__file__).resolve()
    sympath = pathlib.Path(a.symbols) if a.symbols else here.parents[2] / "re" / "symbols" / "symbols.tsv"
    if not sympath.exists():
        sympath = pathlib.Path(r"C:\Users\Ahmad Hamza\nioh3-engine\re\symbols\symbols.tsv")
    sym, symkeys = load_symbols(sympath)
    logs = sorted((cap / "logs").glob("*.log")) if (cap / "logs").exists() else sorted(cap.glob("*.log"))
    if not logs:
        sys.exit("no logs in %s" % cap)
    pts, stage_info, events, warns, windows, counters, armed, done, span = parse_logs(logs)
    prog = load_progress(cap / "progress.txt")
    pm = load_pm(cap / "presentmon.csv")

    # ---- per point rows
    rows = []
    for d in pts:
        p = {
            "pass": int(d.get("pass", 0)), "stage": int(d.get("stage", "0/0").split("/")[0]), "nstages": int(d.get("stage", "0/0").split("/")[1]),
            "key": int(d.get("key", 0)), "pt": int(d.get("pt", "0/0").split("/")[0]), "src": d.get("src", "?"), "label": d.get("label", "-"),
            "pos": d.get("pos", ""), "fps": fnum(d.get("fps")), "p50": fnum(d.get("p50")), "p99": fnum(d.get("p99")), "max": fnum(d.get("max")),
            "hitch": int(d.get("hitch", 0)), "cpu": fnum(d.get("cpu")), "main": fnum(d.get("main")), "private": fnum(d.get("private")),
            "ws": fnum(d.get("ws")), "handles": fnum(d.get("handles")), "threads": fnum(d.get("threads")), "load": fnum(d.get("load")),
            "warpS": fnum(d.get("warpS")), "int": d.get("int") == "1", "frames": int(d.get("frames", 0)), "run": d["run"],
            "t0": (fnum(d.get("t0")) or 0) / 1000.0, "t1": (fnum(d.get("t1")) or 0) / 1000.0,
        }
        vr = d.get("vram", "-1/-1").split("/")
        p["vram"] = fnum(vr[0]) if fnum(vr[0]) is not None and fnum(vr[0]) >= 0 else None
        sc = d.get("syscommit", "").split("/")
        p["syscommit"] = fnum(sc[0]) if sc else None
        tag = "tour %d/%d/%d" % (p["pass"], p["stage"], p["pt"])
        w = windows.get(tag)
        p["gp_frame"] = w.gp_frame if w and w.gp_frame else (w.gp2_frame if w else None)
        p["win"] = w
        pw = pm_window(pm, p["t0"], p["t1"])
        p["pm_gpu"] = pw["gpu_busy"] if pw else None
        p["pm_cpu"] = pw["cpu_busy"] if pw else None
        p["pm_ft"] = pw["ft"] if pw else None
        p["bound"] = bound_of(p)
        rows.append(p)

    out = []
    w_ = out.append
    w_("# Tour report - %s\n" % cap.name)
    nst = max([r["nstages"] for r in rows] or [0])
    stages_ok = sorted({(r["pass"], r["stage"]) for r in rows})
    w_("## Run overview\n")
    w_("- launches (log files): %d; tour %s; %s" % (len(logs), "finished" if (done or prog["finished"]) else "NOT finished",
                                                      "armed" if armed else "never armed (Continue not pressed / never reached the field?)"))
    w_("- points measured: %d over %d stage visits (list: %d stages); stalls / crashed loads: %d; skipped (locked / missing / refused): %d" %
       (len(rows), len(stages_ok), nst, len(prog["stall"]), len(prog["skipped"])))
    w_("- crash points (resume skipped them): %s" % (", ".join("pass %d stage %d point %d" % c for c in prog["crash_pts"]) or "none"))
    w_("- save writes skipped by the tour: user %d, BACKUP.BIN %d; save-folder guard denials: %d (expected 0)" %
       (counters["sb_user"], counters["sb_backup"], counters["guard"]))
    if span:
        w_("- wall time of the last launch: %.1f min" % (span / 60.0))
    if rows:
        tot = sum((r["load"] or 0) + (r["warpS"] or 0) for r in rows)
        w_("- time in loads + warps: %.1f min; in dwells: %.1f min" % (tot / 60.0, sum(r["frames"] and (r["t1"] - r["t0"]) or 0 for r in rows) / 60.0))
    w_("")

    # ---- per stage
    w_("## Per stage\n")
    w_("| pass | stage | key | mission | load s | points | avg fps | min fps | worst p99 ms | max ms | hitches | bound | private MB max | VRAM MB max |")
    w_("|---|---|---|---|---|---|---|---|---|---|---|---|---|---|")
    by_stage = {}
    for r in rows:
        by_stage.setdefault((r["pass"], r["stage"]), []).append(r)
    for (ps, st), lst in sorted(by_stage.items()):
        fps = [r["fps"] for r in lst if r["fps"]]
        p99 = [r["p99"] for r in lst if r["p99"]]
        bounds = [r["bound"] for r in lst]
        w_("| %d | %d | %d | %s | %s | %d | %.1f | %.1f | %.2f | %.1f | %d | %s | %s | %s |" % (
            ps, st, lst[0]["key"], lst[0]["label"], "%.1f" % lst[0]["load"] if lst[0]["load"] else "-", len(lst),
            statistics.fmean(fps) if fps else 0, min(fps) if fps else 0, max(p99) if p99 else 0, max(r["max"] or 0 for r in lst),
            sum(r["hitch"] for r in lst), max(set(bounds), key=bounds.count), "%.0f" % max(r["private"] or 0 for r in lst),
            "%.0f" % max(r["vram"] or 0 for r in lst) if any(r["vram"] for r in lst) else "-"))
    w_("")

    # ---- worst points
    w_("## Worst points (frame-time p99)\n")
    w_("| p99 ms | fps | pass/stage/pt | mission | src | position | bound | GPU ms (GP / PresentMon) | main busy % | top GPU pass (self ms) | top CPU function (main %) |")
    w_("|---|---|---|---|---|---|---|---|---|---|---|")
    for r in sorted((r for r in rows if r["p99"]), key=lambda r: -r["p99"])[:12]:
        w = r["win"]
        gp_top = max(w.gp.items(), key=lambda kv: kv[1]) if w and w.gp else None
        sp_main = [(k[1], v) for k, v in (w.sp.items() if w else []) if k[0] == "main"]
        sp_top = max(sp_main, key=lambda kv: kv[1]) if sp_main else None
        def fn_label(s):
            m = re.search(r"fn 0x([0-9A-Fa-f]+)", s)
            return s.replace(m.group(0), sym_name(sym, symkeys, int(m.group(1), 16))) if m else s
        w_("| %.2f | %.1f | %d/%d/%d | %s | %s | %s | %s | %s / %s | %s | %s | %s |" % (
            r["p99"], r["fps"] or 0, r["pass"], r["stage"], r["pt"], r["label"], r["src"], r["pos"], r["bound"],
            "%.2f" % r["gp_frame"] if r["gp_frame"] else "-", "%.2f" % r["pm_gpu"] if r["pm_gpu"] else "-",
            "%.0f" % r["main"] if r["main"] is not None else "-",
            "%s (%.2f)" % gp_top if gp_top else "-", "%s (%.1f)" % (fn_label(sp_top[0]), sp_top[1]) if sp_top else "-"))
    w_("")

    # ---- opportunities
    w_("## Opportunities (ranked by ms x points hit)\n")
    n_win = sum(1 for r in rows if r["win"] and (r["win"].gp or r["win"].gp2 or r["win"].sp))
    if not n_win:
        w_("No GP / SP windows in the logs (run with GPUProf = 2 and SP = 1, the run_tour.ps1 default).\n")
    def rank_table(title, getter, unit, label_fn=lambda s: s, top=20):
        agg = {}
        for r in rows:
            w = r["win"]
            if not w:
                continue
            for k, v in getter(r, w).items():
                agg.setdefault(k, []).append((v, r))
        if not agg:
            return
        w_("### %s\n" % title)
        w_("| rank | %s | mean %s | max %s | points | score | spread (max/median) | worst at |" % ("name", unit, unit))
        w_("|---|---|---|---|---|---|---|---|")
        items = []
        for k, lst in agg.items():
            vals = [v for v, _ in lst]
            mean = statistics.fmean(vals)
            med = statistics.median(vals)
            mx, worst = max(lst, key=lambda x: x[0])
            items.append((mean * len(lst), k, mean, mx, len(lst), (mx / med) if med > 0 else float("inf"), worst))
        items.sort(key=lambda x: -x[0])
        for i, (score, k, mean, mx, n, spread, worst) in enumerate(items[:top], 1):
            w_("| %d | %s | %.3f | %.3f | %d/%d | %.2f | %.1f%s | %d/%d/%d %s |" % (
                i, label_fn(k), mean, mx, n, len(rows), score, spread, " VARIES" if spread >= 2.0 and n >= 3 else "",
                worst["pass"], worst["stage"], worst["pt"], worst["label"]))
        w_("")
    rank_table("GPU passes (GP self ms/frame)", lambda r, w: w.gp, "ms")
    rank_table("GPU PSO buckets (GP2 ms/frame; shader hashes = psoswap file names)", lambda r, w: w.gp2, "ms")

    def cpu_ms(r, w):  # main-thread share x frame time = ms/frame spent in the function on the game thread
        ft = 1000.0 / r["fps"] if r["fps"] else None
        res = {}
        for (g, lab), pct in w.sp.items():
            if g == "main" and ft:
                res[lab] = pct / 100.0 * ft
        return res

    def fn_lab(s):
        m = re.search(r"fn 0x([0-9A-Fa-f]+)", s)
        return s.replace(m.group(0), "%s (0x%s)" % (sym_name(sym, symkeys, int(m.group(1), 16)), m.group(1))) if m else s

    rank_table("CPU functions on the game thread (SP share x frame time = ms/frame)", cpu_ms, "ms", fn_lab)

    def worker_pct(r, w):
        return {"%s: %s" % (g, lab): pct for (g, lab), pct in w.sp.items() if g != "main"}

    rank_table("CPU functions on workers / dispatcher (SP % of that thread group's samples)", worker_pct, "%", fn_lab, top=12)
    bc = {}
    for r in rows:
        bc[r["bound"]] = bc.get(r["bound"], 0) + 1
    w_("Bound classification over all points: %s (GPU = GPU frame >= 90 %% of frame time; CPU(main) = game thread busy >= 85 %%).\n" %
       ", ".join("%s %d" % kv for kv in sorted(bc.items(), key=lambda kv: -kv[1])))

    # ---- leak check
    passes = sorted({r["pass"] for r in rows})
    w_("## Leak check\n")
    if rows:
        f, l = rows[0], rows[-1]
        def dl(k):
            return ("%+.0f" % (l[k] - f[k])) if l.get(k) is not None and f.get(k) is not None else "-"
        w_("- first -> last point: private %s MB, working set %s MB, handles %s, threads %s, VRAM %s MB, system commit %s MB" %
           (dl("private"), dl("ws"), dl("handles"), dl("threads"), dl("vram"), dl("syscommit")))
    if len(passes) >= 2:
        p1 = {(r["stage"], r["pt"]): r for r in rows if r["pass"] == passes[0]}
        p2 = {(r["stage"], r["pt"]): r for r in rows if r["pass"] == passes[-1]}
        common = sorted(set(p1) & set(p2))
        if common:
            def avgd(k):
                v = [p2[c][k] - p1[c][k] for c in common if p1[c].get(k) is not None and p2[c].get(k) is not None]
                return ("%+.1f (max %+.1f)" % (statistics.fmean(v), max(v))) if v else "-"
            w_("- pass %d vs pass %d at the same %d points: private %s MB, handles %s, threads %s, VRAM %s MB, p99 %s ms" %
               (passes[-1], passes[0], len(common), avgd("private"), avgd("handles"), avgd("threads"), avgd("vram"), avgd("p99")))
            w_("  (a steady rise of private / VRAM / handles at the same spots = a leak; the same numbers = none)")
    else:
        w_("- one pass only (TourPasses = 2 compares the same points twice)")
    w_("")

    # ---- events and warnings
    w_("## Events (stalls, skips, interruptions, resumes, guard)\n")
    for run, ln in events[:80]:
        w_("- run %d: `%s`" % (run, ln))
    if not events:
        w_("- none")
    w_("\n## Warnings / errors in the mod log\n")
    for run, ln in warns[:40]:
        w_("- run %d: `%s`" % (run, ln))
    if not warns:
        w_("- none")
    rep = cap / a.out
    rep.write_text("\n".join(out) + "\n", encoding="utf-8")
    cols = ["pass", "stage", "pt", "key", "label", "src", "pos", "fps", "p50", "p99", "max", "hitch", "cpu", "main", "private", "ws",
            "handles", "threads", "vram", "syscommit", "load", "warpS", "gp_frame", "pm_gpu", "pm_cpu", "bound", "int", "run"]
    with open(cap / "tour_points.csv", "w", newline="", encoding="utf-8") as fcsv:
        wr = csv.writer(fcsv)
        wr.writerow(cols)
        for r in rows:
            wr.writerow([r.get(c) for c in cols])
    print(rep)


if __name__ == "__main__":
    main()
