"""Helper for the test matrix (run via `py -I tm.py ...`).
  stats  <presentmon.csv>                     -> one JSON line: fps, 1%low, p99 ms, gpu/cpu busy ms, hitch count/worst
  logtail <log> <offset> <out.txt>            -> copy the log lines after byte offset that carry stats/ GP / SP / lm / mem
  report <results_dir> <device> [--noise-floor 0.5]  -> summary.csv + verdict.md in results_dir
  best   <results_dir> <device> <matrix.json> [low] -> ini lines (section|key|value) of all KEEP items, for the BEST combo
 Final phase (results\\final_<device>_*.json written by final_pc.ps1; see README.md):
  sensors <log.csv> [--start ISO --end ISO]   -> JSON of HWiNFO / RTSS / Afterburner-style CSV means+max (CPU/GPU %, temps, W, MHz)
  ingest  <results_dir> <device> [--hwinfo f.csv] [--rtss f.csv] -> attach sensor stats to every final_<device>_* window by its t0/t1
  latency <results_dir> <device>              -> latency_<device>.md : STOCK vs OPTIMIZED vs OPTIMIZED+FG block
  table   <results_dir> <device> [out.md]     -> results_table_<device>.md : README tables (preset x config, sweep bang-for-buck)
  paired  <results_dir> <device>             -> read-only pinned launch-pair/block JSON; no significance verdict
"""
import csv, json, math, os, re, statistics, sys
_BENCH = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "bench"))
if _BENCH not in sys.path:
    sys.path.insert(0, _BENCH)
import arm_budget


def pct(xs, p):
    """Discrete nearest-rank percentile; low1 is 1000/p99, not a tail mean."""
    xs = sorted(xs)
    return xs[max(0, min(len(xs) - 1, math.ceil(len(xs) * p) - 1))] if xs else float("nan")


def col(row, *names):
    for n in names:
        v = row.get(n)
        if v not in (None, "", "NA"):
            try:
                x = float(v)
                if math.isfinite(x):
                    return x
            except ValueError:
                pass
    return None


def stats(path):
    """One Nioh3 process/swapchain; submission and display metrics stay separate.

    The first recorded row is excluded by the benchmark's boundary/startup
    policy from every metric. Later positive outliers stay. No mod log is read.
    Missing FrameType does not establish that frame generation was disabled.
    """
    with open(path, newline="", encoding="utf-8-sig") as f:
        reader = csv.DictReader(f)
        fields = reader.fieldnames or []
        raw = list(reader)
    identity = ("Application", "ProcessID", "SwapChainAddress")
    if not all(k in fields for k in identity):
        return {"ok": False, "reason": "missing process/swapchain identity"}
    rows = [r for r in raw if (r.get("Application") or "").casefold() == "nioh3.exe"]
    if any(None in r or any(v is None for v in r.values()) for r in rows):
        return {"ok": False, "reason": "malformed target CSV row"}
    streams = {(r["ProcessID"], r["SwapChainAddress"]) for r in rows}
    if len(streams) != 1 or any(not v for stream in streams for v in stream):
        return {"ok": False, "reason": "expected one target process/swapchain", "streams": sorted(streams)}
    rows = rows[1:]
    app = [r for r in rows if r.get("FrameType", "Application") in ("Application", "NotSet", "")]
    frame_col = next((k for k in ("FrameTime", "MsBetweenPresents") if k in fields), None)
    if frame_col is None:
        return {"ok": False, "reason": "missing submission frame interval"}
    xs = [v for r in app if (v := col(r, frame_col)) is not None and v > 0]
    if len(xs) != len(app) or not xs:
        return {"ok": False, "reason": "invalid application frame interval", "valid_frames": len(xs), "invalid_frames": len(app)-len(xs)}
    display_col = next((k for k in ("DisplayedTime", "MsBetweenDisplayChange") if k in fields), None)
    disp = [v for r in rows if display_col and (v := col(r, display_col)) is not None and v > 0]
    def busy(*names):
        return [v for r in app if (v := col(r, *names)) is not None and v >= 0]
    gpu = busy("MsGPUBusy", "GPUBusy")
    gpu_time = busy("MsGPUTime", "GPUTime")
    cpu = busy("MsCPUBusy", "CPUBusy")
    avg = statistics.mean(xs)
    p99 = pct(xs, .99)
    hit = arm_budget.hitch(xs)
    lat = {}        # PresentMon 2.x latency columns (need --v2_metrics + input during the window); NA rows are skipped
    for key, names in (("lat_aip", ("MsAllInputToPhotonLatency", "AllInputToPhotonLatency")),
                       ("lat_click", ("MsClickToPhotonLatency", "ClickToPhotonLatency")),
                       ("lat_pcl", ("MsPCLatency", "PCLatency")),
                       ("lat_disp", ("MsDisplayLatency", "DisplayLatency"))):
        v = [x for r in rows if (x := col(r, *names)) is not None and x > 0]
        if v:
            lat[key] = round(statistics.median(v), 2)
            lat[key + "_p95"] = round(pct(v, .95), 2)
            lat[key + "_n"] = len(v)
    ftypes = sorted({r.get("FrameType") for r in rows if r.get("FrameType")})
    fps = round(1000 / avg, 2)
    return {"ok": True, **lat, "app_fps": fps, "frame_types": ftypes, "frames": len(xs), "fps": fps,
            "low1": round(1000 / p99, 2), "p99": round(p99, 2), "percentile_method": "nearest-rank",
            "capture_duration_s": sum(xs)/1000,
            "fps_metric": frame_col, "low1_definition": "1000/p99_ms",
            "display_fps": round(1000 / statistics.mean(disp), 2) if disp else None,
            "display_p99": round(pct(disp, .99), 2) if disp else None,
            "display_frames": len(disp), "display_metric": display_col,
            "gpu_ms": round(statistics.mean(gpu), 2) if gpu else None,
            "gpu_time_ms": round(statistics.mean(gpu_time), 2) if gpu_time else None,
            "cpu_ms": round(statistics.mean(cpu), 2) if cpu else None,
            "hitch_count": hit["count"], "hitch_worst_ms": hit["worst"],
            "boundary_rows_excluded": 1, "foreign_rows_excluded": len(raw)-len(rows)-1,
            "stream": dict(zip(identity, ("Nioh3.exe", *next(iter(streams))))),
            "frame_type_tracking": "FrameType" in fields, "fg_off_verified": False}


KEEP_RE = re.compile(r"(stats[ /]|^GP2?:|^SP |stats lm|stats mem|stats latency|stats wakes|stats spr|stats tiles|ASYNC)")


def logtail(log, off, out):
    try:
        with open(log, "rb") as f:
            f.seek(int(off))
            txt = f.read().decode("utf-8", "replace").splitlines()
    except OSError:
        txt = []
    keep = [l for l in txt if KEEP_RE.search(l)]
    open(out, "w", encoding="utf-8").write("\n".join(keep[-400:]) + "\n")
    print(len(keep))


def med(xs):
    xs = [x for x in xs if x is not None]
    return statistics.median(xs) if xs else None


def load_results(d, device):
    res = {}
    for fn in sorted(os.listdir(d)):
        if fn.endswith(".json") and fn.startswith(device + "_"):
            j = json.load(open(os.path.join(d, fn), encoding="utf-8-sig"))
            res[j["id"]] = j
    return res


def summarize(j):
    st = [w for w in j.get("windows", []) if w.get("kind") == "stand" and w.get("ok")]
    mv = [w for w in j.get("windows", []) if w.get("kind") == "move" and w.get("ok")]
    cb = [w for w in j.get("windows", []) if w.get("kind") == "combat" and w.get("ok")]
    return {"fps": med([w["fps"] for w in st]), "low1": med([w["low1"] for w in st]), "p99": med([w["p99"] for w in st]),
            "gpu_ms": med([w["gpu_ms"] for w in st]), "cpu_ms": med([w["cpu_ms"] for w in st]),
            "move_fps": med([w["fps"] for w in mv]), "combat_fps": med([w["fps"] for w in cb]),
            "load_s": j.get("load_s"), "valid": j.get("valid", True) and bool(st)}


def report(d, device, floor):
    res = load_results(d, device)
    bases = [summarize(j) for k, j in res.items() if k == "BASE" or k.startswith("BASE#")]
    bases = [b for b in bases if b["valid"]]
    if not bases:
        print("no valid BASE runs"); return 1
    bfps = med([b["fps"] for b in bases]); bp99 = med([b["p99"] for b in bases])
    spread = (max(b["fps"] for b in bases) - min(b["fps"] for b in bases)) / bfps * 100 if len(bases) > 1 else 0
    noise = max(floor, spread)          # percent; verdict needs > 2x noise
    lines = []; rows = []
    for k, j in sorted(res.items(), key=lambda kv: (kv[1].get("grp", 99), kv[0])):
        if k.startswith("BASE"):
            continue
        s = summarize(j)
        if not s["valid"]:
            v, dp, dp99, dgpu = "INVALID", None, None, None
        else:
            dp = (s["fps"] / bfps - 1) * 100
            dp99 = (s["p99"] / bp99 - 1) * 100
            dgpu = (s["gpu_ms"] - med([b["gpu_ms"] for b in bases])) if s["gpu_ms"] and med([b["gpu_ms"] for b in bases]) else None
            typ = j.get("type")
            if typ == "measure": v = "MEASURE"
            elif dp < -2 * noise or dp99 > 4 * noise: v = "DROP"
            elif dp > 2 * noise: v = "KEEP"
            elif j.get("metric") == "latency": v = "REVIEW(latency)"
            else: v = "NEUTRAL"
            if v in ("KEEP", "NEUTRAL") and j.get("shot"): v += "+IMGCHECK"
            if v == "NEUTRAL" and j.get("risk") == "low" and typ == "asi" and device == "ally": v = "NEUTRAL(cpu-only: keep for power if W/MHz improved)"
        rows.append([k, j.get("type"), j.get("risk"), v, s["fps"], s["low1"], s["p99"], s["gpu_ms"], s["cpu_ms"], s["move_fps"],
                     s["combat_fps"], s["load_s"], None if dp is None else round(dp, 2), None if dp99 is None else round(dp99, 2)])
    hdr = ["id", "type", "risk", "verdict", "fps", "low1", "p99_ms", "gpu_ms", "cpu_ms", "move_fps", "combat_fps", "load_s", "d_fps_pct", "d_p99_pct"]
    with open(os.path.join(d, f"summary_{device}.csv"), "w", newline="") as f:
        w = csv.writer(f); w.writerow(hdr); w.writerows(rows)
    md = [f"# Verdict ({device})", "", f"BASE: {len(bases)} run(s), median {bfps:.1f} fps, p99 {bp99:.1f} ms. Noise = max({floor}%, BASE spread {spread:.2f}%) = {noise:.2f}%. KEEP needs > +{2*noise:.2f}% fps; DROP needs < -{2*noise:.2f}% fps or p99 worse by > {4*noise:.2f}%.", "",
          "| " + " | ".join(hdr) + " |", "|" + "---|" * len(hdr)]
    md += ["| " + " | ".join("" if c is None else str(c) for c in r) + " |" for r in rows]
    open(os.path.join(d, f"verdict_{device}.md"), "w", encoding="utf-8").write("\n".join(md) + "\n")
    print("\n".join(md[:6])); print(f"{len(rows)} rows -> summary_{device}.csv, verdict_{device}.md")
    return 0


def best(d, device, mfile, low):
    m = json.load(open(mfile, encoding="utf-8-sig")); items = {i["id"]: i for i in m["items"]}
    rows = list(csv.DictReader(open(os.path.join(d, f"summary_{device}.csv"))))
    out = []
    for r in rows:
        if not r["verdict"].startswith("KEEP"):
            continue
        it = items.get(r["id"])
        if not it or it["type"] not in ("asi",) or device not in it["devices"]:
            continue
        if low and it["risk"] != "low":
            continue
        for l in it.get("needs", []) + it.get("ini", []):
            out.append("|".join(l))
    seen = set(); [print(x) for x in out if not (x in seen or seen.add(x))]
    # shader/swap KEEPs are printed as SHADER|name / SWAP|digest
    for r in rows:
        it = items.get(r["id"])
        if it and r["verdict"].startswith("KEEP") and device in it["devices"] and (not low or it["risk"] == "low"):
            for s in it.get("shaders", []): print("SHADER|" + s)
            for s in it.get("swap", []): print("SWAP|" + s)


# ---------------------------------------------------------------- final phase: sensors, latency block, README tables
import datetime as _dt

SENS_PAT = [  # metric, include regex, exclude regex (matched case-insensitively against the column header, first match wins)
    ("fps",      r"^(framerate|fps)$|framerate", r"1%|0\.1%|low"),
    ("cpu_pct",  r"^(total cpu usage|cpu usage|cpu total|cpu load|cpu utility)", r"temp|clock|power"),
    ("cpu_temp", r"cpu.*(tctl|tdie|package|die|temperature)|^cpu temp|cpu.*\[temperature\]", r"power|clock|usage|vid|limit|margin"),
    ("cpu_w",    r"cpu.*package power|cpu.*(socket|ppt) ?power|^cpu power|cpu.*\[power\]", r"limit|%|gpu"),
    ("cpu_mhz",  r"core clocks? \(avg\)|average effective clock|cpu clock|cpu frequency|^core clock avg", r"gpu|ratio|limit"),
    ("gpu_pct",  r"gpu.*(core load|utilization|usage|load)|^gpu usage", r"memory|mem |video|d3d|encode|decode|power"),
    ("gpu_hot",  r"gpu.*(hot ?spot|junction)", r"memory"),
    ("gpu_temp", r"^gpu.*temp|gpu.*\[temperature\]", r"hot ?spot|junction|memory|limit"),
    ("gpu_w",    r"gpu.*(asic power|board power|total board|chip power)|^gpu power|^power$|gpu.*\[power\]", r"limit|%|effective"),
    ("gpu_mhz",  r"^gpu (core )?clock|gpu core clock|^core clock$", r"memory|effective|cpu"),
]
TIME_FMTS = ("%d.%m.%Y %H:%M:%S.%f", "%d.%m.%Y %H:%M:%S", "%Y-%m-%d %H:%M:%S.%f", "%Y-%m-%d %H:%M:%S", "%d-%m-%Y %H:%M:%S", "%m/%d/%Y %H:%M:%S",
             "%Y-%m-%dT%H:%M:%S", "%d/%m/%Y %H:%M:%S")


def _ptime(s):
    s = (s or "").strip()
    for f in TIME_FMTS:
        try:
            return _dt.datetime.strptime(s, f)
        except ValueError:
            pass
    try:
        return _dt.datetime.fromisoformat(s)
    except ValueError:
        return None


def _num(v):
    try:
        return float(str(v).strip().replace(",", "."))
    except ValueError:
        return None


def sensors(path, start=None, end=None):
    """Mean/max per metric from a HWiNFO CSV (Date,Time,<sensor [unit]>...), an RTSS benchmark CSV, or any CSV with a
    Time/Timestamp column (Afterburner via 'export'/HWiNFO shared memory). No time column -> all rows are used."""
    raw = open(path, "rb").read().decode("utf-8-sig", "replace")
    rd = csv.reader(raw.splitlines())
    hdr = next(rd)
    names = [h.strip() for h in hdr]
    low = [n.lower() for n in names]
    di = next((i for i, n in enumerate(low) if n == "date"), None)
    ti = next((i for i, n in enumerate(low) if n in ("time", "timestamp", "datetime", "date/time")), None)
    pick = {}
    for m, inc, exc in SENS_PAT:
        for i, n in enumerate(low):
            if i in (di, ti):
                continue
            if re.search(inc, n) and not (exc and re.search(exc, n)):
                pick[m] = i
                break
    t0 = _dt.datetime.fromisoformat(start) if start else None
    t1 = _dt.datetime.fromisoformat(end) if end else None
    acc = {m: [] for m in pick}
    n = 0
    for row in rd:
        if len(row) < len(names) - 1:
            continue
        if ti is not None:
            t = _ptime((row[di] + " " if di is not None else "") + row[ti])
            if t is None:
                continue
            if (t0 and t < t0) or (t1 and t > t1):
                continue
        n += 1
        for m, i in pick.items():
            v = _num(row[i]) if i < len(row) else None
            if v is not None:
                acc[m].append(v)
    out = {"rows": n, "columns": {m: names[i] for m, i in pick.items()}}
    for m, xs in acc.items():
        if xs:
            out[m] = round(statistics.mean(xs), 2)
            out[m + "_max"] = round(max(xs), 2)
    if n == 0:
        out["note"] = "no rows in the window (clock mismatch? HWiNFO/RTSS and this PC must share the same local time)"
    return out


def load_final(d, device):
    res = []
    pre = f"final_{device}_"
    for fn in sorted(os.listdir(d)) if os.path.isdir(d) else []:
        if fn.startswith(pre) and fn.endswith(".json"):
            with open(os.path.join(d, fn), encoding="utf-8-sig") as f:
                j = json.load(f)
            j["_file"] = fn
            res.append(j)
    return res


def ingest(d, device, hw, rt):
    n = 0
    for j in load_final(d, device):
        ch = False
        for w in j.get("windows", []):
            if not (w.get("t0") and w.get("t1")):
                continue
            sens = {}
            for tag, f in (("hwinfo", hw), ("rtss", rt)):
                if f:
                    sens[tag] = sensors(f, w["t0"], w["t1"])
            if sens:
                w["sens"] = sens
                ch = True
                n += 1
        if ch:
            json.dump({k: v for k, v in j.items() if k != "_file"}, open(os.path.join(d, j["_file"]), "w", encoding="utf-8"), indent=1)
    print(f"sensor stats attached to {n} windows")


def _sv(w, m, key=None):
    """one sensor value for a window: HWiNFO preferred, RTSS fallback"""
    s = w.get("sens") or {}
    for tag in ("hwinfo", "rtss"):
        v = (s.get(tag) or {}).get(key or m)
        if v is not None:
            return v
    return None


def cell_key(j):
    sw = j.get("sweep")
    return (j.get("cfg"), j.get("preset"), f"{sw['key']}={sw['value']}" if sw else "")


def run_summary(j):
    """one run -> metrics; STAND = median of stand windows, MOVE/COMBAT single, latency from latency windows"""
    W = [w for w in j.get("windows", []) if w.get("ok")]
    st = [w for w in W if w["kind"] == "stand"]
    mv = [w for w in W if w["kind"] == "move"]
    cb = [w for w in W if w["kind"] == "combat"]
    lt = [w for w in W if w["kind"] == "latency" or w.get("lat_aip") is not None or w.get("lat_click") is not None]
    gm = lambda ws, k: med([w.get(k) for w in ws])
    r = {"fps": gm(st, "fps"), "low1": gm(st, "low1"), "p99": gm(st, "p99"), "gpu_ms": gm(st, "gpu_ms"), "cpu_ms": gm(st, "cpu_ms"),
         "app_fps": gm(st, "app_fps"), "display_fps": gm(st, "display_fps"), "move_fps": gm(mv, "fps"), "combat_fps": gm(cb, "fps"),
         "lat_aip": gm(lt, "lat_aip"), "lat_click": gm(lt, "lat_click"), "lat_pcl": gm(lt, "lat_pcl"),
         "lat_n": sum(w.get("lat_aip_n") or w.get("lat_click_n") or 0 for w in lt),
         "load1_s": j.get("load1_s"), "load2_s": j.get("load2_s"), "hitches": j.get("hitches"), "valid": bool(j.get("valid", True)) and bool(st)}
    hc = [w.get("hitch_count") for w in st if isinstance(w.get("hitch_count"), (int, float))]
    hw = [w.get("hitch_worst_ms") for w in st if isinstance(w.get("hitch_worst_ms"), (int, float))]
    r["hitch_count"] = int(sum(hc)) if hc else None
    r["hitch_worst_ms"] = max(hw) if hw else None
    for m in ("cpu_pct", "gpu_pct", "cpu_w", "gpu_w", "cpu_mhz", "gpu_mhz"):
        r[m] = med([_sv(w, m) for w in st + mv + cb])
    for m in ("cpu_temp", "gpu_temp", "gpu_hot"):
        xs = [_sv(w, m, m + "_max") for w in st + mv + cb]
        xs = [x for x in xs if x is not None]
        r[m] = max(xs) if xs else None
    return r


def cells(d, device):
    g = {}
    for j in load_final(d, device):
        rs = run_summary(j)
        if rs["valid"]:
            g.setdefault(cell_key(j), []).append(rs)
    out = {}
    for k, runs in g.items():
        c = {"runs": len(runs)}
        for m in runs[0]:
            if m != "valid":
                c[m] = med([r[m] for r in runs])
        fps = [r["fps"] for r in runs if r["fps"] is not None]
        c["fps_min"], c["fps_max"] = (min(fps), max(fps)) if fps else (None, None)
        c["spread_pct"] = (c["fps_max"] - c["fps_min"]) / c["fps"] * 100 if fps and c["fps"] else 0
        counts = [r.get("hitch_count") for r in runs if isinstance(r.get("hitch_count"), (int, float))]
        worsts = [r.get("hitch_worst_ms") for r in runs if isinstance(r.get("hitch_worst_ms"), (int, float))]
        c["hitch_count"] = int(sum(counts)) if counts else None
        c["hitch_worst_ms"] = max(worsts) if worsts else None
        out[k] = c
    return out


def f1(v, n=1):
    return "-" if v is None else (f"{v:.{n}f}" if isinstance(v, (int, float)) else str(v))


CFG_ORDER = ["STOCK", "OPTIMIZED", "OPTIMIZED+FG"]


def latency_block(d, device):
    C = cells(d, device)
    lines = [f"## Input latency ({device}): STOCK vs OPTIMIZED vs OPTIMIZED+FG", "",
             "PresentMon 2.x, `--v2_metrics` (input tracking on): AllInput->photon = keyboard or mouse event to photon; Click->photon = mouse click to photon; "
             "PC latency needs Reflex/PCL markers (blank = the game emits none). Medians over runs; the presses are scripted at a fixed rate so sample counts are comparable.", "",
             "| preset | config | runs | fps shown | fps app | AllInput->photon ms | Click->photon ms | PC latency ms | samples | d AllInput vs STOCK |",
             "|---|---|---|---|---|---|---|---|---|---|"]
    n0 = len(lines)
    presets = sorted({k[1] for k in C if not k[2]})
    for p in presets:
        base = C.get(("STOCK", p, ""))
        for cfg in CFG_ORDER:
            c = C.get((cfg, p, ""))
            if not c:
                continue
            dl = ""
            if base and c.get("lat_aip") is not None and base.get("lat_aip"):
                dl = f"{c['lat_aip'] - base['lat_aip']:+.1f} ms ({(c['lat_aip'] / base['lat_aip'] - 1) * 100:+.0f}%)"
            lines.append(f"| {p} | {cfg} | {c['runs']} | {f1(c['display_fps'])} | {f1(c['app_fps'])} | {f1(c['lat_aip'])} | {f1(c['lat_click'])} | {f1(c['lat_pcl'])} | {c['lat_n']} | {dl} |")
    if len(lines) == n0:
        lines.append("| (no results yet) | | | | | | | | | |")
    lines += ["", "FG note: with frame generation `fps shown` counts generated frames, `fps app` only the rendered ones; read latency together with `fps app`.", ""]
    return "\n".join(lines)


def table_md(d, device):
    C = cells(d, device)
    L = [f"# Results ({device})", "",
         f"Generated by `py -I tm.py table`; {sum(c['runs'] for c in C.values())} valid runs in {len(C)} cells. Medians over runs, [min-max] fps. "
         "`d fps` is a descriptive comparison with STOCK of the same preset. Run ranges are not confidence intervals; this table does not retain ABBA pairing. Use `tm.py paired` for pinned launch pairs. New parser fps uses submission intervals; display fps is separate. Archived summaries are not silently recalculated.", ""]
    presets = sorted({k[1] for k in C if not k[2]}, key=lambda p: (p != "MAX", p != "USER", p))
    hdr = "| config | fps [min-max] | 1% low | p99 ms | move fps | combat fps | GPU ms | CPU ms | GPU % | CPU % | GPU C | CPU C | GPU W | CPU W | fps/GPU W | hitch n | hitch worst ms | d fps vs STOCK |"
    for p in presets:
        L += [f"## Preset {p}", "", hdr, "|" + "---|" * 18]
        base = C.get(("STOCK", p, ""))
        for cfg in CFG_ORDER:
            c = C.get((cfg, p, ""))
            if not c:
                continue
            dv = ""
            if base and cfg != "STOCK" and base["fps"]:
                dd = (c["fps"] / base["fps"] - 1) * 100
                real = abs(dd) > max(base["spread_pct"], c["spread_pct"])
                dv = f"{dd:+.1f}%" + ("" if real else " (within run spread)")
            eff = c["fps"] / c["gpu_w"] if c.get("gpu_w") and c.get("fps") else None
            L.append(f"| {cfg} | {f1(c['fps'])} [{f1(c['fps_min'])}-{f1(c['fps_max'])}] | {f1(c['low1'])} | {f1(c['p99'])} | {f1(c['move_fps'])} | {f1(c['combat_fps'])} | "
                     f"{f1(c['gpu_ms'], 2)} | {f1(c['cpu_ms'], 2)} | {f1(c['gpu_pct'], 0)} | {f1(c['cpu_pct'], 0)} | {f1(c['gpu_temp'], 0)} | {f1(c['cpu_temp'], 0)} | "
                     f"{f1(c['gpu_w'], 0)} | {f1(c['cpu_w'], 0)} | {f1(eff, 2)} | {f1(c.get('hitch_count'), 0)} | {f1(c.get('hitch_worst_ms'), 1)} | {dv} |")
        L.append("")
    # bang for buck: sweep cells vs the base-preset cell of the same config
    pm = json.load(open(os.path.join(os.path.dirname(os.path.abspath(__file__)), "presets.json"), encoding="utf-8-sig"))
    vis = pm.get("visual", {})
    basep = pm.get("base_preset", "MAX")
    for cfg in CFG_ORDER:
        base = C.get((cfg, basep, ""))
        sw = {k: c for k, c in C.items() if k[0] == cfg and k[2]}
        if not base or not sw:
            continue
        bms = 1000 / base["fps"]
        L += [f"## Bang for buck ({cfg}): one setting changed from {basep}", "",
              f"Base {basep}: {base['fps']:.1f} fps = {bms:.2f} ms (spread {base['spread_pct']:.1f}%). `ms saved` = base frame ms - cell frame ms (positive = cheaper). "
              "Visual grade 0 invisible .. 3 obvious (presets.json `visual`; correct it after looking at the screenshots). Values inside the noise are marked.", "",
              "| setting | level | fps | ms saved | fps gain | visual grade | ms saved per grade | GPU W | verdict |", "|---|---|---|---|---|---|---|---|---|"]
        rows = []
        for (_, _, sk), c in sw.items():
            key, val = sk.split("=", 1)
            ms = bms - 1000 / c["fps"]
            g = vis.get(f"{key}:{val}")
            real = abs(ms) > bms * max(base["spread_pct"], c["spread_pct"]) / 100
            rows.append((ms, key, val, c, g, real))
        for ms, key, val, c, g, real in sorted(rows, key=lambda r: -r[0]):
            per = "-" if g is None else ("free" if g == 0 and ms > 0 else f"{ms / g:.2f}" if g else "-")
            vd = "noise" if not real else ("FREE LUNCH" if (g == 0 and ms > 0) else ("good" if g is not None and g <= 1 and ms > 0 else ("costly look" if g is not None and g >= 3 else ("trade-off" if g == 2 and ms > 0 else ""))))
            L.append(f"| {key} | {val} | {f1(c['fps'])} | {ms:+.2f} | {(c['fps'] / base['fps'] - 1) * 100:+.1f}% | {'' if g is None else g} | {per} | {f1(c['gpu_w'], 0)} | {vd} |")
        L.append("")
    L.append(latency_block(d, device))
    return "\n".join(L) + "\n"


def paired_runs(records):
    """Fresh launches are the statistical units. An ABBA quartet is one block.

    Adjacent pairs are descriptive; neither frame counts nor two pairs within
    one quartet are independent replications. No CI or performance verdict is
    inferred from a preliminary block. Ordering alone cannot establish equal
    wall-time spacing or cancel nonlinear scene drift.
    """
    import xml.etree.ElementTree as ET
    grouped = {}
    for j in records:
        if j.get("cfg") not in ("STOCK", "OPTIMIZED") or j.get("sweep"):
            raise ValueError("paired accepts pinned STOCK/OPTIMIZED without sweeps")
        try:
            root = ET.fromstring(j["graphics_xml"])
            fg = root.findall(".//FRAME_GENERATION")
            if len(fg) != 1 or (fg[0].text or "").strip() != "OFF":
                raise ValueError("missing, duplicated or non-OFF FRAME_GENERATION")
            header = j["input"]
            pins = tuple(header[k].lower() for k in ("asi_sha256", "ini_sha256"))
            if any(not re.fullmatch(r"[0-9a-f]{64}", p) for p in pins):
                raise ValueError("missing pinned artifact hashes")
            windows = j["windows"]
            st = [w for w in windows if w["kind"] == "stand"]
            if len(st) != header["stand_windows"] or not all(w.get("ok") for w in windows):
                raise ValueError("missing or invalid required windows")
            expected = {"stand": header["stand_windows"], "move": 1,
                        "combat": int(bool(j.get("window_protocol_complete")))}
            if any(sum(w["kind"] == k for w in windows) != n for k, n in expected.items()) or any(w["kind"] not in expected for w in windows):
                raise ValueError("required window counts differ")
            for w in windows:
                seconds = 25 if w["kind"] == "combat" else header["window_s"]
                if w.get("requested_s") != seconds or not math.isfinite(w.get("duration_s", float("nan"))) or w.get("duration_s", 0) < .9*seconds:
                    raise ValueError("short or unverified capture coverage")
            if any(w.get("fps_metric") not in ("FrameTime", "MsBetweenPresents") for w in windows):
                raise ValueError("recompute CSVs with the audited submission parser")
            if any(set(w.get("frame_types", [])) - {"Application", "NotSet", ""} for w in windows):
                raise ValueError("generated/unknown frame types contradict FG OFF control")
            summary = run_summary(j)
            if not summary["valid"]:
                raise ValueError("invalid launch")
            first = min(_dt.datetime.fromisoformat(w["t0"]).timestamp() for w in st)
            signature = (pins, j["graphics_xml"], tuple(w["fps_metric"] for w in windows),
                         j.get("full_protocol", False), j.get("window_protocol_complete", False))
            key = (j["preset"], int(j["run"]))
        except (KeyError, TypeError, ET.ParseError) as exc:
            raise ValueError("incomplete pinned launch provenance") from exc
        arms = grouped.setdefault(key, {})
        if j["cfg"] in arms:
            raise ValueError("duplicate launch arm")
        arms[j["cfg"]] = (summary, first, signature)
    presets = {}
    metrics = ("fps", "low1", "p99", "gpu_ms", "cpu_ms")
    for (preset, run), arms in sorted(grouped.items()):
        if set(arms) != {"STOCK", "OPTIMIZED"}:
            raise ValueError("missing paired launch arm")
        a, ta, sa = arms["STOCK"]; b, tb, sb = arms["OPTIMIZED"]
        if sa != sb:
            raise ValueError("paired artifacts, picture, metrics or protocol differ")
        entry = presets.setdefault(preset, {"signature": sa, "pairs": [], "blocks": []})
        if entry["signature"] != sa:
            raise ValueError("mixed release/settings/protocol within preset")
        entry["pairs"].append({"run": run, "order": "AB" if ta < tb else "BA", "a_t0": ta, "b_t0": tb,
            "delta": {k: b[k]-a[k] if a[k] is not None and b[k] is not None else None for k in metrics}})
    for entry in presets.values():
        pairs = entry["pairs"]
        if [p["run"] for p in pairs] != list(range(1, len(pairs)+1)):
            raise ValueError("missing launch pair in sequence")
        for i in range(0, len(pairs)-1, 2):
            p, q = pairs[i:i+2]
            if p["order"] == q["order"]:
                raise ValueError("launch pairs are not balanced ABBA/BAAB")
            entry["blocks"].append({"runs": [p["run"], q["run"]],
                "mean_delta": {k: (p["delta"][k]+q["delta"][k])/2 if p["delta"][k] is not None and q["delta"][k] is not None else None for k in metrics},
                "time_center_delta_s": (p["b_t0"]+q["b_t0"]-p["a_t0"]-q["a_t0"])/2})
        entry["unbalanced_tail_pairs"] = len(pairs) % 2
        entry["balanced_blocks"] = len(entry["blocks"])
        entry["full_protocol"], entry["window_protocol_complete"] = entry.pop("signature")[-2:]
    return {"unit": "fresh launch; two opposite adjacent pairs form one balanced block",
            "runtime_fg_off_verified": False, "ci95": None, "significance_verdict": None,
            "note": "Descriptive pairing only. Frame counts are not replications; order does not prove scene or wall-time balance.",
            "presets": presets}


def paired(d, device):
    records = load_final(d, device)
    if not records:
        raise ValueError("no pinned launch records")
    for j in records:
        for w in j.get("windows", []):
            path = w["csv"]
            if not os.path.isabs(path):
                path = os.path.join(d, path)
            result = stats(path)
            observed = bool(w.get("ok"))
            w.update(result)
            w["ok"] = observed and result["ok"]
            w["duration_s"] = result.get("capture_duration_s", 0)
    return paired_runs(records)


if __name__ == "__main__":
    a = sys.argv[1:]
    if a[0] == "stats": print(json.dumps(stats(a[1])))
    elif a[0] == "paired": print(json.dumps(paired(a[1], a[2]), allow_nan=False))
    elif a[0] == "logtail": logtail(a[1], a[2], a[3])
    elif a[0] == "report": sys.exit(report(a[1], a[2], float(a[a.index("--noise-floor") + 1]) if "--noise-floor" in a else 0.5))
    elif a[0] == "best": best(a[1], a[2], a[3], len(a) > 4 and a[4] == "low")
    elif a[0] == "sensors":
        print(json.dumps(sensors(a[1], a[a.index("--start") + 1] if "--start" in a else None, a[a.index("--end") + 1] if "--end" in a else None)))
    elif a[0] == "ingest":
        ingest(a[1], a[2], a[a.index("--hwinfo") + 1] if "--hwinfo" in a else None, a[a.index("--rtss") + 1] if "--rtss" in a else None)
    elif a[0] == "latency":
        t = latency_block(a[1], a[2])
        open(os.path.join(a[1], f"latency_{a[2]}.md"), "w", encoding="utf-8").write(t + "\n")
        print(t)
    elif a[0] == "table":
        out = a[3] if len(a) > 3 else os.path.join(a[1], f"results_table_{a[2]}.md")
        t = table_md(a[1], a[2])
        open(out, "w", encoding="utf-8").write(t)
        print(t)
        print("->", out)
