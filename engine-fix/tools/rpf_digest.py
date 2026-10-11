"""rpf_digest.py - digest the RPF replay-profiler lines (and BKS lines) of a Nioh3EngineFix log. stdlib only.

    py -I tools/rpf_digest.py <Nioh3EngineFix.log | mod_run01.log> [--top 30] [--api 25] [--md out.md]

1. RPF windows: frame-weighted mean over every report window whose fps is within 25 % of the median (drops title / loading
   windows): replay-thread split (wall = handlers [self CPU / D3D12 / waits] + outside handlers + idle), top handlers by self
   time, call groups, top D3D12 methods. A handler / method missing from a window's top list counts 0 there.
2. Overhead (on vs off): BKS windows are assigned to RPF on / off by the last 'LIVE tuning: RPF = n' line at least one BKS
   period before the window end; per arm: fps, replay-thread busy ms/f = (1 - starved) x frame time, drain ms/f.
"""
import argparse
import re
import statistics
import sys
from collections import defaultdict

ap = argparse.ArgumentParser()
ap.add_argument("log")
ap.add_argument("--top", type=int, default=30)
ap.add_argument("--api", type=int, default=25)
ap.add_argument("--md")
a = ap.parse_args()

TS = re.compile(r"^\[(\d\d):(\d\d):(\d\d)\.(\d+)\] (.*)$")
HDR = re.compile(r"RPF: ([\d.]+) s, (\d+) frames \(([\d.]+)/s by token 95\); replay thread ms/frame: wall ([\d.]+) = handlers ([\d.]+) "
                 r"\[self CPU ([-\d.]+), D3D12 ([\d.]+), waits ([\d.]+)\] \+ outside handlers ([-\d.]+) \[D3D12 ([\d.]+), waits ([\d.]+)\] "
                 r"\+ idle \(waiting for kicks\) ([\d.]+) in ([\d.]+) waits; (\d+) tokens/f, (\d+) D3D12 calls/f; est. probe cost ([\d.]+) ms/f")
TOK = re.compile(r"RPF tok +\d+ ([0-9A-F]{2}) (\S+) +n/f +([\d.]+) self +([-\d.]+) \( *[-\d.]+ %\) incl +([\d.]+) cpu +([-\d.]+) "
                 r"d3d +([\d.]+) wait +([\d.]+) ms/f, +(\d+) ns/call(.*)$")
API = re.compile(r"RPF api +\d+ (\S+) +([\d.]+) ms/f +n/f +([\d.]+) +(\d+) ns/call")
GRP = re.compile(r"^(.+) ([\d.]+) \((\d+)\)$")
BKS1 = re.compile(r"BKS: (\d+) s \| frames (\d+) \(([\d.]+)/s\).*\| drain ([\d.]+) ms/f")
BKS2 = re.compile(r"BKS: disp: starved-while-recording ([\d.]+) % .*\| starved ([\d.]+) %")
LIVE = re.compile(r"LIVE tuning: RPF = (\d)")

wins = []
cur = None
bks = []
toggles = []
pend_bks = None
for raw in open(a.log, encoding="utf-8", errors="replace"):
    m = TS.match(raw.rstrip("\n"))
    if not m:
        continue
    t = int(m.group(1)) * 3600 + int(m.group(2)) * 60 + int(m.group(3)) + float("0." + m.group(4))
    s = m.group(5)
    if s.startswith("RPF: ") and "frames (" in s:
        h = HDR.match(s)
        if h:
            g = [float(x) for x in h.groups()]
            cur = dict(t=t, sec=g[0], F=g[1], fps=g[2], wall=g[3], handlers=g[4], cpu=g[5], d3d=g[6], waits=g[7], outside=g[8],
                       out_d3d=g[9], out_waits=g[10], idle=g[11], idle_n=g[12], tokens=g[13], calls=g[14], probe=g[15],
                       tok={}, api={}, grp={})
            wins.append(cur)
        continue
    if cur is not None and s.startswith("RPF tok"):
        k = TOK.match(s)
        if k:
            cur["tok"][k.group(1)] = dict(name=k.group(2), n=float(k.group(3)), self=float(k.group(4)), incl=float(k.group(5)),
                                          cpu=float(k.group(6)), d3d=float(k.group(7)), wait=float(k.group(8)), groups=k.group(10).strip())
        continue
    if cur is not None and s.startswith("RPF api"):
        k = API.match(s)
        if k:
            cur["api"][k.group(1)] = dict(ms=float(k.group(2)), n=float(k.group(3)))
        continue
    if cur is not None and s.startswith("RPF groups"):
        for item in s.split(":", 1)[1].split(";"):
            k = GRP.match(item.strip())
            if k:
                cur["grp"][k.group(1).strip()] = dict(ms=float(k.group(2)), n=float(k.group(3)))
        continue
    k = LIVE.search(s)
    if k:
        toggles.append((t, int(k.group(1))))
        continue
    k = BKS1.match(s)
    if k:
        pend_bks = dict(t=t, sec=float(k.group(1)), F=float(k.group(2)), fps=float(k.group(3)), drain=float(k.group(4)))
        continue
    k = BKS2.match(s)
    if k and pend_bks:
        pend_bks["starved"] = float(k.group(2))
        pend_bks["starved_rec"] = float(k.group(1))
        bks.append(pend_bks)
        pend_bks = None

out = []
P = out.append
if wins:
    med = statistics.median(w["fps"] for w in wins)
    sel = [w for w in wins if abs(w["fps"] - med) <= 0.25 * med]
    TF = sum(w["F"] for w in sel)
    avg = lambda key: sum(w[key] * w["F"] for w in sel) / TF
    P("RPF: %d of %d windows (fps within 25 %% of median %.1f), %d frames, %.0f s" % (len(sel), len(wins), med, TF, sum(w["sec"] for w in sel)))
    P("")
    P("| replay thread per frame | ms/f |")
    P("|---|---|")
    for key, lab in [("wall", "wall (frame time)"), ("handlers", "inside token handlers"), ("cpu", "  handler CPU (no D3D12, no waits)"),
                     ("d3d", "  D3D12 calls in handlers"), ("waits", "  waits / Sleep in handlers"), ("outside", "outside handlers (loop, close/submit)"),
                     ("out_d3d", "  D3D12 calls outside handlers"), ("out_waits", "  waits outside handlers"), ("idle", "idle (waiting for main's kicks)"),
                     ("probe", "est. min probe cost (in the above)")]:
        P("| %s | %.2f |" % (lab, avg(key)))
    P("| tokens / D3D12 calls per frame | %.0f / %.0f |" % (avg("tokens"), avg("calls")))
    P("")
    agg = defaultdict(lambda: defaultdict(float))
    names = {}
    groups = {}
    for w in sel:
        for op, r in w["tok"].items():
            names[op] = r["name"]
            for f in ("n", "self", "incl", "cpu", "d3d", "wait"):
                agg[op][f] += r[f] * w["F"]
            groups.setdefault(op, r["groups"])
    rows = sorted(agg.items(), key=lambda kv: -kv[1]["self"])[: a.top]
    P("| # | op | handler | n/f | self ms/f | CPU | D3D12 | wait | incl | top call groups (one window) |")
    P("|---|---|---|---|---|---|---|---|---|---|")
    for i, (op, r) in enumerate(rows):
        P("| %d | %s | %s | %.1f | %.3f | %.3f | %.3f | %.3f | %.3f | %s |" % (i + 1, op, names[op], r["n"] / TF, r["self"] / TF, r["cpu"] / TF,
                                                                      r["d3d"] / TF, r["wait"] / TF, r["incl"] / TF, groups[op]))
    P("")
    ga = defaultdict(lambda: [0.0, 0.0])
    for w in sel:
        for g, r in w["grp"].items():
            ga[g][0] += r["ms"] * w["F"]
            ga[g][1] += r["n"] * w["F"]
    P("| call group | ms/f | calls/f | ns/call |")
    P("|---|---|---|---|")
    for g, (ms, n) in sorted(ga.items(), key=lambda kv: -kv[1][0]):
        P("| %s | %.3f | %.0f | %.0f |" % (g, ms / TF, n / TF, ms * 1e6 / n if n else 0))
    P("")
    aa = defaultdict(lambda: [0.0, 0.0])
    for w in sel:
        for k, r in w["api"].items():
            aa[k][0] += r["ms"] * w["F"]
            aa[k][1] += r["n"] * w["F"]
    P("| D3D12 method | ms/f | calls/f | ns/call |")
    P("|---|---|---|---|")
    for k, (ms, n) in sorted(aa.items(), key=lambda kv: -kv[1][0])[: a.api]:
        P("| %s | %.3f | %.0f | %.0f |" % (k, ms / TF, n / TF, ms * 1e6 / n if n else 0))
    P("")
if bks:
    arms = defaultdict(list)
    for b in bks:
        st = None
        for t, v in toggles:
            if t <= b["t"] - b["sec"]:
                st = v
        if st is None and toggles:
            continue
        arms["on" if st == 1 else "off" if st == 0 else "boot"].append(b)
    P("| BKS arm | windows | fps | frame ms | replay busy ms/f (1 - starved) | starved % | drain ms/f |")
    P("|---|---|---|---|---|---|---|")
    for arm, L in sorted(arms.items()):
        med = statistics.median(x["fps"] for x in L)
        L = [x for x in L if abs(x["fps"] - med) <= 0.25 * med]
        F = sum(x["F"] for x in L)
        if not F:
            continue
        fps = F / sum(x["sec"] for x in L)
        busy = sum((1 - x["starved"] / 100) * 1000.0 / x["fps"] * x["F"] for x in L) / F
        P("| RPF %s | %d | %.2f | %.2f | %.2f | %.1f | %.2f |" % (arm, len(L), fps, 1000 / fps, busy,
                                                           sum(x["starved"] * x["F"] for x in L) / F, sum(x["drain"] * x["F"] for x in L) / F))
txt = "\n".join(out)
print(txt)
if a.md:
    open(a.md, "w", encoding="utf-8").write(txt + "\n")
