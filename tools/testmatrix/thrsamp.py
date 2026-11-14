# thrsamp.py (#102384): per-thread state sampler for Nioh3.exe (no admin). Waits for the process, samples every
# --ms ms via NtQuerySystemInformation(SystemProcessInformation) until it exits. Writes JSONL:
#  {"k":"thr","tid":..,"start":"Nioh3.exe+0x..","name":".."}           first sighting of a thread
#  {"k":"s","t":"HH:MM:SS.fff","rd":bytes_read_delta,"n":samples,"th":{tid:[cpu_ms,run,ready,{wait_reason:count}]}}  per second
import ctypes as C, ctypes.wintypes as W, json, sys, time, datetime, argparse
ap = argparse.ArgumentParser(); ap.add_argument("--out", required=True); ap.add_argument("--ms", type=float, default=10)
ap.add_argument("--wait", type=float, default=300); ap.add_argument("--name", default="nioh3.exe"); a = ap.parse_args()
nt = C.WinDLL("ntdll"); k32 = C.WinDLL("kernel32", use_last_error=True); psapi = C.WinDLL("psapi")
class UNI(C.Structure): _fields_ = [("Length", W.USHORT), ("Max", W.USHORT), ("Buf", C.c_void_p)]
class SPI(C.Structure):
    _fields_ = [("Next", W.ULONG), ("NThreads", W.ULONG), ("r1", C.c_byte * 48), ("Name", UNI), ("Prio", C.c_long),
                ("Pid", C.c_void_p), ("PPid", C.c_void_p), ("Handles", W.ULONG), ("Session", W.ULONG), ("Key", C.c_void_p),
                ("vm", C.c_size_t * 12), ("io", C.c_longlong * 6)]
class STI(C.Structure):
    _fields_ = [("Kernel", C.c_longlong), ("User", C.c_longlong), ("Create", C.c_longlong), ("WaitTime", W.ULONG),
                ("Start", C.c_void_p), ("Pid", C.c_void_p), ("Tid", C.c_void_p), ("Prio", C.c_long), ("Base", C.c_long),
                ("Switches", W.ULONG), ("State", W.ULONG), ("Reason", W.ULONG)]
SPI_SIZE = C.sizeof(SPI); assert SPI_SIZE == 256, SPI_SIZE
buf = C.create_string_buffer(8 << 20)
def snap():
    n = W.ULONG()
    st = nt.NtQuerySystemInformation(5, buf, len(buf), C.byref(n))
    if st != 0: raise OSError("NtQSI %x" % (st & 0xffffffff))
    off = 0
    while True:
        p = SPI.from_buffer(buf, off)
        if p.Name.Buf and p.Name.Length:
            nm = C.wstring_at(p.Name.Buf, p.Name.Length // 2)
            if nm.lower() == a.name:
                th = (STI * p.NThreads).from_buffer(buf, off + SPI_SIZE)
                return p.Pid, p.io[3], [(t.Tid, t.Kernel + t.User, t.State, t.Reason, t.Start) for t in th]
        if not p.Next: return None
        off += p.Next
OT = k32.OpenThread; OT.restype = W.HANDLE; OT.argtypes = [W.DWORD, W.BOOL, W.DWORD]
def thread_info(tid, mods):
    h = OT(0x0040 | 0x0800, False, tid)   # QUERY_INFORMATION | QUERY_LIMITED
    start, name = 0, ""
    if h:
        v = C.c_void_p(); nt.NtQueryInformationThread(W.HANDLE(h), 9, C.byref(v), C.sizeof(v), None); start = v.value or 0
        try:
            pw = C.c_wchar_p();
            if k32.GetThreadDescription(W.HANDLE(h), C.byref(pw)) >= 0 and pw.value: name = pw.value
        except Exception: pass
        k32.CloseHandle(W.HANDLE(h))
    s = hex(start)
    for b, e, m in mods:
        if b <= start < e: s = "%s+0x%x" % (m, start - b); break
    return s, name
def modules(pid):
    h = k32.OpenProcess(0x0410, False, pid); out = []
    if not h: return out
    arr = (C.c_void_p * 1024)(); need = W.DWORD()
    psapi.EnumProcessModulesEx(W.HANDLE(h), arr, C.sizeof(arr), C.byref(need), 3)
    class MI(C.Structure): _fields_ = [("base", C.c_void_p), ("size", W.DWORD), ("entry", C.c_void_p)]
    for i in range(min(need.value // 8, 1024)):
        if not arr[i]: continue
        nmb = C.create_unicode_buffer(260); psapi.GetModuleBaseNameW(W.HANDLE(h), C.c_void_p(arr[i]), nmb, 260)
        mi = MI(); psapi.GetModuleInformation(W.HANDLE(h), C.c_void_p(arr[i]), C.byref(mi), C.sizeof(mi))
        out.append((arr[i], arr[i] + mi.size, nmb.value))
    k32.CloseHandle(W.HANDLE(h)); return out
f = open(a.out, "a", buffering=1); t0 = time.time(); s = None
while s is None and time.time() - t0 < a.wait: s = snap(); s or time.sleep(0.5)
if s is None: f.write(json.dumps({"k": "err", "e": "no Nioh3.exe"}) + "\n"); sys.exit(1)
pid = s[0]; f.write(json.dumps({"k": "pid", "pid": pid, "t": datetime.datetime.now().strftime("%H:%M:%S.%f")[:-3]}) + "\n")
time.sleep(3); mods = modules(pid); seen = {}; mods_t = time.time()
last_cpu, last_rd = {}, s[1]; acc = {}; nsm = 0; sec = int(time.time())
while True:
    s = snap()
    if s is None or s[0] != pid: break
    now = time.time()
    if now - mods_t > 20: mods = modules(pid) or mods; mods_t = now
    for tid, cpu, state, reason, _ in s[2]:
        if tid not in seen:
            st, nm = thread_info(tid, mods); seen[tid] = 1
            f.write(json.dumps({"k": "thr", "tid": tid, "start": st, "name": nm}) + "\n")
        r = acc.setdefault(tid, [0.0, 0, 0, {}])
        if tid in last_cpu: r[0] += (cpu - last_cpu[tid]) / 1e4
        last_cpu[tid] = cpu
        if state == 2: r[1] += 1
        elif state in (1, 3, 7): r[2] += 1      # ready / standby / deferred ready
        elif state == 5: r[3][reason] = r[3].get(reason, 0) + 1
    nsm += 1
    if int(now) != sec:
        rd = s[1] - last_rd; last_rd = s[1]
        f.write(json.dumps({"k": "s", "t": datetime.datetime.fromtimestamp(now).strftime("%H:%M:%S.%f")[:-3], "rd": rd, "n": nsm,
                            "th": {t: [round(v[0], 1), v[1], v[2], v[3]] for t, v in acc.items()}}) + "\n")
        acc = {}; nsm = 0; sec = int(now)
    time.sleep(max(0, a.ms / 1000 - (time.time() - now)))
f.write(json.dumps({"k": "end", "t": datetime.datetime.now().strftime("%H:%M:%S.%f")[:-3]}) + "\n")
