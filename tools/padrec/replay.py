"""Replay a padrec.py CSV into the game through the mod's VirtualPad pipe (EF [Debug] VirtualPad = 1 or 2).

Usage: py -I tools/padrec/replay.py PAD.csv [--from HH:MM:SS[.f]] [--to HH:MM:SS[.f]] [--lead-in S] [--stick-ms 8]
                                    [--pipe NAME] [--owner-pid N] [--dry-run] [--log OUT.csv]
Button and trigger edges are sent at their recorded times (absolute schedule from one perf_counter origin, no drift).
Stick changes are coalesced to at most one update per --stick-ms per stick (the newest value wins). Every command's
scheduled vs actual send time goes to --log, so timing error is measurable; the summary prints p50/p99/max lateness.
--from/--to cut a window by the 'wall' column (e.g. the act log's span). Ends with 'release all' and both sticks centred.
Exit codes as captures/kipress3/vpad.py: 0 ok, 1 err reply, 2 no pipe, 3 owner gone.
"""
import argparse, csv, datetime, os, sys, time

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "captures", "kipress3"))

BTN = [("UP", 0x0001), ("DOWN", 0x0002), ("LEFT", 0x0004), ("RIGHT", 0x0008), ("START", 0x0010), ("BACK", 0x0020),
       ("LS", 0x0040), ("RS", 0x0080), ("LB", 0x0100), ("RB", 0x0200), ("A", 0x1000), ("B", 0x2000), ("X", 0x4000),
       ("Y", 0x8000)]


def wall(s):
    return datetime.datetime.strptime(s, "%H:%M:%S.%f" if "." in s else "%H:%M:%S")


def ax(v):
    v = int(v)
    return max(-1.0, min(1.0, v / (32767.0 if v >= 0 else 32768.0)))


def mask_names(m):
    return "+".join(n for n, b in BTN if m & b)


def build(rows, stick_ms):
    """rows: (t_ms, buttons, lt, rt, lx, ly, rx, ry) -> list of (t_ms, command)."""
    out = []
    pb, plt, prt, pl, pr = 0, 0, 0, (0, 0), (0, 0)
    last_stick = {"L": -1e9, "R": -1e9}
    pend = {}
    for t, b, lt, rt, lx, ly, rx, ry in rows:
        for side, val, prev in (("L", (lx, ly), pl), ("R", (rx, ry), pr)):
            if val != prev:
                pend[side] = val
        pl, pr = (lx, ly), (rx, ry)
        for side in list(pend):
            if t - last_stick[side] >= stick_ms:
                x, y = pend.pop(side)
                out.append((t, "stick %s %.4f %.4f" % (side, ax(x), ax(y))))
                last_stick[side] = t
        up, down = pb & ~b, b & ~pb
        if up:
            out.append((t, "release " + mask_names(up)))
        if down:
            out.append((t, "hold " + mask_names(down)))
        pb = b
        if lt != plt:
            out.append((t, "trigger L %d" % lt)); plt = lt
        if rt != prt:
            out.append((t, "trigger R %d" % rt)); prt = rt
    t_end = rows[-1][0] if rows else 0
    for side, (x, y) in pend.items():
        out.append((t_end, "stick %s %.4f %.4f" % (side, ax(x), ax(y))))
    return out


def load(path, t_from, t_to):
    rows = []
    for r in csv.reader(open(path)):
        if len(r) < 11 or r[0] == "t_ms" or r[0].startswith("#") or r[2] != "0":
            continue
        w = wall(r[1])
        if (t_from and w < t_from) or (t_to and w > t_to):
            continue
        rows.append((float(r[0]), int(r[4], 16), int(r[5]), int(r[6]), int(r[7]), int(r[8]), int(r[9]), int(r[10])))
    return rows


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("pad")
    ap.add_argument("--from", dest="t_from")
    ap.add_argument("--to", dest="t_to")
    ap.add_argument("--lead-in", type=float, default=1.0, help="seconds before the first event")
    ap.add_argument("--stick-ms", type=float, default=8.0)
    ap.add_argument("--pipe", default=r"\\.\pipe\nioh3vpad")
    ap.add_argument("--owner-pid", type=int, default=0)
    ap.add_argument("--dry-run", action="store_true")
    ap.add_argument("--log")
    a = ap.parse_args()
    rows = load(a.pad, a.t_from and wall(a.t_from), a.t_to and wall(a.t_to))
    if not rows:
        raise SystemExit("no rows in window")
    t0 = rows[0][0]
    # start from the recorded state at the window's first row: neutral first, then that row as a change
    rows = [(t0 - 1, 0, 0, 0, 0, 0, 0, 0)] + rows
    cmds = [(t - t0, c) for t, c in build(rows, a.stick_ms)]
    holds = sum(1 for _, c in cmds if c.startswith("hold"))
    print("REPLAY rows %d cmds %d hold-edges %d span %.1f s" % (len(rows) - 1, len(cmds), holds, cmds[-1][0] / 1000.0))
    if a.dry_run:
        for t, c in cmds[:20]:
            print("%9.1f %s" % (t, c))
        return 0
    import vpad
    vpad.OWNER = a.owner_pid
    try:
        cl = vpad.Client(a.pipe)
    except vpad.NoPipe as e:
        print("REPLAY-NOPIPE", e); return 2
    import ctypes
    ctypes.windll.winmm.timeBeginPeriod(1)
    k32 = ctypes.windll.kernel32
    k32.SetPriorityClass(k32.GetCurrentProcess(), 0x80)  # HIGH: edges stay on time while other work loads the CPU
    k32.SetThreadPriority(k32.GetCurrentThread(), 2)
    log = open(a.log, "w", buffering=1) if a.log else None
    if log:
        log.write("sched_ms,sent_ms,reply_ms,cmd,reply\n")
    for _ in range(3):  # warm the pipe so the first edge is not late
        cl.cmd("ping")
    late, rc = [], 0
    origin = time.perf_counter() + a.lead_in
    try:
        for i, (t, c) in enumerate(cmds):
            due = origin + t / 1000.0
            while True:
                d = due - time.perf_counter()
                if d <= 0:
                    break
                time.sleep(d - 0.002 if d > 0.003 else 0)
            sent = (time.perf_counter() - origin) * 1000.0
            rep = cl.cmd(c)
            got = (time.perf_counter() - origin) * 1000.0
            late.append(sent - t)
            if log:
                log.write("%.1f,%.1f,%.1f,%s,%s\n" % (t, sent, got, c, rep.replace(",", ";")))
            if rep.startswith("err"):
                print("REPLAY-ERR", c, rep); rc = 1; break
            if i % 200 == 0:
                vpad.check_owner()
    except vpad.NoPipe as e:
        print("REPLAY-NOPIPE", e); rc = 2
    finally:
        for c in ("release all", "stick L 0 0", "stick R 0 0", "trigger L 0", "trigger R 0"):
            try:
                cl.cmd(c)
            except Exception:
                pass
        cl.close()
    s = sorted(late) or [0]
    print("REPLAY done sent %d/%d late_ms p50 %.1f p99 %.1f max %.1f" % (
        len(late), len(cmds), s[len(s) // 2], s[min(len(s) - 1, int(len(s) * 0.99))], s[-1]))
    return rc


if __name__ == "__main__":
    sys.exit(main())
