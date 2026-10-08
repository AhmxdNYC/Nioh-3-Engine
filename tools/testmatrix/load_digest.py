"""load_digest.py <log>...  one line per LoadGates window: window s, holder, last eval, ReadProbe CreateEventA/CloseHandle avg us, 740392 avg us, EP pool, AutoContinue."""
import re, sys

def digest(path):
    tx = open(path, encoding="utf-8", errors="replace").read()
    out = []
    ac = re.findall(r"AutoContinue #(\d+): load done", tx)
    for m in re.finditer(r"LoadGates #(\d+): (\d+) evaluations .*?last eval \+([\d.]+) s, window ([\d.]+) s", tx):
        n = m.group(1)
        h = re.search(r"LoadGates #%s holder: (\S+ \S+) \(([\d.]+) s of" % n, tx)
        rp = re.search(r"ReadProbe #%s site 740392: (\d+) calls, (\d+) MB, in-call ([\d.]+) s \(avg (\d+) us\)" % n, tx)
        ev = re.search(r"ReadProbe #%s events: CreateEventA (\d+) \(avg ([\d.]+) us\), CloseHandle \d+ \(avg ([\d.]+) us\), WFMO \d+ \(avg (\d+) us.*?\| EP (\w+): pool hits (\d+)" % n, tx)
        out.append("%s #%s window %ss last-eval +%ss holder %s | 740392 %s | CreateEventA %s us CloseHandle %s us | EP %s hits %s | AutoContinue fired %s" % (
            path.split("\\")[-1], n, m.group(4), m.group(3), (h.group(1) + " %ss" % h.group(2)) if h else "?",
            ("avg %s us (%s calls, in-call %s s)" % (rp.group(4), rp.group(1), rp.group(3))) if rp else "?",
            ev.group(2) if ev else "?", ev.group(3) if ev else "?", ev.group(5) if ev else "?", ev.group(6) if ev else "?", "yes" if ac else "no"))
    return out or [path + ": no LoadGates window"]

for p in sys.argv[1:]:
    print("\n".join(digest(p)))
