"""Record XInput controller state changes (read-only) to a CSV for later replay/analysis.

Usage: py -I tools/padrec/padrec.py OUT.csv [--minutes N] [--hz 1000]
Columns: t_ms (since start, QPC), wall (local time), pad, packet, buttons (hex), lt, rt, lx, ly, rx, ry
Writes a row only when buttons, triggers or sticks change. Never sends input.
"""
import ctypes, ctypes.wintypes as wt, sys, time, datetime, argparse

class GAMEPAD(ctypes.Structure):
    _fields_ = [("wButtons", wt.WORD), ("bLeftTrigger", ctypes.c_ubyte), ("bRightTrigger", ctypes.c_ubyte),
                ("sThumbLX", ctypes.c_short), ("sThumbLY", ctypes.c_short),
                ("sThumbRX", ctypes.c_short), ("sThumbRY", ctypes.c_short)]

class STATE(ctypes.Structure):
    _fields_ = [("dwPacketNumber", wt.DWORD), ("Gamepad", GAMEPAD)]

def load_xinput():
    for n in ("xinput1_4", "xinput1_3", "xinput9_1_0"):
        try:
            return ctypes.WinDLL(n)
        except OSError:
            pass
    raise SystemExit("no XInput dll")

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("out")
    ap.add_argument("--minutes", type=float, default=60)
    ap.add_argument("--hz", type=float, default=1000)
    a = ap.parse_args()
    xi = load_xinput()
    ctypes.windll.winmm.timeBeginPeriod(1)
    st = STATE()
    last = [None] * 4
    t0 = time.perf_counter()
    end = t0 + a.minutes * 60
    period = 1.0 / a.hz
    rows = 0
    with open(a.out, "w", buffering=1) as f:
        f.write("t_ms,wall,pad,packet,buttons,lt,rt,lx,ly,rx,ry\n")
        f.write("#start,%s\n" % datetime.datetime.now().isoformat(timespec="milliseconds"))
        while time.perf_counter() < end:
            for i in range(4):
                if xi.XInputGetState(i, ctypes.byref(st)) != 0:
                    if last[i] is not None:
                        f.write("%.3f,%s,%d,disconnected\n" % ((time.perf_counter() - t0) * 1000,
                                datetime.datetime.now().strftime("%H:%M:%S.%f")[:-3], i))
                        last[i] = None
                    continue
                g = st.Gamepad
                key = (g.wButtons, g.bLeftTrigger, g.bRightTrigger, g.sThumbLX, g.sThumbLY, g.sThumbRX, g.sThumbRY)
                if key == last[i]:
                    continue  # packet number alone ticks without any state change on some pads
                last[i] = key
                f.write("%.3f,%s,%d,%d,%04X,%d,%d,%d,%d,%d,%d\n" % (
                    (time.perf_counter() - t0) * 1000, datetime.datetime.now().strftime("%H:%M:%S.%f")[:-3],
                    i, st.dwPacketNumber, g.wButtons, g.bLeftTrigger, g.bRightTrigger,
                    g.sThumbLX, g.sThumbLY, g.sThumbRX, g.sThumbRY))
                rows += 1
            time.sleep(period)
    print("rows", rows)

if __name__ == "__main__":
    main()
