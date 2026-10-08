"""abba2_report.py - analysis for abba2_pc.ps1 (drift-aware PC flag A/B, 2026-10-08).

  py -I abba2_report.py <run_dir>            -> prints the report and writes <run_dir>\\report.txt + verdicts.json
  py -I abba2_report.py --selftest           -> synthetic data: drift/step rejection + verdict rules

Input: <run_dir>\\manifest.jsonl (one line per window, written by the harness) + <name>.csv (PresentMon) + <name>.log
(the mod-log slice of that window).

Why this exists (diagnosis of run 4a2b108, captures\\tm\\abba\\report_A.txt):
  * the in-game FSR frame generation was ON (config.xml FRAME_GENERATION=FSR) and its engagement flapped between
    three regimes - paused (rendered ~150 = displayed), flapping running/paused (rendered ~117, displayed ~170) and
    steady (rendered ~119, displayed ~203) - with no change to any flag. PresentMon fps = displayed fps, so every
    'step' (MQ 172->146, PP 145->196, GF 203->158) was an FG regime change; with FG engaged the rendered rate is pinned
    near REN_FPS 120, so CPU flags could not move fps at all.
  * no flag was ever toggled: U passed the key list as ONE argument ('79 w500 26 ...') -> ui.fsx failed to parse it
    (exit 1, not 3, so no abort); Toggle() returned @(T-output, $false) which is truthy, so 'NOT confirmed' was
    swallowed. The log has zero 'MENU: <label> -> ' lines and the stats lines show GF/PP/MQ in their boot state all run.
  * 'draws' in the OSD stats line is g_st.draws = overlay frames drawn since start (cumulative), not game draw calls.
    Here it is turned into overlay draws per rendered frame over the window (sanity ~1.00); a real draw-call count
    needs the GP profiler.

Method checks (per window): FG must be off (every OSD stats line 'FG off', x1.00, no 'FG state: ON - running'),
the live switch must read back in the stats lines (GF '(switch ON|OFF)', MQ 'MQ on|off', PP skipped>0 only when ON),
and the toggle must have been confirmed by a 'MENU: <label> -> <state>' line (harness field toggle_ok).
Live items: quads ON OFF OFF ON (symmetric -> linear drift cancels). A quad is rejected when the two OFF windows
differ > STEP_IN % (a step inside the quad), the two ON windows differ > STEP_OUT % (drift too large to be linear),
or median GPU busy ON vs OFF moves > GPU_MOVE % (scene change; a CPU patch cannot move GPU time that much).
Verdict: >= 2 valid quads; KEEP = median dfps >= +1 % with every valid quad > 0 and median p99 not worse > 2 %;
DROP = otherwise (neutral or worse); INVALID = < 2 valid quads or unverified switch.
Launch items (ini-only flags, relaunch per arm): 2 BASE launches (order BASE F1 F2 F2 F1 BASE): d1 = F1a - BASE1,
d2 = F1b - BASE2; INVALID if a launch is missing/unverified or the BASE-BASE spread > 3 %; KEEP = both d > 0 and mean >=
max(2 %, 1.5 x BASE spread); DROP otherwise.
3+ BASE launches (notes/pc-cpu-next.md order BASE F1 F2 BASE F2 F1 BASE, harness -Arms): every arm launch is compared with
the BASE fps linearly interpolated (by launch number) between the nearest BASE launch before and after it, so linear drift
cancels; noise = the largest deviation of an inner BASE from the line through its neighbours. INVALID if a launch is
missing/unverified/not bracketed, the BASE range > 8 % or noise > 1.5 %; KEEP = every paired d > 0 and mean >= max(1 %,
2 x noise); DROP otherwise.
Cap check (2026-10-08, run5/run6): the test scene ran at 116-118.5 fps against the 120 cap ([VRR] MaxFps, read from
test_base.ini), so at most +1.3..3 % was reachable. When the median OFF / BASE fps is within CAP_WARN % of the cap the
verdict says "cap-compressed": a DROP there is not evidence that the flag does nothing. Run CPU A/Bs with -MaxFps 240
(the engine's internal tier stays 120).
"""
import csv, json, math, os, re, statistics as st, sys

STEP_IN, STEP_OUT, GPU_MOVE = 1.0, 1.5, 3.0
LAUNCH_SPREAD_MAX = 3.0
LAUNCH_RANGE_MAX, LAUNCH_NOISE_MAX = 8.0, 1.5   # 3+ BASE launches (interpolated baseline)
CAP_WARN = 4.0   # % below the fps cap at which a verdict is flagged cap-compressed
STRICT_FOCUS = False   # see window_ok


def pm_metrics(path):
    bt, cpu, gpu = [], [], []
    with open(path, newline="") as f:
        for r in csv.DictReader(f):
            try:
                bt.append(float(r["MsBetweenPresents"]))
            except (KeyError, ValueError):
                continue
            for k, dst in (("MsCPUBusy", cpu), ("MsGPUBusy", gpu)):
                try:
                    dst.append(float(r[k]))
                except (KeyError, ValueError):
                    pass
    if len(bt) < 50:
        return None
    bt = bt[1:]
    s = sorted(bt)
    return {"n": len(bt), "fps": 1000.0 / (sum(bt) / len(bt)), "p99": s[min(len(s) - 1, int(0.99 * len(s)))],
            "cpu": st.median(cpu) if cpu else None, "gpu": st.median(gpu) if gpu else None}


RX_STATS = re.compile(r"^\[(\d+):(\d+):([\d.]+)\] OSD: stats: rendered ([\d.]+) fps, displayed ([\d.]+) fps \(x([\d.]+), FG (\w+).*?draws (\d+)")


def log_metrics(path):
    tx = open(path, encoding="utf-8", errors="replace").read() if os.path.exists(path) else ""
    out = {"fg_ok": True, "fg_why": "", "ovl_per_frame": None, "gf": None, "mq": None, "pp_skipped": None, "dq": None, "p1e7": None}
    stats = []
    for line in tx.splitlines():
        m = RX_STATS.match(line)
        if m:
            h, mi, s, ren, disp, ratio, fg, dr = m.groups()
            stats.append((int(h) * 3600 + int(mi) * 60 + float(s), float(ren), float(disp), float(ratio), fg, int(dr)))
    if "FG state: ON - running" in tx:
        out["fg_ok"], out["fg_why"] = False, "FG running"
    for t, ren, disp, ratio, fg, dr in stats:
        if fg != "off" or abs(ratio - 1.0) > 0.01:
            out["fg_ok"], out["fg_why"] = False, f"FG {fg} x{ratio:.2f}"
    if len(stats) >= 2:   # OSD 'draws' = cumulative overlay frames -> per-interval delta per rendered frame
        (t0, _, _, _, _, d0), (t1, ren1, _, _, _, d1) = stats[0], stats[-1]
        ren = st.mean(x[1] for x in stats[1:])
        if t1 > t0 and ren > 0:
            out["ovl_per_frame"] = (d1 - d0) / ((t1 - t0) * ren)
    gf = re.findall(r"stats/s GF: fence waits (\d+).*?\(switch (ON|OFF|off)\)", tx)
    if gf:
        out["gf"] = gf[-1][1].upper()
    mq = re.findall(r"stats cpu: MQ (on|off|ON|OFF)", tx)
    if mq:
        out["mq"] = mq[-1].upper()
    pp = re.findall(r"PP scans (\d+) skipped (\d+)", tx)
    if pp:
        out["pp_skipped"] = sum(int(b) for a, b in pp)
    m = re.search(r"DQ: main-loop retry quantum .*?\(switch (ON|off)\)", tx)
    if m:
        out["dq"] = m.group(1).upper()
    m = re.search(r"P1E7: texture-loop prefetch .*?\(switch (ON|off)\)", tx)
    if m:
        out["p1e7"] = m.group(1).upper()
    if "MA: 25 main-thread locked atomic loads" in tx:
        out["ma"] = "ON"
    # 1.7.39 live rows: the 30 s 'stats/s drawprep:' line carries both switches ('-' = not installed)
    dp = re.findall(r"drawprep: DQ (ON|off|-) \(.*?\), P1E7 (ON|off|-)", tx)
    if dp:
        out["dq_sw"], out["p1e7_sw"] = (None if x == "-" else x.upper() for x in dp[-1])
    return out


def seen_state(item, lm):
    if item == "GF":
        return lm["gf"]
    if item == "MQ":
        return lm["mq"]
    if item == "PP":
        return None if lm["pp_skipped"] is None else ("ON" if lm["pp_skipped"] > 0 else "OFF")
    if item == "DQ":
        return lm.get("dq_sw")
    if item == "P1E7":
        return lm.get("p1e7_sw")
    return None   # B has no counter: the MENU read-back is the proof


def pct(a, b):
    return (a - b) / b * 100.0 if b else float("nan")


def load(run):
    rows = []
    with open(os.path.join(run, "manifest.jsonl"), encoding="utf-8") as f:
        for line in f:
            line = line.strip()
            if line:
                rows.append(json.loads(line))
    for r in rows:
        pm = os.path.join(run, r["name"] + ".csv")
        r["pm"] = pm_metrics(pm) if os.path.exists(pm) else None
        r["lm"] = log_metrics(os.path.join(run, r["name"] + ".log"))
    return rows


def win_line(r):
    pm, lm = r["pm"], r["lm"]
    if not pm:
        return f"  {r['name']:<28} NO DATA"
    opf = "-" if lm["ovl_per_frame"] is None else f"{lm['ovl_per_frame']:.2f}"
    return (f"  {r['name']:<28} {r.get('state', ''):<3} fps {pm['fps']:7.2f} p99 {pm['p99']:6.2f} cpu {pm['cpu'] or 0:5.2f} "
            f"gpu {pm['gpu'] or 0:5.2f} n {pm['n']:5d} ovl/frame {opf} fg {'ok' if lm['fg_ok'] else 'BAD ' + lm['fg_why']}{'' if r.get('focus_end') in (None, 'Nioh3') else ' focus_end=' + r['focus_end']}"
            f" seen {seen_state(r.get('item'), lm) or '-'} tog {'ok' if r.get('toggle_ok', True) else 'FAIL'}")


def window_ok(r):
    if not r["pm"]:
        return False, "no data"
    if not r["lm"]["fg_ok"]:
        return False, r["lm"]["fg_why"]
    # focus_end is sampled right after the window. run5: two windows read 'Code' there, but the focus check before the next
    # step found Nioh3 in front again without a refocus (no 'focus lost' trace line) and their fps matched the neighbours,
    # i.e. a transient flash at the window end. So it only rejects when STRICT_FOCUS is set.
    if STRICT_FOCUS and r.get("focus_end") not in (None, "Nioh3"):
        return False, f"focus lost to {r['focus_end']}"
    if abs(r["pm"]["fps"] - 120.0) < 1.0:
        return False, "at the 120 fps cap (not CPU/GPU limited)"
    if not r.get("toggle_ok", True):
        return False, "toggle not confirmed"
    s = seen_state(r.get("item"), r["lm"])
    if s is not None and s != r["state"].upper():
        return False, f"switch reads {s}"
    return True, ""


def live_verdict(item, ws, min_quads=2):
    lines, quads = [], {}
    for r in ws:
        quads.setdefault(r["quad"], []).append(r)
    res = []
    for q in sorted(quads):
        w = sorted(quads[q], key=lambda r: r["pos"])
        if len(w) != 4:
            lines.append(f"    q{q}: incomplete ({len(w)} windows) -> rejected"); continue
        bad = [f"{r['name']}: {why}" for r in w for ok, why in [window_ok(r)] if not ok]
        if bad:
            lines.append(f"    q{q}: rejected ({'; '.join(bad)})"); continue
        on = [r["pm"] for r in w if r["state"].upper() == "ON"]
        off = [r["pm"] for r in w if r["state"].upper() == "OFF"]
        if len(on) != 2 or len(off) != 2:
            lines.append(f"    q{q}: not ON OFF OFF ON -> rejected"); continue
        f_on, f_off = st.mean(x["fps"] for x in on), st.mean(x["fps"] for x in off)
        step_in = abs(pct(off[0]["fps"], off[1]["fps"]))
        step_out = abs(pct(on[0]["fps"], on[1]["fps"]))
        g_on = st.mean(x["gpu"] for x in on); g_off = st.mean(x["gpu"] for x in off)
        gmove = abs(pct(g_on, g_off))
        d = pct(f_on, f_off)
        dp99 = pct(st.mean(x["p99"] for x in on), st.mean(x["p99"] for x in off))
        dcpu = pct(st.mean(x["cpu"] for x in on), st.mean(x["cpu"] for x in off))
        why = []
        if step_in > STEP_IN: why.append(f"OFF-OFF step {step_in:.2f}%")
        if step_out > STEP_OUT: why.append(f"ON-ON drift {step_out:.2f}%")
        if gmove > GPU_MOVE: why.append(f"gpu moved {gmove:.1f}%")
        tag = "rejected (" + ", ".join(why) + ")" if why else "valid"
        lines.append(f"    q{q}: dfps {d:+.2f}% dp99 {dp99:+.2f}% dcpu_busy {dcpu:+.2f}% (OFF-OFF {step_in:.2f}%, ON-ON {step_out:.2f}%, gpu {gmove:.2f}%) {tag}")
        if not why:
            res.append((d, dp99, dcpu))
    if len(res) < min_quads:
        return "INVALID", f"{len(res)} valid quads < {min_quads}", lines, None
    md = st.median(x[0] for x in res); mp = st.median(x[1] for x in res); mc = st.median(x[2] for x in res)
    allpos = all(x[0] > 0 for x in res)
    v = "KEEP" if (md >= 1.0 and allpos and mp <= 2.0) else "DROP"
    return v, f"median dfps {md:+.2f}% (quads {', '.join(f'{x[0]:+.2f}' for x in res)}), dp99 {mp:+.2f}%, dcpu_busy {mc:+.2f}%", lines, md


def launch_fps(ws, flag=None, want=None):
    good = []
    for r in ws:
        ok, why = window_ok(r)
        if not ok:
            continue
        if flag:
            seen = r["lm"].get(flag.lower())
            if seen is None:   # the install line is printed once at boot: the harness copies it into the first window log
                seen = r.get("boot_" + flag.lower())
            if seen != want:
                continue
        good.append(r["pm"])
    if len(good) < 2:
        return None
    return {"fps": st.median(x["fps"] for x in good), "cpu": st.median(x["cpu"] for x in good), "n": len(good)}


def launch_verdicts(rows):
    out, lines = {}, []
    by = {}
    for r in rows:
        if r.get("kind") == "launch":
            by.setdefault(r["launch"], []).append(r)
    arms = {L: ws[0]["arm"] for L, ws in by.items()}
    base = [L for L in sorted(by) if arms[L] == "BASE"]
    if len(base) < 2:
        return out, ["  launch arms: fewer than 2 BASE launches"]
    bm = [launch_fps(by[L]) for L in base]
    for L in sorted(by):
        lines.append(f"  launch L{L} {arms[L]}: " + ", ".join(f"{r['name']} {r['pm']['fps']:.2f}" if r["pm"] else r["name"] + " -" for r in by[L]))
    if None in bm:
        return out, lines + ["  BASE launch without 2 valid windows -> launch flags INVALID"]
    if len(base) >= 3:
        return launch_verdicts_interp(by, arms, base, bm, out, lines)
    spread = abs(pct(bm[1]["fps"], bm[0]["fps"]))
    lines.append(f"  BASE L{base[0]} {bm[0]['fps']:.2f} / L{base[-1]} {bm[1]['fps']:.2f} fps: spread {spread:.2f}%")
    for f in sorted(set(arms.values()) - {"BASE"}):
        Ls = [L for L in sorted(by) if arms[L] == f]
        fm = [launch_fps(by[L], f, "ON") for L in Ls]
        if len(Ls) != 2 or None in fm:
            out[f] = ("INVALID", "launch missing or switch not read back ON"); continue
        if spread > LAUNCH_SPREAD_MAX:
            out[f] = ("INVALID", f"BASE spread {spread:.2f}% > {LAUNCH_SPREAD_MAX}%"); continue
        d1, d2 = pct(fm[0]["fps"], bm[0]["fps"]), pct(fm[1]["fps"], bm[1]["fps"])
        c1, c2 = pct(fm[0]["cpu"], bm[0]["cpu"]), pct(fm[1]["cpu"], bm[1]["cpu"])
        mean = (d1 + d2) / 2
        thr = max(2.0, 1.5 * spread)
        v = "KEEP" if (d1 > 0 and d2 > 0 and mean >= thr) else "DROP"
        out[f] = (v, f"dfps {d1:+.2f}% / {d2:+.2f}% (mean {mean:+.2f}%, threshold {thr:.2f}%), dcpu_busy {c1:+.2f}% / {c2:+.2f}%")
    return out, lines


def launch_verdicts_interp(by, arms, base, bm, out, lines):
    """3+ BASE launches: each arm launch vs the BASE fps interpolated between its bracketing BASE launches."""
    bf = dict(zip(base, (x["fps"] for x in bm)))
    bc = dict(zip(base, (x["cpu"] for x in bm)))

    def interp(L, d):
        lo = max((b for b in base if b < L), default=None)
        hi = min((b for b in base if b > L), default=None)
        if lo is None or hi is None:
            return None
        return d[lo] + (d[hi] - d[lo]) * (L - lo) / (hi - lo)
    rng = abs(pct(max(bf.values()), min(bf.values())))
    noise = 0.0
    for i in range(1, len(base) - 1):   # inner BASE vs the line through its neighbours
        lo, b, hi = base[i - 1], base[i], base[i + 1]
        noise = max(noise, abs(pct(bf[b], bf[lo] + (bf[hi] - bf[lo]) * (b - lo) / (hi - lo))))
    lines.append("  BASE " + " / ".join(f"L{b} {bf[b]:.2f}" for b in base) + f" fps: range {rng:.2f}%, noise (inner BASE vs line) {noise:.2f}%")
    for f in sorted(set(arms.values()) - {"BASE"}):
        Ls = [L for L in sorted(by) if arms[L] == f]
        fm = [launch_fps(by[L], f, "ON") for L in Ls]
        if len(Ls) < 2 or None in fm or any(interp(L, bf) is None for L in Ls):
            out[f] = ("INVALID", "launch missing, not bracketed by BASE launches, or switch not read back ON"); continue
        if rng > LAUNCH_RANGE_MAX or noise > LAUNCH_NOISE_MAX:
            out[f] = ("INVALID", f"BASE range {rng:.2f}% (max {LAUNCH_RANGE_MAX}) / noise {noise:.2f}% (max {LAUNCH_NOISE_MAX})"); continue
        ds = [pct(m["fps"], interp(L, bf)) for L, m in zip(Ls, fm)]
        cs = [pct(m["cpu"], interp(L, bc)) for L, m in zip(Ls, fm)]
        mean = sum(ds) / len(ds)
        thr = max(1.0, 2.0 * noise)
        v = "KEEP" if (all(d > 0 for d in ds) and mean >= thr) else "DROP"
        out[f] = (v, "dfps " + " / ".join(f"{d:+.2f}%" for d in ds) + f" (mean {mean:+.2f}%, threshold {thr:.2f}%), dcpu_busy "
                  + " / ".join(f"{c:+.2f}%" for c in cs) + " vs interpolated BASE")
    return out, lines


def cap_fps(run):
    """The fps cap of the run: [VRR] MaxFps in test_base.ini (120 when absent; 0 = unlocked)."""
    try:
        m = re.search(r"(?m)^MaxFps\s*=\s*(\d+)", open(os.path.join(run, "test_base.ini"), encoding="utf-8", errors="replace").read())
        v = int(m.group(1)) if m else 120
    except OSError:
        v = 120
    return 1000.0 if v == 0 else float(v)


def cap_note(rows, item, cap):
    """' [cap-compressed: ...]' when either side (OFF/BASE or ON/arm, median fps) sits within CAP_WARN % below the cap."""
    if any(r.get("kind") == "launch" and r.get("arm") == item for r in rows):
        sides = [[r["pm"]["fps"] for r in rows if r.get("kind") == "launch" and r.get("arm") == a and r["pm"]] for a in ("BASE", item)]
    else:
        sides = [[r["pm"]["fps"] for r in rows if r.get("kind") == "live" and r.get("item") == item and r["state"].upper() == s and r["pm"]]
                 for s in ("OFF", "ON")]
    sides = [st.median(x) for x in sides if x]
    if not sides:
        return ""
    f = max(sides)
    room = (cap / f - 1.0) * 100.0
    if not -1.0 <= room < CAP_WARN:   # far below the cap, or above it (the cap read from the ini does not apply)
        return ""
    return f" [cap-compressed: {f:.1f} fps vs cap {cap:.0f}, only {room:+.1f}% headroom]"


def report(run):
    rows = load(run)
    txt = [f"abba2 report: {run}", "windows:"] + [win_line(r) for r in rows]
    verdicts = {}
    cap = cap_fps(run)
    items = []
    for r in rows:
        if r.get("kind") == "live" and r["item"] not in items:
            items.append(r["item"])
    for it in items:
        ws = [r for r in rows if r.get("kind") == "live" and r["item"] == it]
        v, why, lines, md = live_verdict(it, ws)
        why += cap_note(rows, it, cap)
        verdicts[it] = {"verdict": v, "detail": why, "method": "live ON-OFF-OFF-ON quads, one launch, FG off"}
        txt += [f"{it}: {v} - {why}"] + lines
    lv, lines = launch_verdicts(rows)
    txt += lines
    for f, (v, why) in lv.items():
        why += cap_note(rows, f, cap)
        verdicts[f] = {"verdict": v, "detail": why, "method": "relaunch per arm (BASE-bracketed), FG off"}
        txt.append(f"{f}: {v} - {why}")
    s = "\n".join(txt)
    open(os.path.join(run, "report.txt"), "w", encoding="utf-8").write(s + "\n")
    json.dump(verdicts, open(os.path.join(run, "verdicts.json"), "w"), indent=1)
    return s, verdicts


def selftest():
    import random, tempfile
    random.seed(1)
    d = tempfile.mkdtemp(prefix="abba2_selftest_")
    man = open(os.path.join(d, "manifest.jsonl"), "w")
    t = [0.0]

    def win(name, fps, gpu, cpu, logtxt, **kw):
        with open(os.path.join(d, name + ".csv"), "w", newline="") as f:
            w = csv.writer(f); w.writerow(["MsBetweenPresents", "MsCPUBusy", "MsGPUBusy"])
            for _ in range(600):
                w.writerow([1000 / fps * random.uniform(0.98, 1.02), cpu, gpu])
        open(os.path.join(d, name + ".log"), "w").write(logtxt)
        man.write(json.dumps(dict(name=name, **kw)) + "\n")

    def stats(fg="off", ratio="1.00", sw=None, mq=None, pp=None):
        s = (f"[11:00:00.000] OSD: stats: rendered 150.0 fps, displayed 150.0 fps (x{ratio}, FG {fg}, 1 chain(s)), draws 1000, skipped 0\n"
             f"[11:00:10.000] OSD: stats: rendered 150.0 fps, displayed 150.0 fps (x{ratio}, FG {fg}, 1 chain(s)), draws 2500, skipped 0\n")
        if sw: s += f"[11:00:10.000] stats/s GF: fence waits 150, avg wait 134 us, fallbacks to Sleep(0) 0 (switch {sw})\n"
        if mq: s += f"[11:00:10.000] stats cpu: MQ {mq}, calls 5 (+5 in 30 s)\n"
        if pp is not None: s += f"[11:00:10.000] stats/s exp: W6 waits 0 | PP scans 20 skipped {pp} | FPC\n"
        return s
    seq = ["on", "off", "off", "on"]
    # GF: real +3 % with slow linear drift -> KEEP
    k = 0
    for q in (1, 2, 3):
        for p, s in enumerate(seq, 1):
            k += 1; base = 150 + 0.1 * k
            win(f"L1_GF_q{q}_{p}_{s}", base * (1.03 if s == "on" else 1), 4.2, 6.0, stats(sw=s.upper()), kind="live", item="GF", quad=q, pos=p, state=s, toggle_ok=True, launch=1, arm="BASE")
    # MQ: neutral, quad 2 has a step (FG regime) inside -> rejected, rest neutral -> DROP
    for q in (1, 2, 3):
        for p, s in enumerate(seq, 1):
            f = 150.0
            lg = stats(mq=s.upper())
            if q == 2 and p >= 3: f = 170.0
            if q == 2 and p == 3: lg = stats(fg="active", ratio="1.42", mq=s.upper())
            win(f"L1_MQ_q{q}_{p}_{s}", f, 4.2, 6.0, lg, kind="live", item="MQ", quad=q, pos=p, state=s, toggle_ok=True, launch=1, arm="BASE")
    # PP: toggle never confirmed -> INVALID
    for q in (1, 2):
        for p, s in enumerate(seq, 1):
            win(f"L1_PP_q{q}_{p}_{s}", 150, 4.2, 6.0, stats(pp=0), kind="live", item="PP", quad=q, pos=p, state=s, toggle_ok=(p == 1), launch=1, arm="BASE")
    # launch arms: BASE DQ P1E7 P1E7 DQ BASE ; DQ +4 %, P1E7 0
    arms = {1: "BASE", 2: "DQ", 3: "P1E7", 4: "P1E7", 5: "DQ", 6: "BASE"}
    for L, a in arms.items():
        for s in (1, 2, 3):
            f = 150 * (1.04 if a == "DQ" else 1.0) * (1.005 if L > 3 else 1)
            lg = stats() + (f"[x] DQ: main-loop retry quantum 1 ms -> 200 us at 1A74C45 (switch {'ON' if a == 'DQ' else 'off'})\n"
                            f"[x] P1E7: texture-loop prefetch at 1E72C6 (switch {'ON' if a == 'P1E7' else 'off'})\n")
            win(f"L{L}_{a}_s{s}", f, 4.2, 6.0, lg, kind="launch", launch=L, arm=a, state="on" if a != "BASE" else "off")
    man.close()
    s, v = report(d)
    print(s)
    want = {"GF": "KEEP", "MQ": "DROP", "PP": "INVALID", "DQ": "KEEP", "P1E7": "DROP"}
    got = {k: v[k]["verdict"] for k in want}
    assert got == want, (got, want)
    assert "OFF-OFF step" in s or "FG active" in s
    # 3 BASE launches (pc-cpu-next.md order) with -1 %/launch linear drift: DQ +3 % -> KEEP, P1E7 0 -> DROP; the
    # 2-BASE rule would call this INVALID (BASE spread 9 %), the interpolated baseline cancels the drift
    d2 = tempfile.mkdtemp(prefix="abba2_selftest3_")
    d, man = d2, open(os.path.join(d2, "manifest.jsonl"), "w")
    for L, a in enumerate(["BASE", "DQ", "P1E7", "BASE", "P1E7", "DQ", "BASE"], 1):
        for s in (1, 2, 3):
            f = 150 * (1 - 0.01 * (L - 1)) * (1.03 if a == "DQ" else 1.0)
            lg = stats() + (f"[x] DQ: main-loop retry quantum 1 ms -> 200 us at 1A74C45 (switch {'ON' if a == 'DQ' else 'off'})\n"
                            f"[x] P1E7: texture-loop prefetch at 1E72C6 (switch {'ON' if a == 'P1E7' else 'off'})\n")
            win(f"L{L}_{a}_s{s}", f, 4.2, 6.0, lg, kind="launch", launch=L, arm=a, state="on" if a != "BASE" else "off")
    man.close()
    s3, v3 = report(d2)
    print(s3)
    got3 = {k: v3[k]["verdict"] for k in ("DQ", "P1E7")}
    assert got3 == {"DQ": "KEEP", "P1E7": "DROP"}, got3
    print("SELFTEST OK", got, got3)


if __name__ == "__main__":
    if len(sys.argv) > 1 and sys.argv[1] == "--selftest":
        selftest()
    elif len(sys.argv) > 1:
        print(report(sys.argv[1])[0])
    else:
        print(__doc__)
