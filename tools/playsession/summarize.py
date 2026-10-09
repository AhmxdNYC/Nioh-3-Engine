"""summarize.py - one play session -> report.md + summary.json (tools\\playsession, see README.md).

    py -I tools\\playsession\\summarize.py <session dir>        (stop_session.ps1 runs this)
    py -I tools\\playsession\\summarize.py compare <mod session dir> <stock session dir>   mod-vs-stock table (STOCK mode, README)
    py -I tools\\playsession\\summarize.py --selftest            fake capture + log built from the real source formats; must end SELFTEST OK

Inputs in the session dir (any may be missing; the report says what was missing):
  presentmon.csv      PresentMon 2.x --v2_metrics (--date_time) of Nioh3.exe: frame times, CPU/GPU busy+wait, latency columns
  modlog_run*.log     Nioh3EngineFix.log of each game launch (copied by monitor.ps1 at each exit / by stop_session.ps1);
                      incl. the PadLatency lines (ASI 1.7.35, controller input -> Present / GPU-done, notes\\pad-latency.md)
  sysmon.csv          monitor.ps1: Windows counters every 5 s (CPU %, CPU perf %, GPU 3D %, VRAM, RAM, commit)
  session.json        start_session.ps1: times, versions, ini keys, PresentMon clock bias
  config.xml          the game's settings at session start
A STOCK session (start_session.ps1 -Stock, session.json mode=stock) has no mod log: its report comes from PresentMon + sysmon only.
"""
import array
import bisect
import csv
import datetime as dt
import json
import math
import pathlib
import re
import statistics
import sys

REPO = pathlib.Path(__file__).resolve().parent.parent.parent
SYMBOLS = REPO / "re" / "symbols" / "symbols.tsv"


# ---------------------------------------------------------------- helpers
def pct(sorted_xs, p):
    if not sorted_xs:
        return None
    k = (len(sorted_xs) - 1) * p
    f = math.floor(k)
    c = min(f + 1, len(sorted_xs) - 1)
    return sorted_xs[f] + (sorted_xs[c] - sorted_xs[f]) * (k - f)


def fnum(v, nd=1):
    if v is None:
        return "-"
    if isinstance(v, float) and (math.isnan(v) or math.isinf(v)):
        return "-"
    return ("%%.%df" % nd) % v


def fcol(row, name):
    v = row.get(name)
    if v is None or v == "" or v == "NA":
        return None
    try:
        x = float(v)
    except ValueError:
        return None
    return x if math.isfinite(x) else None


def parse_pm_time(s):
    # "2026-10-8 10:51:34.108067600"
    m = re.match(r"(\d+)-(\d+)-(\d+) (\d+):(\d+):(\d+)(?:\.(\d+))?", s or "")
    if not m:
        return None
    y, mo, d, h, mi, se, fr = m.groups()
    us = int((fr or "0")[:6].ljust(6, "0"))
    return dt.datetime(int(y), int(mo), int(d), int(h), int(mi), int(se), us)


# ---------------------------------------------------------------- frames (PresentMon)
class Frames:
    """Per-frame arrays of one capture (all Nioh3 processes; `pid` separates launches)."""

    COLS = ("FrameTime", "CPUBusy", "CPUWait", "GPULatency", "GPUTime", "GPUBusy", "GPUWait", "DisplayLatency", "DisplayedTime",
            "AnimationError", "AllInputToPhotonLatency", "ClickToPhotonLatency", "PCLatency", "MsPCLatency")

    def __init__(self):
        self.t = array.array("d")    # local POSIX seconds of CPUStartDateTime (or cumulative if absent)
        self.pid = array.array("q")
        self.cols = {c: array.array("d") for c in self.COLS}
        self.ftype = array.array("b")   # 0 rendered (Application/NotSet), 1 generated (FG: AMD_AFMF, Intel_XEFG, ...), 2 repeated
        self.shown = array.array("b")   # Displayed column: 1 shown, 0 dropped, -1 unknown
        self.has_ftype = False
        self.present = set()
        self.rows = 0
        self.header = []

    def load(self, path, bias_s):
        with open(path, newline="", encoding="utf-8-sig", errors="replace") as f:
            r = csv.DictReader(f)
            self.header = r.fieldnames or []
            self.has_ftype = "FrameType" in self.header
            self.present = {c for c in self.COLS if c in self.header}
            # pre-v2 column names as fallbacks
            alias = {"FrameTime": "MsBetweenPresents", "CPUBusy": "MsCPUBusy", "CPUWait": "MsCPUWait", "GPUBusy": "MsGPUBusy",
                     "GPUWait": "MsGPUWait", "GPULatency": "MsGPULatency", "GPUTime": "MsGPUTime", "DisplayedTime": "MsBetweenDisplayChange",
                     "AnimationError": "MsAnimationError", "AllInputToPhotonLatency": "MsAllInputToPhotonLatency",
                     "ClickToPhotonLatency": "MsClickToPhotonLatency", "DisplayLatency": "MsUntilDisplayed"}
            for c, a in alias.items():
                if c not in self.present and a in self.header:
                    self.present.add(c)
            cum = 0.0
            for row in r:
                if row.get("Application", "").lower() not in ("nioh3.exe", ""):
                    continue
                ft = fcol(row, "FrameTime")
                if ft is None:
                    ft = fcol(row, "MsBetweenPresents")
                if ft is None or ft <= 0:
                    continue
                ts = parse_pm_time(row.get("CPUStartDateTime"))
                if ts is not None:
                    tsec = ts.timestamp() - bias_s
                else:
                    cum += ft / 1000.0
                    tsec = cum
                self.t.append(tsec)
                fty = (row.get("FrameType") or "").strip()
                self.ftype.append(0 if fty in ("", "NA", "Application", "NotSet", "Unspecified") else (2 if fty == "Repeated" else 1))
                dsp = (row.get("Displayed") or "").strip().lower()
                self.shown.append(1 if dsp in ("1", "true") else (0 if dsp in ("0", "false") else -1))
                try:
                    self.pid.append(int(row.get("ProcessID") or 0))
                except ValueError:
                    self.pid.append(0)
                for c in self.COLS:
                    v = fcol(row, c)
                    if v is None and c in alias:
                        v = fcol(row, alias[c])
                    if c == "FrameTime":
                        v = ft
                    self.cols[c].append(float("nan") if v is None else v)
                self.rows += 1


def frame_stats(fr, idx=None):
    """Stats over frame indices idx (None = all)."""
    ftc = fr.cols["FrameTime"]
    ii = range(len(ftc)) if idx is None else idx
    ft = [ftc[i] for i in ii]
    if len(ft) < 10:
        return None
    s = sorted(ft)
    tot = sum(ft)
    o = {"frames": len(ft), "seconds": tot / 1000.0, "fps_avg": 1000.0 * len(ft) / tot,
         "ft_p50": pct(s, .50), "ft_p95": pct(s, .95), "ft_p99": pct(s, .99), "ft_p999": pct(s, .999), "ft_max": s[-1]}
    o["low1_fps"] = 1000.0 / o["ft_p99"]
    o["low01_fps"] = 1000.0 / o["ft_p999"]
    n1 = max(1, len(s) // 100)
    n01 = max(1, len(s) // 1000)
    o["low1_avg_fps"] = 1000.0 / (sum(s[-n1:]) / n1)      # average of the worst 1 % (the "1 % low" of many overlays)
    o["low01_avg_fps"] = 1000.0 / (sum(s[-n01:]) / n01)
    # spikes: frame > 2x the median of the previous 60 frames and > median + 8 ms; plus absolute hitch buckets
    spikes = 0
    win = []
    for x in ft:
        if len(win) >= 15:
            med = sorted(win)[len(win) // 2]
            if x > 2 * med and x > med + 8:
                spikes += 1
        win.append(x)
        if len(win) > 60:
            win.pop(0)
    o["spikes"] = spikes
    o["spikes_per_min"] = spikes / (tot / 60000.0)
    for lim in (33.4, 50, 100, 250):
        o["over_%g" % lim] = sum(1 for x in ft if x > lim)
    # pacing
    o["ft_stdev"] = statistics.pstdev(ft)
    d = [abs(ft[k] - ft[k - 1]) for k in range(1, len(ft))]
    o["ft_mean_abs_delta"] = sum(d) / len(d)
    o["ft_delta_p99"] = pct(sorted(d), .99)

    def colstat(name, key):
        if name not in fr.present:
            return
        arr = fr.cols[name]
        v = [arr[i] for i in ii]
        v = [x for x in v if not math.isnan(x)]
        if not v:
            o[key + "_n"] = 0
            return
        sv = sorted(v)
        o[key + "_avg"] = sum(v) / len(v)
        o[key + "_p50"] = pct(sv, .5)
        o[key + "_p95"] = pct(sv, .95)
        o[key + "_n"] = len(v)

    for name, key in (("CPUBusy", "cpu_busy"), ("CPUWait", "cpu_wait"), ("GPUBusy", "gpu_busy"), ("GPUWait", "gpu_wait"),
                      ("GPULatency", "gpu_latency"), ("GPUTime", "gpu_time"), ("DisplayedTime", "displayed"),
                      ("DisplayLatency", "display_latency"), ("AllInputToPhotonLatency", "aip_latency"),
                      ("ClickToPhotonLatency", "click_latency"), ("PCLatency", "pc_latency"), ("MsPCLatency", "pc_latency")):
        colstat(name, key)
    if "DisplayLatency" in fr.present and "CPUBusy" in fr.present:
        # PresentMon v2: DisplayLatency runs from the frame's CPU start, CPUBusy from the CPU start to the Present call, so
        # DisplayLatency - CPUBusy = Present call -> on screen (the part after PadLatency's input->present)
        dl, cb = fr.cols["DisplayLatency"], fr.cols["CPUBusy"]
        v = sorted(dl[i] - cb[i] for i in ii if not math.isnan(dl[i]) and not math.isnan(cb[i]))
        if v:
            o["present_display_p50"], o["present_display_p95"], o["present_display_n"] = pct(v, .5), pct(v, .95), len(v)
    if "AnimationError" in fr.present:
        v = [abs(fr.cols["AnimationError"][i]) for i in ii if not math.isnan(fr.cols["AnimationError"][i])]
        if v:
            o["anim_err_abs_avg"] = sum(v) / len(v)
    if "GPUBusy" in fr.present:
        gb = fr.cols["GPUBusy"]
        n = [i for i in ii if not math.isnan(gb[i])]
        if n:
            o["gpu_bound_pct"] = 100.0 * sum(1 for i in n if gb[i] >= 0.9 * ftc[i]) / len(n)
    return o


# ---------------------------------------------------------------- mod log
TS_RE = re.compile(r"^\[(\d\d):(\d\d):(\d\d)\.(\d\d\d)\] (.*)$")


class ModLog:
    def __init__(self):
        self.lines = []      # (posix seconds, text, run)
        self.version = None

    def load(self, path, day, run):
        prev = None
        base = dt.datetime(day.year, day.month, day.day)
        for raw in open(path, encoding="utf-8", errors="replace"):
            m = TS_RE.match(raw.rstrip("\r\n"))
            if not m:
                continue
            h, mi, s, ms, text = m.groups()
            t = base + dt.timedelta(hours=int(h), minutes=int(mi), seconds=int(s), milliseconds=int(ms))
            if prev is not None and t < prev - dt.timedelta(hours=1):   # past midnight
                base += dt.timedelta(days=1)
                t += dt.timedelta(days=1)
            prev = t
            self.lines.append((t.timestamp(), text, run))
            if self.version is None:
                v = re.match(r"Nioh3EngineFix (\d+\.\d+\.\d+)", text)
                if v:
                    self.version = v.group(1)

    def grep(self, pat):
        r = re.compile(pat)
        return [(t, x, run) for t, x, run in self.lines if r.search(x)]


def num(pat, s, cast=float):
    m = re.search(pat, s)
    return cast(m.group(1)) if m else None


def parse_loads(log):
    loads = []
    for t, x, run in log.grep(r"^LoadStats #\d+: [\d.]+ s \|"):
        n = num(r"#(\d+)", x, int)
        sec = num(r"#\d+: ([\d.]+) s", x)
        loads.append({"n": n, "run": run, "end": t, "start": t - (sec or 0), "sec": sec, "frames": num(r"frames (\d+)", x, int),
                      "cpu_cores": num(r"CPU ([\d.]+) cores", x), "stage_ready": num(r"stage ready \+([\d.]+) s", x)})
    for t, x, run in log.grep(r"^LoadStats #\d+ io:"):
        n = num(r"#(\d+)", x, int)
        for L in loads:
            if L["n"] == n and L["run"] == run:
                L["read_mb"] = (num(r"read (\d+) MB archive", x) or 0) + (num(r"\+ (\d+) MB other", x) or 0)
                L["pso"] = num(r"PSO gfx (\d+) rt", x, int)
    for t, x, run in log.grep(r"^LoadGates #\d+ holder:"):
        n = num(r"#(\d+)", x, int)
        for L in loads:
            if L["n"] == n and L["run"] == run:
                L["holder"] = re.search(r"holder: (.*)$", x).group(1)
    for t, x, run in log.grep(r"^AutoContinue #\d+"):
        n = num(r"#(\d+)", x, int)
        for L in loads:
            if L["n"] == n and L["run"] == run:
                L["autocontinue"] = True
    return loads


def parse_boss(log):
    fights, cur = [], None
    for t, x, run in log.grep(r"^BossLog #\d+"):
        if "BossBattleMode start" in x:
            cur = {"n": num(r"#(\d+)", x, int), "run": run, "start": t, "end": None, "events": [], "flags": 0, "rewards": 0}
            fights.append(cur)
        elif "BossBattleModeEnd" in x and cur is not None:
            cur["end"] = t
            cur["dur"] = num(r"fight ([\d.]+) s", x)
        if cur is not None:
            if "SetMissionFlag" in x:
                cur["flags"] += 1
            if "REWARD" in x:
                cur["rewards"] += 1
            if any(k in x for k in ("ShowBossGauge", "SetMissionClearFlag", "BossBattleMode", "placement ids")):
                cur["events"].append((t, re.sub(r"^BossLog #\d+: ", "", x)))
    return fights


def parse_rematch(log):
    tags = {}
    for t, x, run in log.grep(r"^RematchLog"):
        m = re.match(r"RematchLog (L\d+|Q\d+)", x)
        k = m.group(1) if m else "other"
        tags[k] = tags.get(k, 0) + 1
    return tags


def parse_mem(log):
    rows = []
    for t, x, run in log.grep(r"^stats mem:"):
        rows.append({"t": t, "ws": num(r"working set (\d+) MB", x), "commit": num(r"commit (\d+) MB,", x), "private": num(r"private (\d+) MB", x),
                     "vram": num(r"VRAM local used/budget (\d+)/", x), "vram_budget": num(r"VRAM local used/budget \d+/(\d+)", x),
                     "nonlocal": num(r"non-local (\d+)/", x), "sys_avail": num(r"sys RAM avail (\d+) MB", x),
                     "handles": num(r"handles (\d+)", x), "threads": num(r"threads (\d+)", x)})
    return rows


def parse_latency(log):
    keys = {"K->C input age": "k_c", "C->L update+limiter": "c_l", "L->W": "l_w", "W->B waitable wait": "w_b", "B->P render": "b_p",
            "C->C logic frame": "logic_frame", "P->P present": "present_interval", "K->P sample->present": "k_p",
            "input->display B(n+1)-K(n)": "input_display"}
    rows = []
    for t, x, run in log.grep(r"^stats latency \(TS"):
        d = {"t": t}
        for k, v in keys.items():
            m = re.search(re.escape(k) + r" ([\d.]+)", x)
            if m:
                d[v] = float(m.group(1))
        rows.append(d)
    return rows


PL_TOT_RE = re.compile(
    r"^PadLatency total: changes (\d+) \(buttons (\d+), triggers (\d+), sticks (\d+)\), matched (\d+), unmatched (\d+) \| "
    r"input->present p50 (-?[\d.]+) p95 (-?[\d.]+) p99 (-?[\d.]+) ms \(buttons p50 (-?[\d.]+)\) \| "
    r"input->GPU-done p50 (-?[\d.]+) p95 (-?[\d.]+) p99 (-?[\d.]+) ms \(n (\d+), missing (\d+)\) \| "
    r"poll interval p50 (-?[\d.]+) p95 (-?[\d.]+) p99 (-?[\d.]+) ms \((\d+) rounds\) \| (.*)$")
PL_WIN_RE = re.compile(
    r"^PadLatency (\d+)s: changes (\d+) \(buttons (\d+), triggers (\d+), sticks (\d+)\), matched (\d+), unmatched (\d+) \| "
    r"input->present p50 (-?[\d.]+) p95 (-?[\d.]+) p99 (-?[\d.]+) ms \(buttons p50 (-?[\d.]+)\) \| "
    r"input->GPU-done p50 (-?[\d.]+) p95 (-?[\d.]+) p99 (-?[\d.]+) ms \(n (\d+), missing (\d+)\) \| "
    r"poll interval p50 (-?[\d.]+) p95 (-?[\d.]+) p99 (-?[\d.]+) ms \((\d+) rounds, ([\d.]+) reads/round\)$")


def parse_padlat(log):
    """PadLatency (ASI 1.7.35): window lines + the last 'total' line of each game launch. -1 in the log = no data -> None."""
    f = lambda x: None if float(x) < 0 else float(x)  # noqa: E731
    wins, tots, hooked = [], {}, []
    for t, x, run in log.grep(r"^PadLatency"):
        m = PL_WIN_RE.match(x)
        if m:
            g = m.groups()
            wins.append({"t": t, "run": run, "sec": int(g[0]), "changes": int(g[1]), "matched": int(g[5]), "unmatched": int(g[6]),
                         "in_present_p50": f(g[7]), "in_present_p95": f(g[8]), "in_present_p99": f(g[9]), "in_gpu_p50": f(g[11]),
                         "in_gpu_p99": f(g[13]), "poll_p50": f(g[16]), "poll_p99": f(g[18]), "reads_per_round": float(g[20])})
            continue
        m = PL_TOT_RE.match(x)
        if m:
            g = m.groups()
            tots[run] = {"t": t, "run": run, "changes": int(g[0]), "buttons": int(g[1]), "triggers": int(g[2]), "sticks": int(g[3]),
                         "matched": int(g[4]), "unmatched": int(g[5]), "in_present_p50": f(g[6]), "in_present_p95": f(g[7]),
                         "in_present_p99": f(g[8]), "buttons_p50": f(g[9]), "in_gpu_p50": f(g[10]), "in_gpu_p95": f(g[11]),
                         "in_gpu_p99": f(g[12]), "gpu_n": int(g[13]), "gpu_missing": int(g[14]), "poll_p50": f(g[15]),
                         "poll_p95": f(g[16]), "poll_p99": f(g[17]), "rounds": int(g[18]), "sources": g[19]}
            continue
        if x.startswith("PadLatency: hooked"):
            hooked.append(x)
    return {"windows": wins, "totals": [tots[k] for k in sorted(tots)], "hooked": hooked}


def parse_spr(log):
    rows = []
    for t, x, run in log.grep(r"^stats spr:"):
        rows.append({"t": t, "pso_total": num(r"PSO creates (\d+)", x, int), "pso_new": num(r"PSO creates \d+ \(\+(\d+)", x, int),
                     "pso_avg_us": num(r"PSO creates .*?avg ([\d.]+) us", x), "pso_max_us": num(r"PSO creates .*?max ([\d.]+) us", x),
                     "pso_ge1ms": num(r"PSO creates .*?>=1 ms: (\d+)", x, int),
                     "upload_total": num(r"fallbacks (\d+)", x, int), "upload_ge1ms": num(r"fallbacks .*?>=1 ms: (\d+)", x, int)})
    return rows


def parse_cpu(log):
    rows = []
    for t, x, run in log.grep(r"^stats cpu:"):
        rows.append({"t": t, "mq_calls_30s": num(r"MQ \w+, calls \d+ \(\+(\d+)", x, int), "pt1_lock_saved": num(r"lock pairs saved (\d+)", x, int),
                     "dl_skipped": num(r"lookups skipped (\d+)", x, int)})
    return rows


FN_RE = re.compile(r"^FnProbe ([0-9A-F]+) hits (\d+) \(\+(\d+) in (\d+) s, ([\d.]+)/s, peak (\d+)/s\) first (\S+) last (\S+) tid (\d+) \| "
                   r"callers \((\d+) distinct\+?, (\d+) sampled\):(.*)$")
ARG_RE = re.compile(r"^FnProbe ([0-9A-F]+) hit#(\d+) (\S+) tid (\d+) ret (\S+): (.*)$")


def parse_fnprobe(log):
    fns, args, never, header = {}, {}, set(), None
    for t, x, run in log.grep(r"^FnProbe"):
        m = FN_RE.match(x)
        if m:
            rva, hits, add, win, rate, peak, first, last, tid, distinct, sampled, callers = m.groups()
            cl = re.findall(r" (\S+) (\d+)%", callers)
            fns[rva] = {"hits": int(hits), "last_rate": float(rate), "peak": max(int(peak), fns.get(rva, {}).get("peak", 0)), "first": first, "last": last,
                        "tid": int(tid), "distinct": int(distinct), "sampled": int(sampled), "callers": cl, "run": run,
                        "max_rate": max(float(rate), fns.get(rva, {}).get("max_rate", 0.0))}
            continue
        m = ARG_RE.match(x)
        if m:
            args.setdefault(m.group(1), []).append(x)
            continue
        if x.startswith("FnProbe: never hit yet:"):
            never = set(x.split("yet:")[1].split())
        elif re.match(r"FnProbe: \d+/\d+ functions hooked", x):
            header = x
    for r in fns:
        never.discard(r)
    return {"fns": fns, "args": args, "never": sorted(never), "header": header}


def load_symbols():
    starts, names = [], {}
    if not SYMBOLS.exists():
        return starts, names
    for line in SYMBOLS.read_text(encoding="utf-8", errors="replace").splitlines():
        p = line.split("\t")
        if len(p) > 2 and p[0].startswith("0x") and p[1] == "function":
            a = int(p[0], 16)
            names[a] = p[2]
    starts = sorted(names)
    return starts, names


def sym(addr_txt, starts, names):
    try:
        a = int(addr_txt, 16)
    except ValueError:
        return addr_txt
    i = bisect.bisect_right(starts, a) - 1
    if i < 0:
        return "0x%X" % a
    f = starts[i]
    if a - f > 0x40000:
        return "0x%X" % a
    return "0x%X %s+0x%X" % (a, names[f], a - f)


# ---------------------------------------------------------------- sysmon
def load_sysmon(path):
    rows = []
    with open(path, newline="", encoding="utf-8-sig") as f:
        for r in csv.DictReader(f):
            try:
                t = dt.datetime.fromisoformat(r["time"]).timestamp()
            except (KeyError, ValueError):
                continue
            d = {"t": t}
            for k, v in r.items():
                if k != "time":
                    try:
                        d[k] = float(v)
                    except (TypeError, ValueError):
                        pass
            rows.append(d)
    return rows


def agg(rows, key, fn=None):
    v = [r[key] for r in rows if r.get(key) is not None and (fn is None or fn(r))]
    if not v:
        return None
    return {"avg": sum(v) / len(v), "max": max(v), "min": min(v), "n": len(v)}


def load_frames(d, S):
    """presentmon.csv of session dir d -> Frames (with the PresentMon --date_time clock-bias check)."""
    fr = Frames()
    pm = pathlib.Path(d) / "presentmon.csv"
    bias = float(S.get("pm_bias_s", 0.0))
    if pm.exists():
        fr.load(pm, bias)
        # PresentMon 2.6 --date_time has printed local time shifted by the UTC offset on this PC; if the shifted clock falls
        # outside the session but the raw clock fits, the raw clock is right (newer PresentMon builds)
        if fr.rows and S.get("start_ts") and bias:
            t0 = fr.t[0]
            if not (S["start_ts"] - 60 <= t0 <= S.get("stop_ts", t0 + 1) + 60) and (S["start_ts"] - 60 <= t0 + bias <= S.get("stop_ts", t0 + 1) + 60):
                fr = Frames()
                fr.load(pm, 0.0)
    return fr


# ---------------------------------------------------------------- report
def idx_between(fr, t0, t1):
    lo = bisect.bisect_left(fr.t, t0)
    hi = bisect.bisect_right(fr.t, t1)
    return range(lo, hi)


def hm(t):
    return dt.datetime.fromtimestamp(t).strftime("%H:%M:%S")


def summarize(d):
    d = pathlib.Path(d)
    S = json.loads((d / "session.json").read_text(encoding="utf-8-sig")) if (d / "session.json").exists() else {}
    out, J, missing = [], {"session": S}, []
    day = dt.datetime.fromisoformat(S["start"]) if S.get("start") else dt.datetime.now()
    out.append("# Nioh 3 play session %s" % d.name)
    out.append("")
    stock = S.get("mode") == "stock"
    if stock:
        out.append("**STOCK session (unmodded game).** Started %s, stopped %s. Our mod, the ASI loader%s were moved out of the game folder for the run and put back by STOP. "
                   "Graphics: %s." % (S.get("start", "?"), S.get("stop", "?"), "" if S.get("keep_optiscaler") else " and OptiScaler",
                                      S.get("graphics", "see config.xml")))
        out.append("")
        out.append("There is NO mod log in a stock session (nothing to write one), so this report is PresentMon + Windows counters only: no loads, boss fights, "
                   "PadLatency, memory or PSO sections. Compare with a mod session: `summarize.py compare <mod dir> <this dir>`.")
        out.append("Stashed (%d): %s." % (len(S.get("stashed") or []), ", ".join(S.get("stashed") or []) or "none"))
    else:
        out.append("Started %s, stopped %s. ASI %s (game folder had %s before; restored after). Graphics: %s." % (
            S.get("start", "?"), S.get("stop", "?"), S.get("asi_version", "?"), S.get("asi_version_before", "?"), S.get("graphics", "see config.xml")))
    fgm = re.search(r"FRAME_GENERATION=(\w+)", S.get("graphics") or "")
    if fgm and fgm.group(1).upper() != "OFF":
        out.append("Frame generation was %s: PresentMon counts generated frames too, so fps here is presented fps (about 2x the rendered rate)." % fgm.group(1))
    if S.get("capture_keys"):
        out.append("Capture keys added to the play ini: %s." % ", ".join(S["capture_keys"]))
    out.append("")

    # mod logs
    log = ModLog()
    logs = sorted(d.glob("modlog_run*.log"))
    for k, p in enumerate(logs, 1):
        log.load(p, day, k)
    if not logs and not stock:
        missing.append("mod log (modlog_run*.log)")

    # frames
    fr = load_frames(d, S)
    if not fr.rows:
        missing.append("PresentMon frames (presentmon.csv empty or missing)")
    J["pm_columns"] = sorted(fr.present)

    loads = parse_loads(log)
    fights = parse_boss(log)
    in_load = []
    for L in loads:
        in_load.append((L["start"], L["end"]))

    def not_loading(i):
        t = fr.t[i]
        return not any(a <= t <= b for a, b in in_load)

    out.append("## Frames (PresentMon)")
    if fr.rows:
        all_s = frame_stats(fr)
        play_idx = [i for i in range(fr.rows) if not_loading(i)] if in_load else None
        play_s = frame_stats(fr, play_idx) if play_idx is not None else None
        J["frames_all"], J["frames_play"] = all_s, play_s
        cols = [("whole session", all_s)] + ([("gameplay (load windows removed)", play_s)] if play_s else [])
        for f in fights:
            if f.get("end"):
                s = frame_stats(fr, idx_between(fr, f["start"], f["end"]))
                f["frames"] = s
                if s:
                    cols.append(("boss fight #%d" % f["n"], s))
        rows = [("frames", "frames", 0), ("captured seconds", "seconds", 0), ("avg fps", "fps_avg", 1), ("1 % low fps (1000/p99)", "low1_fps", 1),
                ("0.1 % low fps (1000/p99.9)", "low01_fps", 1), ("1 % low fps (avg of worst 1 %)", "low1_avg_fps", 1),
                ("0.1 % low fps (avg of worst 0.1 %)", "low01_avg_fps", 1), ("frame time p50 ms", "ft_p50", 2), ("p95 ms", "ft_p95", 2),
                ("p99 ms", "ft_p99", 2), ("p99.9 ms", "ft_p999", 2), ("max ms", "ft_max", 1), ("spikes (>2x local median, >+8 ms)", "spikes", 0),
                ("spikes per minute", "spikes_per_min", 2), ("frames > 33 ms", "over_33.4", 0), ("frames > 50 ms", "over_50", 0),
                ("frames > 100 ms", "over_100", 0), ("pacing: frame-time stdev ms", "ft_stdev", 2), ("pacing: mean |delta| ms", "ft_mean_abs_delta", 2),
                ("pacing: p99 |delta| ms", "ft_delta_p99", 2), ("animation error |avg| ms", "anim_err_abs_avg", 2), ("displayed time avg ms", "displayed_avg", 2),
                ("CPU busy avg ms", "cpu_busy_avg", 2), ("CPU wait avg ms", "cpu_wait_avg", 2), ("GPU busy avg ms", "gpu_busy_avg", 2),
                ("GPU wait avg ms", "gpu_wait_avg", 2), ("GPU latency avg ms", "gpu_latency_avg", 2), ("GPU-bound frames % (GPU busy >= 90 % of frame)", "gpu_bound_pct", 1),
                ("display latency p50 ms", "display_latency_p50", 1), ("present -> display p50 ms (DisplayLatency - CPUBusy)", "present_display_p50", 1),
                ("present -> display p95 ms", "present_display_p95", 1), ("all-input-to-photon p50 ms", "aip_latency_p50", 1),
                ("all-input-to-photon p95 ms", "aip_latency_p95", 1), ("input samples", "aip_latency_n", 0), ("click-to-photon p50 ms", "click_latency_p50", 1),
                ("click samples", "click_latency_n", 0), ("PC latency p50 ms", "pc_latency_p50", 1)]
        out.append("")
        out.append("| metric | " + " | ".join(c for c, _ in cols) + " |")
        out.append("|---|" + "---|" * len(cols))
        for label, key, nd in rows:
            if all(s.get(key) is None for _, s in cols):
                continue
            out.append("| %s | %s |" % (label, " | ".join(fnum(s.get(key), nd) if nd else (str(int(s[key])) if s.get(key) is not None else "-") for _, s in cols)))
        pids = sorted(set(fr.pid))
        out.append("")
        out.append("Launches seen by PresentMon: %d. Columns present: %s." % (len(pids), ", ".join(sorted(fr.present))))
        if "AllInputToPhotonLatency" in fr.present and not all_s.get("aip_latency_n"):
            out.append("Input latency columns are present but empty: PresentMon saw no input events tied to frames (needs real mouse/keyboard; a pad may not count).")
    out.append("")

    # controller latency (PadLatency, ASI 1.7.35) next to PresentMon's display latency
    real_out = out
    if stock:
        out = []
    out.append("## Controller latency (PadLatency)")
    pl = parse_padlat(log)
    J["padlat"] = pl
    if not pl["totals"] and not pl["windows"]:
        out.append("No PadLatency lines in the log (needs ASI >= 1.7.35 with [Debug] PadLatency = 1, and pad input during the session).")
    else:
        pmf = (J.get("frames_play") or J.get("frames_all") or {}) if fr.rows else {}
        out.append("")
        out.append("| run | pad changes (buttons / triggers / sticks) | matched / unmatched | input->present p50 / p95 / p99 ms | buttons p50 | input->GPU-done p50 / p95 / p99 ms | game pad poll interval p50 / p95 / p99 ms | sources |")
        out.append("|---|---|---|---|---|---|---|---|")
        for T in pl["totals"]:
            out.append("| %d | %d (%d / %d / %d) | %d / %d | %s / %s / %s | %s | %s / %s / %s | %s / %s / %s | %s |" % (
                T["run"], T["changes"], T["buttons"], T["triggers"], T["sticks"], T["matched"], T["unmatched"],
                fnum(T["in_present_p50"], 2), fnum(T["in_present_p95"], 2), fnum(T["in_present_p99"], 2), fnum(T["buttons_p50"], 2),
                fnum(T["in_gpu_p50"], 2), fnum(T["in_gpu_p95"], 2), fnum(T["in_gpu_p99"], 2),
                fnum(T["poll_p50"], 2), fnum(T["poll_p95"], 2), fnum(T["poll_p99"], 2), T["sources"]))
        if pl["windows"]:
            w = pl["windows"]
            p50s = [x["in_present_p50"] for x in w if x["in_present_p50"] is not None]
            p99s = [x["in_present_p99"] for x in w if x["in_present_p99"] is not None]
            out.append("")
            out.append("%d report windows with pad input: median of the window input->present p50s %s ms, worst window p99 %s ms; %d changes in total." % (
                len(p50s), fnum(statistics.median(p50s) if p50s else None, 2), fnum(max(p99s) if p99s else None, 2), sum(x["changes"] for x in w)))
        T = max(pl["totals"], key=lambda r: r["matched"]) if pl["totals"] else None
        pd = pmf.get("present_display_p50")
        out.append("")
        out.append("PresentMon on the same session: display latency p50 %s ms (frame CPU start -> on screen), present -> display p50 %s / p95 %s ms." % (
            fnum(pmf.get("display_latency_p50"), 1), fnum(pd, 1), fnum(pmf.get("present_display_p95"), 1)))
        if T and T["in_present_p50"] is not None and T["poll_p50"] is not None and pd is not None:
            est = T["poll_p50"] / 2 + T["in_present_p50"] + pd
            J["pad_to_display_est_ms"] = est
            out.append("Estimated pad press -> on screen (median): poll interval / 2 %.1f + input->present %.1f + present->display %.1f = **%.1f ms** "
                       "(+ the pad / USB / Steam Input hop and scanout to the screen middle, not visible to software)." % (
                           T["poll_p50"] / 2, T["in_present_p50"], pd, est))
            if pmf.get("aip_latency_p50") is not None:
                out.append("For comparison, PresentMon's mouse/keyboard all-input-to-photon p50: %s ms." % fnum(pmf.get("aip_latency_p50"), 1))
        if T and T["matched"] < 0.8 * max(1, T["changes"]):
            out.append("Note: under 80 %% of the pad changes were matched to a frame (unmatched %d): loads/menus without logic frames, or the TS probes / Present hook were off part of the time." % T["unmatched"])
    out.append("")

    out = real_out
    # sysmon
    out.append("## System (Windows counters every 5 s, monitor.ps1)")
    sm = load_sysmon(d / "sysmon.csv") if (d / "sysmon.csv").exists() else []
    if not sm:
        missing.append("sysmon.csv")
    else:
        game = lambda r: r.get("game_running", 0) >= 1  # noqa: E731
        J["system"] = {}
        out.append("")
        out.append("| counter (while the game ran) | avg | max |")
        out.append("|---|---|---|")
        for key, label in (("cpu_total_pct", "CPU total %"), ("cpu_perf_pct", "CPU performance % (clock vs base)"), ("game_cpu_pct", "Nioh3 CPU % of one core x cores"),
                           ("gpu3d_total_pct", "GPU 3D engine % (all processes)"), ("gpu3d_game_pct", "GPU 3D engine % (Nioh3)"),
                           ("vram_game_mb", "VRAM Nioh3 dedicated MB"), ("vram_adapter_mb", "VRAM adapter dedicated MB"), ("game_ws_mb", "Nioh3 working set MB"),
                           ("game_private_mb", "Nioh3 private bytes MB"), ("ram_avail_mb", "RAM available MB"), ("commit_mb", "system commit MB"),
                           ("gpu_temp_c", "GPU temperature C"), ("thermal_c", "ACPI thermal zone C")):
            a = agg(sm, key, game)
            if a:
                J["system"][key] = a
                out.append("| %s | %s | %s |" % (label, fnum(a["avg"]), fnum(a["max"])))
        out.append("")
        out.append("GPU clock / temperature / board power: not available from Windows counters or PresentMon's console build on this PC (AMD telemetry needs the driver SDK); add an HWiNFO CSV by hand if needed.")
    out.append("")

    if stock:
        out = []
    # loads
    out.append("## Loads (LoadStats / LoadGates)")
    J["loads"] = loads
    if loads:
        out.append("")
        out.append("| # | run | ended | seconds | frames | CPU cores | stage ready +s | MB read | PSOs | gate holder | auto-continue |")
        out.append("|---|---|---|---|---|---|---|---|---|---|---|")
        for L in loads:
            out.append("| %s | %s | %s | %s | %s | %s | %s | %s | %s | %s | %s |" % (L["n"], L["run"], hm(L["end"]), fnum(L["sec"]), L.get("frames", "-"),
                       fnum(L.get("cpu_cores"), 2), fnum(L.get("stage_ready")), fnum(L.get("read_mb"), 0), L.get("pso", "-"), L.get("holder", "-"),
                       "yes" if L.get("autocontinue") else "no"))
        secs = [L["sec"] for L in loads if L.get("sec")]
        if secs:
            out.append("")
            out.append("Loads: %d, median %.1f s, longest %.1f s." % (len(secs), statistics.median(secs), max(secs)))
    else:
        out.append("No LoadStats summaries in the log.")
    out.append("")

    # boss
    out.append("## Boss fights (BossLog / RematchLog)")
    J["fights"] = [{k: v for k, v in f.items() if k != "events"} for f in fights]
    if fights:
        for f in fights:
            s = f.get("frames") or {}
            out.append("")
            out.append("**Fight #%d** (run %d): %s -> %s, %s s; mission flags set %d, rewards %d; avg %s fps, 1 %% low %s, 0.1 %% low %s, spikes %s." % (
                f["n"], f["run"], hm(f["start"]), hm(f["end"]) if f.get("end") else "no end (quit/died?)", fnum(f.get("dur")), f["flags"], f["rewards"],
                fnum(s.get("fps_avg")), fnum(s.get("low1_fps")), fnum(s.get("low01_fps")), s.get("spikes", "-")))
            for t, e in f["events"][:12]:
                out.append("- %s %s" % (hm(t), e[:200]))
    else:
        out.append("No BossBattleMode start in the log.")
    rm = parse_rematch(log)
    J["rematch_tags"] = rm
    if rm:
        out.append("")
        out.append("RematchLog lines by tag: " + ", ".join("%s %d" % kv for kv in sorted(rm.items())))
    out.append("")

    # mod counters
    out.append("## Mod counters (30 s lines)")
    mem = parse_mem(log)
    lat = parse_latency(log)
    spr = parse_spr(log)
    cpu = parse_cpu(log)
    J["mem"], J["latency_ts"], J["spr"], J["cpu"] = mem, lat, spr, cpu
    if mem:
        mx = lambda k: max((r[k] for r in mem if r.get(k) is not None), default=None)  # noqa: E731
        out.append("- Memory (MemStats, %d lines): working set max %s MB, commit max %s MB, VRAM local max %s of %s MB budget, non-local max %s MB, sys RAM avail min %s MB, threads max %s." % (
            len(mem), fnum(mx("ws"), 0), fnum(mx("commit"), 0), fnum(mx("vram"), 0), fnum(mx("vram_budget"), 0), fnum(mx("nonlocal"), 0),
            fnum(min((r["sys_avail"] for r in mem if r.get("sys_avail") is not None), default=None), 0), fnum(mx("threads"), 0)))
    if lat:
        med = lambda k: statistics.median([r[k] for r in lat if k in r]) if any(k in r for r in lat) else None  # noqa: E731
        out.append("- Thread timing / latency (TS probes, median of %d lines, ms): pad sample age K->C %s, update+limiter C->L %s, logic->waitable L->W %s, waitable wait W->B %s, render B->P %s, logic frame %s, present interval %s, sample->present K->P %s, input->display %s." % (
            len(lat), fnum(med("k_c"), 2), fnum(med("c_l"), 2), fnum(med("l_w"), 2), fnum(med("w_b"), 2), fnum(med("b_p"), 2), fnum(med("logic_frame"), 2),
            fnum(med("present_interval"), 2), fnum(med("k_p"), 2), fnum(med("input_display"), 2)))
    if spr:
        last = spr[-1]
        out.append("- PSO / stutter (SPR): %s runtime graphics PSO creates (%s took >= 1 ms, worst %s us); upload-heap fallbacks %s (%s >= 1 ms)." % (
            last.get("pso_total"), last.get("pso_ge1ms"), fnum(max((r.get("pso_max_us") or 0) for r in spr), 0),
            last.get("upload_total"), last.get("upload_ge1ms")))
    if cpu:
        out.append("- CpuStats: MQ calls per 30 s median %s; PT1 lock pairs saved %s; DL lookups skipped %s." % (
            fnum(statistics.median([r["mq_calls_30s"] for r in cpu if r.get("mq_calls_30s") is not None]) if any(r.get("mq_calls_30s") is not None for r in cpu) else None, 0),
            cpu[-1].get("pt1_lock_saved"), cpu[-1].get("dl_skipped")))
    if not (mem or lat or spr or cpu):
        out.append("No 30 s stats lines in the log.")
    out.append("")

    # fnprobe
    out.append("## FnProbe (remaining-work 10.1)")
    fp = parse_fnprobe(log)
    J["fnprobe"] = {"fns": fp["fns"], "never": fp["never"], "header": fp["header"]}
    if fp["header"]:
        out.append(fp["header"])
    if fp["fns"]:
        starts, names = load_symbols()
        out.append("")
        out.append("| function | hits | max /s (window) | peak /s | first | last | first tid | callers (sampled share) |")
        out.append("|---|---|---|---|---|---|---|---|")
        for rva, f in sorted(fp["fns"].items(), key=lambda kv: -kv[1]["hits"]):
            nm = sym(rva, starts, names)
            cl = "; ".join("%s %s%%" % (sym(a, starts, names) if re.fullmatch(r"[0-9A-F]+", a) else a, s) for a, s in f["callers"])
            out.append("| %s | %d | %.1f | %d | %s | %s | %d | %s |" % (nm, f["hits"], f["max_rate"], f["peak"], f["first"], f["last"], f["tid"], cl))
        out.append("")
        out.append("First hits with arguments (hash? = 32-bit values that look like name hashes):")
        for rva in sorted(fp["args"]):
            for x in fp["args"][rva][:3]:
                out.append("- `%s`" % x[:400])
    if fp["never"]:
        out.append("")
        out.append("Never hit: " + " ".join(fp["never"]))
    if not fp["fns"] and not fp["header"]:
        out.append("No FnProbe lines in the log.")
    out.append("")

    # side checks for open items (remaining-work 4.3 AutoContinueLoad routes, 6.5 VRR online detection, crashes)
    out.append("## Side checks (remaining-work 4.3 / 6.5, exits)")
    vrr = log.grep(r"VRR:.*(online|offline)")
    ac = log.grep(r"^AutoContinue #")
    J["vrr_online_lines"] = [x for _, x, _ in vrr]
    out.append("- 6.5 VRR online detection: %s" % ("%d line(s): " % len(vrr) + "; ".join(x[:120] for _, x, _ in vrr[:5]) if vrr else "no online/offline switch logged (solo session or VRROnline off)"))
    out.append("- 4.3 AutoContinueLoad: %d tip page(s) answered by the mod (0 is expected while the option is off)." % len(ac))
    ev = d / "events.txt"
    if ev.exists():
        ex = [l.strip() for l in ev.read_text(encoding="utf-8-sig", errors="replace").splitlines() if "game exit" in l]
        J["exits"] = ex
        for l in ex:
            out.append("- " + l)
    out.append("")

    out = real_out
    if missing:
        out.append("## Missing inputs")
        for m in missing:
            out.append("- " + m)
    J["missing"] = missing
    (d / "report.md").write_text("\n".join(out) + "\n", encoding="utf-8")
    (d / "summary.json").write_text(json.dumps(J, indent=1, default=str), encoding="utf-8")
    return out, J


# ---------------------------------------------------------------- compare (mod session vs STOCK session)
BIG_FRAME_MS = 250.0     # a frame this long = "loading-like" stall
EXCL_PAD_S = 2.0         # seconds around a stall cluster that are not "sustained gameplay"
SEG_MIN_S = 30.0         # a sustained segment is at least this long


def phases(fr):
    """Split one capture, using PresentMon only (the stock run has no mod log), into comparable phases.
    Returns dict: cum (end time of each frame, s since capture start), events (loading-like stall clusters), gameplay idx list,
    segments (list of (start_i, end_i_exclusive, seconds), >= SEG_MIN_S, longest first)."""
    ft = fr.cols["FrameTime"]
    n = fr.rows
    cum, s = [], 0.0
    for x in ft:
        s += x / 1000.0
        cum.append(s)
    events = []     # [start_s, end_s, count, worst_ms]
    for i in range(n):
        if ft[i] > BIG_FRAME_MS:
            st = cum[i] - ft[i] / 1000.0
            if events and st - events[-1][1] <= 4.0:
                events[-1][1] = cum[i]
                events[-1][2] += 1
                events[-1][3] = max(events[-1][3], ft[i])
            else:
                events.append([st, cum[i], 1, ft[i]])
    gameplay, k = [], 0
    for i in range(n):
        a, b = cum[i] - ft[i] / 1000.0, cum[i]
        while k < len(events) and events[k][1] + EXCL_PAD_S < a:
            k += 1
        if k < len(events) and events[k][0] - EXCL_PAD_S <= b:
            continue
        gameplay.append(i)
    segs, run = [], []
    for i in gameplay:
        if run and i != run[-1] + 1:
            segs.append(run)
            run = []
        run.append(i)
    if run:
        segs.append(run)
    seglist = []
    for r in segs:
        secs = (cum[r[-1]] - (cum[r[0]] - ft[r[0]] / 1000.0))
        if secs >= SEG_MIN_S:
            seglist.append((r[0], r[-1] + 1, secs))
    seglist.sort(key=lambda x: -x[2])
    return {"cum": cum, "events": events, "gameplay": gameplay, "segments": seglist}


def fg_split(fr, idx=None):
    """Rendered vs generated vs displayed over frames idx. Needs PresentMon FrameType (v2); Displayed is optional."""
    ii = range(fr.rows) if idx is None else idx
    ft = fr.cols["FrameTime"]
    tot = sum(ft[i] for i in ii) / 1000.0
    if tot <= 0:
        return None
    o = {"seconds": tot}
    if fr.has_ftype:
        nr = sum(1 for i in ii if fr.ftype[i] == 0)
        ng = sum(1 for i in ii if fr.ftype[i] == 1)
        o.update(rendered_fps=nr / tot, generated_share=100.0 * ng / max(1, len(ii)), fg_on=ng > 0)
    nd = [i for i in ii if fr.shown[i] >= 0]
    if nd:
        o["displayed_fps"] = sum(1 for i in nd if fr.shown[i] == 1) / tot
        o["dropped_pct"] = 100.0 * sum(1 for i in nd if fr.shown[i] == 0) / len(nd)
    return o


def compare_session(d):
    d = pathlib.Path(d)
    S = json.loads((d / "session.json").read_text(encoding="utf-8-sig")) if (d / "session.json").exists() else {}
    fr = load_frames(d, S)
    sm = load_sysmon(d / "sysmon.csv") if (d / "sysmon.csv").exists() else []
    R = {"dir": d, "name": d.name, "S": S, "fr": fr, "mode": S.get("mode", "mod"), "sm": sm}
    if not fr.rows:
        return R
    P = phases(fr)
    R["P"] = P
    R["all"] = frame_stats(fr)
    R["all_fg"] = fg_split(fr)
    R["gameplay"] = frame_stats(fr, P["gameplay"]) if P["gameplay"] else None
    R["gameplay_fg"] = fg_split(fr, P["gameplay"]) if P["gameplay"] else None
    # stall list (the "loading-like spikes"): clusters of frames > BIG_FRAME_MS
    ev = P["events"]
    ft = fr.cols["FrameTime"]
    R["stalls"] = {"clusters": len(ev), "frames_gt250": sum(1 for x in ft if x > BIG_FRAME_MS), "frames_gt1000": sum(1 for x in ft if x > 1000.0),
                   "stall_seconds": sum(e[1] - e[0] for e in ev), "worst_ms": max((e[3] for e in ev), default=None),
                   "per_hour": len(ev) / max(1e-9, P["cum"][-1] / 3600.0)}
    R["segments"] = P["segments"]
    sysrows = [r for r in sm if r.get("game_running", 0) >= 1]
    R["sys"] = {k: (agg(sysrows, k) or {}).get("avg") for k in ("cpu_total_pct", "gpu3d_total_pct", "gpu3d_game_pct", "vram_game_mb", "game_ws_mb")}
    padest = None
    sj = d / "summary.json"
    if sj.exists():
        try:
            padest = json.loads(sj.read_text(encoding="utf-8-sig", errors="replace")).get("pad_to_display_est_ms")
        except ValueError:
            pass
    R["pad_est"] = padest
    return R


def trim_to_seconds(fr, P, seg, secs):
    """Frame indices of segment `seg` (start_i, end_i, s) covering its first `secs` seconds."""
    a, b, _ = seg
    ft = fr.cols["FrameTime"]
    acc, out = 0.0, []
    for i in range(a, b):
        if acc >= secs * 1000.0:
            break
        out.append(i)
        acc += ft[i]
    return out


def compare(mod_dir, stock_dir):
    M, K = compare_session(mod_dir), compare_session(stock_dir)
    out = []
    out.append("# Nioh 3: mod vs stock")
    out.append("")
    out.append("- mod   : `%s` (mode=%s, ASI %s, graphics: %s)" % (M["name"], M["mode"], M["S"].get("asi_version", "?"), M["S"].get("graphics", "?")))
    out.append("- stock : `%s` (mode=%s, OptiScaler %s, graphics: %s)" % (K["name"], K["mode"], "kept" if K["S"].get("keep_optiscaler") else "removed", K["S"].get("graphics", "?")))
    warn = []
    if M["mode"] == "stock":
        warn.append("the first argument is itself a stock session (expected the mod session first)")
    if K["mode"] != "stock":
        warn.append("the second argument is not a stock session (session.json mode != stock)")
    if M["S"].get("graphics") != K["S"].get("graphics"):
        warn.append("the in-game graphics settings differ between the two sessions (see the two lines above) - not a like-for-like comparison")
    if not M["fr"].rows or not K["fr"].rows:
        warn.append("a session has no PresentMon frames: %s" % ", ".join(x["name"] for x in (M, K) if not x["fr"].rows))
    for w in warn:
        out.append("- **WARNING**: " + w)
    out.append("")
    J = {"mod": M["name"], "stock": K["name"], "warnings": warn}
    if M["fr"].rows and K["fr"].rows:
        # ---- phases
        cols = [("whole session", M["all"], K["all"], M["all_fg"], K["all_fg"])]
        if M["gameplay"] and K["gameplay"]:
            cols.append(("gameplay (stalls > %d ms and +-%d s around them removed)" % (BIG_FRAME_MS, EXCL_PAD_S), M["gameplay"], K["gameplay"], M["gameplay_fg"], K["gameplay_fg"]))
        if M["segments"] and K["segments"]:
            ms, ks = M["segments"][0], K["segments"][0]
            T = min(ms[2], ks[2])
            mi, ki = trim_to_seconds(M["fr"], M["P"], ms, T), trim_to_seconds(K["fr"], K["P"], ks, T)
            sm_, sk_ = frame_stats(M["fr"], mi), frame_stats(K["fr"], ki)
            if sm_ and sk_:
                cols.append(("longest sustained segment, both trimmed to the first %.0f s (mod's is %.0f s, stock's %.0f s)" % (T, ms[2], ks[2]),
                             sm_, sk_, fg_split(M["fr"], mi), fg_split(K["fr"], ki)))
        else:
            out.append("No sustained segment of at least %d s without a stall in %s." % (SEG_MIN_S, "the mod session" if not M["segments"] else "the stock session"))
            out.append("")
        # FG on/off: split each whole session by PresentMon FrameType (rendered vs generated) - reported as rows below
        rows = [("avg fps (presented)", "fps_avg", 1), ("1 % low fps (1000/p99)", "low1_fps", 1), ("0.1 % low fps (1000/p99.9)", "low01_fps", 1),
                ("1 % low fps (avg of worst 1 %)", "low1_avg_fps", 1), ("frame time p50 ms", "ft_p50", 2), ("frame time p99 ms", "ft_p99", 2),
                ("frame time max ms", "ft_max", 1), ("spikes (>2x local median, >+8 ms)", "spikes", 0), ("spikes per minute", "spikes_per_min", 2),
                ("frames > 33 ms", "over_33.4", 0), ("frames > 100 ms", "over_100", 0), ("pacing: frame-time stdev ms", "ft_stdev", 2),
                ("CPU busy avg ms", "cpu_busy_avg", 2), ("GPU busy avg ms", "gpu_busy_avg", 2), ("GPU-bound frames %", "gpu_bound_pct", 1),
                ("display latency p50 ms", "display_latency_p50", 1), ("display latency p95 ms", "display_latency_p95", 1),
                ("present -> display p50 ms", "present_display_p50", 1), ("input-to-photon p50 ms (mouse/keyboard only)", "aip_latency_p50", 1),
                ("input-to-photon samples", "aip_latency_n", 0)]
        fgrows = [("rendered fps (FrameType = Application)", "rendered_fps", 1), ("generated frames share %", "generated_share", 1),
                  ("displayed fps (Displayed = 1)", "displayed_fps", 1), ("dropped (not displayed) %", "dropped_pct", 2)]

        def cell(v, nd):
            if v is None or (isinstance(v, float) and not math.isfinite(v)):
                return "-"
            return ("%.*f" % (nd, v)) if nd else str(int(round(v)))

        def delta(a, b, nd):
            if a is None or b is None:
                return "-"
            d_ = b - a
            pc = (" (%+.0f %%)" % (100.0 * d_ / a)) if a else ""
            return ("%+.*f" % (nd, d_)) + pc

        J["phases"] = {}
        for title, ma, sa, mf, sf in cols:
            out.append("## %s" % title)
            out.append("")
            out.append("| metric | mod | stock | stock - mod |")
            out.append("|---|---|---|---|")
            J["phases"][title] = {}
            for label, key, nd in rows:
                a, b = ma.get(key), sa.get(key)
                if a is None and b is None:
                    continue
                out.append("| %s | %s | %s | %s |" % (label, cell(a, nd), cell(b, nd), delta(a, b, nd)))
                J["phases"][title][key] = [a, b]
            mf, sf = mf or {}, sf or {}
            for label, key, nd in fgrows:
                a, b = mf.get(key), sf.get(key)
                if a is None and b is None:
                    continue
                out.append("| %s | %s | %s | %s |" % (label, cell(a, nd), cell(b, nd), delta(a, b, nd)))
            if title == "whole session" or title.startswith("gameplay"):
                out.append("| frame generation (PresentMon FrameType) | %s | %s | |" % tuple(
                    ("-" if x.get("fg_on") is None else ("ON" if x["fg_on"] else "off")) for x in (mf, sf)))
            out.append("")
        # ---- stalls
        ms_, ks_ = M["stalls"], K["stalls"]
        out.append("## Loading-like stalls (frames > %d ms, clustered within 4 s)" % BIG_FRAME_MS)
        out.append("")
        out.append("| metric | mod | stock |")
        out.append("|---|---|---|")
        for label, key, nd in (("stall clusters", "clusters", 0), ("frames > 250 ms", "frames_gt250", 0), ("frames > 1 s", "frames_gt1000", 0),
                               ("seconds inside stall clusters", "stall_seconds", 1), ("worst single frame ms", "worst_ms", 0), ("clusters per hour", "per_hour", 1)):
            out.append("| %s | %s | %s |" % (label, cell(ms_[key], nd), cell(ks_[key], nd)))
        J["stalls"] = {"mod": ms_, "stock": ks_}
        out.append("")
        out.append("Sustained segments (>= %d s without a stall), longest first - mod: %s; stock: %s." % (
            SEG_MIN_S, ", ".join("%.0f s" % s_[2] for s_ in M["segments"][:6]) or "none", ", ".join("%.0f s" % s_[2] for s_ in K["segments"][:6]) or "none"))
        out.append("")
        # ---- system + pad
        out.append("## System counters while the game ran (monitor.ps1 every 5 s)")
        out.append("")
        out.append("| counter | mod | stock |")
        out.append("|---|---|---|")
        for label, key in (("CPU total %", "cpu_total_pct"), ("GPU 3D engine % (all)", "gpu3d_total_pct"), ("GPU 3D engine % (Nioh3)", "gpu3d_game_pct"),
                           ("VRAM Nioh3 MB", "vram_game_mb"), ("Nioh3 working set MB", "game_ws_mb")):
            if M["sys"].get(key) is None and K["sys"].get(key) is None:
                continue
            out.append("| %s | %s | %s |" % (label, cell(M["sys"].get(key), 1), cell(K["sys"].get(key), 1)))
        out.append("")
        out.append("Pad press -> on screen estimate (mod PadLatency only; the stock game has no PadLatency hook): mod %s ms, stock -." % cell(M["pad_est"], 1))
        out.append("")
        gfg = [x for x in (M["all_fg"], K["all_fg"]) if x and x.get("fg_on") is not None]
        if len(gfg) == 2 and gfg[0]["fg_on"] != gfg[1]["fg_on"]:
            out.append("**Frame generation differs** (mod %s, stock %s): presented fps is not comparable; use the 'rendered fps' row." % (
                "ON" if gfg[0]["fg_on"] else "off", "ON" if gfg[1]["fg_on"] else "off"))
        elif not M["fr"].has_ftype or not K["fr"].has_ftype:
            out.append("Note: FrameType column missing in a capture, so rendered-vs-generated could not be split; with FG on, fps is presented (generated included) fps.")
        out.append("")
    out.append("## Caveats (read before trusting a delta)")
    out.append("")
    for c in ("These are two separate play-throughs, not a replay: different route, different fights, different camera, different enemy aggro. "
              "Only the gameplay / sustained-segment columns reduce that, they do not remove it. Treat differences under a few percent as noise.",
              "Phases come from PresentMon alone (frames > %d ms = stall) so the same rule applies to both runs; the mod session's own LoadStats windows are not used here. "
              "A real load that still presents at normal speed is not seen as a stall, and a hitch in play counts as one." % BIG_FRAME_MS,
              "The stock run has no mod log: no PadLatency, boss timeline, memory or PSO numbers. Input-to-photon is PresentMon's mouse/keyboard path only "
              "(empty with a controller).",
              "Shader/PSO caches: the first run after changing files can stutter more (cold shader cache, OptiScaler/driver caches). Run the mission once to warm up if the numbers look odd.",
              "Stock runs without OptiScaler also change the upscaler/FG path (XeSS inputs -> FSR4); use `-KeepOptiScaler` to isolate our ASI only.",
              "With frame generation on, PresentMon fps includes generated frames; the 'rendered fps' row is the real render rate."):
        out.append("- " + c)
    out.append("")
    txt = "\n".join(out) + "\n"
    dest = pathlib.Path(stock_dir).parent
    name = "compare_%s__vs__%s" % (M["name"], K["name"])
    try:
        (dest / (name + ".md")).write_text(txt, encoding="utf-8")
        (dest / (name + ".json")).write_text(json.dumps(J, indent=1, default=str), encoding="utf-8")
    except OSError:
        pass
    return txt, J, dest / (name + ".md")


# ---------------------------------------------------------------- selftest (no game)
def c_printf(fmt, *args):
    """Render a C printf format with Python % (drops ll/l/z length modifiers)."""
    return re.sub(r"%(-?\d*(?:\.\d+)?)(?:ll|l|z|I64)?([duxXsfgcp%])", lambda m: "%" + m.group(1) + m.group(2).replace("u", "d").replace("p", "s"), fmt) % args


def selftest():
    import random
    import shutil
    import tempfile
    root = REPO / "mod" / "Nioh3EngineFix" / "src"
    fsrc = (root / "fnprobe.cpp").read_text(encoding="utf-8")
    bsrc = (root / "bosslog.cpp").read_text(encoding="utf-8")
    dsrc = (root / "dllmain.cpp").read_text(encoding="utf-8")
    psrc = (root / "padlat.cpp").read_text(encoding="utf-8")

    def fmt_of(src, start):
        i = src.index(start)
        j = src.index('",', i)
        # join adjacent string literals
        seg = src[i:j + 1]
        k = j + 1
        while True:
            m = re.match(r'\s*"((?:[^"\\]|\\.)*)"', src[k:])
            if not m:
                break
            seg += m.group(0)
            k += m.end()
        parts = re.findall(r'"((?:[^"\\]|\\.)*)"', seg)
        return "".join(parts).replace("%%", "%%")

    f_hdr = fmt_of(fsrc, '"FnProbe: %d/%d functions hooked')
    f_fn = fmt_of(fsrc, '"FnProbe %X hits %llu')
    f_arg = fmt_of(fsrc, '"FnProbe %X hit#%llu')
    f_never = fmt_of(fsrc, '"FnProbe: never hit yet:')
    b_start = fmt_of(bsrc, '"BossLog #%d: BossBattleMode start')
    b_end = fmt_of(bsrc, '"BossLog #%d: BossBattleModeEnd')
    b_flag = fmt_of(bsrc, '"BossLog #%d: SetMissionFlag')
    b_gauge = fmt_of(bsrc, '"BossLog #%d: ShowBossGauge')
    b_clear = fmt_of(bsrc, '"BossLog #%d: SetMissionClearFlag')
    s_spr = fmt_of(dsrc, '"stats spr:')
    p_win = fmt_of(psrc, '"PadLatency %ds: changes')
    p_tot = fmt_of(psrc, '"PadLatency total: changes')
    tmp = pathlib.Path(tempfile.mkdtemp(prefix="playsession_selftest_"))
    day = dt.datetime(2026, 10, 8, 20, 0, 0)
    L = []

    def at(sec, text):
        t = day + dt.timedelta(seconds=sec)
        L.append("[%s] %s" % (t.strftime("%H:%M:%S.") + "%03d" % (t.microsecond // 1000), text))

    at(0, "Nioh3EngineFix 1.7.35 (built Oct  8 2026) - native port of the live-tested Nioh 3 engine patches")
    at(1, c_printf(f_hdr, 14, 14, "default 10.1", 30, 8, 16, "", ""))
    # real line formats copied from captures (LoadStats / LoadGates / mem / latency / cpu) with times shifted
    at(60, "LoadStats #1: 58A set (loader in-flight cap 10, GameMain+4Dh = 0)")
    at(84, "LoadStats #1: 24.0 s | frames 2504 (9.6 ms) | CPU 5.24 cores | stage ready +1.2 s | loaders busy 21.5/15.9 s (first +0.0, last +22.2), in-flight avg A 13.3 B 2.2 of 10 (at cap A 96 % B 6 %), pending avg A 1236.3 B 1.7 | ready-wait -1.0..-1.0 s (acc max 0.0) | event scene 0.0 s")
    at(84, "LoadStats #1 io: read 1177 MB archive (62436 calls, 1738 MB overlapped, 6057 ms in-call) + 562 MB other (523621 calls, 55057 ms) | inflate 5.53 CPU-s, 76248 chunks, 443 -> 1031 MB, 0 failed, max 7 parallel, 7 threads so far | PSO gfx 20 rt + 0 preload, cmp 8 rt + 0 preload, 0.00 CPU-s, max 5.9 ms | mgr Sleep(1) A idle 1825, A no-progress 31480, B idle 50498")
    at(84, "LoadGates #1 holder: G8 StageAreasObjectsReady (13.27 s of 24.0 s)")
    for k in range(1, 20):
        at(30 * k, "stats mem: private 0 MB, working set %d MB (peak 4550), commit 4416 MB, handles 0, threads 339, VRAM local used/budget %d/10230 MB, non-local 928/4819 MB | sys RAM avail 5942 MB, commit 11286/23505 MB" % (4500 + k, 8000 + k))
        at(30 * k, "stats latency (TS medians over the last 4096 probes, ms): K->C input age 15.88 | C->L update+limiter 16.62 | L->W 9.33 | W->B waitable wait 0.00 | B->P render 16.44 | C->C logic frame 16.67 | P->P present 16.71 | K->P sample->present 58.43 | input->display B(n+1)-K(n) 58.74 (+ half a refresh for mid-screen; vkd3d latency-1 semantics)")
        at(30 * k, "stats cpu: MQ ON, calls 48014180 (+47965612 in 30 s) | PT1 ON, textures claimed 38493 (+38493), lock pairs saved 219364 (+219364) | DL ON, lookups skipped 0 (+0)")
        at(30 * k, c_printf(s_spr, 100 + k, 3, 850.0, 4200.0, 2, 5, 0, 120.0, 900.0, 0))
        at(30 * k, c_printf(f_fn, 0x4F9750, 1000 * k, 1000, 30, 33.3, 120, "20:00:05.120", "20:%02d:%02d.000" % (k // 2, (30 * k) % 60), 4242, 3, "", 640,
                            " 4FA123 61% 2C5AA6C+1F0 20% Nioh3.exe+12 9%"))
        at(30 * k, c_printf(f_never, " 556118 59F498"))
        at(30 * k, c_printf(p_win, 30, 40, 30, 4, 6, 40, 0, 31.5 + k * 0.01, 38.0, 45.0, 31.0, 40.5, 48.0, 55.0, 40, 0, 16.67, 16.9, 17.5, 1800, 6.0))
        at(30 * k, c_printf(p_tot, 40 * k, 30 * k, 4 * k, 6 * k, 40 * k, 0, 31.6, 38.1, 45.2, 31.05, 40.6, 48.1, 55.2, 40 * k, 0, 16.67, 16.9, 17.5,
                            1800 * k, "present=DXGI Present hook, gpu=own fence, frames=TS ring"))
    at(31, c_printf(f_arg, 0x4F9750, 1, "20:00:05.120", 4242, "4FA123", 0x1F2E3D4C5B6A, 0x91A41AD2, 3, 0, " [rcx] 91A41AD2 00000001 3F800000 00000000",
                    " rdx=91A41AD2 [rcx+0]=91A41AD2"))
    at(200, c_printf(b_start, 1, 200.0, 0x12345678, 0))
    at(201, c_printf(b_gauge, 1, 777, 201.0, ""))
    at(250, c_printf(b_flag, 1, 392, 1, 250.0, "in fight"))
    at(320, c_printf(b_end, 1, 320.0, 120.0, 1))
    at(321, c_printf(b_clear, 1, 31002, 321.0, "fight window"))
    at(400, "RematchLog Q9: BuildSaveFilePath caller=+1FD6E47  t=4.800")
    (tmp / "modlog_run1.log").write_text("\n".join(L) + "\n", encoding="utf-8")
    # fake PresentMon v2 csv in the exact header this PC's PresentMon 2.6.0 writes with --v2_metrics --date_time
    hdr = "Application,ProcessID,SwapChainAddress,PresentRuntime,SyncInterval,PresentFlags,AllowsTearing,PresentMode,CPUStartDateTime,FrameTime,CPUBusy,CPUWait,GPULatency,GPUTime,GPUBusy,GPUWait,DisplayLatency,DisplayedTime,AnimationError,AnimationTime,MsFlipDelay,AllInputToPhotonLatency,ClickToPhotonLatency"
    bias = -4 * 3600.0
    rnd = random.Random(1)
    rows = [hdr]
    t = 0.0
    while t < 600:
        ft = 8.33 + rnd.random() * 0.5
        if rnd.random() < 0.002:
            ft = 60.0
        cpu = ft * 0.7
        gpu = ft * 0.95
        stamp = day + dt.timedelta(seconds=t + bias)
        ds = "%d-%d-%d %s.%06d000" % (stamp.year, stamp.month, stamp.day, stamp.strftime("%H:%M:%S"), stamp.microsecond)
        aip = "%.4f" % (30 + rnd.random() * 10) if rnd.random() < 0.05 else "NA"
        rows.append("Nioh3.exe,1234,0x1,DXGI,0,0,1,Hardware Composed: Independent Flip,%s,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,NA,%s,NA" % (
            ds, ft, cpu, ft - cpu, 1.0, gpu, gpu, ft - gpu, 12.0, ft, 0.1, t * 1000, aip))
        t += ft / 1000.0
    (tmp / "presentmon.csv").write_text("\n".join(rows) + "\n", encoding="utf-8")
    sm = ["time,game_running,cpu_total_pct,cpu_perf_pct,game_cpu_pct,gpu3d_total_pct,gpu3d_game_pct,vram_game_mb,vram_adapter_mb,game_ws_mb,game_private_mb,ram_avail_mb,commit_mb"]
    for k in range(0, 600, 5):
        sm.append("%s,1,35.5,104.2,410.0,97.0,95.5,8100,8900,4500,5200,12000,30000" % (day + dt.timedelta(seconds=k)).isoformat())
    (tmp / "sysmon.csv").write_text("\n".join(sm) + "\n", encoding="utf-8")
    (tmp / "session.json").write_text(json.dumps({"start": day.isoformat(), "start_ts": day.timestamp(), "stop": (day + dt.timedelta(seconds=700)).isoformat(),
                                                  "stop_ts": day.timestamp() + 700, "pm_bias_s": bias, "asi_version": "1.7.35", "asi_version_before": "1.7.34",
                                                  "capture_keys": ["[Debug] FnProbe=1"]}), encoding="utf-8")
    out, J = summarize(tmp)
    fails = []

    def chk(c, m):
        if not c:
            fails.append(m)

    fa = J.get("frames_all") or {}
    chk(fa.get("frames", 0) > 60000, "frames parsed: %s" % fa.get("frames"))
    chk(110 < (fa.get("fps_avg") or 0) < 120, "fps %s" % fa.get("fps_avg"))
    chk((fa.get("over_50") or 0) > 50 and (fa.get("spikes") or 0) > 50, "spikes %s / >50 ms %s" % (fa.get("spikes"), fa.get("over_50")))
    chk((fa.get("aip_latency_n") or 0) > 1000 and 30 <= fa.get("aip_latency_p50", 0) <= 40, "input latency %s" % fa.get("aip_latency_p50"))
    chk((fa.get("gpu_bound_pct") or 0) > 90, "gpu bound %s" % fa.get("gpu_bound_pct"))
    chk(J.get("frames_play") and J["frames_play"]["frames"] < fa["frames"], "gameplay split (load window removed)")
    chk(len(J.get("loads", [])) == 1 and J["loads"][0].get("holder", "").startswith("G8") and J["loads"][0].get("read_mb") == 1739, "loads %s" % J.get("loads"))
    fights = J.get("fights", [])
    chk(len(fights) == 1 and fights[0].get("dur") == 120.0 and fights[0].get("flags") == 1, "fights %s" % fights)
    chk(fights and fights[0].get("frames") and 100 < fights[0]["frames"]["fps_avg"] < 125, "fight window frames (clock bias)")
    chk(len(J.get("mem", [])) == 19 and len(J.get("latency_ts", [])) == 19 and len(J.get("spr", [])) == 19 and len(J.get("cpu", [])) == 19, "30 s lines")
    fn = J.get("fnprobe", {}).get("fns", {})
    chk("4F9750" in fn and fn["4F9750"]["hits"] == 19000 and len(fn["4F9750"]["callers"]) == 3 and fn["4F9750"]["peak"] == 120, "fnprobe %s" % fn.get("4F9750"))
    chk(J["fnprobe"].get("never") == ["556118", "59F498"], "fnprobe never %s" % J["fnprobe"].get("never"))
    chk(J["fnprobe"].get("header", "").startswith("FnProbe: 14/14"), "fnprobe header")
    chk(J.get("system", {}).get("gpu3d_game_pct", {}).get("avg") == 95.5, "sysmon")
    plt = J.get("padlat", {}).get("totals", [])
    chk(len(plt) == 1 and plt[0]["changes"] == 760 and plt[0]["in_present_p50"] == 31.6 and plt[0]["poll_p50"] == 16.67
        and plt[0]["in_gpu_p99"] == 55.2 and plt[0]["sources"].startswith("present=DXGI"), "padlat totals %s" % plt)
    chk(len(J.get("padlat", {}).get("windows", [])) == 19, "padlat windows")
    chk(5.0 < (fa.get("present_display_p50") or 0) < 7.0, "present->display %s" % fa.get("present_display_p50"))
    chk(abs((J.get("pad_to_display_est_ms") or 0) - (16.67 / 2 + 31.6 + fa.get("present_display_p50", 0))) < 0.01, "pad->display estimate")
    chk(not J.get("missing"), "missing %s" % J.get("missing"))
    txt = (tmp / "report.md").read_text(encoding="utf-8")
    chk("Unknown_GridBuild15KStack_4F9750" in txt or "0x4F9750" in txt, "symbol resolution in report")
    # ---- compare mod vs stock (stock = no mod log, FrameType / Displayed columns, frame generation, a few stalls)
    cmp_root = pathlib.Path(tempfile.mkdtemp(prefix="playsession_selftest_cmp_"))
    shutil.copytree(tmp, cmp_root / "20261008-2000-session")
    sd = cmp_root / "20261008-2100-session"
    sd.mkdir()
    rows2 = [hdr.replace("CPUStartDateTime,", "CPUStartDateTime,FrameType,Displayed,")]
    t = 0.0
    k2 = 0
    while t < 600:
        ft = 16.0 + rnd.random() * 1.0
        if 100 < t < 100.5 or 300 < t < 300.4:
            ft = 700.0
        cpu = ft * 0.5
        gpu = ft * 0.9
        stamp = day + dt.timedelta(seconds=t + bias)
        ds = "%d-%d-%d %s.%06d000" % (stamp.year, stamp.month, stamp.day, stamp.strftime("%H:%M:%S"), stamp.microsecond)
        rows2.append("Nioh3.exe,1234,0x1,DXGI,0,0,1,Hardware Composed: Independent Flip,%s,%s,1,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,NA,NA,NA" % (
            ds, "Application" if k2 % 2 == 0 else "AMD_AFMF", ft, cpu, ft - cpu, 1.0, gpu, gpu, ft - gpu, 20.0, ft, 0.1, t * 1000))
        k2 += 1
        t += ft / 1000.0
    (sd / "presentmon.csv").write_text("\n".join(rows2) + "\n", encoding="utf-8")
    shutil.copy(tmp / "sysmon.csv", sd / "sysmon.csv")
    (sd / "session.json").write_text(json.dumps({"start": day.isoformat(), "start_ts": day.timestamp(), "stop": (day + dt.timedelta(seconds=700)).isoformat(),
                                                 "stop_ts": day.timestamp() + 700, "pm_bias_s": bias, "asi_version": "none (stock)", "mode": "stock",
                                                 "stashed": ["Nioh3EngineFix.asi", "dinput8.dll"], "keep_optiscaler": False}), encoding="utf-8")
    out_s, J_s = summarize(sd)
    rep_s = "\n".join(out_s)
    chk(any("STOCK session" in l for l in out_s) and "## Loads" not in rep_s and "## FnProbe" not in rep_s, "stock report: says stock, no mod sections")
    chk(not J_s.get("missing"), "stock report: nothing 'missing' (no mod log is expected) %s" % J_s.get("missing"))
    ctxt, cJ, cpath = compare(cmp_root / "20261008-2000-session", sd)
    chk(cpath.exists() and "# Nioh 3: mod vs stock" in ctxt and "| avg fps (presented) |" in ctxt, "compare: table written")
    chk("longest sustained segment" in ctxt and "Loading-like stalls" in ctxt and "Caveats" in ctxt, "compare: phases + stalls + caveats")
    chk(cJ["stalls"]["stock"]["clusters"] == 2 and cJ["stalls"]["mod"]["clusters"] == 0, "compare: stock stall clusters %s" % cJ["stalls"])
    chk("rendered fps (FrameType = Application)" in ctxt and "| frame generation (PresentMon FrameType) | - | ON |" in ctxt, "compare: FG split from FrameType")
    chk("FrameType column missing" in ctxt, "compare: missing FrameType noted")
    shutil.rmtree(cmp_root, ignore_errors=True)
    if fails:
        print("\n".join("FAIL: " + f for f in fails))
        print("report kept in", tmp)
        return 1
    if "--keep" in sys.argv:
        print("report kept in", tmp)
    else:
        shutil.rmtree(tmp, ignore_errors=True)
    print("SELFTEST OK (%d report lines; PresentMon %d frames, fight, load, FnProbe, PadLatency, mem, latency, SPR, CpuStats, sysmon parsed)" % (len(out), fa["frames"]))
    return 0


if __name__ == "__main__":
    if len(sys.argv) > 1 and sys.argv[1] == "--selftest":
        sys.exit(selftest())
    if len(sys.argv) == 4 and sys.argv[1] == "compare":
        txt, _, path = compare(sys.argv[2], sys.argv[3])
        print(txt)
        print("... saved: %s" % path)
        sys.exit(0)
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    lines, _ = summarize(sys.argv[1])
    print("\n".join(lines[:60]))
    print("... full report: %s" % (pathlib.Path(sys.argv[1]) / "report.md"))
