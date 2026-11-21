# pc/1 #128506: read-only poller of the player's anim-speed inputs (FUN 0x20DAD4 / 0x134E68 map).
# usage: py -I modpoll.py OUT.tsv SECONDS
import ctypes, ctypes.wintypes as W, sys, time, subprocess
k32 = ctypes.WinDLL("kernel32", use_last_error=True); psapi = ctypes.WinDLL("psapi")
ctypes.windll.winmm.timeBeginPeriod(1)
PT_RVA = 0x4745348
o = subprocess.run(["tasklist","/FI","IMAGENAME eq Nioh3.exe","/FO","CSV","/NH"],capture_output=True,text=True).stdout
pid = next((int(l.split('","')[1]) for l in o.splitlines() if l.lower().startswith('"nioh3.exe')), 0)
if not pid: sys.exit("no Nioh3")
h = k32.OpenProcess(0x0410, False, pid)
mods = (W.HMODULE*1024)(); need = W.DWORD(); psapi.EnumProcessModulesEx(h, mods, ctypes.sizeof(mods), ctypes.byref(need), 3); B = mods[0]
n = ctypes.c_size_t()
def rb(a, sz):
    b = ctypes.create_string_buffer(sz)
    return b.raw if a and k32.ReadProcessMemory(h, ctypes.c_void_p(a), b, sz, ctypes.byref(n)) and n.value == sz else None
import struct
def r64(a):
    r = rb(a, 8); return struct.unpack('<Q', r)[0] if r else 0
out = open(sys.argv[1], "w"); out.write("# pid %d base %x\n" % (pid, B))
dur = float(sys.argv[2]); t0 = time.perf_counter(); last = None; cnt = 0
while time.perf_counter() - t0 < dur:
    pt = r64(B + PT_RVA); me = r64(pt + 0xCD*0x18) if pt else 0
    ai = r64(me + 0x3B0) if me else 0; ctrl = r64(ai + 8) if ai else 0
    blk = rb(ctrl, 0xE00) if ctrl else None
    if not blk: time.sleep(0.05); continue
    f = lambda off: struct.unpack_from('<f', blk, off)[0]
    p = struct.unpack_from('<Q', blk, 0xE8)[0]; a = rb(p, 4); act = struct.unpack('<I', a)[0] if a else 0xFFFF
    mods_s = []
    for i in range(19):
        op = struct.unpack_from('<Q', blk, 0x6B8 + 8*i)[0]; ob = rb(op, 0x14)
        if ob:
            rem, fac = struct.unpack_from('<ff', ob, 8)
            vt = struct.unpack_from('<Q', ob, 0)[0] - B
            if rem > 0 or abs(fac - 1.0) > 1e-4: mods_s.append("m%d:%x:%.3f/%.2f" % (i, vt, fac, rem))
    row = "0x%X\tstep %.3f\tdc4 %.3f\tdc0 %.3f\tmult %.3f/%.2f\tfrz %.2f/%.2f\tt %.2f\t%s" % (act, f(0x40), f(0xDC4), f(0xDC0), f(0x3CC), f(0x3D0), f(0x488), f(0x48C), f(0x60), " ".join(mods_s))
    key = row.split("\tt ")[0].split("\t",2)[2] + " ".join(mods_s)
    cnt += 1
    if key != last or cnt % 200 == 0:
        out.write("%.3f\t%s\n" % (time.perf_counter() - t0, row)); out.flush(); last = key
    time.sleep(0.004)
