# GH #157 A/B: external read-only poller of the player's action id and step [ctrl+0x40] (same reads as
# Training player_log.h / player_clock.h), so stock and modded arms are measured identically.
# usage: py -I clkpoll.py OUT.tsv SECONDS
import ctypes, ctypes.wintypes as W, sys, time, subprocess
k32 = ctypes.WinDLL("kernel32", use_last_error=True)
psapi = ctypes.WinDLL("psapi")
ctypes.windll.winmm.timeBeginPeriod(1)
PT_RVA, G_RVA = 0x4745348, 0x4759910

def pid_of():
    o = subprocess.run(["tasklist", "/FI", "IMAGENAME eq Nioh3.exe", "/FO", "CSV", "/NH"], capture_output=True, text=True).stdout
    for l in o.splitlines():
        p = l.split('","')
        if len(p) > 1 and p[0].strip('"').lower() == "nioh3.exe": return int(p[1])
    return 0

pid = pid_of()
if not pid: sys.exit("no Nioh3")
h = k32.OpenProcess(0x0410, False, pid)
mods = (W.HMODULE * 1024)(); need = W.DWORD()
psapi.EnumProcessModulesEx(h, mods, ctypes.sizeof(mods), ctypes.byref(need), 3)
B = mods[0]
buf8 = ctypes.c_uint64(); buf4 = ctypes.c_uint32(); bf = ctypes.c_float(); n = ctypes.c_size_t()
def rd(a, b, sz):
    return k32.ReadProcessMemory(h, ctypes.c_void_p(a), ctypes.byref(b), sz, ctypes.byref(n)) and n.value == sz
def r64(a): return buf8.value if a and rd(a, buf8, 8) else 0
def r32(a): return buf4.value if a and rd(a, buf4, 4) else None
def rf(a): return bf.value if a and rd(a, bf, 4) else None

out = open(sys.argv[1], "w")
out.write("# pid %d base %x\n# act\tstart_s\twall_ms\tgame_ms\tratio\tframes\tmin_step\tmax_step\n" % (pid, B))
dur = float(sys.argv[2]); t0 = time.perf_counter()
act = None; a_t = 0; a_game = 0.0; a_fr = 0; a_min = 99; a_max = -1; cnt0 = None
def close(now):
    if act is None: return
    w = (now - a_t) * 1000
    out.write("0x%X\t%.3f\t%.0f\t%.0f\t%.2f\t%d\t%.3f\t%.3f\n" % (act, a_t - t0, w, a_game, a_game / w if w > 0 else 0, a_fr, a_min if a_fr else 0, a_max if a_fr else 0)); out.flush()
while time.perf_counter() - t0 < dur:
    now = time.perf_counter()
    g = r64(B + G_RVA)
    cnt = r32(g + 0x610) if g else None
    pt = r64(B + PT_RVA); me = r64(pt + 0xCD * 0x18) if pt else 0
    ai = r64(me + 0x3B0) if me else 0; ctrl = r64(ai + 8) if ai else 0
    p = r64(ctrl + 0xE8) if ctrl else 0
    a = (r32(p) if p else 0xFFFF) if ctrl else None
    step = rf(ctrl + 0x40) if ctrl else None
    if a is not None and a != act:
        close(now); act = a; a_t = now; a_game = 0.0; a_fr = 0; a_min = 99; a_max = -1
    if cnt is not None:
        if cnt0 is not None:
            k = (cnt - cnt0) & 0xFFFFFFFF
            if 0 < k <= 8 and step is not None and 0 <= step < 16:
                a_game += step * k * 1000 / 60; a_fr += k; a_min = min(a_min, step); a_max = max(a_max, step)
        cnt0 = cnt
    time.sleep(0.0005)
close(time.perf_counter())
