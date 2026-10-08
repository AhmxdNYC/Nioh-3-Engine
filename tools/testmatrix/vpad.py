"""vpad.py - client for the mod's VirtualPad pipe (ASI 1.7.37, [Debug] VirtualPad = 1; notes\\virtual-input.md).

No OS-level input: commands go to \\\\.\\pipe\\nioh3vpad inside the game, so the game does not need the foreground and the
user's mouse / keyboard are never touched. Exit codes: 0 ok, 1 the mod answered "err", 2 no pipe (game not running or
VirtualPad off), 3 owner process gone, 4 timeout.

    py -I vpad.py <command ...>               one command (e.g. press A 100 | stick L 0 1 2000 | menu set GF 1 | state)
    py -I vpad.py run <file|->                one command per line ('#' comments); stops at the first err
    py -I vpad.py wait-pipe [sec]             wait until the game's pipe exists (default 180 s)
    py -I vpad.py in-game                     exit 0 when 'state' says field=1 (player in the field, not loading / paused / menu)
    py -I vpad.py title-continue [sec]        boot screens -> title -> Continue -> field, by pressing A while not in the field
                                              and not loading; exit 0 when field=1 holds for 3 s (default timeout 300 s)
    py -I vpad.py selftest                    offline: fake pipe server + the client paths above (no game)
Options (before the command): --owner-pid N (abort with exit 3 when that process is gone), --pipe NAME, --quiet.
"""
import ctypes
import os
import sys
import threading
import time

PIPE = r"\\.\pipe\nioh3vpad"
ERROR_PIPE_BUSY = 231
k32 = ctypes.WinDLL("kernel32", use_last_error=True)
k32.OpenProcess.restype = ctypes.c_void_p
k32.OpenProcess.argtypes = [ctypes.c_uint32, ctypes.c_int, ctypes.c_uint32]
k32.GetExitCodeProcess.argtypes = [ctypes.c_void_p, ctypes.POINTER(ctypes.c_uint32)]
k32.CloseHandle.argtypes = [ctypes.c_void_p]
k32.WaitNamedPipeW.argtypes = [ctypes.c_wchar_p, ctypes.c_uint32]

OWNER = 0
QUIET = False


class NoPipe(Exception):
    pass


def owner_alive():
    if not OWNER:
        return True
    h = k32.OpenProcess(0x1000, 0, OWNER)  # PROCESS_QUERY_LIMITED_INFORMATION
    if not h:
        return False
    code = ctypes.c_uint32()
    ok = k32.GetExitCodeProcess(h, ctypes.byref(code))
    k32.CloseHandle(h)
    return bool(ok) and code.value == 259  # STILL_ACTIVE


def check_owner():
    if not owner_alive():
        print("VPAD-ABORT: owner pid %d is gone" % OWNER)
        sys.exit(3)


class Client:
    def __init__(self, pipe=PIPE, connect_sec=5.0):
        t0 = time.time()
        while True:
            try:
                self.f = open(pipe, "r+b", buffering=0)
                break
            except OSError as e:
                busy = getattr(e, "winerror", 0) == ERROR_PIPE_BUSY
                if time.time() - t0 > connect_sec:
                    raise NoPipe("%s: %s" % (pipe, e))
                if busy:
                    k32.WaitNamedPipeW(pipe, 500)
                else:
                    time.sleep(0.2)
        self.buf = b""

    def cmd(self, line):
        self.f.write(line.strip().encode("utf-8") + b"\n")
        while b"\n" not in self.buf:
            chunk = self.f.read(4096)
            if not chunk:
                raise NoPipe("pipe closed")
            self.buf += chunk
        ln, self.buf = self.buf.split(b"\n", 1)
        return ln.decode("utf-8", "replace").strip()

    def close(self):
        try:
            self.f.close()
        except OSError:
            pass


def parse_state(s):
    d = {}
    for tok in s.split()[1:]:
        if "=" in tok:
            k, v = tok.split("=", 1)
            d[k] = v
    return d


def say(s):
    if not QUIET:
        print(s, flush=True)


def wait_pipe(pipe, sec):
    t0 = time.time()
    while time.time() - t0 < sec:
        check_owner()
        try:
            c = Client(pipe, 1.0)
            r = c.cmd("ping")
            c.close()
            say(r)
            return 0
        except NoPipe:
            time.sleep(2)
    say("timeout: no pipe %s after %d s" % (pipe, sec))
    return 4


def title_continue(pipe, sec):
    """Boot -> field. Presses A every 4 s while not in the field and not loading (logos, the autosave notice, 'press any
    button', Continue on the title menu, the load tip page and post-load popups all take A). Done when field=1 for 3 s."""
    t0 = time.time()
    if wait_pipe(pipe, min(sec, 240)) != 0:
        return 4
    c = Client(pipe)
    last_press, field_since, presses, last_frame, frame_t = 0.0, None, 0, -1, time.time()
    try:
        while time.time() - t0 < sec:
            check_owner()
            st = parse_state(c.cmd("state"))
            now = time.time()
            frame = int(st.get("frame", "-1")) if st.get("frame", "-1").lstrip("-").isdigit() else -1
            if frame != last_frame:
                last_frame, frame_t = frame, now
            if st.get("field") == "1":
                field_since = field_since or now
                if now - field_since >= 3:
                    say("in the field after %.0f s, %d A presses: %s" % (now - t0, presses, " ".join("%s=%s" % kv for kv in st.items())))
                    return 0
            else:
                field_since = None
                frames_moving = now - frame_t < 2
                if st.get("loading") != "1" and frames_moving and now - last_press >= 4:
                    r = c.cmd("press A 120")
                    presses += 1
                    last_press = now
                    say("[%3.0fs] not in the field (loading=%s paused=%s menu_pause=%s player=%s) -> press A: %s" % (
                        now - t0, st.get("loading"), st.get("paused"), st.get("menu_pause"), st.get("player"), r))
            time.sleep(1)
    finally:
        c.close()
    say("timeout: not in the field after %d s" % sec)
    return 4


def run_lines(pipe, lines):
    c = Client(pipe)
    try:
        for ln in lines:
            ln = ln.strip()
            if not ln or ln.startswith("#"):
                continue
            check_owner()
            r = c.cmd(ln)
            say("%s -> %s" % (ln, r))
            if not r.startswith("ok"):
                return 1
    finally:
        c.close()
    return 0


# ---- offline selftest: a fake server that answers like the mod ----
def selftest():
    import subprocess

    name = r"\\.\pipe\nioh3vpad_selftest_%d" % os.getpid()
    PIPE_ACCESS_DUPLEX, PIPE_TYPE_BYTE, PIPE_WAIT = 3, 0, 0
    k32.CreateNamedPipeW.restype = ctypes.c_void_p
    k32.CreateNamedPipeW.argtypes = [ctypes.c_wchar_p, ctypes.c_uint32, ctypes.c_uint32, ctypes.c_uint32, ctypes.c_uint32,
                                     ctypes.c_uint32, ctypes.c_uint32, ctypes.c_void_p]
    k32.ConnectNamedPipe.argtypes = [ctypes.c_void_p, ctypes.c_void_p]
    k32.ReadFile.argtypes = [ctypes.c_void_p, ctypes.c_void_p, ctypes.c_uint32, ctypes.POINTER(ctypes.c_uint32), ctypes.c_void_p]
    k32.WriteFile.argtypes = k32.ReadFile.argtypes
    k32.DisconnectNamedPipe.argtypes = [ctypes.c_void_p]
    state = {"t": time.time(), "presses": 0}

    def answer(line):
        el = time.time() - state["t"]
        if line == "ping":
            return "ok vpad fake mode=exclusive armed=1 unfocused=on"
        if line.startswith("press"):
            state["presses"] += 1
            return "ok press 1000 120 ms (8 rounds)"
        if line == "state":  # boot 0-2 s, loading 2-4 s after the 2nd press, then field
            field = state["presses"] >= 2 and el > 4
            return "ok frame=%d active=0 loading=%d paused=0 player=%d pos=0,0,0 field=%d menu_pause=0 fg=0" % (
                int(el * 60), 1 if (state["presses"] >= 2 and el <= 4) else 0, 1 if field else 0, 1 if field else 0)
        if line.startswith("bogus"):
            return "err unknown command (help lists them)"
        return "ok " + line

    def serve():
        while True:
            h = k32.CreateNamedPipeW(name, PIPE_ACCESS_DUPLEX, PIPE_TYPE_BYTE | PIPE_WAIT, 8, 4096, 4096, 0, None)
            if not h or h == ctypes.c_void_p(-1).value:
                return
            k32.ConnectNamedPipe(h, None)
            threading.Thread(target=client, args=(h,), daemon=True).start()

    def client(h):
        buf = b""
        tmp = ctypes.create_string_buffer(4096)
        n = ctypes.c_uint32()
        while k32.ReadFile(h, tmp, 4096, ctypes.byref(n), None) and n.value:
            buf += tmp.raw[:n.value]
            while b"\n" in buf:
                ln, buf = buf.split(b"\n", 1)
                out = (answer(ln.decode().strip()) + "\n").encode()
                w = ctypes.c_uint32()
                k32.WriteFile(h, out, len(out), ctypes.byref(w), None)
        k32.DisconnectNamedPipe(h)
        k32.CloseHandle(h)

    threading.Thread(target=serve, daemon=True).start()
    fails = []
    me = os.path.abspath(__file__)

    def run(*a):
        return subprocess.run([sys.executable, "-I", me, "--pipe", name] + list(a), capture_output=True, text=True, timeout=60)

    r = run("ping")
    if r.returncode != 0 or "ok vpad fake" not in r.stdout:
        fails.append("ping: %s %s" % (r.returncode, r.stdout))
    r = run("bogus")
    if r.returncode != 1:
        fails.append("err reply must exit 1 (%s)" % r.returncode)
    r = subprocess.run([sys.executable, "-I", me, "--pipe", name + "_missing", "ping"], capture_output=True, text=True, timeout=60)
    if r.returncode != 2:
        fails.append("missing pipe must exit 2 (%s)" % r.returncode)
    r = run("--owner-pid", "999999", "wait-pipe", "5")
    if r.returncode != 3:
        fails.append("dead owner must exit 3 (%s)" % r.returncode)
    state["t"] = time.time()
    r = run("title-continue", "40")
    if r.returncode != 0 or "in the field" not in r.stdout:
        fails.append("title-continue: %s %s" % (r.returncode, r.stdout[-400:]))
    r = run("in-game")
    if r.returncode != 0:
        fails.append("in-game after the field: %s" % r.returncode)
    p = subprocess.run([sys.executable, "-I", me, "--pipe", name, "run", "-"], input="# c\nping\nmenu set GF 1\nbogus\nping\n",
                       capture_output=True, text=True, timeout=60)
    if p.returncode != 1 or p.stdout.count("->") != 3:
        fails.append("run stops at the first err: %s %s" % (p.returncode, p.stdout))
    if fails:
        print("FAILED:\n  " + "\n  ".join(fails))
        return 1
    print("VPAD.PY SELFTEST: ALL PASSED (ping, err exit 1, missing pipe exit 2, dead owner exit 3, title-continue, in-game, run)")
    return 0


def main(argv):
    global OWNER, QUIET
    pipe = PIPE
    while argv and argv[0].startswith("--"):
        if argv[0] == "--owner-pid":
            OWNER = int(argv[1])
            argv = argv[2:]
        elif argv[0] == "--pipe":
            pipe = argv[1]
            argv = argv[2:]
        elif argv[0] == "--quiet":
            QUIET = True
            argv = argv[1:]
        else:
            print(__doc__)
            return 2
    if not argv:
        print(__doc__)
        return 2
    check_owner()
    try:
        if argv[0] == "selftest":
            return selftest()
        if argv[0] == "wait-pipe":
            return wait_pipe(pipe, int(argv[1]) if len(argv) > 1 else 180)
        if argv[0] == "title-continue":
            return title_continue(pipe, int(argv[1]) if len(argv) > 1 else 300)
        if argv[0] == "in-game":
            c = Client(pipe)
            r = c.cmd("state")
            c.close()
            say(r)
            return 0 if parse_state(r).get("field") == "1" else 1
        if argv[0] == "run":
            src = sys.stdin.read() if len(argv) < 2 or argv[1] == "-" else open(argv[1], encoding="utf-8").read()
            return run_lines(pipe, src.splitlines())
        c = Client(pipe)
        r = c.cmd(" ".join(argv))
        c.close()
        print(r)
        return 0 if r.startswith("ok") else 1
    except NoPipe as e:
        print("VPAD-NOPIPE: %s (game not running, or [Debug] VirtualPad = 0)" % e)
        return 2


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
