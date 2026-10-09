"""vs_digest.py <modlog> [presentmon.csv] - VS1 digest (notes/root-cause-vram-streaming.md).

Per tour point (the 'Tour P' lines: t0/t1 = dwell window, epoch ms) the 1 s 'stats vs:' lines of ASI >= 1.7.48
(MemStats = 2) inside the dwell are averaged: VRAM local used/budget, local MB/s churn, non-local, the engine texture page
pool occupancy (vs its 85 % controller threshold), last-frame page misses / queue-full, and D3D12 MakeResident / Evict /
allocation rates. With presentmon.csv the GPUBusy / frame time of the same window is added. Also prints whole-run totals.
"""
import csv, datetime as dt, re, statistics, sys
from pathlib import Path

TS = re.compile(r"^\[(\d+):(\d+):(\d+)\.(\d+)\]")
VS = re.compile(r"stats vs: local (-?[\d.]+)/([\d.]+) MB \(([+-]?[\d.]+) MB/s\), nonlocal (-?[\d.]+)/([\d.]+) MB \| (.*)$")
POOL = re.compile(r"pool (-?\d+) pages \((-?\d+) MB\) used (\d+) reserved (\d+) occ (-?[\d.]+)%( OVER85)? \| req-pool (\d+) free (-?\d+) "
                  r"frame (\d+)/(\d+) \| lastframe touches (\d+) hits (\d+) misses (\d+) qfull (\d+)")
D3D = re.compile(r"MakeResident (\d+) \((\d+) obj\) Evict (\d+) \((\d+) obj\) CreateHeap (\d+) \(\+([\d.]+) MB vram, \+([\d.]+) MB sys\) "
                 r"Committed (\d+) \(\+([\d.]+) MB vram, \+([\d.]+) MB sys\) \| since hook: heaps (\d+)/(\d+) MB, committed (\d+)/(\d+) MB")
TP = re.compile(r"Tour P pass=(\d+) stage=(\d+)/\d+ key=(\d+) pt=(\d+)/\d+ src=(\S+).*? vram=.*? t0=(\d+) t1=(\d+) label=(.*)$")


def parse(path):
    lines = Path(path).read_text(encoding="utf-8", errors="replace").splitlines()
    pts, raw = [], []
    for ln in lines:
        m = TP.search(ln)
        if m:
            t = TS.match(ln)
            pts.append(dict(hms=(int(t[1]), int(t[2]), int(t[3]), int(t[4])) if t else None, pass_=int(m[1]), stage=int(m[2]), key=int(m[3]), pt=int(m[4]), src=m[5], t0=int(m[6]) / 1000.0,
                            t1=int(m[7]) / 1000.0, label=m[8].strip()[:34]))
            continue
        v = VS.search(ln)
        t = TS.match(ln)
        if v and t:
            rec = dict(hms=(int(t[1]), int(t[2]), int(t[3]), int(t[4])), lu=float(v[1]), lb=float(v[2]), dlu=float(v[3]),
                       nu=float(v[4]), nb=float(v[5]))
            p = POOL.search(v[6])
            if p:
                rec.update(size=int(p[1]), used=int(p[3]), resv=int(p[4]), occ=float(p[5]), cap=int(p[7]), free=int(p[8]),
                           freq=int(p[9]), touches=int(p[11]), hits=int(p[12]), miss=int(p[13]), qfull=int(p[14]))
            d = D3D.search(v[6])
            if d:
                rec.update(mr=int(d[2]), ev=int(d[4]), heapv=float(d[6]), heaps=float(d[7]), comv=float(d[9]), coms=float(d[10]),
                           th_v=int(d[11]), th_s=int(d[12]), tc_v=int(d[13]), tc_s=int(d[14]))
            raw.append(rec)
    # log times are the capturing machine's local wall clock without a date (its time zone may differ from this one): the
    # offset comes from the first tour point (logged right after its t1), rounded to 15 min; roll over at midnight
    off, day = 0.0, dt.date.today()
    if pts and pts[0]["hms"]:
        h, mi, s, ms = pts[0]["hms"]
        utc = dt.datetime.fromtimestamp(pts[0]["t1"], dt.timezone.utc)
        best = None
        for dd in (-1, 0, 1):
            cand = dt.datetime.combine(utc.date() + dt.timedelta(days=dd), dt.time(h, mi, s, ms * 1000), dt.timezone.utc)
            o = round((pts[0]["t1"] - cand.timestamp()) / 900.0) * 900.0
            if best is None or abs(pts[0]["t1"] - cand.timestamp()) < abs(best[0]):
                best = (pts[0]["t1"] - cand.timestamp(), o, cand.date())
        off, day = best[1], best[2]
        if raw and raw[0]["hms"] > pts[0]["hms"]:
            day -= dt.timedelta(days=1)
    prev = None
    for r in raw:
        h, mi, s, ms = r["hms"]
        t = dt.datetime.combine(day, dt.time(h, mi, s, ms * 1000), dt.timezone.utc).timestamp() + off
        if prev is not None and t < prev - 3600:
            day += dt.timedelta(days=1)
            t += 86400
        r["t"] = prev = t
    return pts, raw, off


def load_pm(path, off):
    rows = []
    with open(path, newline="", encoding="utf-8-sig", errors="replace") as f:
        for r in csv.DictReader(f):
            if r.get("Application", "").lower() != "nioh3.exe":
                continue
            m = re.match(r"(\d+)-(\d+)-(\d+) (\d+):(\d+):(\d+)(?:\.(\d+))?", r.get("CPUStartDateTime") or "")
            if not m:
                continue
            y, mo, d, h, mi, se, fr = m.groups()
            ts = dt.datetime(int(y), int(mo), int(d), int(h), int(mi), int(se), int((fr or "0")[:6].ljust(6, "0")),
                             dt.timezone.utc).timestamp()
            try:
                rows.append((ts, float(r["FrameTime"]), float(r["GPUBusy"])))
            except (KeyError, ValueError):
                pass
    rows.sort()
    # PresentMon's CPUStartDateTime is wall-clock-like but not reliably the log's zone (seen: log local - 4 h on the PC):
    # align on the run start - PresentMon starts < 7 min before the game's first log line - in 15 min steps
    return rows


def mean(v):
    v = [x for x in v if x is not None]
    return statistics.fmean(v) if v else float("nan")


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        return 1
    pts, raw, off = parse(sys.argv[1])
    pm = load_pm(sys.argv[2], off) if len(sys.argv) > 2 and Path(sys.argv[2]).exists() else []
    if pm and raw:
        shift = round((raw[0]["t"] - pm[0][0]) / 900.0) * 900.0
        pm = [(t + shift, a, b) for t, a, b in pm]
    print(f"VS1 digest: {len(raw)} 'stats vs:' lines, {len(pts)} tour points, presentmon frames {len(pm)}")
    if raw:
        lb = [r["lb"] for r in raw]
        print("run: local used max %.0f MB / budget %.0f-%.0f MB, nonlocal %.0f-%.0f MB; pool occ max %.1f %% (>85: %d of %d s); "
              "MakeResident obj %d, Evict obj %d total; heaps since hook vram/sys %s/%s MB, committed %s/%s MB" % (
                  max(r["lu"] for r in raw), min(lb), max(lb), min(r["nu"] for r in raw), max(r["nu"] for r in raw),
                  max((r.get("occ", -1) for r in raw), default=-1), sum(1 for r in raw if r.get("occ", 0) > 85), len(raw),
                  sum(r.get("mr", 0) for r in raw), sum(r.get("ev", 0) for r in raw), raw[-1].get("th_v", "-"),
                  raw[-1].get("th_s", "-"), raw[-1].get("tc_v", "-"), raw[-1].get("tc_s", "-")))
    hdr = "pass st pt src   | local/budget  dMB/s(abs) nonloc | pool MB occ%  miss/f qfull/f | MR/s Ev/s alloc MB/s v/s | GPUBusy ft  | label"
    print(hdr)
    for p in pts:
        sel = [r for r in raw if p["t0"] <= r["t"] <= p["t1"] + 0.5]
        gb = ft = float("nan")
        if pm:
            w = [x for x in pm if p["t0"] <= x[0] <= p["t1"]]
            if len(w) > 5:
                gb, ft = mean([x[2] for x in w]), mean([x[1] for x in w])
        if not sel:
            print(f"{p['pass_']:>4} {p['stage']:>2} {p['pt']:>2} {p['src']:<5} | (no vs lines) | GPUBusy {gb:.2f} ft {ft:.2f} | {p['label']}")
            continue
        print("%4d %2d %2d %-5s | %5.0f/%5.0f %6.1f %6.0f | %5.0f %5.1f %6.1f %6.2f | %4.1f %4.1f %5.1f/%4.1f | %6.2f %6.2f | %s" % (
            p["pass_"], p["stage"], p["pt"], p["src"], mean([r["lu"] for r in sel]), mean([r["lb"] for r in sel]),
            mean([abs(r["dlu"]) for r in sel]), mean([r["nu"] for r in sel]), mean([r.get("size", 0) / 16.0 for r in sel]),
            mean([r.get("occ") for r in sel]), mean([r.get("miss") for r in sel]), mean([r.get("qfull") for r in sel]),
            mean([r.get("mr", 0) for r in sel]), mean([r.get("ev", 0) for r in sel]),
            mean([r.get("heapv", 0) + r.get("comv", 0) for r in sel]), mean([r.get("heaps", 0) + r.get("coms", 0) for r in sel]),
            gb, ft, p["label"]))
    return 0


if __name__ == "__main__":
    sys.exit(main())
