"""Helper for the test matrix (run via `py -I tm.py ...`).
  stats  <presentmon.csv>                     -> one JSON line: fps, 1%low, p99 ms, gpu/cpu busy ms
  logtail <log> <offset> <out.txt>            -> copy the log lines after byte offset that carry stats/ GP / SP / lm / mem
  report <results_dir> <device> [--noise-floor 0.5]  -> summary.csv + verdict.md in results_dir
  best   <results_dir> <device> <matrix.json> [low] -> ini lines (section|key|value) of all KEEP items, for the BEST combo
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
    gpu = [v for r in app if (v := col(r, "MsGPUBusy", "GPUBusy")) is not None]
    cpu = [v for r in app if (v := col(r, "MsCPUBusy", "CPUBusy")) is not None]
    if not xs:
        return {"ok": False}
    avg = statistics.mean(xs)
    p99 = pct(xs, .99)
    return {"ok": True, "frames": len(xs), "fps": round(1000 / avg, 2), "low1": round(1000 / p99, 2), "p99": round(p99, 2),
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
            j = json.load(open(os.path.join(d, fn)))
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
    m = json.load(open(mfile)); items = {i["id"]: i for i in m["items"]}
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


if __name__ == "__main__":
    a = sys.argv[1:]
    if a[0] == "stats": print(json.dumps(stats(a[1])))
    elif a[0] == "logtail": logtail(a[1], a[2], a[3])
    elif a[0] == "report": sys.exit(report(a[1], a[2], float(a[a.index("--noise-floor") + 1]) if "--noise-floor" in a else 0.5))
    elif a[0] == "best": best(a[1], a[2], a[3], len(a) > 4 and a[4] == "low")
