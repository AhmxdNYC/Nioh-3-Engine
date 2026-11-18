# hold.py TAG N OUTDIR: Windows port of ally arm.py (#98). N x (Y, RB, LB) 'press 1000' via VirtualPad pipe; samples the player's
# action id (player=[[base+4745348]+0xCD*0x18]; ctrl=[[pl+3B0]+8]; act=u32[[ctrl+E8]]) and G+614 fps every 2 ms.
import sys, os, time, json, struct, threading, ctypes, ctypes.wintypes as W
sys.path.insert(0, r"C:\Users\Ahmad Hamza\nioh3-engine\Temp\dojo106699"); import vpad
TAG, N, OUT = sys.argv[1], int(sys.argv[2]), sys.argv[3]
k32 = ctypes.WinDLL("kernel32", use_last_error=True); ps = ctypes.WinDLL("psapi")
k32.OpenProcess.restype = W.HANDLE
k32.ReadProcessMemory.argtypes = [W.HANDLE, ctypes.c_void_p, ctypes.c_void_p, ctypes.c_size_t, ctypes.POINTER(ctypes.c_size_t)]
import subprocess
pid = int(subprocess.run(["powershell", "-NoProfile", "-c", "(Get-Process Nioh3).Id"], capture_output=True, text=True).stdout.strip())
h = k32.OpenProcess(0x0410, 0, pid)
mods = (ctypes.c_void_p * 1)(); need = W.DWORD()
ps.EnumProcessModules(h, mods, ctypes.sizeof(mods), ctypes.byref(need)); BASE = mods[0]
def rd(a, n):
    b = ctypes.create_string_buffer(n); got = ctypes.c_size_t()
    if not a or not k32.ReadProcessMemory(h, ctypes.c_void_p(a), b, n, ctypes.byref(got)): raise OSError
    return b.raw
def rq(a): return struct.unpack("<Q", rd(a, 8))[0]
def player(): return rq(rq(BASE + 0x4745348) + 0xCD * 0x18)
def fpsi():
    try: return struct.unpack("<i", rd(rq(BASE + 0x4759910) + 0x614, 4))[0]
    except OSError: return None
C = player()
def act():
    try: return struct.unpack("<I", rd(rq(rq(rq(C + 0x3B0) + 8) + 0xE8), 4))[0]
    except OSError: return -1
seq, fseq, stop = [], [], False
def samp():
    prev = None
    while not stop:
        a = act(); t = time.perf_counter()
        if a != prev: seq.append((t, a)); prev = a
        fseq.append((t, fpsi())); time.sleep(0.002)
print("pid", pid, "base", hex(BASE), "player", hex(C), "act", hex(act()), "fps_i", fpsi(), flush=True)
threading.Thread(target=samp, daemon=True).start()
cl = vpad.Client()
out = open(os.path.join(OUT, "arm-%s.jsonl" % TAG), "w")
INP = [("Y", "press Y 1000"), ("RB", "press RB 1000"), ("LB", "press LB 1000")]
for i in range(N):
    for name, cmd in INP:
        time.sleep(1.2)
        n, nf = len(seq), len(fseq); t = time.perf_counter(); r = cl.cmd(cmd); time.sleep(3.0)
        s = [(round(tt - t, 3), hex(a)) for tt, a in seq[n:]]
        fv = [v for tt, v in fseq[nf:] if v is not None and tt - t < 1.0]
        out.write(json.dumps(dict(i=i, inp=name, reply=r, acts=s, fps_min=min(fv) if fv else None, fps_max=max(fv) if fv else None,
                                  fps_med=sorted(fv)[len(fv)//2] if fv else None)) + "\n"); out.flush()
        print(i, name, s[:5], "fps", (min(fv), max(fv)) if fv else None, flush=True)
stop = True
