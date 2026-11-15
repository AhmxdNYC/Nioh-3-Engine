"""padrec.py - input recorder: record a real pad session to JSONL, replay it through the VirtualPad pipe (notes\\virtual-input.md).

Off by default: nothing records unless this tool is started. The mod has no recorder; the replay uses the VirtualPad pipe
([Debug] VirtualPad = 1 or 2, default 0) and its 'pad' command (EngineFix with #105757: the whole pad state in one step).

    py -I padrec.py record OUT.jsonl [--sec N] [--slot 0] [--poll-ms 2] [--state-ms 250] [--pipe NAME]
        Polls the real controller with XInput from this process (the game is not touched; XInput needs no focus) and writes
        one line per change: {"t": ms since start, "pad": [buttons, lt, rt, lx, ly, rx, ry] or null (disconnected),
        "state": tag}. The game state tag comes from the VirtualPad pipe 'state' reply (record with VirtualPad = 2 so the
        real pad still drives the game): field / loading / menu / paused / unknown (pipe up, game state not readable) /
        nogame (no pipe). Stops after --sec (default: until Ctrl+C).
    py -I padrec.py replay IN.jsonl [--pipe NAME] [--speed 1.0] [--wait-state]
        Sends each recorded state as one 'pad' command at its recorded time (time.perf_counter schedule, the last 2 ms spun),
        then 'pad 0 0 0 0 0 0 0'. --wait-state: first wait until the game's state tag equals the first event's tag.
        Exit 0 ok, 1 the mod answered err (an EngineFix without the 'pad' command says so), 2 no pipe.
    py -I padrec.py info IN.jsonl        events, duration, tags, buttons seen
Offline round trip: mod\\Nioh3EngineFix\\tests\\run_padrec_test.py (recorder on a scripted pad -> replay into the real
VirtualPad code with a fake 60 Hz game -> identical event stream within 1 frame).
"""
import ctypes
import json
import os
import sys
import threading
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import vpad  # noqa: E402

FORMAT = "nioh3-padrec"
VERSION = 1
NEUTRAL = (0, 0, 0, 0, 0, 0, 0)


# ---- XInput (this process; the game's own XInput import is never touched) ----
class _GP(ctypes.Structure):
    _fields_ = [("wButtons", ctypes.c_uint16), ("bLeftTrigger", ctypes.c_uint8), ("bRightTrigger", ctypes.c_uint8),
                ("sThumbLX", ctypes.c_int16), ("sThumbLY", ctypes.c_int16), ("sThumbRX", ctypes.c_int16), ("sThumbRY", ctypes.c_int16)]


class _ST(ctypes.Structure):
    _fields_ = [("dwPacketNumber", ctypes.c_uint32), ("Gamepad", _GP)]


def xinput_reader(slot=0):
    """-> read() returning a 7-tuple, or None while the pad is not connected"""
    dll = None
    for name in ("xinput1_4", "xinput1_3", "xinput9_1_0"):
        try:
            dll = ctypes.WinDLL(name)
            break
        except OSError:
            continue
    if dll is None:
        raise RuntimeError("no XInput DLL")
    get = dll.XInputGetState
    get.argtypes = [ctypes.c_uint32, ctypes.POINTER(_ST)]
    get.restype = ctypes.c_uint32
    st = _ST()

    def read():
        if get(slot, ctypes.byref(st)) != 0:
            return None
        g = st.Gamepad
        return (g.wButtons, g.bLeftTrigger, g.bRightTrigger, g.sThumbLX, g.sThumbLY, g.sThumbRX, g.sThumbRY)

    return read


def state_tag(reply):
    """VirtualPad 'state' reply -> tag"""
    if not reply or not reply.startswith("ok"):
        return "unknown"
    d = vpad.parse_state(reply)
    if "field" not in d:
        return "unknown"  # game=? (unknown build / GameMain not up)
    if d.get("field") == "1":
        return "field"
    if d.get("loading") == "1":
        return "loading"
    if d.get("menu_pause", "0") not in ("0", "-1"):
        return "menu"
    if d.get("paused") == "1":
        return "paused"
    return "unknown"


class StatePoller:
    """game state tag from the pipe on its own thread, so a slow reply never stalls the pad poll"""

    def __init__(self, pipe, every_ms, source=None):
        self.tag = "nogame"
        self.every = every_ms / 1000.0
        self.pipe = pipe
        self.source = source  # tests: callable() -> tag
        self.stop = threading.Event()
        self.t = threading.Thread(target=self.run, daemon=True)
        self.t.start()

    def run(self):
        c = None
        while not self.stop.is_set():
            if self.source:
                self.tag = self.source()
            else:
                try:
                    if c is None:
                        c = vpad.Client(self.pipe, 0.2)
                    self.tag = state_tag(c.cmd("state"))
                except (vpad.NoPipe, OSError):
                    if c:
                        c.close()
                    c = None
                    self.tag = "nogame"
            self.stop.wait(self.every)
        if c:
            c.close()


def record(out_path, sec=0.0, slot=0, poll_ms=2.0, state_ms=250, pipe=vpad.PIPE, read=None, state_source=None, quiet=False):
    """Writes the JSONL file; returns the number of events. read / state_source: tests only (scripted pad and tags)."""
    read = read or xinput_reader(slot)
    _fine_timer()
    poller = StatePoller(pipe, state_ms, state_source)
    if state_source is None:
        time.sleep(min(0.3, state_ms / 1000.0 + 0.05))  # first tag before the first event
    else:
        poller.tag = state_source()
    n = 0
    last = ()
    last_tag = None
    t0 = time.perf_counter()
    with open(out_path, "w", encoding="utf-8", newline="\n") as f:
        f.write(json.dumps({"type": "header", "format": FORMAT, "version": VERSION, "slot": slot, "poll_ms": poll_ms,
                            "started": time.strftime("%Y-%m-%dT%H:%M:%S%z"), "clock": "perf_counter"}) + "\n")
        try:
            while True:
                now = time.perf_counter()
                el = now - t0
                if sec and el >= sec:
                    break
                p = read()
                tag = poller.tag
                if p != last or tag != last_tag:
                    f.write(json.dumps({"t": round(el * 1000.0, 3), "pad": list(p) if p is not None else None, "state": tag}) + "\n")
                    n += 1
                    last, last_tag = p, tag
                nxt = t0 + (int(el * 1000.0 / poll_ms) + 1) * poll_ms / 1000.0
                _sleep_until(nxt, spin_ms=0.5)
        except KeyboardInterrupt:
            pass
        f.write(json.dumps({"type": "end", "t": round((time.perf_counter() - t0) * 1000.0, 3), "events": n}) + "\n")
    poller.stop.set()
    if not quiet:
        print("recorded %d events in %.1f s -> %s" % (n, time.perf_counter() - t0, out_path))
    return n


def load(path):
    """-> (header, [(t_ms, 7-tuple, tag)]); raises ValueError on a bad file"""
    head, ev, end = None, [], None
    with open(path, encoding="utf-8") as f:
        for i, ln in enumerate(f, 1):
            ln = ln.strip()
            if not ln:
                continue
            o = json.loads(ln)
            if o.get("type") == "header":
                if o.get("format") != FORMAT or o.get("version") != VERSION:
                    raise ValueError("line %d: not a %s v%d file" % (i, FORMAT, VERSION))
                head = o
                continue
            if o.get("type") == "end":
                end = o
                continue
            if head is None:
                raise ValueError("line %d: event before the header" % i)
            p = o.get("pad")
            if p is None:
                p = NEUTRAL  # pad disconnected: the game sees a neutral pad
            if len(p) != 7 or not all(isinstance(v, int) for v in p) or not (0 <= p[0] <= 0xFFFF and 0 <= p[1] <= 255 and 0 <= p[2] <= 255
                                                                              and all(-32768 <= v <= 32767 for v in p[3:])):
                raise ValueError("line %d: bad pad %r" % (i, p))
            t = float(o["t"])
            if ev and t < ev[-1][0]:
                raise ValueError("line %d: time goes back" % i)
            ev.append((t, tuple(p), o.get("state", "unknown")))
    if head is None:
        raise ValueError("no header")
    if end is not None and end.get("events") != len(ev):
        raise ValueError("end says %s events, file has %d" % (end.get("events"), len(ev)))
    return head, ev


def pad_cmd(p):
    return "pad %04X %d %d %d %d %d %d" % p


_timer_res = []


def _fine_timer():
    """1 ms Windows timer resolution for this process (time.sleep otherwise wakes up to 15.6 ms late)"""
    if not _timer_res:
        try:
            ctypes.WinDLL("winmm").timeBeginPeriod(1)
        except OSError:
            pass
        _timer_res.append(1)


def _sleep_until(t, spin_ms=2.0):
    while True:
        d = t - time.perf_counter()
        if d <= 0:
            return
        if d > spin_ms / 1000.0:
            time.sleep(d - spin_ms / 1000.0)


def replay(path, pipe=vpad.PIPE, speed=1.0, wait_state=False, quiet=False, wait_state_sec=120):
    """-> (rc, info). info: t0 (perf_counter s), sent, max_late_ms, events"""
    _fine_timer()
    _, ev = load(path)
    info = {"events": len(ev), "sent": 0, "max_late_ms": 0.0, "t0": None}
    try:
        c = vpad.Client(pipe)
    except vpad.NoPipe as e:
        if not quiet:
            print("VPAD-NOPIPE: %s (game not running, or [Debug] VirtualPad = 0)" % e)
        return 2, info
    try:
        if wait_state and ev:
            want, tw = ev[0][2], time.time()
            while state_tag(c.cmd("state")) != want:
                if time.time() - tw > wait_state_sec:
                    if not quiet:
                        print("timeout: game state never became %r" % want)
                    return 1, info
                time.sleep(0.25)
        r = c.cmd(pad_cmd(NEUTRAL))
        if not r.startswith("ok"):
            if not quiet:
                print("pad command refused: %s (needs EngineFix with the VirtualPad 'pad' command, #105757)" % r)
            return 1, info
        t0 = time.perf_counter() + 0.05
        info["t0"] = t0
        for t, p, _ in ev:
            due = t0 + t / 1000.0 / speed
            _sleep_until(due)
            late = (time.perf_counter() - due) * 1000.0
            r = c.cmd(pad_cmd(p))
            if not r.startswith("ok"):
                if not quiet:
                    print("err at t=%.1f ms: %s" % (t, r))
                return 1, info
            info["sent"] += 1
            info["max_late_ms"] = max(info["max_late_ms"], late)
        info["t_end"] = time.perf_counter()
    finally:
        # best effort: an error or Ctrl-C must not leave buttons held for the pad hold timeout (#106023)
        try:
            c.cmd(pad_cmd(NEUTRAL))
        except BaseException:
            pass
        c.close()
    if not quiet:
        print("replayed %d events, max late %.2f ms" % (info["sent"], info["max_late_ms"]))
    return 0, info


def main(argv):
    if not argv or argv[0] not in ("record", "replay", "info") or len(argv) < 2:
        print(__doc__)
        return 2
    op, path, opts = argv[0], argv[1], argv[2:]

    def opt(name, default, cast):
        if name in opts:
            return cast(opts[opts.index(name) + 1])
        return default

    pipe = opt("--pipe", vpad.PIPE, str)
    if op == "record":
        record(path, opt("--sec", 0.0, float), opt("--slot", 0, int), opt("--poll-ms", 2.0, float), opt("--state-ms", 250, int), pipe)
        return 0
    if op == "replay":
        rc, _ = replay(path, pipe, opt("--speed", 1.0, float), "--wait-state" in opts)
        return rc
    head, ev = load(path)
    btn = 0
    for _, p, _ in ev:
        btn |= p[0]
    tags = sorted({e[2] for e in ev})
    print("%s: %d events, %.1f s, slot %s, poll %s ms, tags %s, buttons seen %04X" % (
        path, len(ev), ev[-1][0] / 1000.0 if ev else 0, head.get("slot"), head.get("poll_ms"), ",".join(tags), btn))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
