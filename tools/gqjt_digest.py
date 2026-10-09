"""gqjt_digest.py - digest the GQ (graphics-queue gap census) and JT (job-timing probe) lines of a Nioh3EngineFix.log
(ASI >= 1.7.40) and print the decisions of notes/gq-jt-probes.md (build R2 / merge lists; reorder / split jobs).

    py -I tools\\gqjt_digest.py <Nioh3EngineFix.log> [--symbols re\\symbols\\symbols.tsv] [--skip 1]
    py -I tools\\gqjt_digest.py --selftest

RVAs in the JT / GQ lines are return addresses (or job function / callback starts); they are named with the function that
contains them (symbols.tsv, kind = function). --skip N drops the first N windows of each kind (warm-up / loading).
"""
import bisect
import pathlib
import re
import statistics
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
PFX = re.compile(r"^(?:\[[0-9:.]+\]\s*)?")


def load_symbols(path):
    starts, names = [], []
    p = pathlib.Path(path)
    if not p.exists():
        return starts, names
    rows = []
    for line in p.read_text(encoding="utf-8", errors="replace").splitlines():
        f = line.split("\t")
        if len(f) > 2 and f[0].startswith("0x") and f[1] == "function":
            try:
                rows.append((int(f[0], 16), f[2]))
            except ValueError:
                pass
    rows.sort()
    return [r[0] for r in rows], [r[1] for r in rows]


class Names:
    def __init__(self, path):
        self.s, self.n = load_symbols(path)

    def __call__(self, hexrva):
        if hexrva in ("-", "ext", ""):
            return hexrva
        try:
            r = int(hexrva, 16)
        except ValueError:
            return hexrva
        i = bisect.bisect_right(self.s, r) - 1
        if i < 0:
            return hexrva
        return "%s(%s+%X)" % (hexrva, self.n[i], r - self.s[i])

    def chain(self, txt):   # "A < B < C" or "FN/INNER"
        return re.sub(r"\b([0-9A-F]{4,8})\b", lambda m: self(m.group(1)), txt)


def num(pat, s, default=None, cast=float):
    m = re.search(pat, s)
    return cast(m.group(1)) if m else default


def parse(lines):
    gq, jt = [], []
    for raw in lines:
        s = PFX.sub("", raw.rstrip())
        if s.startswith("GQ: frame: "):
            gq.append({"line": s, "lists": num(r"frame: ([\d.]+) DIRECT", s), "ecl": num(r"\(([\d.]+) % with ECL", s),
                       "frames": num(r"over (\d+) sampled", s, 0, int), "gaps": num(r"GAPS ([\d.]+) ms/frame", s),
                       "ngaps": num(r"\(([\d.]+) gaps/frame", s), "p50": num(r"p50 ([\d.]+)", s), "p90": num(r"p90 ([\d.]+)", s),
                       "unk": num(r"unmeasured submissions ([\d.]+) ms", s), "heads": num(r"list HEADS ([\d.]+)", s),
                       "nocov": num(r"without GP2 coverage ([\d.]+)", s), "uncov": num(r"\(span - covered\) ([\d.]+) ms", s),
                       "edges": [], "worst": []})
        elif s.startswith("GQ:   ") and gq:
            m = re.match(r"GQ:\s+\d+ \[(.*) -> (.*)\] \| ([\d.]+) ms \| ([\d.]+) \| ([\d.]+) \| ([\d.]+) \| (.*)$", s)
            if m:
                gq[-1]["edges"].append({"prev": m.group(1), "next": m.group(2), "ms": float(m.group(3)), "n": float(m.group(4)),
                                        "mean": float(m.group(5)), "max": float(m.group(6)), "ecl": m.group(7)})
            elif " us | " in s:
                gq[-1]["worst"].append(s[3:].strip())
        elif s.startswith("JT: window "):
            jt.append({"line": s, "ticks": num(r"logic ticks (\d+)", s, 0, int), "waits": num(r"\(([\d.]+)/tick\), blocked", s),
                       "blocked": num(r"blocked ([\d.]+) ms/tick", s), "p50": num(r"per tick p50 ([\d.]+)", s),
                       "p90": num(r" p90 ([\d.]+) max", s), "sites": [], "jobs": []})
        elif s.startswith("JT:      critical job") and jt and jt[-1]["sites"]:
            jt[-1]["sites"][-1]["crit"] = s
        elif s.startswith("JT:      longest job") and jt and jt[-1]["sites"]:
            jt[-1]["sites"][-1]["long"] = s
        elif s.startswith("JT:   ") and jt:
            m = re.match(r"JT:\s+\d+ \[(.*?)\] \| ([\d.]+) \| ([\d.]+) \| ([\d.]+) / ([\d.]+) / ([\d.]+) \| (.*?) \| (.*)$", s)
            if m:
                jt[-1]["sites"].append({"site": m.group(1), "ms": float(m.group(2)), "n": float(m.group(3)), "p50": float(m.group(4)),
                                        "p90": float(m.group(5)), "rel": m.group(7), "relsite": m.group(8), "crit": "", "long": ""})
                continue
            m = re.match(r"JT:\s+\d+ (\S+/\S+) \| ([\d.]+) \| ([\d.]+) \| ([\d.]+) / ([\d.]+) / ([\d.]+) \| (\d+) \| (.*)$", s)
            if m:
                jt[-1]["jobs"].append({"fn": m.group(1), "ms": float(m.group(2)), "n": float(m.group(3)), "p50": float(m.group(4)),
                                       "p90": float(m.group(5)), "max": float(m.group(6)), "thr": int(m.group(7)), "split": m.group(8)})
    return gq, jt


def med(v):
    v = [x for x in v if x is not None]
    return statistics.median(v) if v else float("nan")


def digest_gq(gq, nm, out):
    out("GQ: %d window(s)" % len(gq))
    if not gq:
        return
    g, h, u, unk, nc = (med([w[k] for w in gq]) for k in ("gaps", "heads", "uncov", "unk", "nocov"))
    out("  per frame (median of windows): GAPS %.3f ms, heads %.3f ms, unmeasured-submission gaps %.3f ms, no-coverage %.3f ms, "
        "lists %.1f (ECL info %.0f %%) | GP2 uncovered %.3f ms" % (g, h, unk, nc, med([w["lists"] for w in gq]), med([w["ecl"] for w in gq]), u))
    closure = abs(u - (g + h + unk + nc)) <= 0.15 + 0.10 * u if u == u else False
    out("  closure (GP2 uncovered == gaps + heads + unmeasured + no-cov): %s" % ("OK" if closure else "NOT closed - lists missing "
        "from the census (other DIRECT queues / straddling lists): read the gap numbers as a lower bound"))
    agg = {}
    for w in gq:
        for e in w["edges"]:
            a = agg.setdefault((e["prev"], e["next"]), {"ms": 0.0, "n": 0.0, "mean": [], "ecl": e["ecl"]})
            a["ms"] += e["ms"] / len(gq)
            a["n"] += e["n"] / len(gq)
            a["mean"].append(e["mean"])
    top = sorted(agg.items(), key=lambda kv: -kv[1]["ms"])[:8]
    out("  top edges over all windows [prev last marker -> next first marker]: ms/frame | gaps/frame | mean us | ECL site")
    struct_ms = 0.0
    for (p, n), a in top:
        mean = med(a["mean"])
        structural = a["n"] >= 0.9 and mean <= 150
        struct_ms += a["ms"] if structural else 0
        out("    [%s -> %s] %.3f | %.2f | %.0f | %s%s" % (p, n, a["ms"], a["n"], mean, nm.chain(a["ecl"]),
                                                    "  <- per-frame submission gap" if structural else ""))
    for w in gq[-1:]:
        for s in w["worst"][:5]:
            out("    worst (last window): " + nm.chain(s))
    out("  DECISION merge lists (ECL merge, vkd3d-audit design 3): %s" % (
        "BUILD - per-frame submission gaps (>= 0.9/frame, mean <= 150 us) sum to %.3f ms/frame >= 0.3" % struct_ms if struct_ms >= 0.3 else
        "no - per-frame submission gaps sum to %.3f ms/frame (< 0.3); gaps elsewhere are CPU-late submissions (fix the producer, not the queue)" % struct_ms))
    out("  DECISION R2a / R1 (list-head barriers): %s" % (
        "BUILD/ABBA - list heads %.3f ms/frame >= 0.2 (and check B6: COMMON->X mid-list >= 50/frame in the trace)" % h if h >= 0.2 else
        "heads %.3f ms/frame < 0.2: R2a's list-head share is small; decide on the trace's mid-list counts (B6) alone" % h))


def digest_jt(jt, nm, out):
    out("JT: %d window(s)" % len(jt))
    if not jt:
        return
    out("  blocked per tick (median of windows) %.3f ms, waits/tick %.2f, per-tick p90 %.3f ms" % (
        med([w["blocked"] for w in jt]), med([w["waits"] for w in jt]), med([w["p90"] for w in jt])))
    w = max(jt, key=lambda x: x["ticks"])   # the longest window
    for k, s in enumerate(w["sites"][:3]):
        out("  site %d [%s]: %.3f ms/tick, %.2f waits/tick, p50 %.0f us" % (k + 1, nm.chain(s["site"]), s["ms"], s["n"], s["p50"]))
        out("    released by: %s | waker site %s" % (s["rel"], nm.chain(s["relsite"])))
        out("    " + nm.chain(s["crit"].replace("JT:      ", "")))
        out("    " + nm.chain(s["long"].replace("JT:      ", "")))
    out("  jobs by total time:")
    for j in w["jobs"][:8]:
        out("    %s | %.3f ms/tick | %.1f/tick | p50 %.0f p90 %.0f max %.0f us | %d thr | %s" % (
            nm.chain(j["fn"]), j["ms"], j["n"], j["p50"], j["p90"], j["max"], j["thr"], j["split"]))
    if not w["sites"]:
        out("  DECISION: no waits of the watched thread recorded (thread not found? see the 'JT: watching' line)")
        return
    s = w["sites"][0]
    rel = s["rel"]
    jobThreads = rel.startswith("KIDSTaskScheduler") or rel.startswith("KIDSTS") or rel.startswith("Collision") or rel.startswith("StageUpdate") or rel.startswith("CharacterUpdate")
    m = re.search(r"1\) (\S+) ([\d.]+) % of the waits, job p50 ([\d.]+) p90 ([\d.]+) us, started before the wait in ([\d.]+) % "
                  r"\(p50 ([\d.]+) us before\), else p50 ([\d.]+) us after", s["crit"])
    busy = num(r"busy ([\d.]+) % of the wait", s["long"], 0.0)
    split = None
    if m:
        fn, share, p50, before, after = m.group(1), float(m.group(2)), float(m.group(3)), float(m.group(5)), float(m.group(7))
        isSplit = fn.startswith("204584/") or fn.startswith("1753B0/")
        if not jobThreads:
            split = "NOT a job join: the top site is released by %s - back-pressure / effect join; reorder or split jobs cannot help it" % rel
        elif share >= 50 and before < 50 and after >= 200:
            split = ("REORDER - %s releases %.0f %% of the waits and starts p50 %.0f us after the wait began: kick it first (JG reorder); "
                     "expected <= %.0f us/tick" % (nm(fn.split('/')[0]), share, after, min(after, s["ms"] * 1000)))
        elif share >= 50 and before >= 50 and p50 >= 500 and busy < 50:
            split = ("SPLIT - %s (p50 %.0f us) is already running when the wait begins and the other job threads are only %.0f %% busy: %s"
                     % (nm.chain(fn), p50, busy, "smaller grain at the fan-out (JG, job+78h / +50h)" if isSplit else
                        "split / parallelise its body (RE), or start it earlier in the frame"))
        elif busy >= 80:
            split = "THROUGHPUT - other job threads %.0f %% busy during the wait: no reorder / split gain; cut job work (AABB, BP, ...)" % busy
        else:
            split = "MIXED - critical job %s at %.0f %% of the waits, threads %.0f %% busy: read the tables (no single lever)" % (nm.chain(fn), share, busy)
    else:
        split = "no critical job (releaser outside JobWorkerPoll jobs): %s" % rel
    out("  DECISION reorder / split jobs: " + split)


def main(argv):
    if "--selftest" in argv:
        return selftest()
    if len(argv) < 2:
        print(__doc__)
        return 2
    sym = ROOT / "re" / "symbols" / "symbols.tsv"
    if not sym.exists():
        sym = pathlib.Path(r"C:\Users\Ahmad Hamza\nioh3-engine\re\symbols\symbols.tsv")
    skip = 0
    for i, a in enumerate(argv):
        if a == "--symbols":
            sym = pathlib.Path(argv[i + 1])
        if a == "--skip":
            skip = int(argv[i + 1])
    lines = pathlib.Path(argv[1]).read_text(encoding="utf-8", errors="replace").splitlines()
    gq, jt = parse(lines)
    nm = Names(sym)
    digest_gq(gq[skip:], nm, print)
    digest_jt(jt[skip:], nm, print)
    return 0


def selftest():
    log = """[10:00:00.000] GQ: frame: 24 DIRECT lists/frame (100 % with ECL info) over 110 sampled frames; span 28.900 ms, lists busy 27.200 ms, GAPS 1.100 ms/frame (23.0 gaps/frame; per frame p50 1.050 p90 1.400 max 2.000 ms), gaps around unmeasured submissions 0.000 ms (0.0/frame, excluded), list HEADS 0.450 ms (start -> first GP2 stamp), tails 0.000 ms, lists without GP2 coverage 0.020 ms | GP2 uncovered (span - covered) 1.600 ms (gaps + heads + no-cov = 1.570) | armed 2640, no head 0, dropped: rec cap 0, ECL table 0, late 0, bad ts 0
[10:00:00.000] GQ: top 5 of 30 gap edges [previous list's last marker -> next list's first marker] | ms/frame | gaps/frame | mean us | max us | ECL call site of the next list at the max (return RVA < next game frame)
[10:00:00.000] GQ:   1 [Shadow -> GBuffer] | 0.050 ms | 1.00 | 50.0 | 80.0 | 4F9240 < 4F9214
[10:00:00.000] GQ:   2 [GBuffer -> Lighting] | 0.045 ms | 1.00 | 45.0 | 70.0 | 4F9240 < 4F9214
[10:00:00.000] GQ:   1 812.0 us | 3 -> 4 of 24 | [Shadow] -> [GBuffer] | 10 -> 11 | 4F9240 -> 4F9240 < 4F9214
[10:00:01.000] JT: window 30.0 s | watched KIDSTaskScheduler0 (tid 1) | logic ticks 1450 (48.3/s) | CV waits 2900 (2.00/tick), blocked 1.440 ms/tick (7.0 % of the window); blocked per tick p50 1.300 p90 2.000 max 6.000 ms; timed out 0, no waker seen 0 | jobs 400000 (13333/s, 30.000 ms/tick summed) on 14 threads | dropped: jobs 0, waits 0, wakes 0; unbalanced 0
[10:00:01.000] JT:   1 [3D7CF1 < 3D6F8B < 39E5A0] | 1.200 | 1.00 | 1100 / 1500 / 6000 | KIDSTaskScheduler3 60 %, KIDSTaskScheduler5 40 % | [2F66C0 < 3D7000] 100 %
[10:00:01.000] JT:      critical job (what the releasing thread was running at the wake, job fn/inner callback): 1) 111184/5ACA10 70 % of the waits, job p50 1300 p90 1600 us, started before the wait in 80 % (p50 200 us before), else p50 50 us after it began
[10:00:01.000] JT:      longest job overlapping the wait: 1) 111184/5ACA10 70 % (p50 1300 us) | other job threads busy 24 % of the wait (6.0 threads ran jobs), no job running on any other thread 10 % of the wait
[10:00:01.000] JT:   1 204584/16EF8C | 3.200 | 210.0 | 12 / 80 / 1900 | 7 | leaves 180.0/tick, grain 4, leaf range 4.0
"""
    gq, jt = parse(log.splitlines())
    out = []
    nm = Names("nonexistent")
    digest_gq(gq, nm, out.append)
    digest_jt(jt, nm, out.append)
    txt = "\n".join(out)
    print(txt)
    ok = (len(gq) == 1 and len(gq[0]["edges"]) == 2 and len(gq[0]["worst"]) == 1 and len(jt) == 1 and len(jt[0]["sites"]) == 1
          and len(jt[0]["jobs"]) == 1 and "closure (GP2 uncovered == gaps + heads + unmeasured + no-cov): OK" in txt
          and "DECISION merge lists (ECL merge, vkd3d-audit design 3): no" in txt and "DECISION R2a / R1 (list-head barriers): BUILD/ABBA" in txt
          and "DECISION reorder / split jobs: SPLIT" in txt)
    print("SELFTEST", "OK" if ok else "FAILED")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv))
