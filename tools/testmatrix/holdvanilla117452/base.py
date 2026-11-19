# TASK #116947. External reads plus VirtualPad Y only. No debugger.
# 16 Y holds, each only after action 0x7d0 has been current for >= 1.0 s.
# Arm: uncapped (MaxFps 0, REN_FPS 120) then REN_FPS 60 (MaxFps left 120).
import ctypes, ctypes.wintypes as W, datetime, hashlib, os, shutil, struct, subprocess, sys, time

GAME = r"C:\Program Files (x86)\Steam\steamapps\common\Nioh3"
INI = os.path.join(GAME, "Nioh3EngineFix.ini")
SAVES = os.path.join(os.environ["LOCALAPPDATA"], "KoeiTecmo", "NIOH3")
OUT = r"D:\nioh3-re\holdmeas-116947"
BAK = os.path.join(OUT, "saves-bak")
INI_BAK = os.path.join(OUT, "EngineFix.ini.bak")
MARKER = r"C:\Users\Ahmad Hamza\AppData\Local\uo-agentd\gx-pc\test-active"
VPAD = r"C:\Users\Ahmad Hamza\nioh3-engine\tools\testmatrix\vpad.py"
AUTOPLAY = r"C:\Users\Ahmad Hamza\universal-optimizer\tools\steam\steam_autoplay.ps1"
EF = os.path.join(GAME, "Nioh3EngineFix.asi")
HOLD_S = 1.4
IDLE_S = 1.0
N_HOLDS = 16
CHARGE_MS = 2000
STATE = {"pid": None, "auto": None, "launched": False}

k32 = ctypes.WinDLL("kernel32", use_last_error=True)
ps = ctypes.WinDLL("psapi", use_last_error=True)
k32.OpenProcess.restype = W.HANDLE
k32.ReadProcessMemory.argtypes = [W.HANDLE, ctypes.c_void_p, ctypes.c_void_p, ctypes.c_size_t, ctypes.POINTER(ctypes.c_size_t)]
ps.GetModuleBaseNameW.argtypes = [W.HANDLE, ctypes.c_void_p, ctypes.c_wchar_p, W.DWORD]


def log(msg):
    line = "%s %s" % (datetime.datetime.now().strftime("%H:%M:%S"), msg)
    print(line, flush=True)
    with open(os.path.join(OUT, "trace.txt"), "a", encoding="utf-8") as f:
        f.write(line + "\n")


def nioh_pids():
    arr = (W.DWORD * 8192)()
    need = W.DWORD()
    if not ps.EnumProcesses(arr, ctypes.sizeof(arr), ctypes.byref(need)):
        return []
    out = []
    for i in range(need.value // 4):
        pid = int(arr[i])
        if not pid:
            continue
        h = k32.OpenProcess(0x100410, 0, pid)
        if not h:
            continue
        name = ctypes.create_unicode_buffer(260)
        ok = ps.GetModuleBaseNameW(h, None, name, 260)
        k32.CloseHandle(h)
        if ok and name.value.lower() == "nioh3.exe":
            out.append(pid)
    return out


def human_block():
    class LII(ctypes.Structure):
        _fields_ = [("cbSize", ctypes.c_uint), ("dwTime", ctypes.c_uint)]
    lii = LII()
    lii.cbSize = ctypes.sizeof(lii)
    ctypes.windll.user32.GetLastInputInfo(ctypes.byref(lii))
    idle = (ctypes.windll.kernel32.GetTickCount() - lii.dwTime) & 0xFFFFFFFF
    class GP(ctypes.Structure):
        _fields_ = [("wButtons", ctypes.c_ushort), ("bLeftTrigger", ctypes.c_ubyte),
                    ("bRightTrigger", ctypes.c_ubyte), ("sThumbLX", ctypes.c_short),
                    ("sThumbLY", ctypes.c_short), ("sThumbRX", ctypes.c_short), ("sThumbRY", ctypes.c_short)]
    class ST(ctypes.Structure):
        _fields_ = [("dwPacketNumber", ctypes.c_uint), ("Gamepad", GP)]
    x = ctypes.WinDLL("xinput1_4")
    def snap():
        rows = []
        for slot in range(4):
            st = ST()
            rc = x.XInputGetState(slot, ctypes.byref(st))
            g = st.Gamepad
            rows.append((rc, st.dwPacketNumber, g.wButtons, g.bLeftTrigger, g.bRightTrigger,
                         g.sThumbLX, g.sThumbLY, g.sThumbRX, g.sThumbRY))
        return rows
    a = snap(); time.sleep(0.35); b = snap()
    live = False
    for s0, s1 in zip(a, b):
        if s1[0] != 0:
            continue
        if s1[1] != s0[1] and any(s1[2:]):
            live = True
    log("idle_ms %d pad %s -> %s live %s" % (idle, a, b, live))
    return live, idle


def set_keys(text, pairs):
    lines = text.splitlines(keepends=True)
    seen = {k: 0 for k, _ in pairs}
    out = []
    for line in lines:
        body = line.rstrip("\r\n")
        nl = line[len(body):] or "\n"
        hit = False
        for k, v in pairs:
            if body.startswith(k + "=") or body.startswith(k + " ="):
                out.append("%s=%s%s" % (k, v, nl))
                seen[k] += 1
                hit = True
                break
        if not hit:
            out.append(line)
    missing = [k for k, n in seen.items() if n == 0]
    if missing:
        raise SystemExit("ini keys missing: %s" % missing)
    return "".join(out), seen


def set_ren(fps):
    cfg = os.path.join(SAVES, "config.xml")
    xml = open(cfg, "r", encoding="utf-8", errors="replace").read()
    import re
    new, n = re.subn(r"<REN_FPS>\d+</REN_FPS>", "<REN_FPS>%d</REN_FPS>" % fps, xml, count=1)
    if n != 1:
        raise SystemExit("REN_FPS tag missing")
    open(cfg, "w", encoding="utf-8", newline="").write(new)
    log("REN_FPS %d" % fps)


def kill_pid(pid):
    h = k32.OpenProcess(0x0001, 0, int(pid))
    if not h:
        log("kill open failed %s" % pid)
        return
    ok = k32.TerminateProcess(h, 1)
    k32.CloseHandle(h)
    log("kill %s ok %s" % (pid, ok))


def launch(maxfps):
    text0 = open(INI_BAK, "r", encoding="utf-8", errors="replace").read()
    pairs = [("VirtualPad", "1")]
    if maxfps is not None:
        pairs.append(("MaxFps", str(maxfps)))
    new, seen = set_keys(text0, pairs)
    if seen["VirtualPad"] != 2:
        raise SystemExit("expected 2 VirtualPad lines")
    open(INI, "w", encoding="utf-8", newline="").write(new)
    log("ini %s" % seen)
    open(MARKER, "w", encoding="utf-8").write("gx-pc/0 hold-vanilla-117392\n")
    auto = subprocess.Popen(
        ["powershell", "-NoProfile", "-ExecutionPolicy", "Bypass", "-File", AUTOPLAY, "-Minutes", "15"],
        stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    STATE["auto"] = auto.pid
    log("autoplay pid %s" % auto.pid)
    time.sleep(1)
    STATE["launched"] = True
    os.startfile("steam://rungameid/3681010")
    log("launch requested")
    pid = None
    t0 = time.time()
    while time.time() - t0 < 180:
        pids = nioh_pids()
        if pids:
            pid = pids[0]
            break
        time.sleep(1)
    if not pid:
        raise SystemExit("game did not start")
    STATE["pid"] = pid
    log("pid %s" % pid)
    return pid, auto.pid


def attach(pid):
    h = k32.OpenProcess(0x100410, 0, pid)
    mods = (ctypes.c_void_p * 1)()
    need = W.DWORD()
    ps.EnumProcessModules(h, mods, ctypes.sizeof(mods), ctypes.byref(need))
    base = mods[0] or 0
    log("base %s" % hex(base))

    def rd(addr, n):
        buf = ctypes.create_string_buffer(n)
        got = ctypes.c_size_t()
        if not addr or not k32.ReadProcessMemory(h, ctypes.c_void_p(addr), buf, n, ctypes.byref(got)):
            raise OSError
        return buf.raw

    def rq(addr):
        return struct.unpack("<Q", rd(addr, 8))[0]

    def player():
        try:
            return rq(rq(base + 0x4745348) + 0xCD * 0x18)
        except OSError:
            return 0

    def sample(c):
        act = node = 0
        t60 = s40 = None
        fps = -1
        is30 = -1
        try:
            ctrl = rq(rq(c + 0x3B0) + 8)
            act = struct.unpack("<I", rd(rq(ctrl + 0xE8), 4))[0]
            node = rq(ctrl + 0x260)
            t60 = struct.unpack("<f", rd(ctrl + 0x60, 4))[0]
            s40 = struct.unpack("<f", rd(ctrl + 0x40, 4))[0]
        except OSError:
            act = -1
        try:
            fps = struct.unpack("<i", rd(rq(base + 0x4759910) + 0x614, 4))[0]
        except OSError:
            pass
        try:
            p = rq(base + 0x475A148)
            is30 = rd(p + 0x1E, 1)[0]
        except OSError:
            pass
        return act, fps, node, t60, s40, is30

    return h, player, sample


def close_game():
    if STATE["pid"]:
        kill_pid(STATE["pid"])
        for _ in range(30):
            if STATE["pid"] not in nioh_pids():
                break
            time.sleep(0.3)
    if STATE["auto"]:
        kill_pid(STATE["auto"])
    STATE["pid"] = None
    STATE["auto"] = None
    log("nioh after kill %s" % nioh_pids())


def seek_idle(player, sample, cl):
    def burst(label, x, y, sec):
        rep = cl.cmd("stick L %s %s" % (x, y))
        log("%s stick %s" % (label, rep))
        t0 = time.perf_counter()
        last = None
        hit = False
        while time.perf_counter() - t0 < sec:
            c = player()
            cur = sample(c) if c else (-1, -1, 0, None, None, -1)
            if cur[0] != last:
                name = hex(cur[0]) if isinstance(cur[0], int) and cur[0] >= 0 else cur[0]
                log("%s t %.2f act %s t60 %s fps %s" % (label, time.perf_counter() - t0, name, cur[3], cur[1]))
                last = cur[0]
            if cur[0] == 0x7d0:
                hit = True
                break
            time.sleep(0.05)
        cl.cmd("stick L 0 0")
        cl.cmd("release all")
        return hit

    log("seek start %s" % cl.cmd("state"))
    for label, x, y, sec, watch in (
        ("fwd", "0", "1", 3.5, 12.0),
        ("back", "0", "-1", 4.0, 12.0),
        ("left", "-1", "0", 4.0, 12.0),
    ):
        if burst(label, x, y, sec):
            got = wait_idle(player, sample, 2.0, "seek-" + label)
        else:
            got = wait_idle(player, sample, watch, "seek-" + label)
        log("seek %s state %s" % (label, cl.cmd("state")))
        if got:
            log("SEEK 0x7d0 via %s" % label)
            return True
    log("SEEK no 0x7d0")
    return False


def run_arm(tag, maxfps, ren, do_probe, n_holds=N_HOLDS, idle_timeout=25.0, watch_after=0.0, seek=False, hold_base=1, append=False):
    if ren is not None:
        set_ren(ren)
    pid, auto_pid = launch(maxfps)
    h, player, sample = attach(pid)
    r = subprocess.run([sys.executable, "-I", VPAD, "--owner-pid", str(pid), "title-continue", "300"],
                       capture_output=True, text=True)
    log("title-continue rc %s %s" % (r.returncode, (r.stdout or "")[-220:].replace("\n", " | ")))
    if r.returncode != 0:
        raise SystemExit("title-continue failed")
    sys.path.insert(0, os.path.join(os.path.dirname(VPAD)))
    import vpad
    vpad.OWNER = pid
    cl = vpad.Client()
    cl.cmd("stick L 0 0")
    cl.cmd("stick R 0 0")
    cl.cmd("release all")
    c = player()
    snap = sample(c) if c else (-1, -1, 0, None, None, -1)
    log("sanity act %s fps %s t60 %s s40 %s is30 %s" % (
        hex(snap[0]) if snap[0] >= 0 else snap[0], snap[1], snap[3], snap[4], snap[5]))
    holds_path = os.path.join(OUT, "holds-%s.csv" % tag)
    with open(holds_path, "a" if append else "w", encoding="utf-8") as hf:
        if not append:
            hf.write("arm,n,wall,act,t60,s40,fps,node,idle_s,first,first_ms,saw624d,saw629d,charged\n")
        charged = 0
        for n in range(hold_base, hold_base + n_holds):
            idle_for = wait_idle(player, sample, idle_timeout, "%s-H%02d" % (tag, n))
            if idle_for is None:
                log("H%02d no idle" % n)
                st = cl.cmd("state")
                log("H%02d state %s" % (n, st))
                hf.write("%s,%d,%s,-1,,,,,,no-idle,,,,0\n" % (tag, n, datetime.datetime.now().strftime("%H:%M:%S.%f")[:-3]))
                hf.flush()
                break
            pre_t, pre = idle_for
            act, fps, node, t60, s40, is30 = pre
            down_rep = cl.cmd("hold Y")
            log("H%02d down %s" % (n, down_rep))
            t_down = time.perf_counter()
            wall = datetime.datetime.now().strftime("%H:%M:%S.%f")[:-3]
            first = None
            first_ms = None
            saw624d = saw629d = 0
            released = False
            rel_rep = ""
            while True:
                elapsed = time.perf_counter() - t_down
                c = player()
                cur = sample(c) if c else (-1, -1, 0, None, None, -1)
                if cur[0] == 0x624d:
                    saw624d = 1
                if cur[0] == 0x629d:
                    saw629d = 1
                if first is None and cur[0] not in (0x7d0, -1) and cur[0] != act:
                    first = cur[0]
                    first_ms = int(elapsed * 1000)
                if not released and elapsed >= HOLD_S:
                    rel_rep = cl.cmd("release all")
                    released = True
                    log("H%02d release %s" % (n, rel_rep))
                if released and (saw629d or elapsed >= CHARGE_MS / 1000.0):
                    break
                if elapsed >= CHARGE_MS / 1000.0 + 1.0:
                    break
                time.sleep(0.008)
            if not released:
                rel_rep = cl.cmd("release all")
                log("H%02d release-late %s" % (n, rel_rep))
            st = cl.cmd("state")
            log("H%02d state %s" % (n, st))
            ch = 1 if saw629d else 0
            charged += ch
            log("H%02d act %s t60 %.2f fps %s idle %.2f first %s +%s 624d %d 629d %d %s" % (
                n, hex(act), t60 if t60 is not None else -1, fps, pre_t,
                hex(first) if first is not None else "-", first_ms, saw624d, saw629d,
                "CHARGED" if ch else "MISS"))
            hf.write("%s,%d,%s,%s,%s,%s,%s,%s,%.3f,%s,%s,%d,%d,%d\n" % (
                tag, n, wall, hex(act),
                "" if t60 is None else "%.5f" % t60,
                "" if s40 is None else "%.5f" % s40,
                fps, hex(node) if node else "0", pre_t,
                hex(first) if first is not None else "-",
                "" if first_ms is None else first_ms,
                saw624d, saw629d, ch))
            hf.flush()
        log("ARM %s charged %d/%d" % (tag, charged, n_holds))
        if watch_after:
            wait_idle(player, sample, watch_after, tag + "-after")
            log("%s-after state %s" % (tag, cl.cmd("state")))
        if seek:
            seek_idle(player, sample, cl)
        if do_probe:
            probe_1745(player, sample, cl, hf, tag)
    try:
        cl.cmd("release all")
    except Exception:
        pass
    cl.close()
    k32.CloseHandle(h)
    close_game()
    return charged


def _top(acc, n=6):
    items = sorted(acc.items(), key=lambda kv: -kv[1])[:n]
    parts = []
    for act, dur in items:
        name = hex(act) if isinstance(act, int) and act >= 0 else str(act)
        parts.append("%s:%.2f" % (name, dur))
    return " ".join(parts) or "-"


def wait_idle(player, sample, timeout, label):
    t_start = time.perf_counter()
    deadline = t_start + timeout
    since = None
    changes = []
    last = None
    last_t = t_start
    acc = {}
    while time.perf_counter() < deadline:
        now = time.perf_counter()
        c = player()
        cur = sample(c) if c else (-1, -1, 0, None, None, -1)
        act = cur[0]
        if last is None:
            last = act
            last_t = now
            changes.append((0.0, act, cur[3], cur[1]))
        elif act != last:
            acc[last] = acc.get(last, 0.0) + (now - last_t)
            changes.append((now - t_start, act, cur[3], cur[1]))
            last = act
            last_t = now
        if act in (0x7d0, 0x6224):
            if since is None or since[0] != act:
                since = (act, now)
            elif now - since[1] >= IDLE_S:
                acc[act] = acc.get(act, 0.0) + (now - last_t)
                _write_settle(label, changes)
                log("%s idle ok %.2fs act %s changes %d top %s" % (
                    label, now - since[1], hex(act), len(changes), _top(acc)))
                return now - since[1], cur
        else:
            since = None
        if timeout >= 40 and int(now - t_start) // 5 != int((now - t_start - 0.016) // 5):
            name = hex(act) if isinstance(act, int) and act >= 0 else act
            log("%s beat %.0fs act %s t60 %s fps %s" % (
                label, now - t_start, name, None if cur[3] is None else round(cur[3], 1), cur[1]))
        time.sleep(0.016)
    now = time.perf_counter()
    if last is not None:
        acc[last] = acc.get(last, 0.0) + (now - last_t)
    _write_settle(label, changes)
    last_name = hex(last) if isinstance(last, int) and last is not None and last >= 0 else last
    log("%s NO IDLE changes %d top %s last %s" % (label, len(changes), _top(acc), last_name))
    return None


def _write_settle(label, changes):
    path = os.path.join(OUT, "settle-%s.txt" % label.replace(" ", "-"))
    with open(path, "w", encoding="utf-8") as f:
        for t, act, t60, fps in changes:
            name = hex(act) if isinstance(act, int) and act >= 0 else str(act)
            f.write("%.3f %s t60 %s fps %s\n" % (t, name, t60, fps))


def probe_1745(player, sample, cl, hf, tag):
    log("probe 1745 start")
    got = drive_until(player, sample, cl, 0x1745, 8.0, hold_y=True)
    if not got:
        log("probe no 1745 on long Y; try RB then Y")
        cl.cmd("release all")
        time.sleep(0.2)
        if wait_idle(player, sample, 12.0, tag + "-probe") is None:
            log("probe no idle before RB")
        cl.cmd("hold RB")
        time.sleep(0.12)
        cl.cmd("release RB")
        time.sleep(0.05)
        got = drive_until(player, sample, cl, 0x1745, 8.0, hold_y=True)
    cl.cmd("release all")
    if not got:
        log("PROBE 1745 absent")
        hf.write("%s,probe,%s,absent,,,,,,,,,\n" % (tag, datetime.datetime.now().strftime("%H:%M:%S.%f")[:-3]))
        return
    time.sleep(0.35)
    c = player()
    cur = sample(c) if c else (-1, -1, 0, None, None, -1)
    log("probe still %s t60 %s node %s fps %s" % (
        hex(cur[0]) if cur[0] >= 0 else cur[0], cur[3], hex(cur[2]) if cur[2] else 0, cur[1]))
    if cur[0] != 0x1745:
        log("PROBE left 1745 before the press")
        hf.write("%s,probe,%s,%s,,,,,,,,,\n" % (tag, datetime.datetime.now().strftime("%H:%M:%S.%f")[:-3], hex(cur[0]) if isinstance(cur[0], int) and cur[0] >= 0 else cur[0]))
        return
    act, fps, node, t60, s40, is30 = cur
    cl.cmd("hold Y")
    t_down = time.perf_counter()
    saw624d = saw629d = 0
    first = None
    while time.perf_counter() - t_down < HOLD_S + 0.6:
        c = player()
        now = sample(c) if c else (-1, -1, 0, None, None, -1)
        if now[0] == 0x624d:
            saw624d = 1
        if now[0] == 0x629d:
            saw629d = 1
        if first is None and now[0] not in (0x1745, -1):
            first = now[0]
        if time.perf_counter() - t_down >= HOLD_S:
            cl.cmd("release Y")
        time.sleep(0.008)
    cl.cmd("release Y")
    log("PROBE during 1745 t60 %s node %s fps %s first %s 624d %d 629d %d" % (
        "%.2f" % t60 if t60 is not None else "-", hex(node) if node else 0, fps,
        hex(first) if first is not None else "still-1745", saw624d, saw629d))
    hf.write("%s,probe,%s,%s,%s,%s,%s,%s,,%s,,%d,%d,%d\n" % (
        tag, datetime.datetime.now().strftime("%H:%M:%S.%f")[:-3], hex(act),
        "" if t60 is None else "%.5f" % t60,
        "" if s40 is None else "%.5f" % s40,
        fps, hex(node) if node else "0",
        hex(first) if first is not None else "still-1745",
        saw624d, saw629d, 1 if saw629d else 0))


def drive_until(player, sample, cl, want, seconds, hold_y):
    if hold_y:
        cl.cmd("hold Y")
    t0 = time.perf_counter()
    while time.perf_counter() - t0 < seconds:
        c = player()
        cur = sample(c) if c else (-1, -1, 0, None, None, -1)
        if cur[0] == want:
            log("saw %s at %.2f s t60 %s" % (hex(want), time.perf_counter() - t0, cur[3]))
            return True
        time.sleep(0.016)
    return False


def run_probe():
    # The #116869 cut entered 0x1745 after a charged Y, a short RB tap during
    # 0x6310, then sat on node 0. Press Y only once that id is already current.
    pid, auto_pid = launch(0)
    r = subprocess.run([sys.executable, "-I", VPAD, "--owner-pid", str(pid), "title-continue", "300"],
                       capture_output=True, text=True)
    log("title-continue rc %s %s" % (r.returncode, (r.stdout or "")[-220:].replace("\n", " | ")))
    if r.returncode != 0:
        raise SystemExit("title-continue failed")
    sys.path.insert(0, os.path.dirname(VPAD))
    import vpad
    vpad.OWNER = pid
    h, player, sample = attach(pid)
    cl = vpad.Client()
    cl.cmd("stick L 0 0")
    cl.cmd("stick R 0 0")
    cl.cmd("release all")
    idle_for = wait_idle(player, sample, 15.0, "probe-idle")
    if idle_for is None:
        log("PROBE no idle")
        cl.close()
        k32.CloseHandle(h)
        close_game()
        return
    cl.cmd("hold Y")
    log("probe setup Y down")
    time.sleep(HOLD_S)
    cl.cmd("release all")
    log("probe setup Y up")
    t0 = time.perf_counter()
    rb_done = False
    seen = []
    last = None
    entered = None
    while time.perf_counter() - t0 < 4.0:
        c = player()
        cur = sample(c) if c else (-1, -1, 0, None, None, -1)
        act = cur[0]
        if act != last:
            seen.append((round(time.perf_counter() - t0, 3), act, cur[3], cur[1]))
            last = act
            log("probe see %s t60 %s fps %s" % (hex(act) if isinstance(act, int) and act >= 0 else act, cur[3], cur[1]))
        if act == 0x6310 and not rb_done and time.perf_counter() - t0 > 0.15:
            cl.cmd("hold RB")
            time.sleep(0.09)
            cl.cmd("release RB")
            rb_done = True
            log("probe RB tap")
        if act == 0x1745:
            entered = cur
            break
        time.sleep(0.008)
    path = os.path.join(OUT, "probe-1745.txt")
    with open(path, "w", encoding="utf-8") as f:
        for t, act, t60, fps in seen:
            name = hex(act) if isinstance(act, int) and act >= 0 else str(act)
            f.write("enter %.3f %s t60 %s fps %s\n" % (t, name, t60, fps))
    if entered is None:
        log("PROBE 1745 absent after charge+RB")
        cl.close()
        k32.CloseHandle(h)
        close_game()
        return
    time.sleep(0.25)
    c = player()
    cur = sample(c) if c else (-1, -1, 0, None, None, -1)
    log("probe confirm %s t60 %s node %s fps %s" % (
        hex(cur[0]) if isinstance(cur[0], int) and cur[0] >= 0 else cur[0], cur[3],
        hex(cur[2]) if cur[2] else 0, cur[1]))
    if cur[0] != 0x1745:
        log("PROBE left 1745 before the press")
        cl.close()
        k32.CloseHandle(h)
        close_game()
        return
    act, fps, node, t60 = cur[0], cur[1], cur[2], cur[3]
    cl.cmd("hold Y")
    log("probe Y during 1745")
    t_down = time.perf_counter()
    saw624d = saw629d = 0
    first = None
    changes = []
    last = 0x1745
    while time.perf_counter() - t_down < HOLD_S + 0.4:
        c = player()
        now = sample(c) if c else (-1, -1, 0, None, None, -1)
        if now[0] == 0x624d:
            saw624d = 1
        if now[0] == 0x629d:
            saw629d = 1
        if now[0] != last:
            changes.append((round(time.perf_counter() - t_down, 3), now[0], now[3]))
            last = now[0]
            if first is None and now[0] not in (0x1745, -1):
                first = now[0]
        if time.perf_counter() - t_down >= HOLD_S:
            cl.cmd("release Y")
        time.sleep(0.008)
    cl.cmd("release all")
    log("PROBE during 1745 t60 %s node %s fps %s first %s 624d %d 629d %d changes %s" % (
        "%.2f" % t60 if t60 is not None else "-", hex(node) if node else 0, fps,
        hex(first) if first is not None else "still-1745", saw624d, saw629d, changes))
    with open(path, "a", encoding="utf-8") as f:
        f.write("press act %s t60 %s fps %s node %s first %s 624d %d 629d %d\n" % (
            hex(act), t60, fps, hex(node) if node else 0,
            hex(first) if first is not None else "still-1745", saw624d, saw629d))
        for t, a, t60b in changes:
            f.write("during %.3f %s t60 %s\n" % (t, hex(a) if isinstance(a, int) and a >= 0 else a, t60b))
    cl.close()
    k32.CloseHandle(h)
    close_game()


def run_probe_replay():
    # Watch the same 03:22:12-03:23:27 pad as #116869. The moment 0x1745 is
    # current, stop that replay and press Y ourselves.
    pad = r"D:\nioh3-re\handtest-115878\pad-until-323.csv"
    replay = r"C:\Users\Ahmad Hamza\nioh3-engine\tools\padrec\replay.py"
    pid, auto_pid = launch(0)
    r = subprocess.run([sys.executable, "-I", VPAD, "--owner-pid", str(pid), "title-continue", "300"],
                       capture_output=True, text=True)
    log("title-continue rc %s %s" % (r.returncode, (r.stdout or "")[-180:].replace("\n", " | ")))
    if r.returncode != 0:
        raise SystemExit("title-continue failed")
    h, player, sample = attach(pid)
    log("probe replay start")
    logf = open(os.path.join(OUT, "probe-replay.csv"), "w", encoding="utf-8")
    proc = subprocess.Popen([sys.executable, "-I", replay, pad, "--from", "03:22:12", "--to", "03:23:27",
                             "--stick-ms", "40", "--owner-pid", str(pid)],
                            stdout=logf, stderr=subprocess.STDOUT)
    t0 = time.perf_counter()
    entered = None
    last = None
    seen = []
    last_beat = t0
    while time.perf_counter() - t0 < 75.0:
        if proc.poll() is not None:
            break
        c = player()
        cur = sample(c) if c else (-1, -1, 0, None, None, -1)
        now = time.perf_counter()
        if cur[0] != last or now - last_beat >= 5.0:
            if cur[0] != last:
                seen.append((round(now - t0, 3), cur[0], cur[3], cur[1], cur[2]))
            last = cur[0]
            last_beat = now
            log("probe2 see %s t60 %s fps %s node %s" % (
                hex(cur[0]) if isinstance(cur[0], int) and cur[0] >= 0 else cur[0],
                None if cur[3] is None else round(cur[3], 2), cur[1], hex(cur[2]) if cur[2] else 0))
        if cur[0] == 0x1745:
            entered = cur
            break
        time.sleep(0.016)
    if proc.poll() is None:
        kill_pid(proc.pid)
        try:
            proc.wait(timeout=3)
        except subprocess.TimeoutExpired:
            pass
    logf.close()
    log("probe replay stopped rc %s" % proc.returncode)
    path = os.path.join(OUT, "probe-1745.txt")
    with open(path, "a", encoding="utf-8") as f:
        f.write("replay-live\n")
        for row in seen:
            name = hex(row[1]) if isinstance(row[1], int) and row[1] >= 0 else str(row[1])
            f.write("live %.3f %s t60 %s fps %s node %s\n" % (row[0], name, row[2], row[3], hex(row[4]) if row[4] else 0))
    sys.path.insert(0, os.path.dirname(VPAD))
    import vpad
    vpad.OWNER = pid
    cl = vpad.Client()
    cl.cmd("release all")
    cl.cmd("stick L 0 0")
    cl.cmd("stick R 0 0")
    if entered is None:
        log("PROBE2 1745 absent during the 3:23 replay")
        cl.close()
        k32.CloseHandle(h)
        close_game()
        return
    time.sleep(0.20)
    c = player()
    cur = sample(c) if c else (-1, -1, 0, None, None, -1)
    log("probe2 confirm %s t60 %s node %s fps %s" % (
        hex(cur[0]) if isinstance(cur[0], int) and cur[0] >= 0 else cur[0], cur[3],
        hex(cur[2]) if cur[2] else 0, cur[1]))
    if cur[0] != 0x1745:
        log("PROBE2 left 1745 before the press")
        cl.close()
        k32.CloseHandle(h)
        close_game()
        return
    act, fps, node, t60 = cur[0], cur[1], cur[2], cur[3]
    cl.cmd("hold Y")
    log("probe2 Y during 1745")
    t_down = time.perf_counter()
    saw624d = saw629d = 0
    first = None
    changes = []
    last = 0x1745
    while time.perf_counter() - t_down < HOLD_S + 0.4:
        c = player()
        now = sample(c) if c else (-1, -1, 0, None, None, -1)
        if now[0] == 0x624d:
            saw624d = 1
        if now[0] == 0x629d:
            saw629d = 1
        if now[0] != last:
            changes.append((round(time.perf_counter() - t_down, 3), now[0]))
            last = now[0]
            if first is None and now[0] not in (0x1745, -1):
                first = now[0]
        if time.perf_counter() - t_down >= HOLD_S:
            cl.cmd("release Y")
        time.sleep(0.008)
    cl.cmd("release all")
    log("PROBE2 during 1745 t60 %s node %s fps %s first %s 624d %d 629d %d changes %s" % (
        "%.2f" % t60 if t60 is not None else "-", hex(node) if node else 0, fps,
        hex(first) if first is not None else "still-1745", saw624d, saw629d, changes))
    with open(path, "a", encoding="utf-8") as f:
        f.write("press act %s t60 %s fps %s node %s first %s 624d %d 629d %d\n" % (
            hex(act), t60, fps, hex(node) if node else 0,
            hex(first) if first is not None else "still-1745", saw624d, saw629d))
    cl.close()
    k32.CloseHandle(h)
    close_game()


def run_mid(tag, maxfps, ren, n_holds=40):
    # Y goes down only while a move is already playing and the stick is held.
    # Late matches the #113137 score: 0x629d more than 0.3 s after 0x624d.
    if ren is not None:
        set_ren(ren)
    pid, auto_pid = launch(maxfps)
    h, player, sample = attach(pid)
    r = subprocess.run([sys.executable, "-I", VPAD, "--owner-pid", str(pid), "title-continue", "300"],
                       capture_output=True, text=True)
    log("title-continue rc %s %s" % (r.returncode, (r.stdout or "")[-180:].replace("\n", " | ")))
    if r.returncode != 0:
        raise SystemExit("title-continue failed")
    sys.path.insert(0, os.path.dirname(VPAD))
    import vpad
    vpad.OWNER = pid
    cl = vpad.Client()
    cl.cmd("stick R 0 0")
    cl.cmd("release all")
    cl.cmd("stick L 0 24000")
    idle_ids = (0x7d0, 0x6224, -1)
    path = os.path.join(OUT, "holds-%s.csv" % tag)
    charged = late_n = 0
    with open(path, "w", encoding="utf-8") as hf:
        hf.write("arm,n,wall,act,t60,s40,fps,node,first,first_ms,t624_ms,t629_ms,late_ms,saw624d,saw629d,charged,late\n")
        for n in range(1, n_holds + 1):
            cl.cmd("stick L 0 24000")
            c = player()
            cur = sample(c) if c else (-1, -1, 0, None, None, -1)
            if cur[0] in idle_ids:
                cl.cmd("hold Y")
                time.sleep(0.12)
                cl.cmd("release Y")
                t_wait = time.perf_counter()
                while time.perf_counter() - t_wait < 1.6:
                    c = player()
                    cur = sample(c) if c else (-1, -1, 0, None, None, -1)
                    if cur[0] not in idle_ids:
                        break
                    time.sleep(0.008)
            if os.environ.get("WAIT624") == "1" and cur[0] not in idle_ids and cur[0] != 0x624d:
                t_w = time.perf_counter()
                while time.perf_counter() - t_w < 0.8:
                    c = player()
                    cur = sample(c) if c else (-1, -1, 0, None, None, -1)
                    if cur[0] == 0x624d:
                        break
                    time.sleep(0.004)
            act, fps, node, t60, s40, is30 = cur
            wall = datetime.datetime.now().strftime("%H:%M:%S.%f")[:-3]
            if act in idle_ids:
                log("H%02d still idle %s" % (n, hex(act) if isinstance(act, int) and act >= 0 else act))
                hf.write("%s,%d,%s,%s,%s,%s,%s,%s,-,,,,,0,0,0,0\n" % (
                    tag, n, wall, hex(act) if isinstance(act, int) and act >= 0 else act,
                    "" if t60 is None else "%.5f" % t60,
                    "" if s40 is None else "%.5f" % s40,
                    fps, hex(node) if node else "0"))
                hf.flush()
                continue
            cl.cmd("stick L 0 24000")
            down = cl.cmd("hold Y")
            log("H%02d down %s act %s" % (n, down, hex(act)))
            t_down = time.perf_counter()
            first = None
            first_ms = None
            t624 = t629 = None
            released = False
            while True:
                elapsed = time.perf_counter() - t_down
                c = player()
                now = sample(c) if c else (-1, -1, 0, None, None, -1)
                if now[0] == 0x624d and t624 is None:
                    t624 = elapsed
                if now[0] == 0x629d and t629 is None:
                    t629 = elapsed
                if first is None and now[0] not in (act, -1):
                    first = now[0]
                    first_ms = int(elapsed * 1000)
                if not released and elapsed >= 1.0:
                    cl.cmd("release Y")
                    released = True
                if released and (t629 is not None or elapsed >= 2.0):
                    break
                time.sleep(0.008)
            if not released:
                cl.cmd("release Y")
            ch = 1 if t629 is not None else 0
            late_ms = None if t624 is None or t629 is None else int((t629 - t624) * 1000)
            is_late = 1 if late_ms is not None and late_ms > 300 else 0
            charged += ch
            late_n += is_late
            log("H%02d act %s t60 %s fps %s first %s +%s 624 %s 629 %s late %s %s" % (
                n, hex(act), None if t60 is None else round(t60, 2), fps,
                hex(first) if first is not None else "-", first_ms,
                None if t624 is None else int(t624 * 1000),
                None if t629 is None else int(t629 * 1000),
                late_ms, "LATE" if is_late else ("CHARGED" if ch else "MISS")))
            hf.write("%s,%d,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%d,%d,%d,%d\n" % (
                tag, n, wall, hex(act),
                "" if t60 is None else "%.5f" % t60,
                "" if s40 is None else "%.5f" % s40,
                fps, hex(node) if node else "0",
                hex(first) if first is not None else "-",
                "" if first_ms is None else first_ms,
                "" if t624 is None else int(t624 * 1000),
                "" if t629 is None else int(t629 * 1000),
                "" if late_ms is None else late_ms,
                1 if t624 is not None else 0, ch, ch, is_late))
            hf.flush()
            time.sleep(0.15)
    log("ARM %s charged %d/%d late %d/%d" % (tag, charged, n_holds, late_n, n_holds))
    try:
        cl.cmd("release all")
        cl.cmd("stick L 0 0")
    except Exception:
        pass
    cl.close()
    k32.CloseHandle(h)
    close_game()


def restore():
    close_game()
    if os.path.exists(INI_BAK):
        shutil.copyfile(INI_BAK, INI)
        text = open(INI, "r", encoding="utf-8", errors="replace").read()
        vp = [ln.strip() for ln in text.splitlines() if "VirtualPad" in ln and not ln.strip().startswith(";")]
        mf = [ln.strip() for ln in text.splitlines() if ln.strip().startswith("MaxFps")]
        ha = [ln.strip() for ln in text.splitlines() if ln.strip().startswith("HoldAttackFix")]
        log("restored ini %s %s %s" % (vp, mf, ha))
    if os.path.isdir(BAK):
        subprocess.run(["robocopy", BAK, SAVES, "/MIR", "/R:1", "/W:1", "/NFL", "/NDL", "/NJH", "/NJS"], check=False)
        log("saves restored")
    if os.path.exists(MARKER):
        os.remove(MARKER)
        log("marker removed")


def main():
    os.makedirs(OUT, exist_ok=True)
    ef = hashlib.sha256(open(EF, "rb").read()).hexdigest()
    log("ef %s" % ef)
    if not ef.upper().startswith("1C8E1245"):
        raise SystemExit("EF hash is not 1c8e1245")
    text0 = open(INI, "r", encoding="utf-8", errors="replace").read()
    ha = [ln.strip() for ln in text0.splitlines() if ln.strip().startswith("HoldAttackFix")]
    log("holdattack before %s" % ha)
    if any(ln.split("=")[-1].strip() not in ("0",) for ln in ha):
        raise SystemExit("HoldAttackFix is not 0")
    if nioh_pids():
        raise SystemExit("Nioh already running")
    live, idle = human_block()
    if live:
        raise SystemExit("gamepad activity, not launching")
    if idle < 120000:
        log("idle under 2 min; empty pad, treating as the autoplay clicker")
    shutil.copyfile(INI, INI_BAK)
    log("backup saves")
    subprocess.run(["robocopy", SAVES, BAK, "/MIR", "/R:1", "/W:1", "/NFL", "/NDL", "/NJH", "/NJS"], check=False)
    mode = sys.argv[1] if len(sys.argv) > 1 else "full"
    if mode == "diag":
        log("DIAG one hold then 20s settle")
        run_arm("diag", 0, None, False, n_holds=1, idle_timeout=20.0, watch_after=20.0)
        return
    if mode == "diag2":
        log("DIAG2 one hold then stick to find 0x7d0")
        run_arm("diag2", 0, None, False, n_holds=1, idle_timeout=15.0, seek=True)
        return
    if mode == "diag3":
        log("DIAG3 one hold then 50s still watch")
        run_arm("diag3", 0, None, False, n_holds=1, idle_timeout=12.0, watch_after=50.0)
        return
    if mode == "probe":
        log("PROBE on purpose during 1745")
        run_probe()
        return
    if mode == "probe2":
        log("PROBE2 replay into 1745 then Y")
        run_probe_replay()
        return
    if mode == "mid":
        which = sys.argv[2] if len(sys.argv) > 2 else "u"
        log("MID combo holds, stick held, not from idle (%s)" % which)
        if which == "u":
            run_mid("mU", 0, None, 40)
        elif which == "60":
            run_mid("m60", None, 60, 40)
        else:
            raise SystemExit("mid wants u or 60")
        return
    if mode == "cont":
        which = sys.argv[2] if len(sys.argv) > 2 else "both"
        log("CONT idle is 0x7d0 or 0x6224 for >=1s (%s)" % which)
        if which in ("u", "both"):
            run_arm("u6224", 0, None, False, n_holds=16, idle_timeout=15.0)
        if which in ("60", "both"):
            if which == "both":
                subprocess.run(["robocopy", BAK, SAVES, "/MIR", "/R:1", "/W:1", "/NFL", "/NDL", "/NJH", "/NJS"], check=False)
                log("saves reset before 60")
            run_arm("f6224", None, 60, False, n_holds=16, idle_timeout=15.0)
        return
    if mode in ("reload", "reload60"):
        count = int(sys.argv[2]) if len(sys.argv) > 2 else 8
        start = int(sys.argv[3]) if len(sys.argv) > 3 else 1
        ren = 60 if mode == "reload60" else None
        maxfps = None if mode == "reload60" else 0
        tag = "fps60" if mode == "reload60" else "uncapped"
        log("RELOAD %s holds %d..%d" % (tag, start, start + count - 1))
        for n in range(start, start + count):
            if n != start:
                subprocess.run(["robocopy", BAK, SAVES, "/MIR", "/R:1", "/W:1", "/NFL", "/NDL", "/NJH", "/NJS"], check=False)
                log("saves reset before H%02d" % n)
            run_arm(tag, maxfps, ren, False, n_holds=1, idle_timeout=15.0, hold_base=n, append=(n > 1))
        return
    log("ARM uncapped")
    run_arm("uncapped", 0, None, True)
    log("restore saves between arms")
    subprocess.run(["robocopy", BAK, SAVES, "/MIR", "/R:1", "/W:1", "/NFL", "/NDL", "/NJH", "/NJS"], check=False)
    log("ARM fps60")
    run_arm("fps60", None, 60, True)


if __name__ == "__main__":
    os.makedirs(OUT, exist_ok=True)
    try:
        main()
    except SystemExit as e:
        log("EXIT %s" % e)
        raise
    except Exception as e:
        log("ERR %s" % e)
        raise
    finally:
        try:
            restore()
        except Exception as e:
            log("RESTORE ERR %s" % e)
