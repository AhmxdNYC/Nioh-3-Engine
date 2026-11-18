# TASK #117667 (human #117585): replay human pad D:/nioh3-re/handtest-115878/pad.csv through a ViGEm X360 pad.
# ARM=A vanilla 0 ASIs REN 120 | B full mods MaxFps 0 | C full mods REN_FPS 60.  Reuses Temp/van117452 (van.py/base.py).
import csv, ctypes, datetime, os, shutil, subprocess, sys, threading, time
VAN = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "van117452")
sys.path.insert(0, VAN)
import base, van
ARM = os.environ["ARM"]
OUT = r"D:\nioh3-re\rep-117667"
os.makedirs(OUT, exist_ok=True)
base.OUT = OUT
BAK = os.path.join(OUT, "saves-bak-" + ARM)
INI = base.INI
INI_BAK = os.path.join(OUT, "ini-bak-" + ARM)
van.ASIDIR = os.path.join(OUT, "asi-out")
PAD = r"D:\nioh3-re\handtest-115878\pad.csv"
log = base.log
BTNMAP = {0x0001: "DPAD_UP", 0x0002: "DPAD_DOWN", 0x0004: "DPAD_LEFT", 0x0008: "DPAD_RIGHT", 0x0010: "START",
          0x0020: "BACK", 0x0040: "LEFT_THUMB", 0x0080: "RIGHT_THUMB", 0x0100: "LEFT_SHOULDER",
          0x0200: "RIGHT_SHOULDER", 0x1000: "A", 0x2000: "B", 0x4000: "X", 0x8000: "Y"}


def load():
    rows = []
    for r in csv.reader(open(PAD)):
        if len(r) < 11 or r[0] == "t_ms" or r[0].startswith("#") or r[2] != "0":
            continue
        rows.append((float(r[0]), r[1], int(r[4], 16), int(r[5]), int(r[6]), int(r[7]), int(r[8]), int(r[9]), int(r[10])))
    return rows


def still_human():
    class PT(ctypes.Structure):
        _fields_ = [("x", ctypes.c_long), ("y", ctypes.c_long)]
    pts = []
    for _ in range(9):
        p = PT(); ctypes.windll.user32.GetCursorPos(ctypes.byref(p)); pts.append((p.x, p.y)); time.sleep(0.5)
    live, idle = base.human_block()
    moved = len(set(pts)) > 1
    log("cursor %s moved %s pad live %s" % (pts[0], moved, live))
    return moved or live


def replay(pad, rows, sent, stop):
    import vgamepad as vg
    k32 = ctypes.windll.kernel32
    ctypes.windll.winmm.timeBeginPeriod(1)
    k32.SetThreadPriority(k32.GetCurrentThread(), 2)
    p = pad.p
    t0 = rows[0][0]
    prev = None
    origin = time.perf_counter() + 0.5
    for t, w, b, lt, rt, lx, ly, rx, ry in rows:
        if stop.is_set():
            break
        due = origin + (t - t0) / 1000.0
        while True:
            d = due - time.perf_counter()
            if d <= 0:
                break
            time.sleep(d - 0.002 if d > 0.003 else 0)
        st = (b, lt, rt, lx, ly, rx, ry)
        if st == prev:
            continue
        p.report.wButtons = b
        p.left_trigger(value=lt); p.right_trigger(value=rt)
        p.left_joystick(x_value=lx, y_value=ly); p.right_joystick(x_value=rx, y_value=ry)
        p.report.wButtons = b
        p.update()
        now = time.perf_counter()
        sent.append((w, datetime.datetime.now().strftime("%H:%M:%S.%f")[:-3], b, (now - due) * 1000.0))
        prev = st
    p.reset(); p.update()


def run():
    pad = van.Pad(); van.S["pad"] = pad
    log("vigem pad up")
    pid = van.launch()
    h, player, sample = base.attach(pid)
    import ctypes.wintypes as W
    mods = (ctypes.c_void_p * 1)(); need = W.DWORD()
    base.ps.EnumProcessModules(h, mods, ctypes.sizeof(mods), ctypes.byref(need))
    gbase = mods[0]
    van.focus(pid, h, gbase)
    van.title_continue(pad, player, sample, h, gbase)
    time.sleep(2)
    if not van.focus(pid, h, gbase):
        raise SystemExit("game window not active")
    rows = load()
    log("replay start rows %d span %.1f s" % (len(rows), (rows[-1][0] - rows[0][0]) / 1000))
    sent, stop = [], threading.Event()
    th = threading.Thread(target=replay, args=(pad, rows, sent, stop), daemon=True)
    acts = open(os.path.join(OUT, "act-%s.csv" % ARM), "w", encoding="utf-8")
    acts.write("wall,act,fps,t60\n")
    th.start()
    last = None; fl = []; lastbeat = time.time()
    while th.is_alive():
        if pid not in base.nioh_pids():
            stop.set(); raise SystemExit("game exited mid replay")
        c = player()
        cur = sample(c) if c else (-1, -1, 0, None, None, -1)
        if cur[1] > 0:
            fl.append(cur[1])
        if cur[0] != last:
            acts.write("%s,%s,%s,%s\n" % (datetime.datetime.now().strftime("%H:%M:%S.%f")[:-3],
                       hex(cur[0]) if cur[0] >= 0 else cur[0], cur[1], cur[3]))
            last = cur[0]
        if time.time() - lastbeat > 60:
            log("replay %d edges sent, act %s fps %s" % (len(sent), hex(cur[0]) if cur[0] >= 0 else cur[0], cur[1]))
            lastbeat = time.time()
        time.sleep(0.004)
    acts.close()
    with open(os.path.join(OUT, "sent-%s.csv" % ARM), "w") as f:
        f.write("rec_wall,send_wall,buttons,late_ms\n")
        for s in sent:
            f.write("%s,%s,%04x,%.2f\n" % s)
    lat = sorted(s[3] for s in sent) or [0]
    fl.sort()
    log("ARM %s replay done edges %d late p50 %.1f p99 %.1f max %.1f fps p5 %s p50 %s p95 %s" % (
        ARM, len(sent), lat[len(lat) // 2], lat[int(len(lat) * .99)], lat[-1],
        fl[len(fl) // 20] if fl else -1, fl[len(fl) // 2] if fl else -1, fl[len(fl) * 19 // 20] if fl else -1))


def main():
    if base.nioh_pids():
        raise SystemExit("Nioh already running")
    if still_human():
        raise SystemExit("human input, not launching")
    shutil.copy2(INI, INI_BAK)
    subprocess.run(["robocopy", base.SAVES, BAK, "/MIR", "/R:1", "/W:1", "/NFL", "/NDL", "/NJH", "/NJS"], check=False)
    log("ARM %s saves + ini backed up" % ARM)
    if ARM == "A":
        van.move_asis_out()
    else:
        hs = {n: van.sha(os.path.join(base.GAME, n)) for n in van.ASIS}
        if hs != van.ASIS:
            raise SystemExit("asis %s" % hs)
        txt = open(INI, encoding="utf-8", errors="replace").read()
        new, seen = base.set_keys(txt, [("MaxFps", "0" if ARM == "B" else "120"), ("VirtualPad", "0")])
        open(INI, "w", encoding="utf-8", newline="").write(new)
        log("ini %s MaxFps %s" % (seen, "0" if ARM == "B" else "120"))
        if ARM == "C":
            base.set_ren(60)
    run()


if __name__ == "__main__":
    try:
        main()
    except SystemExit as e:
        log("EXIT %s" % e)
    except Exception as e:
        log("ERR %r" % e)
    finally:
        for step in (base.close_game, van.restore_asis):
            try:
                step()
            except Exception as e:
                log("RESTORE ERR %r" % e)
        try:
            if van.S["pad"]:
                van.S["pad"].close(); del van.S["pad"].p; van.S["pad"] = None
                log("vigem pad unplugged")
        except Exception as e:
            log("pad close err %r" % e)
        if os.path.exists(INI_BAK):
            shutil.copy2(INI_BAK, INI); log("ini restored")
        if os.path.isdir(BAK):
            subprocess.call(["robocopy", BAK, base.SAVES, "/MIR", "/R:1", "/W:1", "/NFL", "/NDL", "/NJH", "/NJS"])
            log("saves restored")
        if os.path.exists(base.MARKER):
            os.remove(base.MARKER); log("marker removed")
        log("ARM %s END" % ARM)
