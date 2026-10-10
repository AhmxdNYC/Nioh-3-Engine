"""cbu_digest.py - per-arm RPF rows for the CBU switches (CBS / CBN / CBT / CBALL / CBSTATS) from a Nioh3EngineFix log. stdlib only.

    py -I tools/cbu_digest.py <Nioh3EngineFix.log | mod_run01.log>

Each RPF report window (end time t, length s) is assigned to the CBU state that held over the whole window (state from the
'LIVE tuning: CBx = n' lines; windows that straddle a switch are dropped). Per state, frame-weighted: fps (RPF token 95),
replay handlers ms/f, 6A / 69 / 7E handler self / cpu / d3d ms/f, n/f and ns/call (6A: us/draw of CPU), D3D12 methods
CreateConstantBufferView / CopyDescriptorsSimple / SetGraphicsRootDescriptorTable. Also the 'CBU:' census lines.
"""
import re
import sys
from collections import defaultdict

TS = re.compile(r"^\[(\d\d):(\d\d):(\d\d)\.(\d+)\] (.*)$")
LIVE = re.compile(r"LIVE tuning: (CBS_FENCE|CBSTATS|CBMODE|CBALL|CBF_QUEUE|CBF|CBS|CBN|CBT) = (\d+)")
HDR = re.compile(r"RPF: ([\d.]+) s, (\d+) frames \(([\d.]+)/s by token 95\); replay thread ms/frame: wall ([\d.]+) = handlers ([\d.]+) "
                 r"\[self CPU ([-\d.]+), D3D12 ([\d.]+)")
TOK = re.compile(r"RPF tok +\d+ ([0-9A-F]{2}) (\S+) +n/f +([\d.]+) self +([-\d.]+) \( *[-\d.]+ %\) incl +([\d.]+) cpu +([-\d.]+) "
                 r"d3d +([\d.]+) wait +([\d.]+) ms/f, +(\d+) ns/call")
API = re.compile(r"RPF api +\d+ (\S+) +([\d.]+) ms/f +n/f +([\d.]+) +(\d+) ns/call")
CBU = re.compile(r"CBU: ([\d.]+) s \| switches CBS (\d) CBN (\d) CBT (\d)(?: fence (\d))? \| (.*)$")
TOKS = ("6A", "69", "7E")
APIS = ("dev.CreateConstantBufferView", "dev.CopyDescriptorsSimple", "list.SetGraphicsRootDescriptorTable")


def secs(m):
    return int(m.group(1)) * 3600 + int(m.group(2)) * 60 + int(m.group(3)) + float("0." + m.group(4))


def main(path):
    state = {"CBS": None, "CBN": None, "CBT": None, "CBSTATS": None, "CBS_FENCE": 1, "CBF": 0, "CBF_QUEUE": 0}
    changes = []  # (t, state copy)
    wins = []
    cur = None
    cbu = defaultdict(list)
    day = 0.0
    last_t = -1.0
    for line in open(path, encoding="utf-8", errors="replace"):
        m = TS.match(line.rstrip("\n"))
        if not m:
            continue
        t = secs(m) + day
        if t < last_t - 3600:
            day += 86400
            t += 86400
        last_t = t
        body = m.group(5)
        lv = LIVE.search(body)
        if lv:
            k, v = lv.group(1), int(lv.group(2))
            if k == "CBALL":
                for kk in ("CBS", "CBN", "CBT"):
                    state[kk] = v
            elif k == "CBMODE":
                state.update(CBS=1 if v & 9 else 0, CBN=1 if v & 2 else 0, CBT=1 if v & 4 else 0, CBSTATS=1 if v & 16 else 0,
                             CBS_FENCE=0 if v & 8 else 1, CBF=1 if v & 32 else 0, CBF_QUEUE=1 if v & 64 else 0)
            else:
                state[k] = v
            changes.append((t, dict(state)))
            continue
        h = HDR.search(body)
        if h:
            cur = {"t": t, "len": float(h.group(1)), "frames": int(h.group(2)), "fps": float(h.group(3)), "handlers": float(h.group(5)),
                   "tok": {}, "api": {}}
            wins.append(cur)
            continue
        tk = TOK.search(body)
        if tk and cur is not None and tk.group(1) in TOKS:
            cur["tok"][tk.group(1)] = dict(nf=float(tk.group(3)), self=float(tk.group(4)), cpu=float(tk.group(6)), d3d=float(tk.group(7)),
                                           ns=float(tk.group(9)))
            continue
        ap = API.search(body)
        if ap and cur is not None and ap.group(1) in APIS:
            cur["api"][ap.group(1)] = dict(ms=float(ap.group(2)), nf=float(ap.group(3)), ns=float(ap.group(4)))
            continue
        cb = CBU.search(body)
        if cb:
            st = "CBS %s CBN %s CBT %s fence %s" % (cb.group(2), cb.group(3), cb.group(4), cb.group(5))
            cbu[st].append(cb.group(6))

    def state_at(t):
        s = None
        for ct, cs in changes:
            if ct <= t:
                s = cs
            else:
                break
        return s

    arms = defaultdict(list)
    for w in wins:
        t0 = w["t"] - w["len"]
        s0, s1 = state_at(t0 - 0.5), state_at(w["t"])
        if s0 is None or s0 != s1:
            continue
        key = "CBS %s fence %s CBN %s CBT %s stats %s CBF %s q %s" % (s1["CBS"], s1["CBS_FENCE"], s1["CBN"], s1["CBT"], s1["CBSTATS"], s1["CBF"], s1["CBF_QUEUE"])
        arms[key].append(w)
    print("RPF windows per CBU state (frame-weighted; windows straddling a switch dropped)")
    for key in sorted(arms):
        ws = arms[key]
        fr = sum(w["frames"] for w in ws)
        if not fr:
            continue
        wm = lambda f: sum(f(w) * w["frames"] for w in ws) / fr
        print("  %-38s windows %2d frames %6d | fps %.1f | handlers %.3f ms/f" % (key, len(ws), fr, wm(lambda w: w["fps"]), wm(lambda w: w["handlers"])))
        for tk in TOKS:
            g = lambda f, tk=tk: wm(lambda w: f(w["tok"].get(tk, dict(nf=0, self=0, cpu=0, d3d=0, ns=0))))
            nf = g(lambda d: d["nf"])
            cpu = g(lambda d: d["cpu"])
            print("      %s n/f %7.1f self %.3f cpu %.3f d3d %.3f ms/f | cpu %.0f ns/call" % (tk, nf, g(lambda d: d["self"]), cpu,
                                                                                         g(lambda d: d["d3d"]), cpu * 1e6 / nf if nf else 0))
        for a in APIS:
            g = lambda f, a=a: wm(lambda w: f(w["api"].get(a, dict(ms=0, nf=0, ns=0))))
            ms, nf = g(lambda d: d["ms"]), g(lambda d: d["nf"])
            print("      %-38s %.3f ms/f n/f %7.1f %4.0f ns/call" % (a, ms, nf, ms * 1e6 / nf if nf else 0))
    if cbu:
        print("CBU census lines")
        for st, ls in cbu.items():
            print("  %s: %d lines; last: %s" % (st, len(ls), ls[-1]))


if __name__ == "__main__":
    main(sys.argv[1])
