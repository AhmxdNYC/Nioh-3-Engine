"""Pacing census: 1% / 0.1% low and the cause of the slowest frames (#103464).

    py -I tools/bench/pacing_census.py --pm presentmon.csv [--log Nioh3EngineFix.log] [--label X]
    py -I tools/bench/pacing_census.py --log Nioh3EngineFix.log [--label X]      (HITCH-only)
    py -I tools/bench/pacing_census.py --mango mangohud.csv [--label X]          (Ally MangoHud)

Prints one JSON line per input.

PresentMon mode: every frame. 1% low = 1000 / p99 frame time, 0.1% low = 1000 / p99.9
(nearest rank). Each frame of the slowest 1% gets a bucket:
  gpu      MsGPUBusy >= 0.8 x frame
  present  MsInPresentAPI >= 0.5 x frame (present / vsync / flip queue wait)
  cpu      MsCPUBusy >= 0.8 x frame (main thread side; split further with --log)
  wait     none of these (app sleep / limiter / idle; MsCPUWait large)
With --log, PresentMon frames >= the log's HitchLogMs are aligned to HITCH lines (best time
offset over the big frames) and a cpu frame with a HITCH match is re-labelled by the HITCH fields.

HITCH fields -> bucket (first rule that fits):
  loadscreen pause 1 or menu 1            (load screen / menu: not gameplay)
  gpu        GF fence wait >= 30 % of gap
  present    W6 present wait >= 30 % of gap
  stream     rd >= 8 MB or pf >= 5000 or ld infl > 0   (file load / streaming in that gap)
  cpu-busy   game thread CPU >= 60 % of gap            (main thread itself running)
  cpu-starved                                           (main thread not running: ready/waiting)
Frame time lost = gap - median frame (median from PresentMon, else 1000 / mean OSD fps).
"""
import argparse
import csv
import json
import math
import os
import re
import statistics
import sys



def pct(xs, p):
    """Nearest-rank percentile (same rule as tools/bench/hitch.py and tools/testmatrix/tm.py)."""
    xs = sorted(xs)
    if not xs:
        return None
    i = max(0, min(len(xs) - 1, math.ceil(len(xs) * p) - 1))
    return xs[i]

HITCH_RE = re.compile(r"^(?:[A-Z]{1,3} )?\[(\d+):(\d+):(\d+(?:\.\d+)?)\] HITCH #\d+ gap ([\d.]+) ms \| gt cpu ([\d.]+) ms (\d+)%")
OSD_RE = re.compile(r"^(?:[A-Z]{1,3} )?\[(\d+):(\d+):(\d+(?:\.\d+)?)\] OSD: stats: rendered ([\d.]+) fps")
THR_RE = re.compile(r"HitchLogMs = (\d+)")


def fnum(s):
    try:
        v = float(s)
        return v if math.isfinite(v) else None
    except (TypeError, ValueError):
        return None


def field(line, rx, cast=float, default=0.0):
    m = re.search(rx, line)
    return cast(m.group(1)) if m else default


def parse_hitch(line):
    m = HITCH_RE.match(line)
    if not m:
        return None
    hh, mm, ss = int(m.group(1)), int(m.group(2)), float(m.group(3))
    gap = float(m.group(4))
    h = {
        "t": hh * 3600 + mm * 60 + ss,
        "gap": gap,
        "gt_pct": float(m.group(6)),
        "cores": field(line, r"proc cpu [\d.]+ ms ([\d.]+) cores"),
        "pf": field(line, r"\| pf (\d+)"),
        "rd": field(line, r"\| rd ([\d.]+) MB"),
        "gf_ms": field(line, r"GF \d+/([\d.]+)"),
        "w6_ms": field(line, r"W6 \d+/([\d.]+)"),
        "menu": field(line, r"menu (\d+)", int, 0),
        "pause": field(line, r"pause (\d+)", int, 0),
        "infl": field(line, r"infl (\d+)", int, 0),
    }
    h["bucket"] = hitch_bucket(h)
    return h


def hitch_bucket(h):
    g = h["gap"]
    if h["pause"] or h["menu"]:
        return "loadscreen"
    if h["gf_ms"] >= 0.3 * g:
        return "gpu"
    if h["w6_ms"] >= 0.3 * g:
        return "present"
    if h["rd"] >= 8 or h["pf"] >= 5000 or h["infl"] > 0:
        return "stream"
    if h["gt_pct"] >= 60:
        return "cpu-busy"
    return "cpu-starved"


def read_log(path):
    hitches, osd, thr = [], [], None
    with open(path, encoding="utf-8", errors="replace") as f:
        for line in f:
            if "HITCH #" in line:
                h = parse_hitch(line)
                if h:
                    hitches.append(h)
            elif "OSD: stats: rendered" in line:
                m = OSD_RE.match(line)
                if m:
                    osd.append((int(m.group(1)) * 3600 + int(m.group(2)) * 60 + float(m.group(3)), float(m.group(4))))
            elif thr is None and "HitchLogMs" in line:
                m = THR_RE.search(line)
                if m:
                    thr = int(m.group(1))
    return hitches, osd, thr or 50


def lows(fts):
    p99, p999 = pct(fts, 0.99), pct(fts, 0.999)
    return {
        "frames": len(fts),
        "avg_fps": round(1000 * len(fts) / sum(fts), 1) if fts else None,
        "median_ms": round(statistics.median(fts), 2) if fts else None,
        "p99_ms": round(p99, 1) if p99 else None,
        "p999_ms": round(p999, 1) if p999 else None,
        "low1_fps": round(1000 / p99, 1) if p99 else None,
        "low01_fps": round(1000 / p999, 1) if p999 else None,
    }


def rank(lost):
    tot = sum(lost.values()) or 1
    return [{"bucket": k, "lost_ms": round(v), "share": round(100 * v / tot, 1)}
            for k, v in sorted(lost.items(), key=lambda kv: -kv[1]) if v > 0]


def align(pm_big, hitches):
    """pm_big: [(t_s, ft_ms)], hitches: [h]. Best offset (log_t - pm_t) over big frames."""
    if not pm_big or not hitches:
        return None, 0
    cands = sorted(hitches, key=lambda h: -h["gap"])[:30]
    pmc = sorted(pm_big, key=lambda x: -x[1])[:30]
    best, score = None, 0
    hs = sorted(h["t"] for h in hitches)
    import bisect
    for h in cands:
        for t, ft in pmc:
            if abs(ft - h["gap"]) > max(15, 0.15 * h["gap"]):
                continue
            off = h["t"] - t
            s = 0
            for tt, _ in pm_big:
                i = bisect.bisect_left(hs, tt + off - 0.15)
                if i < len(hs) and hs[i] <= tt + off + 0.15:
                    s += 1
            if s > score:
                best, score = off, s
    return best, score


def pm_mode(path, logpath):
    rows = []
    with open(path, encoding="utf-8-sig", errors="replace", newline="") as f:
        for r in csv.DictReader(f):
            ft = fnum(r.get("MsBetweenPresents", r.get("FrameTime")))  # PresentMon 1.x / 2.x column names
            if ft is None or ft <= 0:
                continue
            if r.get("TimeInMs"):
                t = (fnum(r.get("TimeInMs")) or 0) / 1000.0
            elif r.get("CPUStartDateTime"):  # 2026-11-12 13:01:38.333539700 -> seconds of day
                hh, mm, ss = r["CPUStartDateTime"].split(" ")[1].split(":")
                t = int(hh) * 3600 + int(mm) * 60 + float(ss[:12])
            else:
                t = fnum(r.get("TimeInSeconds")) or 0
            rows.append({
                "t": t,
                "ft": ft,
                "cpu": fnum(r.get("MsCPUBusy", r.get("CPUBusy"))) or 0,
                "gpu": fnum(r.get("MsGPUBusy", r.get("GPUBusy"))) or 0,
                "pres": fnum(r.get("MsInPresentAPI")) or 0,
            })
    fts = [r["ft"] for r in rows]
    out = lows(fts)
    med = out["median_ms"] or 0
    p99 = pct(fts, 0.99)
    slow = [r for r in rows if r["ft"] >= p99]
    hitches, thr, off, score = [], 50, None, 0
    if logpath:
        hitches, _, thr = read_log(logpath)
        big = [(r["t"], r["ft"]) for r in rows if r["ft"] >= thr]
        off, score = align(big, hitches)
        out["align"] = {"offset_s": round(off, 3) if off is not None else None, "matched": score, "pm_big": len(big),
                        "hitches": len(hitches), "hitch_ms": thr}
    import bisect
    ht = [h["t"] for h in hitches]
    lost, count = {}, {}
    for r in slow:
        if r["gpu"] >= 0.8 * r["ft"]:
            b = "gpu"
        elif r["pres"] >= 0.5 * r["ft"]:
            b = "present"
        elif r["cpu"] >= 0.8 * r["ft"]:
            b = "cpu"
        else:
            b = "wait"
        if off is not None and r["ft"] >= thr and b in ("cpu", "wait"):
            i = bisect.bisect_left(ht, r["t"] + off - 0.15)
            if i < len(ht) and ht[i] <= r["t"] + off + 0.15:
                b = hitches[i]["bucket"]
        if b == "cpu" and hitches:
            b = "cpu(<hitch ms)"
        lost[b] = lost.get(b, 0) + max(0, r["ft"] - med)
        count[b] = count.get(b, 0) + 1
    out["slow1"] = {"n": len(slow), "count": count, "ranked": rank(lost)}
    out["gameplay_ex_loadscreen"] = None
    if off is not None:
        ls = [h for h in hitches if h["bucket"] == "loadscreen"]
        if ls:
            win = [(h["t"] - h["gap"] / 1000 - 0.5, h["t"] + 0.5) for h in ls]
            keep = [r["ft"] for r in rows if not any(a <= r["t"] + off <= b for a, b in win)]
            out["gameplay_ex_loadscreen"] = lows(keep)
    return out


def log_mode(logpath):
    hitches, osd, thr = read_log(logpath)
    if len(osd) >= 2:
        frames = sum(fps * (osd[i][0] - osd[i - 1][0]) for i, (_, fps) in enumerate(osd) if i > 0)
        secs = osd[-1][0] - osd[0][0]
        live = [h for h in hitches if osd[0][0] <= h["t"] <= osd[-1][0]]
    else:
        frames, secs, live = 0, 0, hitches
    mean_fps = frames / secs if secs else None
    med = 1000 / mean_fps if mean_fps else 16.7
    out = {"hitch_ms": thr, "secs": round(secs), "frames_est": round(frames), "avg_fps": round(mean_fps, 1) if mean_fps else None,
           "hitches": len(live)}
    gaps = sorted((h["gap"] for h in live), reverse=True)

    def low(p):
        k = int(math.ceil(frames * (1 - p)))
        if frames and k <= len(gaps) and k > 0:
            return round(1000 / gaps[k - 1], 1)
        return "> %.0f" % (1000 / thr)

    out["low1_fps"], out["low01_fps"] = low(0.99), low(0.999)
    # gameplay only: drop load-screen / menu hitch frames (and their frames from the count)
    ls = [h for h in live if h["bucket"] == "loadscreen"]
    ls_secs = sum(h["gap"] for h in ls) / 1000
    frames_all, gaps_all = frames, gaps
    frames = max(0, frames - len(ls))
    gaps = sorted((h["gap"] for h in live if h["bucket"] != "loadscreen"), reverse=True)
    out["low1_play"], out["low01_play"], out["loadscreen_s"] = low(0.99), low(0.999), round(ls_secs, 1)
    frames, gaps = frames_all, gaps_all
    lost, count = {}, {}
    k1 = int(math.ceil(frames * 0.01)) if frames else len(live)
    for h in sorted(live, key=lambda h: -h["gap"])[:k1]:
        b = h["bucket"]
        lost[b] = lost.get(b, 0) + max(0, h["gap"] - med)
        count[b] = count.get(b, 0) + 1
    out["slow1"] = {"n": min(k1, len(live)), "count": count, "ranked": rank(lost)}
    play = [h for h in live if h["bucket"] != "loadscreen"]
    lost2 = {}
    for h in play:
        lost2[h["bucket"]] = lost2.get(h["bucket"], 0) + max(0, h["gap"] - med)
    out["all_hitches_gameplay"] = {"n": len(play), "ranked": rank(lost2)}
    return out


def mango_mode(path):
    rows = []
    with open(path, encoding="utf-8", errors="replace") as f:
        lines = f.read().splitlines()
    hi = next(i for i, l in enumerate(lines) if l.startswith("fps,") or ",frametime," in l or l.startswith("frametime"))
    for r in csv.DictReader(lines[hi:]):
        ft = fnum(r.get("frametime"))
        if ft and ft > 0:
            rows.append({"ft": ft, "gpu": fnum(r.get("gpu_load")) or 0, "cpu": fnum(r.get("cpu_load")) or 0})
    fts = [r["ft"] for r in rows]
    rows = [r for r in rows if r["ft"] < 5000]  # boot rows carry a raw ns counter (1.5e7), not ms
    fts = [r["ft"] for r in rows]
    out = lows(fts)
    out["note"] = "MangoHud rows are per log interval averages, not single frames: lows are understated"
    med, p99 = out["median_ms"], pct(fts, 0.99)
    lost, count = {}, {}
    for r in (r for r in rows if r["ft"] >= p99):
        b = "gpu" if r["gpu"] >= 95 else "cpu/other"
        lost[b] = lost.get(b, 0) + max(0, r["ft"] - med)
        count[b] = count.get(b, 0) + 1
    out["slow1"] = {"n": sum(count.values()), "count": count, "ranked": rank(lost)}
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--pm")
    ap.add_argument("--log")
    ap.add_argument("--mango")
    ap.add_argument("--label", default="")
    a = ap.parse_args()
    if a.pm:
        out = pm_mode(a.pm, a.log)
    elif a.mango:
        out = mango_mode(a.mango)
    elif a.log:
        out = log_mode(a.log)
    else:
        ap.error("need --pm, --log or --mango")
    out = {"label": a.label, **out}
    print(json.dumps(out))


if __name__ == "__main__":
    main()
