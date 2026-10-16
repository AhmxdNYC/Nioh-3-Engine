"""Per-load phase timeline from Nioh3EngineFix LoadStats/LoadGates logs (+ optional Nioh3BossRematch log).

usage: py -I tools/loading/ally_load_phases.py <engine.log> [--rematch rematch.log] [--marker NAME=HH:MM:SS.mmm ...] [--confirm-marker NAME=HH:MM:SS.mmm] [--confirm-name NAME]
                                                [--markers index.txt] [--json out.json]
  --markers reads "NAME HH:MM:SS[.mmm]" lines (e.g. the harness seq/index.txt "TIP-A 00:54:35"); other lines ignored. Markers are annotations unless explicitly typed with --confirm-marker or --confirm-name.

Splits each 58A window (set -> clear) into:
  pre-gate     set -> first LoadIsLoadEnd evaluation
  active       first -> last evaluation; per-gate first-failing time, with the stale tail removed (below)
  post-eval    last observed evaluation -> clear: no further evaluation recorded. With AutoContinueLoad=0 this holds the tip page's
               confirm wait (notes/auto-continue-load.md: sequencer states 0Bh-0Fh), but the split inside it is only
               reported when the log or an external marker gives a time; otherwise it stays "unknown".

Accounting limits: LoadGates FlushEval credits [evaluation -> next evaluation/clear] to
that evaluation's first failing gate. These are wall intervals, not native resource
service time. The final credited tail is removed, but remaining gate totals still
include scheduling gaps. G9 zero is a per-character observation; aggregate counts do
not prove the enclosing native evaluator passed or identify its sequencer caller.
"uncredited" is the arithmetic residual; unhooked LoaderBusy 3B2970 is a candidate,
not a measured attribution. Resource reads can continue after the final evaluation.
Cover lifecycle attribution requires an ordered same-token begin/Present ACK/native
post/terminal sequence, and still does not prove continuously visible rendering.

Every number is tagged: "log" = printed by the ASI, "derived" = arithmetic on printed values (0.1 s print rounding),
"marker" = an external timestamp you passed, "unknown" = not in the data. Unknowns are never filled in.
"""
import json, re, sys

TS = re.compile(r"^\[(\d+):(\d+):(\d+(?:\.\d+)?)\] (.*)$")
R_SET = re.compile(r"LoadStats #(\d+): 58A set")
R_OWNER = re.compile(r"LoadSeq #(\d+) owner at set: (.*?) \(")
R_SUM = re.compile(r"LoadStats #(\d+): ([\d.]+) s \| frames (\d+) .*?CPU ([\d.]+) cores \| stage ready ([+-][\d.]+) s"
                   r" \| loaders busy ([\d.]+)/([\d.]+) s \(first ([+-][\d.]+), last ([+-][\d.]+)\)")
R_IO = re.compile(r"LoadStats #(\d+) io: read (\d+) MB archive \((\d+) calls.*?(\d+) ms in-call\) \+ (\d+) MB other "
                  r"\((\d+) calls, (\d+) ms\)")
R_LG = re.compile(r"LoadGates #(\d+): (\d+) evaluations \((\d+) with no failing gate\) \| last failing eval ([+-][\d.]+) s,"
                  r" first full-pass eval ([+-][\d.]+) s, last eval ([+-][\d.]+) s, window ([\d.]+) s")
R_G = re.compile(r"LoadGates #(\d+) (G\d) (\S+)\s+calls (\d+), failed (\d+), first-failing in (\d+) evals .*?held ([\d.]+) s"
                 r" .*?first fail ([+-][\d.]+) s, last fail ([+-][\d.]+) s, last flip to pass ([+-][\d.]+) s")
R_HOLD = re.compile(r"LoadGates #(\d+) holder: (G\d \S+|none) \(([\d.]+) s")
R_TA = re.compile(r"LoadStats tick #(\d+) ([+-]\d+) s: frames (\d+), CPU ([\d.]+) cores, A act (\d+), read ([\d.]+) MB")
R_TB = re.compile(r"LoadStats tick #(\d+) ([+-]\d+) s: 58A \d, FT -?\d+, B -?\d+, first failing gate (G\d \S+|none), evals (\d+)")
R_RP = re.compile(r"ReadProbe #(\d+) split \[(set->work end|work end->clear)(?: ([+-][\d.]+) s)?\] ([\d.]+) s: (\d+) calls,"
                  r" (\d+) MB, in-call ([\d.]+) thread-s")
R_AC = re.compile(r"AutoContinue #(\d+): load done")
R_ACCFG = re.compile(r"AutoContinue: installed .*?AutoContinueLoad = (\d)")
NEVER = -999.0

# Nioh3BossRematch events worth placing on the timeline (key, regex on the message)
REMATCH_EV = [
    ("request", re.compile(r"calling MissionRequestById\((0x[0-9A-Fa-f]+), slot byte \d+, start point (\d+)")),
    ("spawn_override_applied", re.compile(r"spawn override applied \(load began\)")),
    ("first_frame_after_load", re.compile(r"first frame after the load \(#(\d+) in the mission\)")),
    ("boss_battle_start", re.compile(r"arena: BossBattleMode start")),
    ("in_replay_mission", re.compile(r"IN replay mission \(stage (\d+)\) ([\d.]+) s after the start")),
    ("load_inside_mission", re.compile(r"load inside the mission #(\d+)")),
    ("replay_exit", re.compile(r"replay mission exited")),
    ("cover_begin", re.compile(r"cover token=(\d+) phase=1 attempt=\d+ event=begin")),
    ("cover_present_ack", re.compile(r"cover token=(\d+) phase=2 attempt=0 event=present_ack")),
    ("cover_native_post", re.compile(r"cover token=(\d+) phase=2 attempt=(\d+) event=native_post")),
    ("cover_wait_boss", re.compile(r"cover token=(\d+) phase=3 attempt=\d+ event=phase reason=Waiting for the requested boss")),
    # transition_cover.h phases 4 Ready, 5 Failed, 6 Cancelled end the cover (any reason)
    ("cover_end", re.compile(r"cover token=(\d+) phase=([456]) attempt=\d+ event=phase reason=([^;]*)")),
    ("cover_end", re.compile(r"cover token=(\d+) phase=([456]) attempt=\d+ event=(end|done|release)(?![A-Za-z0-9_])")),
]
# A terminal event provides chronology only; cover_intervals validates lifecycle correlation.


def lines_with_time(path):
    """(t seconds since the first line's midnight, message, line number). Handles one midnight wrap."""
    out, day, prev = [], 0.0, None
    with open(path, encoding="utf-8", errors="replace") as f:
        for no, ln in enumerate(f, 1):
            m = TS.match(ln.rstrip("\n"))
            if not m:
                continue
            t = int(m[1]) * 3600 + int(m[2]) * 60 + float(m[3])
            if prev is not None and t + day < prev - 43200:
                day += 86400.0
            prev = t + day
            out.append((t + day, m[4], no))
    return out


def clock(t):
    t %= 86400.0
    return "%02d:%02d:%06.3f" % (int(t // 3600), int(t % 3600 // 60), t % 60)


def parse_clock(s):
    h, m, sec = s.split(":")
    return int(h) * 3600 + int(m) * 60 + float(sec)


def v(x, src):
    return {"s": None if x is None else round(x, 3), "src": src}


def parse_engine(path):
    loads, ac_cfg = {}, None
    ac_lines = []
    L = lambda n: loads.setdefault(n, {"n": n, "gates": {}, "ticks": {}, "rp": {}, "lines": {}})
    for t, msg, no in lines_with_time(path):
        if (m := R_ACCFG.search(msg)):
            ac_cfg = int(m[1])
        elif (m := R_AC.search(msg)):
            ac_lines.append((t, no))
        elif (m := R_SET.search(msg)):
            d = L(int(m[1])); d["t_set"] = t; d["lines"]["set"] = no
        elif (m := R_OWNER.search(msg)):
            L(int(m[1]))["owner"] = m[2]
        elif (m := R_SUM.search(msg)):
            d = L(int(m[1])); d["t_clear"] = t; d["lines"]["summary"] = no
            d["window"] = float(m[2]); d["frames"] = int(m[3]); d["cpu"] = float(m[4]); d["stage_ready"] = float(m[5])
            d["busyA"] = float(m[6]); d["busyB"] = float(m[7]); d["loader_last"] = float(m[9])
        elif (m := R_IO.search(msg)):
            d = L(int(m[1])); d["io"] = {"archive_MB": int(m[2]), "archive_calls": int(m[3]), "archive_incall_ms": int(m[4]),
                                         "other_MB": int(m[5]), "other_calls": int(m[6]), "other_incall_ms": int(m[7])}
        elif (m := R_LG.search(msg)):
            d = L(int(m[1])); d["lines"]["gates"] = no
            d["evals"] = int(m[2]); d["full_pass"] = int(m[3]); d["last_fail_eval"] = float(m[4])
            d["first_pass_eval"] = float(m[5]); d["last_eval"] = float(m[6]); d["lg_window"] = float(m[7])
        elif (m := R_G.search(msg)):
            L(int(m[1]))["gates"][m[2]] = {"name": m[3], "calls": int(m[4]), "failed": int(m[5]), "first_failing": int(m[6]),
                                            "held": float(m[7]), "first_fail": float(m[8]), "last_fail": float(m[9]),
                                            "last_flip": float(m[10])}
        elif (m := R_HOLD.search(msg)):
            L(int(m[1]))["holder"] = (m[2], float(m[3]))
        elif (m := R_TA.search(msg)):
            tk = L(int(m[1]))["ticks"].setdefault(int(m[2]), {})
            tk.update(frames=int(m[3]), cpu=float(m[4]), a_act=int(m[5]), read=float(m[6]))
        elif (m := R_TB.search(msg)):
            tk = L(int(m[1]))["ticks"].setdefault(int(m[2]), {})
            tk.update(first_failing=m[3], evals=int(m[4]))
        elif (m := R_RP.search(msg)):
            L(int(m[1]))["rp"][m[2]] = {"at": float(m[3]) if m[3] else None, "s": float(m[4]), "calls": int(m[5]),
                                        "MB": int(m[6]), "incall": float(m[7])}
    # AutoContinue answers fall inside the window they belong to
    for d in loads.values():
        if "t_set" in d and "t_clear" in d:
            hits = [(t, no) for t, no in ac_lines if d["t_set"] <= t <= d["t_clear"] + 0.05]
            if hits:
                d["ac_answer"] = hits[0]
    return [loads[k] for k in sorted(loads)], ac_cfg


def analyze(d, markers, events=()):
    """Phase split of one complete window. Returns a dict of tagged values."""
    r = {"n": d["n"], "set": clock(d["t_set"]), "clear": clock(d["t_clear"]), "owner": d.get("owner", "unknown"),
         "window": v(d["window"], "log"), "lines": d["lines"]}
    gates = d["gates"]
    if "last_eval" not in d or d["evals"] == 0:
        r["note"] = "no LoadGates data (LoadGates=0 or no evaluation): phases unknown"
        r["pre_gate"] = r["active"] = r["post_eval"] = v(None, "unknown")
        return r
    W, last_eval = d["window"], d["last_eval"]
    # Earliest observed gate failure is an upper bound on first evaluation:
    # zero full-pass evals does not exclude earlier unhooked/no-gate evaluations.
    ff = [g["first_fail"] for g in gates.values() if g["first_fail"] != NEVER]
    if d["full_pass"] == 0 and ff:
        first_eval, fe_src = min(ff), "log (earliest observed gate failure; earlier uncredited evals not excluded)"
    else:
        first_eval, fe_src = None, "unknown (full-pass evals exist; first eval not printed)"
        tk = [k for k, x in sorted(d["ticks"].items()) if x.get("evals", 0) > 0]
        if tk:
            first_eval, fe_src = None, "unknown (cumulative tick counts do not locate first evaluation)"
    r["pre_gate"] = v(first_eval, fe_src)
    r["active"] = v(None if first_eval is None else last_eval - first_eval, "derived" if first_eval is not None else "unknown")
    r["last_evaluation_boundary"] = {"last_eval": v(last_eval, "log"), "loader_last_busy": v(d.get("loader_last"), "log"),
                     "readprobe_work_end": v(d["rp"].get("set->work end", {}).get("at"), "log")}
    tail = W - last_eval
    r["post_eval"] = v(tail, "derived")
    # Which gate got the tail: the last eval's first failing gate (lowest-numbered gate failing at the last failing eval)
    stale_gate = None
    if d["last_fail_eval"] != NEVER and abs(d["last_fail_eval"] - last_eval) <= 0.05:
        cand = sorted((k for k, g in gates.items() if abs(g["last_fail"] - last_eval) <= 0.05), key=lambda k: int(k[1:]))
        stale_gate = cand[0] if cand else None
    per = {}
    for k in sorted(gates, key=lambda k: int(k[1:])):
        g = gates[k]
        if g["first_failing"] == 0 and g["held"] == 0:
            continue
        held = g["held"]
        stale = min(held, tail) if k == stale_gate else 0.0
        per[k] = {"name": g["name"], "held_log": held, "stale_tail": round(stale, 2), "active_held": round(held - stale, 2),
                  "first_failing_evals": g["first_failing"], "first_fail": g["first_fail"], "last_fail": g["last_fail"]}
    r["gates"] = per
    r["stale_gate"] = stale_gate
    # Aggregates contain per-character zero returns, not the native evaluator's
    # return value/caller. Even two first-failing evals do not prove two passes.
    g9 = gates.get("G9")
    if g9 and g9["first_fail"] != NEVER:
        r["first_g9_zero"] = v(g9["first_fail"], "log (first probe-classified G9 zero; not a native pass receipt)")
        r["g9_first_failing_evals"] = g9["first_failing"]
    # Residual wall accounting only; attribution to an unhooked native gate requires its own probe.
    if first_eval is not None:
        credited = sum(g["held"] for g in gates.values())
        r["uncredited"] = v(max(0.0, (W - first_eval) - credited), "derived (window - first eval - sum held)")
        quiet = [k for k, x in sorted(d["ticks"].items()) if x.get("first_failing") == "none" and x.get("evals", 0) > 0
                 and k <= last_eval + 1.0]
        r["ticks_no_failing_gate"] = quiet
    hold = d.get("holder")
    if hold:
        act = max(per.items(), key=lambda kv: kv[1]["active_held"]) if per else None
        r["holder_log"] = "%s %.2f s" % hold
        r["holder_active"] = "%s %s %.2f s" % (act[0], act[1]["name"], act[1]["active_held"]) if act else "none"
    # Resource work after the last eval, from 1 s ticks (A act = manager A in flight, read MB in that second)
    after = [(k, x) for k, x in sorted(d["ticks"].items()) if k > last_eval + 1.0 and "read" in x]
    r["tail_ticks"] = {"n": len(after), "read_MB": round(sum(x["read"] for _, x in after), 1),
                       "a_act_max": max((x["a_act"] for _, x in after), default=None),
                       "cpu_avg": round(sum(x["cpu"] for _, x in after) / len(after), 2) if after else None,
                       "fps_avg": round(sum(x["frames"] for _, x in after) / len(after), 1) if after else None}
    stall = [k for k, x in sorted(d["ticks"].items()) if x.get("evals") == d["evals"]]
    r["evals_stalled_from_tick"] = stall[0] if stall else None
    if "work end->clear" in d["rp"]:
        rp = d["rp"]["work end->clear"]
        r["tail_reads_probe"] = {"s": rp["s"], "calls": rp["calls"], "MB": rp["MB"], "incall_thread_s": rp["incall"]}
    if "io" in d:
        r["io"] = d["io"]
    # Tip-confirm wait: only measured when something timestamps the confirm inside the tail
    tip = {"tip_confirm_wait": v(None, "unknown"), "after_confirm_to_clear": v(None, "unknown")}
    t_last = d["t_set"] + last_eval
    if "ac_answer" in d:
        ta = d["ac_answer"][0]
        tip = {"tip_confirm_wait": v(ta - t_last, "log (AutoContinue answer line %d)" % d["ac_answer"][1]),
               "after_confirm_to_clear": v(d["t_clear"] - ta, "log")}
    else:
        for name, tm in ((n, t0 + day) for n, t0, kind in markers if kind == "confirm" for day in (0.0, 86400.0)):  # markers carry no date
            if t_last <= tm <= d["t_clear"] + 0.05:
                tip = {"tip_confirm_wait": v(tm - t_last, "marker %s (external, not native)" % name),
                       "after_confirm_to_clear": v(d["t_clear"] - tm, "marker %s" % name)}
                break
    r["post_eval_split"] = tip
    # Ordered tail segments: last eval -> (mod cover end) -> (confirm: AutoContinue line or marker) -> clear
    pts = [("last eval", t_last, "log")]
    for e in events:
        if e["event"] == "cover_end" and t_last < e["t"] <= d["t_clear"]:
            pts.append(("mod cover end (%s)" % e["args"][-1], e["t"], "rematch log line %d" % e["line"]))
    if "ac_answer" in d:
        pts.append(("AutoContinue answer", d["ac_answer"][0], "log"))
    for name, tm in ((n, t0 + day) for n, t0, kind in markers for day in (0.0, 86400.0)):
        if t_last < tm <= d["t_clear"] + 0.05:
            pts.append(("marker " + name, tm, "external marker"))
    pts.sort(key=lambda p: p[1])
    pts.append(("clear", d["t_clear"], "log"))
    r["tail_segments"] = [{"from": a[0], "to": b[0], "s": round(b[1] - a[1], 2), "src": b[2]} for a, b in zip(pts, pts[1:])]
    intervals = cover_intervals(events)
    covered = [x for x in intervals if x["ack"] <= t_last < x["end"] <= d["t_clear"]]
    if len(covered) == 1:
        x = covered[0]
        r["cover_ack_to_terminal_tail"] = dict(v(x["end"] - t_last,
            "derived from same-token begin/Present ACK/native post/terminal logs; continuous rendering and tip state unobserved"),
            token=x["token"], attempt=x["attempt"])
    return r


def cover_intervals(events):
    """Accept one ordered begin/ACK/native-post/end per token; reject ambiguity.

    This is logged lifecycle chronology, never a continuous Present/visibility proof.
    """
    result = []
    for token in {e.get("token") for e in events if e.get("token")}:
        seq = [e for e in events if e.get("token") == token]
        kinds = ("cover_begin", "cover_present_ack", "cover_native_post", "cover_end")
        parts = [[e for e in seq if e["event"] == kind] for kind in kinds]
        if any(len(x) != 1 for x in parts):
            continue
        begin, ack, post, end = [x[0] for x in parts]
        if not (begin["line"] < ack["line"] < post["line"] < end["line"] and
                begin["t"] <= ack["t"] <= post["t"] <= end["t"]):
            continue
        attempt = post.get("attempt", 0)
        if not attempt or end.get("attempt") != attempt or begin.get("attempt") != 0:
            continue
        if any(e["event"] == "cover_begin" and begin["line"] < e["line"] < end["line"] for e in events):
            continue
        result.append(dict(token=token, attempt=attempt, ack=ack["t"], end=end["t"]))
    return result


def place_rematch(path, loads):
    """Rematch events with the load window they fall in / follow."""
    out = []
    for t, msg, no in lines_with_time(path):
        for key, rx in REMATCH_EV:
            m = rx.search(msg)
            if not m:
                continue
            where = "outside any 58A window"
            for d in loads:
                if "t_set" in d and "t_clear" in d:
                    if d["t_set"] <= t <= d["t_clear"]:
                        where = "inside #%d at %+.1f s" % (d["n"], t - d["t_set"])
                    elif d["t_clear"] < t:
                        where = "after #%d clear %+.1f s" % (d["n"], t - d["t_clear"])
            meta = re.search(r"cover token=(\d+) phase=(\d+) attempt=(\d+)", msg)
            out.append({"token": int(meta[1]) if meta else None, "attempt": int(meta[3]) if meta else None, "t": t, "clock": clock(t), "event": key, "args": list(m.groups()), "where": where, "line": no})
    return out


def fmt(x):
    return "unknown" if x["s"] is None else "%.1f s" % x["s"]


def report(res, ac_cfg, events, loads):
    print("AutoContinueLoad at start: %s" % ("unknown (no install line)" if ac_cfg is None else ac_cfg))
    for r in res:
        print("\n#%d  %s -> %s  window %s  owner %s" % (r["n"], r["set"], r["clear"], fmt(r["window"]), r["owner"]))
        if "note" in r:
            print("  " + r["note"]); continue
        print("  set -> earliest gate failure   %-9s [%s]" % (fmt(r["pre_gate"]), r["pre_gate"]["src"]))
        print("  gate-observation span          %-9s [%s] -> last observed eval %+.1f s (loader A last %+.1f, ReadProbe %s)" % (
            fmt(r["active"]), r["active"]["src"], r["last_evaluation_boundary"]["last_eval"]["s"], r["last_evaluation_boundary"]["loader_last_busy"]["s"] or 0,
            fmt(r["last_evaluation_boundary"]["readprobe_work_end"]) if r["last_evaluation_boundary"]["readprobe_work_end"]["s"] is not None else "n/a"))
        for k, g in r["gates"].items():
            s = "    %s %-24s active %6.2f s  (log held %6.2f" % (k, g["name"], g["active_held"], g["held_log"])
            s += ", stale tail %.2f)" % g["stale_tail"] if g["stale_tail"] else ")"
            print(s + "  first-failing evals %d" % g["first_failing_evals"])
        if "uncredited" in r:
            print("    uncredited interval (loader drain is an unhooked candidate, not measured) %s [%s]; ticks with evals and no failing gate: %s" % (
                fmt(r["uncredited"]), r["uncredited"]["src"], r["ticks_no_failing_gate"]))
        if "first_g9_zero" in r:
            print("    first G9 zero at %+.1f s (%d first-failing evals); native pass/caller unobserved" % (
                r["first_g9_zero"]["s"], r["g9_first_failing_evals"]))
        print("  holder: log says %s; without the stale tail: %s" % (r.get("holder_log", "?"), r.get("holder_active", "?")))
        tt = r["tail_ticks"]
        print("  post-eval tail                 %-9s [derived: window - last eval]; evals frozen from tick %s" % (
            fmt(r["post_eval"]), r["evals_stalled_from_tick"]))
        if tt["n"]:
            print("    tail ticks: %d s, read %.1f MB, manager A in flight max %s, CPU %s cores, %s fps" % (
                tt["n"], tt["read_MB"], tt["a_act_max"], tt["cpu_avg"], tt["fps_avg"]))
        if "tail_reads_probe" in r:
            p = r["tail_reads_probe"]
            print("    ReadProbe work end->clear: %d calls, %d MB, %.2f thread-s in reads" % (p["calls"], p["MB"], p["incall_thread_s"]))
        for sg in r.get("tail_segments", []):
            print("    %-20s -> %-40s %6.1f s [%s]" % (sg["from"], sg["to"], sg["s"], sg["src"]))
        if "cover_ack_to_terminal_tail" in r:
            print("    same-token ACK-to-terminal lifecycle overlaps tail by %.1f s; continuous visibility/tip state unobserved" %
                  r["cover_ack_to_terminal_tail"]["s"])
        sp = r["post_eval_split"]
        print("    tip confirm wait %s [%s]; confirm -> clear %s" % (fmt(sp["tip_confirm_wait"]), sp["tip_confirm_wait"]["src"],
                                                                    fmt(sp["after_confirm_to_clear"])))
    done = [d for d in loads if "t_set" in d and "t_clear" in d]
    for a, b in zip(done, done[1:]):
        print("\ngap #%d clear -> #%d set: %.1f s (no 58A)" % (a["n"], b["n"], b["t_set"] - a["t_clear"]), end="")
    print()
    if events:
        t0 = next((e["t"] for e in events if e["event"] == "request"), events[0]["t"])
        print("\nrematch events (s from the first request):")
        for e in events:
            print("  %+7.1f  %s  %-24s %-28s %s" % (e["t"] - t0, e["clock"], e["event"], ",".join(e["args"]), e["where"]))


def main(argv):
    if len(argv) < 2 or argv[1] in ("-h", "--help"):
        print(__doc__); return 0
    rematch, markers, jout = None, [], None
    confirm_names = set()
    i = 2
    while i < len(argv):
        a = argv[i]
        if a == "--rematch": rematch = argv[i + 1]; i += 2
        elif a == "--json": jout = argv[i + 1]; i += 2
        elif a == "--markers":
            for ln in open(argv[i + 1], encoding="utf-8", errors="replace"):
                m = re.match(r"^\s*([A-Za-z][\w-]*)\s+(\d+:\d+:\d+(?:\.\d+)?)\s*$", ln)
                if m: markers.append((m[1], parse_clock(m[2]), "annotation"))
            i += 2
        elif a == "--confirm-name":
            confirm_names.add(argv[i + 1]); i += 2
        elif a in ("--marker", "--confirm-marker"):
            name, tm = argv[i + 1].split("=", 1); markers.append((name, parse_clock(tm), "confirm" if a == "--confirm-marker" else "annotation")); i += 2
        else: sys.exit("unknown arg " + a)
    markers = [(n, t, "confirm" if n in confirm_names else kind) for n, t, kind in markers]
    loads, ac_cfg = parse_engine(argv[1])
    events = place_rematch(rematch, loads) if rematch else []
    res = [analyze(d, markers, events) for d in loads if "t_set" in d and "t_clear" in d]
    open_w = [d["n"] for d in loads if "t_set" in d and "t_clear" not in d]
    report(res, ac_cfg, events, loads)
    if open_w:
        print("open windows (set, no clear in the log): %s" % open_w)
    if jout:
        with open(jout, "w", encoding="utf-8") as f:
            json.dump({"source": argv[1], "auto_continue": ac_cfg, "loads": res, "open_windows": open_w,
                       "rematch_events": events}, f, indent=1)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
