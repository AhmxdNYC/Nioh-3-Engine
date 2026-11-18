# TASK #117475 / #117452: hold step 3, TRUE vanilla arm. 0 ASIs in the Nioh3 folder, ViGEm X360 pad,
# same mid-combo 40-Y-hold script as gx-pc #117392 (base.py = copy of holdmeas-116947/idlehold.py run_mid).
import ctypes, datetime, hashlib, os, shutil, subprocess, sys, time
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import base
import vgamepad as vg

OUT = r"D:\nioh3-re\holdmeas-117452"
os.makedirs(OUT, exist_ok=True)
base.OUT = OUT
base.MARKER = r"C:\Users\Ahmad Hamza\AppData\Local\uo-agentd\pc\test-active"
GAME = base.GAME
SAVES = base.SAVES
BAK = os.path.join(OUT, "saves-bak")
ASIDIR = os.path.join(OUT, "asi-out")
ASIS = {"Nioh3EngineFix.asi": "1c8e1245", "Nioh3BossRematch.asi": "64e63f0c",
        "Nioh3Training.asi": "633b2f35", "Nioh3CamKiProbe.asi": "e6142afc"}
log = base.log
S = {"moved": [], "pad": None}
B = vg.XUSB_BUTTON
BTN = {"Y": B.XUSB_GAMEPAD_Y, "A": B.XUSB_GAMEPAD_A, "RB": B.XUSB_GAMEPAD_RIGHT_SHOULDER}


def sha(p):
    return hashlib.sha256(open(p, "rb").read()).hexdigest()[:8]


class Pad:
    """Same cmd() strings as the vpad.Client the base script used, sent to a ViGEm X360 pad."""
    def __init__(self):
        self.p = vg.VX360Gamepad()
        self.p.reset(); self.p.update()

    def cmd(self, line):
        a = line.split()
        if a[0] == "hold":
            self.p.press_button(BTN[a[1]])
        elif a[0] == "release":
            if a[1] == "all":
                self.p.reset()
            else:
                self.p.release_button(BTN[a[1]])
        elif a[0] == "stick":
            x, y = int(float(a[2])), int(float(a[3]))
            x = max(-32768, min(32767, x if abs(x) > 1 else x * 32767))
            y = max(-32768, min(32767, y if abs(y) > 1 else y * 32767))
            (self.p.left_joystick if a[1] == "L" else self.p.right_joystick)(x_value=x, y_value=y)
        elif a[0] == "press":
            self.p.press_button(BTN[a[1]]); self.p.update()
            time.sleep(int(a[2]) / 1000.0 if len(a) > 2 else 0.12)
            self.p.release_button(BTN[a[1]])
        elif a[0] == "state":
            return "vigem"
        self.p.update()
        return "ok"

    def close(self):
        try:
            self.p.reset(); self.p.update()
        except Exception:
            pass


def move_asis_out():
    os.makedirs(ASIDIR, exist_ok=True)
    for name, want in ASIS.items():
        src = os.path.join(GAME, name)
        got = sha(src)
        if got != want:
            raise SystemExit("%s is %s not %s" % (name, got, want))
    for name in ASIS:
        shutil.move(os.path.join(GAME, name), os.path.join(ASIDIR, name))
        S["moved"].append(name)
    left = [f for f in os.listdir(GAME) if f.lower().endswith(".asi")]
    log("asis moved %s left %s" % (S["moved"], left))
    if left:
        raise SystemExit("asi still in folder: %s" % left)


def restore_asis():
    for name in list(S["moved"]):
        shutil.move(os.path.join(ASIDIR, name), os.path.join(GAME, name))
        S["moved"].remove(name)
    hs = {n: sha(os.path.join(GAME, n)) for n in ASIS}
    log("asis restored %s ok %s" % (hs, hs == ASIS))


def launch():
    open(base.MARKER, "w", encoding="utf-8").write("pc/1 hold-vanilla-117452\n")
    auto = subprocess.Popen(["powershell", "-NoProfile", "-ExecutionPolicy", "Bypass", "-File", base.AUTOPLAY,
                             "-Minutes", "15"], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    base.STATE["auto"] = auto.pid
    time.sleep(1)
    os.startfile("steam://rungameid/3681010")
    log("launch requested, autoplay %s" % auto.pid)
    t0 = time.time()
    while time.time() - t0 < 180:
        pids = base.nioh_pids()
        if pids:
            base.STATE["pid"] = pids[0]
            log("pid %s" % pids[0])
            return pids[0]
        time.sleep(1)
    raise SystemExit("game did not start")


def stops(h, gbase):
    import struct
    got = ctypes.c_size_t(); b8 = ctypes.create_string_buffer(8); b6 = ctypes.create_string_buffer(6)
    if not base.k32.ReadProcessMemory(h, ctypes.c_void_p(gbase + 0x4759910), b8, 8, ctypes.byref(got)):
        return None
    g = struct.unpack("<Q", b8.raw)[0]
    if not base.k32.ReadProcessMemory(h, ctypes.c_void_p(g + 0x588), b6, 6, ctypes.byref(got)):
        return None
    return list(b6.raw)


def title_continue(pad, player, sample, h, gbase, sec=300):
    # No EF pipe. field = player present, G+0x58A..0x58C clear, AND the stick actually moves the player
    # (a post-load popup / game menu leaves the player in 0x7d0). Else press A every 4 s, like vpad title-continue.
    t0 = time.time(); last = 0; presses = 0
    while time.time() - t0 < sec:
        if base.STATE["pid"] not in base.nioh_pids():
            raise SystemExit("game exited during title")
        c = player()
        cur = sample(c) if c else (-1, -1, 0, None, None, -1)
        st = stops(h, gbase)
        ok = c and cur[0] > 0 and cur[1] > 0 and st is not None and not (st[2] or st[3] or st[4])
        now = time.time()
        if ok and now - last >= 2:
            pad.cmd("stick L 0 24000")
            t1 = time.time(); moved = None
            while time.time() - t1 < 1.5:
                c = player(); cur = sample(c) if c else (-1,) * 6
                if cur[0] not in (0x7d0, 0x6224, -1):
                    moved = cur[0]; break
                time.sleep(0.03)
            pad.cmd("stick L 0 0")
            log("move test stops %s -> %s" % (st, hex(moved) if moved else "none"))
            if moved:
                time.sleep(1.5)
                log("in field after %.0f s, %d A" % (time.time() - t0, presses))
                return True
        if now - last >= 4 and now - t0 > 20:
            pad.cmd("press A 120"); presses += 1; last = time.time()
            log("[%3.0fs] not in field (player %s act %s fps %s stops %s) press A" % (now - t0, hex(c or 0), cur[0], cur[1], st))
        time.sleep(0.5)
    raise SystemExit("not in field after %d s" % sec)


u32 = ctypes.WinDLL("user32", use_last_error=True)
WNDENUM = ctypes.WINFUNCTYPE(ctypes.c_bool, ctypes.c_void_p, ctypes.c_void_p)


def game_hwnd(pid):
    found = []
    def cb(hw, _):
        p = ctypes.c_ulong()
        u32.GetWindowThreadProcessId(ctypes.c_void_p(hw), ctypes.byref(p))
        if p.value == pid and u32.IsWindowVisible(ctypes.c_void_p(hw)):
            found.append(hw)
        return True
    u32.EnumWindows(WNDENUM(cb), 0)
    return found[0] if found else None


def active_flag(h, gbase):
    try:
        import struct
        buf = ctypes.create_string_buffer(8); got = ctypes.c_size_t()
        base.k32.ReadProcessMemory(h, ctypes.c_void_p(gbase + 0x4759910), buf, 8, ctypes.byref(got))
        g = struct.unpack("<Q", buf.raw)[0]
        b1 = ctypes.create_string_buffer(1)
        base.k32.ReadProcessMemory(h, ctypes.c_void_p(g + 0x1AB), b1, 1, ctypes.byref(got))
        return b1.raw[0]
    except Exception:
        return -1


def padinfo(h, gbase):
    import struct
    got = ctypes.c_size_t()
    def rd(a, n):
        b = ctypes.create_string_buffer(n)
        if not base.k32.ReadProcessMemory(h, ctypes.c_void_p(a), b, n, ctypes.byref(got)):
            raise OSError
        return b.raw
    try:
        m = struct.unpack("<Q", rd(gbase + 0x45BA1E0, 8))[0]
        con = [rd(m + 0x69850 + i * 0x14, 0x14).hex() for i in range(4)]
        act = struct.unpack("<i", rd(m + 0x69B20, 4))[0]
        return "padmgr %s active_slot %s slots %s" % (hex(m), act, con)
    except OSError:
        return "padmgr unreadable"


def focus(pid, h, gbase):
    for i in range(6):
        a = active_flag(h, gbase)
        hw = game_hwnd(pid)
        log("focus try %d active %s hwnd %s fg %s" % (i, a, hw, u32.GetForegroundWindow()))
        if a == 1:
            return True
        if hw:
            u32.keybd_event(0x12, 0, 0, 0); u32.keybd_event(0x12, 0, 2, 0)
            u32.ShowWindow(ctypes.c_void_p(hw), 9)
            u32.SetForegroundWindow(ctypes.c_void_p(hw))
        time.sleep(1.5)
    return active_flag(h, gbase) == 1


def run(n_holds):
    pad = Pad()
    S["pad"] = pad
    log("vigem pad up")
    pid = launch()
    h, player, sample = base.attach(pid)
    import ctypes.wintypes as W
    mods = (ctypes.c_void_p * 1)(); need = W.DWORD()
    base.ps.EnumProcessModules(h, mods, ctypes.sizeof(mods), ctypes.byref(need))
    gbase = mods[0]
    focus(pid, h, gbase)
    title_continue(pad, player, sample, h, gbase)
    time.sleep(2)
    if not focus(pid, h, gbase):
        raise SystemExit("game window not active, pad input ignored")
    S["chk"] = (h, gbase)
    log("pad before Y: %s" % padinfo(h, gbase))
    pad.cmd("hold Y"); time.sleep(0.05); log("pad Y down: %s" % padinfo(h, gbase)); time.sleep(0.3)
    pad.cmd("release all"); time.sleep(1.5)

    # base.run_mid with vpad.Client swapped for the ViGEm pad.
    class FakeVpad:
        OWNER = 0
        @staticmethod
        def Client():
            return pad
    sys.modules["vpad"] = FakeVpad
    real_launch, real_run = base.launch, subprocess.run
    base.launch = lambda maxfps: (pid, base.STATE["auto"])
    subprocess.run = lambda *a, **k: subprocess.CompletedProcess(a, 0, "title skipped (vigem)", "")
    real_attach = base.attach
    base.attach = lambda p: (h, player, sample)
    try:
        base.run_mid("mV", 0, None, n_holds) if False else None
        _orig_log = base.log
        cnt = {"n": 0}
        def log2(msg):
            _orig_log(msg)
            if msg.startswith("H") and (" act " in msg or "still idle" in msg):
                cnt["n"] += 1
                if cnt["n"] % 5 == 0:
                    _orig_log("active flag %s %s" % (active_flag(h, gbase), padinfo(h, gbase)))
        base.log = log2
        base.run_mid(os.environ.get("ARMTAG", "mV"), 0, None, n_holds)
        base.log = _orig_log
    finally:
        base.launch, subprocess.run, base.attach = real_launch, real_run, real_attach


def main():
    if base.nioh_pids():
        raise SystemExit("Nioh already running")
    live, idle = base.human_block()
    if live:
        raise SystemExit("gamepad activity, not launching")
    ini = open(os.path.join(GAME, "Nioh3EngineFix.ini"), encoding="utf-8", errors="replace").read()
    log("ini %s" % [l.strip() for l in ini.splitlines() if l.strip().startswith(("VirtualPad", "MaxFps", "HoldAttackFix"))])
    log("backup saves")
    subprocess.run(["robocopy", SAVES, BAK, "/MIR", "/R:1", "/W:1", "/NFL", "/NDL", "/NJH", "/NJS"], check=False)
    if os.environ.get("KEEPASI") == "1":
        log("KEEPASI: mods stay installed %s" % {n: sha(os.path.join(GAME, n)) for n in ASIS})
    else:
        move_asis_out()
    run(int(sys.argv[1]) if len(sys.argv) > 1 else 40)


if __name__ == "__main__":
    try:
        main()
    except SystemExit as e:
        log("EXIT %s" % e); raise
    except Exception as e:
        log("ERR %r" % e); raise
    finally:
        for step in (base.close_game, restore_asis):
            try:
                step()
            except Exception as e:
                log("RESTORE ERR %s %r" % (step.__name__, e))
        try:
            if S["pad"]:
                S["pad"].close(); del S["pad"].p; S["pad"] = None
                log("vigem pad unplugged")
        except Exception as e:
            log("pad close err %r" % e)
        if os.path.isdir(BAK):
            subprocess.call(["robocopy", BAK, SAVES, "/MIR", "/R:1", "/W:1", "/NFL", "/NDL", "/NJH", "/NJS"])
            log("saves restored")
        if os.path.exists(base.MARKER):
            os.remove(base.MARKER); log("marker removed")
