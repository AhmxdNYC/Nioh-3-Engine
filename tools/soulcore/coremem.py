# pc/2 #128901: read / write the equipped soul core slots of the running Nioh3 (external RPM/WPM).
# inv = **(base+0x4751530); slots u32 at inv+0x23B6B8 (0,1 Yang; 2.. Yin); count u64 inv+0x927D0; item rec inv+0x10+i*0xF0 (u16 id at +0)
# item master: db=[base+0x45BA330], T=[[db+0x68]], rows T+8 stride 0x1A0, core row: u32 +0x64 == 900000000 and byte +0x182 == 9, id u16 +0x152
import ctypes as C, ctypes.wintypes as W, struct, sys, subprocess
k = C.WinDLL("kernel32", use_last_error=True)
k.OpenProcess.restype = W.HANDLE
def pid():
    o = subprocess.run(["tasklist", "/FI", "IMAGENAME eq Nioh3.exe", "/FO", "CSV", "/NH"], capture_output=True, text=True).stdout
    for l in o.splitlines():
        if "Nioh3.exe" in l: return int(l.split('","')[1])
    sys.exit("no Nioh3")
P = pid(); h = k.OpenProcess(0x0010 | 0x0020 | 0x0008 | 0x0400, False, P)
def rd(a, n):
    b = C.create_string_buffer(n); r = C.c_size_t()
    if not k.ReadProcessMemory(h, C.c_void_p(a), b, n, C.byref(r)) or r.value != n: raise IOError("read %X" % a)
    return b.raw
def q(a): return struct.unpack("<Q", rd(a, 8))[0]
def u32(a): return struct.unpack("<I", rd(a, 4))[0]
def u16(a): return struct.unpack("<H", rd(a, 2))[0]
def wr32(a, v):
    b = struct.pack("<I", v); r = C.c_size_t()
    if not k.WriteProcessMemory(h, C.c_void_p(a), b, 4, C.byref(r)): raise IOError("write %X err %d" % (a, C.get_last_error()))
def base():
    class ME(C.Structure): _fields_ = [("sz", W.DWORD), ("a", W.DWORD), ("pid", W.DWORD), ("g", W.DWORD), ("p", W.DWORD), ("baseAddr", C.c_void_p), ("baseSize", W.DWORD), ("hMod", W.HMODULE), ("name", C.c_char * 256), ("path", C.c_char * 260)]
    k.CreateToolhelp32Snapshot.restype = W.HANDLE
    s = k.CreateToolhelp32Snapshot(0x18, P); m = ME(); m.sz = C.sizeof(ME)
    ok = k.Module32First(s, C.byref(m))
    while ok:
        if m.name.lower() == b"nioh3.exe": return m.baseAddr
        ok = k.Module32Next(s, C.byref(m))
    sys.exit("no module")
B = base()
inv = q(q(B + 0x4751530))
cnt = q(inv + 0x927D0)
def core_ids():
    db = q(B + 0x45B9E30); T = q(q(db + 0x68)); n = u32(T + 4); raw = rd(T + 8, n * 0x1A0); ids = set()
    for i in range(n):
        r = raw[i * 0x1A0:(i + 1) * 0x1A0]
        if struct.unpack_from("<I", r, 0x64)[0] == 900000000 and r[0x182] == 9: ids.add(struct.unpack_from("<H", r, 0x152)[0])
    return ids
def state():
    slots = [u32(inv + 0x23B6B8 + i * 4) for i in range(10)]
    print("pid %d base %X inv %X count %d" % (P, B, inv, cnt))
    for i, s in enumerate(slots):
        print("slot %d idx %s item %s" % (i, "-" if s == 0xFFFFFFFF else s, "-" if s == 0xFFFFFFFF or s >= cnt else "%X" % u16(inv + 0x10 + s * 0xF0)))
    ids = core_ids(); raw = rd(inv + 0x10, cnt * 0xF0) if cnt else b""
    owned = [(i, struct.unpack_from("<H", raw, i * 0xF0)[0]) for i in range(cnt) if struct.unpack_from("<H", raw, i * 0xF0)[0] in ids]
    print("core items in inventory (%d core ids known):" % len(ids), " ".join("%d:%X%s" % (i, it, "*" if i in slots else "") for i, it in owned))
    return slots, owned
if __name__ == "__main__":
    if len(sys.argv) > 1 and sys.argv[1] == "set":
        sl, idx = int(sys.argv[2]), int(sys.argv[3])
        assert 0 <= sl < 10 and 0 <= idx < cnt
        a = inv + 0x23B6B8 + sl * 4; old = u32(a); wr32(a, idx); print("slot %d: %d -> %d" % (sl, old, u32(a)))
    state()
