"""Ally load: SP sampling rows + LoadStats ticks bucketed by load phase (58A window offsets).

usage: py -I re/scripts_load/ally_load_sp_phases.py <modlog> [--top N] [--phases name:a:b,...]
Default phases come from the LoadGates lines of window #1 (pre-gate, G8, G9/G5 mid, G5, tail).
Each SP report covers the 5 s before its timestamp; a report is assigned to the phase holding its midpoint.
Per phase and group: samples, share with RIP in Nioh3.exe, share in [unix/other], share in ntdll, top rows.
"""
import re, sys, collections

TS = re.compile(r"^\[(\d+):(\d+):(\d+\.\d+)\] ")
def t_of(line):
    m = TS.match(line)
    return int(m[1]) * 3600 + int(m[2]) * 60 + float(m[3]) if m else None

def main():
    path = sys.argv[1]
    top = 12
    if "--top" in sys.argv: top = int(sys.argv[sys.argv.index("--top") + 1])
    lines = open(path, encoding="utf-8", errors="replace").read().splitlines()
    t_set = None; gates = {}
    for ln in lines:
        if t_set is None and "LoadStats #1: 58A set" in ln: t_set = t_of(ln)
        m = re.search(r"LoadGates #1 (G\d) \S+ .*first fail ([+-][\d.]+) s, last fail ([+-][\d.]+) s", ln)
        if m: gates[m[1]] = (float(m[2]), float(m[3]))
    if t_set is None: sys.exit("no window #1")
    g8 = gates.get("G8", (30.5, 77.4)); g5 = gates.get("G5", (81.6, 92.1))
    phases = [("pre-gate", -1.0, g8[0]), ("G8", g8[0], g8[1]), ("G8->G5", g8[1], g5[0]),
              ("G5", g5[0], g5[1] + 1.5), ("tail(tip)", g5[1] + 1.5, 1e9)]
    if "--phases" in sys.argv:
        phases = []
        for p in sys.argv[sys.argv.index("--phases") + 1].split(","):
            n, a, b = p.split(":"); phases.append((n, float(a), float(b)))
    t_clear = None
    for ln in lines:
        if re.search(r"LoadStats #1: [\d.]+ s \|", ln): t_clear = t_of(ln); break
    def phase(off):
        if t_clear is not None and off > t_clear - t_set + 0.1: return None
        for n, a, b in phases:
            if a <= off < b: return n
        return None
    # ticks
    tick = collections.defaultdict(lambda: [0, 0.0, 0.0, 0, 0.0])  # n, cpu, read, frames, inflate
    for ln in lines:
        m = re.search(r"LoadStats tick #1 \+(\d+) s: frames (\d+), CPU ([\d.]+) cores, A act (\d+), read ([\d.]+) MB, inflate (\d+) ms", ln)
        if m:
            p = phase(int(m[1]) - 0.5)
            if p:
                s = tick[p]; s[0] += 1; s[1] += float(m[3]); s[2] += float(m[5]); s[3] += int(m[2]); s[4] += int(m[6])
    print(f"window #1 clear {'%.1f s' % (t_clear - t_set) if t_clear else '?'}")
    print("phase       s   CPU cores  fps   MB/s  inflate ms/s")
    for n, a, b in phases:
        s = tick.get(n)
        if s and s[0]: print(f"{n:10s} {s[0]:4d}  {s[1]/s[0]:6.2f}  {s[3]/s[0]:5.1f} {s[2]/s[0]:6.1f}  {s[4]/s[0]:6.0f}")
    # SP
    agg = {}  # (phase, group) -> [samples, rip_game_samples, threads, Counter(label->samples)]
    cur = None; cur_n = 0; cur_p = None
    HDR = re.compile(r"SP (main \[App\]Game|KIDSTaskScheduler|KIDSTS|SynergisticDispatcher|\[App\] other): (\d+) samples in (\d+) s from (\d+) thread\(s\), RIP in Nioh3.exe ([\d.]+) %")
    ROW = re.compile(r"SP (\S+)\s+([\d.]+) %  (.*)$")
    for ln in lines:
        m = HDR.search(ln)
        if m:
            t = t_of(ln); off = t - t_set - int(m[3]) / 2
            cur_p = phase(off); cur = m[1]; cur_n = int(m[2])
            if cur_p:
                a = agg.setdefault((cur_p, cur), [0, 0.0, 0, collections.Counter(), 0])
                a[0] += cur_n; a[1] += cur_n * float(m[5]) / 100; a[2] = max(a[2], int(m[4])); a[4] += 1
            continue
        m = ROW.search(ln)
        if m and cur_p and not ln.endswith("thread(s)"):
            agg[(cur_p, cur)][3][m[3]] += cur_n * float(m[2]) / 100
    for n, a, b in phases:
        print(f"\n=== {n} ({a:+.1f}..{b:+.1f} s)")
        for g in ["main [App]Game", "KIDSTaskScheduler", "KIDSTS", "SynergisticDispatcher", "[App] other"]:
            x = agg.get((n, g))
            if not x: continue
            c = x[3]; tot = x[0]
            unix = sum(v for k, v in c.items() if k.startswith("[unix/other]"))
            nt = sum(v for k, v in c.items() if k.startswith("[ntdll.dll]"))
            game = sum(v for k, v in c.items() if k.startswith("fn "))
            print(f"-- {g}: {tot} samples / {x[4]} reports, {x[2]} threads | RIP game {100*x[1]/tot:.1f} % "
                  f"(rows: game {100*game/tot:.1f}, unix {100*unix/tot:.1f}, ntdll {100*nt/tot:.1f}) "
                  f"-> busy-thread est (game+unix) x threads = {(game+unix)/tot*x[2]:.2f}")
            for k, v in c.most_common(top):
                print(f"   {100*v/tot:5.1f} %  {k}")

main()
