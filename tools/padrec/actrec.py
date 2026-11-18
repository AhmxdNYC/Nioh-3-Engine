"""Read-only logger of the player's action id in Nioh3.exe (v2.02) for #98 hold debugging.

Usage: py -I tools/padrec/actrec.py OUT.csv [--minutes N]
Pointer chain (from Temp/h98v2/scripts/hold.py): player=[[base+4745348]+0xCD*0x18]; act=u32[[[[pl+3B0]+8]+E8]].
fps_i = i32 [[base+4759910]+614]. Logs a row per action change (2 ms poll): t_ms, wall, act, prev, prev_ms, fps_i.
Known ids: 0x624d heavy (strong) start, 0x629d hold martial art (MA), 0x6310/0x6311 follow-ups.
Only ReadProcessMemory; never writes to the game. Re-resolves the player each second (area loads).
"""
import sys, time, struct, ctypes, ctypes.wintypes as W, datetime, argparse, subprocess

k32 = ctypes.WinDLL("kernel32", use_last_error=True); ps = ctypes.WinDLL("psapi")
k32.OpenProcess.restype = W.HANDLE
k32.ReadProcessMemory.argtypes = [W.HANDLE, ctypes.c_void_p, ctypes.c_void_p, ctypes.c_size_t, ctypes.POINTER(ctypes.c_size_t)]


def main():
    ap = argparse.ArgumentParser(); ap.add_argument("out"); ap.add_argument("--minutes", type=float, default=90)
    a = ap.parse_args()
    pid = int(subprocess.run(["powershell", "-NoProfile", "-c", "(Get-Process Nioh3).Id"],
                             capture_output=True, text=True).stdout.strip())
    h = k32.OpenProcess(0x100410, 0, pid)
    mods = (ctypes.c_void_p * 1)(); need = W.DWORD()
    ps.EnumProcessModules(h, mods, ctypes.sizeof(mods), ctypes.byref(need)); base = mods[0]

    def rd(addr, n):
        b = ctypes.create_string_buffer(n); got = ctypes.c_size_t()
        if not addr or not k32.ReadProcessMemory(h, ctypes.c_void_p(addr), b, n, ctypes.byref(got)):
            raise OSError
        return b.raw

    def rq(addr): return struct.unpack("<Q", rd(addr, 8))[0]

    def player():
        try: return rq(rq(base + 0x4745348) + 0xCD * 0x18)
        except OSError: return 0

    def act(c):
        try: return struct.unpack("<I", rd(rq(rq(rq(c + 0x3B0) + 8) + 0xE8), 4))[0]
        except OSError: return -1

    def fpsi():
        try: return struct.unpack("<i", rd(rq(base + 0x4759910) + 0x614, 4))[0]
        except OSError: return -1

    ctypes.windll.winmm.timeBeginPeriod(1)
    t0 = time.perf_counter(); end = t0 + a.minutes * 60
    c = player(); tc = t0; prev = None; tprev = t0
    with open(a.out, "w", buffering=1) as f:
        f.write("t_ms,wall,act,prev,prev_ms,fps_i\n")
        f.write("#start,%s,pid %d,base %x,player %x\n" % (datetime.datetime.now().isoformat(timespec="milliseconds"), pid, base, c))
        while time.perf_counter() < end:
            t = time.perf_counter()
            if t - tc > 1.0:
                c = player(); tc = t
            v = act(c) if c else -1
            if v != prev:
                f.write("%.3f,%s,%s,%s,%.0f,%d\n" % ((t - t0) * 1000, datetime.datetime.now().strftime("%H:%M:%S.%f")[:-3],
                        hex(v) if v >= 0 else "-1", hex(prev) if prev not in (None, -1) else str(prev),
                        (t - tprev) * 1000, fpsi()))
                prev = v; tprev = t
            time.sleep(0.002)
            if k32.WaitForSingleObject(h, 0) == 0:
                f.write("#game exited\n"); break


if __name__ == "__main__":
    main()
