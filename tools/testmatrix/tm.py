"""Helper for the test matrix (run via `py -I tm.py ...`).
  stats  <presentmon.csv>                     -> one JSON line: fps, 1%low, p99 ms, gpu/cpu busy ms
  logtail <log> <offset> <out.txt>            -> copy the log lines after byte offset that carry stats/ GP / SP / lm / mem
  report <results_dir> <device> [--noise-floor 0.5]  -> summary.csv + verdict.md in results_dir
  best   <results_dir> <device> <matrix.json> [low] -> ini lines (section|key|value) of all KEEP items, for the BEST combo
 Final phase (results\\final_<device>_*.json written by final_pc.ps1; see README.md):
  sensors <log.csv> [--start ISO --end ISO]   -> JSON of HWiNFO / RTSS / Afterburner-style CSV means+max (CPU/GPU %, temps, W, MHz)
  ingest  <results_dir> <device> [--hwinfo f.csv] [--rtss f.csv] -> attach sensor stats to every final_<device>_* window by its t0/t1
  latency <results_dir> <device>              -> latency_<device>.md : STOCK vs OPTIMIZED vs OPTIMIZED+FG block
  table   <results_dir> <device> [out.md]     -> results_table_<device>.md : README tables (preset x config, sweep bang-for-buck)
"""
import csv, json, os, re, statistics, sys


def pct(xs, p):
    xs = sorted(xs)
    return xs[min(len(xs) - 1, int(len(xs) * p))] if xs else float("nan")


def col(row, *names):
    for n in names:
        v = row.get(n)
        if v not in (None, "", "NA"):
            try:
                return float(v)
            except ValueError:
                pass
    return None


def stats(path):
    rows = list(csv.DictReader(open(path, newline="", encoding="utf-8-sig")))
    app = [r for r in rows if r.get("FrameType", "Application") in ("Application", "NotSet", "")]
    disp = [v for r in rows if (v := col(r, "MsBetweenDisplayChange")) is not None and v > 0]
    pres = [v for r in rows if (v := col(r, "MsBetweenPresents", "FrameTime")) is not None]
    xs = disp or pres
    gpu = [v for r in app if (v := col(r, "MsGPUBusy", "GPUBusy", "MsGPUTime")) is not None]
    cpu = [v for r in app if (v := col(r, "MsCPUBusy", "CPUBusy")) is not None]
    if not xs:
        return {"ok": False}
    avg = statistics.mean(xs)
    p99 = pct(xs, .99)
    lat = {}        # PresentMon 2.x latency columns (need --v2_metrics + input during the window); NA rows are skipped
    for key, name in (("lat_aip", "MsAllInputToPhotonLatency"), ("lat_click", "MsClickToPhotonLatency"), ("lat_pcl", "MsPCLatency"),
                      ("lat_disp", "MsDisplayLatency")):
        v = [x for r in rows if (x := col(r, name)) is not None and x > 0]
        if v:
            lat[key] = round(statistics.median(v), 2)
            lat[key + "_p95"] = round(pct(v, .95), 2)
            lat[key + "_n"] = len(v)
    appx = [v for r in app if (v := col(r, "MsBetweenPresents", "FrameTime")) is not None and v > 0]
    ftypes = sorted({r.get("FrameType") for r in rows if r.get("FrameType")})
    return {"ok": True, **lat, "app_fps": round(1000 / statistics.mean(appx), 2) if appx else None, "frame_types": ftypes, "frames": len(xs), "fps": round(1000 / avg, 2), "low1": round(1000 / p99, 2), "p99": round(p99, 2),
            "gpu_ms": round(statistics.mean(gpu), 2) if gpu else None, "cpu_ms": round(statistics.mean(cpu), 2) if cpu else None}


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
    ("cpu_temp", r"cpu.*(tctl|tdie|package|die|temperature)|^cpu temp", r"power|clock|usage|vid|limit|margin"),
    ("cpu_w",    r"cpu.*package power|cpu.*(socket|ppt) ?power|^cpu power", r"limit|%|gpu"),
    ("cpu_mhz",  r"core clocks? \(avg\)|average effective clock|cpu clock|cpu frequency|^core clock avg", r"gpu|ratio|limit"),
    ("gpu_pct",  r"gpu.*(core load|utilization|usage|load)|^gpu usage", r"memory|mem |video|d3d|encode|decode|power"),
    ("gpu_hot",  r"gpu.*(hot ?spot|junction)", r"memory"),
    ("gpu_temp", r"^gpu.*temp", r"hot ?spot|junction|memory|limit"),
    ("gpu_w",    r"gpu.*(asic power|board power|total board|chip power)|^gpu power|^power$", r"limit|%|effective"),
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
            j = json.load(open(os.path.join(d, fn), encoding="utf-8-sig"))
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
         "app_fps": gm(st, "app_fps"), "move_fps": gm(mv, "fps"), "combat_fps": gm(cb, "fps"),
         "lat_aip": gm(lt, "lat_aip"), "lat_click": gm(lt, "lat_click"), "lat_pcl": gm(lt, "lat_pcl"),
         "lat_n": sum(w.get("lat_aip_n") or w.get("lat_click_n") or 0 for w in lt),
         "load1_s": j.get("load1_s"), "load2_s": j.get("load2_s"), "hitches": j.get("hitches"), "valid": bool(j.get("valid", True)) and bool(st)}
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
            lines.append(f"| {p} | {cfg} | {c['runs']} | {f1(c['fps'])} | {f1(c['app_fps'])} | {f1(c['lat_aip'])} | {f1(c['lat_click'])} | {f1(c['lat_pcl'])} | {c['lat_n']} | {dl} |")
    if len(lines) == n0:
        lines.append("| (no results yet) | | | | | | | | | |")
    lines += ["", "FG note: with frame generation `fps shown` counts generated frames, `fps app` only the rendered ones; read latency together with `fps app`.", ""]
    return "\n".join(lines)


def table_md(d, device):
    C = cells(d, device)
    L = [f"# Results ({device})", "",
         f"Generated by `py -I tm.py table`; {sum(c['runs'] for c in C.values())} valid runs in {len(C)} cells. Medians over runs, [min-max] fps. "
         "`d fps` is vs STOCK of the same preset and counts as real only when larger than the larger run spread of the two cells (control = STOCK vs STOCK).", ""]
    presets = sorted({k[1] for k in C if not k[2]}, key=lambda p: (p != "MAX", p != "USER", p))
    hdr = "| config | fps [min-max] | 1% low | p99 ms | move fps | combat fps | GPU ms | CPU ms | GPU % | CPU % | GPU C | CPU C | GPU W | CPU W | fps/GPU W | d fps vs STOCK |"
    for p in presets:
        L += [f"## Preset {p}", "", hdr, "|" + "---|" * 16]
        base = C.get(("STOCK", p, ""))
        for cfg in CFG_ORDER:
            c = C.get((cfg, p, ""))
            if not c:
                continue
            dv = ""
            if base and cfg != "STOCK" and base["fps"]:
                dd = (c["fps"] / base["fps"] - 1) * 100
                real = abs(dd) > max(base["spread_pct"], c["spread_pct"])
                dv = f"{dd:+.1f}%" + ("" if real else " (noise)")
            eff = c["fps"] / c["gpu_w"] if c.get("gpu_w") and c.get("fps") else None
            L.append(f"| {cfg} | {f1(c['fps'])} [{f1(c['fps_min'])}-{f1(c['fps_max'])}] | {f1(c['low1'])} | {f1(c['p99'])} | {f1(c['move_fps'])} | {f1(c['combat_fps'])} | "
                     f"{f1(c['gpu_ms'], 2)} | {f1(c['cpu_ms'], 2)} | {f1(c['gpu_pct'], 0)} | {f1(c['cpu_pct'], 0)} | {f1(c['gpu_temp'], 0)} | {f1(c['cpu_temp'], 0)} | "
                     f"{f1(c['gpu_w'], 0)} | {f1(c['cpu_w'], 0)} | {f1(eff, 2)} | {dv} |")
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


if __name__ == "__main__":
    a = sys.argv[1:]
    if a[0] == "stats": print(json.dumps(stats(a[1])))
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
